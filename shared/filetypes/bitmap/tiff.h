#ifndef TIFF_H
#define TIFF_H

#include "base/defs.h"
#include "base/maths.h"
#include "base/array.h"
#include "extra/date.h"

namespace tiff {

using namespace iso;

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

template<size_t N>		 constexpr TYPE get_type_by_size()	{ return TYPE::UNDEFINED; }
template<typename T>	 constexpr TYPE get_type()			{ return get_type_by_size<sizeof(T)>(); }

inline int	type_length(TYPE type) {
	const int lengths[] = {0,1,1,2,4,8,1,1,2,4,8,4,8,4,0,0,8,8,8};
	return lengths[(int)type];
}

template<> constexpr TYPE	get_type_by_size<1>()			{ return TYPE::BYTE; }
template<> constexpr TYPE	get_type_by_size<2>()			{ return TYPE::SHORT; }
template<> constexpr TYPE	get_type_by_size<4>()			{ return TYPE::LONG; }
template<> constexpr TYPE	get_type_by_size<8>()			{ return TYPE::LONG8; }

template<> constexpr TYPE	get_type<uint8>()				{ return TYPE::BYTE; }
template<> constexpr TYPE	get_type<const char*>()			{ return TYPE::ASCII; }
template<> constexpr TYPE	get_type<uint16>()				{ return TYPE::SHORT; }
template<> constexpr TYPE	get_type<uint32>()				{ return TYPE::LONG; }
template<> constexpr TYPE	get_type<rational<uint32>>()	{ return TYPE::RATIONAL; }
template<> constexpr TYPE	get_type<int8>()				{ return TYPE::SBYTE; }
template<> constexpr TYPE	get_type<int16>()				{ return TYPE::SSHORT; }
template<> constexpr TYPE	get_type<int32>()				{ return TYPE::SLONG; }
template<> constexpr TYPE	get_type<rational<int32>>()		{ return TYPE::SRATIONAL; }
template<> constexpr TYPE	get_type<float>()				{ return TYPE::FLOAT; }
template<> constexpr TYPE	get_type<double>()				{ return TYPE::DOUBLE; }
template<> constexpr TYPE	get_type<uint64>()				{ return TYPE::LONG8; }
template<> constexpr TYPE	get_type<int64>()				{ return TYPE::SLONG8; }

enum class TAG : uint16 {
	// GPS tags
	GPSVersionID				= 0x0000,
	GPSLatitudeRef				= 0x0001,
	GPSLatitude					= 0x0002,
	GPSLongitudeRef				= 0x0003,
	GPSLongitude				= 0x0004,
	GPSAltitudeRef				= 0x0005,
	GPSAltitude					= 0x0006,
	GPSTimeStamp				= 0x0007,
	GPSSatellites				= 0x0008,
	GPSStatus					= 0x0009,
	GPSMeasureMode				= 0x000A,
	GPSDOP						= 0x000B,
	GPSSpeedRef					= 0x000C,
	GPSSpeed					= 0x000D,
	GPSTrackRef					= 0x000E,
	GPSTrack					= 0x000F,
	GPSImgDirectionRef			= 0x0010,
	GPSImgDirection				= 0x0011,
	GPSMapDatum					= 0x0012,
	GPSDestLatitudeRef			= 0x0013,
	GPSDestLatitude				= 0x0014,
	GPSDestLongitudeRef			= 0x0015,
	GPSDestLongitude			= 0x0016,
	GPSDestBearingRef			= 0x0017,
	GPSDestBearing				= 0x0018,
	GPSDestDistanceRef			= 0x0019,
	GPSDestDistance				= 0x001A,
	GPSProcessingMethod			= 0x001B,
	GPSAreaInformation			= 0x001C,
	GPSDateStamp				= 0x001D,
	GPSDifferential				= 0x001E,

