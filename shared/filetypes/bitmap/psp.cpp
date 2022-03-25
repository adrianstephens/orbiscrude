#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "comms/zlib_stream.h"

//-----------------------------------------------------------------------------
//	Corel PaintShop Pro bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

class PSPFileHandler : public FileHandler {

	// Block identifiers.

	enum PSPBlockID {
		PSP_IMAGE_BLOCK = 0,		// General Image Attributes Block (main)
		PSP_CREATOR_BLOCK,			// Creator Data Block (main)
		PSP_COLOR_BLOCK,			// Color Palette Block (main and sub)
		PSP_LAYER_START_BLOCK,		// Layer Bank Block (main)
		PSP_LAYER_BLOCK,			// Layer Block (sub)
		PSP_CHANNEL_BLOCK,			// Channel Block (sub)
		PSP_SELECTION_BLOCK,		// Selection Block (main)
		PSP_ALPHA_BANK_BLOCK,		// Alpha Bank Block (main)
		PSP_ALPHA_CHANNEL_BLOCK,	// Alpha Channel Block (sub)
		PSP_COMPOSITE_IMAGE_BLOCK,	// Composite Image Block (sub)
		PSP_EXTENDED_DATA_BLOCK,	// Extended Data Block (main)
		PSP_TUBE_BLOCK,				// Picture Tube Data Block (main)
		PSP_ADJUSTMENT_EXTENSION_BLOCK, // Adjustment Layer Block (sub)
		PSP_VECTOR_EXTENSION_BLOCK, // Vector Layer Block (sub)
		PSP_SHAPE_BLOCK,			// Vector Shape Block (sub)
		PSP_PAINTSTYLE_BLOCK,		// Paint Style Block (sub)
		PSP_COMPOSITE_IMAGE_BANK_BLOCK, // Composite Image Bank (main)
		PSP_COMPOSITE_ATTRIBUTES_BLOCK, // Composite Image Attr. (sub)
		PSP_JPEG_BLOCK,				// JPEG Image Block (sub)
		PSP_LINESTYLE_BLOCK,		// Line Style Block (sub)
		PSP_TABLE_BANK_BLOCK,		// Table Bank Block (main)
		PSP_TABLE_BLOCK,			// Table Block (sub)
		PSP_PAPER_BLOCK,			// Vector Table Paper Block (sub)
		PSP_PATTERN_BLOCK,			// Vector Table Pattern Block (sub)
		PSP_GRADIENT_BLOCK,			// Vector Table Gradient Block (not used)
		PSP_GROUP_EXTENSION_BLOCK,	// Group Layer Block (sub)
		PSP_MASK_EXTENSION_BLOCK,	// Mask Layer Block (sub)
		PSP_BRUSH_BLOCK,			// Brush Data Block (main)
	};

	// Possible metrics used to measure resolution.
	enum PSP_METRIC {
		PSP_METRIC_UNDEFINED = 0,	// Metric unknown
		PSP_METRIC_INCH,			// Resolution is in inches
		PSP_METRIC_CM,				// Resolution is in centimeters
	};


	// Creator application identifiers.
	enum PSPCreatorAppID {
		PSP_CREATOR_APP_UNKNOWN = 0,     // Creator application unknown
		PSP_CREATOR_APP_PAINT_SHOP_PRO,  // Creator is Paint Shop Pro
	};

	// Creator field types.
	enum PSPCreatorFieldID {
		PSP_CRTR_FLD_TITLE = 0,		// Image document title field
		PSP_CRTR_FLD_CRT_DATE,		// Creation date field
		PSP_CRTR_FLD_MOD_DATE,		// Modification date field
		PSP_CRTR_FLD_ARTIST,		// Artist name field
		PSP_CRTR_FLD_CPYRGHT,		// Copyright holder name field
		PSP_CRTR_FLD_DESC,			// Image document description field
		PSP_CRTR_FLD_APP_ID,		// Creating app id field
		PSP_CRTR_FLD_APP_VER,		// Creating app version field
	};

