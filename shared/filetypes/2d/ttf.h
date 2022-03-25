#include "base/vector.h"

namespace ttf {
using namespace iso::bigendian_ns;
typedef iso::uintn<3>		uint24;			//24-bit unsigned integer.
typedef iso::fixed<16,16>	_fixed32;
typedef iso::fixed<2,14>	_fixed16;
typedef BE(_fixed32)		fixed32;	//32-bit signed fixed-point number (16.16)
typedef BE(_fixed16)		fixed16;	//32-bit signed fixed-point number (16.16)
typedef uint64				datetime64;	//Date represented in number of seconds since 12:00 midnight, January 1, 1904. The value is represented as a signed 64-bit integer.
typedef uint32				TAG;		//Array of four uint8s (length = 32 bits) used to identify a script, language system, feature, or baseline
typedef	int16				fword;
typedef	uint16				ufword;

struct SFNTHeader {
	fixed32	version;		// 0x00010000 for version 1.0 (or 'true' or 'typ1'); 'OTTO' for opentype
	uint16	num_tables;		// Number of tables.
	uint16	search_range;	// (Maximum power of 2 <= numTables) x 16.
	uint16	entry_selector;	// Log2(maximum power of 2 <= numTables).
	uint16	range_shift;	// NumTables x 16-searchRange.
};
struct TableRecord {
	TAG		tag;			// 4 -byte identifier.
	uint32	checksum;		// CheckSum for this table.
	uint32	offset;			// Offset from beginning of TrueType font file.
	uint32	length;			// Length of this table.
};
struct TTCHeader {
	TAG		tag;			// TrueType Collection ID string: 'ttcf'
	fixed32	version;		// Version of the TTC Header (1.0), 0x00010000 or (2.0), 0x00020000
	uint32	num_fonts;		// Number of fonts in TTC
	uint32	offsets[];		// Array of offsets to the sfnt_header for each font from the beginning of the file
};

struct TTCHeader2 {
	TAG		tag;			// Tag indicating that a DSIG table exists, 0x44534947 ('DSIG') (null if no signature)
	uint32	length;			// The length (in bytes) of the DSIG table (null if no signature)
	uint32	offset;			// The offset (in bytes) of the DSIG table from the beginning of the TTC file (null if no signature)
};

enum Tags {
	//truetype
	tag_acnt	= 'acnt',	//accent attachment
	tag_avar	= 'avar',	//axis variation
	tag_bdat	= 'bdat',	//bitmap data
	tag_bhed	= 'bhed',	//bitmap font header
	tag_bloc	= 'bloc',	//bitmap location
	tag_bsln	= 'bsln',	//baseline
	tag_cmap	= 'cmap',	//character code mapping
	tag_cvar	= 'cvar',	//CVT variation
	tag_cvt		= 'cvt ',	//control value
	tag_EBSC	= 'EBSC',	//embedded bitmap scaling control
	tag_fdsc	= 'fdsc',	//font descriptor
	tag_feat	= 'feat',	//layout feature
	tag_fmtx	= 'fmtx',	//font metrics
	tag_fpgm	= 'fpgm',	//font program
	tag_fvar	= 'fvar',	//font variation
	tag_gasp	= 'gasp',	//grid-fitting and scan-conversion procedure
	tag_glyf	= 'glyf',	//glyph outline
	tag_gvar	= 'gvar',	//glyph variation
	tag_hdmx	= 'hdmx',	//horizontal device metrics
	tag_head	= 'head',	//font header
	tag_hhea	= 'hhea',	//horizontal header
	tag_hmtx	= 'hmtx',	//horizontal metrics
	tag_hsty	= 'hsty',	//horizontal style
	tag_just	= 'just',	//justification
	tag_kern	= 'kern',	//kerning
	tag_lcar	= 'lcar',	//ligature caret
	tag_loca	= 'loca',	//glyph location
	tag_maxp	= 'maxp',	//maximum profile
	tag_mort	= 'mort',	//metamorphosis
	tag_morx	= 'morx',	//extended metamorphosis
	tag_name	= 'name',	//name
	tag_opbd	= 'opbd',	//optical bounds
	tag_OS_2	= 'OS/2',	//compatibility
	tag_post	= 'post',	//glyph name and PostScript compatibility
	tag_prep	= 'prep',	//control value program
	tag_prop	= 'prop',	//properties
	tag_trak	= 'trak',	//tracking
	tag_vhea	= 'vhea',	//vertical header
	tag_vmtx	= 'vmtx',	//vertical metrics
	tag_Zapf	= 'Zapf',	//glyph reference