	InteropIndex				= 0x0001,	//
	InteropVersion				= 0x0002,	//undef				InteropIFD	 
	ProcessingSoftware			= 0x000b,	//string			IFD0	(used by ACD Systems Digital Imaging)
	SubfileType					= 0x00fe,	//uint32			IFD0	(called NewSubfileType by the TIFF specification)
	OldSubfileType				= 0x00ff,	//uint16			IFD0	(called SubfileType by the TIFF specification)
	ImageWidth					= 0x0100,	//uint32			IFD0	 
	ImageHeight					= 0x0101,	//uint32			IFD0	(called ImageLength by the EXIF spec.)
	BitsPerSample				= 0x0102,	//uint16[n]			IFD0	 
	Compression					= 0x0103,	//uint16			IFD0	--> EXIF Compression Values
	PhotometricInterpretation	= 0x0106,	//uint16			IFD0	
	Thresholding				= 0x0107,	//uint16			IFD0	1 = No dithering or halftoning
	CellWidth					= 0x0108,	//uint16			IFD0	 
	CellLength					= 0x0109,	//uint16			IFD0	 
	FillOrder					= 0x010a,	//uint16			IFD0	1 = Normal
	DocumentName				= 0x010d,	//string			IFD0	 
	ImageDescription			= 0x010e,	//string			IFD0	 
	Make						= 0x010f,	//string			IFD0	 
	Model						= 0x0110,	//string			IFD0	 
	StripOffsets				= 0x0111,	//
	Orientation					= 0x0112,	//uint16			IFD0	
	SamplesPerPixel				= 0x0115,	//uint16			IFD0	 
	RowsPerStrip				= 0x0116,	//uint32			IFD0	 
	StripByteCounts				= 0x0117,	//
	MinSampleValue				= 0x0118,	//uint16			IFD0	 
	MaxSampleValue				= 0x0119,	//uint16			IFD0	 
	XResolution					= 0x011a,	//urational			IFD0	 
	YResolution					= 0x011b,	//urational			IFD0	 
	PlanarConfiguration			= 0x011c,	//uint16			IFD0	1 = Chunky
	PageName					= 0x011d,	//string			IFD0	 
	XPosition					= 0x011e,	//urational			IFD0	 
	YPosition					= 0x011f,	//urational			IFD0	 
	FreeOffsets					= 0x0120,	//
	FreeByteCounts				= 0x0121,	//
	GrayResponseUnit			= 0x0122,	//uint16			IFD0	1 = 0.1
	GrayResponseCurve			= 0x0123,	//
	T4Options					= 0x0124,	//	Bit 0 = 2-Dimensional encoding
	T6Options					= 0x0125,	//	Bit 1 = Uncompressed
	ResolutionUnit				= 0x0128,	//uint16			IFD0	(the value 1 is not standard EXIF)
	PageNumber					= 0x0129,	//uint16[2]			IFD0	 
	ColorResponseUnit			= 0x012c,	//
	TransferFunction			= 0x012d,	//uint16[768]		IFD0	 
	Software					= 0x0131,	//string			IFD0	 
	ModifyDate					= 0x0132,	//string			IFD0	(called DateTime by the EXIF spec.)
	Artist						= 0x013b,	//string			IFD0	(becomes a list-type tag when the MWG module is loaded)
	HostComputer				= 0x013c,	//string			IFD0	 
	Predictor					= 0x013d,	//uint16			IFD0	
	WhitePoint					= 0x013e,	//urational[2]		IFD0	 
	PrimaryChromaticities		= 0x013f,	//urational[6]		IFD0	 
	ColorMap					= 0x0140,	//
	HalftoneHints				= 0x0141,	//uint16[2]			IFD0	 
	TileWidth					= 0x0142,	//uint32			IFD0	 
	TileLength					= 0x0143,	//uint32			IFD0	 
	TileOffsets					= 0x0144,	//
	TileByteCounts				= 0x0145,	//
	BadFaxLines					= 0x0146,	//
	CleanFaxData				= 0x0147,	//	0 = Clean
	ConsecutiveBadFaxLines		= 0x0148,	//
	SubIFD						= 0x014a,	//
	InkSet						= 0x014c,	//uint16			IFD0	1 = CMYK
	InkNames					= 0x014d,	//
	NumberofInks				= 0x014e,	//
	DotRange					= 0x0150,	//
	TargetPrinter				= 0x0151,	//string			IFD0	 
	ExtraSamples				= 0x0152,	//	0 = Unspecified
	SampleFormat				= 0x0153,	//no				SubIFD	(SamplesPerPixel values)
	SMinSampleValue				= 0x0154,	//
	SMaxSampleValue				= 0x0155,	//
	TransferRange				= 0x0156,	//
	ClipPath					= 0x0157,	//
	XClipPathUnits				= 0x0158,	//
	YClipPathUnits				= 0x0159,	//
	Indexed						= 0x015a,	//	0 = Not indexed
	JPEGTables					= 0x015b,	//
	OPIProxy					= 0x015f,	//	0 = Higher resolution image does not exist
	GlobalParametersIFD			= 0x0190,	//--> EXIF Tags
	ProfileType					= 0x0191,	//	0 = Unspecified
	FaxProfile					= 0x0192,	//	
	CodingMethods				= 0x0193,	//	
	VersionYear					= 0x0194,	//
	ModeNumber					= 0x0195,	//
	Decode						= 0x01b1,	//
	DefaultImageColor			= 0x01b2,	//
	T82Options					= 0x01b3,	//
	//JPEGTables				= 0x01b5,	//
	JPEGProc					= 0x0200,	//	1 = Baseline
	JPEGOffset					= 0x0201,
	JPEGLength					= 0x0202,
	//ThumbnailOffset			= 0x0201,	//
	//ThumbnailLength			= 0x0202,	//
	JPEGRestartInterval			= 0x0203,	//
	JPEGLosslessPredictors		= 0x0205,	//
	JPEGPointTransforms			= 0x0206,	//
	JPEGQTables					= 0x0207,	//
	JPEGDCTables				= 0x0208,	//
	JPEGACTables				= 0x0209,	//
	YCbCrCoefficients			= 0x0211,	//urational[3]		IFD0	 
	YCbCrSubSampling			= 0x0212,	//uint16[2]			IFD0	
	YCbCrPositioning			= 0x0213,	//uint16			IFD0	1 = Centered
	ReferenceBlackWhite			= 0x0214,	//urational[6]		IFD0	 
	StripRowCounts				= 0x022f,	//
	ApplicationNotes			= 0x02bc,	//int8				IFD0	--> XMP Tags
	USPTOMiscellaneous			= 0x03e7,	//
	RelatedImageFileFormat		= 0x1000,	//string			InteropIFD	 
	RelatedImageWidth			= 0x1001,	//uint16			InteropIFD	 
	RelatedImageHeight			= 0x1002,	//uint16			InteropIFD	(called RelatedImageLength by the DCF spec.)
	Rating						= 0x4746,	//uint16/			IFD0	 
	XP_DIP_XML					= 0x4747,	//
	StitchInfo					= 0x4748,	//--> Microsoft Stitch Tags
	RatingPercent				= 0x4749,	//uint16			IFD0	 
	SonyRawFileType				= 0x7000,	//	0 = Sony Uncompressed 14-bit RAW
	SonyToneCurve				= 0x7010,	//
	VignettingCorrection		= 0x7031,	//int16				SubIFD	(found in Sony ARW images)
	VignettingCorrParams		= 0x7032,	//int16[17]			SubIFD	(found in Sony ARW images)
	ChromaticAberrationCorrection=0x7034,	//int16				SubIFD	(found in Sony ARW images)
	ChromaticAberrationCorrParams=0x7035,	//int16[33]			SubIFD	(found in Sony ARW images)
	DistortionCorrection		= 0x7036,	//int16				SubIFD	(found in Sony ARW images)
	DistortionCorrParams		= 0x7037,	//int16[17]			SubIFD	(found in Sony ARW images)
	SonyCropTopLeft				= 0x74c7,	//int32u[2]			SubIFD	 
	SonyCropSize				= 0x74c8,	//int32u[2]			SubIFD	 
	ImageID						= 0x800d,	//
	WangTag1					= 0x80a3,	//
	WangAnnotation				= 0x80a4,	//
	WangTag3					= 0x80a5,	//
	WangTag4					= 0x80a6,	//
	ImageReferencePoints		= 0x80b9,	//
	RegionXformTackPoint		= 0x80ba,	//
	WarpQuadrilateral			= 0x80bb,	//
	AffineTransformMat			= 0x80bc,	//
	Matteing					= 0x80e3,	//
	DataType					= 0x80e4,	//
	ImageDepth					= 0x80e5,	//
	TileDepth					= 0x80e6,	//
	ImageFullWidth				= 0x8214,	//
	ImageFullHeight				= 0x8215,	//
	TextureFormat				= 0x8216,	//
	WrapModes					= 0x8217,	//
	FovCot						= 0x8218,	//
	MatrixWorldToScreen			= 0x8219,	//
	MatrixWorldToCamera			= 0x821a,	//
	Model2						= 0x827d,	//
	CFARepeatPatternDim			= 0x828d,	//uint16[2]			SubIFD	 
	CFAPattern2					= 0x828e,	//int8[n]			SubIFD	 
	BatteryLevel				= 0x828f,	//
	KodakIFD					= 0x8290,	//--> Kodak IFD Tags
	Copyright					= 0x8298,	//string			IFD0	(may contain copyright notices for photographer and editor, separated by a newline. As per the EXIF specification, the newline is replaced by a null byte when writing to file, but this may be avoided by disabling the print conversion)
	ExposureTime				= 0x829a,	//urational			ExifIFD		(s)
	FNumber						= 0x829d,	//urational			ExifIFD	 
	MDFileTag					= 0x82a5,	//	(tags 0x82a5-0x82ac are used in Molecular Dynamics GEL files)
	MDScalePixel				= 0x82a6,	//
	MDColorTable				= 0x82a7,	//
	MDLabName					= 0x82a8,	//
	MDSampleInfo				= 0x82a9,	//
	MDPrepDate					= 0x82aa,	//
	MDPrepTime					= 0x82ab,	//
	MDFileUnits					= 0x82ac,	//
	PixelScale					= 0x830e,	//double[3]			IFD0	 
	AdventScale					= 0x8335,	//
	AdventRevision				= 0x8336,	//
	UIC1Tag						= 0x835c,	//
	UIC2Tag						= 0x835d,	//
	UIC3Tag						= 0x835e,	//
	UIC4Tag						= 0x835f,	//
	IPTCNAA						= 0x83bb,	//uint32			IFD0	--> IPTC Tags
	IntergraphPacketData		= 0x847e,	//
	IntergraphFlagRegisters		= 0x847f,	//
	IntergraphMatrix			= 0x8480,	//double[n]			IFD0	 
	INGRReserved				= 0x8481,	//
	ModelTiePoint				= 0x8482,	//double[n]			IFD0	 
	Site						= 0x84e0,	//
	ColorSequence				= 0x84e1,	//
	IT8Header					= 0x84e2,	//
	RasterPadding				= 0x84e3,	//	0 = Byte
	BitsPerRunLength			= 0x84e4,	//
	BitsPerExtendedRunLength	= 0x84e5,	//
	ColorTable					= 0x84e6,	//
	ImageColorIndicator			= 0x84e7,	//	0 = Unspecified Image Color
	BackgroundColorIndicator	= 0x84e8,	//	0 = Unspecified Background Color
	ImageColorValue				= 0x84e9,	//
	BackgroundColorValue		= 0x84ea,	//
	PixelIntensityRange			= 0x84eb,	//
	TransparencyIndicator		= 0x84ec,	//
	ColorCharacterization		= 0x84ed,	//
	HCUsage						= 0x84ee,	//	0 = CT
	TrapIndicator				= 0x84ef,	//
	CMYKEquivalent				= 0x84f0,	//
	SEMInfo						= 0x8546,	//string			IFD0	(found in some scanning electron microscope images)
	AFCP_IPTC					= 0x8568,	//--> IPTC Tags		
	PixelMagicJBIGOptions		= 0x85b8,	//					
	JPLCartoIFD					= 0x85d7,	//					
	ModelTransform				= 0x85d8,	//double[16]		IFD0	 
	WB_GRGBLevels				= 0x8602,	//	(found in		IFD0 of Leaf MOS images)
	LeafData					= 0x8606,	//--> Leaf Tags		
	PhotoshopSettings			= 0x8649,	//-					IFD0	--> Photoshop Tags
	ExifOffset					= 0x8769,	//-					IFD0	--> EXIF Tags
	ICC_Profile					= 0x8773,	//-					IFD0	--> ICC_Profile Tags
	TIFF_FXExtensions			= 0x877f,	//	Bit 0 = Resolution/Image Width
	MultiProfiles				= 0x8780,	//	
	SharedData					= 0x8781,	//
	T88Options					= 0x8782,	//
	ImageLayer					= 0x87ac,	//
	GeoTiffDirectory			= 0x87af,	//uint16[0.5]		IFD0	(these "GeoTiff" tags may read and written as a block, but they aren't extracted unless specifically requested. Byte order changes are handled automatically when copying between TIFF images with different byte order)
	GeoTiffDoubleParams			= 0x87b0,	//double[0.125]		IFD0	 
	GeoTiffAsciiParams			= 0x87b1,	//string			IFD0	 
	JBIGOptions					= 0x87be,	//
	ExposureProgram				= 0x8822,	//uint16			ExifIFD	(the value of 9 is not standard EXIF, but is used by the Canon EOS 7D)
	SpectralSensitivity			= 0x8824,	//string			ExifIFD	 
	GPSInfo						= 0x8825,	//-					IFD0	--> GPS Tags
	ISO							= 0x8827,	//uint16[n]			ExifIFD	(called ISOSpeedRatings by EXIF 2.2, then PhotographicSensitivity by the EXIF 2.3 spec.)
	OECF						= 0x8828,	//	pto-ElectricConvFactor
	Interlace					= 0x8829,	//
	TimeZoneOffset				= 0x882a,	//int16[n]			ExifIFD	(1 or 2 values 1. The time zone offset of DateTimeOriginal from GMT in hours, 2. If present, the time zone offset of ModifyDate)
	SelfTimerMode				= 0x882b,	//uint16			ExifIFD	 
	SensitivityType				= 0x8830,	//uint16			ExifIFD	(applies to EXIFISO tag)
	StandardOutputSensitivity	= 0x8831,	//int32u			ExifIFD	 
	RecommendedExposureIndex	= 0x8832,	//int32u			ExifIFD	 
	ISOSpeed					= 0x8833,	//int32u			ExifIFD	 
	ISOSpeedLatitudeyyy			= 0x8834,	//int32u			ExifIFD	 
	ISOSpeedLatitudezzz			= 0x8835,	//int32u			ExifIFD	 
	FaxRecvParams				= 0x885c,	//
	FaxSubAddress				= 0x885d,	//
	FaxRecvTime					= 0x885e,	//
	FedexEDR					= 0x8871,	//
	LeafSubIFD					= 0x888a,	//--> Leaf SubIFD Tags
	ExifVersion					= 0x9000,	//undef				ExifIFD	 
	DateTimeOriginal			= 0x9003,	//string			ExifIFD	(date/time when original image was taken)
	CreateDate					= 0x9004,	//string			ExifIFD	(called DateTimeDigitized by the EXIF spec.)
	GooglePlusUploadCode		= 0x9009,	//undef[n]			ExifIFD	 
	OffsetTime					= 0x9010,	//string			ExifIFD	(time zone for ModifyDate)
	OffsetTimeOriginal			= 0x9011,	//string			ExifIFD	(time zone for DateTimeOriginal)
	OffsetTimeDigitized			= 0x9012,	//string			ExifIFD	(time zone for CreateDate)
	ComponentsConfiguration		= 0x9101,	//undef[4]			ExifIFD		Information about channels
	CompressedBitsPerPixel		= 0x9102,	//urational			ExifIFD	 
	ShutterSpeedValue			= 0x9201,	//rational			ExifIFD	(displayed in seconds, but stored as an APEX value)
	ApertureValue				= 0x9202,	//urational			ExifIFD	(displayed as an F number, but stored as an APEX value)
	BrightnessValue				= 0x9203,	//rational			ExifIFD	 
	ExposureBiasValue			= 0x9204,	//rational			ExifIFD
	MaxApertureValue			= 0x9205,	//urational			ExifIFD	Smallest F number of lens
	SubjectDistance				= 0x9206,	//urational			ExifIFD	 Distance to subject in meters
	MeteringMode				= 0x9207,	//uint16			ExifIFD	
	LightSource					= 0x9208,	//uint16			ExifIFD	--> EXIF LightSource Values
	Flash						= 0x9209,	//uint16			ExifIFD	--> EXIF Flash Values
	FocalLength					= 0x920a,	//urational			ExifIFD		Focal length of the lens in mm
	FlashEnergy					= 0x920b,	//					Strobe energy in BCPS	 
	SpatialFrequencyResponse	= 0x920c,	//
	Noise						= 0x920d,	//
	FocalPlaneXResolution		= 0x920e,	//Number of pixels in width direction per FocalPlaneResolutionUnit
	FocalPlaneYResolution		= 0x920f,	//Number of pixels in height direction per FocalPlaneResolutionUnit
	FocalPlaneResolutionUnit	= 0x9210,	//	1 = None	Unit for measuring FocalPlaneXResolution and FocalPlaneYResolution
	ImageNumber					= 0x9211,	//int32u			ExifIFD	 
	SecurityClassification		= 0x9212,	//string			ExifIFD	'C' = Confidential
	ImageHistory				= 0x9213,	//string			ExifIFD	 
	SubjectArea					= 0x9214,	//uint16[n]			ExifIFD	 
	ExposureIndex				= 0x9215,	//
	TIFFEPStandardID			= 0x9216,	//
	SensingMethod				= 0x9217,	//	
	CIP3DataFile				= 0x923a,	//
	CIP3Sheet					= 0x923b,	//
	CIP3Side					= 0x923c,	//
	StoNits						= 0x923f,	//
	MakerNoteApple				= 0x927c,	//
	UserComment					= 0x9286,	//undef				ExifIFD	 
	SubSecTime					= 0x9290,	//string			ExifIFD	(fractional seconds for ModifyDate)
	SubSecTimeOriginal			= 0x9291,	//string			ExifIFD	(fractional seconds for DateTimeOriginal)
	SubSecTimeDigitized			= 0x9292,	//string			ExifIFD	(fractional seconds for CreateDate)
	MSDocumentText				= 0x932f,	//
	MSPropertySetStorage		= 0x9330,	//
	MSDocumentTextPosition		= 0x9331,	//
	ImageSourceData				= 0x935c,	//undef				IFD0	--> Photoshop DocumentData Tags
	AmbientTemperature			= 0x9400,	//rational			ExifIFD	(ambient temperature in degrees C, called Temperature by the EXIF spec.)
	Humidity					= 0x9401,	//urational			ExifIFD	(ambient relative humidity in percent)
	Pressure					= 0x9402,	//urational			ExifIFD	(air pressure in hPa or mbar)
	WaterDepth					= 0x9403,	//rational			ExifIFD	(depth under water in metres, negative for above water)
	Acceleration				= 0x9404,	//urational			ExifIFD	(directionless camera acceleration in units of mGal, or 10-5 m/s2)
	CameraElevationAngle		= 0x9405,	//rational			ExifIFD	 
	Title						= 0x9c9b,	//int8				IFD0	(tags 0x9c9b-0x9c9f are used by Windows Explorer; special characters in these values are converted to UTF-8 by default, or Windows Latin1 with the -L option. XPTitle is ignored by Windows Explorer if ImageDescription exists)
	Comment						= 0x9c9c,	//int8				IFD0	 
	Author						= 0x9c9d,	//int8				IFD0	(ignored by Windows Explorer if Artist exists)
	Keywords					= 0x9c9e,	//int8				IFD0	 
	Subject						= 0x9c9f,	//int8				IFD0	 
	FlashpixVersion				= 0xa000,	//undef				ExifIFD	 
	ColorSpace					= 0xa001,	//uint16			ExifIFD	(the value of 0x2 is not standard EXIF. Instead, an Adobe RGB image is indicated by "Uncalibrated" with an InteropIndex of "R03". The values 0xfffd and 0xfffe are also non-standard, and are used by some Sony cameras)
	PixelXDimension				= 0xa002,	//uint16			ExifIFD	
	PixelYDimension				= 0xa003,	//uint16			ExifIFD	
	RelatedSoundFile			= 0xa004,	//string			ExifIFD	 
	InteropOffset				= 0xa005,	//--> EXIF Tags
	SamsungRawPointersOffset	= 0xa010,	//
	SamsungRawPointersLength	= 0xa011,	//
	SamsungRawByteOrder			= 0xa101,	//
	SamsungRawUnknown			= 0xa102,	//
	ExifFlashEnergy				= 0xa20b,	//urational			ExifIFD	 
	ExifSpatialFrequencyResponse= 0xa20c,	//
	ExifNoise					= 0xa20d,	//
	ExifFocalPlaneXResolution	= 0xa20e,	//urational			ExifIFD	 
	ExifFocalPlaneYResolution	= 0xa20f,	//urational			ExifIFD	 
	ExifFocalPlaneResolutionUnit= 0xa210,	//uint16			ExifIFD	(values 1, 4 and 5 are not standard EXIF)
	ExifImageNumber				= 0xa211,	//
	ExifSecurityClassification	= 0xa212,	//
	ExifImageHistory			= 0xa213,	//
	ExifSubjectLocation			= 0xa214,	//uint16[2]			ExifIFD	 
	ExifExposureIndex			= 0xa215,	//urational			ExifIFD		Exposure index selected on camera
	ExifTIFFEPStandardID		= 0xa216,	//
	ExifSensingMethod			= 0xa217,	//uint16			ExifIFD	
	FileSource					= 0xa300,	//undef				ExifIFD	1 = Film Scanner
	SceneType					= 0xa301,	//undef				ExifIFD	1 = Directly photographed
	CFAPattern					= 0xa302,	//undef				ExifIFD	 
	CustomRendered				= 0xa401,	//uint16			ExifIFD
	ExposureMode				= 0xa402,	//uint16			ExifIFD	0 = Auto
	WhiteBalance				= 0xa403,	//uint16			ExifIFD	0 = Auto
	DigitalZoomRatio			= 0xa404,	//urational			ExifIFD	 
	FocalLengthIn35mmFormat		= 0xa405,	//uint16			ExifIFD	(called FocalLengthIn35mmFilm by the EXIF spec.)
	SceneCaptureType			= 0xa406,	//uint16			ExifIFD	(the value of 4 is non-standard, and used by some Samsung models)
	GainControl					= 0xa407,	//uint16			ExifIFD	0 = None
	Contrast					= 0xa408,	//uint16			ExifIFD	0 = Normal
	Saturation					= 0xa409,	//uint16			ExifIFD	0 = Normal
	Sharpness					= 0xa40a,	//uint16			ExifIFD	0 = Normal
	DeviceSettingDescription	= 0xa40b,	//
	SubjectDistanceRange		= 0xa40c,	//uint16			ExifIFD	0 = Unknown	Location of subject in image
	ImageUniqueID				= 0xa420,	//string			ExifIFD	 
	CameraOwnerName				= 0xa430,	//string			ExifIFD	(called CameraOwnerName by the EXIF spec.)
	BodySerialNumber			= 0xa431,	//string			ExifIFD	(called BodySerialNumber by the EXIF spec.)
	LensInfo					= 0xa432,	//urational[4]		ExifIFD	(4 rational values giving focal and aperture ranges, called LensSpecification by the EXIF spec.)
	LensMake					= 0xa433,	//string			ExifIFD	 
	LensModel					= 0xa434,	//string			ExifIFD	 
	LensSerialNumber			= 0xa435,	//string			ExifIFD	 
	CompositeImage				= 0xa460,	//uint16			ExifIFD	0 = Unknown
	CompositeImageCount			= 0xa461,	//uint16[2]			ExifIFD	(2 values 1. Number of source images, 2. Number of images used. Called SourceImageNumberOfCompositeImage by the EXIF spec.)
	CompositeImageExposureTimes	= 0xa462,	//undef				ExifIFD	(11 or more values 1. Total exposure time period, 2. Total exposure of all source images, 3. Total exposure of all used images, 4. Max exposure time of source images, 5. Max exposure time of used images, 6. Min exposure time of source images, 7. Min exposure of used images, 8. Number of sequences, 9. Number of source images in sequence. 10-N. Exposure times of each source image. Called SourceExposureTimesOfCompositeImage by the EXIF spec.)
	GDALMetadata				= 0xa480,	//string			IFD0	 
	GDALNoData					= 0xa481,	//string			IFD0	 
	Gamma						= 0xa500,	//urational			ExifIFD	 
	ExpandSoftware				= 0xafc0,	//
	ExpandLens					= 0xafc1,	//
	ExpandFilm					= 0xafc2,	//
	ExpandFilterLens			= 0xafc3,	//
	ExpandScanner				= 0xafc4,	//
	ExpandFlashLamp				= 0xafc5,	//
	HasselbladRawImage			= 0xb4c3,	//
	WDPPixelFormat				= 0xbc01,	//	(tags 0xbc** are used in Windows HD Photo (HDP and WDP) images. The actual PixelFormat values are 16-byte GUID's but the leading 15 bytes, '6fddc324-4e03-4bfe-b1853-d77768dc9', have been removed below to avoid unnecessary clutter)
	WDPTransformation			= 0xbc02,	//	
	WDPUncompressed				= 0xbc03,	//	0 = No
	WDPImageType				= 0xbc04,	//	Bit 0 = Preview
	WDPImageWidth				= 0xbc80,	//
	WDPImageHeight				= 0xbc81,	//
	WDPWidthResolution			= 0xbc82,	//
	WDPHeightResolution			= 0xbc83,	//
	WDPImageOffset				= 0xbcc0,	//
	WDPImageByteCount			= 0xbcc1,	//
	WDPAlphaOffset				= 0xbcc2,	//
	WDPAlphaByteCount			= 0xbcc3,	//
	WDPImageDataDiscard			= 0xbcc4,	//	0 = Full Resolution
	WDPAlphaDataDiscard			= 0xbcc5,	//	0 = Full Resolution
	OceScanjobDesc				= 0xc427,	//
	OceApplicationSelector		= 0xc428,	//
	OceIDNumber					= 0xc429,	//
	OceImageLogic				= 0xc42a,	//
	Annotations					= 0xc44f,	//
	PrintIM						= 0xc4a5,	//undef				IFD0	--> PrintIM Tags
	HasselbladExif				= 0xc51b,	//
	OriginalFileName			= 0xc573,	//	(used by some obscure software)
	USPTOOriginalContentType	= 0xc580,	//	0 = Text or Drawing
	CR2CFAPattern				= 0xc5e0,	//	1 => '0 1 1 2' = [Red,Green][Green,Blue]
	DNGVersion					= 0xc612,	//int8[4]			IFD0	(tags 0xc612-0xcd3b are defined by the DNG specification unless otherwise noted. See https//helpx.adobe.com/photoshop/digital-negative.html for the specification)
	DNGBackwardVersion			= 0xc613,	//int8[4]			IFD0	 
	UniqueCameraModel			= 0xc614,	//string			IFD0	 
	LocalizedCameraModel		= 0xc615,	//string			IFD0	 
	CFAPlaneColor				= 0xc616,	//no				SubIFD	 
	CFALayout					= 0xc617,	//no				SubIFD	1 = Rectangular
	LinearizationTable			= 0xc618,	//uint16[n]			SubIFD	 
	BlackLevelRepeatDim			= 0xc619,	//uint16[2]			SubIFD	 
	BlackLevel					= 0xc61a,	//urational[n]		SubIFD	 
	BlackLevelDeltaH			= 0xc61b,	//rational[n]		SubIFD	 
	BlackLevelDeltaV			= 0xc61c,	//rational[n]		SubIFD	 
	WhiteLevel					= 0xc61d,	//int32u[n]			SubIFD	 
	DefaultScale				= 0xc61e,	//urational[2]		SubIFD	 
	DefaultCropOrigin			= 0xc61f,	//int32u[2]			SubIFD	 
	DefaultCropSize				= 0xc620,	//int32u[2]			SubIFD	 
	ColorMatrix1				= 0xc621,	//rational[n]		IFD0	 
	ColorMatrix2				= 0xc622,	//rational[n]		IFD0	 
	CameraCalibration1			= 0xc623,	//rational[n]		IFD0	 
	CameraCalibration2			= 0xc624,	//rational[n]		IFD0	 
	ReductionMatrix1			= 0xc625,	//rational[n]		IFD0	 
	ReductionMatrix2			= 0xc626,	//rational[n]		IFD0	 
	AnalogBalance				= 0xc627,	//urational[n]		IFD0	 
	AsShotNeutral				= 0xc628,	//urational[n]		IFD0	 
	AsShotWhiteXY				= 0xc629,	//urational[2]		IFD0	 
	BaselineExposure			= 0xc62a,	//rational			IFD0	 
	BaselineNoise				= 0xc62b,	//urational			IFD0	 
	BaselineSharpness			= 0xc62c,	//urational			IFD0	 
	BayerGreenSplit				= 0xc62d,	//uint32			SubIFD	 
	LinearResponseLimit			= 0xc62e,	//urational			IFD0	 
	CameraSerialNumber			= 0xc62f,	//string			IFD0	 
	DNGLensInfo					= 0xc630,	//urational[4]		IFD0	 
	ChromaBlurRadius			= 0xc631,	//urational			SubIFD	 
	AntiAliasStrength			= 0xc632,	//urational			SubIFD	 
	ShadowScale					= 0xc633,	//urational			IFD0	 
	SR2Private					= 0xc634,	//
	MakerNoteSafety				= 0xc635,	//uint16			IFD0	0 = Unsafe
	RawImageSegmentation		= 0xc640,	//	(used in segmented Canon CR2 images. 3 numbers 1. Number of segments minus one; 2. Pixel width of segments except last; 3. Pixel width of last segment)
	CalibrationIlluminant1		= 0xc65a,	//uint16			IFD0	--> EXIF LightSource Values
	CalibrationIlluminant2		= 0xc65b,	//uint16			IFD0	--> EXIF LightSource Values
	BestQualityScale			= 0xc65c,	//urational			SubIFD	 
	RawDataUniqueID				= 0xc65d,	//int8[16]			IFD0	 
	AliasLayerMetadata			= 0xc660,	//	(used by Alias Sketchbook Pro)
	OriginalRawFileName			= 0xc68b,	//string			IFD0	 
	OriginalRawFileData			= 0xc68c,	//undef				IFD0	--> DNG OriginalRaw Tags
	ActiveArea					= 0xc68d,	//int32u[4]			SubIFD	 
	MaskedAreas					= 0xc68e,	//int32u[n]			SubIFD	 
	AsShotICCProfile			= 0xc68f,	//undef				IFD0	--> ICC_Profile Tags
	AsShotPreProfileMatrix		= 0xc690,	//rational[n]		IFD0	 
	CurrentICCProfile			= 0xc691,	//undef				IFD0	--> ICC_Profile Tags
	CurrentPreProfileMatrix		= 0xc692,	//rational[n]		IFD0	 
	ColorimetricReference		= 0xc6bf,	//uint16			IFD0	 
	SRawType					= 0xc6c5,	//no				IFD0	 
	PanasonicTitle				= 0xc6d2,	//undef				IFD0	(proprietary Panasonic tag used for baby/pet name, etc)
	PanasonicTitle2				= 0xc6d3,	//undef				IFD0	(proprietary Panasonic tag used for baby/pet name with age)
	CameraCalibrationSig		= 0xc6f3,	//string			IFD0	 
	ProfileCalibrationSig		= 0xc6f4,	//string			IFD0	 
	ProfileIFD					= 0xc6f5,	//--> EXIF Tags		IFD0	
	AsShotProfileName			= 0xc6f6,	//string			IFD0	 
	NoiseReductionApplied		= 0xc6f7,	//urational			SubIFD	 
	ProfileName					= 0xc6f8,	//string			IFD0	 
	ProfileHueSatMapDims		= 0xc6f9,	//int32u[3]			IFD0	 
	ProfileHueSatMapData1		= 0xc6fa,	//float[n]			IFD0	 
	ProfileHueSatMapData2		= 0xc6fb,	//float[n]			IFD0	 
	ProfileToneCurve			= 0xc6fc,	//float[n]			IFD0	 
	ProfileEmbedPolicy			= 0xc6fd,	//uint32			IFD0	0 = Allow Copying
	ProfileCopyright			= 0xc6fe,	//string			IFD0	 
	ForwardMatrix1				= 0xc714,	//rational[n]		IFD0	 
	ForwardMatrix2				= 0xc715,	//rational[n]		IFD0	 
	PreviewApplicationName		= 0xc716,	//string			IFD0	 
	PreviewApplicationVersion	= 0xc717,	//string			IFD0	 
	PreviewSettingsName			= 0xc718,	//string			IFD0	 
	PreviewSettingsDigest		= 0xc719,	//int8				IFD0	 
	PreviewColorSpace			= 0xc71a,	//uint32			IFD0	0 = Unknown
	PreviewDateTime				= 0xc71b,	//string			IFD0	 
	RawImageDigest				= 0xc71c,	//int8[16]			IFD0	 
	OriginalRawFileDigest		= 0xc71d,	//int8[16]			IFD0	 
	SubTileBlockSize			= 0xc71e,	//
	RowInterleaveFactor			= 0xc71f,	//
	ProfileLookTableDims		= 0xc725,	//int32u[3]			IFD0	 
	ProfileLookTableData		= 0xc726,	//float[n]			IFD0	 
	OpcodeList1					= 0xc740,	//undef~			SubIFD	
	OpcodeList2					= 0xc741,	//undef~			SubIFD	
	OpcodeList3					= 0xc74e,	//undef~			SubIFD	
	NoiseProfile				= 0xc761,	//double[n]			SubIFD	 
	TimeCodes					= 0xc763,	//int8[n]			IFD0	 
	FrameRate					= 0xc764,	//rational			IFD0	 
	TStop						= 0xc772,	//urational[n]		IFD0	 
	ReelName					= 0xc789,	//string			IFD0	 
	OriginalDefaultFinalSize	= 0xc791,	//int32u[2]			IFD0	 
	OriginalBestQualitySize		= 0xc792,	//int32u[2]			IFD0	(called OriginalBestQualityFinalSize by the DNG spec)
	OriginalDefaultCropSize		= 0xc793,	//urational[2]		IFD0	 
	CameraLabel					= 0xc7a1,	//string			IFD0	 
	ProfileHueSatMapEncoding	= 0xc7a3,	//uint32			IFD0	0 = Linear
	ProfileLookTableEncoding	= 0xc7a4,	//uint32			IFD0	0 = Linear
	BaselineExposureOffset		= 0xc7a5,	//rational			IFD0	 
	DefaultBlackRender			= 0xc7a6,	//uint32			IFD0	0 = Auto
	NewRawImageDigest			= 0xc7a7,	//int8[16]			IFD0	 
	RawToPreviewGain			= 0xc7a8,	//double			IFD0	 
	CacheVersion				= 0xc7aa,	//uint32			SubIFD2	 
	DefaultUserCrop				= 0xc7b5,	//urational[4]		SubIFD	 
	NikonNEFInfo				= 0xc7d5,	//--> Nikon NEFInfo Tags
	DepthFormat					= 0xc7e9,	//uint16			IFD0	(tags 0xc7e9-0xc7ee added by DNG 1.5.0.0)
	DepthNear					= 0xc7ea,	//urational			IFD0	 
	DepthFar					= 0xc7eb,	//urational			IFD0	 
	DepthUnits					= 0xc7ec,	//uint16			IFD0	0 = Unknown
	DepthMeasureType			= 0xc7ed,	//uint16			IFD0	0 = Unknown
	EnhanceParams				= 0xc7ee,	//string			IFD0	 
	ProfileGainTableMap			= 0xcd2d,	//undef				SubIFD	 
	SemanticName				= 0xcd2e,	//no				SubIFD	 
	SemanticInstanceID			= 0xcd30,	//no				SubIFD	 
	CalibrationIlluminant3		= 0xcd31,	//uint16			IFD0	--> EXIF LightSource Values
	CameraCalibration3			= 0xcd32,	//rational[n]		IFD0	 
	ColorMatrix3				= 0xcd33,	//rational[n]		IFD0	 
	ForwardMatrix3				= 0xcd34,	//rational[n]		IFD0	 
	IlluminantData1				= 0xcd35,	//undef				IFD0	 
	IlluminantData2				= 0xcd36,	//undef				IFD0	 
	IlluminantData3				= 0xcd37,	//undef				IFD0	 
	MaskSubArea					= 0xcd38,	//no				SubIFD	 
	ProfileHueSatMapData3		= 0xcd39,	//float[n]			IFD0	 
	ReductionMatrix3			= 0xcd3a,	//rational[n]		IFD0	 
	RGBTables					= 0xcd3b,	//undef				IFD0	 
	Padding						= 0xea1c,	//undef				ExifIFD	 
	OffsetSchema				= 0xea1d,	//int32				ExifIFD	(Microsoft's ill-conceived maker note offset difference)

//Photoshop Camera RAW
	RAWOwnerName				= 0xfde8,	//string			ExifIFD
	RAWSerialNumber				= 0xfde9,	//string			ExifIFD	 
	RAWLens						= 0xfdea,	//string			ExifIFD	 
	KDC_IFD						= 0xfe00,	//--> Kodak KDC_IFD Tags
	RAWRawFile					= 0xfe4c,	//string			ExifIFD	 
	RAWConverter				= 0xfe4d,	//string			ExifIFD	 
	RAWWhiteBalance				= 0xfe4e,	//string			ExifIFD	 
	RAWExposure					= 0xfe51,	//string			ExifIFD	 
	RAWShadows					= 0xfe52,	//string			ExifIFD	 
	RAWBrightness				= 0xfe53,	//string			ExifIFD	 
	RAWContrast					= 0xfe54,	//string			ExifIFD	 
	RAWSaturation				= 0xfe55,	//string			ExifIFD	 
	RAWSharpness				= 0xfe56,	//string			ExifIFD	 
	RAWSmoothness				= 0xfe57,	//string			ExifIFD	 
	RAWMoireFilter				= 0xfe58,	//string			ExifIFD

//local for exchange with ms property system
	Duration					= 0xff00,	//uint64	in 100ns units

