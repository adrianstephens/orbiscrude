#include "base/vector.h"
#include "base/pointer.h"

namespace ttf {
using namespace iso;
typedef fixed<16,16>	_fixed32;
typedef fixed<2,14>		_fixed16;
typedef BE(_fixed32)	fixed32;	//32-bit signed fixed-point number (16.16)
typedef BE(_fixed16)	fixed16;	//32-bit signed fixed-point number (16.16)
typedef uint64be		datetime64;	//Date represented in number of seconds since 12:00 midnight, January 1, 1904. The value is represented as a signed 64-bit integer.
typedef uint32			TAG;		//Array of four uint8s (length = 32 bits) used to identify a script, language system, feature, or baseline
typedef	int16be			fword;
typedef	uint16be		ufword;

template<typename I, typename T> struct table {
	I		n;
	T		t;
	size_t	size()		const	{ return n; }
	uint32	size32()	const	{ return n; }
	auto	all()		const	{ return make_range_n(&t, n); }
	auto	begin()		const	{ return &t; }
	auto	end()		const	{ return &t + n; }
	auto&	front()		const	{ return t; }
	auto&	back()		const	{ return end()[-1]; }
	auto&	operator[](intptr_t i)	const	{ return (&t)[i]; }
	auto	index_of(const T &v)	const	{ return &v - &t; }
};

struct CommonTable {
	uint16be	format;
};

struct SFNTHeader {
	fixed32		version;		// 0x00010000 for version 1.0 (or 'true' or 'typ1'); 'OTTO' for opentype
	uint16be	num_tables;		// Number of tables.
	uint16be	search_range;	// (Maximum power of 2 <= numTables) x 16.
	uint16be	entry_selector;	// Log2(maximum power of 2 <= numTables).
	uint16be	range_shift;	// NumTables x 16-searchRange.
};
struct TableRecord {
	TAG			tag;			// 4 -byte identifier.
	uint32be	checksum;		// CheckSum for this table.
	uint32be	offset;			// Offset from beginning of TrueType font file.
	uint32be	length;			// Length of this table.
};
struct TTCHeader {
	TAG			tag;			// TrueType Collection ID string: 'ttcf'
	fixed32		version;		// Version of the TTC Header (1.0), 0x00010000 or (2.0), 0x00020000
	uint32be	num_fonts;
};

struct TTCHeader2 {
	TAG			tag;			// Tag indicating that a DSIG table exists, 0x44534947 ('DSIG') (null if no signature)
	uint32be	length;			// The length (in bytes) of the DSIG table (null if no signature)
	uint32be	offset;			// The offset (in bytes) of the DSIG table from the beginning of the TTC file (null if no signature)
};
/*
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
	typedef float2p		vec;
	typedef uint16		val;
	typedef fixed<26,6>	f26_6;
	typedef uint32		stack_element;

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
*/
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
//	common
//-----------------------------------------------------------------------------

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
	enum {tag = "OS/2"_u32};
	uint16be	version;			//0x0004
	int16be		avg_char_width;
	uint16be	weight_class;
	uint16be	width_class;
	uint16be	type;
	int16be		subscript_size_x;
	int16be		subscript_size_y;
	int16be		subscript_offset_x;
	int16be		subscript_offset_y;
	int16be		superscript_size_x;
	int16be		superscript_size_y;
	int16be		superscript_offset_x;
	int16be		superscript_offset_y;
	int16be		strikeout_size;
	int16be		strikeout_position;
	int16be		family_class;
	PANOSE		panose;
	uint32be	unicode_range[4];
	char		vend_id[4];
	uint16be	selection;
	uint16be	first_char_index;
	uint16be	last_char_index;
	int16be		typo_ascender;
	int16be		typo_descender;
	int16be		typo_line_gap;
	uint16be	win_ascent;
	uint16be	win_descent;
	uint32be	codepage_range[2];
	int16be		height;
	int16be		cap_height;
	uint16be	default_char;
	uint16be	break_char;
	uint16be	max_context;
};

//-----------------------------------------------------------------------------
//	head
//-----------------------------------------------------------------------------

struct head {
	enum {tag = "head"_u32, MAGIC = 0x5F0F3CF5};
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

	fixed32		version;				//0x00010000 if (version 1.0)
	fixed32		font_revision;			//set by font manufacturer
	uint32be	checksum_adjustment;	//To compute: set it to 0, calculate the checksum for the 'head' table and put it in the table directory, sum the entire font as uint32be, then store B1B0AFBA - sum. The checksum for the 'head' table will not be wrong. That is OK.
	uint32be	magic;					//set to 0x5F0F3CF5
	uint16be	flags;
	uint16be	units_per_em;			//range from 64 to 16384
	packed<datetime64>	created;		//international date
	packed<datetime64>	modified;		//international date
	fword		xMin, yMin, xMax, yMax;	//for all glyph bounding boxes
	uint16be	macStyle;
	uint16be	lowestRecPPEM;			//smallest readable size in pixels
	int16be		fontDirectionHint;		//0 Mixed directional glyphs
	int16be		indexToLocFormat;		//0 for short offsets, 1 for long
	int16be		glyphDataFormat;		//0 for current format
};

//-----------------------------------------------------------------------------
//	hhea	Horizontal Header Table
//-----------------------------------------------------------------------------

struct hhea {
	enum {tag = "hhea"_u32};
	fixed32		version;				//0x00010000 (1.0)
	fword		ascent;					//Distance from baseline of highest ascender
	fword		descent;				//Distance from baseline of lowest descender
	fword		lineGap;				//typographic line gap
	ufword		advanceWidthMax;		//must be consistent with horizontal metrics
	fword		minLeftSideBearing;		//must be consistent with horizontal metrics
	fword		minRightSideBearing;	//must be consistent with horizontal metrics
	fword		xMaxExtent;				//max(lsb + (xMax-xMin))
	int16be		caretSlopeRise;			//used to calculate the slope of the caret (rise/run) set to 1 for vertical caret
	int16be		caretSlopeRun;			//0 for vertical
	fword		caretOffset;			//set value to 0 for non-slanted fonts
	int16be		reserved[4];			//set value to 0
	int16be		metricDataFormat;		//0 for current format
	uint16be	numOfLongHorMetrics;	//number of advance widths in metrics table
};

//-----------------------------------------------------------------------------
//	hmtx	Horizontal Metrics Table
//-----------------------------------------------------------------------------

struct hmtx {
	enum {tag = "htmx"_u32};
	struct metric {
		ufword	advanceWidth;
		fword	leftSideBearing;
	};
	metric	metrics[];		//# from hhead

	auto	advance(const hhea* h, uint32 glyph) {
		return metrics[min(glyph, h->numOfLongHorMetrics)].advanceWidth;
	}
	auto	left(const hhea* h, uint32 glyph) {
		return glyph < h->numOfLongHorMetrics
			? metrics[glyph].leftSideBearing
			: ((fword*)&metrics[h->numOfLongHorMetrics])[glyph - h->numOfLongHorMetrics];
	}

//	range<const metric*>	metrics(const hhea *h)			const { return make_range_n((const metric*)this, h->numOfLongHorMetrics); }
//	fword*				leftSideBearing(const hhea *h)	const { return (fword*)metrics(h).end(); }
	//fword	leftSideBearing[];	//
};

//-----------------------------------------------------------------------------
//	maxp	Maximum Profile
//-----------------------------------------------------------------------------

struct maxp {
	enum {tag = "maxp"_u32};
	fixed32		version;				//0x00010000 (1.0)
	uint16be	numGlyphs;				//the number of glyphs in the font
	uint16be	maxPoints;				//points in non-compound glyph
	uint16be	maxContours;			//contours in non-compound glyph
	uint16be	maxComponentPoints;		//points in compound glyph
	uint16be	maxComponentContours;	//contours in compound glyph
	uint16be	maxZones;				//set to 2
	uint16be	maxTwilightPoints;		//points used in Twilight Zone (Z0)
	uint16be	maxStorage;				//number of Storage Area locations
	uint16be	maxFunctionDefs;		//number of FDEFs
	uint16be	maxInstructionDefs;		//number of IDEFs
	uint16be	maxStackElements;		//maximum stack depth
	uint16be	maxSizeOfInstructions;	//byte count for glyph instructions
	uint16be	maxComponentElements;	//number of glyphs referenced at top level
	uint16be	maxComponentDepth;		//levels of recursion, set to 0 if font has only simple glyphs
};