	//opentype
	tag_CFF		= 'CFF ',	// PostScript font program (compact font format)
	tag_VORG	= 'VORG',	// Vertical Origin
	tag_EBDT	= 'EBDT',	// Embedded bitmap data
	tag_EBLC	= 'EBLC',	// Embedded bitmap location data
	tag_BASE	= 'BASE',	// Baseline data
	tag_GDEF	= 'GDEF',	// Glyph definition data
	tag_GPOS	= 'GPOS',	// Glyph positioning data
	tag_GSUB	= 'GSUB',	// Glyph substitution data
	tag_JSTF	= 'JSTF',	// Justification data
	tag_DSIG	= 'DSIG',	// Digital signature
	tag_LTSH	= 'LTSH',	// Linear threshold data
	tag_PCLT	= 'PCLT',	// PCL 5 data
	tag_VDMX	= 'VDMX',	// Vertical device metrics

	//obsolete
	//'fvar', 'MMSD', 'MMFX'
};

struct graphics_state {
	typedef iso::float2p		vec;
	typedef iso::uint16			val;
	typedef iso::fixed<26,6>	f26_6;
	typedef iso::uint32			stack_element;

	bool	auto_flip;
	f26_6	control_value_cut_in;
	uint8	delta_base;
	uint8	delta_shift;
	vec		dual_projection_vector;
	vec		freedom_vector;
	bool	instruct_control;
	val		loop;
	val		minimum_distance;
	vec		projection_vector;
	uint8	round_state;
	uint8	reference_point[3];
	bool	scan_control;
	val		single_width_cut_in;
	val		single_width_value;
	bool	zone_pointer[3];
};

enum instructions {
	AA			= 0x7f,	// Adjust Angle
	ABS			= 0x64,	// ABSolute value
	ADD			= 0x60,	// ADD
	ALIGNPTS	= 0x27,	// ALIGN Points
	ALIGNRP		= 0x3c,	// ALIGN to Reference Point
	AND			= 0x5c,	// logical AND
	CALL		= 0x2b,	// CALL function
	CEILING		= 0x67,	// CEILING
	CINDEX		= 0x25,	// Copy the INDEXed element to the top of the stack
	CLEAR		= 0x22,	// CLEAR the stack
	DEBUG		= 0x4f,	// DEBUG call
	DELTAC1		= 0x73,	// DELTA exception C1
	DELTAC2		= 0x74,	// DELTA exception C2
	DELTAC3		= 0x75,	// DELTA exception C3
	DELTAP1		= 0x5d,	// DELTA exception P1
	DELTAP2		= 0x71,	// DELTA exception P2
	DELTAP3		= 0x72,	// DELTA exception P3
	DEPTH		= 0x24,	// DEPTH of the stack
	DIV			= 0x62,	// DIVide
	DUP			= 0x20,	// DUPlicate top stack element
	EIF			= 0x59,	// End IF
	ELSE		= 0x1b,	// ELSE clause
	ENDF		= 0x2d,	// END Function definition
	EQ			= 0x54,	// EQual
	EVEN		= 0x57,	// EVEN
	FDEF		= 0x2c,	// Function DEFinition
	FLIPOFF		= 0x4e,	// set the auto FLIP Boolean to OFF
	FLIPON		= 0x4d,	// set the auto FLIP Boolean to ON
	FLIPPT		= 0x80,	// FLIP PoinT
	FLIPRGOFF	= 0x82,	// FLIP RanGe OFF
	FLIPRGON	= 0x81,	// FLIP RanGe ON
	FLOOR		= 0x66,	// FLOOR
	GC			= 0x46,	//-0x47[a] Get Coordinate projected onto the projection vector
	GETINFO		= 0x88,	// GET INFOrmation
	GFV			= 0x0d,	// Get Freedom Vector
	GPV			= 0x0c,	// Get Projection Vector
	GT			= 0x52,	// Greater Than
	GTEQ		= 0x53,	// Greater Than or EQual
	IDEF		= 0x89,	// Instruction DEFinition
	IF			= 0x58,	// IF test
	INSTCTRL	= 0x8e,	// INSTRuction execution ConTRoL
	IP			= 0x39,	// Interpolate Point
	ISECT		= 0x0f,	// moves point p to the InterSECTion of two lines
	IUP			= 0x30,	//-0x31[a] Interpolate Untouched Points through the outline
	JMPR		= 0x1c,	// JuMP Relative
	JROF		= 0x79,	// Jump Relative On False
	JROT		= 0x78,	// Jump Relative On True
	LOOPCALL	= 0x2a,	// LOOP and CALL function
	LT			= 0x50,	// Less Than
	LTEQ		= 0x51,	// Less Than or Equal
	MAX			= 0x8b,	// MAXimum of top two stack elements
	MD			= 0x49,	//-0x4a[a] Measure Distance
	MDAP		= 0x2e,	//-0x2f[a] Move Direct Absolute Point
	MDRP		= 0xc0,	//-0xdf[abcde] Move Direct Relative Point
	MIAP		= 0x3e,	//-0x3f[a] Move Indirect Absolute Point
	MIN			= 0x8c,	// MINimum of top two stack elements
	MINDEX		= 0x26,	// Move the INDEXed element to the top of the stack
	MIRP		= 0xe0,	//-0xff[abcde] Move Indirect Relative Point
	MPPEM		= 0x4b,	// Measure Pixels Per EM
	MPS			= 0x4c,	// Measure Point Size
	MSIRP		= 0x3a,	//-0x3b[a] Move Stack Indirect Relative Point
	MUL			= 0x63,	// MULtiply
	NEG			= 0x65,	// NEGate
	NEQ			= 0x55,	// Not EQual
	NOT			= 0x5c,	// logical NOT
	NPUSHB		= 0x40,	// PUSH N Bytes
	NPUSHW		= 0x41,	// PUSH N Words
	NROUND		= 0x6c,	//-0x6f[ab] No ROUNDing of value
	ODD			= 0x56,	// ODD
	OR			= 0x5b,	// logical OR
	POP			= 0x21,	// POP top stack element
	PUSHB		= 0xb0,	//-0xb7[abc] PUSH Bytes
	PUSHW		= 0xb8,	//-0xbf[abc] PUSH Words
	RCVT		= 0x45,	// Read Control Value Table entry
	RDTG		= 0x7d,	// Round Down To Grid
	ROFF		= 0x7a,	// Round OFF
	ROLL		= 0x8a,	//ROLL the top three stack elements
	ROUND		= 0x68,	//-0x6b[ab] ROUND value
	RS			= 0x43,	// Read Store
	RTDG		= 0x3d,	// Round To Double Grid
	RTG			= 0x18,	// Round To Grid
	RTHG		= 0x19,	// Round To Half Grid
	RUTG		= 0x7c,	// Round Up To Grid
	S45ROUND	= 0x77,	// Super ROUND 45 degrees
	SANGW		= 0x7e,	// Set Angle Weight
	SCANCTRL	= 0x85,	// SCAN conversion ConTRoL
	SCANTYPE	= 0x8d,	// SCANTYPE
	SCFS		= 0x48,	// Sets Coordinate From the Stack using projection vector and freedom vector
	SCVTCI		= 0x1d,	// Set Control Value Table Cut-In
	SDB			= 0x5e,	// Set Delta Base in the graphics state
	SDPVTL		= 0x86,	//-0x87[a] Set Dual Projection Vector To Line
	SDS			= 0x5f,	// Set Delta Shift in the graphics state
	SFVFS		= 0x0b,	// Set Freedom Vector From Stack
	SFVTCA		= 0x04,	//-0x05[a] Set Freedom Vector To Coordinate Axis
	SFVTL		= 0x08,	//-0x09[a] Set Freedom Vector To Line
	SFVTP		= 0x0e,	// Set Freedom Vector To Projection Vector
	SHC			= 0x34,	//-0x35[a] SHift Contour using reference point
	SHP			= 0x32,	//-0x33[a] SHift Point using reference point
	SHPIX		= 0x38,	// SHift point by a PIXel amount
	SHZ			= 0x36,	//-0x37[a] SHift Zone using reference point
	SLOOP		= 0x17,	// Set LOOP variable
	SMD			= 0x1a,	// Set Minimum Distance
	SPVFS		= 0x0a,	// Set Projection Vector From Stack
	SPVTCA		= 0x02,	//-0x03[a] Set Projection Vector To Coordinate Axis
	SPVTL		= 0x06,	//-0x07[a] Set Projection Vector To Line
	SROUND		= 0x76,	// Super ROUND
	SRP0		= 0x10,	// Set Reference Point 0
	SRP1		= 0x11,	// Set Reference Point 1
	SRP2		= 0x12,	// Set Reference Point 2
	SSW			= 0x1f,	// Set Single Width
	SSWCI		= 0x1e,	// Set Single Width Cut-In
	SUB			= 0x61,	// SUBtract
	SVTCA		= 0x00,	//-0x01[a] Set freedom and projection Vectors To Coordinate Axis
	SWAP		= 0x23,	// SWAP the top two elements on the stack
	SZP0		= 0x13,	// Set Zone Pointer 0
	SZP1		= 0x14,	// Set Zone Pointer 1
	SZP2		= 0x15,	// Set Zone Pointer 2
	SZPS		= 0x16,	// Set Zone PointerS
	UTP			= 0x29,	// UnTouch Point
	WCVTF		= 0x70,	// Write Control Value Table in Funits
	WCVTP		= 0x44,	// Write Control Value Table in Pixel units
	WS			= 0x42,	// Write Store
};

enum PLATFORM {
	PLAT_unicode	= 0,
	PLAT_macintosh	= 1,
	PLAT_microsoft	= 3,
};

//-----------------------------------------------------------------------------
//	OS/2
//-----------------------------------------------------------------------------

struct PANOSE {
	uint8	bFamilyType;
	uint8	bSerifStyle;
	uint8	bWeight;
	uint8	bProportion;
	uint8	bContrast;
	uint8	bStrokeVariation;
	uint8	bArmStyle;
	uint8	bLetterform;
	uint8	bMidline;
	uint8	bXHeight;
};

struct OS2 {
	enum {tag = 'OS/2'};
	uint16	version;			//0x0004
	int16	avg_char_width;
	uint16	weight_class;
	uint16	width_class;
	uint16	type;
	int16	subscript_size_x;
	int16	subscript_size_y;
	int16	subscript_offset_x;
	int16	subscript_offset_y;
	int16	superscript_size_x;
	int16	superscript_size_y;
	int16	superscript_offset_x;
	int16	superscript_offset_y;
	int16	strikeout_size;
	int16	strikeout_position;
	int16	family_class;
	PANOSE	panose;
	uint32	unicode_range[4];
	char	vend_id[4];
	uint16	selection;
	uint16	first_char_index;
	uint16	last_char_index;
	int16	typo_ascender;
	int16	typo_descender;
	int16	typo_line_gap;
	uint16	win_ascent;
	uint16	win_descent;
	uint32	codepage_range[2];
	int16	height;
	int16	cap_height;
	uint16	default_char;
	uint16	break_char;
	uint16	max_context;
};

//-----------------------------------------------------------------------------
//	head
//-----------------------------------------------------------------------------

struct head {
	enum {tag = 'head', MAGIC = 0x5F0F3CF5};
/*	enum FLAGS {
bit 0 - y value of 0 specifies baseline
bit 1 - x position of left most black bit is LSB
bit 2 - scaled point size and actual point size will differ (i.e. 24 point glyph differs from 12 point glyph scaled by factor of 2)
bit 3 - use integer scaling instead of fractional
bit 4 - (used by the Microsoft implementation of the TrueType scaler)
bit 5 - This bit should be set in fonts that are intended to e laid out vertically, and in which the glyphs have been drawn such that an x-coordinate of 0 corresponds to the desired vertical baseline.
bit 6 - This bit must be set to zero.
bit 7 - This bit should be set if the font requires layout for correct linguistic rendering (e.g. Arabic fonts).
bit 8 - This bit should be set for a GX font which has one or more metamorphosis effects designated as happening by default.
bit 9 - This bit should be set if the font contains any strong right-to-left glyphs.
bit 10 - This bit should be set if the font contains Indic-style rearrangement effects.
bits 11-12 - Defined by Adobe.
	};*/
	enum STYLE {
		bold		= 1 << 0,
		italic		= 1 << 1,
		underline	= 1 << 2,
		outline		= 1 << 3,
		shadow		= 1 << 4,
		condensed	= 1 << 5,
		extended	= 1 << 6,
	};
	enum DIRECTION {
		DIR_LEFTRIGHT_STRONG	= 1,	// Only strongly left to right glyphs
		DIR_LEFTRIGHT			= 2,	// Like 1 but also contains neutrals
		DIR_RIGHTLEFT_STRONG	= -1,	// Only strongly right to left glyphs
		DIR_RIGHTLEFT			= -2,	// Like -1 but also contains neutrals
	};

