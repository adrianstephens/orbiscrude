#include "iso/iso_files.h"
#include "codec/vlc.h"
#include "codec/reed_solomon.h"
#include "base/algorithm.h"
#include "base/vector.h"
#include "utilities.h"


#undef TEXT
#undef POINT

namespace iso {
template<typename T, int N> auto get(const array<T, N>& t) {
	typedef decltype(get(declval<T>()))	T2;
	return array<T2, N>(t);
}
}

using namespace iso;

//-----------------------------------------------------------------------------
// decompression
//-----------------------------------------------------------------------------

struct decompress_dwg {
	const uint8*	compressedBuffer;
	uint32	compressedSize;
	uint32	compressedPos;
	bool	compressedGood;

	uint8*	decompBuffer;
	uint32	decompSize;
	uint32	decompPos;
	bool	decompGood;

	bool buffersGood() { return compressedGood && decompGood; }

	bool compressedInc(const int32 inc= 1) {
		compressedPos += inc;
		return compressedGood = (compressedPos <= compressedSize);
	}
	uint8 compressedByte() {
		uint8 result = 0;
		if (compressedGood = compressedPos < compressedSize) {
			result = compressedBuffer[compressedPos];
			++compressedPos;
		}
		return result;
	}
	uint8 compressedByte(const uint32 index) {
		return index < compressedSize ? compressedBuffer[index] : 0;
	}

	uint8 decompByte(const uint32 index) {
		return index < decompSize ? decompBuffer[index] : 0;
	}
	void decompSet(const uint8 value) {
		if (decompGood = decompPos < decompSize) {
			decompBuffer[decompPos] = value;
			++decompPos;
		}
	}

	decompress_dwg(const void *cbuf, void *dbuf, uint64 csize, uint64 dsize) : 
		compressedBuffer((const uint8*)cbuf),	compressedSize(csize),	compressedPos(0),	compressedGood(true),
		decompBuffer((uint8*)dbuf),				decompSize(dsize),		decompPos(0),		decompGood(true)
	{}
};

struct decompress18 : decompress_dwg {
	using decompress_dwg::decompress_dwg;
	uint32 litLength() {
		uint32	cont = 0;
		uint8	ll = compressedByte();
		//no literal length, this byte is next opCode
		if (ll > 0x0F) {
			--compressedPos;
			return 0;
		}

		if (ll == 0x00) {
			cont = 0x0F;
			ll	= compressedByte();
			while (ll == 0 && compressedGood) {//repeat until ll != 0x00
				cont += 0xFF;
				ll = compressedByte();
			}
		}

		return cont + ll + 3;
	}
	uint32 twoByteOffset(uint32 *ll) {
		uint8	fb		= compressedByte();
		uint32	cont	= (fb >> 2) | (compressedByte() << 6);
		*ll = (fb & 0x03);
		return cont;
	}
	uint32 longCompressionOffset() {
		uint32	cont	= 0;
		uint8	ll		= compressedByte();
		while (ll == 0 && compressedGood) {
			cont	+= 0xFF;
			ll		= compressedByte();
		}
		return cont += ll;
	}

	bool process();
};

bool decompress18::process() {
	uint32 compBytes	= 0;
	uint32 compOffset	= 0;
	uint32 litCount		= litLength();

	//copy first literal length
	for (uint32 i = 0; i < litCount && buffersGood(); ++i)
		decompSet( compressedByte());

	while (buffersGood()) {
		uint8 oc = compressedByte(); //next opcode
		if (oc == 0x10) {
			compBytes	= longCompressionOffset()+ 9;
			compOffset	= twoByteOffset(&litCount) + 0x3FFF;
			if (litCount == 0)
				litCount= litLength();

		} else if (oc > 0x11 && oc< 0x20) {
			compBytes	= (oc & 0x0F) + 2;
			compOffset	= twoByteOffset(&litCount) + 0x3FFF;
			if (litCount == 0)
				litCount= litLength();

		} else if (oc == 0x20) {
			compBytes	= longCompressionOffset() + 0x21;
			compOffset	= twoByteOffset(&litCount);
			if (litCount == 0)
				litCount= litLength();

		} else if (oc > 0x20 && oc< 0x40) {
			compBytes	= oc - 0x1E;
			compOffset	= twoByteOffset(&litCount);
			if (litCount == 0)
				litCount= litLength();

		} else if (oc > 0x3F) {
			compBytes	= ((oc & 0xF0) >> 4) - 1;
			compOffset	= (compressedByte() << 2) | ((oc & 0x0C) >> 2);
			litCount	= oc & 0x03;
			if (litCount < 1) {
				litCount = litLength();}

		} else if (oc == 0x11) {
			return true; //end of input stream

		} else { //ll < 0x10
			return false; //fails, not valid
		}

		//copy "compressed data", if size allows
		if (decompSize < decompPos + compBytes)
			compBytes = decompSize - decompPos;		// only copy what we can fit

		uint32 j = decompPos - compOffset - 1;
		for (uint32 i = 0; i < compBytes && buffersGood(); i++)
			decompSet(decompByte(j++));

		//copy "uncompressed data", if size allows
		if (decompSize < decompPos + litCount)
			litCount = decompSize - decompPos;		// only copy what we can fit

		for (uint32 i = 0; i < litCount && buffersGood(); i++)
			decompSet( compressedByte());
	}

	return false;
}

struct decompress21 : decompress_dwg {
	enum {
		MaxBlockLength = 32,
		BlockOrderArray,
	};
	static const uint8 *CopyOrder[];

	using decompress_dwg::decompress_dwg;

	uint32 litLength(uint8 opCode) {
		uint32 length = 8 + opCode;
		if (length == 0x17) {
			uint32 n = compressedByte();
			length += n;
			if (n == 0xffu) {
				do {
					n = compressedByte();
					n |= compressedByte() << 8;
					length += n;
				} while (n == 0xffffu);
			}
		}
		return length;
	}
	bool copyCompBytes(uint32 length) {
		while (length) {
			uint32	n = min(length, MaxBlockLength);
			auto order = CopyOrder[n];
			for (uint32 index = 0; n > index && buffersGood(); ++index)
				decompSet(compressedByte( compressedPos + order[index]));
			compressedInc(n);
			length -= n;
		}
		return buffersGood();
	}
	void readInstructions(uint8 &opCode, uint32 &sourceOffset, uint32 &length) {
		switch (opCode >> 4) {
			case 0:
				length			= (opCode & 0x0f) + 0x13;
				sourceOffset	= compressedByte();
				opCode			= compressedByte();
				length			= ((opCode >> 3) & 0x10) + length;
				sourceOffset	= ((opCode & 0x78) << 5) + 1 + sourceOffset;
				break;
			case 1:
				length			= (opCode & 0xf) + 3;
				sourceOffset	= compressedByte();
				opCode			= compressedByte();
				sourceOffset	= ((opCode & 0xf8) << 5) + 1 + sourceOffset;
				break;
			case 2:
				sourceOffset	= compressedByte();
				sourceOffset	= (compressedByte() << 8) | sourceOffset;
				length			= opCode & 7;
				if (!(opCode & 8)) {
					opCode		= compressedByte();
					length		= (opCode & 0xf8) + length;
				} else {
					++sourceOffset;
					length		= (compressedByte() << 3) + length;
					opCode		= compressedByte();
					length		= (((opCode & 0xf8) << 8) + length) + 0x100;
				}
				break;
			default:
				length			= opCode >> 4;
				sourceOffset	= opCode & 15;
				opCode			= compressedByte();
				sourceOffset	= (((opCode & 0xf8) << 1) + sourceOffset) + 1;
				break;
		}
	}
	bool process();
};

bool decompress21::process() {
	uint32	length			= 0;
	uint32	sourceOffset	= 0;
	uint8	opCode			= compressedByte();

	if ((opCode >> 4) == 2) {
		compressedInc( 2);
		length = compressedByte() & 0x07;
	}

	while (buffersGood()) {
		if (length == 0)
			length = litLength(opCode);
		
		copyCompBytes(length);

		if (decompPos >= decompSize)
			break; //check if last chunk are compressed & terminate

		length = 0;
		opCode = compressedByte();
		readInstructions(opCode, sourceOffset, length);
		for (;;) {
			//prevent crash with corrupted data
			if (sourceOffset > decompPos)
				sourceOffset = decompPos;

			//prevent crash with corrupted data
			if (length > decompSize - decompPos) {
				length = decompSize - decompPos;
				compressedPos	= compressedSize; //force exit
				compressedGood	= false;
			}
			sourceOffset = decompPos - sourceOffset;
			for (uint32 i = 0; i< length; i++)
				decompSet(decompByte(sourceOffset + i));

			length = opCode & 7;
			if ((length != 0) || (compressedPos >= compressedSize))
				break;

			opCode = compressedByte();
			if ((opCode >> 4) == 0)
				break;

			if ((opCode >> 4) == 15)
				opCode &= 15;

			readInstructions( opCode, sourceOffset, length);
		}

		if (compressedPos >= compressedSize)
			break;
	}
	return buffersGood();
}