//-----------------------------------------------------------------------------
//	name	Naming Table
//-----------------------------------------------------------------------------

struct name {
	enum {tag = "name"_u32};
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
		uint16be	platformID;			//Platform identifier code.
		uint16be	encodingID;			//Platform-specific encoding identifier.
		uint16be	languageID;			//Language identifier.
		uint16be	nameID;				//Name identifiers.
		uint16be	length;				//Name string length in bytes.
		uint16be	offset;				//Name string offset in bytes from stringOffset.
	};

	uint16be	format;			//Format selector. Set to 0.
	uint16be	count;			//The number of nameRecords in this name table.
	uint16be	stringOffset;	//Offset in bytes to the beginning of the name character strings.
	NameRecord	names[];		//The name records array.
};

//-----------------------------------------------------------------------------
//	cmap	Character to Glyph Index Mapping Table
//-----------------------------------------------------------------------------

struct cmap {
	enum {tag = "cmap"_u32};
	enum PLATFORM {
		UNICODE			= 0,	//Various
		MACINTOSH		= 1,	//Script manager code
		ISO				= 2,	//ISO encoding [deprecated]
		WINDOWS			= 3,	//Windows encoding
		CUSTOM			= 4,	//Custom
	};
	enum UNICODE_ENCODING {
		UNICODE_1_0		= 0,	//deprecated
		UNICODE_1_1		= 1,	//deprecated
		ISO_IEC_10646	= 2,	//deprecated
		UNICODE_BMP		= 3,	//Unicode BMP only
		UNICODE_FULL	= 4,	//Unicode full repertoire
		UNICODE_UVS		= 5,	//for use with subtable format 14
		UNICODE_FULL13	= 6,	//for use with subtable format 13
	};
	enum ISO_ENCODING {
		ISO_ASCII7		= 0,
		ISO_10646		= 1,
		ISO_8859_1		= 2,
	};
	enum WINDOWS_ENCODING {
		WIN_SYMBOL		= 0,
		WIN_UNICODE_BMP	= 1,
		WIN_SHIFTJIS	= 2,
		WIN_PRC			= 3,
		WIN_BIG5		= 4,
		WIN_WANSUNG		= 5,
		WIN_JOHAB		= 6,
		WIN_UNICODE_FULL= 10,
	};
	enum FORMAT {
		BYTE_ENCODING	= 0,
		HIGHBYTE_TABLE	= 2,
		SEGMENT_DELTA	= 4,
		TRIMMED_MAPPING	= 6,
		MIXED_16_32		= 8,
		TRIMMED_ARRAY	= 10,
		SEGMENTED		= 12,
		MANY_TO_ONE		= 13,
		VARIATION_SEQ	= 14,
	};
	struct group {
		uint32be	start, end, glyph;
	};

	struct format_header1 : CommonTable {
		uint16be	length;		//Length in bytes of the subtable (set to 262 for format 0)
		uint16be	language;	//Language code for this encoding subtable, or zero if language-independent
		template<typename M> uint32 get(M& map);
	};
	struct format_header2 {
		fixed32		format;		//Subtable format; set to 8.0,10.0,12.0
		uint32be	length;		//Byte length of this subtable (including the header)
		uint32be	language;	//Language code for this encoding subtable, or zero if language-independent
	};

	struct format0 : format_header1 {
		uint8	glyphs[256];	//An array that maps character codes to glyph index values
		template<typename M> uint32 get(M& map) {
			for (int i = 0; i < 256; i++)
				map[i] = glyphs[i];
			return 255;
		}
	};
	struct format2 : format_header1 {
		uint16be	subHeaderKeys[256];		//Array that maps high bytes to subHeaders: value is index * 8
		struct subheader {
			uint16be	start;
			uint16be	count;
			int16be		delta;
			offset_pointer<uint16be,uint16be>	range;
		} subHeaders[];
		template<typename M> uint32 get(M& map) {
			for (int i = 0; i < 256; i++) {
				auto		&sub	= subHeaders[subHeaderKeys[i] / 8];
				uint32		coffset	= i * 256 + sub.start;
				uint16be	*glyphs	= sub.range.get(&sub.range);
				for (int c = 0; c < sub.count; c++)
					map[c + coffset] = uint16(glyphs[c] + sub.delta);
			}
			return 65535;//?
		}
	};

	struct format4 : format_header1 {
		uint16be	segCountX2;			//2 * segCount
		uint16be	searchRange;		//2 * (2**FLOOR(log2(segCount)))
		uint16be	entrySelector;		//log2(searchRange/2)
		uint16be	rangeShift;			//(2 * segCount) - searchRange
		uint16be	endCode[];			//Ending character code for each segment, last = 0xFFFF.
/*
		uint16be	endCode[segCount];			//Ending character code for each segment, last = 0xFFFF.
		uint16be	reservedPad;				//This value should be zero
		uint16be	startCode[segCount];		//Starting character code for each segment
		uint16be	idDelta[segCount];			//Delta for all character codes in segment
		uint16be	idRangeOffset[segCount];	//Offset in bytes to glyph indexArray, or 0
		uint16be	glyphIndexArray[variable];	//Glyph index array
*/
		struct segment {
			uint16			end, start, delta;
			const uint16be	*glyphs;
		};
		struct segments {
			uint32			count;
			const uint16be	*ends;
			segments(const format4 &t) : count(t.segCountX2 / 2), ends(t.endCode) {}
			segment operator[](int i) {
				auto	r	= ends + i + 1 + count * 3;
				return {
					ends[i],
					ends[i + 1 + count * 1],
					ends[i + 1 + count * 2],
					*r ? r + *r / 2 : 0
				};
			}
		};
		auto	get_segments() const { return make_indexed_container(segments(*this), int_range(segCountX2 / 2)); }

		template<typename M> uint32 get(M& map) {
			for (auto s : get_segments()) {
				if (s.glyphs) {
					for (int c = s.start; c <= s.end; c++)
						map[c] = s.glyphs[c - s.start];
				} else {
					for (int c = s.start; c <= s.end; c++)
						map[c] = uint16(c + s.delta);
				}
			}
			return get_segments().back().end;
		}
	};

	struct format6 : format_header1 {
		uint16be					start;	//First character code of subrange
		table<uint16be, uint16be>	glyphs;	//Array of glyph index values for character codes in the range
		template<typename M> uint32 get(M& map) {
			for (auto &i : glyphs)
				map[int(glyphs.index_of(i) + start)] = i;
			return start + glyphs.size32();
		}
	};

	struct format8 : format_header2 {
		uint8		is32[8192];			//Tightly packed array of bits (8K bytes total) indicating whether the particular 16-bit (index) value is the start of a 32-bit character code
		table<uint32be, group>	groups;
		bool		is32bit(uint16 c)	const	{ return is32[c / 8] & (1 << (~c & 7)); }
	};

	struct format10 : format_header2 {
		uint32be					start;	//First character code covered
		table<uint32be, uint16be>	glyphs;	//Array of glyph indices for the character codes covered
		template<typename M> uint32 get(M& map) {
			for (auto &i : glyphs)
				map[int(glyphs.index_of(i) + start)] = i;
			return start + glyphs.size();
		}
	};

	struct format12 : format_header2 {
		table<uint32be, group>	groups;
		template<typename M> uint32 get(M& map) {
			for (auto& g : groups) {
				uint32	offset = g.glyph - g.start;
				for (int c = g.start; c <= g.end; c++)
					map[c] = c + offset;
			}
			return groups.back().end;
		}
	};

	struct format13 : format_header2 {
		table<uint32be, group>	groups;
		template<typename M> uint32 get(M& map) {
			for (auto& g : groups) {
				for (int c = g.start; c <= g.end; c++)
					map[c] = g.glyph;
			}
			return groups.back().end;
		}
	};