	fixed32		version;			//0x00010000 if (version 1.0)
	fixed32		font_revision;		//set by font manufacturer
	uint32		checksum_adjustment;//To compute: set it to 0, calculate the checksum for the 'head' table and put it in the table directory, sum the entire font as uint32, then store B1B0AFBA - sum. The checksum for the 'head' table will not be wrong. That is OK.
	uint32		magic;				//set to 0x5F0F3CF5
	uint16		flags;
	uint16		units_per_em;		//range from 64 to 16384
	iso::packed<datetime64>	created;			//international date
	iso::packed<datetime64>	modified;			//international date
	fword		xMin;				//for all glyph bounding boxes
	fword		yMin;				//for all glyph bounding boxes
	fword		xMax;				//for all glyph bounding boxes
	fword		yMax;				//for all glyph bounding boxes
	uint16		macStyle;
	uint16		lowestRecPPEM;		//smallest readable size in pixels
	int16		fontDirectionHint;	//0 Mixed directional glyphs
	int16		indexToLocFormat;	//0 for short offsets, 1 for long
	int16		glyphDataFormat;	//0 for current format
};

//-----------------------------------------------------------------------------
//	hhea
//-----------------------------------------------------------------------------

struct hhea {
	enum {tag = 'hhea'};
	fixed32		version;				//0x00010000 (1.0)
	fword		ascent;					//Distance from baseline of highest ascender
	fword		descent;				//Distance from baseline of lowest descender
	fword		lineGap;				//typographic line gap
	ufword		advanceWidthMax;		//must be consistent with horizontal metrics
	fword		minLeftSideBearing;		//must be consistent with horizontal metrics
	fword		minRightSideBearing;	//must be consistent with horizontal metrics
	fword		xMaxExtent;				//max(lsb + (xMax-xMin))
	int16		caretSlopeRise;			//used to calculate the slope of the caret (rise/run) set to 1 for vertical caret
	int16		caretSlopeRun;			//0 for vertical
	fword		caretOffset;			//set value to 0 for non-slanted fonts
	int16		reserved[4];			//set value to 0
	int16		metricDataFormat;		//0 for current format
	uint16		numOfLongHorMetrics;	//number of advance widths in metrics table
};

//-----------------------------------------------------------------------------
//	hmtx
//-----------------------------------------------------------------------------

struct hmtx {
	enum {tag = 'htmx'};
	struct metric {
		ufword	advanceWidth;
		fword	leftSideBearing;
	};
	//metric	metrics[];		//# from hhead
	//fword	leftSideBearing[];	//
};

//-----------------------------------------------------------------------------
//	maxp
//-----------------------------------------------------------------------------

struct maxp {
	enum {tag = 'maxp'};
	fixed32		version;				//0x00010000 (1.0)
	uint16		numGlyphs;				//the number of glyphs in the font
	uint16		maxPoints;				//points in non-compound glyph
	uint16		maxContours;			//contours in non-compound glyph
	uint16		maxComponentPoints;		//points in compound glyph
	uint16		maxComponentContours;	//contours in compound glyph
	uint16		maxZones;				//set to 2
	uint16		maxTwilightPoints;		//points used in Twilight Zone (Z0)
	uint16		maxStorage;				//number of Storage Area locations
	uint16		maxFunctionDefs;		//number of FDEFs
	uint16		maxInstructionDefs;		//number of IDEFs
	uint16		maxStackElements;		//maximum stack depth
	uint16		maxSizeOfInstructions;	//byte count for glyph instructions
	uint16		maxComponentElements;	//number of glyphs referenced at top level
	uint16		maxComponentDepth;		//levels of recursion, set to 0 if font has only simple glyphs
};

//-----------------------------------------------------------------------------
//	name
//-----------------------------------------------------------------------------

struct name {
	enum {tag = 'name'};
	enum ENC_MAC {
		MAC_Roman			= 0,
		MAC_Japanese		= 1,
		MAC_TradChinese		= 2,
		MAC_Korean			= 3,
		MAC_Arabic			= 4,
		MAC_Hebrew			= 5,
		MAC_Greek			= 6,
		MAC_Russian			= 7,
		MAC_RSymbol			= 8,
		MAC_Devanagari		= 9,
		MAC_Gurmukhi		= 10,
		MAC_Gujarati		= 11,
		MAC_Oriya			= 12,
		MAC_Bengali			= 13,
		MAC_Tamil			= 14,
		MAC_Telugu			= 15,
		MAC_Kannada			= 16,
		MAC_Malayalam		= 17,
		MAC_Sinhalese		= 18,
		MAC_Burmese			= 19,
		MAC_Khmer			= 20,
		MAC_Thai			= 21,
		MAC_Laotian			= 22,
		MAC_Georgian		= 23,
		MAC_Armenian		= 24,
		MAC_SimpChinese		= 25,
		MAC_Tibetan			= 26,
		MAC_Mongolian		= 27,
		MAC_Geez			= 28,
		MAC_Slavic			= 29,
		MAC_Vietnamese		= 30,
		MAC_Sindhi			= 31,
		MAC_Uninterpreted	= 32,
	};