const uint8 *decompress21::CopyOrder[] = {
	nullptr,
	(const uint8[]){0},
	(const uint8[]){1,0},
	(const uint8[]){2,1,0},
	(const uint8[]){0,1,2,3},
	(const uint8[]){4,0,1,2,3},
	(const uint8[]){5,1,2,3,4,0},
	(const uint8[]){6,5,1,2,3,4,0},
	(const uint8[]){0,1,2,3,4,5,6,7},
	(const uint8[]){8,0,1,2,3,4,5,6,7},
	(const uint8[]){9,1,2,3,4,5,6,7,8,0},
	(const uint8[]){10,9,1,2,3,4,5,6,7,8,0},
	(const uint8[]){8,9,10,11,0,1,2,3,4,5,6,7},
	(const uint8[]){12,8,9,10,11,0,1,2,3,4,5,6,7},
	(const uint8[]){13,9,10,11,12,1,2,3,4,5,6,7,8,0},
	(const uint8[]){14,13,9,10,11,12,1,2,3,4,5,6,7,8,0},
	(const uint8[]){8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){9,10,11,12,13,14,15,16,8,0,1,2,3,4,5,6,7},
	(const uint8[]){17,9,10,11,12,13,14,15,16,1,2,3,4,5,6,7,8,0},
	(const uint8[]){18,17,16,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){16,17,18,19,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){20,16,17,18,19,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){21,20,16,17,18,19,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){22,21,20,16,17,18,19,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){16,17,18,19,20,21,22,23,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){17,18,19,20,21,22,23,24,16,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){25,17,18,19,20,21,22,23,24,16,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){26,25,17,18,19,20,21,22,23,24,16,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){24,25,26,27,16,17,18,19,20,21,22,23,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){28,24,25,26,27,16,17,18,19,20,21,22,23,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){29,28,24,25,26,27,16,17,18,19,20,21,22,23,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
	(const uint8[]){30,26,27,28,29,18,19,20,21,22,23,24,25,10,11,12,13,14,15,16,17,2,3,4,5,6,7,8,9,1,0},
	(const uint8[]){24,25,26,27,28,29,30,31,16,17,18,19,20,21,22,23,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
};

//-----------------------------------------------------------------------------
namespace dwg {

enum VER {
	BAD_VER			= 0,
	R13				= 1012,
	R14				= 1014,
	R2000			= 1015,
	R2004			= 1018,
	R2007			= 1021,
	R2010			= 1024,
	R2013			= 1027,
	R2018			= 1032,
	MIN_VER			= R13, MAX_VER = R2018
};

enum DXFCODE {
	DXF_STRING		= 1000,
	DXF_INVALID		= 1001,
	DXF_BRACKET		= 1002,
	DXF_LAYER_REF	= 1003,
	DXF_BINARY		= 1004,
	DXF_ENTITY_REF	= 1005,
	DXF_POINTS		= 1010,
	DXF_REALS		= 1040,
	DXF_SHORT		= 1070,
	DXF_LONG		= 1071,
};

enum OBJECTTYPE : uint16 {
	UNUSED					= 0x00,
	TEXT					= 0x01,
	ATTRIB					= 0x02,
	ATTDEF					= 0x03,
	BLOCK					= 0x04,
	ENDBLK					= 0x05,
	SEQEND					= 0x06,
	INSERT					= 0x07,
	MINSERT					= 0x08,
	//						= 0x09,
	VERTEX_2D				= 0x0A,
	VERTEX_3D				= 0x0B,
	VERTEX_MESH				= 0x0C,
	VERTEX_PFACE			= 0x0D,
	VERTEX_PFACE_FACE		= 0x0E,
	POLYLINE_2D				= 0x0F,
	POLYLINE_3D				= 0x10,
	ARC						= 0x11,
	CIRCLE					= 0x12,
	LINE					= 0x13,
	DIMENSION_ORDINATE		= 0x14,
	DIMENSION_LINEAR		= 0x15,
	DIMENSION_ALIGNED		= 0x16,
	DIMENSION_ANG_PT3		= 0x17,
	DIMENSION_ANG_LN2		= 0x18,
	DIMENSION_RADIUS		= 0x19,
	DIMENSION_DIAMETER		= 0x1A,
	POINT					= 0x1B,
	FACE_3D					= 0x1C,
	POLYLINE_PFACE			= 0x1D,
	POLYLINE_MESH			= 0x1E,
	SOLID					= 0x1F,
	TRACE					= 0x20,
	SHAPE					= 0x21,
	VIEWPORT				= 0x22,
	ELLIPSE					= 0x23,
	SPLINE					= 0x24,
	REGION					= 0x25,
	SOLID_3D				= 0x26,
	BODY					= 0x27,
	RAY						= 0x28,
	XLINE					= 0x29,
	DICTIONARY				= 0x2A,
	OLEFRAME				= 0x2B,
	MTEXT					= 0x2C,
	LEADER					= 0x2D,
	TOLERANCE				= 0x2E,
	MLINE					= 0x2F,
	BLOCK_CONTROL_OBJ		= 0x30,
	BLOCK_HEADER			= 0x31,
	LAYER_CONTROL_OBJ		= 0x32,
	LAYER					= 0x33,
	STYLE_CONTROL_OBJ		= 0x34,
	STYLE					= 0x35,
	//						= 0x36,
	//						= 0x37,
	LTYPE_CONTROL_OBJ		= 0x38,
	LTYPE					= 0x39,
	//						= 0x3A,
	//						= 0x3B,
	VIEW_CONTROL_OBJ		= 0x3C,
	VIEW					= 0x3D,
	UCS_CONTROL_OBJ			= 0x3E,
	UCS						= 0x3F,
	VPORT_CONTROL_OBJ		= 0x40,
	VPORT					= 0x41,
	APPID_CONTROL_OBJ		= 0x42,
	APPID					= 0x43,
	DIMSTYLE_CONTROL_OBJ	= 0x44,
	DIMSTYLE				= 0x45,
	VP_ENT_HDR_CTRL_OBJ		= 0x46,
	VP_ENT_HDR				= 0x47,
	GROUP					= 0x48,
	MLINESTYLE				= 0x49,
	OLE2FRAME				= 0x4A,
	//	(DUMMY)				= 0x4B,
	LONG_TRANSACTION		= 0x4C,
	LWPOLYLINE				= 0x4D,
	HATCH					= 0x4E,
	XRECORD					= 0x4F,
	ACDBPLACEHOLDER			= 0x50,
	VBA_PROJECT				= 0x51,
	LAYOUT					= 0x52,

	IMAGE					= 0x65,
	IMAGEDEF				= 0x66,

	ACAD_PROXY_ENTITY		= 0x1f2,
	ACAD_PROXY_OBJECT		= 0x1f3,

	_LOOKUP					= 0x1f4,	//=500,

	// non-fixed types:
	ACAD_TABLE				= 0x8000,
	CELLSTYLEMAP,
	DBCOLOR,
	DICTIONARYVAR,
	DICTIONARYWDFLT,
	FIELD,
	IDBUFFER,
	IMAGEDEFREACTOR,
	LAYER_INDEX,
	LWPLINE,
	MATERIAL,
	MLEADER,
	MLEADERSTYLE,
	PLACEHOLDER,
	PLOTSETTINGS,
	RASTERVARIABLES,
	SCALE,
	SORTENTSTABLE,
	SPATIAL_FILTER,
	SPATIAL_INDEX,
	TABLEGEOMETRY,
	TABLESTYLES,
	VISUALSTYLE,
	WIPEOUTVARIABLE,

	ACDBDICTIONARYWDFLT,
	TABLESTYLE,
	EXACXREFPANELOBJECT,
	NPOCOLLECTION,
	ACDBSECTIONVIEWSTYLE,
	ACDBDETAILVIEWSTYLE,
	ACDB_BLKREFOBJECTCONTEXTDATA_CLASS,
	ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS,
};
// non-fixed types:
static const char *extra_types[] = {
	"ACAD_TABLE",
	"CELLSTYLEMAP",
	"DBCOLOR",
	"DICTIONARYVAR",
	"DICTIONARYWDFLT",
	"FIELD",
	"IDBUFFER",
	"IMAGEDEFREACTOR",
	"LAYER_INDEX",
	"LWPLINE",
	"MATERIAL",
	"MLEADER",
	"MLEADERSTYLE",
	"PLACEHOLDER",
	"PLOTSETTINGS",
	"RASTERVARIABLES",
	"SCALE",
	"SORTENTSTABLE",
	"SPATIAL_FILTER",
	"SPATIAL_INDEX",
	"TABLEGEOMETRY",
	"TABLESTYLES",
	"VISUALSTYLE",
	"WIPEOUTVARIABLE",

	"ACDBDICTIONARYWDFLT",
	"TABLESTYLE",
	"EXACXREFPANELOBJECT",
	"NPOCOLLECTION",
	"ACDBSECTIONVIEWSTYLE",
	"ACDBDETAILVIEWSTYLE",
	"ACDB_BLKREFOBJECTCONTEXTDATA_CLASS",
	"ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS"
};

enum LineWidth {
	width00			= 0,	// 0.00mm (dxf 0)
	width01			= 1,	// 0.05mm (dxf 5)
	width02			= 2,	// 0.09mm (dxf 9)
	width03			= 3,	// 0.13mm (dxf 13)
	width04			= 4,	// 0.15mm (dxf 15)
	width05			= 5,	// 0.18mm (dxf 18)
	width06			= 6,	// 0.20mm (dxf 20)
	width07			= 7,	// 0.25mm (dxf 25)
	width08			= 8,	// 0.30mm (dxf 30)
	width09			= 9,	// 0.35mm (dxf 35)
	width10			= 10,	// 0.40mm (dxf 40)
	width11			= 11,	// 0.50mm (dxf 50)
	width12			= 12,	// 0.53mm (dxf 53)
	width13			= 13,	// 0.60mm (dxf 60)
	width14			= 14,	// 0.70mm (dxf 70)
	width15			= 15,	// 0.80mm (dxf 80)
	width16			= 16,	// 0.90mm (dxf 90)
	width17			= 17,	// 1.00mm (dxf 100)
	width18			= 18,	// 1.06mm (dxf 106)
	width19			= 19,	// 1.20mm (dxf 120)
	width20			= 20,	// 1.40mm (dxf 140)
	width21			= 21,	// 1.58mm (dxf 158)
	width22			= 22,	// 2.00mm (dxf 200)
	width23			= 23,	// 2.11mm (dxf 211)
	widthByLayer	= 29,	// by layer (dxf -1)
	widthByBlock	= 30,	// by block (dxf -2)
	widthDefault	= 31	// by default (dxf -3)
};

int ToDXF(LineWidth lw) {
	static const int16 table[] = {
		0,		//width00
		5,		//width01
		9,		//width02
		13,		//width03
		15,		//width04
		18,		//width05
		20,		//width06
		25,		//width07
		30,		//width08
		35,		//width09
		40,		//width10
		50,		//width11
		53,		//width12
		60,		//width13
		70,		//width14
		80,		//width15
		90,		//width16
		100,	//width17
		106,	//width18
		120,	//width19
		140,	//width20
		158,	//width21
		200,	//width22
		211,	//width23
		-1,		//widthByLayer
		-2,		//widthByBlock
		-3,		//widthDefault
	};
	return table[lw];
}

LineWidth DXFtoLineWidth(int i) {
	return	i == -1	? widthByLayer
		:	i == -2	? widthByBlock
		:	i < 0	? widthDefault
		:	i < 3	? width00
		:	i < 7	? width01
		:	i < 11	? width02
		:	i < 14	? width03
		:	i < 16	? width04
		:	i < 19	? width05
		:	i < 22	? width06
		:	i < 27	? width07
		:	i < 32	? width08
		:	i < 37	? width09
		:	i < 45	? width10
		:	i < 52	? width11
		:	i < 57	? width12
		:	i < 65	? width13
		:	i < 75	? width14
		:	i < 85	? width15
		:	i < 95	? width16
		:	i < 103	? width17
		:	i < 112	? width18
		:	i < 130	? width19
		:	i < 149	? width20
		:	i < 180	? width21
		:	i < 205	? width22
		:	width23;
}

LineWidth DWGtoLineWidth(int i) {
	return between(i, 0, 23) || between(i, 29, 31) ? LineWidth(i) : widthDefault;
}

enum ShadowMode {
	CastAndReceieveShadows = 0,
	CastShadows = 1,
	ReceiveShadows = 2,
	IgnoreShadows = 3
};

enum MaterialCodes {
	MaterialByLayer = 0
};

enum PlotStyleCodes {
	DefaultPlotStyle = 0
};

enum TransparencyCodes {
	Opaque = 0,
	Transparent = -1
};

enum VAlign {
	VBaseLine = 0,			// Top = 0
	VBottom,				// Bottom = 1
	VMiddle,				// Middle = 2
	VTop					// Top = 3
};
enum HAlign {
	HLeft = 0,				// Left = 0
	HCenter,				// Centered = 1
	HRight,					// Right = 2
	HAligned,				// Aligned = 3 (if VAlign==0)
	HMiddle,				// middle = 4 (if VAlign==0)
	HFit					// fit into point = 5 (if VAlign==0)
};

struct bitsin : vlc_in<uint32, true> {
	typedef	vlc_in<uint32, true>	B;
	using B::get;
	VER		ver;

	uint16	get16() {
		auto	lo = B::get(8);
		return lo | (B::get(8) << 8);
	}
	uint32	get32() {
		auto	lo = get16();
		return lo | (get16() << 16);
	}
	template<typename T> T	get() { return B::get<T>(); }
	bitsin(istream_ref file, VER ver) : B(file), ver(ver) {}

	size_t	readbuff(void* buffer, size_t size) {
		for (uint8	*p = (uint8*)buffer, *e = p + size; p != e; ++p)
			*p = B::get(8);
		return size;
	}
	template<typename T> friend bool read(bitsin &in, T &t) { t = in; return true; }
};

template<> uint16 bitsin::get<uint16>() {
	auto	lo = get<uint8>();
	return lo | (get<uint8>() << 8);
}
template<> uint32 bitsin::get<uint32>() {
	auto	lo = get<uint16>();
	return lo | (get<uint16>() << 16);
}
template<> double bitsin::get<double>() {
	uint8	buffer[8];
	for (auto &i : buffer)
		i = get<uint8>();
	return *(double*)buffer;
}


template<typename T> struct raw {
	T	v;
	raw()			{}
	raw(T v)		: v(v) {}
	raw(bitsin& in) : v(in.get<T>()) {}
	operator T() const	{ return v; }
	friend T get(const raw &b)	{ return b; }
};

typedef raw<uint8>		RC;		// raw char (not compressed)
typedef raw<uint16>		RS;		// raw short (not compressed)
typedef raw<double>		RD;		// raw double (not compressed)
typedef raw<uint32>		RL;		// raw long (not compressed)
typedef raw<uint64>		RLL;	// raw long long (not compressed)
typedef array<RD, 2>	RD2;	// 2 raw doubles
typedef array<RD, 3>	RD3;	// 3 raw doubles

// bit (1 or 0)
struct B {
	bool	v;
	B(bool v = false)	: v(v) {}
	B(bitsin& in)		: v(in.get_bit()) {}
	operator bool() const { return v; }
	friend bool get(const B &b)	{ return b; }
};

// special 2 bit code (entmode in entities, for instance)
struct BB {
	uint8	v;
	BB(uint8 v = 0)	: v(v) {}
	BB(bitsin& in)	: v(in.get(2)) {}
	operator uint8() const { return v; }
	friend bool get(const BB &b)	{ return b; }
};

// bit triplet (1-3 bits) (R24)
struct B3 {
	uint8	v;
	B3(bitsin& in) {
		v	= !in.get_bit() ? 0
			: !in.get_bit() ? 1
			: !in.get_bit() ? 2 : 3;
	}
	operator uint8() const { return v; }
	friend uint8 get(const B3 &b)	{ return b; }
};
// bitshort (16 bits)
struct BS {
	uint16	v;
	BS(uint16 v = 0) : v(v) {}
	BS(bitsin& in) {
		switch (in.get(2)) {
			case 0: v = in.get16(); break;
			case 1: v = in.get(8); break;
			case 2: v = 0; break;
			case 3: v = 256; break;
		}
	}
	operator uint16() const { return v; }
	friend uint16 get(const BS &b)	{ return b; }
};
// bitlong (32 bits)
struct BL {
	uint32	v;
	BL(uint32 v = 0) : v(v) {}
	BL(bitsin& in) {
		switch (in.get(2)) {
			case 0: v = in.get32(); break;
			case 1: v = in.get(8); break;
			case 2: v = 0; break;
			case 3: v = 256; break;
		}
	}
	operator uint32() const { return v; }
	friend uint32 get(const BL &b)	{ return b; }
};
// bitlonglong (64 bits) (R24)
struct BLL {
	uint64	v;
	BLL(uint64 v = 0) : v(v) {}
	BLL(bitsin& in) : v(0) {
		auto	n = in.get(3);
		for (int i = 0; i < n; i++)
			v = (v << 8) |in.get(8);
	}
	operator uint64() const { return v; }
	friend uint64 get(const BLL &b)	{ return b; }
};
// bitdouble
struct BD {
	double	v;
	BD(double v = 0) : v(v) {}
	BD(bitsin& in) {
		switch (in.get(2)) {
			case 0: v = in.get<double>(); break;
			case 1: v = 1; break;
			case 2: v = 0; break;
			case 3: v = 256; break;
		}
	}
	operator double() const { return v; }
	friend double get(const BD &b)	{ return b; }
};

typedef array<BD,2> BD2;
typedef array<BD,3> BD3;

// BitDouble With Default
double DD(bitsin& in, double def) {
	double	v = def;
	auto	p = (uint8*)&v;
	switch (in.get(2)) {
		case 0: break;
		case 1:
			*(uint32*)p			= in.get32();
			break;
		case 2:
			*((uint16*)(p + 4)) = in.get16();
			*((uint32*)p)		= in.get32();
			break;
		case 3:
			v = in.get<double>();
			break;
	}
	return v;
}

// modular char
struct MC {
	uint64	v;
	MC(uint64 v = 0) : v(v) {}
	MC(bitsin& in) {
		uint64	r = 0;
		for (int i = 0; i < 64; i += 7) {
			uint8 c = in.get(8);
			r |= uint64(c & 0x7f) << i;
			if (!(c & 0x80))
				break;
		}
		v = r;
	}
	bool	read(istream_ref file) {
		uint64	r = 0;
		for (int i = 0; i < 64; i += 7) {
			uint8 c = file.getc();
			r |= uint64(c & 0x7f) << i;
			if (!(c & 0x80))
				break;
		}
		v = r;
		return true;
	}

	operator uint64() const { return v; }
	friend uint64 get(const MC &b)	{ return b; }
};

struct MCS {
	int64	v;
	MCS(uint64 v = 0) : v(v) {}
	MCS(bitsin& in) {
		int64	r = 0;
		for (int i = 0; i < 64; i += 7) {
			uint8 c = in.get(8);
			r |= uint64(c & 0x7f) << i;
			if (!(c & 0x80)) {
				if (c & 0x40)
					r = (0x40ull << i) - r;
				break;
			}
		}
		v = r;
	}
	bool	read(istream_ref file) {
		int64	r = 0;
		for (int i = 0; i < 64; i += 7) {
			uint8 c = file.getc();
			r |= uint64(c & 0x7f) << i;
			if (!(c & 0x80)) {
				if (c & 0x40)
					r = (0x40ull << i) - r;
				break;
			}
		}
		v = r;
		return true;
	}

	operator int64() const { return v; }
	friend int64 get(const MCS &b)	{ return b; }
};

// modular short
struct MS {
	uint64	v;
	MS(uint64 v = 0) : v(v) {}
	MS(bitsin& in) {
		uint64	r = 0;
		for (int i = 0; i < 64; i += 15) {
			uint16 c = in.get16();
			r |= uint64(c & 0x7fff) << i;
			if (!(c & 0x8000))
				break;
		}
		v = r;
	}
	bool	read(istream_ref file) {
		uint64	r = 0;
		for (int i = 0; i < 64; i += 15) {
			uint16 c = file.get<uint16>();
			r |= uint64(c & 0x7fff) << i;
			if (!(c & 0x8000))
				break;
		}
		v = r;
		return true;
	}
	operator uint64() const { return v; }
	friend uint64 get(const MS &b)	{ return b; }
};

// text (bitshort length, followed by the string).
struct T : string {
	T(string_param v = none) : string(v) {}
	T(bitsin& in) {
		BS	len(in);
		resize(len.v);
		for (int i = 0; i < len.v; i++)
			(*this)[i] = in.get<char>();
	}
};

// Unicode text (bitshort character length, followed by Unicode string, 2 bytes per character). Unicode text is read from the “string stream” within the object data, see the main Object description section for details.
struct TU : string16 {
	TU(string_param16 v = none) : string16(v) {}
	TU(bitsin& in) {
		BS	len(in);
		resize(len.v);
		for (int i = 0; i < len.v; i++)
			(*this)[i] = in.get<char16>();
	}
	friend string get(const TU &b)	{ return b; }
};

// Variable text, T for 2004 and earlier files, TU for 2007+ files.
struct TV : string16 {
	TV(string_param16 v = none) : string16(v) {}
	TV(bitsin& in) {
		BS	len(in);
		resize(len.v);
		for (int i = 0; i < len.v; i++)
			(*this)[i] = in.ver >= R2007 ? in.get<uint16>() : in.get<uint8>();
	}
	friend string get(const TV &b)	{ return b; }
};

struct X; // special form
struct U; // unknown

// 16 byte sentinel
struct SN {
	uint8	v[16];
	SN(bitsin& in) {
		for (auto &i : v)
			i = in.get<uint8>();
	}
};

// BitExtrusion
struct BEXT : BD3 {
	BEXT()	{}
	BEXT(bitsin& in) : BD3(in.ver >= R2000 && in.get_bit() ? BD3(0, 0, 1) : in) {}
};

// BitScale
struct BSCALE {
	BD3		scale	= {1,1,1};
	BSCALE() {}
	BSCALE(bitsin &bits) {
		if (bits.ver < 1015) {//14-
			scale = bits;
		} else {
			switch (BB(bits)) {
				case 0:
					scale[0] = (double)RD(bits);
					// fallthrough
				case 1: //x default value 1, y & z can be x value
					scale[1] = DD(bits, scale[0]);
					scale[2] = DD(bits, scale[0]);
					break;
				case 2:
					scale[0] = scale[1] = scale[2] = (double)RD(bits);
					break;
				case 3:
					//none default value 1,1,1
					break;
			}
		}
	}
};


// BitThickness
struct BT : BD {
	BT()	{}
	BT(bitsin& in) : BD(in.ver >= R2000 && in.get_bit() ? BD(0) : in) {}
};

// Object type
struct OT {
	OBJECTTYPE	type;
	OT(OBJECTTYPE type = UNUSED) : type(type) {}
	OT(bitsin& in) {
		if (in.ver < R2007) {
			type = (OBJECTTYPE)in.get<uint16>();
		} else {
			switch (in.get(2)) {
				case 0:		type = (OBJECTTYPE)in.get<uint8>(); break;
				case 1:		type = (OBJECTTYPE)(in.get<uint8>() + 0x1f0); break;
				default:	type = (OBJECTTYPE)in.get<uint16>(); break;
			}
		}
	}
	operator OBJECTTYPE() const { return type; }
};

// Handle

struct H {
	enum CODE : uint8 {
		SoftOwnerRef	= 2,
		HardOwnerRef	= 3,
		SoftPointerRef	= 4,
		HardPointerRef	= 5,
		AddOne			= 6,
		SubOne			= 8,
		AddOffset		= 10,
		SubOffset		= 12,
	};
	union {
		uint32	u;
		struct { uint32 size:4, code:4, offset:24; };
		struct { uint8 code_size, bytes[3]; };
	};

	H(uint32 u = 0)	: u(u) {}
	H(bitsin& in)	: u(0) {
		code_size = in.get<uint8>();
		for (int i = 0; i < size; i++)
			offset = (offset << 8) | in.get<uint8>();
	}
	operator uint32() const { return offset; }
	friend uint32 get(const H &b)	{ return b; }

	uint32	get_offset(uint32 href) const {
		switch (code) {
			case AddOne:	return href + 1;
			case SubOne:	return href - 1;
			case AddOffset: return href + offset;
			case SubOffset: return href - offset;
			default:		return offset;
		}
	}
};

struct HandleRange {
	uint32					firstEH, lastEH; //pre 2004
	dynamic_array<uint32>	handles;

	bool	read(bitsin& bits, uint32 count) {
		if (bits.ver > 1015) {//2004+
			for (uint32 i = 0; i < count; i++)
				handles.push_back(H(bits));

		} else {//2000-
			if (count) {
				firstEH	= H(bits);
				lastEH	= H(bits);
			}
		}
		return true;
	}
};

struct CMC {
	enum TYPE {
		ByLayer = 0xC0,
		ByBlock	= 0xC1,
		RGB		= 0xC2,
		ACIS	= 0xC3,
	};
	struct Name : TV {
		uint8		name_type = 0;
		Name() {}
		Name(bitsin& in) {
			if (in.ver >= R2000) {
				if (name_type = in.get(8))
					TV::operator=(in);
			}
		}
	};

	BS		index;
	BL		rgb;
	Name	name;

	CMC()	{}
	CMC(bitsin& in) : index(in), rgb(in.ver >= R2000 ? in : BL(0)), name(in) {}
};

struct TC; // True Color: this is the same format as CMC in R2004+.

struct ENC {
	enum FLAGS {
		Complex			= 0x8000,
		AcDbRef			= 0x4000,
		Transparency	= 0x2000,
	};
	BS	flags;
	BL	rgb;
	BL	transparency;
	H	h;
	ENC() : flags(0) {}
	ENC(bitsin& in) : flags(in) {
		if (flags.v & Complex) {
			rgb = in;
			if (flags.v & AcDbRef)
				h = in;
		}
		if (flags.v & Transparency) {
			transparency = in;
		}
	}

};

struct RenderMode {
	RC		mode;
	B		use_default_lights;
	RC		default_lighting_type;
	BD		brightness;
	BD		contrast;
	CMC		ambient;
	bool	read(bitsin &bits) {
		mode	= bits;
		return bits.ver <= 1018 || iso::read(bits, use_default_lights, default_lighting_type, brightness, contrast, ambient);
	}
};

struct UserCoords {
	BD3		origin, xdir, ydir;
	BD		elevation;
	BS		ortho_view_type;
	bool read(bitsin &bits) {
		origin	= bits;
		xdir	= bits;
		ydir	= bits;

		if (bits.ver >= R2000) {
			elevation	= bits;
			ortho_view_type	= bits;
		}
		return true;
	}
};

struct Gradient {
	struct Entry {
		BD unkDouble;
		BS unkShort;
		BL rgbCol;
		RC ignCol;
		Entry(bitsin &bits) : unkDouble(bits), unkShort(bits), rgbCol(bits), ignCol(bits) {}
	};
	BL isGradient;
	BL res;
	BD gradAngle;
	BD gradShift;
	BL singleCol;
	BD gradTint;
	dynamic_array<Entry>	entries;

	bool	read(bitsin &bits) {
		iso::read(bits, isGradient, res, gradAngle, gradShift, singleCol, gradTint);
		entries = repeat(bits, BL(bits));
		return true;
	}
};

struct DimStyleParams {
	enum DIMFLAGS {
		DIMTOL			= 1 << 0,
		DIMLIM			= 1 << 1,
		DIMTIH			= 1 << 2,
		DIMTOH			= 1 << 3,
		DIMSE1			= 1 << 4,
		DIMSE2			= 1 << 5,
		DIMALT			= 1 << 6,
		DIMTOFL			= 1 << 7,
		DIMSAH			= 1 << 8,
		DIMTIX			= 1 << 9,
		DIMSOXD			= 1 << 10,
		DIMSD1			= 1 << 11,
		DIMSD2			= 1 << 12,
		DIMUPT			= 1 << 13,
		DIMFXLON		= 1 << 14,
		DIMTXTDIRECTION	= 1 << 15,
	};
	uint32	dim_flags;
	T	DIMPOST;
	T	DIMAPOST;
	T	DIMBLK;
	T	DIMBLK1;
	T	DIMBLK2;
	T	DIMALTMZS;
	T	DIMMZS;

	RC	DIMALTD;
	RC	DIMZIN;
	RC	DIMTOLJ;
	RC	DIMJUST;
	RC	DIMFIT;
	RC	DIMTZIN;
	RC	DIMALTZ;
	RC	DIMALTTZ;
	RC	DIMTAD;
	BS	DIMUNIT;
	BS	DIMAUNIT;
	BS	DIMDEC;
	BS	DIMTDEC;
	BS	DIMALTU;
	BS	DIMALTTD;
	BD	DIMSCALE;
	BD	DIMASZ;
	BD	DIMEXO;
	BD	DIMDLI;
	BD	DIMEXE;
	BD	DIMRND;
	BD	DIMDLE;
	BD	DIMTP;
	BD	DIMTM;
	BD	DIMTXT;
	BD	DIMCEN;
	BD	DIMTSZ;
	BD	DIMALTF;
	BD	DIMLFAC;
	BD	DIMTVP;
	BD	DIMTFAC;
	BD	DIMGAP;
	BS	DIMCLRD;
	BS	DIMCLRE;
	BS	DIMCLRT;
	BD	DIMFXL;
	BD	DIMJOGANG;
	BS	DIMTFILL;
	CMC	DIMTFILLCLR;
	BS	DIMAZIN;
	BS	DIMARCSYM;
	BD	DIMALTRND;
	BS	DIMADEC;
	BS	DIMFRAC;
	BS	DIMLUNIT;
	BS	DIMDSEP;
	BS	DIMTMOVE;
	BD	DIMALTMZF;
	BS	DIMLWD;
	BS	DIMLWE;
	BD	DIMMZF;

	H	DIMTXSTY;
	H	DIMLDRBLK;
	H	HDIMBLK;
	H	HDIMBLK1;
	H	HDIMBLK2;
	H	DIMLTYPE;
	H	DIMLTEX1;
	H	DIMLTEX2;

	bool parse(bitsin &bits, bitsin &sbits, bitsin &hbits) {
		//	R13 & R14 Only:
		if (bits.ver <= R14) {
			dim_flags = bits.get(11);
			read(bits, DIMALTD, DIMZIN);
			dim_flags |= bits.get(2) * DIMSD1;
			read(bits, DIMTOLJ, DIMJUST, DIMFIT);
			dim_flags |= bits.get(1) * DIMUPT;
			read(bits, DIMTZIN, DIMALTZ,DIMALTTZ, DIMTAD, DIMUNIT, DIMAUNIT, DIMDEC, DIMTDEC, DIMALTU, DIMALTTD);
		}  else {
			DIMPOST		= sbits;
			DIMAPOST	= sbits;
		}

		read(bits, DIMSCALE, DIMASZ, DIMEXO, DIMDLI, DIMEXE, DIMRND, DIMDLE, DIMTP, DIMTM);


		if (bits.ver >= R2007) {
			read(bits, DIMFXL, DIMJOGANG, DIMTFILL, DIMTFILLCLR);
		}
		if (bits.ver >= R2000) {
			dim_flags = bits.get(6);
			read(bits, DIMTAD, DIMZIN, DIMAZIN);
		}
		if (bits.ver >= R2007)
			DIMARCSYM = bits;

		read(bits, DIMTXT, DIMCEN, DIMTSZ, DIMALTF, DIMLFAC, DIMTVP, DIMTFAC, DIMGAP);

		if (bits.ver <= R14) {
			read(sbits, DIMPOST, DIMAPOST, DIMBLK, DIMBLK1, DIMBLK2);
		} else {
			read(bits, DIMALTRND);
			dim_flags |= bits.get_bit() * DIMALT;
			DIMALTD = bits;
			dim_flags |= bits.get(4) * DIMTOFL;
		}
		read(bits, DIMCLRD, DIMCLRE, DIMCLRT);
		if (bits.ver >= R2000) {
			read(bits, DIMADEC, DIMDEC, DIMTDEC, DIMALTU, DIMALTTD, DIMAUNIT, DIMFRAC, DIMLUNIT, DIMDSEP, DIMTMOVE, DIMJUST);
			dim_flags |= bits.get(2) * DIMSD1;
			read(bits, DIMTOLJ, DIMTZIN, DIMALTZ, DIMALTTZ);
			dim_flags |= bits.get_bit() * DIMUPT;
			DIMFIT = bits;
		}
		if (bits.ver >= R2007)
			dim_flags |= bits.get_bit() * DIMFXLON;

		if (bits.ver >= R2010) {
			dim_flags |= bits.get_bit() * DIMTXTDIRECTION;
			DIMALTMZF	= bits;
			read(sbits, DIMALTMZS, DIMMZS, DIMMZF);
		}

		//handles
		if (bits.ver >= R2000)
			read(hbits, DIMTXSTY, DIMLDRBLK, HDIMBLK, HDIMBLK1, HDIMBLK2);
		if (bits.ver >= R2007)
			read(hbits, DIMLTYPE, DIMLTEX1, DIMLTEX2);

		if (bits.ver >= R2000)
			read(bits, DIMLWD, DIMLWE);

		return true;
	}
};

template<typename T> auto global_get(const T &t) { return get(t); }
typedef variant<string, bool, int32, uint32, double, array<double,2>, array<double,3>, CMC, H, malloc_block> _variant;
struct Variant : _variant {
	template<typename T> Variant(const T &t) : _variant(global_get(t)) {}
};

typedef double3	coord;

struct bit_seeker {
	bitsin& bits;
	streamptr	end;
	bit_seeker(bitsin& bits, uint32 size) : bits(bits), end(bits.tell_bit() + size) {}
	~bit_seeker() { bits.seek_bit(end); }
};

Variant read_extended(bitsin& bits) {
//	H		ah(bits);
	RC		dxfCode(bits);
	switch (dxfCode + 1000) {
		case DXF_STRING: {
			if (bits.ver <= R2004) {
				RC			len(bits);
				RS			cp(bits);
				string		s(len);
				for (int i = 0; i < len + 1; i++)
					s[i] = RC(bits);
				return s;

			} else {
				RS			len(bits);
				string16	s(len);
				for (int i = 0; i < len; i++)
					s[i] = RS(bits);
				return (string)s;
			}
		}

		case DXF_BRACKET:
			return RC(bits);

		case DXF_LAYER_REF:
		case DXF_ENTITY_REF: {
			uint32	v = 0;
			for (int i = 0; i < 8; i++)
				v = (v << 4) | from_digit(RC(bits));
			return H(v);
		}
		case DXF_BINARY: {
			RC	len(bits);
			malloc_block	data(len);
			for (int i = 0; i < len; i++)
				((RC*)data)[i] = bits;
			return data;
		}
		case DXF_POINTS: case DXF_POINTS + 1: case DXF_POINTS + 2: case DXF_POINTS + 3:
			return RD3(bits);

		case DXF_REALS: case DXF_REALS + 1: case DXF_REALS + 2: 
			return RD(bits);

		case DXF_SHORT:
			return RS(bits);

		case DXF_LONG:
			return RL(bits);

		case DXF_INVALID:
		default:
			return none;
	}
}

uint32 get_string_offset(bitsin& bits, uint32 bsize) {
	uint32	offset = 0;
	if (bits.ver > 1018) {
		bits.seek_bit(bsize - 1);
		if (B(bits)) {
			bits.seek_bit(bsize - 17);
			uint32	ssize	= RS(bits);
			if (ssize & 0x8000) {
				bits.seek_bit(bsize - 33);
				ssize = ((ssize & 0x7fff) + (RS(bits)<<15)) + 16;
			}
			offset = bsize - ssize - 17;
		}
	}
	bits.seek_bit(0);
	return offset;
}

// for entities or tableentries
template<typename T> bool parse(T *t, istream_ref file, VER version, uint32 size, uint32 bsize) {
	temp_block		data(file, size);
	memory_reader	mr(data);
	bitsin			bits(mr, version);

	if (auto soffset = get_string_offset(bits, bsize)) {
		memory_reader	mrs(data);
		bitsin			sbits(mrs, version);
		sbits.seek_bit(soffset);
		return t->parse(bits, sbits, bsize);
	}
	return t->parse(bits, bits, bsize);
}

//-----------------------------------------------------------------------------
//	entities
//-----------------------------------------------------------------------------

struct Entity {
	enum FLAGS {
		visible			= 1 << 0,
		haveExtrusion	= 1 << 1,
		haveNextLinks	= 1 << 2,
		ownerHandle		= 1 << 3,
		paperSpace		= 1 << 4,
		no_xdict		= 1 << 16,
	};
	enum LINEFLAGS : uint8 {
		BYLAYER		= 0,
		CONTINUOUS	= 1,
		BYBLOCK		= 2,
		HANDLE		= 3,
	};

	OT			type;
	uint32		obj_size		= 0;
	uint32		flags			= 0;
	uint32		handle			= 0;
	uint32		parentH			= 0;

	dynamic_array<H>			reactors;
	hash_map<uint32, Variant>	extended;

	LineWidth	lWeight			= widthByLayer;
	BD			linetypeScale	= 1.0;
	ENC			color;

	LINEFLAGS	plotFlags		= BYLAYER;
	LINEFLAGS	lineFlags		= BYLAYER;
	LINEFLAGS	materialFlag	= BYLAYER;
	RC			shadowFlag;

	//handles
	uint32		linetypeH;
	uint32		plotstyleH;
	uint32		materialH;
	uint32		shadowH;
	uint32		layerH;
	uint32		nextEntLink = 0;
	uint32		prevEntLink = 0;

	malloc_block	graphics_data;

	virtual ~Entity() {}

protected:
	bool parse(bitsin& bits, uint32 bsize);
	bool parse_handles(bitsin &bits);
};


bool Entity::parse(bitsin& bits, uint32 bsize) {
	type		= bits;
	obj_size	= bits.ver > 1014 && bits.ver < 1024 ? bits.get<uint32>() : bsize;
	handle		= H(bits);

	while (uint32 xsize = BS(bits)) {
		H		ah(bits);
		bit_seeker(bits, xsize * 8), (extended[ah] = read_extended(bits));
	}

	if (B(bits))
		graphics_data.read(bits, RL(bits));

	if (bits.ver < 1015)
		obj_size = bits.get<uint32>();

	BB	entmode(bits);
	flags |= ((entmode == 0) * ownerHandle) | ((entmode & 1) * paperSpace);

	reactors.resize(BS(bits));

	if (bits.ver < 1015)
		lineFlags = B(bits) ? BYLAYER : HANDLE;

	if (bits.ver > 1015)
		flags |= B(bits) * no_xdict;

	flags |= ((bits.ver < 1024 && bits.ver > 1018) || B(bits)) * haveNextLinks;

	color			= bits;
	linetypeScale	= bits;
	if (bits.ver > 1014) {
		lineFlags	= (LINEFLAGS)get(BB(bits));
		plotFlags	= (LINEFLAGS)get(BB(bits));
	}
	if (bits.ver > 1018) {
		materialFlag	= (LINEFLAGS)get(BB(bits));
		shadowFlag		= bits;
	}
	if (bits.ver > 1021) {
		BB	visualFlags(bits);
		B	edge(bits); //edge visual style
	}
	BS	invisibleFlag(bits);
	if (bits.ver > 1014)
		lWeight = DWGtoLineWidth(RC(bits));

	return true;
}

bool Entity::parse_handles(bitsin &bits) {
	if (bits.ver > 1018)		// skip string area
		bits.seek_bit(obj_size);

	if (flags & ownerHandle)	//entity is in block or polyline
		parentH = H(bits).get_offset(handle);

	for (auto &h : reactors)
		h = bits;

	if (!(flags & no_xdict))
		H XDicObjH(bits);

	if (bits.ver < 1015) {
		layerH = H(bits);
		if (lineFlags == HANDLE)
			linetypeH = H(bits);
	}
	if (bits.ver < 1018) {
		if (flags & haveNextLinks) {
			nextEntLink = handle + 1;
			prevEntLink = handle - 1;
		} else {
			prevEntLink = H(bits).get_offset(handle);
			nextEntLink = H(bits).get_offset(handle);
		}
	}
	if (bits.ver > 1015) {
		//Parses Bookcolor handle
	}

	if (bits.ver > 1014) {
		layerH = H(bits).get_offset(handle);
		if (lineFlags == HANDLE)
			linetypeH = H(bits).get_offset(handle);
	
		if (bits.ver > 1018) {//2007+
			if (materialFlag == HANDLE)
				materialH = H(bits).get_offset(handle);
			if (shadowFlag == HANDLE)
				shadowH = H(bits).get_offset(handle);
		}
		if (plotFlags == HANDLE)
			plotstyleH = H(bits).get_offset(handle);
	}
	return true;
}

struct Point : Entity {
public:
	BD3		point;			// base point, code 10, 20 & 30 */
	BT		thickness;		// thickness, code 39 */
	BEXT	extPoint;		// Dir extrusion normal vector, code 210, 220 & 230 */
	BD		x_axis;			// Angle of the X axis for the UCS in effect when the point was drawn

	bool parse(bitsin& bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		point		= bits;
		thickness	= bits;
		extPoint	= bits;
		x_axis		= bits;

		return Entity::parse_handles(bits);
	}
};

struct Line : Entity {
	BD3		point1;
	BD3		point2;
	BT		thickness;
	BEXT	extPoint;

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		if (bits.ver < 1015) {
			point1	= bits;
			point2	= bits;
		}
		if (bits.ver > 1014) {
			B zIsZero(bits);
			point1[0]	= (double)RD(bits);
			point2[0]	= DD(bits, point1[0]);
			point1[1]	= (double)RD(bits);
			point2[1]	= DD(bits, point1[1]);//DD
			if (!zIsZero) {
				point1[2] = (double)RD(bits);
				point2[2] = DD(bits, point1[2]);
			}
		}
		thickness	= bits;
		extPoint	= bits;
		return Entity::parse_handles(bits);
	}
};

struct Ray : Entity {
	BD3		point1;
	BD3		point2;

	bool parse(bitsin &bits, uint32 bsize) {
		bool ret = Entity::parse(bits, bsize);
		if (!ret)
			return ret;
		point1	= bits;
		point2	= bits;
		return Entity::parse_handles(bits);
	}
};

struct Xline : Ray {};

struct Circle : Entity {
	BD3		centre;
	BD		radius;
	BT		thickness;
	BEXT	extPoint;

	bool parse(bitsin &bits, uint32 bsize) {
		return Entity::parse(bits, bsize)
			&&	read(bits, centre, radius, thickness, extPoint)
			&&	Entity::parse_handles(bits);
	}
};

struct Arc : Circle {
	BD		staangle;	// start angle, code 50 in radians
	BD		endangle;	// end angle, code 51 in radians 
	bool	isccw;		// is counter clockwise arc?, only used in hatch, code 73 

	bool parse(bitsin &bits, uint32 bsize) {
		return Circle::parse(bits, bsize)
			&&	read(bits, staangle, endangle)
			&&	Entity::parse_handles(bits);
	}
};

struct Ellipse : Entity {
	BD3		point1;
	BD3		point2;
	BD3		extPoint;
	BD		ratio;
	BD		staparam;	// start parameter, code 41, 0.0 for full ellips
	BD		endparam;	// end parameter, code 42, 2*PI for full ellipse
	bool	isccw;		// is counter clockwise arc?, only used in hatch, code 73

	bool parse(bitsin &bits, uint32 bsize) {
		return  Entity::parse(bits, bsize)
			&&	read(bits, point1, point2, extPoint, ratio, staparam, endparam)
			&&	Entity::parse_handles(bits);
	}
};

struct Trace : Entity	{
	BT		thickness;
	BD3		point1;
	BD2		point2;
	BD2		point3;
	BD2		point4;
	BEXT	extPoint;

	bool parse(bitsin &bits, uint32 bsize) {
		return Entity::parse(bits, bsize)
			&&	read(bits, thickness, point1, point2, point3, point4, extPoint)
			&&	Entity::parse_handles(bits);
	}
};

struct Solid : Trace {};

struct Face3D : Trace {
	BD3		point1;
	BD3		point2;
	BD3		point3;
	BD3		point4;
	BS		invisibleflag = 0;	// bit per edge

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		if (bits.ver < 1015 ) {// R13 & R14
			if (!read(bits, point1, point2, point3, point4, invisibleflag))
				return false;

		} else { // 2000+
			B has_no_flag(bits);
			B z_is_zero(bits);
			point1[0]	= (double)RD(bits);
			point1[1]	= (double)RD(bits);
			point1[2]	= z_is_zero ? 0.0 : (double)RD(bits);
			point2[0]	= DD(bits, point1[0]);
			point2[1]	= DD(bits, point1[1]);
			point2[2]	= DD(bits, point1[2]);
			point3[0]	= DD(bits, point2[0]);
			point3[1]	= DD(bits, point2[1]);
			point3[2]	= DD(bits, point2[2]);
			point4[0]	= DD(bits, point3[0]);
			point4[1]	= DD(bits, point3[1]);
			point4[2]	= DD(bits, point3[2]);
			if (!has_no_flag)
				invisibleflag = bits;
		}

		return Entity::parse_handles(bits);
	}
};

struct Block : Entity {
	TV		name	= string_param16(L"*U0");	// block name, code 2
	BD3		basePoint;
	dynamic_array<Entity*>	entities;

	bool parse(bitsin& bits, bitsin &sbits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		name = sbits;
		if (bits.ver > 1018)
			(void)B(bits);

		return Entity::parse_handles(bits);
	}

	friend tag2 _GetName(const Block &block)	{ return block.name; }

};

struct BlockEnd : Entity {
	bool parse(bitsin& bits, bitsin &sbits, uint32 bsize, bool isEnd = false) {
		if (!Entity::parse(bits, bsize))
			return false;

		if (bits.ver > 1018)
			(void)B(bits);

		return Entity::parse_handles(bits);
	}
};

struct Insert : Entity {
	BD3		basePoint;
	BEXT	extPoint;
	BSCALE	scale;
	BD		angle;			// rotation angle in radians, code 50

	H		blockRecH;		//block_map.findname(e->blockRecH)
	dynamic_array<H>	att;
	H		seqendH;		//RLZ: on implement attrib remove this handle from obj list (see pline/vertex code)

	bool parse_handles(bitsin &bits, uint32 objCount) {
		if (!Entity::parse_handles(bits))
			return false;

		blockRecH	= bits;
		if (objCount) {
			att =	 repeat(bits, objCount);
			seqendH = bits;
		}
		return true;
	}

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		return read(bits, basePoint, scale, angle, extPoint)
			&& parse_handles(bits, !B(bits) ? 0 : bits.ver > 1015 ? get(BL(bits)) : 2);
	}
};