	// Extended data field types.
	enum PSPExtendedDataID {
		PSP_XDATA_TRNS_INDEX = 0,	// Transparency index field
		PSP_XDATA_GRID,				// Image grid information
		PSP_XDATA_GUIDE,			// Image guide information
		PSP_XDATA_EXIF,				// Image EXIF information
	};

	// Grid units type.
	enum PSPGridUnitsType {
		keGridUnitsPixels = 0,		// Grid units is pixels
		keGridUnitsInches,			// Grid units is inches
		keGridUnitsCentimeters		// Grid units is centimeters
	};

	// Guide orientation type.
	enum PSPGuideOrientationType {
		keHorizontalGuide = 0,		// Horizontal guide direction
		keVerticalGuide				// Vertical guide direction
	};

	// Bitmap types.
	enum PSPDIBType {
		PSP_DIB_IMAGE = 0,			// Layer color bm
		PSP_DIB_TRANS_MASK,			// Layer transparency mask bm
		PSP_DIB_USER_MASK,			// Layer user mask bm
		PSP_DIB_SELECTION,			// Selection mask bm
		PSP_DIB_ALPHA_MASK,			// Alpha channel mask bm
		PSP_DIB_THUMBNAIL,			// Thumbnail bm
		PSP_DIB_THUMBNAIL_TRANS_MASK,	// Thumbnail transparency mask
		PSP_DIB_ADJUSTMENT_LAYER,	// Adjustment layer bm
		PSP_DIB_COMPOSITE,			// Composite image bm
		PSP_DIB_COMPOSITE_TRANS_MASK,	// Composite image transparency
		PSP_DIB_PAPER,				// Paper bm
		PSP_DIB_PATTERN,			// Pattern bm
		PSP_DIB_PATTERN_TRANS_MASK,	// Pattern transparency mask
	};

	// Type of image in the composite image bank block.
	enum PSPCompositeImageType {
		PSP_IMAGE_COMPOSITE = 0,  // Composite Image
		PSP_IMAGE_THUMBNAIL,      // Thumbnail Image
	};

	// Channel types.
	enum PSPChannelType {
		PSP_CHANNEL_COMPOSITE = 0,	// Channel of single channel bm
		PSP_CHANNEL_RED,			// Red channel of 24-bit bm
		PSP_CHANNEL_GREEN,			// Green channel of 24-bit bm
		PSP_CHANNEL_BLUE,			// Blue channel of 24-bit bm
	};

	// Possible types of compression.
	enum PSPCompression {
		PSP_COMP_NONE = 0,			// No compression
		PSP_COMP_RLE,				// RLE compression
		PSP_COMP_LZ77,				// LZ77 compression
		PSP_COMP_JPEG				// JPEG compression (only used by thumbnail and composite image)
	};

	// Layer types.
	enum PSPLayerType {
		keGLTUndefined = 0,			// Undefined layer type
		keGLTRaster,				// Standard raster layer
		keGLTFloatingRasterSelection,// Floating selection (raster)
		keGLTVector,				// Vector layer
		keGLTAdjustment,			// Adjustment layer
		keGLTGroup,					// Group layer
		keGLTMask					// Mask layer
	};

	// Layer flags.
	enum PSPLayerProperties {
		keVisibleFlag		= 0x00000001,	// Layer is visible
		keMaskPresenceFlag	= 0x00000002,	// Layer has a mask
	};