	Music_Genre,							//string
	Music_Composer,							//array of strings
	Music_Conductor,						//array of strings
	Music_Period,							//string
	Music_Mood,								//string
	Music_PartOfSet,						//string
	Music_InitialKey,						//string
	Music_BeatsPerMinute,					//string

	Director,								//array of strings
	Producer,								//array of strings
	Writer,									//array of strings
	Publisher,								//string
	Distributor,							//string
};

struct EXIF {

	template<bool be> struct raw_entry {
		typedef	packed<endian_t<uint32, be>>	uint32e;
		endian_t<TAG, be>	tag;
		endian_t<TYPE, be>	type;
		uint32e				count;
		union {
			uint32e		offset;
			uint8		data[4];
		};
		raw_entry()	{}
		raw_entry(TAG tag, TYPE type, uint32 count, uint32 value) : tag(tag), type(type), count(count), offset(value) {}

		constexpr auto	size()	const { return type_length(type) * count; }
		constexpr auto	fits()	const { return size() <= 4; }

		template<typename T> T	_as(const void *base) const {
			return (T)*(const endian_t<T, be>*)(sizeof(T) <= 4 ? data : (const uint8*)base + offset);
		}

		int64	as_int(const void *base) const {
			switch (type) {
				case TYPE::BYTE:	return _as<uint8> (base);
				case TYPE::SHORT:	return _as<uint16>(base);
				case TYPE::LONG:	return _as<uint32>(base);
				case TYPE::LONG8:	return _as<uint64>(base);
				case TYPE::SBYTE:	return _as<uint8> (base);
				case TYPE::SSHORT:	return _as<uint16>(base);
				case TYPE::SLONG:	return _as<uint32>(base);
				case TYPE::SLONG8:	return _as<int64> (base);
				default:			throw("unrecognised type");
			}
		}
		double	as_float(const void *base) const {
			switch (type) {
				case TYPE::FLOAT:		return _as<float>(base);
				case TYPE::DOUBLE:		return _as<double>(base);
				case TYPE::RATIONAL:	return _as<rational<uint32>>(base);
				case TYPE::SRATIONAL:	return _as<rational<int32>>(base);
				default:				return as_int(base);
			}
		}

		template<typename T> constexpr enable_if_t<is_int<T>, T>	convert(const void *base)	const { return T(as_int(base)); }
		template<typename T> constexpr enable_if_t<is_float<T>, T>	convert(const void *base)	const { return T(as_float(base)); }

		template<typename T> T	as(const void *base) const {
			if (type == get_type<T>())
				return _as<T>(base);
			return convert<T>(base);
		}
		template<> const char*	as<const char*>(const void *base) const {
			return (const char*)base + offset;
		}

	};
	template<bool be> struct raw_table {
		endian_t<uint16, be>	count;
		raw_entry<be>			_entries[];
		auto	entries(const void *base) const { return with_param(make_range_n(_entries, count), base); }
		auto	find(TAG tag, const void *base) {
			for (auto& i : entries(base)) {
				if (i->tag == tag)
					return i;
			}
			return nullptr;
		}
	};