struct MInsert : Insert {
	BS colcount;
	BS rowcount;
	BD colspace;
	BD rowspace;

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize) || !read(bits, basePoint, scale, angle, extPoint))
			return false;

		int32	objCount = !B(bits) ? 0 : bits.ver > 1015 ? get(BL(bits)) : 2;
		return read(bits, colcount, rowcount, colspace, rowspace)
			&& parse_handles(bits, objCount);
	}
};

struct LWPolyline : Entity {
	BS		flags;			// polyline flag, code 70, default 0
	BD		width;			// constant width, code 43
	BD		elevation;		// elevation, code 38
	BD		thickness;		// thickness, code 39
	BEXT	extPoint;		// Dir extrusion normal vector, code 210, 220 & 230

	struct Vertex {
		double	x;			// x coordinate, code 10
		double	y;			// y coordinate, code 20
		double	stawidth;	// Start width, code 40
		double	endwidth;	// End width, code 41
		double	bulge;		// bulge, code 42
		int		id;
		Vertex(): x(0), y(0), stawidth(0), endwidth(0), bulge(0), id(0) {}
	};

	dynamic_array<Vertex>	vertlist;

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		flags = bits;
		if (flags & 4)
			width		= bits;
		if (flags & 8)
			elevation	= bits;
		if (flags & 2)
			thickness	= bits;
		if (flags & 1)
			extPoint	= bits;

		BL	vertexnum(bits);
		vertlist.resize(vertexnum);

		uint32	bulge_count		= flags & 16						? get(BL(bits)) : 0;
		uint32	id_count		= bits.ver > 1021 && (flags & 1024)	? get(BL(bits)) : 0;
		uint32	widths_count	= flags & 32						? get(BL(bits)) : 0;

		//clear all bit except 128 = plinegen and set 1 to open/close //RLZ:verify plinegen & open dxf: plinegen 128 & open 1
		flags = (flags & 0x80) | ((flags >> 9) & 1);
		double	px = 0, py = 0;
		for (int i = 0; i < vertexnum; i++) {
			auto	&vertex = vertlist[i];
			if (bits.ver < 1015) {//14-
				vertex.x = RD(bits);
				vertex.y = RD(bits);
			} else {
				vertex.x = DD(bits, px);
				vertex.y = DD(bits, py);
				px = vertex.x;
				py = vertex.y;
			}
		}

		//add bulges
		for (int i = 0; i < bulge_count; i++) {
			BD bulge(bits);
			if (i < vertexnum)
				vertlist[i].bulge = bulge;
		}
		//add vertexId
		for (int i = 0; i < id_count; i++) {
			BL id(bits);
			if (i < vertexnum)
				vertlist[i].id = id;
		}
		//add widths
		for (int i = 0; i < widths_count; i++) {
			BD staW(bits), endW(bits);
			if (i < vertexnum) {
				vertlist[i].stawidth = staW;
				vertlist[i].endwidth = endW;
			}
		}
		return Entity::parse_handles(bits);
	}
};


struct Text : Entity {
	RD2		point1, point2;
	double	z;
	BEXT	extPoint;		// Dir extrusion normal vector, code 210, 220 & 230
	BT		thickness;		// thickness, code 39 *
	BD		height;			// height text, code 40
	TV		text;			// text string, code 1
	BD		angle;			// rotation angle in degrees (360), code 50
	BD		widthscale;		// width factor, code 41
	BD		oblique;		// oblique angle, code 51
	int		textgen;		// text generation, code 71
	HAlign	alignH;			// horizontal align, code 72
	VAlign	alignV;			// vertical align, code 73
	H		styleH;			// e->style = textstyle_map.findname(e->styleH);

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		// DataFlags RC Used to determine presence of subsequent data, set to 0xFF for R14-
		uint8 data_flags = 0;
		if (bits.ver > 1014) {//2000+
			data_flags = RC(bits);
			if (!(data_flags & 1))
				z = RD(bits);
		} else {//14-
			z = BD(bits);
		}

		point1		= bits;

		if (bits.ver > 1014) {
			if (!(data_flags & 2) ) { // Alignment pt 2DD 11 present if !(DataFlags & 0x02), use 10 & 20 values for 2 default values
				point2[0] = DD(bits, point1[0]);
				point2[1] = DD(bits, point1[1]);
			}
		} else {
			point2 = bits;
		}

		extPoint	= bits;
		thickness	= bits;

		if (bits.ver > 1014) {//2000+
			if (!(data_flags & 4))
				oblique = bits;
			if (!(data_flags & 8))
				angle = bits;
			height = bits; /* Height RD 40 */
			if (!(data_flags & 16))
				widthscale = bits;
		} else {//14-
			oblique		= bits;
			angle		= bits;
			height		= bits;
			widthscale	= bits;
		}

		text = sbits;
		if (!(data_flags & 0x20))
			textgen = BS(bits);
		if (!(data_flags & 0x40))
			alignH = (HAlign)get(BS(bits));
		if (!(data_flags & 0x80))
			alignV = (VAlign)get(BS(bits));

		if (!Entity::parse_handles(bits))
			return false;

		styleH = bits;
		return true;
	}
};

struct MText : Entity {
	enum Attach {
		TopLeft = 1,
		TopCenter,
		TopRight,
		MiddleLeft,
		MiddleCenter,
		MiddleRight,
		BottomLeft,
		BottomCenter,
		BottomRight
	};
	BD3		point1, point2;
	BD3		extPoint;		// Dir extrusion normal vector, code 210, 220 & 230
	BT		thickness;		// thickness, code 39 */
	BD		height;			// height text, code 40
	TV		text;			// text string, code 1
	BD		widthscale;		// width factor, code 41
	BD		oblique;		// oblique angle, code 51
	string	style;			// style name, code 7
	BS		textgen;		// text generation, code 71
	HAlign	alignH;			// horizontal align, code 72
	VAlign	alignV;			// vertical align, code 73
	H		styleH;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		point1		= bits;
		extPoint	= bits;
		point2		= bits;
		widthscale	= bits;

		if (bits.ver > 1018)
			(void)BD(bits);		// Rect height BD 46 Reference rectangle height

		height		= bits;		// Text height BD 40 Undocumented
		textgen		= bits;		// Attachment BS 71 Similar to justification;
		
		BS	draw_dir(bits);		// Drawing dir BS 72 Left to right, etc.; see DXF doc
		BD	ext_ht(bits);
		BD	ext_wid(bits);

		text = sbits;

		if (bits.ver > 1014) {//2000+
			BS	LinespacingStyle(bits);
			BD	LinespacingFactor(bits);
			B	unknown(bits);
		}
		if (bits.ver > 1015) {//2004+
			// Background flags BL 0 = no background, 1 = background fill, 2 =background fill with drawing fill color
			BL bk_flags(bits);
			if (bk_flags == 1) {
				BL	unk1(bits);
				CMC	col(bits);
				BL	unk2(bits);
			}
		}

		if (!Entity::parse_handles(bits))
			return false;

		styleH = bits;
		return true;
	}
};

struct PFaceFace : Entity {
	array<BS,4>	index;		// polyface mesh vertex indices

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		index = bits;
		return Entity::parse_handles(bits);
	}
};

struct Vertex2D : Entity {
	RC		flags;			// vertex flag, code 70, default 0
	BD3		point;
	BD		stawidth;		// Start width, code 40
	BD		endwidth;		// End width, code 41
	BD		bulge;			// bulge, code 42
	BL		id;
	BD		tgdir;			// curve fit tangent direction, code 50

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		flags		= bits;
		point		= bits;
		stawidth	= bits;
		if (stawidth < 0)
			endwidth = stawidth = abs(stawidth);
		else
			endwidth = bits;
		bulge = bits;
		if (bits.ver > 1021) //2010+
			id	= bits;
		tgdir = bits;

		return Entity::parse_handles(bits);
	}
};

// VERTEX_3D, VERTEX_MESH, VERTEX_PFACE
struct Vertex : Entity {
	RC		flags;
	BD3		point;

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		flags	= bits; //RLZ: EC unknown type
		point	= bits;
		return Entity::parse_handles(bits);
	}
};

struct Polyline : Entity {
	HandleRange				handles;
	H						seqEndH;
	dynamic_array<Entity*>	vertices;

	bool parse_handles(bitsin &bits) {
		int32 count = bits.ver > 1015 ? get(BL(bits)) : 1;

		if (!Entity::parse_handles(bits))
			return false;

		handles.read(bits, count);
		seqEndH = bits;
		return true;
	}
};

struct Polyline2D : Polyline {
	BS		flags;
	BS		curvetype;		// curves & smooth surface type, code 75, default 0
	BD		defstawidth;	// Start width, code 40, default 0
	BD		defendwidth;	// End width, code 41, default 0
	BT		thickness;
	BD		z;
	BEXT	extPoint;