	// Blend modes.
	enum PSPBlendModes {
		LAYER_BLEND_NORMAL,
		LAYER_BLEND_DARKEN,
		LAYER_BLEND_LIGHTEN,
		LAYER_BLEND_LEGACY_HUE,
		LAYER_BLEND_LEGACY_SATURATION,
		LAYER_BLEND_LEGACY_COLOR,
		LAYER_BLEND_LEGACY_LUMINOSITY,
		LAYER_BLEND_MULTIPLY,
		LAYER_BLEND_SCREEN,
		LAYER_BLEND_DISSOLVE,
		LAYER_BLEND_OVERLAY,
		LAYER_BLEND_HARD_LIGHT,
		LAYER_BLEND_SOFT_LIGHT,
		LAYER_BLEND_DIFFERENCE,
		LAYER_BLEND_DODGE,
		LAYER_BLEND_BURN,
		LAYER_BLEND_EXCLUSION,
		LAYER_BLEND_TRUE_HUE,
		LAYER_BLEND_TRUE_SATURATION,
		LAYER_BLEND_TRUE_COLOR,
		LAYER_BLEND_TRUE_LIGHTNESS,
		LAYER_BLEND_ADJUST = 255,
	};

	// Adjustment layer types.
	enum PSPAdjustmentLayerType {
		keAdjNone = 0,			// Undefined adjustment layer type
		keAdjLevel,				// Level adjustment
		keAdjCurve,				// Curve adjustment
		keAdjBrightContrast,	// Brightness-contrast adjustment
		keAdjColorBal,			// Color balance adjustment
		keAdjHSL,				// HSL adjustment
		keAdjChannelMixer,		// Channel mixer adjustment
		keAdjInvert,			// Invert adjustment
		keAdjThreshold,			// Threshold adjustment
		keAdjPoster				// Posterize adjustment
	};

	// Vector shape types.
	enum PSPVectorShapeType {
		keVSTUnknown = 0,	// Undefined vector type
		keVSTText,			// Shape represents lines of text
		keVSTPolyline,		// Shape represents a multiple segment line
		keVSTEllipse,		// Shape represents an ellipse (or circle)
		keVSTPolygon,		// Shape represents a closed polygon
		keVSTGroup,			// Shape represents a group shape
	};

	// Shape property flags
	enum PSPShapeProperties {
		keShapeAntiAliased	= 0x00000001,  // Shape is anti-aliased
		keShapeSelected		= 0x00000002,  // Shape is selected
		keShapeVisible		= 0x00000004,  // Shape is visible
	};

	// Polyline node type flags.
	enum PSPPolylineNodeTypes {
		keNodeUnconstrained	= 0x0000, // Default node type
		keNodeSmooth	 	= 0x0001, // Node is smooth
		keNodeSymmetric	 	= 0x0002, // Node is symmetric
		keNodeAligned	 	= 0x0004, // Node is aligned
		keNodeActive	 	= 0x0008, // Node is active
		keNodeLocked	 	= 0x0010, // Node is locked
		keNodeSelected	 	= 0x0020, // Node is selected
		keNodeVisible	 	= 0x0040, // Node is visible
		keNodeClosed	 	= 0x0080, // Node is closed
	};

	// Paint style types.
	enum PSPPaintStyleType {
		keStyleNone		= 0x0000,  // No paint style info applies
		keStyleColor	= 0x0001,  // Color paint style info
		keStyleGradient	= 0x0002,  // Gradient paint style info
		keStylePattern	= 0x0004,  // Pattern paint style info
		keStylePaper	= 0x0008,  // Paper paint style info
		keStylePen		= 0x0010,  // Organic pen paint style info
	};

	// Gradient type.
	enum PSPStyleGradientType {
		keSGTLinear = 0,			// Linera gradient type
		keSGTRadial,				// Radial gradient type
		keSGTRectangular,			// Rectangulat gradient type
		keSGTSunburst				// Sunburst gradient type
	};

	// Paint Style Cap Type (Start & End).
	enum PSPStyleCapType {
		keSCTCapFlat = 0,			// Flat cap type (was round in psp6)
		keSCTCapRound,				// Round cap type (was square in psp6)
		keSCTCapSquare,				// Square cap type (was flat in psp6)
		keSCTCapArrow,				// Arrow cap type
		keSCTCapCadArrow,			// Cad arrow cap type
		keSCTCapCurvedTipArrow,		// Curved tip arrow cap type
		keSCTCapRingBaseArrow,		// Ring base arrow cap type
		keSCTCapFluerDelis,			// Fluer deLis cap type
		keSCTCapFootball,			// Football cap type
		keSCTCapXr71Arrow,			// Xr71 arrow cap type
		keSCTCapLilly,				// Lilly cap type
		keSCTCapPinapple,			// Pinapple cap type
		keSCTCapBall,				// Ball cap type
		keSCTCapTulip				// Tulip cap type
	};