	enum ENC_UNICODE {
		UNI_default			= 0,
		UNI_1_1				= 1,
		UNI_iso_10646		= 2,
		UNI_2_bmp			= 3,
		UNI_2_full			= 4,
		UNI_variation		= 5,
		UNI_full			= 6,
	};

	enum ENC_MICROSOFT {
		MS_Symbol			= 0,
		MS_UCS2				= 1,
		MS_ShiftJIS			= 2,
		MS_PRC				= 3,
		MS_Big5				= 4,
		MS_Wansung			= 5,
		MS_Johab			= 6,
		MS_Reserved7		= 7,
		MS_Reserved8		= 8,
		MS_Reserved9		= 9,
		MS_UCS4				= 10,
	};

	enum ID {
		ID_COPYRIGHT		= 0,	//Copyright notice.
		ID_FAMILY			= 1,	//Font Family. This string is the font family name the user sees on Macintosh platforms.
		ID_SUBFAMILY		= 2,	//Font Subfamily. This string is the font family the user sees on Macintosh platforms.
		ID_IDENTIFICATION	= 3,	//Unique subfamily identification.
		ID_FULLNAME			= 4,	//Full name of the font.
		ID_VERSION			= 5,	//Version of the name table.
		ID_PS_NAME			= 6,	//PostScript name of the font. Note: A font may have only one PostScript name and that name must be ASCII.
		ID_TRADEMARK		= 7,	//Trademark notice.
		ID_MANUFACTURER		= 8,	//Manufacturer name.
		ID_DESIGNER			= 9,	//Designer; name of the designer of the typeface.
		ID_DESCRIPTION		= 10,	//Description; description of the typeface. Can contain revision information, usage recommendations, history, features, and so on.
		ID_VENDOR_URL		= 11,	//URL of the font vendor (with procotol, e.g., http://, ftp://). If a unique serial number is embedded in the URL, it can be used to register the font.
		ID_DESIGNER_URL		= 12,	//URL of the font designer (with protocol, e.g., http://, ftp://)
		ID_LICENSE			= 13,	//License description; description of how the font may be legally used, or different example scenarios for licensed use. This field should be written in plain language, not legalese.
		ID_LICENSE_URL		= 14,	//License information URL, where additional licensing information can be found.
		ID_RESERVED			= 15,	//Reserved
		ID_PREF_FAMILY		= 16,	//Preferred Family (Windows only); In Windows, the Family name is displayed in the font menu; the Subfamily name is presented as the Style name. For historical reasons, font families have contained a maximum of four styles, but font designers may group more than four fonts to a single family. The Preferred Family and Preferred Subfamily IDs allow font designers to include the preferred family/subfamily groupings. These IDs are only present if they are different from IDs 1 and 2.
		ID_PREF_SUBFAMILY	= 17,	//Preferred Subfamily (Windows only); In Windows, the Family name is displayed in the font menu; the Subfamily name is presented as the Style name. For historical reasons, font families have contained a maximum of four styles, but font designers may group more than four fonts to a single family. The Preferred Family and Preferred Subfamily IDs allow font designers to include the preferred family/subfamily groupings. These IDs are only present if they are different from IDs 1 and 2.
		ID_COMPATIBLE		= 18,	//Compatible Full (Macintosh only); On the Macintosh, the menu name is constructed using the FOND resource. This usually matches the Full Name. If you want the name of the font to appear differently than the Full Name, you can insert the Compatible Full Name in ID 18. This name is not used by the Mac OS itself, but may be used by application developers (e.g., Adobe).
		ID_SAMPLE			= 19,	//Sample text. This can be the font name, or any other text that the designer thinks is the best sample text to show what the font looks like.
		//20 - 255	Reserved for future expansion.
		ID_FONT_SPECIFIC	= 256,	// 256 - 32767	Font-specific names (layout features and settings, variations, track names, etc.)
	};