	bool parse(bitsin &bits, uint32 bsize) {
		return Entity::parse(bits, bsize)
			&&	read(bits, flags, curvetype, defstawidth, defendwidth, thickness, z, extPoint)
			&&	parse_handles(bits);
	}
};

struct Polyline3D : Polyline {
	uint16	flags;
	uint8	curvetype;		// curves & smooth surface type, code 75, default 0

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		uint8 tmpFlag = RC(bits);
		if (tmpFlag & 1)
			curvetype = 5;
		else if (tmpFlag & 2)
			curvetype = 6;
		if (tmpFlag & 3) {
			curvetype = 8;
			flags |= 4;
		}
		tmpFlag = RC(bits);
		if (tmpFlag & 1)
			flags |= 1;
		flags |= 8; //indicate 3DPOL

		return parse_handles(bits);
	}
};

struct PolylinePFace : Polyline {
	BS		vertexcount;		// polygon mesh M vertex or polyface vertex num, code 71, default 0
	BS		facecount;			// polygon mesh N vertex or polyface face num, code 72, default 0

	bool parse(bitsin &bits, uint32 bsize) {
		return Entity::parse(bits, bsize)
			&&	read(bits, vertexcount, facecount)
			&&	parse_handles(bits);
	}
};

struct Spline : Entity {
	enum FLAGS {
		closed		= 1 << 0,
		periodic	= 1 << 1,
		rational	= 1 << 2,
	};
	BD3		tgStart;
	BD3		tgEnd;
	int		flags;		// spline flag, code 70
	BL		degree;		// degree of the spline, code 71
	BD		tolknot;	// knot tolerance, code 42, default 0.0000001
	BD		tolcontrol;	// control point tolerance, code 43, default 0.0000001
	BD		tolfit;		// fit point tolerance, code 44, default 0.0000001

	dynamic_array<BD>	knotslist;			// knots list, code 40
	dynamic_array<BD>	weightlist;			// weight list, code 41
	dynamic_array<BD3>	controllist;		// control points list, code 10, 20 & 30
	dynamic_array<BD3>	fitlist;			// fit points list, code 11, 21 & 31

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		BL scenario(bits);
		if (bits.ver > 1024) {
			BL splFlag1(bits);
			if (splFlag1 & 1)
				scenario = 2;
			BL knotParam(bits);
		}
		degree = bits;

		BL		nknots;		// number of knots, code 72, default 0
		BL		ncontrol;	// number of control points, code 73, default 0
		BL		nfit;		// number of fit points, code 74, default 0
		B		weight;		// RLZ ??? flags, weight, code 70, bit 4 (16)

		if (scenario == 2) {
			flags		= 8;		//scenario 2 = not rational & planar
			read(bits, tolfit, tgStart, tgEnd, nfit);
		} else if (scenario == 1) {
			flags		= 8 + B(bits) * rational + B(bits) * closed + B(bits) * periodic;
			read(bits, tolknot, tolcontrol, nknots, ncontrol, weight);
		} else {
			return false; //RLZ: from doc only 1 or 2 are ok ?
		}

		knotslist = repeat(bits, nknots);
		
		controllist.reserve(ncontrol);
		if (weight)
			weightlist.reserve(ncontrol);

		for (int32 i = 0; i < ncontrol; ++i) {
			controllist.push_back(bits);
			if (weight)
				weightlist.push_back(bits); //RLZ Warning: D (BD or RD)
		}
		fitlist = repeat(bits, nfit);
		return parse_handles(bits);
	}
};

struct Hatch : Entity {
	struct Loop {
		enum TYPE {
			VERTEX		= -1,
			LINE		= 1,
			CIRCLE_ARC	= 2,
			ELLIPSE_ARC	= 3,
			SPLINE		= 4,
		};
		struct Item {
			TYPE	type;
			Item(TYPE type) : type(type) {}
		};
		struct Vertex : Item {
			RD2	point;
			BD	bulge;
			Vertex(bitsin& bits, bool has_bulge) : Item(VERTEX), point(bits), bulge(has_bulge ? get(BD(bits)) : 0) {}
		};
		struct Line : Item {
			RD2	point1, point2;
			Line(bitsin &bits) : Item(LINE), point1(bits), point2(bits) {}
		};
		struct CircleArc : Item {
			RD2	centre;
			BD	radius, angle0, angle1;
			B	isccw;
			CircleArc(bitsin &bits) : Item(CIRCLE_ARC), centre(bits), radius(bits), angle0(bits), angle1(bits), isccw(bits) {}
		};
		struct EllipseArc : Item {
			RD2	point1, point2;
			BD	ratio, param0, param1;
			B	isccw;
			EllipseArc(bitsin &bits) : Item(ELLIPSE_ARC), point1(bits), point2(bits), ratio(bits), param0(bits), param1(bits), isccw(bits) {}
		};
		struct Spline : Item {
			BL	degree;
			B	isRational, periodic;
			RD2	tgStart, tgEnd;
			dynamic_array<RD>		knotslist;
			dynamic_array<coord>	controllist;
			dynamic_array<RD2>		fitlist;

			Spline(bitsin& bits) : Item(SPLINE), degree(bits), isRational(bits), periodic(bits) {
				BL	nknots(bits), ncontrol(bits);
				knotslist	= repeat(bits, nknots);

				controllist.reserve(ncontrol);
				for (int32 j = 0; j < ncontrol;++j) {
					RD	x(bits), y(bits), z(isRational ? get(RD(bits)) : 0);
					controllist.push_back(coord{x, y, z});
				}
				if (bits.ver > 1021) { //2010+
					BL	nfit(bits);
					fitlist = repeat(bits, nfit);
					tgStart = bits;
					tgEnd	= bits;
				}
			}
		};

		BL	type;	// boundary path type, code 92, polyline=2, default=0 */
		B	closed;	// only polyline
		dynamic_array<Item*> objlist;

		Loop(bitsin &bits) : type(bits) {
			if (!(type & 2)) {
				for (int32 j = 0, n = BL(bits); j < n; ++j) {
					switch (RC(bits)) {
						case LINE:			objlist.push_back(new Line(bits));		break;
						case CIRCLE_ARC:	objlist.push_back(new CircleArc(bits));	break;
						case ELLIPSE_ARC:	objlist.push_back(new EllipseArc(bits));break;
						case SPLINE:		objlist.push_back(new Spline(bits));	break;
					}
				}
			} else {
				B	has_bulge(bits);
				closed	= bits;
				for (int32 j = 0, n = BL(bits); j < n; ++j)
					objlist.push_back(new Vertex(bits, has_bulge));
			}
		}
	};

	struct Line {
		BD	angle;
		BD2 point, offset;
		dynamic_array<BD>	dash;
		Line(bitsin &bits) : angle(bits), point(bits), offset(bits), dash(repeat(bits, BS(bits))) {}
	};

	TV		name;			// hatch pattern name, code 2
	BD		z;
	BD3		extPoint;
	B		solid;			// solid fill flag, code 70, solid=1, pattern=0 */
	B		associative;	// associativity, code 71, associatve=1, non-assoc.=0 */
	BS		hstyle;			// hatch style, code 75 */
	BS		hpattern;		// hatch pattern type, code 76 */
	B		doubleflag;		// hatch pattern double flag, code 77, double=1, single=0 */
	BD		angle;			// hatch pattern angle, code 52 */
	BD		scale;			// hatch pattern scale, code 41 */
	BD		pixsize;

	dynamic_array<Line>	deflines;	// pattern definition lines
	dynamic_array<Loop> loops;		// polyline list
	TV					gradName;
	Gradient			grad;
	dynamic_array<RD2>	seeds;
	dynamic_array<H>	bound;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		if (bits.ver > 1015) { //2004+
			grad.read(bits);
			gradName	= sbits;
		}
		z			= bits;
		extPoint	= bits;
		name		= sbits;
		solid		= bits;
		associative = bits;

		BL	numloops(bits);
		loops.reserve(numloops);
		uint32 totalBoundItems = 0;
		bool havePixelSize = false;
		for (int i = 0; i < numloops; i++) {
			auto&	loop = loops.push_back(bits);
			havePixelSize	|= loop.type & 4;
			totalBoundItems += BL(bits);
		}

		hstyle		= bits;
		hpattern	= bits;

		if (!solid) {
			read(bits, angle, scale, doubleflag);
			deflines = repeat(bits, BS(bits));
		}

		if (havePixelSize)
			pixsize	= bits;
		
		seeds = repeat(bits, BL(bits));
		
		if (!parse_handles(bits))
			return false;

		bound = repeat(bits, totalBoundItems);
		return true;
	}
};

struct Image : Entity {
	BD3		point1;
	BD3		point2;
	BD3		vVector;		// V-vector of single pixel, x coordinate, code 12, 22 & 32 */
	RD2		size;			// image size in pixels, U value, code 13
	B		clip;			// Clipping state, code 280, 0=off 1=on
	RC		brightness;		// Brightness value, code 281, (0-100) default 50
	RC		contrast;		// Brightness value, code 282, (0-100) default 50
	RC		fade;			// Brightness value, code 283, (0-100) default 0
	uint32	ref;			// Hard reference to imagedef object, code 340 */

	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		BL	classVersion;
		BS	displayProps;
		read(bits, classVersion, point1, point2, vVector, size, displayProps, clip, brightness, contrast, fade);

		if (bits.ver > 1021) //2010+
			B clipMode(bits);

		BS clipType(bits);
		if (clipType == 1) {
			unused(RD(bits), RD(bits));

		} else { //clipType == 2
			for (uint32 i = 0, n = BL(bits); i < n; ++i)
				(void)RD(bits);
		}

		if (!parse_handles(bits))
			return false;

		ref	= H(bits);
		(void)H(bits);
		//    RS crc;   //RS */
		return true;
	}
};

struct Dimension : Entity {
	RC		dim_type;		// Dimension type, code 70
	coord	textPoint;		// Middle point of text, code 11, 21 & 31 (OCS)
	TV		text;			// Dimension text explicitly entered by the user, code 1
	BS		align;			// attachment point, code 71
	BS		linesty;		// Dimension text line spacing style, code 72, default 1
	BD		linefactor;		// Dimension text line spacing factor, code 41, default 1? (value range 0.25 to 4.00
	BD		rot;			// rotation angle of the dimension text, code 53
	BEXT	extPoint;		// extrusion normal vector, code 210, 2
	BD		hdir;			// horizontal direction for the dimension, code 51, default ?
	RD2		clonePoint;		// Insertion point for clones (Baseline & Continue), code 12, 22 & 32 (OCS)

	H		styleH;
	H		blockH;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		if (bits.ver > 1021) //2010+
			RC dimVersion(bits);

		extPoint = bits;
		if (bits.ver > 1014) //2000+
			bits.get(5);

		textPoint = {RD(bits), RD(bits), BD(bits)};
		
		dim_type	= bits;
		text		= sbits;
		rot			= bits;
		hdir		= bits;
		BD3		inspoint(bits);
		BD		insRot_code54(bits);
		if (bits.ver > 1014) { //2000+
			align		= bits;
			linesty		= bits;
			linefactor	= bits;
			BD actMeas(bits);
			if (bits.ver > 1018) { //2007+
				B	unk(bits), flip1(bits), flip2(bits);
			}
		}
		clonePoint = bits;
		return true;
	}

	bool parse_handles(bitsin &bits) {
		if (!Entity::parse_handles(bits))
			return false;

		styleH	= bits;
		blockH	= bits;
		return true;
	}
};

struct DimensionAligned : Dimension	{
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	BD	oblique;		// oblique angle

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, def1, def2, defpoint, oblique)
			&&	parse_handles(bits);
	}
};

struct DimensionLinear : Dimension {
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	BD	oblique;		// oblique angle
	BD	angle;			// Angle of rotated, horizontal, or vertical dimensions, code 50

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, def1, def2, defpoint, oblique, angle)
			&&	parse_handles(bits);
	}
};

struct DimensionRadial : Dimension {
	BD3	defpoint;
	BD3	circlePoint;	// Definition point for diameter, radius & angular dims code 15, 25 & 35 (WCS)
	BD	radius;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, defpoint, circlePoint, radius)
			&&	parse_handles(bits);
	}
};

struct DimensionDiametric : Dimension	{
	BD3	circlePoint;	// Definition point for diameter, radius & angular dims code 15, 25 & 35 (WCS)
	BD3	defpoint;
	BD	radius;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, circlePoint, defpoint, radius)
			&&	parse_handles(bits);
	}
};

struct DimensionAngularLn2 : Dimension	{
	RD2	arcPoint;		// Point defining dimension arc, x coordinate, code 16, 26 & 36 (OCS)
	BD3	def1;
	BD3	def2;
	BD3	centrePoint;
	BD3	defpoint;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, arcPoint, def1, def2, centrePoint, defpoint)
			&&	parse_handles(bits);
	}
};

struct DimensionAngularPt3 : Dimension	{
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	BD3	centrePoint;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, defpoint, def1, def2, centrePoint)
			&&	parse_handles(bits);
	}
};

struct DimensionOrdinate : Dimension	{
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		RC	type2;
		//type =  (type2 & 1) ? type | 0x80 : type & 0xBF; //set bit 6
		return Dimension::parse(bits, sbits, bsize)
			&&	read(bits, defpoint, def1, def2, type2)
			&&	parse_handles(bits);
	}
};

struct Leader : Entity {
	B		arrow;				// Arrowhead flag, code 71, 0=Disabled; 1=Enabled
	B		hookline;			// Hook line direction flag, code 74, default 1
	BD2		textsize;			// Text annotation height, code 40
	BEXT	extrusionPoint;		// Normal vector, code 210, 220 & 230
	BD3		horizdir;			// "Horizontal" direction for leader, code 211, 221 & 231
	BD3		offsetblock;		// Offset of last leader vertex from block, code 212, 222 & 232
	coord	offsettext;			// Offset of last leader vertex from annotation, code 213, 223 & 233

	dynamic_array<BD3>	vertexlist;		// vertex points list, code 10, 20 & 30

	H		styleH;
	H		AnnotH;
	
	bool parse(bitsin &bits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		B	unknown_bit(bits);
		BS	annot_type(bits);
		BS	Path_type(bits);
		BL	nPt(bits);

		// add vertexes
		vertexlist = repeat(bits, nPt);

		BD3		Endptproj(bits);
		extrusionPoint = bits;
		if (bits.ver > 1014) //2000+
			bits.get(5);

		horizdir	= bits;
		offsetblock = bits;
		if (bits.ver > 1012) //R14+
			BD3	unk(bits);

		if (bits.ver < 1015) //R14 -
			BD	dimgap(bits);

		if (bits.ver < 1024) //2010-
			textsize = bits;

		hookline	= bits;
		arrow		= bits;

		if (bits.ver < 1015) { //R14 -
			BS	nArrow_head_type(bits);
			BD	dimasz(bits);
			B	nunk_bit(bits);
			B	unk_bit(bits);
			BS	unk_short(bits);
			BS	byBlock_color(bits);
		} else { //R2000+
			BS	unk_short(bits);
		}
		bits.get(2);

		if (!parse_handles(bits))
			return false;

		AnnotH		= bits;
		styleH	= bits;
		return true;
	}
};

struct Viewport : Entity { 
	BD3		point;
	BD2		pssize;			// Width in paper space units, code 40
	RD2		centerP;		// view center point X, code 12
	RD2		snapP;			// Snap base point X, code 13
	RD2		snapSpP;		// Snap spacing X, code 14
	int		vpstatus;		// Viewport status, code 68
	int		vpID;			// Viewport ID, code 69

	BD3		viewTarget;		// View target point, code 17, 27, 37
	BD3		viewDir;		// View direction vector, code 16, 26 & 36
	BD		twistAngle;		// view twist angle, code 51
	BD		viewHeight;		// View height in model space units, code 45
	BD		viewLength;		// Perspective lens length, code 42
	BD		frontClip;		// Front clip plane Z value, code 43
	BD		backClip;		// Back clip plane Z value, code 44
	BD		snapAngle;		// Snap angle, code 50

	BS		GridMajor;
	RenderMode	renderMode;
	UserCoords	ucs;
	dynamic_array<H>	frozen;

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!Entity::parse(bits, bsize))
			return false;

		point	= bits;
		pssize	= bits;
		//RLZ TODO: complete in dxf
		if (bits.ver > 1014) {//2000+
			read(bits, viewTarget, viewDir, twistAngle, viewHeight, viewLength, frontClip, backClip, snapAngle, centerP, snapP, snapSpP);
			//RLZ: need to complete
		}
		if (bits.ver > 1018)//2007+
			GridMajor = bits;

		if (bits.ver > 1014) {//2000+
			frozen.resize(BL(bits));
			BL	Status_Flags(bits);
			TV	Style_sheet(sbits);
			RC	Render_mode(bits);
			B	UCS_OMore(bits);
			B	UCS_VMore(bits);
			ucs.read(bits);
		}
		if (bits.ver > 1015)//2004+
			renderMode.read(bits);

		if (!parse_handles(bits))
			return false;

		if (bits.ver < 1015)
			(void)H(bits);

		if (bits.ver > 1014) {
			for (auto &h : frozen)
				h = bits;
			(void)H(bits);
			if (bits.ver == 1015)//2000 only
				(void)H(bits);
			unused(H(bits), H(bits));
		}
		if (bits.ver > 1018)//2007+
			unused(H(bits), H(bits), H(bits), H(bits));

		return true;
	}
};

//-----------------------------------------------------------------------------
// TableEntries
//-----------------------------------------------------------------------------

struct TableEntry {
	enum FLAGS {
		xdep		= 1 << 4,
		has_entity	= 1 << 6,

		no_xdict	= 1 << 16,
		has_binary	= 1 << 17,
	};
	OT			type;
	uint32		obj_size		= 0;
	uint32		flags			= 0;
	uint32		handle			= 0;
	uint32		parentH			= 0;	// Soft-pointer ID/handle to owner object, code 330
	TV			name;

	dynamic_array<H>			reactors;
	hash_map<uint32, Variant>	extended;

protected:
	bool parse(bitsin &bits, uint32 size);
	bool parse_handles(bitsin &bits);
};

bool TableEntry::parse(bitsin &bits, uint32 bsize) {
	type		= bits;
	obj_size	= bits.ver > 1014 && bits.ver < 1024 ? bits.get<uint32>() : bsize;
	handle		= H(bits);

	while (uint32 xsize = BS(bits)) {
		H		ah(bits);
		bit_seeker(bits, xsize * 8), (extended[ah] = read_extended(bits));
	}

	if (bits.ver < 1015)
		obj_size = bits.get<uint32>();

	reactors.resize(BS(bits));
	
	if (bits.ver > 1015)
		flags |= B(bits) * no_xdict;

	if (bits.ver > 1024)
		flags |= B(bits) * has_binary;

	return true;
}

bool TableEntry::parse_handles(bitsin &bits) {
	if (bits.ver > 1018)		// skip string area
		bits.seek_bit(obj_size);

	parentH = H(bits).get_offset(handle);

	for (auto &h : reactors)
		h = bits;

	if (!(flags & no_xdict))//linetype in 2004 seems not have XDicObjH or NULL handle
		H XDicObjH(bits);

	return true;
}

struct ObjControl : TableEntry {
	dynamic_array<uint32>	handles;
	bool	parse(bitsin& bits, uint32 bsize);
};

bool ObjControl::parse(bitsin& bits, uint32 bsize) {
	if (!TableEntry::parse(bits, bsize))
		return false;

	uint32	numEntries	= BL(bits);

	// if (type == 68 && bits.ver== 1015) {//V2000 dimstyle seems have one unknown byte hard handle counter??
	int		unkData = type == DIMSTYLE_CONTROL_OBJ && bits.ver > 1014 ? (int)RC(bits) : 0;

	parse_handles(bits);

	//add 2 for modelspace, paperspace blocks & bylayer, byblock linetypes
	if (type == BLOCK_CONTROL_OBJ || type == LTYPE_CONTROL_OBJ)
		numEntries += 2;

	for (int i = 0; i < numEntries; i++) {
		if (auto h = H(bits).get_offset(handle)) //in vports R14 I found some NULL handles
			handles.push_back(h);
	}

	for (int i = 0; i < unkData; i++)
		(void)H(bits).get_offset(handle);

	return true;
}

struct BlockRecord : TableEntry {
	enum {CONTROL = BLOCK_CONTROL_OBJ};
	enum FLAGS {
		anonymous		= 1 << 0,
		contains_attdefs = 1 << 1,
		blockIsXref		= 1 << 2,
		xrefOverlaid	= 1 << 3,
		loaded_Xref		= 1 << 5,
		canExplode		= 1 << 7,
	};
	
	BS			insUnits;		// block insertion units, code 70 of block_record
	RC			scaling;
	BD3			basePoint;		// block insertion base point dwg only
	TV			xref_path;
	uint32		block;			// handle for block entity
	uint32		endBlock;		// handle for end block entity
	HandleRange	entities;
	dynamic_array<H>	inserts;
	H			layoutH;
	bool parse(bitsin &bits, bitsin &sbits, uint32 size);
};