	struct format14 {
		struct DefaultUVS {
			struct UnicodeRange {
				uint24be			startUnicodeValue;	//First value in this range
				uint8				additionalCount;	//Number of additional values in this range
			};
			table<packed<uint32be>, UnicodeRange>	ranges;
		};

		struct NonDefaultUVS {
			struct UVSMapping {
				uint24be			unicodeValue;	//Base Unicode value of the UVS
				packed<uint16be>	glyphID;		//Glyph ID of the UVS
			};
			table<packed<uint32be>, UVSMapping>		mappings;
		};

		struct Variation {
			uint24be		selector;
			offset_pointer<DefaultUVS, packed<uint32be>>	_default_uvs;
			offset_pointer<NonDefaultUVS, packed<uint32be>>	_non_default_uvs;
			auto	default_uvs(const format14 *t)		const { return _default_uvs ? _default_uvs.get(t)->ranges.all() : none; }
			auto	non_default_uvs(const format14 *t)	const { return _non_default_uvs ? _non_default_uvs.get(t)->mappings.all() : none; }
		};
		uint16be			format;
		packed<uint32be>	length;
		table<packed<uint32be>, Variation>	variations;
	};

	struct cmap_table {
		uint16be	platform;
		uint16be	encoding;
		offset_pointer<format_header1, packed<uint32be>>	data;
	};

	uint16be	ver;	//0
	table<uint16be, cmap_table>	tables;
};

template<typename M> uint32 cmap::format_header1::get(M& map) {
	switch (format) {
		case BYTE_ENCODING:		return ((format0*)this)->get(map);
		case HIGHBYTE_TABLE:	return ((format2*)this)->get(map);
		case SEGMENT_DELTA:		return ((format4*)this)->get(map);
		case TRIMMED_MAPPING:	return ((format6*)this)->get(map);
		case MIXED_16_32:		return 0;
		case TRIMMED_ARRAY:		return ((format10*)this)->get(map);
		case SEGMENTED:			return ((format12*)this)->get(map);
		case MANY_TO_ONE:		return ((format13*)this)->get(map);
		case VARIATION_SEQ:
		default:				return 0;
	}
}

//-----------------------------------------------------------------------------
//	gasp	Grid-fitting and Scan-conversion Procedure Table
//-----------------------------------------------------------------------------

struct gasp {
	enum {tag = "gasp"_u32};
	enum {
		grid_fit	= 1,	//Use gridfitting
		do_gray		= 2,	//Use grayscale rendering
	};
	struct range {
		uint16be	mac_ppem;	//Upper limit of range, in PPEM
		uint16be	behavior;	//Flags describing desired rasterizer behavior.
	};
	uint16be		version;	//Version number (set to 0)
	uint16be		num;		//Number of records to follow
	range		entries[];	//Sorted by ppem
};

//-----------------------------------------------------------------------------
//	glyf	Glyph Data
//-----------------------------------------------------------------------------

struct glyf {
	enum {tag = "glyf"_u32};
	struct glyph {
		int16be	num_contours;	//positive or zero => single; -1 => compound
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
		uint16be	end_pts[];		//Array of last points of each contour; n is the number of contours; array entries are point indices
	//	uint16be	ins_length;		//Total number of bytes needed for instructions
	//	uint8	instructions[];	//Array of instructions for this glyph
	//	uint8	flags[];		//Array of flags
	//	uint8 or int16be	x[];	//Array of x-coordinates; the first is relative to (0,0), others are relative to previous point
	//	uint8 or int16be	y[];	//Array of y-coordinates; the first is relative to (0,0), others are relative to previous point

		uint16				num_pts()		const { return end_pts[num_contours - 1] + 1; }
		const_memory_block	instructions()	const { return {end_pts + num_contours + 1, end_pts[num_contours]}; }
		const uint8*		contours()		const { return instructions().end(); }

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
			uint16be	flags;		//Component flag
			uint16be	index;		//Glyph index of component
			//int16, uint16be, int8 or uint8	argument1	X-offset for component or point number;
			//int16, uint16be, int8 or uint8	argument2	Y-offset for component or point number
			//transformation option	One of scale, {xscale, yscale}, {xscale	scale01	scale10	yscale}
		};
		entry	entries[];
	};
};

//-----------------------------------------------------------------------------
//	CPAL	Color Palette Table
//-----------------------------------------------------------------------------

struct CPAL {
	struct Color {
		uint8	b,g,r,a;	//sRGB
	};

	uint16be	version;
	uint16be	numPaletteEntries;		//	Number of palette entries in each palette.
	uint16be	numPalettes;			//	Number of palettes in the table.
	uint16be	numColorRecords;		//	Total number of color records, combined for all palettes.
	offset_pointer<Color, uint32be>	colors;
	uint16be	colorRecordIndices[];	//	Index of each palette’s first color record in the combined color record array.
};

struct CPAL1 {
	enum Type {
		USABLE_WITH_LIGHT_BACKGROUND	= 0x0001,	//palette is appropriate to use when displaying the font on a light background such as white.
		USABLE_WITH_DARK_BACKGROUND		= 0x0002,	//palette is appropriate to use when displaying the font on a dark background such as black.
		//Note that the USABLE_WITH_LIGHT_BACKGROUND and USABLE_WITH_DARK_BACKGROUND flags are not mutually exclusive: they may both be set.
	};
	offset_pointer<Type, uint32be>		types;
	offset_pointer<uint16be, uint32be>	paletteLabels;		//Array of 'name' table IDs (typically in the font-specific name ID range) that specify user interface strings associated with each palette. Use 0xFFFF if no name ID is provided for a palette.
	offset_pointer<uint16be, uint32be>	paletteEntryLabels;	//Array of 'name' table IDs (typically in the font-specific name ID range) that specify user interface strings associated with each palette entry, e.g. “Outline”, “Fill”. This set of palette entry labels applies to all palettes in the font. Use 0xFFFF if no name ID is provided for a palette entry.
};

//-----------------------------------------------------------------------------
//	COLR	Color Table
//-----------------------------------------------------------------------------

enum PAINT : uint8 {
	ColrLayers					= 1,
	Solid						= 2,	VarSolid,
	LinearGradient				= 4,	VarLinearGradient,
	RadialGradient				= 6,	VarRadialGradient,
	SweepGradient				= 8,	VarSweepGradient,
	Glyph						= 10,
	ColrGlyph					= 11,
	Transform					= 12,	VarTransform,
	Translate					= 14,	VarTranslate,
	Scale						= 16,	VarScale,
	ScaleAroundCenter			= 18,	VarScaleAroundCenter,
	ScaleUniform				= 20,	VarScaleUniform,
	ScaleUniformAroundCenter	= 22,	VarScaleUniformAroundCenter,
	Rotate						= 24,	VarRotate,
	RotateAroundCenter			= 26,	VarRotateAroundCenter,
	Skew						= 28,	VarSkew,
	SkewAroundCenter			= 30,	VarSkewAroundCenter,
	Composite					= 32,
};

//template<PAINT P> constexpr PAINT var = PAINT(P|1);
template<PAINT P> struct Paint;

template<typename T> struct Var : T {
	uint32be	varIndexBase;  // Base index into DeltaSetIndexMap.
};

struct PaintBase {
	PAINT		format;
	template<PAINT P>	auto	as() const {
		ISO_ASSERT(format == P);
		return static_cast<const Paint<P>*>(this);
	}
};

struct COLR0 {
	struct BaseGlyph {
		uint16be	glyphID;			// Glyph ID of the base glyph
		uint16be	firstLayerIndex;	// Index (base 0) into the layerRecords array
		uint16be	numLayers;			// Number of color layers associated with this glyph
	};
	struct Layer {
		uint16be	glyphID;			// Glyph ID of the glyph used for a given layer
		uint16be	paletteIndex;		// Index (base 0) for a palette entry in the CPAL table
	};

	uint16be	version;
	uint16be	numBaseGlyphRecords;
	offset_pointer<BaseGlyph,	packed<uint32be>>	_base_glyphs;
	offset_pointer<Layer,		packed<uint32be>>	_layers;
	uint16be	numLayerRecords;