	struct NameRecord {
		uint16	platformID;			//Platform identifier code.
		uint16	encodingID;			//Platform-specific encoding identifier.
		uint16	languageID;			//Language identifier.
		uint16	nameID;				//Name identifiers.
		uint16	length;				//Name string length in bytes.
		uint16	offset;				//Name string offset in bytes from stringOffset.
	};

	uint16		format;			//Format selector. Set to 0.
	uint16		count;			//The number of nameRecords in this name table.
	uint16		stringOffset;	//Offset in bytes to the beginning of the name character strings.
	NameRecord	names[];		//The name records array.
};

//-----------------------------------------------------------------------------
//	cmap
//-----------------------------------------------------------------------------

struct cmap {
	enum {tag = 'cmap'};
	struct table {
		uint16	platform;
		uint16	encoding;
		uint32	offset;
	};

	struct format_header1 {
		uint16	format;		//Set to 0,2,4,6
		uint16	length;		//Length in bytes of the subtable (set to 262 for format 0)
		uint16	language;	//Language code for this encoding subtable, or zero if language-independent
	};
	struct format_header2 {
		fixed32	format;		//Subtable format; set to 8.0,10.0,12.0
		uint32	length;		//Byte length of this subtable (including the header)
		uint32	language;	//Language code for this encoding subtable, or zero if language-independent
	};