bool BlockRecord::parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
	if (!TableEntry::parse(bits, bsize))
		return false;

	name	= sbits;

	flags	|= B(bits) * has_entity;
	if (bits.ver < 1021)//2004-
		BS xrefindex(bits);
	flags	|= B(bits) * xdep;
	flags	|= B(bits) * anonymous;
	flags	|= B(bits) * contains_attdefs;
	flags	|= B(bits) * blockIsXref;
	flags	|= B(bits) * xrefOverlaid;

	if (bits.ver > 1014)
		flags |= B(bits) * loaded_Xref;

	//Number of objects owned by this block
	uint32	objectCount = bits.ver > 1015 ? (uint32)BL(bits) : !(flags & blockIsXref) && !(flags & xrefOverlaid);

	basePoint	= bits;
	xref_path	= bits;

	uint32 insertCount = 0;
	if (bits.ver > 1014) {
		while (uint8 i = RC(bits))
			insertCount +=i;
		TV bkdesc(bits);

		uint32 prevData = BL(bits);
		for (uint32 j= 0; j < prevData; ++j)
			(void)RC(bits);
	}

	if (bits.ver > 1018) {//2007+
		insUnits	= bits;
		flags		|= B(bits) * canExplode;
		scaling		= bits;
	}

	parse_handles(bits);
	H XRefH(bits);

	block		= H(bits).get_offset(handle);
	entities.read(bits, objectCount);
	endBlock	= H(bits).get_offset(handle);

	if (bits.ver > 1014) {//2000+
		inserts = repeat(bits, insertCount);
		layoutH	= bits;
	}
	//    RS crc;   //RS */

	return true;
}

struct Layer : TableEntry {
	enum {CONTROL = LAYER_CONTROL_OBJ};
	enum FLAGS {
		frozen		= 1 << 0,
		layeron		= 1 << 1,
		frozen_new	= 1 << 2,
		locked		= 1 << 3,
		plotF		= 1 << 4,
	};
	CMC			color;
	LineWidth	lWeight;
	H			handlePlotS;			// Hard-pointer ID/handle of plotstyle, code 390
	H			handleMaterialS;		// Hard-pointer ID/handle of materialstyle, code 347
	H			linetypeH;
	bool parse(bitsin &bits, bitsin &sbits, uint32 size);
	friend tag2 _GetName(const Layer &entry)	{ return entry.name; }
};

bool Layer::parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
	if (!TableEntry::parse(bits, bsize))
		return false;

	name	= sbits;
	flags	|= B(bits) * has_entity;
	if (bits.ver < 1021)
		BS xrefindex(bits);
	flags	|= B(bits) * xdep;
	if (bits.ver < 1015) {
		flags |= B(bits) * frozen;
		(void)B(bits); //unused, negate the color
		flags |= B(bits) * frozen_new;
		flags |= B(bits) * locked;
	}
	if (bits.ver > 1014) {
		int16 f = BS(bits);
		flags |= f & 31;
		lWeight = DWGtoLineWidth((f >> 5) & 31);
	}
	color = bits; //BS or CMC //ok for R14 or negate

	parse_handles(bits);
	H XRefH(bits);

	if (bits.ver > 1014)//2000+
		handlePlotS = bits;

	if (bits.ver > 1018)//2007+
		handleMaterialS = bits;

	linetypeH = bits;
	//    RS crc;   //RS */

	return true;
}

struct TextStyle : TableEntry {
	enum {CONTROL = STYLE_CONTROL_OBJ};
	enum FLAGS {
		shape		= 1 << 0,
		vertical	= 1 << 2,
	};
	BD		height;		// Fixed text height (0 not set), code 40
	BD		width;		// Width factor, code 41
	BD		oblique;	// Oblique angle, code 50
	RC		genFlag;	// Text generation flags, code 71
	BD		lastHeight;	// Last height used, code 42
	TV		font;		// primary font file name, code 3
	TV		bigFont;	// bigfont file name or blank if none, code 4
	bool parse(bitsin &bits, bitsin &sbits, uint32 size);
};

bool TextStyle::parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
	if (!TableEntry::parse(bits, bsize))
		return false;

	name		= sbits;
	flags		|= B(bits) * has_entity;
	BS	xrefindex(bits);
	flags		|= B(bits) * xdep;
	flags		|= B(bits) * vertical;
	flags		|= B(bits) * shape;

	height		= bits;
	width		= bits;
	oblique		= bits;
	genFlag		= bits;
	lastHeight	= bits;
	font		= bits;
	bigFont		= bits;

	parse_handles(bits);
	H XRefH(bits);
	//    RS crc;   //RS */

	return true;
}

struct LineType : TableEntry {
	enum {CONTROL = LTYPE_CONTROL_OBJ};

	struct Entry {
		enum FLAGS {
			horizontal	= 1 << 0,	// text is rotated 0 degrees, otherwise it follows the segment
			shape_index	= 1 << 1,	// complexshapecode holds the index of the shape to be drawn
			text_index	= 1 << 2,	// complexshapecode holds the index into the text area of the string to be drawn.
		};
		BD	hash_length;
		BS	code;
		RD	x_offset, y_offset, scale, rotation;
		BS	flags;
		Entry(bitsin &bits) : hash_length(bits), code(bits), x_offset(bits), y_offset(bits), scale(bits), rotation(bits), flags(bits) {}
	};
	TV	desc;					// descriptive string, code 3
	RC	align;					// align code, always 65 ('A') code 72
	BD	length;					// total length of pattern, code 40
	RC	haveShape;				// complex linetype type, code 74
	malloc_block	strarea;
	H	dashH, shapeH;
	dynamic_array<Entry> path;	// trace, point or space length sequence, code 49

	bool parse(bitsin &bits, bitsin &sbits, uint32 size);
};

bool LineType::parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
	if (!TableEntry::parse(bits, bsize))
		return false;

	name	= TV(sbits);
	flags	|= B(bits) * has_entity;
	if (bits.ver <= 1018)//2007+
		BS xrefindex(bits);
	flags	|= B(bits) * xdep;

	desc	= sbits;
	length	= bits;
	align	= bits;

	path	= repeat(bits, RC(bits));
	bool haveStrArea = false;
	for (auto &i : path)
		haveStrArea = haveStrArea || (i.flags & Entry::shape_index);

	if (bits.ver < 1021) //2004-
		strarea.read(bits, 256);
	else if (haveStrArea)
		strarea.read(bits, 512);

	parse_handles(bits);
	H XRefH(bits);

	dashH	= bits;
	shapeH	= bits;
	return true;
}

struct View : TableEntry {
	enum {CONTROL = VIEW_CONTROL_OBJ};
	enum FLAGS {
		plottable			= 1 << 0,
		pspace				= 1 << 1,
		use_default_lights	= 1 << 4,
	};
	BD		height;
	BD		width;
	RD2		center;

	BD3		viewTarget;		// View target point, code 17, 27, 37
	BD3		viewDir;		// View direction vector, code 16, 26 & 36
	BD		twistAngle;		// view twist angle, code 51
	BD		LensLength;
	BD		frontClip;		// Front clip plane Z value, code 43
	BD		backClip;		// Back clip plane Z value, code 44
	uint8	ViewMode;		// 4 bits

	RenderMode	renderMode;
	UserCoords	ucs;
		
	H		BackgroundH;
	H		VisualStyleH;
	H		SunH;
	H		BaseUCSH;
	H		NamedUCSH;
	H		LiveSectionH;

	bool parse(bitsin& bits, bitsin& sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;

		name	= TV(sbits);
		flags	|= B(bits) * has_entity;
		if (bits.ver <= 1018)//2007+
			BS xrefindex(bits);
		flags	|= B(bits) * xdep;

		read(bits, height, width, center, viewTarget, viewDir, twistAngle, LensLength, frontClip, backClip);
		ViewMode = bits.get(4);

		if (bits.ver > 1014) //2000+
			renderMode.read(bits);

		flags		|= B(bits) * pspace;

		if (bits.ver >= R2000 &&  B(bits))
			ucs.read(bits);

		if (bits.ver >= R2007)
			flags	|= B(bits) * plottable;
		
		parse_handles(bits);
		H XRefH(bits);

		if (bits.ver >= R2007)
			read(bits, BackgroundH, VisualStyleH, SunH);

		if (bits.ver >= R2000)
			read(bits, BaseUCSH, NamedUCSH);

		if (bits.ver >= R2007)
			LiveSectionH = bits;
		return true;
	}
};


struct UCSEntry : TableEntry, UserCoords {
	enum {CONTROL = UCS_CONTROL_OBJ};

	BS		ortho_type;

	bool parse(bitsin& bits, bitsin& sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;

		name	= sbits;
		flags	|= B(bits) * has_entity;
		BS	xrefindex(bits);
		flags	|= B(bits) * xdep;

		UserCoords::read(bits);

		if (bits.ver >= R2000)
			ortho_type	= bits;

		return parse_handles(bits);
	}
};

struct ViewPort : TableEntry {
	enum {CONTROL = VPORT_CONTROL_OBJ};
	enum FLAGS {
		fastZoom			= 1 << 0,
		snap				= 1 << 1,
		grid				= 1 << 2,
		snapStyle			= 1 << 3,
	};
	RD2		lowerLeft;		// Lower left corner, code 10 & 20 */
	RD2		UpperRight;		// Upper right corner, code 11 & 21 */
	RD2		center;			// center point in WCS, code 12 & 22 */
	RD2		snapBase;		// snap base point in DCS, code 13 & 23 */
	RD2		snapSpacing;	// snap Spacing, code 14 & 24 */
	RD2		gridSpacing;	// grid Spacing, code 15 & 25 */
	BD3		viewDir;		// view direction from target point, code 16, 26 & 36 */
	BD3		viewTarget;		// view target point, code 17, 27 & 37 */
	BD		height;			// view height, code 40 */
	BD		ratio;			// viewport aspect ratio, code 41 */
	BD		lensHeight;		// lens height, code 42 */
	BD		frontClip;		// front clipping plane, code 43 */
	BD		backClip;		// back clipping plane, code 44 */
	BD		snapAngle;		// snap rotation angle, code 50 */
	BD		twistAngle;		// view twist angle, code 51 */
	int		viewMode;		// view mode, code 71 */
	BS		circleZoom;		// circle zoom percent, code 72 */
	uint8	ucsIcon;		// UCSICON setting, code 74 */
	BS		snapIsopair;	// snap isopair, code 78 */
	BS		gridBehavior;	// grid behavior, code 60, undocummented */

	RenderMode	renderMode;

	bool parse(bitsin &bits, bitsin &sbits, uint32 size);
};

bool ViewPort::parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
	if (!TableEntry::parse(bits, bsize))
		return false;

	name		= sbits;
	flags		|= B(bits) * has_entity;
	if (bits.ver < 1021)//2004-
		BS xrefindex(bits);

	flags		|= B(bits) * xdep;
	height		= bits;
	ratio		= bits;
	center		= bits;
	viewTarget	= bits;
	viewDir		= bits;
	twistAngle	= bits;
	lensHeight	= bits;
	frontClip	= bits;
	backClip	= bits;
	viewMode	= bits.get(4);

	if (bits.ver > 1014) //2000+
		renderMode.read(bits);

	lowerLeft	= bits;
	UpperRight	= bits;
	viewMode	|= B(bits) << 3; //UCSFOLLOW, view mode, code 71, bit 3 (8)
	circleZoom	= bits;
	flags		|= B(bits) * fastZoom;
	ucsIcon		= bits.get(2);
	flags		|= B(bits) * grid;
	gridSpacing	= bits;
	flags		|= B(bits) * snap;
	flags		|= B(bits) * snapStyle;
	snapIsopair	= bits;
	snapAngle	= bits;
	snapBase	= bits;
	snapSpacing	= bits;

	if (bits.ver > 1014) { //2000+
		B	Unknown(bits);
		BD3	UCSorigin(bits);
		BD	UCSXAxis(bits);
		BD3	UCSYAxis(bits);
		BD	UCSelevation(bits);
		BS	UCSOrthographicType(bits);
		if (bits.ver > 1018) { //2007+
			gridBehavior = bits;
			BS	GridMajor(bits);
		}
	}

	parse_handles(bits);
	H XRefH(bits);

	if (bits.ver > 1014) { //2000+
		if (bits.ver > 1018) { //2007+
			H bkgrdH(bits);
			H visualStH(bits);
			H sunH(bits);
		}
		H namedUCSH(bits);
		H baseUCSH(bits);
	}

	//    RS crc;   //RS */

	return true;
}

struct AppId : TableEntry {
	enum {CONTROL = APPID_CONTROL_OBJ};
	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;

		name	= sbits;
		flags	|= B(bits) * has_entity;
		BS		xrefindex(bits);
		flags	|= B(bits) * xdep;
		RC		unknown(bits);

		parse_handles(bits);
		H XRefH(bits);

		//    RS crc;   //RS */
		return true;
	}
};

struct DimStyle : TableEntry, DimStyleParams {
	enum {CONTROL = DIMSTYLE_CONTROL_OBJ};

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;

		name	= sbits;
		flags	|= B(bits) * has_entity;
		BS		xrefindex(bits);
		flags	|= B(bits) * xdep;

		return DimStyleParams::parse(bits, sbits, bits)
			&& parse_handles(bits);
	}
};

struct VPEntityHeader : TableEntry {
	enum {CONTROL = VP_ENT_HDR_CTRL_OBJ};
	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		return TableEntry::parse(bits, bsize);
	}
};

struct ImageDef : public TableEntry {
	BL	imgVersion;				// class version, code 90, 0=R14 version
	RD2	imageSize;				// image size in pixels U value, code 10
	RD2	pixelSize;				// default size of one pixel U value, code 11
	B	loaded;					// image is loaded flag, code 280, 0=unloaded, 1=loaded
	RC	resolution;				// resolution units, code 281, 0=no, 2=centimeters, 5=inch

	bool parse(bitsin &bits, bitsin &sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;
		read(bits, imgVersion, imageSize);
		name		= sbits;
		read(bits, loaded, resolution, pixelSize);

		parse_handles(bits);
		H XRefH(bits);
		//    RS crc;   //RS */
		return true;
	}
};

struct PlotSettings : public TableEntry {
	BD marginLeft;		// Size, in millimeters, of unprintable margin on left side of paper, code 40
	BD marginBottom;	// Size, in millimeters, of unprintable margin on bottom side of paper, code 41
	BD marginRight;		// Size, in millimeters, of unprintable margin on right side of paper, code 42
	BD marginTop;		// Size, in millimeters, of unprintable margin on top side of paper, code 43

	bool parse(bitsin& bits, bitsin& sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;
		name	= sbits;
		return read(bits, marginLeft, marginBottom, marginRight, marginTop)
			&&	parse_handles(bits);
	}
};

struct Group : TableEntry {
	BS	Unnamed;
	BS	Selectable;
	BL	Numhandles;
	H	parenthandle;
};

struct MLinestyle : TableEntry {
	struct Item {
		BD	offset;		// BD Offset of this segment
		CMC	color;		// CMC Color of this segment
		H	linetype;	// (before R2018, index)
		Item(bitsin &bits) : offset(bits), color(bits), linetype(bits) {}
	};
	TV					desc;
	BS					mlineflags;
	CMC					fillcolor;
	BD					startang;
	BD					endang;
	dynamic_array<Item>	items;

	bool parse(bitsin& bits, bitsin& sbits, uint32 bsize) {
		if (!TableEntry::parse(bits, bsize))
			return false;

		name	= sbits;
		desc	= sbits;
		read(bits, mlineflags, fillcolor, startang, endang);
		items	= repeat(bits, RC(bits));
		return parse_handles(bits);
	}

};

struct NamedObjs : TableEntry {
};

struct Layouts : TableEntry {
};

struct PlotStyles : TableEntry {
};

//-----------------------------------------------------------------------------
// Reader
//-----------------------------------------------------------------------------

struct DWG;

struct HeaderBase {
	char			version[11];
	uint8			maint_ver, one;
	packed<uint32>	image_seeker;	//0x0d
	uint8			app_ver;		//0x11
	uint8			app_maint_ver;	//0x12
	packed<uint16>	codepage;	//0x13

	VER	valid() const {
		uint32	v;
		return version[0] == 'A' && version[1] == 'C' && from_string(version + 2, v) == 4 ? VER(v) : BAD_VER;
	}
};

static uint8 header_sentinel[16]		= {0xCF,0x7B,0x1F,0x23,0xFD,0xDE,0x38,0xA9,0x5F,0x7C,0x68,0xB8,0x4E,0x6D,0x33,0x5F};
static uint8 header_sentinel_end[16]	= {0x30,0x84,0xE0,0xDC,0x02,0x21,0xC7,0x56,0xA0,0x83,0x97,0x47,0xB1,0x92,0xCC,0xA0};

static uint8 classes_sentinel[16]		= {0x8D,0xA1,0xC4,0xB8,0xC4,0xA9,0xF8,0xC5,0xC0,0xDC,0xF4,0x5F,0xE7,0xCF,0xB6,0x8A};
static uint8 classes_sentinel_end[16]	= {0x72,0x5E,0x3B,0x47,0x3B,0x56,0x07,0x3A,0x3F,0x23,0x0B,0xA0,0x18,0x30,0x49,0x75};

bool check_sentinel(istream_ref file, const uint8 sentinel[16]) {
	uint8	s[16];
	return file.read(s) && memcmp(s, sentinel, 16) == 0;
}

struct Reader {
	istream_ref file;

	Reader(istream_ref file) : file(file) {}
	virtual	~Reader()	{}
	virtual	bool	read_header(DWG &dwg)	{ return false; }
	virtual	bool	read_classes(DWG &dwg)	{ return false; }
	virtual	bool	read_handles(DWG &dwg)	{ return false; }
	virtual	bool	read_tables(DWG &dwg)	{ return false; }
};

struct DWG {
	struct Class {
		enum FLAGS {
			erase_allowed					= 1 << 0,	//1,
			transform_allowed				= 1 << 1,	//2,
			color_change_allowed			= 1 << 2,	//4,
			layer_change_allowed			= 1 << 3,	//8,
			line_type_change_allowed		= 1 << 4,	//16,
			line_type_scale_change_allowed	= 1 << 5,	//32,
			visibility_change_allowed		= 1 << 6,	//64,
			cloning_allowed					= 1 << 7,	//128,
			lineweight_change_allowed		= 1 << 8,	//256,
			plot_Style_Name_change_allowed	= 1 << 9,	//512,
			disable_proxy_warning_dialog	= 1 << 10,	//1024,
			is_R13_format_proxy				= 1 << 15,	//32768,

			wasazombie						= 1 << 16,
			makes_entities					= 1 << 17,
		};
		OBJECTTYPE	type;
		uint32		flags;
		string16	appName;	// app name, code 3 */
		string16	cName;		// C++ class name, code 2 */
		string16	dxfName;	// record name, code 1 */
		int			count;		// number of instances for a custom class, code 91*/
	};

	struct ObjectHandle {
		uint32		handle;
		uint32		loc;
		OBJECTTYPE	type;
		bool		extracted	= false;
		ObjectHandle(uint32 handle = 0, uint32 loc = 0, OBJECTTYPE type = UNUSED) : handle(handle), loc(loc), type(type) {}
		bool operator<(uint32 h) const { return handle < h; }
	};

	template<typename T> struct Table : sparse_array<T, uint32, uint32> {
		H	ctrl;
		string16	findname(uint32 h) {
			auto it = (*this)[h];
			if (it.exists())
				return it->name;
			return none;
		}
	};

	unique_ptr<Reader>	reader;

	uint16					code_page;
	VER						version;
	uint8					maintenanceVersion;
	string					comments;
	string					name;

	Table<BlockRecord>		block_map;
	Table<Layer>			layer_map;
	Table<TextStyle>		textstyle_map;
	Table<LineType>			linetype_map;
	Table<ViewPort>			viewport_map;
	Table<AppId>			appid_map;
	Table<DimStyle>			dimstyle_map;

	Table<View>				view_map;
	Table<UCSEntry>			ucs_map;
	Table<VPEntityHeader>	vpEntHeader_map;
	Table<ImageDef>			imagedef_map;

	Table<Group>			group_map;
	Table<MLinestyle>		mlinestyle_map;
	Table<NamedObjs>		named_objs_map;		

	Table<Layouts>			layouts_map;
	Table<PlotSettings>		plotsettings_map;
	Table<PlotStyles>		plotstyles_map;

	//H	DICT_MATERIALS		= handles;
	//H	DICT_COLORS			= handles;
	//H	DICT_VISUALSTYLE	= handles;
	//H	BLOCK_PAPER_SPACE	= handles;
	//H	BLOCK_MODEL_SPACE	= handles;
	//H	LTYPE_BYLAYER		= handles;
	//H	LTYPE_BYBLOCK		= handles;

	DimStyleParams				dim;
	hash_map<string, Variant>	vars;
	sparse_array<Class>			classes;
	dynamic_array<ObjectHandle>	handles;

	dynamic_array<Block>	blocks;
	dynamic_array<Entity*>	entities;	// those not in blocks

	template<typename T> bool read_table(istream_ref file, Table<T> &map);
	template<typename T> bool parse_entry(T &t, bitsin &bits, uint32 bsize);

	bool	read(const HeaderBase* h, istream_ref file);
	bool	read_header(bitsin &file, bitsin &sbits, bitsin &handles);
	bool	read_classes(bitsin &bits, bitsin &sbits, uint32 size);
	bool	read_tables(istream_ref file);
	bool	read_handles(istream_ref file);

	bool	read_blocks(istream_ref file);
	Entity*	read_entity(ObjectHandle &obj, istream_ref file);
	bool	read_object(ObjectHandle &obj, istream_ref file);
	bool	read_entities_range(istream_ref file, dynamic_array<Entity*> &result, const HandleRange &range);