	auto	base_glyphs()	const { return make_range_n(_base_glyphs.get(this), numBaseGlyphRecords);  }
	auto	layers()		const { return make_range_n(_layers.get(this), numLayerRecords);  }
};

struct COLR1 : COLR0 {
	struct BaseGlyphs {
		struct record {
			uint16be	glyphID;		// Glyph ID of the base glyph
			offset_pointer<PaintBase, packed<uint32be>>	paint;
		};
		uint32be	num_records;
		record		_records[];
		auto		all() const { return make_range_n(_records, num_records); }
	};
	struct Layers {
		uint32be	numLayers;
		offset_pointer<PaintBase, packed<uint32be>>	paints[];
		auto		all() const { return make_range_n(paints, numLayers); }
	};
	struct Clips {
		struct ClipBox {
			uint8		format;
			fword		xMin, yMin, xMax, yMax;
		};

		struct Clip {
			uint16be	startGlyphID;
			uint16be	endGlyphID;
			offset_pointer<ClipBox, uint24be>	clipBox;
		};
		uint8		format;			// 1
		uint32be	numClips;
		Clip		clips[];		// Sorted by startGlyphID
		auto		all() const { return make_range_n(clips, numClips); }

	};
	offset_pointer<BaseGlyphs,	packed<uint32be>>	_baseGlyphs;
	offset_pointer<Layers,		packed<uint32be>>	_layers;
	offset_pointer<Clips,		packed<uint32be>>	_clips;
	packed<uint32be>	varIndexMapOffset;			// Offset to DeltaSetIndexMap table (may be NULL)
	packed<uint32be>	itemVariationStoreOffset;	// Offset to ItemVariationStore> (may be NULL)

	auto	base_glyphs1()	const { return _baseGlyphs.get(this); }
	auto	layers1()		const { return _layers.get(this); }
	auto	clips()			const { return _clips.get(this); }
};

struct ColorLine {
	enum Extend : uint8 {
		EXTEND_PAD	   = 0,	 // Use nearest color stop
		EXTEND_REPEAT  = 1,	 // Repeat from farthest color stop
		EXTEND_REFLECT = 2,	 // Mirror color line from nearest end
	};
	struct ColorStop {
		packed<fixed16>		stopOffset;	   // Position on a color line
		packed<uint16be>	paletteIndex;
		packed<fixed16>		alpha;
	};
	Extend				extend;
	packed<uint16be>	numStops;
	ColorStop			_stops[];
	auto	stops() const { return make_range_n(_stops, numStops); }
};

template<> struct Paint<ColrLayers> : PaintBase {
	uint8				numLayers;			// Number of offsets to paint tables to read from LayerList
	packed<uint32be>	firstLayerIndex;	// Index (base 0) into the LayerList
};

template<> struct Paint<Solid> : PaintBase {
	packed<uint16be>	paletteIndex;
	packed<fixed16>		alpha;
};

template<> struct Paint<LinearGradient> : PaintBase {
	offset_pointer<ColorLine, uint24be>	colorLine;
	fword		x0, y0;			// Start point
	fword		x1, y1;			// End point
	fword		x2, y2;			// Rotation point
};

template<> struct Paint<RadialGradient> : PaintBase {
	offset_pointer<ColorLine, uint24be>	colorLine;
	fword		x0, y0;			// Start circle center
	ufword		radius0;		// Start circle radius
	fword		x1, y1;			// End circle center
	ufword		radius1;		// End circle radius.
};

template<> struct Paint<SweepGradient> : PaintBase {
	offset_pointer<ColorLine, uint24be>	colorLine;
	fword		centerX, centerY;
	fixed16		startAngle, endAngle;		// Start and End of the angular range of the gradient, in counter-clockwise radians
};

struct SubPaint : PaintBase {
	offset_pointer<PaintBase, uint24be>	paint;
};

template<> struct Paint<Glyph> : SubPaint {
	uint16be	glyphID;		// Glyph ID for the source outline
};

template<> struct Paint<ColrGlyph> : PaintBase {
	uint16be	glyphID;		// Glyph ID for a BaseGlyphList base glyph
};

struct Affine2x3 {
	fixed32		xx, yx, xy, yy, dx, dy;
	operator float2x3() const { return {float2{xx.get(), yx.get()}, float2{xy.get(), yy.get()}, float2{dx.get(), dy.get()}}; }
};

template<> struct Paint<Transform> : SubPaint {
	offset_pointer<Affine2x3, uint24be>			transform;
	float2x3	matrix()	const { return *transform.get(this); }
};

template<> struct Paint<VarTransform> : SubPaint {
	offset_pointer<Var<Affine2x3>, uint24be>	transform;
};

template<> struct Paint<Translate> : SubPaint {
	fword		dx, dy;
	auto		matrix()	const { return translate(dx.get(), dy.get()); }
};

template<> struct Paint<Scale> : SubPaint {
	fixed16		scaleX, scaleY;
	auto		matrix()	const { return scale(float2{scaleX.get(), scaleY.get()}); }
};

template<> struct Paint<ScaleUniform> : SubPaint {
	fixed16		scale;
	auto		matrix()	const { return iso::scale(float(scale.get())); }
};

template<> struct Paint<Rotate> : SubPaint {
	fixed16		angle;		   // Rotation angle, counter-clockwise radians
	auto		matrix()	const { return rotate2D((float)angle.get()); }
};

template<> struct Paint<Skew> : SubPaint {
	fixed16		xSkewAngle;   // Angle of skew in the direction of the x-axis, counter-clockwise radians
	fixed16		ySkewAngle;   // Angle of skew in the direction of the y-axis, counter-clockwise radians
	float2x2	matrix()	const { return {float2{1, ySkewAngle.get()}, float2{xSkewAngle.get(), 1}}; }
};

template<> struct Paint<Composite> : PaintBase {
	enum MODE : uint8 {
		// Porter-Duff modes
		CLEAR			= 0,	// No regions are enabled.
		SRC				= 1,	// Only the source will be present.
		DEST			= 2,	// Only the destination will be present
		SRC_OVER		= 3,	// Source is placed over the destination
		DEST_OVER		= 4,	// Source is placed over the destination
		SRC_IN			= 5,	// The source that overlaps the destination, replaces the destination
		DEST_IN			= 6,	// Destination which overlaps the source, replaces the source
		SRC_OUT			= 7,	// Source is placed, where it falls outside of the destination
		DEST_OUT		= 8,	// Destination is placed, where it falls outside of the source
		SRC_ATOP		= 9,	// Source which overlaps the destination, replaces the destination. Destination is placed elsewhere
		DEST_ATOP		= 10,	// Destination which overlaps the source replaces the source. Source is placed elsewhere
		XOR				= 11,	// The non-overlapping regions of source and destination are combined
		PLUS			= 12,	// Display the sum of the source image and destination image ('Lighter' in Composition & Blending Level 1)
		// Separable color blend modes:
		SCREEN			= 13,	// D + S - (D * S)
		OVERLAY			= 14,	// HardLight(S, D)
		DARKEN			= 15,	// min(D, S)
		LIGHTEN			= 16,	// max(D, S)
		COLOR_DODGE		= 17,	// D == 0 ? 0 : S == 1 ? 1 : min(1, D / (1 - S))
		COLOR_BURN		= 18,	// D == 1 ? 1 : S == 0 ? 0 : 1 - min(1, (1 - D) / S)
		HARD_LIGHT		= 19,	// S <= 0.5 ? Multiply(D, 2 * S) : Screen(D, 2 * S - 1)  
		SOFT_LIGHT		= 20,	// S <= 0.5 ? D - (1 - 2 * S) * D * (1 - D) : D + (2 * S - 1) * (T(D) - D); where T(C) = C <= 0.25 ? ((16 * C - 12) * C + 4) * C : sqrt(C)
		DIFFRENCE		= 21,	// | D - S |
		EXCLUSION		= 22,	// D + S - 2 * D * S
		MULTIPLY		= 23,	// D * S
		// Non-separable color blend modes:
		HUE				= 24,	// SetLum(SetSat(Cs, Sat(Cb)), Lum(Cb))
		SATURATION		= 25,	// SetLum(SetSat(Cb, Sat(Cs)), Lum(Cb))
		COLOR			= 26,	// SetLum(Cs, Lum(Cb))
		LUMINOSITY		= 27,	// SetLum(Cb, Lum(Cs))
	};
	offset_pointer<PaintBase, uint24be>	source;
	MODE		mode;
	offset_pointer<PaintBase, uint24be>	backdrop;
};