	// Paint Style Join Type.
	enum PSPStyleJoinType {
		keSJTJoinMiter = 0,			// Miter join type
		keSJTJoinRound,				// Round join type
		keSJTJoinBevel				// Bevel join type
	};

	// Organic pen type.
	enum PSPStylePenType {
		keSPTOrganicPenNone = 0,	// Undefined pen type
		keSPTOrganicPenMesh,		// Mesh pen type
		keSPTOrganicPenSand,		// Sand pen type
		keSPTOrganicPenCurlicues,	// Curlicues pen type
		keSPTOrganicPenRays,		// Rays pen type
		keSPTOrganicPenRipple,		// Ripple pen type
		keSPTOrganicPenWave,		// Wave pen type
		keSPTOrganicPen				// Generic pen type
	};

	// Text element types.
	enum PSPTextElementType {
		keTextElemUnknown = 0,		// Undefined text element type
		keTextElemChar,				// A single character code
		keTextElemCharStyle,		// A character style change
		keTextElemLineStyle			// A line style change
	};

	// Text alignment types.
	enum PSPTextAlignment {
		keTextAlignmentLeft = 0,	// Left text alignment
		keTextAlignmentCenter,		// Center text alignment
		keTextAlignmentRight		// Right text alignment
	};

	// Character style flags.
	enum PSPCharacterProperties {
		keStyleItalic		= 0x00000001,	// Italic property bit
		keStyleStruck		= 0x00000002,	// Strike-out property bit
		keStyleUnderlined	= 0x00000004,	// Underlined property bit
		keStyleWarped		= 0x00000008,	// Warped property bit
		keStyleAntiAliased	= 0x00000010,	// Anti-aliased property bit
	};

	// Table type.
	enum PSPTableType {
		keTTUndefined = 0,			// Undefined table type
		keTTGradientTable,			// Gradient table type
		keTTPaperTable,				// Paper table type
		keTTPatternTable			// Pattern table type
	};

	// Picture tube placement mode.
	enum TubePlacementMode {
		tpmRandom,					// Place tube images in random intervals
		tpmConstant,				// Place tube images in constant intervals
	};

	// Picture tube selection mode.
	enum TubeSelectionMode {
		tsmRandom,		// Randomly select the next image in tube to display
		tsmIncremental,	// Select each tube image in turn
		tsmAngular,		// Select image based on cursor direction
		tsmPressure,	// Select image based on pressure (from pressure-sensitive pad)
		tsmVelocity,	// Select image based on cursor speed
	};

	// Graphic contents flags.
	enum PSPGraphicContents {
		// Layer types
		keGCRasterLayers      		= 0x00000001,	// At least one raster layer
		keGCVectorLayers      		= 0x00000002,	// At least one vector layer
		keGCAdjustmentLayers  		= 0x00000004,	// At least one adjust. layer

		// Additional attributes
		keGCThumbnail				= 0x01000000,	// Has a thumbnail
		keGCThumbnailTransparency	= 0x02000000,	// Thumbnail transp.
		keGCComposite				= 0x04000000,	// Has a composite image
		keGCCompositeTransparency	= 0x08000000,	// Composite transp.
		keGCFlatImage				= 0x10000000,	// Just a background
		keGCSelection				= 0x20000000,	// Has a selection
		keGCFloatingSelectionLayer	= 0x40000000,	// Has float. selection
		keGCAlphaChannels			= 0x80000000,	// Has alpha channel(s)
	};