	bool	process();
};

template<typename T> bool DWG::read_table(istream_ref file, Table<T> &map) {
	auto mit = lower_boundc(handles, uint32(map.ctrl.offset));
	if (mit == handles.end())
		return false;

	mit->extracted	= true;
	file.seek(mit->loc);
	uint32	size	= file.get<MS>();
	uint32	bsize	= version > 1021 ? size * 8 - file.get<MC>() : size * 8;
	temp_block		data(file, size);
	memory_reader	mr(data);
	bitsin			bits(mr, version);

	OT		type(bits);
	if (type != T::CONTROL)
		return false;

	bits.seek_bit(0);

	ObjControl	control;
	bool	ret = control.parse(bits, bsize);

	for (auto i : control.handles) {
		mit = lower_boundc(handles, i);
		if (mit == handles.end()) {
			ret = false;
			continue;
		}

		mit->extracted	= true;
		file.seek(mit->loc);
		size	= file.get<MS>();
		bsize	= version > 1021 ? size * 8 - file.get<MC>() : size * 8;

		T&	t = map[i].put();
		parse(&t, file, version, size, bsize);
	}
	return ret;
}

bool DWG::read_tables(istream_ref file) {
	bool ret = true;

	ret &= read_table(file, linetype_map);
	ret &= read_table(file, layer_map);
	ret &= read_table(file, textstyle_map);
	ret &= read_table(file, textstyle_map);
	ret &= read_table(file, dimstyle_map);
	ret &= read_table(file, viewport_map);
	ret &= read_table(file, block_map);
	ret &= read_table(file, appid_map);

	ret &= read_table(file, view_map);
	ret &= read_table(file, ucs_map);

	if (version < 1018)//r2000-
		ret &= read_table(file, vpEntHeader_map);

	ret &= read_blocks(file);

	for (auto &i : handles) {
		if (!i.extracted) {
			if (auto ent = read_entity(i, file))
				entities.push_back(ent);
			else
				ret &= !i.extracted;
		}
	}
	// read remaining objects
	for (auto &i : handles)
		ret = ret & (i.extracted || read_object(i, file));

	return ret;
}

bool DWG::read_classes(bitsin &bits, bitsin &sbits, uint32 size) {
	while (bits.tell_bit() < size) {
		BS	classnum(bits);
		auto	&c	= classes[classnum].put();
		c.flags		= BS(bits);
		c.appName	= TV(sbits);
		c.cName		= TV(sbits);
		c.dxfName	= TV(sbits);
		c.flags		|= B(bits) * Class::wasazombie;
		c.flags		|= BS(bits) == ACAD_PROXY_ENTITY ? Class::makes_entities : 0;
		c.count		= BL(bits);

		BS	version(bits), maintenence(bits);
		unused(BL(bits), BL(bits));

		c.type	= c.dxfName == "LWPOLYLINE"			? LWPOLYLINE
				: c.dxfName == "HATCH"				? HATCH
				: c.dxfName == "GROUP"				? GROUP
				: c.dxfName == "LAYOUT"				? LAYOUT
				: c.dxfName == "IMAGE"				? IMAGE
				: c.dxfName == "IMAGEDEF"			? IMAGEDEF
				: c.dxfName == "ACDBPLACEHOLDER"	? ACDBPLACEHOLDER
				: UNUSED;

		if (!c.type) {
			for (auto &i : extra_types) {
				if (c.dxfName == i) {
					c.type = OBJECTTYPE((&i - extra_types) + 0x8000);
					break;
				}
			}
		}
		ISO_ASSERT(c.type);
	}
	return true;
}

bool DWG::read_handles(istream_ref file) {
	while (!file.eof()) {
		uint16 size = file.get<uint16be>();

		temp_block		temp(file, size - 2);
		memory_reader	mr2(temp);
		uint32 handle	= 0;
		uint32 loc		= 0;

		while (!mr2.eof()) {
			handle	+= mr2.get<MC>();
			loc		+= mr2.get<MCS>();
			handles.emplace_back(handle, loc);
		}

		//verify crc
		uint16 crcCalc = crc<16>(temp);
		uint16 crcRead = file.get<uint16be>();
	}

	return true;
}

template<typename T> T* make_read_entity(bitsin &bits, uint32 bsize) {
	T	*t = new T;
	if (t->parse(bits, bsize))
		return t;
	delete t;
	return nullptr;
}

template<typename T> T* make_read_entity(bitsin &bits, bitsin &sbits, uint32 bsize) {
	T	*t = new T;
	if (t->parse(bits, sbits, bsize))
		return t;
	delete t;
	return nullptr;
}

Entity* DWG::read_entity(ObjectHandle &obj, istream_ref file) {
	file.seek(obj.loc);
	uint32	size	= file.get<MS>();
	uint32	bsize	= version > 1021 ? size * 8 - file.get<MC>() : size * 8;

	temp_block		data(file, size);
	memory_reader	mr(data);
	bitsin			bits(mr, version);

	OT				type(bits);
	bits.seek_bit(0);

	if (type >= _LOOKUP) {
		auto it = classes[type];
		if (!it.exists())
			return nullptr;

		if (auto t = it->type)
			type = t;
	}
	obj.type		= type;
	obj.extracted	= true;

	switch (type) {
		case VERTEX_2D:			return make_read_entity<Vertex2D>(bits, bsize);
		case VERTEX_3D:				
		case VERTEX_MESH:		
		case VERTEX_PFACE:		return make_read_entity<Vertex>(bits, bsize);
		case VERTEX_PFACE_FACE:	return make_read_entity<PFaceFace>(bits, bsize);
		case ARC: 				return make_read_entity<Arc>(bits, bsize);
		case CIRCLE:			return make_read_entity<Circle>(bits, bsize);
		case LINE:				return make_read_entity<Line>(bits, bsize);
		case POINT:				return make_read_entity<Point>(bits, bsize);
		case ELLIPSE:			return make_read_entity<Ellipse>(bits, bsize);
		case MINSERT:			return make_read_entity<MInsert>(bits, bsize);
		case INSERT:			return make_read_entity<Insert>(bits, bsize);
		case TRACE:				return make_read_entity<Trace>(bits, bsize);
		case FACE_3D:			return make_read_entity<Face3D>(bits, bsize);
		case SOLID:				return make_read_entity<Solid>(bits, bsize);
		case LWPOLYLINE:		return make_read_entity<LWPolyline>(bits, bsize);
		case LEADER:			return make_read_entity<Leader>(bits, bsize);
		case SPLINE:			return make_read_entity<Spline>(bits, bsize);
		case RAY:				return make_read_entity<Ray>(bits, bsize);
		case POLYLINE_3D:		return make_read_entity<Polyline3D>(bits, bsize);
		case POLYLINE_PFACE: {
			if (auto ent = make_read_entity<PolylinePFace>(bits, bsize)) {
				read_entities_range(file, ent->vertices, ent->handles);
				return ent;
			}
			return nullptr;
		}
		case XLINE:				return make_read_entity<Xline>(bits, bsize);
		case IMAGE:				return make_read_entity<Image>(bits, bsize);
	}

	auto	read_entity2 = [&](bitsin &bits, bitsin &sbits, uint32 bsize)->Entity* {
		switch (type) {
			case TEXT:					return make_read_entity<Text>(bits, sbits, bsize);
			case MTEXT:					return make_read_entity<MText>(bits, sbits, bsize);
			case HATCH:					return make_read_entity<Hatch>(bits, sbits, bsize);
			case DIMENSION_ORDINATE:	return make_read_entity<DimensionOrdinate>(bits, sbits, bsize);
			case DIMENSION_LINEAR:		return make_read_entity<DimensionLinear>(bits, sbits, bsize);
			case DIMENSION_ALIGNED:		return make_read_entity<DimensionAligned>(bits, sbits, bsize);
			case DIMENSION_ANG_PT3:		return make_read_entity<DimensionAngularPt3>(bits, sbits, bsize);
			case DIMENSION_ANG_LN2:		return make_read_entity<DimensionAngularLn2>(bits, sbits, bsize);
			case DIMENSION_RADIUS:		return make_read_entity<DimensionRadial>(bits, sbits, bsize);
			case DIMENSION_DIAMETER:	return make_read_entity<DimensionDiametric>(bits, sbits, bsize);
			case VIEWPORT:				return make_read_entity<Viewport>(bits, sbits, bsize);
			default:					obj.extracted = false; break;
		}
		return nullptr;
	};

	if (auto soffset = get_string_offset(bits, bsize)) {
		memory_reader	mrs(data);
		bitsin			sbits(mrs, version);
		sbits.seek_bit(soffset);
		return read_entity2(bits, sbits, bsize);
	}

	return read_entity2(bits, bits, bsize);
}

bool DWG::read_object(ObjectHandle& obj, istream_ref file) {
	file.seek(obj.loc);
	uint32	size	= file.get<MS>();
	uint32	bsize	= version > 1021 ? size * 8 - file.get<MC>() : size * 8;

	temp_block		data(file, size);
	memory_reader	mr(data);
	bitsin			bits(mr, version);

	OT				type(bits);
	switch (type) {
		case IMAGEDEF: {
			ImageDef	&t = imagedef_map[obj.handle];
			if (!parse(&t, file, version, size, bsize))
				return false;
			break;
		}
	}

	return true;
}


bool DWG::read_entities_range(istream_ref file, dynamic_array<Entity*> &result, const HandleRange &range) {
	bool	ret = true;

	if (version < 1018) { //pre 2004
		for (uint32 i = range.firstEH; i;) {
			auto	mit = lower_boundc(handles, i);
			if (mit == handles.end())
				return false;

			auto	ent = read_entity(*mit, file);
			if (!ent)
				ret &= !mit->extracted;
			else
				result.push_back(ent);

			i = i == range.lastEH || !ent ? 0 : ent->nextEntLink;
		}

	} else {
		for (auto &i : range.handles) {
			auto	mit = lower_boundc(handles, i);
			if (mit == handles.end()) {
				ret = false;

			} else {
				auto	ent = read_entity(*mit, file);
				if (!ent)
					ret &= !mit->extracted;
				else
					result.push_back(ent);
			}
		}
	}
	return ret;
}

bool DWG::read_blocks(istream_ref file) {
	bool ret = true;

	for (auto &i : block_map) {
		BlockRecord&	bkr	= i;

		auto	mit = lower_boundc(handles, bkr.block);
		if (mit == handles.end()) {
			ret = false;
			continue;
		}

		file.seek(mit->loc);
		uint32	size	= file.get<MS>();
		uint32	bsize	= version > 1021 ? size * 8 - file.get<MC>() : size * 8;

		auto&	block	= blocks.push_back();
		ret	&= parse(&block, file, version, size, bsize);

		//complete block entity with block record data
		block.basePoint	= bkr.basePoint;
		block.flags		= bkr.flags;
		bkr.name		= block.name;

		if (block.parentH) {
			ret &= read_entities_range(file, block.entities, bkr.entities);
		} else {
			// in dwg code 330 are not set like dxf in ModelSpace & PaperSpace, set it and do not send block entities like dxf
			block.parentH = bkr.handle;
		}

		//end block entity, really needed to parse a dummy entity??
		mit = lower_boundc(handles, bkr.endBlock);
		if (mit == handles.end()) {
			ret = false;
			continue;
		}
		file.seek(mit->loc);
		size	= file.get<MS>();
		bsize	= version > 1021 ? size * 8 - file.get<MC>() : size * 8;

		BlockEnd	end;
		ret	&= parse(&end, file, version, size, bsize);

		if (!block.parentH)
			block.parentH = bkr.handle;
	}

	return ret;
}