template<typename T> struct WithCentre : T {
	fword		centerX, centerY;
	auto		matrix()	const {
		auto t = translate(centerX.get(), centerY.get());
		return t * T::matrix() * inverse(t);
	}
};

template<> struct Paint<ScaleAroundCenter>				: WithCentre<Paint<Scale>> {};
template<> struct Paint<ScaleUniformAroundCenter>		: WithCentre<Paint<ScaleUniform>> {};
template<> struct Paint<RotateAroundCenter>				: WithCentre<Paint<Rotate>> {};
template<> struct Paint<SkewAroundCenter>				: WithCentre<Paint<Skew>> {};

template<> struct Paint<VarSolid>						: Var<Paint<Solid>> {};
template<> struct Paint<VarLinearGradient>				: Var<Paint<LinearGradient>> {};
template<> struct Paint<VarRadialGradient>				: Var<Paint<RadialGradient>> {};
template<> struct Paint<VarSweepGradient>				: Var<Paint<SweepGradient>> {};
template<> struct Paint<VarTranslate>					: Var<Paint<Translate>> {};
template<> struct Paint<VarScale>						: Var<Paint<Scale>> {};
template<> struct Paint<VarScaleAroundCenter>			: Var<Paint<ScaleAroundCenter>> {};
template<> struct Paint<VarScaleUniform>				: Var<Paint<ScaleUniform>> {};
template<> struct Paint<VarScaleUniformAroundCenter>	: Var<Paint<ScaleUniformAroundCenter>> {};
template<> struct Paint<VarRotate>						: Var<Paint<Rotate>> {};
template<> struct Paint<VarRotateAroundCenter>			: Var<Paint<RotateAroundCenter>> {};
template<> struct Paint<VarSkew>						: Var<Paint<Skew>> {};
template<> struct Paint<VarSkewAroundCenter>			: Var<Paint<SkewAroundCenter>> {};

template<typename R, typename P> R process(const PaintBase *p, P &&proc) {
	switch (p->format) {
		case ColrLayers:					return proc(p->as<ColrLayers>());
		case Solid:							return proc(p->as<Solid>());
		case VarSolid:						return proc(p->as<VarSolid>());
		case LinearGradient:				return proc(p->as<LinearGradient>());
		case VarLinearGradient:				return proc(p->as<VarLinearGradient>());
		case RadialGradient:				return proc(p->as<RadialGradient>());
		case VarRadialGradient:				return proc(p->as<VarRadialGradient>());
		case SweepGradient:					return proc(p->as<SweepGradient>());
		case VarSweepGradient:				return proc(p->as<VarSweepGradient>());
		case Glyph:							return proc(p->as<Glyph>());
		case ColrGlyph:						return proc(p->as<ColrGlyph>());
		case Transform:						return proc(p->as<Transform>());
		case VarTransform:					return proc(p->as<VarTransform>());
		case Translate:						return proc(p->as<Translate>());
		case VarTranslate:					return proc(p->as<VarTranslate>());
		case Scale:							return proc(p->as<Scale>());
		case VarScale:						return proc(p->as<VarScale>());
		case ScaleAroundCenter:				return proc(p->as<ScaleAroundCenter>());
		case VarScaleAroundCenter:			return proc(p->as<VarScaleAroundCenter>());
		case ScaleUniform:					return proc(p->as<ScaleUniform>());
		case VarScaleUniform:				return proc(p->as<VarScaleUniform>());
		case ScaleUniformAroundCenter:		return proc(p->as<ScaleUniformAroundCenter>());
		case VarScaleUniformAroundCenter:	return proc(p->as<VarScaleUniformAroundCenter>());
		case Rotate:						return proc(p->as<Rotate>());
		case VarRotate:						return proc(p->as<VarRotate>());
		case RotateAroundCenter:			return proc(p->as<RotateAroundCenter>());
		case VarRotateAroundCenter:			return proc(p->as<VarRotateAroundCenter>());
		case Skew:							return proc(p->as<Skew>());
		case VarSkew:						return proc(p->as<VarSkew>());
		case SkewAroundCenter:				return proc(p->as<SkewAroundCenter>());
		case VarSkewAroundCenter:			return proc(p->as<VarSkewAroundCenter>());
		case Composite:						return proc(p->as<Composite>());
		default:							return proc(p);
	}
}

//-----------------------------------------------------------------------------
//	sbix	Standard Bitmap Graphics Table
//-----------------------------------------------------------------------------

struct sbix {
	enum FLAGS {
		DRAW_OUTLINES	= 1 << 1,
	};
	struct glyph {
		enum TAG {
			JPG		= 'jpg ',
			PNG		= 'png ',
			TIFF	= 'tiff',
		};
		int16be		originOffsetX;	//The horizontal (x-axis) position of the left edge of the bitmap graphic in relation to the glyph design space origin.
		int16be		originOffsetY;	//The vertical (y-axis) position of the bottom edge of the bitmap graphic in relation to the glyph design space origin.
		uint32be	graphicType;	//Indicates the format of the embedded graphic data: one of 'jpg ', 'png ' or 'tiff', or the special format 'dupe'.
		uint8		data[];			//The actual embedded graphic data. The total length is inferred from sequential entries in the glyphDataOffsets array and the fixed size (8 bytes) of the preceding fields.
	};
	struct strike {
		uint16be	ppem;							//The PPEM size for which this strike was designed.
		uint16be	ppi;							//The device pixel density (in PPI) for which this strike was designed. (E.g., 96 PPI, 192 PPI.)
		offset_pointer<glyph, uint32be, strike>	_glyphs[];
		const glyph&		operator[](uint32 i)	const	{ return *_glyphs[i].get(this); }
		const_memory_block	data(uint32 i)			const	{ auto total = _glyphs[i+1].offset - _glyphs[i].offset; return {_glyphs[i].get(this)->data, total - 8}; }
		auto				glyphs2()				const	{ return with_param(make_range_n(_glyphs, 3574), this); }
	};

	uint16be	version;	//1
	uint16be	flags;
	uint32be	numStrikes;	//Number of bitmap strikes.
	offset_pointer<strike, uint32be, sbix>	_strikes[];//Offsets from the beginning of the 'sbix' table to data for each individual bitmap strike.
	auto		strikes()	const { return make_range_n(_strikes, numStrikes); }
	auto		strikes2()	const { return with_param(make_range_n(_strikes, numStrikes), this); }
};

//-----------------------------------------------------------------------------
//	SVG
//-----------------------------------------------------------------------------

struct SVG {
	struct Document {
		uint16be	startGlyphID;
		uint16be	endGlyphID;
		offset_pointer<void, packed<uint32be>>	doc;
		packed<uint32be>	doc_length;
	};
	struct Documents {
		uint16be	num_docs;
		Document	_documents[];
		auto	documents()	const { return with_param(make_range_n(unconst(_documents), num_docs), this); }
	};

	struct Document_ref {
		uint32	start, end;
		const_memory_block	data;
		//malloc_block	data;
	};
	friend Document_ref	get(const param_element<Document &,const Documents*> &a)	{
		return {a.t.startGlyphID, a.t.endGlyphID, const_memory_block(a.t.doc.get(a.p), a.t.doc_length)};
	}

	uint16be			version;//	Table version (starting at 0). Set to 0.
	offset_pointer<Documents, packed<uint32be>> _documents;
	packed<uint32be>	reserved;//	Set to 0.
	auto	documents()	const { return _documents.get(this)->documents(); }
};

//-----------------------------------------------------------------------------
//	GSUB + GPOS + GDEF Common
//-----------------------------------------------------------------------------