	template<bool be> struct head {
		enum {ENDIAN = be ? 0x4D4D : 0x4949};
		typedef	endian_t<uint16, be>	uint16e;
		typedef	endian_t<uint32, be>	uint32e;

		uint16e		endian;
		uint16e		star;
		uint32e		start;

		constexpr bool	valid()		const { return endian == ENDIAN && star == '*' && start >= 8; }
		//constexpr auto	get_table()		const { return (const raw_table<be>*)((const uint8*)this + start); }
		//constexpr auto	get_entries()	const { return get_table()->entries(this); }
		//auto			find(TAG tag)	const { return get_table()->find(tag, this); }
	};

	struct date : DateTime {
		date(DateTime d) : DateTime(d) {}
		date(const char *s)	{
			int	year, month, day, hours, mins, secs;
			if (scan_string(s, "%i:%i:%i %i:%i:%i", &year, &month, &day, &hours, &mins, &secs) == 6) {
				DateTime::operator=(Day(Date::Days(year, month, day)) + Duration::Hours(hours) + Duration::Mins(mins) + Duration::Secs(secs));
			}
		}
		friend size_t to_string(char *s, const date& dt) {
			fixed_accum	a(s, 64);
			Date		d(dt.Day());
			struct TimeOfDay	t(dt.TimeOfDay());
			a	<< d.year << ':' << formatted(+d.month, FORMAT::ZEROES, 2) << ':' << formatted(+d.day, FORMAT::ZEROES, 2) << ' '
				<< formatted(t.Hour(), FORMAT::ZEROES, 2) << ':' << formatted(t.Min(), FORMAT::ZEROES, 2) << ':' << formatted((int)t.Sec(), FORMAT::ZEROES, 2);
			return a.getp() - s;
		}

	};
};

struct EXIF_parser : EXIF {
	struct entry {
		const void	*base;
		const void	*raw;
		TAG			tag;
		TYPE		type;
		uint32		count;
		bool		is_be;
		template<bool be> entry(param_element<const raw_entry<be>&, const void*> e) : base(e.p), raw(&e.t), tag(e->tag), type(e->type), count(get(e->count)), is_be(be) {}
		template<typename T> T	as() const {
			return is_be
				? ((const raw_entry<true>*)raw)->as<T>(base)
				: ((const raw_entry<false>*)raw)->as<T>(base);
		}
	};