bool DWG::read_header(bitsin &file, bitsin &sbits, bitsin &handles) {
	if (version > 1024)//2013+
		BLL requiredVersions	= file;

	unused(BD(file), BD(file), BD(file), BD(file));
	if (version < 1021) //2007-
		unused(T(file),T(file),T(file),T(file));//unknown text

	unused(BL(file), BL(file));	//unknown

	if (version < 1015)//pre 2000
		(void)BS(file);	//unknown
	
	if (version < 1018)//pre 2004
		(void)H(handles);//hcv

	vars["DIMASO"]		= B(file);
	vars["DIMSHO"]		= B(file);
	if (version < 1015) {//pre 2000
		vars["DIMSAV"]	= B(file);
	}
	vars["PLINEGEN"]	= B(file);
	vars["ORTHOMODE"]	= B(file);
	vars["REGENMODE"]	= B(file);
	vars["FILLMODE"]	= B(file);
	vars["QTEXTMODE"]	= B(file);
	vars["PSLTSCALE"]	= B(file);
	vars["LIMCHECK"]	= B(file);
	if (version < 1015)//pre 2000
		vars["BLIPMODE"]	= B(file);

	if (version > 1015)//2004+
		(void)B(file);//undocumented

	vars["USRTIMER"]	= B(file);
	vars["SKPOLY"]		= B(file);
	vars["ANGDIR"]		= B(file);
	vars["SPLFRAME"]	= B(file);
	if (version < 1015) {//pre 2000
		vars["ATTREQ"]	= B(file);
		vars["ATTDIA"]	= B(file);
	}
	vars["MIRRTEXT"]	= B(file);
	vars["WORLDVIEW"]	= B(file);
	if (version < 1015) {//pre 2000
		vars["WIREFRAME"]	= B(file);
	}
	vars["TILEMODE"]	= B(file);
	vars["PLIMCHECK"]	= B(file);
	vars["VISRETAIN"]	= B(file);
	if (version < 1015)//pre 2000
		vars["DELOBJ"]	= B(file);

	vars["DISPSILH"]	= B(file);
	vars["PELLIPSE"]	= B(file);
	vars["PROXIGRAPHICS"]	= BS(file);//RLZ short or bit??
	if (version < 1015) //pre 2000
		vars["DRAGMODE"]	= BS(file);//RLZ short or bit??

	vars["TREEDEPTH"]	= BS(file);//RLZ short or bit??
	vars["LUNITS"]		= BS(file);
	vars["LUPREC"]		= BS(file);
	vars["AUNITS"]		= BS(file);
	vars["AUPREC"]		= BS(file);
	if (version < 1015)//pre 2000
		vars["OSMODE"]	= BS(file);
	vars["ATTMODE"]	= BS(file);
	if (version < 1015)//pre 2000
		vars["COORDS"]	= BS(file);
	vars["PDMODE"]	= BS(file);
	if (version < 1015)//pre 2000
		vars["PICKSTYLE"]	= BS(file);

	if (version > 1015)//2004+
		unused(BL(file), BL(file), BL(file));	//unknown

	vars["USERI1"]		= BS(file);
	vars["USERI2"]		= BS(file);
	vars["USERI3"]		= BS(file);
	vars["USERI4"]		= BS(file);
	vars["USERI5"]		= BS(file);
	vars["SPLINESEGS"]	= BS(file);
	vars["SURFU"]		= BS(file);
	vars["SURFV"]		= BS(file);
	vars["SURFTYPE"]	= BS(file);
	vars["SURFTAB1"]	= BS(file);
	vars["SURFTAB2"]	= BS(file);
	vars["SPLINETYPE"]	= BS(file);
	vars["SHADEDGE"]	= BS(file);
	vars["SHADEDIF"]	= BS(file);
	vars["UNITMODE"]	= BS(file);
	vars["MAXACTVP"]	= BS(file);
	vars["ISOLINES"]	= BS(file);//////////////////
	vars["CMLJUST"]		= BS(file);
	vars["TEXTQLTY"]	= BS(file);/////////////////////
	vars["LTSCALE"]		= BD(file);
	vars["TEXTSIZE"]	= BD(file);
	vars["TRACEWID"]	= BD(file);
	vars["SKETCHINC"]	= BD(file);
	vars["FILLETRAD"]	= BD(file);
	vars["THICKNESS"]	= BD(file);
	vars["ANGBASE"]		= BD(file);
	vars["PDSIZE"]		= BD(file);
	vars["PLINEWID"]	= BD(file);
	vars["USERR1"]		= BD(file);
	vars["USERR2"]		= BD(file);
	vars["USERR3"]		= BD(file);
	vars["USERR4"]		= BD(file);
	vars["USERR5"]		= BD(file);
	vars["CHAMFERA"]	= BD(file);
	vars["CHAMFERB"]	= BD(file);
	vars["CHAMFERC"]	= BD(file);
	vars["CHAMFERD"]	= BD(file);
	vars["FACETRES"]	= BD(file);/////////////////////////
	vars["CMLSCALE"]	= BD(file);
	vars["CELTSCALE"]	= BD(file);
	if (version < 1021)//2004-
		vars["MENU"]	= T(file);

	BL	day(file), msec(file);
	vars["TDCREATE"]	= day + msec / 1000.0;
	day		= file;
	msec	= file;
	vars["TDUPDATE"]	= day + msec / 1000.0;

	if (version > 1015)//2004+
		unused(BL(file), BL(file), BL(file));	//unknown

	day		= file;
	msec	= file;
	vars["TDINDWG"]		= day + msec / 1000.0;

	day		= file;
	msec	= file;
	vars["TDUSRTIMER"]	= day + msec / 1000.0;
	
	vars["CECOLOR"]	= CMC(file);//RLZ: TODO read CMC or EMC color
	H HANDSEED	= file;//always present in data stream
	H CLAYER	= handles;
	H TEXTSTYLE = handles;
	H CELTYPE	= handles;
	if (version > 1018) {//2007+
		H CMATERIAL = handles;
	}
	H DIMSTYLE	= handles;
	H CMLSTYLE	= handles;
	if (version > 1014) {//2000+
		vars["PSVPSCALE"]	= BD(file);
	}
	vars["PINSBASE"]	= BD3(file);
	vars["PEXTMIN"]		= BD3(file);
	vars["PEXTMAX"]		= BD3(file);
	vars["PLIMMIN"]		= RD2(file);
	vars["PLIMMAX"]		= RD2(file);
	vars["PELEVATION"]	= BD(file);
	vars["PUCSORG"]		= BD3(file);
	vars["PUCSXDIR"]	= BD3(file);
	vars["PUCSYDIR"]	= BD3(file);
	H PUCSNAME = handles;
	if (version > 1014) {//2000+
		H PUCSORTHOREF = handles;
		vars["PUCSORTHOVIEW"]	= BS(file);
		H PUCSBASE = handles;
		vars["PUCSORGTOP"]		= BD3(file);
		vars["PUCSORGBOTTOM"]	= BD3(file);
		vars["PUCSORGLEFT"]		= BD3(file);
		vars["PUCSORGRIGHT"]	= BD3(file);
		vars["PUCSORGFRONT"]	= BD3(file);
		vars["PUCSORGBACK"]		= BD3(file);
	}
	vars["INSBASE"]		= BD3(file);
	vars["EXTMIN"]		= BD3(file);
	vars["EXTMAX"]		= BD3(file);
	vars["LIMMIN"]		= RD2(file);
	vars["LIMMAX"]		= RD2(file);
	vars["ELEVATION"]	= BD(file);
	vars["UCSORG"]		= BD3(file);
	vars["UCSXDIR"]		= BD3(file);
	vars["UCSYDIR"]		= BD3(file);

	H UCSNAME = handles;
	if (version > 1014) {//2000+
		H UCSORTHOREF = handles;
		vars["UCSORTHOVIEW"]	= BS(file);
		H UCSBASE = handles;
		vars["UCSORGTOP"]		= BD3(file);
		vars["UCSORGBOTTOM"]	= BD3(file);
		vars["UCSORGLEFT"]		= BD3(file);
		vars["UCSORGRIGHT"]		= BD3(file);
		vars["UCSORGFRONT"]		= BD3(file);
		vars["UCSORGBACK"]		= BD3(file);
		if (version < 1021) {//2004-
			vars["DIMPOST"]		= T(file);
			vars["DIMAPOST"]	= T(file);
		}
	}

	dim.parse(file, sbits, handles);

	block_map.ctrl		= handles;
	layer_map.ctrl		= handles;
	textstyle_map.ctrl	= handles;
	linetype_map.ctrl	= handles;
	view_map.ctrl		= handles;
	ucs_map.ctrl		= handles;
	viewport_map.ctrl	= handles;
	appid_map.ctrl		= handles;
	dimstyle_map.ctrl	= handles;

	if (version < 1018)//r2000-
		vpEntHeader_map.ctrl = handles;//RLZ: only in R13-R15 ????

	group_map.ctrl		= handles;
	mlinestyle_map.ctrl	= handles;
	named_objs_map.ctrl	= handles;

	if (version > 1014) {//2000+
		vars["TSTACKALIGN"]	= BS(file);
		vars["TSTACKSIZE"]	= BS(file);
		if (version < 1021) {//2004-
			vars["HYPERLINKBASE"]	= T(file);
			vars["STYLESHEET"]	= T(file);
		}
		layouts_map.ctrl		= handles;
		plotsettings_map.ctrl	= handles;
		plotstyles_map.ctrl		= handles;
	}
	if (version > 1015) {//2004+
		H	DICT_MATERIALS = handles;
		H	DICT_COLORS = handles;
	}
	if (version > 1018)//2007+
		H	DICT_VISUALSTYLE = handles;

	if (version > 1024)//2013+
		H	UNKNOWN_HANDLE = handles;

	if (version > 1014) {//2000+
		BL	flags(file);//RLZ TODO change to 8 vars
		vars["INSUNITS"]	= BS(file);
		uint16 cepsntype	= BS(file);
		vars["CEPSNTYPE"]	= cepsntype;
		if (cepsntype == 3) {
			H	CPSNID = handles;
		}
		if (version < 1021) {//2004-
			vars["FINGERPRINTGUID"]	= T(file);
			vars["VERSIONGUID"]	= T(file);
		}
	}
	if (version > 1015) {//2004+
		vars["SORTENTS"]			= RC(file);
		vars["INDEXCTL"]			= RC(file);
		vars["HIDETEXT"]			= RC(file);
		vars["XCLIPFRAME"]			= RC(file);
		vars["DIMASSOC"]			= RC(file);
		vars["HALOGAP"]				= RC(file);
		vars["OBSCUREDCOLOR"]		= BS(file);
		vars["INTERSECTIONCOLOR"]	= BS(file);
		vars["OBSCUREDLTYPE"]		= RC(file);
		vars["INTERSECTIONDISPLAY"]	= RC(file);
		if (version < 1021)//2004-
			vars["PROJECTNAME"]	= T(file);
	}
	H	BLOCK_PAPER_SPACE	= handles;
	H	BLOCK_MODEL_SPACE	= handles;
	H	LTYPE_BYLAYER		= handles;
	H	LTYPE_BYBLOCK		= handles;

	//LTYPE CONTINUOUS
	if (version > 1018) {//2007+
		vars["CAMERADISPLAY"]		= B(file);
		unused(BL(file), BL(file), BD(file));
		vars["STEPSPERSEC"]			= BD(file);
		vars["STEPSIZE"]			= BD(file);
		vars["3DDWFPREC"]			= BD(file);
		vars["LENSLENGTH"]			= BD(file);
		vars["CAMERAHEIGHT"]		= BD(file);
		vars["SOLIDHIST"]			= RC(file);
		vars["SHOWHIST"]			= RC(file);
		vars["PSOLWIDTH"]			= BD(file);
		vars["PSOLHEIGHT"]			= BD(file);
		vars["LOFTANG1"]			= BD(file);
		vars["LOFTANG2"]			= BD(file);
		vars["LOFTMAG1"]			= BD(file);
		vars["LOFTMAG2"]			= BD(file);
		vars["LOFTPARAM"]			= BS(file);
		vars["LOFTNORMALS"]			= RC(file);
		vars["LATITUDE"]			= BD(file);
		vars["LONGITUDE"]			= BD(file);
		vars["NORTHDIRECTION"]		= BD(file);
		vars["TIMEZONE"]			= BL(file);
		vars["LIGHTGLYPHDISPLAY"]	= RC(file);
		vars["TILEMODELIGHTSYNCH"]	= RC(file);
		vars["DWFFRAME"]			= RC(file);
		vars["DGNFRAME"]			= RC(file);
		(void)B(file);
		vars["INTERFERECOLOR"]		= CMC(file);
		H	INTERFEREOBJVS	= handles;
		H	INTERFEREVPVS	= handles;
		H	DRAGVS			= handles;
		vars["CSHADOW"]	= RC(file);
		(void)BD(file);
	}
	if (version > 1012)//R14+
		unused(BS(file),BS(file),BS(file),BS(file));
#if 0
	/**** RLZ: disabled, pending to read all data ***/
	//Start reading string stream for 2007 and further
	if (version > 1018) {//2007+
		uint32 strStartPos = endBitPos -1;
		file.seek_bit(strStartPos);
		if (B(file) == 1) {
			strStartPos -= 16;
			file.seek_bit(strStartPos);
			uint32 strDataSize = file.get<uint16>();
			if (strDataSize & 0x8000) {
				strStartPos -= 16;//decrement 16 bits
				strDataSize &= 0x7FFF; //strip 0x8000;
				file.seek_bit(strStartPos);
				uint32 hiSize = file.get<uint16>();
				strDataSize |= (hiSize << 15);
			}
			strStartPos -= strDataSize;
			file.seek_bit(strStartPos);

		}
		unused(TU(file),TU(file),TU(file),TU(file));
		vars["MENU"]		= TU(file);
		vars["DIMPOST"]		= TU(file);
		vars["DIMAPOST"]	= TU(file);
		if (version > 1021) {//2010+
			vars["DIMALTMZS"]	= TU(file);
			vars["DIMMZS"]		= TU(file);
		}
		vars["HYPERLINKBASE"]	= TU(file);
		vars["STYLESHEET"]		= TU(file);
		vars["FINGERPRINTGUID"]	= TU(file);
		vars["VERSIONGUID"]		= TU(file);
		vars["PROJECTNAME"]		= TU(file);
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------
// R12
//-----------------------------------------------------------------------------

struct Reader12 : Reader {
	enum SECTIONS {
		HEADER,
		CLASSES,
		HANDLES,
		UNKNOWNS,
		TEMPLATE,
		AUXHEADER,
	};

	struct Header : HeaderBase {
		packed<uint32>	num_section_locators;	//0x15
	};

	struct section_locator {
		uint8	id;
		uint32	address;
		uint32	size;
	};

	dynamic_array<section_locator>	sections;

	//app_ver/app_maint_ver ->measuring unit?

	struct SectionStart {
		uint8	sentinel[16];
		uint32	size;
	};

	Reader12(istream_ref file, const HeaderBase* h0) : Reader(file) {
		auto	h = (Header*)h0;
		file.seek(sizeof(Header));
		sections.read(file, h->num_section_locators);

		uint32 ckcrc = 0;//fileBuf->crc8(0,0,fileBuf->getPosition());
		switch (sections.size()) {
			case 3:		ckcrc = ckcrc ^ 0xA598; break;
			case 4:		ckcrc = ckcrc ^ 0x8101; break;
			case 5:		ckcrc = ckcrc ^ 0x3CC4; break;
			case 6:		ckcrc = ckcrc ^ 0x8461;
		}
		//checkSentinel(fileBuf.get(), secEnum::FILEHEADER, false);
	}

	bool read_header(DWG& dwg) {
		auto &si = sections[HEADER];
		file.seek(si.address);

		temp_block		data(si.size);
		SectionStart	*start	= data;
		//checkSentinel(&buff, secEnum::HEADER, true);

		memory_reader	mr(data);
		bitsin			bits(mr, dwg.version);
		return dwg.read_header(bits, bits, bits);
	}


	bool read_classes(DWG& dwg) {
		auto &si = sections[CLASSES];
		file.seek(si.address);

		temp_block		data(si.size);
		SectionStart	*start	= data;
		//checkSentinel(fileBuf.get(), secEnum::CLASSES, true);

		memory_reader	mr(data);
		bitsin			bits(mr, dwg.version);
		return dwg.read_classes(bits, bits, (start->size - 1) * 8);
	}

	bool read_handles(DWG& dwg) {
		auto &si = sections[HANDLES];
		file.seek(si.address);
		temp_block		data(si.size);
		return dwg.read_handles(memory_reader(data));
	}

	bool read_tables(DWG& dwg) {
		return dwg.read_tables(file);
	}
};

//-----------------------------------------------------------------------------
// R18
//-----------------------------------------------------------------------------

struct Reader18 : Reader {
	enum {
		SYS_SECTION		= 0x41630e3b,
		DATA_SECTION	= 0x4163043b,
		MAP_SECTION		= 0x4163003b,
	};

	struct Header : HeaderBase {
		uint8	padding[3];	//0x15
		uint32	security;	//0x18
		uint32	unknown;
		uint32	summary;
		uint32	vba_project;
		uint32	_0x80;
		uint8	padding2[0x54];
	};

	static const uint8 magic[];

	static void	decrypt(uint8 *p, uint32 sz) {
		int		seed = 1;
		while (sz--) {
			seed = (seed * 0x343fd) + 0x269ec3;
			*p++ ^= seed >> 16;
		}
	}

	static uint32 checksum(uint32 seed, uint8* data, uint32 size) {
		uint16 sum1 = seed & 0xffff;
		uint16 sum2 = seed >> 16;
		for (uint8 *end = data + size; data != end;) {
			for (uint8 *chunk_end = min(end, data + 0x15b0); data < chunk_end; ++data) {
				sum1 += *data;
				sum2 += sum1;
			}
			sum1 %= 0xFFF1;
			sum2 %= 0xFFF1;
		}
		return (sum2 << 16) | (sum1 & 0xffff);
	}

	struct SystemPage {
		uint32	page_type;	//0x41630e3b
		uint32	decompressed_size;
		uint32	compressed_size;
		uint32	compression_type;	//2
		uint32	header_checksum;

		malloc_block parse(istream_ref file) {
			SystemPage	sys	= *this;;
			sys.header_checksum	= 0;
			uint32 calcsH = checksum(0, (uint8*)&sys, sizeof(sys));

			malloc_block	data(file, compressed_size);
			uint32 calcsD = checksum(calcsH, data, compressed_size);

			malloc_block	out(decompressed_size);
			decompress18	comp(data, out, compressed_size, decompressed_size);
			if (!comp.process())
				return none;
			return move(out);
		}
	};

	struct DataSection {
		uint32	page_type;// since it’s always a data section: 0x4163043b
		uint32	section;
		uint32	compressed_size;
		uint32	decompressed_size;
		uint32	offset;// (in the decompressed buffer)
		uint32	header_checksum;// (section page checksum calculated from unencoded header bytes, with the data checksum as seed)
		uint32	data_checksum;// (section page checksum calculated from compressed data bytes, with seed 0)
		uint32	Unknown;//(ODA writes a 0
	};

	struct PageMap {
		struct Entry {
			struct Free {
				uint32	parent, left, right, zero;
			};
			int32	page;
			uint32	size;
			Free	free[];
			const Entry* next() const { return page < 0 ? (const Entry*)(free + 1) : this + 1; }
		};
		struct Entry2 {
			int32	page;
			uint32	size;
			uint32	address;
			Entry2() {}
			Entry2(uint32 page, uint32 size, uint32 address) : page(page), size(size), address(address) {}
		};
		sparse_array<Entry2, uint32>	entries;

		PageMap(const_memory_block mem) {
			uint32 address = 0x100;
			for (auto &i : make_next_range<const Entry>(mem)) {
				if (i.page >= 0)
					entries[i.page] = Entry2(i.page, i.size, address);
				address += i.size;
			}
		}
	};


	struct SectionMap {
		struct Entry {
			struct Description {
				struct Page {
					uint32	Page;// (index into SectionPageMap), starts at 1
					uint32	size;// for this page (compressed size).
					uint64	offset;	//offset for this page. If smaller than the sum of the decompressed size of all previous pages, then this page is to be preceded by zero pages until this condition is met
				};

				uint64	size;
				uint32	PageCount;// there can be more pages than this, as it is just the number of pages written to file. Unwritten pages can be detected by checking if the page’s start offset is bigger than it should be based on the sum of previously read pages decompressed size (including zero pages). After reading all pages, if the total decompressed size of the pages is not equal to the section’s size, add more zero pages to the section until this condition is met.
				uint32	MaxDecompressedSize;// Size of a section page of this type (normally 0x7400)
				uint32	Unknown;
				uint32	compression_type;// (1 = no, 2 = yes, normally 2)
				uint32	SectionId;// (starts at 0). The first section (empty section) is numbered 0, consecutive sections are numbered descending from (the number of sections – 1) down to 1.
				uint32	Encrypted;// (0 = no, 1 = yes, 2 = unknown)
				char	Name[64];
				Page	_pages[];

				auto	pages() const	{ return make_range_n(_pages, PageCount); }
				auto	next()	const	{ return (const Description*)(_pages + PageCount); }

			};

			uint32	NumDescriptions;
			uint32	_0x02;
			uint32	MaxDecompressedSize;	// max size of any page
			uint32	_0x00;
			uint32	NumDescriptions2;

			auto descriptions() const { return make_range_n(make_next_iterator((const Description*)(this + 1)), NumDescriptions); }
		};

		struct Section {
			struct Page {
				uint32	page;
				uint32	size;
				uint64	offset;		// offset in dest
				uint64	address;	// offset in file
			};
			uint32				page_size;
			uint64				size;
			dynamic_array<Page>	pages;

			malloc_block parse(istream_ref file) const {
				malloc_block	out(size);
				malloc_block	page_out(page_size);

				for (auto &i : pages) {
					file.seek(i.address);

					//decrypt section header
					auto	h = file.get<DataSection>();
					uint32	x = 0x4164536b ^ i.address;
					for (int j = 0; j < 8; j++)
						((uint32*)&h)[j] ^= x;

					//get compressed data
					malloc_block	data(file, h.compressed_size);

					//calculate checksum
					uint32 calcsD = checksum(0, data, h.compressed_size);
					h.header_checksum	= 0;
					uint32 calcsH = checksum(calcsD, (uint8*)&h, sizeof(h));

					decompress18	comp(data, page_out, h.compressed_size, page_size);
					if (!comp.process())
						return none;

					page_out.slice_to(size - i.offset).copy_to(out + i.offset);
				}
				return move(out);
			}
		};
		hash_map<string, Section>	sections;

		void init(const_memory_block mem, const PageMap &page_map) {
			const Entry	*e = mem;
			for (auto &d : e->descriptions()) {
				auto&	section		= sections[d.Name].put();
				section.size		= d.size;
				section.page_size	= d.MaxDecompressedSize;
				for (auto &p : d.pages())
					section.pages.push_back(Section::Page{p.Page, p.size, p.offset, page_map.entries[p.Page]->address});
			}
		}

		malloc_block	data(istream_ref file, const char *name) {
			auto si = sections[name];
			return si.exists() ? si.get()->parse(file) : none;
		}
	};

	SectionMap	sections;

	struct FileHeader {
		struct encrypted_section {
			char	id[12];							//0x00 12	“AcFssFcAJMB” file ID string
			uint32	_0;								//0x0C 4	0x00 (long)
			uint32	_6c;							//0x10 4	0x6c (long)
			uint32	_4;								//0x14 4	0x04 (long)
			uint32	root_gap;						//0x18 4	Root tree node gap
			uint32	lower_left_gap;					//0x1C 4	Lowermost left tree node gap
			uint32	lower_right_gap;				//0x20 4	Lowermost right tree node gap
			uint32	_1;								//0x24 4	Unknown long (ODA writes 1)
			uint32	last_section_page;				//0x28 4	Last section page Id
			packed<uint64>	last_section_page_end;	//0x2C 8	Last section page end address
			packed<uint64>	second_header;			//0x34 8	Second header data address pointing to the repeated header data at the end of the file
			uint32	gap;							//0x3C 4	Gap amount
			uint32	section_page;					//0x40 4	Section page amount
			uint32	_20;							//0x44 4	0x20 (long)
			uint32	_80;							//0x48 4	0x80 (long)
			uint32	_40;							//0x4C 4	0x40 (long)
			uint32	section_page_map;				//0x50 4	Section Page Map Id
			packed<uint64>	section_page_map_addr;	//0x54 8	Section Page Map address (add 0x100 to this value)
			uint32	section_map;					//0x5C 4	Section Map Id
			uint32	section_page_array;				//0x60 4	Section page array size
			uint32	gap_array;						//0x64 4	Gap array size
			uint32	crc32;							//0x68 4	CRC32 (long) CRC calculation is done including the 4 CRC bytes that are initially zero
		} encrypted;
		uint8	magic[20];

		void decrypt() {
			Reader18::decrypt((uint8*)&encrypted, sizeof(encrypted));
		}
	};

	struct SectionStart {
		uint8	sentinel[16];
		uint32	size;
		uint32	hsize;
	};

	Reader18(istream_ref file, const HeaderBase* h0) : Reader(file) {
		auto	h = (Header*)h0;

		FileHeader	fh;
		file.seek(0x80);
		file.read(fh);
		fh.decrypt();

		auto	crc = crc32(fh.encrypted);
		ISO_VERIFY(memcmp(fh.magic, magic, 20) == 0);

		file.seek(fh.encrypted.section_page_map_addr + 0x100);
		auto	page = file.get<SystemPage>();

		ISO_ASSERT(page.page_type == SYS_SECTION);
		malloc_block	b = page.parse(file);
		if (!b)
			return;

		PageMap		page_map(b);

		auto sectionMap = page_map.entries[fh.encrypted.section_map];
		file.seek(sectionMap.get()->address);
		file.read(page);

		ISO_ASSERT(page.page_type == MAP_SECTION);
		if (b = page.parse(file))
			sections.init(b, page_map);
	}

	bool read_header(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:Header"));
		SectionStart	*start = data;
		
		memory_reader	mr(data.slice(sizeof(SectionStart)));
		bitsin			bits(mr, dwg.version);
		uint32			bitsize = bits.get32();

		memory_reader	mrh	= mr;
		bitsin			hbits(mrh, dwg.version);
		hbits.seek_bit(bitsize);

		if (auto soffset = get_string_offset(bits, bitsize)) {
			memory_reader	mrs	= mr;
			bitsin			sbits(mrs, dwg.version);
			sbits.seek_bit(soffset);
			return dwg.read_header(bits, sbits, hbits);
		}
		return dwg.read_header(bits, bits, hbits);
	}

	bool read_classes(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:Classes"));
		SectionStart	*start = data;

		memory_reader	mr(data.slice(sizeof(SectionStart)));
		bitsin			bits(mr, dwg.version);
		uint32			bitsize = bits.get32();

		BS		maxClassNum(bits);
		uint8	Rc1	= bits.get<uint8>();
		uint8	Rc2	= bits.get<uint8>();
		bool	Bit	= bits.get_bit();

		if (auto soffset = get_string_offset(bits, bitsize)) {
			memory_reader	mrs	= mr;
			bitsin			sbits(mrs, dwg.version);
			sbits.seek_bit(soffset);
			return dwg.read_classes(bits, sbits, soffset);
		}
		return dwg.read_classes(bits, bits, bitsize);
	}

	bool read_handles(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:Handles"));
		return dwg.read_handles(memory_reader(data));
	}

	bool read_tables(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:AcDbObjects"));
		return dwg.read_tables(memory_reader(data));
	}

};

const uint8 Reader18::magic[] = {
	0xf8, 0x46, 0x6a, 0x04, 0x96, 0x73, 0x0e, 0xd9,
	0x16, 0x2f, 0x67, 0x68, 0xd4, 0xf7, 0x4a, 0x4a,
	0xd0, 0x57, 0x68, 0x76
};

struct Reader21 : Reader {
	struct Header : HeaderBase {
		uint8	padding[3];	//0x15
		uint32	security;	//0x18
		uint32	unknown;
		uint32	summary;
		uint32	vba_project;
		uint32	_0x80;
		uint32	app_info_addr;
		uint8	padding2[0x50];
	};

	struct Stuff {
		uint64	CRC;
		uint64	UnknownKey;
		uint64	CompressedCRC;
		uint32	ComprLen;	//(if < 0, not compressed)
		uint32	Length;
		uint8	CompressedData[];
	};

	struct FileHeader {
		uint64	header_size;// (normally 0x70)
		uint64	File_size;
		uint64	PagesMapCrcCompressed;
		uint64	PagesMapCorrectionFactor;
		uint64	PagesMapCrcSeed;
		uint64	PagesMap2offset;// (relative to data page map 1, add 0x480 to get stream position)
		uint64	PagesMap2Id;
		uint64	PagesMapOffset;// (relative to data page map 1, add 0x480 to get stream position)
		uint64	PagesMapId;
		uint64	Header2offset;	// (relative to page map 1 address, add 0x480 to get stream position)
		uint64	PagesMapSizeCompressed;
		uint64	PagesMapSizeUncompressed;
		uint64	PagesAmount;
		uint64	PagesMaxId;
		uint64	Unknown1;// (normally 0x20)
		uint64	Unknown2;// (normally 0x40)
		uint64	PagesMapCrcUncompressed;
		uint64	Unknown3;// (normally 0xf800)
		uint64	Unknown4;// (normally 4)
		uint64	Unknown5;// (normally 1)
		uint64	SectionsAmount;// (number of sections + 1)
		uint64	SectionsMapCrcUncompressed;
		uint64	SectionsMapSizeCompressed;
		uint64	SectionsMap2Id;
		uint64	SectionsMapId;
		uint64	SectionsMapSizeUncompressed;
		uint64	SectionsMapCrcCompressed;
		uint64	SectionsMapCorrectionFactor;
		uint64	SectionsMapCrcSeed;
		uint64	StreamVersion;// (normally 0x60100)
		uint64	CrcSeed;
		uint64	CrcSeedEncoded;
		uint64	RandomSeed;
		uint64	HeaderCRC64;
	};


	struct PageMap {
		struct Entry {
			uint64	size;
			int64	id;
		};
		struct Entry2 {
			int32	page;
			uint32	size;
			uint32	address;
			Entry2() {}
			Entry2(uint32 page, uint32 size, uint32 address) : page(page), size(size), address(address) {}
		};
		sparse_array<Entry2, uint64>	entries;

		PageMap(const_memory_block mem) {
			uint64	address = 0x480;
			for (auto &i : make_range<Entry>(mem)) {
				entries[(uint32)abs(i.id)] = Entry2(i.id, i.size, address);
				address += i.size;
			}
		}
	};

	struct SectionMap {
		struct Description {
			struct Page {
				uint64 offset;	// If a page’s data offset is smaller than the sum of the decompressed size of all previous pages, then it is to be preceded by a zero page with a size that is equal to the difference between these two numbers.
				uint64 Size;
				uint64 ID;
				uint64 UncompressedSize;
				uint64 CompressedSize;
				uint64 Checksum;
				uint64 CRC;
			};
			uint64	DataSize;
			uint64	MaxSize;
			uint64	Encryption;
			uint64	HashCode;
			uint64	SectionNameLength;
			uint64	Unknown;
			uint64	Encoding;
			uint64	NumPages;
			char16	SectionName[];//SectionNameLength + 1];//0x40 SectionNameLength x 2 [+ 2] Unicode Section Name (2 bytes per character, followed by 2 zero bytes if name length > 0)
			//Page	pages[]

			auto	pages() const	{ return make_range_n((Page*)(SectionName + SectionNameLength), NumPages); }
			auto	next()	const	{ return (const Description*)pages().end(); }
		};

		struct Section {
			struct Page {
				uint32	page;
				uint32	size;
				uint32	compressed_size;
				uint64	offset;		// offset in dest
				uint64	address;	// offset in file
			};
			uint32				page_size;
			uint64				size;
			dynamic_array<Page>	pages;

			malloc_block parse(istream_ref file) {
				malloc_block	out(size);

				for (auto &i : pages) {
					file.seek(i.address);

					temp_block	data(file, i.size);
					temp_block	data_rs(i.size);

					decodeI<251, 0xb8, 8, 2>(data, data_rs, i.size / 255);

					decompress21	comp(data_rs, out + i.offset, i.compressed_size, i.size);
					if (!comp.process())
						return none;
				}
				return move(out);
			}
		};

		hash_map<string, Section>	sections;

		void init(const_memory_block mem, const PageMap &page_map) {
			uint8 nextId = 1;
			for (auto &i : make_next_range<const Description>(mem)) {
				auto&	section		= sections[i.SectionName].put();
				for (auto &p : i.pages())
					section.pages.push_back(Section::Page{p.ID, p.Size, p.offset, p.CompressedSize});
			}
		}

		malloc_block	data(istream_ref file, const char *name) {
			auto si = sections[name];
			return si.exists() ? si.get()->parse(file) : none;
		}
	};

	malloc_block parseSysPage(uint64 sizeCompressed, uint64 sizeUncompressed, uint64 correctionFactor, uint64 offset) {
		malloc_block	out;
		uint32 chunks = div_round_up(align(sizeCompressed, 8) * correctionFactor, 239);
		uint64 fpsize = chunks * 255;

		file.seek(offset);
		temp_block	data(file, fpsize);
		temp_block	data_rs(fpsize);
		decodeI<239, 0x96, 8, 8>(data, data_rs, chunks);

		decompress21	comp(data_rs, out, sizeCompressed, sizeUncompressed);
		if (!comp.process())
			return none;
		return out;
	}

	SectionMap	sections;

	struct SectionStart {
		uint8	sentinel[16];
		uint32	size;
		uint32	hsize;
	};


	Reader21(istream_ref file, const HeaderBase* h0) : Reader(file) {
		auto	h = (Header*)h0;

		file.seek(0x80);

		uint8 fileHdrRaw[0x2FD];//0x3D8
		uint8 fileHdrdRS[0x2CD];
 
		file.read(fileHdrRaw);
		decodeI<239, 0x96, 8, 8>(fileHdrRaw, fileHdrdRS, 3);

		Stuff	*stuff	= (Stuff*)fileHdrdRS;

		int fileHdrDataLength = 0x110;

		malloc_block	fileHdrData;

		if (stuff->ComprLen < 0) {
			fileHdrData.resize(-stuff->ComprLen);
			fileHdrData.copy_from(stuff + 1);
		} else {
			decompress21	comp(stuff + 1, fileHdrData, stuff->ComprLen, stuff->Length);
			if (!comp.process())
				return;
		}

		FileHeader	*fh	= fileHdrData;
		auto		b	= parseSysPage(fh->PagesMapSizeCompressed, fh->PagesMapSizeUncompressed, fh->PagesMapCorrectionFactor, 0x480+fh->PagesMapOffset);
		if (!b)
			return;

		PageMap		page_map(b);

		auto		sectionMap = page_map.entries[fh->SectionsMapId];
		if (b = parseSysPage(fh->SectionsMapSizeCompressed, fh->SectionsMapSizeUncompressed, fh->SectionsMapCorrectionFactor, sectionMap.get()->address))
			sections.init(b, page_map);
	}

	bool read_header(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:Header"));
		SectionStart	*start = data;

		memory_reader	mr(data.slice(sizeof(SectionStart)));
		bitsin			bits(mr, dwg.version);
		uint32			bitsize = bits.get32();

		memory_reader	mrh	= mr;
		bitsin			hbits(mrh, dwg.version);
		hbits.seek_bit(bitsize);

		if (auto soffset = get_string_offset(bits, bitsize)) {
			memory_reader	mrs	= mr;
			bitsin			sbits(mrs, dwg.version);
			sbits.seek_bit(soffset);
			return dwg.read_header(bits, sbits, hbits);
		}
		return dwg.read_header(bits, bits, hbits);
	}

	bool read_classes(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:Classes"));
		SectionStart	*start = data;

		memory_reader	mr(data.slice(sizeof(SectionStart)));
		bitsin			bits(mr, dwg.version);
		uint32			bitsize = bits.get32();

		BS		maxClassNum(bits);
		uint8	Rc1	= bits.get<uint8>();
		uint8	Rc2	= bits.get<uint8>();
		bool	Bit	= bits.get_bit();

		if (auto soffset = get_string_offset(bits, bitsize)) {
			memory_reader	mrs	= mr;
			bitsin			sbits(mrs, dwg.version);
			sbits.seek_bit(soffset);
			return dwg.read_classes(bits, sbits, soffset);
		}
		return dwg.read_classes(bits, bits, bitsize);
	}


	bool read_handles(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:Handles"));
		return dwg.read_handles(memory_reader(data));
	}

	bool read_tables(DWG& dwg) {
		malloc_block	data(sections.data(file, "AcDb:AcDbObjects"));
		return dwg.read_tables(memory_reader(data));
	}
};

struct Reader24 : Reader18 {
	using Reader18::Reader18;
};

struct Reader27 : Reader24 {
	using Reader24::Reader24;
};

struct Reader32 : Reader27 {
	using Reader27::Reader27;
};

bool DWG::read(const HeaderBase* h, istream_ref file) {
	switch (version = h->valid()) {
		case dwg::R13:
		case dwg::R14:
		case dwg::R2000:	reader = new Reader12(file, h); break;
		case dwg::R2004:	reader = new Reader18(file, h); break;
		case dwg::R2007:	reader = new Reader21(file, h); break;
		case dwg::R2010:	reader = new Reader24(file, h); break;
		case dwg::R2013:	reader = new Reader27(file, h); break;
		case dwg::R2018:	reader = new Reader32(file, h); break;
		default:			return false;
	}
	if (!reader->read_header(*this))
		return false;

	if (!reader->read_classes(*this))
		return false;

	if (!reader->read_handles(*this))
		return false;

	if (!reader->read_tables(*this))
		return false;
	
	return true;
}

/*
Section Name		Description																			pagesize(18)				pagesize(21)						hashcode		encryption					encoding
Empty section																							0x7400																	
AcDb:Security		Contains information regarding password and data encryption (optional)				0x7400						0xf800								0x4a0204ea		0							1
AcDb:FileDepList	Contains file dependencies (e.g. IMAGE files, or fonts used by STYLE).				0x80						max(0x100, countEntries * 0xc0)		0x6c4205ca		2							1
AcDb:VBAProject		Contains VBA Project data for this drawing (optional section)						DataSize + 0x80 + padding	align(VBA_data_size + 0x80, 0x20)	0x586e0544		2							1
AcDb:AppInfo		Contains information about the application that wrote the .dwg file (encrypted = 2)	0x80						0x300								0x3fa0043e		0							1
AcDb:Preview		Bitmap preview for this drawing.													0x400						0x400 or align(preview_size, 0x20)	0x40aa0473		if properties are encrypted	1
AcDb:SummaryInfo	Contains fields like Title, Subject, Author.										0x100						0x80								0x717a060f		if properties are encrypted	1
AcDb:RevHistory		Revision history																	0x7400						0x1000								0x60a205b3		0							4			compressed
AcDb:AcDbObjects	Database objects																	0x7400						0xf800								0x674c05a9		if data is encrypted		4			compressed
AcDb:ObjFreeSpace																						0x7400						0xf800								0x77e2061f		0							4			compressed
AcDb:Template		Template (Contains the MEASUREMENT system variable only.)							0x7400						0x400								0x4a1404ce		0							4			compressed
AcDb:Handles		Handle list with offsets into the AcDb:AcDbObjects section							0x7400						0xf800								0x3f6e0450		if data is encrypted		4			compressed
AcDb:Classes		Custom classes section																0x7400						0xf800								0x3f54045f		if data is encrypted		4			compressed
AcDb:AuxHeader																							0x7400						0x800								0x54f0050a		0							4			compressed
AcDb:Header			Contains drawing header variables													0x7400						0x800								0x32b803d9		if data is encrypted		4			compressed
AcDb:Signature		Not written by ODA

The section order in the stream is different than the order in the section map. The order in the stream is as follows:
File header
Empty section
AcDb:SummaryInf
AcDb:Preview
AcDb:VBAProject
AcDb:AppInfo
AcDb:FileDepList
AcDb:RevHistory
AcDb:Security
AcDb:AcDbObjects
AcDb:ObjFreeSpace
AcDb:Template
AcDb:Handles
AcDb:Classes
AcDb:AuxHeader
AcDb:Header
Section map
Section page map
*/

//tag2 _GetName(const TableEntry &entry)	{ return entry.name; }


} // namespace swg

ISO_DEFUSERENUM(dwg::OBJECTTYPE, 113) {
	ISO_SETENUMSQ(0, 
	UNUSED,				TEXT,					ATTRIB,				ATTDEF,				BLOCK,				ENDBLK,				SEQEND,				INSERT,
	MINSERT,			VERTEX_2D,				VERTEX_3D,			VERTEX_MESH,		VERTEX_PFACE,		VERTEX_PFACE_FACE,	POLYLINE_2D,		POLYLINE_3D,
	ARC,				CIRCLE,					LINE,				DIMENSION_ORDINATE,	DIMENSION_LINEAR,	DIMENSION_ALIGNED,	DIMENSION_ANG_PT3,	DIMENSION_ANG_LN2,
	DIMENSION_RADIUS,	DIMENSION_DIAMETER,		POINT,				FACE_3D,			POLYLINE_PFACE,		POLYLINE_MESH,		SOLID,				TRACE,
	SHAPE,				VIEWPORT,				ELLIPSE,			SPLINE,				REGION,				SOLID_3D,			BODY,				RAY,
	XLINE,				DICTIONARY,				OLEFRAME,			MTEXT,				LEADER,				TOLERANCE,			MLINE,				BLOCK_CONTROL_OBJ,
	BLOCK_HEADER,		LAYER_CONTROL_OBJ,		LAYER,				STYLE_CONTROL_OBJ,	STYLE,				LTYPE_CONTROL_OBJ,	LTYPE,				VIEW_CONTROL_OBJ,
	VIEW,				UCS_CONTROL_OBJ,		UCS,				VPORT_CONTROL_OBJ,	VPORT,				APPID_CONTROL_OBJ,	APPID,				DIMSTYLE_CONTROL_OBJ
	);
	ISO_SETENUMSQ(64, 
	DIMSTYLE,			VP_ENT_HDR_CTRL_OBJ,	VP_ENT_HDR,			GROUP,				MLINESTYLE,			OLE2FRAME,			LONG_TRANSACTION,	LWPOLYLINE,
	HATCH,				XRECORD,				ACDBPLACEHOLDER,	VBA_PROJECT,		LAYOUT,				IMAGE,				IMAGEDEF,			ACAD_PROXY_ENTITY,
	ACAD_PROXY_OBJECT,
	ACAD_TABLE,				CELLSTYLEMAP,			DBCOLOR,							DICTIONARYVAR,
	DICTIONARYWDFLT,		FIELD,					IDBUFFER,							IMAGEDEFREACTOR,
	LAYER_INDEX,			LWPLINE,				MATERIAL,							MLEADER,
	MLEADERSTYLE,			PLACEHOLDER,			PLOTSETTINGS,						RASTERVARIABLES,
	SCALE,					SORTENTSTABLE,			SPATIAL_FILTER,						SPATIAL_INDEX,
	TABLEGEOMETRY,			TABLESTYLES,			VISUALSTYLE,						WIPEOUTVARIABLE,
	ACDBDICTIONARYWDFLT,	TABLESTYLE,				EXACXREFPANELOBJECT,				NPOCOLLECTION,
	ACDBSECTIONVIEWSTYLE,	ACDBDETAILVIEWSTYLE,	ACDB_BLKREFOBJECTCONTEXTDATA_CLASS,	ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS
	);
} };