struct Lookup {
	enum Flag {
		RIGHT_TO_LEFT				= 0x0001,	//This bit relates only to the correct processing of the cursive attachment lookup type (GPOS lookup type 3). When this bit is set, the last glyph in a given sequence to which the cursive attachment lookup is applied, will be positioned on the baseline.
		IGNORE_BASE_GLYPHS			= 0x0002,	//If set, skips over base glyphs
		IGNORE_LIGATURES			= 0x0004,	//If set, skips over ligatures
		IGNORE_MARKS				= 0x0008,	//If set, skips over all combining marks
		USE_MARK_FILTERING_SET		= 0x0010,	//If set, indicates that the lookup table structure is followed by a MarkFilteringSet field. The layout engine skips over all mark glyphs not in the mark filtering set indicated.
		reserved					= 0x00E0,	//For future use (Set to zero)
		MARK_ATTACHMENT_TYPE_MASK	= 0xFF00,	//If not zero, skips over all marks of attachment type different from specified.
	};
	uint16be	type;	//Different enumerations for GSUB and GPOS
	uint16be	flag;	//Lookup qualifiers
	table<uint16be, offset_pointer<CommonTable, uint16be, Lookup>>	subtables;
	//uint16be	markFilteringSet;	//Index (base 0) into GDEF mark glyph sets structure. This field is only present if the USE_MARK_FILTERING_SET lookup flag is set.
};

struct LookupList : table<uint16be, offset_pointer<Lookup, uint16be>> {};

struct Feature {
	offset_pointer<void, uint16be, Feature>	params;
	table<uint16be, uint16be>				indices;
};

struct FeatureRecord {
	packed<TAG>							tag;
	offset_pointer<Feature, uint16be, struct FeatureList>	feature;
};

struct FeatureList : table<uint16be, FeatureRecord> {};

struct LangSys {
	offset_pointer<void, uint16be>		lookupOrderOffset;		//= NULL (reserved for an offset to a reordering table)
	uint16be							requiredFeatureIndex;	//Index of a feature required for this language system; if no required features = 0xFFFF
	table<uint16be, uint16be>			featureIndices;
};

struct LangSysRecord {
	packed<TAG>							tag;
	offset_pointer<LangSys, uint16be, struct Script>	langSys;
};

struct Script {
	offset_pointer<LangSys, uint16be, Script>		defaultLangSys;
	table<uint16be, LangSysRecord>		langSysRecords;
};

struct ScriptRecord {
	packed<TAG>							tag;
	offset_pointer<Script, uint16be, struct ScriptList>	script;
};

struct ScriptList : table<uint16be, ScriptRecord> {};

struct Coverage : CommonTable {
	int	lookup(uint32 glyph) const;
};

struct Coverage1 : Coverage {
	table<uint16be, uint16be>	glyphs;
	int	lookup(uint32 glyph) const {
		auto	i = lower_boundc(glyphs, glyph);
		return i == glyphs.end() ? -1 : i - glyphs.begin();
	}
};

struct Coverage2 : Coverage {
	struct Range {
		uint16be	start;				//First glyph ID in the range
		uint16be	end;				//Last glyph ID in the range
		uint16be	startCoverageIndex;	//Coverage Index of first glyph ID in range
	};
	table<uint16be, Range>	ranges;
	int	lookup(uint32 glyph) const {
		auto	i = lower_boundc(ranges, glyph, [](const Range &r, uint32 g) { return r.start < g; });
		return i == ranges.end() || i->end < glyph ? -1 : glyph - i->start + i->startCoverageIndex;
	}
};

inline int Coverage::lookup(uint32 glyph) const {
	switch (format) {
		case 1: return ((const Coverage1*)this)->lookup(glyph);
		case 2: return ((const Coverage2*)this)->lookup(glyph);
		default: return -1;
	}
}


//-------------------------------------

struct ClassDef : CommonTable {};

struct ClassDef1 : ClassDef {
	uint16be						start;	//First glyph ID of the classValueArray
	table<uint16be, uint16be>		values;
};

struct ClassDef2 : ClassDef {
	struct Range {
		uint16be	start;	//First glyph ID in the range
		uint16be	end;	//Last glyph ID in the range
		uint16be	Class;	//Applied to all glyphs in the range
	};
	table<uint16be, Range>		ranges;
};

//-------------------------------------

struct SequenceLookup {
	uint16be	sequenceIndex;		//Index (zero-based) into the input glyph sequence
	uint16be	lookupListIndex;	//Index (zero-based) into the LookupList
};

struct ClassSequenceRule {
	uint16be	glyphCount;		//Number of glyphs in the input glyph sequence
	uint16be	seqLookupCount;	//Number of SequenceLookupRecords
	uint16be	inputSequence[];	//[glyphCount - 1]	Array of input glyph IDs—starting with the second glyph
	//SequenceLookup	seqLookupRecords;//[seqLookupCount]	Array of Sequence lookup records
	auto	input()		const	{ return make_range_n(inputSequence, glyphCount); }
	auto	seqLookup()	const	{ return make_range_n((SequenceLookup*)input().end(), seqLookupCount); }
};
struct ClassSequenceRuleSet : table<uint16be, offset_pointer<ClassSequenceRule, uint16be>> {};

struct SequenceContext1 : CommonTable {
	offset_pointer<Coverage, uint16be, SequenceContext1>							coverage;
	table<uint16be, offset_pointer<ClassSequenceRuleSet, uint16be, SequenceContext1>>	rule_sets;
};

struct SequenceContext2 : CommonTable {
	offset_pointer<Coverage, uint16be, SequenceContext2>							coverage;
	offset_pointer<ClassDef, uint16be, SequenceContext2>							class_defs;
	table<uint16be, offset_pointer<ClassSequenceRuleSet, uint16be, SequenceContext2>>	rule_sets;
};

struct SequenceContext3 : CommonTable {
	uint16be	glyphCount;
	uint16be	seqLookupCount;
	offset_pointer<Coverage, uint16be, SequenceContext3>	_coverages[];
//	SequenceLookup	seqLookupRecords;//[seqLookupCount]	Array of SequenceLookupRecords
	auto	coverages()	const	{ return make_range_n(_coverages, glyphCount); }
	auto	seqLookup()	const	{ return make_range_n((SequenceLookup*)coverages().end(), seqLookupCount); }
};

//-------------------------------------

struct ChainedSequenceRule {
	typedef	table<uint16be, uint16be> glyph_table;
	glyph_table	_backtrack;
	//uint16be	backtrackGlyphCount;	//Number of glyphs in the backtrack sequence
	//uint16be	backtrackSequence;//[backtrackGlyphCount];	//Array of backtrack glyph IDs
	//uint16be	inputGlyphCount;	//Number of glyphs in the input sequence
	//uint16be	inputSequence;//[inputGlyphCount - 1];	//Array of input glyph IDs—start with second glyph
	//uint16be	lookaheadGlyphCount;	//Number of glyphs in the lookahead sequence
	//uint16be	lookaheadSequence;//[lookaheadGlyphCount];	//Array of lookahead glyph IDs
	//uint16be	seqLookupCount;	//Number of SequenceLookupRecords
	//SequenceLookup	seqLookupRecords;//[seqLookupCount];	//Array of SequenceLookupReco

	auto	backtrack()	const	{ return _backtrack.all(); }
	auto	input()		const	{ return ((glyph_table*)backtrack().end())->all(); }
	auto	lookahead()	const	{ return ((glyph_table*)input().end())->all(); }
	auto	seqLookup()	const	{ return ((table<uint16be, SequenceLookup>*)lookahead().end())->all(); }
};
struct ChainedSequenceRuleSet : table<uint16be, offset_pointer<ChainedSequenceRule, uint16be>> {};

struct ChainedSequenceContext1 : CommonTable {
	offset_pointer<void, uint16be, ChainedSequenceContext1>				coverage;
	table<uint16be, offset_pointer<ChainedSequenceRuleSet, uint16be, ChainedSequenceContext1>>	rule_sets;
};