	typedef uint8	BYTE;
	typedef	double	DOUBLE;
	typedef	uint32	DWORD;
	typedef int32	LONG;
	struct	RECT	{int left, top, right, bottom;};
	struct	RGBQUAD	{BYTE r, g, b, a; };
	typedef	uint16	WORD;

	class PSPChunk {
		istream_ref		stream;
		streamptr	endofchunk;
	public:
		PSPChunk(istream_ref _stream) : stream(_stream) {
			endofchunk	= (DWORD)stream.get() - 4;
			endofchunk += stream.tell();
		}
		~PSPChunk()				{ stream.seek(endofchunk);	}
	};

	class PSPBlock {
		istream_ref		stream;
		streamptr	endofchunk;
		WORD		id;
	public:
		PSPBlock(istream_ref _stream) : stream(_stream) {
			DWORD	h	= stream.get();
			if (h != 'KB~') {
				id = 0xffff;
				return;
			}
			id			= stream.get();
			endofchunk	= (DWORD)stream.get();
			endofchunk += stream.tell();
		}
		~PSPBlock()				{ stream.seek(endofchunk);	}
		bool		Valid()		{ return id != 0xffff;		}
		WORD		ID()		{ return id;				}
		int			Length()	{ return max(int(endofchunk - stream.tell()), 0); }
	};

#pragma pack(1)

	struct PSPFileHeader {
		BYTE	signature[32];		//PSP file signature	Always 50 61 69 6E 74 20 53 68 6F 70 20 50 72 6F 20 49 6D 61 67 65 20 46 69 6C 65 0A 1A 00 00 00 00” (i.e., the string "Paint Shop Pro Image File\n\x1a", padded with zeroes to 32 bytes).
		WORD	majorversion;		//Currently 6. PSP files produced by Paint Shop Pro 5 have major version number 3. PSP files produced by Paint Shop Pro 6 have major version number 4. PSP files produced by Paint Shop Pro 7 have major version number 5. PSP files produced by Paint Shop Pro 8 have major version number 6.
		WORD	minorversion;		//Currently 0.
	};

	struct CompositeImageAttributes {
		LONG	width;				// Specifies the width of the composite image, in pixels.
		LONG	height;				// Specifies the height of the composite image, in pixels.
		WORD	bitdepth;			// Number of bits used to represent each color pixel of the composite image (must be 1, 4, 8, or 24).
		WORD	compression;		// Type of compression used to compress the composite image (one of PSPCompression, including PSP_COMP_JPEG).
		WORD	planecount;			// Number of planes in the composite image (this value must be 1).
		DWORD	colorcount;			// Number of colors in the image (2Bit depth).
		WORD	type;				// Type of composite image (one of PSPCompositeImageType).
	};

	struct GeneralImageAttributes {
		LONG	width;				// Width of the image.
		LONG	height;				// Height of the image.
		DOUBLE	resolution;			// Number of pixels per metric.
		BYTE	metric;				// Metric used for resolution (one of PSP_METRIC).
		WORD	compression;		// Type of compression used to compress all image document channels, except those in the composite images, which have their own compression type field (one of PSPCompression). The compression type PSP_COMP_JPEG is not valid here (only used for composite images).
		WORD	bitdepth;			// The bit depth of the color bm in each layer of the image document (must be 1, 4, 8, or 24).
		WORD	planecount;			// Number of planes in each layer of the image document (this value must be 1).
		DWORD	colorcount;			// Number of colors in each layer of the image document (2Bit depth).
		BYTE	greyscale;			// Indicates whether the color bm in each layer of image document is a greyscale (0 = not greyscale, 1 = greyscale).
		DWORD	imagesize;			// Sum of the sizes of all layer color bitmaps.
		LONG	activelayer;		// Identifies the layer that was active when the image document was saved.
		WORD	layercount;			// Number of layers in the document.
		DWORD	graphiccontents;	// A series of flags (in PSPGraphicContents) that helps define the image's graphic contents.
	};