ISO_DEFUSER(dwg::BD, double);

ISO_DEFUSERCOMPV(dwg::Arc, centre, radius, staangle, endangle);
ISO_DEFUSERCOMPV(dwg::Circle, centre, radius);
ISO_DEFUSERCOMPV(dwg::Line, point1, point2);
ISO_DEFUSERCOMPV(dwg::Point, point);
ISO_DEFUSERCOMPV(dwg::Ellipse, point1, point2, ratio, staparam, endparam);
//ISO_DEFUSERCOMPV(dwg::MInsert				);
//ISO_DEFUSERCOMPV(dwg::Insert				);
//ISO_DEFUSERCOMPV(dwg::Trace				);
//ISO_DEFUSERCOMPV(dwg::Face3D				);
//ISO_DEFUSERCOMPV(dwg::Solid				);
//ISO_DEFUSERCOMPV(dwg::LWPolyline			);
//ISO_DEFUSERCOMPV(dwg::Leader				);
//ISO_DEFUSERCOMPV(dwg::Spline				);
//ISO_DEFUSERCOMPV(dwg::Ray					);
//ISO_DEFUSERCOMPV(dwg::Polyline3D			);
//ISO_DEFUSERCOMPV(dwg::PolylinePFace		);
//ISO_DEFUSERCOMPV(dwg::Text				);
//ISO_DEFUSERCOMPV(dwg::MText				);
//ISO_DEFUSERCOMPV(dwg::Hatch				);
//ISO_DEFUSERCOMPV(dwg::DimensionOrdinate	);
//ISO_DEFUSERCOMPV(dwg::DimensionLinear		);
//ISO_DEFUSERCOMPV(dwg::DimensionAligned	);
//ISO_DEFUSERCOMPV(dwg::DimensionAngPt3		);
//ISO_DEFUSERCOMPV(dwg::DimensionAngLn2		);
//ISO_DEFUSERCOMPV(dwg::DimensionRadius		);
//ISO_DEFUSERCOMPV(dwg::DimensionDiameter	);
//ISO_DEFUSERCOMPV(dwg::Viewport			);

template<> struct ISO::def<dwg::Entity> : ISO::VirtualT2<dwg::Entity> {
	ISO::Browser2	Deref(const dwg::Entity &ent) {
		switch (ent.type) {
			case dwg::ARC: 						return MakeBrowser(reinterpret_cast<const dwg::Arc 					&>(ent));
			case dwg::CIRCLE:					return MakeBrowser(reinterpret_cast<const dwg::Circle				&>(ent));
			case dwg::LINE:						return MakeBrowser(reinterpret_cast<const dwg::Line					&>(ent));
			case dwg::POINT:					return MakeBrowser(reinterpret_cast<const dwg::Point				&>(ent));
			case dwg::ELLIPSE:					return MakeBrowser(reinterpret_cast<const dwg::Ellipse				&>(ent));
		//	case dwg::MINSERT:					return MakeBrowser(reinterpret_cast<const dwg::MInsert				&>(ent));
		//	case dwg::INSERT:					return MakeBrowser(reinterpret_cast<const dwg::Insert				&>(ent));
		//	case dwg::TRACE:					return MakeBrowser(reinterpret_cast<const dwg::Trace				&>(ent));
		//	case dwg::FACE_3D:					return MakeBrowser(reinterpret_cast<const dwg::Face3D				&>(ent));
		//	case dwg::SOLID:					return MakeBrowser(reinterpret_cast<const dwg::Solid				&>(ent));
		//	case dwg::LWPOLYLINE:				return MakeBrowser(reinterpret_cast<const dwg::LWPolyline			&>(ent));
		//	case dwg::LEADER:					return MakeBrowser(reinterpret_cast<const dwg::Leader				&>(ent));
		//	case dwg::SPLINE:					return MakeBrowser(reinterpret_cast<const dwg::Spline				&>(ent));
		//	case dwg::RAY:						return MakeBrowser(reinterpret_cast<const dwg::Ray					&>(ent));
		//	case dwg::POLYLINE_3D:				return MakeBrowser(reinterpret_cast<const dwg::Polyline3D			&>(ent));
		//	case dwg::POLYLINE_PFACE:			return MakeBrowser(reinterpret_cast<const dwg::PolylinePFace		&>(ent));
		//	case dwg::TEXT:						return MakeBrowser(reinterpret_cast<const dwg::Text					&>(ent));
		//	case dwg::MTEXT:					return MakeBrowser(reinterpret_cast<const dwg::MText				&>(ent));
		//	case dwg::HATCH:					return MakeBrowser(reinterpret_cast<const dwg::Hatch				&>(ent));
		//	case dwg::DIMENSION_ORDINATE:		return MakeBrowser(reinterpret_cast<const dwg::DimensionOrdinate	&>(ent));
		//	case dwg::DIMENSION_LINEAR:			return MakeBrowser(reinterpret_cast<const dwg::DimensionLinear		&>(ent));
		//	case dwg::DIMENSION_ALIGNED:		return MakeBrowser(reinterpret_cast<const dwg::DimensionAligned		&>(ent));
		//	case dwg::DIMENSION_ANG_PT3:		return MakeBrowser(reinterpret_cast<const dwg::DimensionAngPt3		&>(ent));
		//	case dwg::DIMENSION_ANG_LN2:		return MakeBrowser(reinterpret_cast<const dwg::DimensionAngLn2		&>(ent));
		//	case dwg::DIMENSION_RADIUS:			return MakeBrowser(reinterpret_cast<const dwg::DimensionRadius		&>(ent));
		//	case dwg::DIMENSION_DIAMETER:		return MakeBrowser(reinterpret_cast<const dwg::DimensionDiameter	&>(ent));
		//	case dwg::VIEWPORT:					return MakeBrowser(reinterpret_cast<const dwg::Viewport				&>(ent));
		}
		return MakeBrowser(ent.type);
	}
};

ISO_DEFUSERCOMPV(dwg::Layer, flags);

template<typename T> struct ISO::def<dwg::DWG::Table<T>> : ISO::def<sparse_array<T, uint32, uint32>> {};

//template<typename T> struct ISO::def<dwg::DWG::Table<T>> : ISO::VirtualT2<dwg::DWG::Table<T>> {
//	ISO::Browser2	Deref(const dwg::DWG::Table<T> &table) {
//		return {};
//	}
//};


ISO_DEFUSER(dwg::OT, dwg::OBJECTTYPE);
//ISO_DEFUSERCOMPV(dwg::Entity, type);
ISO_DEFUSERCOMPV(dwg::Block, entities);
ISO_DEFUSERCOMPV(dwg::DWG, layer_map, blocks, entities);

class DWGFileHandler : public FileHandler {
	const char*		GetExt()			override { return "dwg"; }
	const char*		GetDescription()	override { return "Autodesk Drawing"; }
	int				Check(const filename &fn) override {
		return CHECK_NO_OPINION;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		char	h[128];
		if (file.read(h)) {
			ISO_ptr<dwg::DWG>	p(id);
			if (p->read((dwg::HeaderBase*)h, file))
				return p;
		}
		return ISO_NULL;
	}
} dwg_file;

#ifdef ISO_EDITOR
//-----------------------------------------------------------------------------
//	Viewer
//-----------------------------------------------------------------------------

#include "viewers/viewer.h"
#include "viewers/viewer2d.h"
#include "windows/control_helpers.h"

class ViewDWG : public win::Inherit<ViewDWG, Viewer2D> {
	ISO_ptr_machine<void>	p;

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	Paint(d2d::PAINT_INFO *info);
	ViewDWG(const win::WindowPos &wpos, const ISO_ptr_machine<void> &p) : p(p) {
		Create(wpos, (tag)p.ID(), CHILD | VISIBLE | CLIPCHILDREN | CLIPSIBLINGS, NOEX);
	}
};

LRESULT ViewDWG::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			break;


		case WM_MOUSEACTIVATE:
			SetFocus();
			SetAccelerator(*this, Accelerator());
			return MA_NOACTIVATE;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case d2d::PAINT: {
					Paint((d2d::PAINT_INFO*)nmh);
					break;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			break;
	}
	return Super(message, wParam, lParam);
}


void ViewDWG::Paint(d2d::PAINT_INFO *info) {
	auto	screen	= transformation() * scale(one, -one);
	SetTransform(screen);

	d2d::SolidBrush	black(*this, colour(0,0,0));

	if (p.IsType<dwg::Block>()) {
		dwg::Block	*block = p;

		for (auto i : block->entities) {
			switch (i->type) {
				case dwg::LINE: {
					auto	line = (dwg::Line*)i;
					d2d::point	p1(line->point1[0], line->point1[1]);
					d2d::point	p2(line->point2[0], line->point2[1]);
					DrawLine(p1, p2, black);
					break;
				}
			}
		}
	}
}


class EditorDWG : public app::Editor {
	bool Matches(const ISO::Browser &b) override {
		return b.Is<dwg::DWG>()
			|| b.Is<dwg::Block>();
	}
	win::Control Create(app::MainWindow &main, const win::WindowPos &wpos, const ISO_ptr_machine<void> &p) override {
		return *new ViewDWG(wpos, p);
	}
} editor_dwg;

#endif