struct ChainedSequenceContext2 : CommonTable {
	offset_pointer<Coverage, uint16be, ChainedSequenceContext2>	coverage;
	offset_pointer<ClassDef, uint16be, ChainedSequenceContext2>	backtrack;	//table containing backtrack sequence context, from beginning of ChainedSequenceContextFormat2 table
	offset_pointer<ClassDef, uint16be, ChainedSequenceContext2>	input;		//table containing input sequence context, from beginning of ChainedSequenceContextFormat2 table
	offset_pointer<ClassDef, uint16be, ChainedSequenceContext2>	lookahead;	//table containing lookahead sequence context, from beginning of ChainedSequenceContextFormat2 table
	table<uint16be, offset_pointer<ChainedSequenceRuleSet, uint16be, ChainedSequenceContext2>>	rule_sets;
};

struct ChainedSequenceContext3 : CommonTable {
	typedef	table<uint16be, offset_pointer<Coverage, uint16be, ChainedSequenceContext3>> glyph_table;
	glyph_table	_backtrack;
	//uint16be		backtrackGlyphCount;	//Number of glyphs in the backtrack sequence
	//offset_pointer<void, uint16be>	backtrackCoverageOffsets;//[backtrackGlyphCount]	Array of offsets to coverage tables for the backtrack sequence
	//uint16be		inputGlyphCount;	//Number of glyphs in the input sequence
	//offset_pointer<void, uint16be>	inputCoverageOffsets;//[inputGlyphCount]	Array of offsets to coverage tables for the input sequence
	//uint16be		lookaheadGlyphCount;	//Number of glyphs in the lookahead sequence
	//offset_pointer<void, uint16be>	lookaheadCoverageOffsets;//[lookaheadGlyphCount]	Array of offsets to coverage tables for the lookahead sequence
	//uint16be		seqLookupCount;	//Number of SequenceLookupRecords
	//SequenceLookup	seqLookupRecords;//[seqLookupCount]	Array of SequenceLookupRecords

	auto	backtrack()	const	{ return _backtrack.all(); }
	auto	input()		const	{ return ((glyph_table*)backtrack().end())->all(); }
	auto	lookahead()	const	{ return ((glyph_table*)input().end())->all(); }
	auto	seqLookup()	const	{ return ((table<uint16be, SequenceLookup>*)lookahead().end())->all(); }
};

//-----------------------------------------------------------------------------
//	GSUB	Glyph Substitution Table
//-----------------------------------------------------------------------------

struct GSUB {
	enum {
		SINGLE		= 1,	// Replace one glyph with one glyph
		MULTIPLE	= 2,	// Replace one glyph with more than one glyph
		ALTERN		= 3,	// Replace one glyph with one of many glyphs
		LIGATURE	= 4,	// Replace multiple glyphs with one glyph
		CONTEXTUAL	= 5,	// Replace one or more glyphs in context
		CHAINED		= 6,	// Replace one or more glyphs in chained context
		EXTENSION	= 7,	// Extension mechanism for other substitutions (i.e. this excludes the Extension type substitution itself)
		REVERSE		= 8,	// Applied in reverse order, replace single glyph in chaining context
	};

	struct Single1 : CommonTable {
		offset_pointer<Coverage, uint16be, Single1>					coverage;
		int16be														delta;		//Add to original glyph ID to get substitute glyph ID
	};

	struct Single2 : CommonTable {
		offset_pointer<Coverage, uint16be, Single2>					coverage;
		table<uint16be, uint16be>									substitutes;//	Array of substitute glyph IDs — ordered by Coverage index
	};

	struct Multiple1 : CommonTable {
		struct Sequence : table<uint16be,uint16be> {};
		offset_pointer<Coverage, uint16be, Multiple1>				coverage;
		table<uint16be, offset_pointer<Sequence, uint16be, Multiple1>>	sequences;
	};

	struct Alternate1 : CommonTable {
		struct AlternateSet : table<uint16be, uint16be> {};
		offset_pointer<Coverage, uint16be, Alternate1>				coverage;
		table<uint16be, offset_pointer<AlternateSet, uint16be, Alternate1>>	alternate_sets;
	};

	struct Ligature1 : CommonTable {
		struct LigatureGlyph {
			uint16be	ligature;	//glyph ID of ligature to substitute
		};
		struct Ligature		: LigatureGlyph, table<uint16be, uint16be> {};
		typedef table<uint16be, offset_pointer<Ligature, uint16be>>		LigatureSet;

		offset_pointer<Coverage, uint16be, Ligature1>				coverage;
		table<uint16be, offset_pointer<LigatureSet, uint16be, Ligature1>>	sets;
	};

	struct Reverse1 : CommonTable {
		typedef	table<uint16be, offset_pointer<void, uint16be>> glyph_table;
		offset_pointer<Coverage, uint16be>	coverage;
		glyph_table	_backtrack;
		//uint16be	backtrackGlyphCount;	//Number of glyphs in the backtrack sequence.
		//offset_pointer<void, uint16be>	backtrackCoverageOffsets;//[backtrackGlyphCount]	Array of offsets to coverage tables in backtrack sequence, in glyph sequence order.
		//uint16be	lookaheadGlyphCount;	//Number of glyphs in lookahead sequence.
		//offset_pointer<void, uint16be>	lookaheadCoverageOffsets;//[lookaheadGlyphCount]	Array of offsets to coverage tables in lookahead sequence, in glyph sequence order.
		//uint16be	glyphCount;	//Number of glyph IDs in the substituteGlyphIDs array.
		//uint16be	substituteGlyphIDs[];//	Array of substitute glyph IDs — ordered by Coverage index.

		auto	backtrack()		const	{ return _backtrack.all(); }
		auto	input()			const	{ return ((glyph_table*)backtrack().end())->all(); }
		auto	lookahead()		const	{ return ((glyph_table*)input().end())->all(); }
		auto	substitute()	const	{ return ((table<uint16be, uint16be>*)lookahead().end())->all(); }
	};


	uint16be		majorVersion;
	uint16be		minorVersion;
	offset_pointer<ScriptList, uint16be>	scripts;
	offset_pointer<FeatureList, uint16be>	features;
	offset_pointer<LookupList, uint16be>	lookups;
};

struct GSUB_1_1 : GSUB {
	offset_pointer<void, uint32be, GSUB_1_1>	variations;
};

//-----------------------------------------------------------------------------
//	GPOS	Glyph Positioning Table
//-----------------------------------------------------------------------------

struct GPOS {
	enum {
		SINGLE			= 1,//	Adjust position of a single glyph
		PAIR			= 2,//	Adjust position of a pair of glyphs
		CURSIVE			= 3,//	Attach cursive glyphs
		MARK_TO_BASE	= 4,//	Attach a combining mark to a base glyph
		MARK_TO_LIG		= 5,//	Attach a combining mark to a ligature
		MARK_TO_MARK	= 6,//	Attach a combining mark to another mark
		CONTEXTUAL		= 7,//	Position one or more glyphs in context
		CHAINED			= 8,//	positioning	Position one or more glyphs in chained context
		EXTENSION		= 9,//	Extension mechanism for other positionings
	};

	struct Anchor : CommonTable {};
	struct Anchor1 : Anchor {
		int16be		xCoordinate;	//Horizontal value, in design units
		int16be		yCoordinate;	//Vertical value, in design units
	};
	struct Anchor2 : Anchor {
		int16be		xCoordinate;	//Horizontal value, in design units
		int16be		yCoordinate;	//Vertical value, in design units
		uint16be	anchorPoint;	//Index to glyph contour point
	};
	struct Anchor3 : Anchor {
		int16be		xCoordinate;	//Horizontal value, in design units
		int16be		yCoordinate;	//Vertical value, in design units
		offset_pointer<void, uint16be>	xDevice;
		offset_pointer<void, uint16be>	yDevice;
	};