	struct format0 : format_header1 {
		uint8	glyphIndexArray[256];	//An array that maps character codes to glyph index values
	};
	struct format2 : format_header1 {
		uint16	subHeaderKeys[256];		//Array that maps high bytes to subHeaders: value is index * 8
		struct subheader {
			uint16	firstCode;
			uint16	entryCount;
			int16	idDelta;
			uint16	idRangeOffset;
		} subHeaders[];	//Variable length array of subHeader structures
//		uint16	glyphIndexArray[];	//Variable length array containing subarrays
	};

	struct format4 : format_header1 {
		uint16	segCountX2;			//2 * segCount
		uint16	searchRange;		//2 * (2**FLOOR(log2(segCount)))
		uint16	entrySelector;		//log2(searchRange/2)
		uint16	rangeShift;			//(2 * segCount) - searchRange
		uint16	endCode[];			//Ending character code for each segment, last = 0xFFFF.
/*
		uint16	endCode[segCount];			//Ending character code for each segment, last = 0xFFFF.
		uint16	reservedPad;				//This value should be zero
		uint16	startCode[segCount];		//Starting character code for each segment
		uint16	idDelta[segCount];			//Delta for all character codes in segment
		uint16	idRangeOffset[segCount];	//Offset in bytes to glyph indexArray, or 0
		uint16	glyphIndexArray[variable];	//Glyph index array
*/
	};