	struct LayerInformation {
		BYTE	type;				// Type of layer (must be one of PSPLayerType).
		RECT	imagerect;			// Rectangle defining image border.
		RECT	savedimagerect;		// Rectangle within image rectangle that contains “significant” data (only the contents of this rectangle are saved to the file).
		BYTE	opacity;			// Overall layer opacity.
		BYTE	blendmode;			// Mode to use when blending layer (one of PSPBlendModes).
		BYTE	flags;				// A series of flags that help define the layer’s attributes. (PSPLayerProperties values, bitwise-ored together).
		BYTE	transprotected;		// TRUE if transparency is protected.
		BYTE	linkgroupid;		// Identifies group to which this layer belongs.
		RECT	maskrect;			// Rectangle defining user mask border.
		RECT	savedmaskrect;		// Rectangle within mask rectangle that contains “significant” data (only the contents of this rectangle are saved to the file).
		BYTE	masklinked;			// TRUE if mask linked to layer (i.e., mask moves relative to layer), FALSE otherwise.
		BYTE	maskdisabled;		// TRUE if mask is disabled, FALSE otherwise.
		BYTE	invertmask;			// TRUE if mask should be inverted when the layer is merged, FALSE otherwise.
		WORD	blendrangecount;	// Number of valid source-destination field pairs to follow (note, there are currently always 5 such pairs, but they are not necessarily all valid).
		BYTE	srceblendrange1[4];	// First source blend range value.
		BYTE	destblendrange1[4];	// First destination blend range value.
		BYTE	srceblendrange2[4];	// Second source blend range value.
		BYTE	destblendrange2[4];	// Second destination blend range value.
		BYTE	srceblendrange3[4];	// Third source blend range value.
		BYTE	destblendrange3[4];	// Third destination blend range value.
		BYTE	srceblendrange4[4];	// Fourth source blend range value.
		BYTE	destblendrange4[4];	// Fourth destination blend range value.
		BYTE	srceblendrange5[4];	// Fifth source blend range value.
		BYTE	destblendrange5[4];	// Fifth destination blend range value.
		BYTE	usehighlight;		// TRUE if use highlight color in layer palette.
		DWORD	highlight;			// Highlight color in layer palette (RGB).
	};

	struct ChannelInformation {
		DWORD	compressedlength;	// Size of the channel in compressed form (for the purpose of determining the length of the last field in this block, let’s call this size j).
		DWORD	uncompressedlength;	// Size of the channel in uncompressed form.
		WORD	bitmaptype;			// Type of bm for which this channel is intended (one of PSPDIBType).
		WORD	channeltype;		// Type of channel (one of PSPChannelType).
	};

	void ReadChannel(istream_ref file, GeneralImageAttributes &gia, bitmap *bm, int channel, uint32 compressedlength) {
		int	width	= bm->Width(), height = bm->Height();
		int	npixels	= width * height;

		switch (gia.compression) {
			case PSP_COMP_NONE: {
				malloc_block	buffer(width);
				for (int y = 0; y < height; y++) {
					file.readbuff(buffer, width);
					if (width % 4)
						file.seek_cur(4 - (width % 4));
					uint8	*dest = &bm->ScanLine(y)->r + channel;
					for (int i = 0; i < width; i++, dest += 4)
						*dest = ((uint8*)buffer)[i];
				}
			}
			break;

			case PSP_COMP_RLE: {
				uint8	*dest	= &bm->ScanLine(0)->r + channel;
				uint8*	end		= dest + npixels * 4;
				uint8	buffer[127];
				while (dest < end) {
					uint8	runcount = file.get();
					if (runcount > 128) {
						runcount -= 128;
						uint8	byte = file.get();
						memset(buffer, byte, runcount);
					} else {
						file.readbuff(buffer, runcount);
					}

					for (int i = 0; i < runcount; i++, dest += 4)
						*dest = buffer[i];
				}
				break;
			}

			case PSP_COMP_LZ77: {
				malloc_block	buffer2(npixels);
				
				zlib_reader(file).read(npixels);

				uint8	*dest	= &bm->ScanLine(0)->r + channel;
				for (int i = 0; i < npixels; i++, dest += 4)
					*dest = ((uint8*)buffer2)[i];
				break;
			}
		}
	}

#pragma pack()