	struct table : dynamic_array<entry> {
		using dynamic_array<entry>::dynamic_array;
		const entry*	find(TAG tag) const {
			for (auto& i : *this) {
				if (i.tag == tag)
					return &i;
			}
			return nullptr;
		}
	};

	const_memory_block		start;
	table					root;

	template<bool be> auto get_table0(uint32 offset) const {
		return ((const raw_table<be>*)(start + offset))->entries(start);
	}

	EXIF_parser(const_memory_block data) : start(data + (*(const uint32be*)data + 4)) {
		const head<false>* h0 = start;
		if (h0->valid())
			root = get_table0<false>(h0->start);

		const head<true>* h1 = start;
		if (h1->valid())
			root = get_table0<true>(h1->start);
	}

	table get_table(uint32 offset) const {
		const head<false>* h0 = start;
		if (h0->valid())
			return get_table0<false>(offset);

		const head<true>* h1 = start;
		if (h1->valid())
			return get_table0<true>(offset);

		return none;
	}
};

struct EXIF_maker : EXIF {
	malloc_block2	data;
	dynamic_array<raw_entry<iso_bigendian>>	root;

	struct pre_head {
		packed<uint32be>	size	= 6;
		packed<uint32>		tag		= "Exif"_u32;
		uint16				x		= 0;
	};