	struct format6 : format_header1 {
		uint16	firstCode;			//First character code of subrange
		uint16	entryCount;			//Number of character codes in subrange
		uint16	glyphIndexArray[];	//Array of glyph index values for character codes in the range
	};

	struct format8 : format_header2 {
		uint8	is32[65536];		//Tightly packed array of bits (8K bytes total) indicating whether the particular 16-bit (index) value is the start of a 32-bit character code
		uint32	nGroups;			//Number of groupings which follow
	};

	struct format10 : format_header2 {
		uint32	startCharCode;		//First character code covered
		uint32	numChars;			//Number of character codes covered
		uint16	glyphs[];			//Array of glyph indices for the character codes covered
	};

	struct format12 : format_header2 {
		uint32	nGroups;			//Number of groupings which follow
	};

	uint16	ver;	//0
	uint16	num;
	table	tables[0];
};

//-----------------------------------------------------------------------------
//	gasp
//-----------------------------------------------------------------------------

struct gasp {
	enum {tag = 'gasp'};
	enum {
		grid_fit	= 1,	//Use gridfitting
		do_gray		= 2,	//Use grayscale rendering
	};
	struct range {
		uint16	mac_ppem;	//Upper limit of range, in PPEM
		uint16	behavior;	//Flags describing desired rasterizer behavior.
	};
	uint16		version;	//Version number (set to 0)
	uint16		num;		//Number of records to follow
	range		entries[];	//Sorted by ppem
};

//-----------------------------------------------------------------------------
//	glyf
//-----------------------------------------------------------------------------

struct glyf {
	enum {tag = 'glyf'};
	struct glyph {
		int16	num_contours;	//positive or zero => single; -1 => compound
		fword	xmin;			//Minimum x for coordinate data
		fword	ymin;			//Minimum y for coordinate data
		fword	xmax;			//Maximum x for coordinate data
		fword	ymax;			//Maximum y for coordinate data
	};