	struct ValueRecord {
		int16be		xPlacement;	//Horizontal adjustment for placement, in design units.
		int16be		yPlacement;	//Vertical adjustment for placement, in design units.
		int16be		xAdvance;	//Horizontal adjustment for advance, in design units — only used for horizontal layout.
		int16be		yAdvance;	//Vertical adjustment for advance, in design units — only used for vertical layout.
		offset_pointer<void, uint16be>	xPlaDevice;
		offset_pointer<void, uint16be>	yPlaDevice;
		offset_pointer<void, uint16be>	xAdvDevice;
		offset_pointer<void, uint16be>	yAdvDevice;
	};
	enum _ValueFormat {
		X_PLACEMENT			= 0x0001,	//Includes horizontal adjustment for placement
		Y_PLACEMENT			= 0x0002,	//Includes vertical adjustment for placement
		X_ADVANCE			= 0x0004,	//Includes horizontal adjustment for advance
		Y_ADVANCE			= 0x0008,	//Includes vertical adjustment for advance
		X_PLACEMENT_DEVICE	= 0x0010,	//Includes Device table (non-variable font) / VariationIndex table (variable font) for horizontal placement
		Y_PLACEMENT_DEVICE	= 0x0020,	//Includes Device table (non-variable font) / VariationIndex table (variable font) for vertical placement
		X_ADVANCE_DEVICE	= 0x0040,	//Includes Device table (non-variable font) / VariationIndex table (variable font) for horizontal advance
		Y_ADVANCE_DEVICE	= 0x0080,	//Includes Device table (non-variable font) / VariationIndex table (variable font) for vertical advance
		Reserved			= 0xFF00,	//For future use (set to zero)
	};
	typedef bigendian<_ValueFormat>	ValueFormat;

	struct SinglePos1 : CommonTable {
		offset_pointer<Coverage, uint16be>	coverage;
		ValueFormat						valueFormat;
		ValueRecord						valueRecord;
	};
	struct SinglePos2 : CommonTable {
		offset_pointer<Coverage, uint16be>	coverage;
		ValueFormat						valueFormat;
		table<uint16be, ValueRecord>	values;
	};

	struct PairPos1 : CommonTable {
		struct PairValueRecord {
			uint16be		secondGlyph;	//Glyph ID of second glyph in the pair (first glyph is listed in the Coverage table).
			ValueRecord		valueRecord1;	//Positioning data for the first glyph in the pair.
			ValueRecord		valueRecord2;	//Positioning data for the second glyph in the pair.
		};
		typedef table<uint16be, PairValueRecord>	PairSet;

		offset_pointer<Coverage, uint16be>	coverage;
		ValueFormat						valueFormat1;	//Defines the types of data in valueRecord1 — for the first glyph in the pair (may be zero).
		ValueFormat						valueFormat2;	//Defines the types of data in valueRecord2 — for the second glyph in the pair (may be zero).
		table<uint16be, offset_pointer<PairSet, uint16be>>	pairSets;
	};

	struct PairPos2 : CommonTable {
		struct Record {
			ValueRecord	valueRecord1;	//Positioning for first glyph — empty if valueFormat1 = 0.
			ValueRecord	valueRecord2;	//Positioning for second glyph — empty if valueFormat2 = 0.
		};
		offset_pointer<Coverage, uint16be>	coverage;
		ValueFormat	valueFormat1;	// for the first glyph of the pair (may be zero).
		ValueFormat	valueFormat2;	// for the second glyph of the pair (may be zero).
		offset_pointer<ClassDef, uint16be>	classDef1Offset;	//Offset to ClassDef table, from beginning of PairPos subtable — for the first glyph of the pair.
		offset_pointer<ClassDef, uint16be>	classDef2Offset;	//Offset to ClassDef table, from beginning of PairPos subtable — for the second glyph of the pair.
		uint16be	class1Count;	//Number of classes in classDef1 table — includes Class 0.
		uint16be	class2Count;	//Number of classes in classDef2 table — includes Class 0.
		Record		_records[];//	Array of Class1 records, ordered by classes in classDef1.

		auto	records() const { return make_range_n(_records, class1Count * class2Count); }
	};

	struct CursivePos1 : CommonTable {
		struct EntryExitRecord {
			offset_pointer<Anchor, uint16be>	entryAnchor;
			offset_pointer<Anchor, uint16be>	exitAnchor;
		};
		offset_pointer<Coverage, uint16be>	coverage;
		table<uint16be, EntryExitRecord>	entryExits;
	};

	struct MarkRecord {
		uint16be	markClass;
		offset_pointer<Anchor, uint16be>	markAnchor;
	};
	typedef table<uint16be, MarkRecord>	 MarkArray;

	struct MarkBasePos1 : CommonTable {
		struct BaseRecord {
			offset_pointer<Anchor, uint16be>	baseAnchorOffsets[];//	Array of offsets (one per mark class) to Anchor tables. Offsets are from beginning of BaseArray table, ordered by class (offsets may be NULL).
		};
		typedef table<uint16be, BaseRecord>	 BaseArray;

		offset_pointer<Coverage, uint16be>		markCoverage;
		offset_pointer<Coverage, uint16be>		baseCoverage;
		uint16be								markClassCount;	//Number of classes defined for marks
		offset_pointer<MarkArray, uint16be>		markArray;
		offset_pointer<BaseArray, uint16be>		baseArray;
	};

	struct MarkLigPos1 : CommonTable {
		struct LigatureAttach {
			table<uint16be, offset_pointer<Anchor, uint16be>>	components;
		};
		typedef table<uint16be, offset_pointer<LigatureAttach, uint16be>> LigatureArray;

		offset_pointer<Coverage, uint16be>		markCoverage;
		offset_pointer<Coverage, uint16be>		ligatureCoverage;
		uint16be								markClassCount;	//Number of defined mark classes
		offset_pointer<MarkArray, uint16be>		markArray;
		offset_pointer<LigatureArray, uint16be>	ligatureArray;
	};

	struct MarkMarkPos1 : CommonTable {
		typedef table<uint16be, offset_pointer<Anchor, uint16be>>	Mark2Array;

		offset_pointer<Coverage, uint16be>		mark1Coverage;
		offset_pointer<Coverage, uint16be>		mark2Coverage;
		uint16be								markClassCount;	//Number of Combining Mark classes defined
		offset_pointer<MarkArray, uint16be>		mark1Array;
		offset_pointer<Mark2Array, uint16be>	mark2Array;
	};


	struct ExtensionPos1 : CommonTable {
		uint16be						type;
		offset_pointer<void, uint32>	extension;
	};

	uint16be		majorVersion;
	uint16be		minorVersion;
	offset_pointer<ScriptList, uint16be>	scripts;
	offset_pointer<FeatureList, uint16be>	features;
	offset_pointer<LookupList, uint16be>	lookups;
};

struct GPOS_1_1 : GPOS {
	offset_pointer<void, uint32be, GPOS_1_1>	variations;
};

//-----------------------------------------------------------------------------
//	EOT
//-----------------------------------------------------------------------------

struct EOTHeader : littleendian_types {
	enum {MAGIC = 0x504c};

	uint32be	eot_size;
	uint32be	font_size;
	uint32be	version;
	uint32be	flags;
	PANOSE		panose;
	uint8		charset;
	uint8		italic;
	uint32be	weight;
	uint16be	type;
	uint16be	magic;
	uint32be	unicode_range[4];
	uint32be	codepage_range[2];
	uint32be	checksum_adjustment;
	uint32be	reserved[4];
	uint16be	padding1;
	/*
	//all padding values must be set to 0x0000
	struct name {
		uint16be	size;
		char	bytes[1];
	};

	name	family_name;		//Family string found in the name table of the font (name ID = 1)
	uint16be	padding2;
	name	style_name;			//Subfamily string found in the name table of the font (name ID = 2)
	uint16be	padding3;
	name	version_name;		//Version string found in the name table of the font (name ID = 5)
	uint16be	padding4;
	name	full_name;			//Full name string found in the name table of the font (name ID = 4)

	//Version 0x00020001
	uint16be	padding5;
	name	root_string;

	//Version 0x00020002
	uint32be	root_string_checksum;
	uint32be	EUDC_codepage;
	uint16be	padding6;
	name	signature;
	uint32be	EUDCFlags;			//processing flags for the EUDC font. Typical values might be TTEMBED_XORENCRYPTDATA and TTEMBED_TTCOMPRESSED.
	name	EUDCFontData;

	// all
	uint8	data[1];//font_size];	//compressed or XOR encrypted as indicated by the processing flags.
	*/
};

uint32 CalcTableChecksum(uint32 *table, uint32 length);

}// namespace ttf