	template<typename T> void add_entry(TAG tag, T t) {
		uint32	value	= 0;
		void	*p		= &value;
		if (sizeof(T) > 4) {
			p		= data.extend(sizeof(T));
			value	= data.offset_of(p);
		}
		memcpy(p, &t, sizeof(T));
		root.emplace_back(tag, get_type<T>(), 1, value);
	}
	template<typename T, int N> void add_entry(TAG tag, T (&t)[N]) {
		uint32	value	= 0;
		void	*p		= &value;
		if (sizeof(T) * N > 4) {
			p		= data.extend(sizeof(T) * N);
			value	= data.offset_of(p);
		}
		memcpy(p, &t, sizeof(T) * N);
		root.emplace_back(tag, get_type<T>(), N, value);
	}
	void add_entry(TAG tag, DateTime t) {
		char	*p		= data.extend(64);
		auto	n		= to_string(p, date(t));
		p[n++]			= 0;
		root.emplace_back(tag, TYPE::ASCII, n, data.offset_of(p));
		data.resize(data.offset_of(p) + n);
	}

	malloc_block	get_data() {
		uint32			offset	= sizeof(head<iso_bigendian>) + sizeof(uint32) + root.size() * sizeof(raw_entry<iso_bigendian>);
		malloc_block	out(sizeof(pre_head) + offset + data.size());

		new(out.p) pre_head;

		head<iso_bigendian>	*h	= out + sizeof(pre_head);
		h->endian	= h->ENDIAN;
		h->star		= '*';
		h->start	= 8;

		data.copy_to((uint8*)h + offset);

		auto	t	= (raw_table<iso_bigendian>*)(h + 1);
		t->count	= root.size();
		auto	e	= t->_entries;
		for (auto& i : root) {
			*e = i;
			if (!e->fits())
				e->offset += offset;
			++e;
		}
		return out;
	}
};


}	// namespace tiff

#endif // TIFF_H