	string getstring(istream_ref file) {
		return string::get(file, file.get());
	}

	const char*		GetExt() override { return "pspimage";	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		PSPFileHeader	header	= file.get();
		if (memcmp(header.signature, "Paint Shop Pro Image File\n\x1a\0\0\0\0", 32) != 0)
			return ISO_NULL;

		ISO_ptr<bitmap>		bm;
		GeneralImageAttributes	gia;

		for (;;) {
			PSPBlock	block(file);
			if (!block.Valid())
				break;

			switch (block.ID()) {
				case PSP_IMAGE_BLOCK: {
					PSPChunk	chunk(file);
					gia	= file.get();
					bm.Create(id)->Create(gia.width, gia.height);
					break;
				}

				case PSP_LAYER_START_BLOCK:
					while (block.Length()) {
						PSPBlock	block(file);
						if (!block.Valid())
							return ISO_NULL;
						switch (block.ID()) {
							case PSP_LAYER_BLOCK: {
								LayerInformation	li;
								string				name;
								int					nbitmaps, nchannels;
								{
									PSPChunk	chunk(file);
									name		= getstring(file);
									li			= file.get();
									//printf(name);
								}
								{
									PSPChunk	chunk(file);
									nbitmaps	= (uint16)file.get();
									nchannels	= (uint16)file.get();
								}
								while (block.Length()) {
									PSPBlock	block(file);
									if (!block.Valid())
										return ISO_NULL;
									switch (block.ID()) {
										case PSP_CHANNEL_BLOCK: {
											ChannelInformation	ci = (PSPChunk(file), file.get());
											ReadChannel(file, gia, bm, ci.bitmaptype == PSP_DIB_TRANS_MASK ? 3 : ci.channeltype - PSP_CHANNEL_RED, ci.compressedlength);
											break;
										}
									}
								}
								break;
							}
						}
					}
//					return bm;
					break;

				case PSP_COMPOSITE_IMAGE_BANK_BLOCK: {
					int	count;
					{
						PSPChunk	chunk(file);
						count		= (DWORD)file.get();
					}
					while (block.Length()) {
						PSPBlock	block(file);
						if (!block.Valid())
							return ISO_NULL;
						switch (block.ID()) {
							case PSP_COMPOSITE_ATTRIBUTES_BLOCK: {
								//CompositeImageAttributes	cia = (PSPChunk(file), file.get());
								break;
							}
							case PSP_COMPOSITE_IMAGE_BLOCK: {
								int		nbitmaps, nchannels;
								{
									PSPChunk	chunk(file);
									nbitmaps	= (uint16)file.get();
									nchannels	= (uint16)file.get();
								}
								while (block.Length()) {
									PSPBlock	block(file);
									if (!block.Valid())
										return ISO_NULL;
									switch (block.ID()) {
										case PSP_CHANNEL_BLOCK: {
											ChannelInformation	ci = (PSPChunk(file), file.get());
											ReadChannel(file, gia, bm, ci.bitmaptype == PSP_DIB_TRANS_MASK ? 3 : ci.channeltype - PSP_CHANNEL_RED, ci.compressedlength);
											break;
										}
									}
								}
								return bm;
								break;
							}

							case PSP_JPEG_BLOCK: {
								DWORD	compressedsize;
								DWORD	uncompressedsize;
								WORD	imagetype;
								{
									PSPChunk	chunk(file);
									compressedsize	= file.get();
									uncompressedsize= file.get();
									imagetype		= file.get();
								}
								break;
							}
						}
					}
					break;
				}
			}
		}

		return bm;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
		if (!bm)
			return false;
		return true;
	}
} psp;