	struct simple_glyph : glyph {
		enum {
			on_curve	= 1 << 0,	//If set, the point is on the curve;Otherwise, it is off the curve.
			short_x		= 1 << 1,	//the corresponding x-coordinate is 1 byte
			short_y		= 1 << 2,	//the corresponding y-coordinate is 1 byte
			repeat		= 1 << 3,	//If set, the next byte specifies the number of additional times this set of flags is to be repeated
			same_x		= 1 << 4,	//If short_x, this is a sign bit (clear is -ve); else if set the current x is the same as the previous x
			same_y		= 1 << 5,	//If short_y, this is a sign bit (clear is -ve); else if set the current y is the same as the previous y
		};
		uint16	end_pts[];		//Array of last points of each contour; n is the number of contours; array entries are point indices
	//	uint16	ins_length;		//Total number of bytes needed for instructions
	//	uint8	instructions[];	//Array of instructions for this glyph
	//	uint8	flags[];		//Array of flags
	//	uint8 or int16	x[];	//Array of x-coordinates; the first is relative to (0,0), others are relative to previous point
	//	uint8 or int16	y[];	//Array of y-coordinates; the first is relative to (0,0), others are relative to previous point

		iso::uint16				num_pts()		const { return end_pts[num_contours - 1] + 1; }
		iso::const_memory_block	instructions()	const { return {end_pts + num_contours + 1, end_pts[num_contours]}; }
		const uint8*			contours()		const { return instructions().end(); }

	};
	struct compound_glyph : glyph {
		enum {
			arg12_words			= 1 << 0,	//the arguments are words; else they are bytes.
			args_xy				= 1 << 1,	//the arguments are xy values; else they are points.
			round_to_grid		= 1 << 2,	//round the xy values to grid
			have_scale			= 1 << 3,	//there is a simple scale for the component
			more_components		= 1 << 5,	//at least one additional glyph follows this one.
			x_and_y_scale		= 1 << 6,	//the x direction will use a different scale than the y direction.
			two_by_two			= 1 << 7,	//there is a 2-by-2 transformation
			have_instructions	= 1 << 8,	//instructions for the component character follow the last component.
			use_my_metrics		= 1 << 9,	//Use metrics from this component for the compound glyph.
			overlap_compound	= 1 << 10,	//the components of this compound glyph overlap.
		};
		struct entry {
			uint16	flags;		//Component flag
			uint16	index;		//Glyph index of component
			//int16, uint16, int8 or uint8	argument1	X-offset for component or point number;
			//int16, uint16, int8 or uint8	argument2	Y-offset for component or point number
			//transformation option	One of scale, {xscale, yscale}, {xscale	scale01	scale10	yscale}
		};
		entry	entries[];
	};
};

//-----------------------------------------------------------------------------
//	EOT
//-----------------------------------------------------------------------------

struct EOTHeader : iso::littleendian_types {
	enum {MAGIC = 0x504c};

	uint32		eot_size;
	uint32		font_size;
	uint32		version;
	uint32		flags;
	PANOSE		panose;
	uint8		charset;
	uint8		italic;
	uint32		weight;
	uint16		type;
	uint16		magic;
	uint32		unicode_range[4];
	uint32		codepage_range[2];
	uint32		checksum_adjustment;
	uint32		reserved[4];
	uint16		padding1;
	/*
	//all padding values must be set to 0x0000
	struct name {
		uint16	size;
		char	bytes[1];
	};

	name	family_name;		//Family string found in the name table of the font (name ID = 1)
	uint16	padding2;
	name	style_name;			//Subfamily string found in the name table of the font (name ID = 2)
	uint16	padding3;
	name	version_name;		//Version string found in the name table of the font (name ID = 5)
	uint16	padding4;
	name	full_name;			//Full name string found in the name table of the font (name ID = 4)

	//Version 0x00020001
	uint16	padding5;
	name	root_string;

	//Version 0x00020002
	uint32	root_string_checksum;
	uint32	EUDC_codepage;
	uint16	padding6;
	name	signature;
	uint32	EUDCFlags;			//processing flags for the EUDC font. Typical values might be TTEMBED_XORENCRYPTDATA and TTEMBED_TTCOMPRESSED.
	name	EUDCFontData;

	// all
	uint8	data[1];//font_size];	//compressed or XOR encrypted as indicated by the processing flags.
	*/
};

uint32 CalcTableChecksum(uint32 *table, iso::uint32 length);

}// namespace ttf
