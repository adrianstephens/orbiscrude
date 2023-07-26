#include "iso/iso_files.h"
#include "codec/codec.h"
#include "codec/reed_solomon.h"
#include "base/algorithm.h"
#include "base/vector.h"
#include "utilities.h"

#undef TEXT
#undef POINT

namespace iso {
template<typename T, int N> auto get(const array<T, N>& t) {
	typedef noref_t<decltype(get(declval<T>()))>	T2;
	return array<T2, N>(t);
}
}

using namespace iso;

//-----------------------------------------------------------------------------
// dwg namespace
//-----------------------------------------------------------------------------
namespace dwg {

/*
Section Name		Description														pagesize(18)		pagesize(21)				hashcode		encryption				encoding
Empty section																		0x7400													
AcDb:Security		information regarding password and data encryption (optional)	0x7400				0xf800						0x4a0204ea		0						1
AcDb:FileDepList	file dependencies (e.g. IMAGE files, or fonts used by STYLE)	0x80				max(0x100, n * 0xc0)		0x6c4205ca		2						1
AcDb:VBAProject		VBA Project data for this drawing (optional)					size+0x80+padding	align(size + 0x80, 0x20)	0x586e0544		2						1
AcDb:AppInfo		information about the application that wrote the .dwg file		0x80				0x300						0x3fa0043e		2						1
AcDb:Preview		Bitmap preview for this drawing									0x400				0x400 or align(size, 0x20)	0x40aa0473		if properties encrypted	1
AcDb:SummaryInfo	fields like Title, Subject, Author								0x100				0x80						0x717a060f		if properties encrypted	1
AcDb:RevHistory		Revision history												0x7400				0x1000						0x60a205b3		0						4			compressed
AcDb:AcDbObjects	Database objects												0x7400				0xf800						0x674c05a9		if data encrypted		4			compressed
AcDb:ObjFreeSpace																	0x7400				0xf800						0x77e2061f		0						4			compressed
AcDb:Template		Template (Contains the MEASUREMENT system variable only)		0x7400				0x400						0x4a1404ce		0						4			compressed
AcDb:Handles		Handle list with offsets into the AcDb:AcDbObjects section		0x7400				0xf800						0x3f6e0450		if data encrypted		4			compressed
AcDb:Classes		Custom classes section											0x7400				0xf800						0x3f54045f		if data encrypted		4			compressed
AcDb:AuxHeader																		0x7400				0x800						0x54f0050a		0						4			compressed
AcDb:Header			drawing header variables										0x7400				0x800						0x32b803d9		if data encrypted		4			compressed
AcDb:Signature

The order in the stream is:
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

uint32	default_cols[256] = {
	0x000000,0xFF0000,0xFFFF00,0x00FF00,0x00FFFF,0x0000FF,0xFF00FF,0xFFFFFF,0x414141,0x808080,0xFF0000,0xFFAAAA,0xBD0000,0xBD7E7E,0x810000,0x815656,
	0x680000,0x684545,0x4F0000,0x4F3535,0xFF3F00,0xFFBFAA,0xBD2E00,0xBD8D7E,0x811F00,0x816056,0x681900,0x684E45,0x4F1300,0x4F3B35,0xFF7F00,0xFFD4AA,
	0xBD5E00,0xBD9D7E,0x814000,0x816B56,0x683400,0x685645,0x4F2700,0x4F4235,0xFFBF00,0xFFEAAA,0xBD8D00,0xBDAD7E,0x816000,0x817656,0x684E00,0x685F45,
	0x4F3B00,0x4F4935,0xFFFF00,0xFFFFAA,0xBDBD00,0xBDBD7E,0x818100,0x818156,0x686800,0x686845,0x4F4F00,0x4F4F35,0xBFFF00,0xEAFFAA,0x8DBD00,0xADBD7E,
	0x608100,0x768156,0x4E6800,0x5F6845,0x3B4F00,0x494F35,0x7FFF00,0xD4FFAA,0x5EBD00,0x9DBD7E,0x408100,0x6B8156,0x346800,0x566845,0x274F00,0x424F35,
	0x3FFF00,0xBFFFAA,0x2EBD00,0x8DBD7E,0x1F8100,0x608156,0x196800,0x4E6845,0x134F00,0x3B4F35,0x00FF00,0xAAFFAA,0x00BD00,0x7EBD7E,0x008100,0x568156,
	0x006800,0x456845,0x004F00,0x354F35,0x00FF3F,0xAAFFBF,0x00BD2E,0x7EBD8D,0x00811F,0x568160,0x006819,0x45684E,0x004F13,0x354F3B,0x00FF7F,0xAAFFD4,
	0x00BD5E,0x7EBD9D,0x008140,0x56816B,0x006834,0x456856,0x004F27,0x354F42,0x00FFBF,0xAAFFEA,0x00BD8D,0x7EBDAD,0x008160,0x568176,0x00684E,0x45685F,
	0x004F3B,0x354F49,0x00FFFF,0xAAFFFF,0x00BDBD,0x7EBDBD,0x008181,0x568181,0x006868,0x456868,0x004F4F,0x354F4F,0x00BFFF,0xAAEAFF,0x008DBD,0x7EADBD,
	0x006081,0x567681,0x004E68,0x455F68,0x003B4F,0x35494F,0x007FFF,0xAAD4FF,0x005EBD,0x7E9DBD,0x004081,0x566B81,0x003468,0x455668,0x00274F,0x35424F,
	0x003FFF,0xAABFFF,0x002EBD,0x7E8DBD,0x001F81,0x566081,0x001968,0x454E68,0x00134F,0x353B4F,0x0000FF,0xAAAAFF,0x0000BD,0x7E7EBD,0x000081,0x565681,
	0x000068,0x454568,0x00004F,0x35354F,0x3F00FF,0xBFAAFF,0x2E00BD,0x8D7EBD,0x1F0081,0x605681,0x190068,0x4E4568,0x13004F,0x3B354F,0x7F00FF,0xD4AAFF,
	0x5E00BD,0x9D7EBD,0x400081,0x6B5681,0x340068,0x564568,0x27004F,0x42354F,0xBF00FF,0xEAAAFF,0x8D00BD,0xAD7EBD,0x600081,0x765681,0x4E0068,0x5F4568,
	0x3B004F,0x49354F,0xFF00FF,0xFFAAFF,0xBD00BD,0xBD7EBD,0x810081,0x815681,0x680068,0x684568,0x4F004F,0x4F354F,0xFF00BF,0xFFAAEA,0xBD008D,0xBD7EAD,
	0x810060,0x815676,0x68004E,0x68455F,0x4F003B,0x4F3549,0xFF007F,0xFFAAD4,0xBD005E,0xBD7E9D,0x810040,0x81566B,0x680034,0x684556,0x4F0027,0x4F3542,
	0xFF003F,0xFFAABF,0xBD002E,0xBD7E8D,0x81001F,0x815660,0x680019,0x68454E,0x4F0013,0x4F353B,0x333333,0x505050,0x696969,0x828282,0xBEBEBE,0xFFFFFF,
};

//missing entities:
//HELIX?
//LIGHT?
//MESH?
//SECTION?
//SUN?
//SURFACE
//TABLE
//UNDERLAY
//WIPEOUT
//ARC_DIMENSION
//LARGE_RADIAL_DIMENSION


//missing objects:
//DATATABLE
//DIMASSOC
//GEODATA
//LAYER_FILTER
//LIGHTLIST
//OBJECT_PTR
//RENDERENVIRONMENT
//MENTALRAYRENDERSETTINGS
//RENDERGLOBAL
//SECTION
//UNDERLAYDEFINITION
//WIPEOUTVARIABLE

enum OBJECTTYPE : uint16 {
	UNUSED					= 0x00,		//	
	TEXT					= 0x01,		//	E
	ATTRIB					= 0x02,		//	E
	ATTDEF					= 0x03,		//	E
	BLOCK					= 0x04,		//	E
	ENDBLK					= 0x05,		//	E
	SEQEND					= 0x06,		//	E
	INSERT					= 0x07,		//	E
	MINSERT					= 0x08,		//	E
	//						= 0x09,		//	
	VERTEX_2D				= 0x0A,		//	E
	VERTEX_3D				= 0x0B,		//	E
	VERTEX_MESH				= 0x0C,		//	E
	VERTEX_PFACE			= 0x0D,		//	E
	VERTEX_PFACE_FACE		= 0x0E,		//	E
	POLYLINE_2D				= 0x0F,		//	E
	POLYLINE_3D				= 0x10,		//	E
	ARC						= 0x11,		//	E
	CIRCLE					= 0x12,		//	E
	LINE					= 0x13,		//	E
	DIMENSION_ORDINATE		= 0x14,		//	E
	DIMENSION_LINEAR		= 0x15,		//	E
	DIMENSION_ALIGNED		= 0x16,		//	E
	DIMENSION_ANG_PT3		= 0x17,		//	E
	DIMENSION_ANG_LN2		= 0x18,		//	E
	DIMENSION_RADIUS		= 0x19,		//	E
	DIMENSION_DIAMETER		= 0x1A,		//	E
	POINT					= 0x1B,		//	E
	FACE_3D					= 0x1C,		//	E
	POLYLINE_PFACE			= 0x1D,		//	E
	POLYLINE_MESH			= 0x1E,		//	E
	SOLID					= 0x1F,		//	E
	TRACE					= 0x20,		//	E
	SHAPE					= 0x21,		//	E
	VIEWPORT				= 0x22,		//	E
	ELLIPSE					= 0x23,		//	E
	SPLINE					= 0x24,		//	E
	REGION					= 0x25,		//	e
	SOLID_3D				= 0x26,		//	E
	BODY					= 0x27,		//	e
	RAY						= 0x28,		//	E
	XLINE					= 0x29,		//	E
	DICTIONARY				= 0x2A,		//	O
	OLEFRAME				= 0x2B,		//	e
	MTEXT					= 0x2C,		//	E
	LEADER					= 0x2D,		//	E
	TOLERANCE				= 0x2E,		//	e
	MLINE					= 0x2F,		//	E

	BLOCK_CONTROL_OBJ		= 0x30,		//
	BLOCK_HEADER			= 0x31,		//
	LAYER_CONTROL_OBJ		= 0x32,		//
	LAYER					= 0x33,		//
	STYLE_CONTROL_OBJ		= 0x34,		//
	STYLE					= 0x35,		//
	//						= 0x36,		//
	//						= 0x37,		//
	LTYPE_CONTROL_OBJ		= 0x38,		//
	LTYPE					= 0x39,		//
	//						= 0x3A,		//
	//						= 0x3B,		//
	VIEW_CONTROL_OBJ		= 0x3C,		//
	VIEW					= 0x3D,		//
	UCS_CONTROL_OBJ			= 0x3E,		//
	UCS						= 0x3F,		//
	VPORT_CONTROL_OBJ		= 0x40,		//
	VPORT					= 0x41,		//
	APPID_CONTROL_OBJ		= 0x42,		//
	APPID					= 0x43,		//
	DIMSTYLE_CONTROL_OBJ	= 0x44,		//
	DIMSTYLE				= 0x45,		//
	VP_ENT_HDR_CTRL_OBJ		= 0x46,		//
	VP_ENT_HDR				= 0x47,		//

	GROUP					= 0x48,		//	O
	MLINESTYLE				= 0x49,		//	O
	OLE2FRAME				= 0x4A,		//	e
	//						= 0x4B,		//
	LONG_TRANSACTION		= 0x4C,		//
	LWPOLYLINE				= 0x4D,		//	E
	HATCH					= 0x4E,		//	E
	XRECORD					= 0x4F,		//	o
	ACDBPLACEHOLDER			= 0x50,		//	o	aka PLACEHOLDER
	VBA_PROJECT				= 0x51,		//	o
	LAYOUT					= 0x52,		//	o

	IMAGE					= 0x65,		//	E
	IMAGEDEF				= 0x66,		//	O

	ACAD_PROXY_ENTITY		= 0x1f2,	//	e
	ACAD_PROXY_OBJECT		= 0x1f3,	//	o
	_LOOKUP					= 0x1f4,	//=500,

	// non-fixed types:
	ACAD_TABLE				= 0x8000,
	CELLSTYLEMAP,
	DBCOLOR,
	DICTIONARYVAR,			//	O
	DICTIONARYWDFLT,		//	O
	FIELD,					//	O
	IDBUFFER,				//	o
	IMAGEDEFREACTOR,		//	o
	LAYER_INDEX,			//	o
	LWPLINE,
	MATERIAL,				//	o
	MLEADER,				//	e
	MLEADERSTYLE,			//	o
	PLACEHOLDER,
	PLOTSETTINGS,			//	O
	RASTERVARIABLES,		//	o
	SCALE,
	SORTENTSTABLE,			//	o
	SPATIAL_FILTER,			//	o
	SPATIAL_INDEX,			//	o
	TABLEGEOMETRY,
	TABLESTYLES,
	VISUALSTYLE,			//	o
	WIPEOUTVARIABLE,

	ACDBDICTIONARYWDFLT,	//	O	aka DICTIONARYWDFLT
	TABLESTYLE,				//	O
	EXACXREFPANELOBJECT,
	NPOCOLLECTION,
	ACDBSECTIONVIEWSTYLE,
	ACDBDETAILVIEWSTYLE,
	ACDB_BLKREFOBJECTCONTEXTDATA_CLASS,
	ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS,
};

using ALL_OBJECTTYPES = meta::value_list<OBJECTTYPE,
	UNUSED,				TEXT,			ATTRIB,			ATTDEF,			BLOCK,		ENDBLK,		SEQEND,
	INSERT,				MINSERT,
	VERTEX_2D,			VERTEX_3D,		VERTEX_MESH,	VERTEX_PFACE,	VERTEX_PFACE_FACE,
	POLYLINE_2D,		POLYLINE_3D,	ARC,			CIRCLE,			LINE,
	DIMENSION_ORDINATE,	DIMENSION_LINEAR,	DIMENSION_ALIGNED,	DIMENSION_ANG_PT3,	DIMENSION_ANG_LN2,	DIMENSION_RADIUS,	DIMENSION_DIAMETER,
	POINT,	FACE_3D,	POLYLINE_PFACE,	POLYLINE_MESH,	SOLID,	TRACE,	SHAPE,	VIEWPORT,	ELLIPSE,	SPLINE,	REGION,	SOLID_3D,	BODY,	RAY,	XLINE,

	DICTIONARY,	OLEFRAME,	MTEXT,	LEADER,	TOLERANCE,	MLINE,
	BLOCK_CONTROL_OBJ,	BLOCK_HEADER,
	LAYER_CONTROL_OBJ,	LAYER,
	STYLE_CONTROL_OBJ,	STYLE,
	LTYPE_CONTROL_OBJ,	LTYPE,
	VIEW_CONTROL_OBJ,	VIEW,
	UCS_CONTROL_OBJ,	UCS,
	VPORT_CONTROL_OBJ,	VPORT,
	APPID_CONTROL_OBJ,	APPID,
	DIMSTYLE_CONTROL_OBJ,	DIMSTYLE,
	VP_ENT_HDR_CTRL_OBJ,	VP_ENT_HDR,
	GROUP,	MLINESTYLE,	OLE2FRAME,	LONG_TRANSACTION,
	LWPOLYLINE,	HATCH,	XRECORD,	ACDBPLACEHOLDER,	VBA_PROJECT,	LAYOUT,	IMAGE,	IMAGEDEF,	ACAD_PROXY_ENTITY,	ACAD_PROXY_OBJECT,
	ACAD_TABLE,	CELLSTYLEMAP,	DBCOLOR,	DICTIONARYVAR,	DICTIONARYWDFLT,	FIELD,	IDBUFFER,	IMAGEDEFREACTOR,	LAYER_INDEX,	LWPLINE,
	MATERIAL,	MLEADER,	MLEADERSTYLE,	PLACEHOLDER,	PLOTSETTINGS,	RASTERVARIABLES,	SCALE,	SORTENTSTABLE,	SPATIAL_FILTER,	SPATIAL_INDEX,
	TABLEGEOMETRY,	TABLESTYLES,	VISUALSTYLE,	WIPEOUTVARIABLE,
	ACDBDICTIONARYWDFLT,	TABLESTYLE,	EXACXREFPANELOBJECT,	NPOCOLLECTION,	ACDBSECTIONVIEWSTYLE,	ACDBDETAILVIEWSTYLE,	ACDB_BLKREFOBJECTCONTEXTDATA_CLASS,	ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS
>;

//template<class L, OBJECTTYPE...T> void switchT(L &&lambda, OBJECTTYPE t, meta::value_list<OBJECTTYPE, T...>) {
//	bool	unused[] = {t == T && (lambda.template operator()<T>(), true)...};
//}
//template<typename R, class L, OBJECTTYPE...T> R switchT(L &&lambda, OBJECTTYPE t, meta::value_list<OBJECTTYPE, T...>) {
//	R		result;
//	bool	unused[] = {t == T && ((result = lambda.template operator()<T>()), true)...};
//	return result;
//}
//template<class L, typename E, E...e> void switchT(L &&lambda, E i, meta::value_list<E, e...>) {
//	bool	unused[] = {i == e && (lambda.template operator()<e>(), true)...};
//}
//template<typename R, class L, typename E, E...e> R switchT(L &&lambda, E i, meta::value_list<E, e...>) {
//	R		result;
//	bool	unused[] = {i == e && ((result = lambda.template operator()<e>()), true)...};
//	return result;
//}

struct TypeNames {
	uint32	start, num;
	const char **names;
};

TypeNames	type_names[] = {
	{0, 0x53, (const char *[]) {
		"UNUSED",				// 0x00
		"TEXT",					// 0x01
		"ATTRIB",				// 0x02
		"ATTDEF",				// 0x03
		"BLOCK",				// 0x04
		"ENDBLK",				// 0x05
		"SEQEND",				// 0x06
		"INSERT",				// 0x07
		"MINSERT",				// 0x08
		"0x09",					// 0x09
		"VERTEX_2D",			// 0x0A
		"VERTEX_3D",			// 0x0B
		"VERTEX_MESH",			// 0x0C
		"VERTEX_PFACE",			// 0x0D
		"VERTEX_PFACE_FACE",	// 0x0E
		"POLYLINE_2D",			// 0x0F
		"POLYLINE_3D",			// 0x10
		"ARC",					// 0x11
		"CIRCLE",				// 0x12
		"LINE",					// 0x13
		"DIMENSION_ORDINATE",	// 0x14
		"DIMENSION_LINEAR",		// 0x15
		"DIMENSION_ALIGNED",	// 0x16
		"DIMENSION_ANG_PT3",	// 0x17
		"DIMENSION_ANG_LN2",	// 0x18
		"DIMENSION_RADIUS",		// 0x19
		"DIMENSION_DIAMETER",	// 0x1A
		"POINT",				// 0x1B
		"FACE_3D",				// 0x1C
		"POLYLINE_PFACE",		// 0x1D
		"POLYLINE_MESH",		// 0x1E
		"SOLID",				// 0x1F
		"TRACE",				// 0x20
		"SHAPE",				// 0x21
		"VIEWPORT",				// 0x22
		"ELLIPSE",				// 0x23
		"SPLINE",				// 0x24
		"REGION",				// 0x25
		"SOLID_3D",				// 0x26
		"BODY",					// 0x27
		"RAY",					// 0x28
		"XLINE",				// 0x29
		"DICTIONARY",			// 0x2A
		"OLEFRAME",				// 0x2B
		"MTEXT",				// 0x2C
		"LEADER",				// 0x2D
		"TOLERANCE",			// 0x2E
		"MLINE",				// 0x2F
		"BLOCK_CONTROL_OBJ",	// 0x30
		"BLOCK_HEADER",			// 0x31
		"LAYER_CONTROL_OBJ",	// 0x32
		"LAYER",				// 0x33
		"STYLE_CONTROL_OBJ",	// 0x34
		"STYLE",				// 0x35
		" 0x36",				// 0x36
		" 0x37",				// 0x37
		"LTYPE_CONTROL_OBJ",	// 0x38
		"LTYPE",				// 0x39
		"0x3A",					// 0x3A
		"0x3B",					// 0x3B
		"VIEW_CONTROL_OBJ",		// 0x3C
		"VIEW",					// 0x3D
		"UCS_CONTROL_OBJ",		// 0x3E
		"UCS",					// 0x3F
		"VPORT_CONTROL_OBJ",	// 0x40
		"VPORT",				// 0x41
		"APPID_CONTROL_OBJ",	// 0x42
		"APPID",				// 0x43
		"DIMSTYLE_CONTROL_OBJ",	// 0x44
		"DIMSTYLE",				// 0x45
		"VP_ENT_HDR_CTRL_OBJ",	// 0x46
		"VP_ENT_HDR",			// 0x47
		"GROUP",				// 0x48
		"MLINESTYLE",			// 0x49
		"OLE2FRAME",			// 0x4A
		"0x4B",					// 0x4B
		"LONG_TRANSACTION",		// 0x4C
		"LWPOLYLINE",			// 0x4D
		"HATCH",				// 0x4E
		"XRECORD",				// 0x4F
		"ACDBPLACEHOLDER",		// 0x50
		"VBA_PROJECT",			// 0x51
		"LAYOUT",				// 0x52
	}},
	{IMAGE, 2, (const char *[]) {
		"IMAGE",
		"IMAGEDEF"
	}},
	{ACAD_PROXY_ENTITY, 2, (const char *[]) {
		"ACAD_PROXY_ENTITY",
		"ACAD_PROXY_OBJECT",
	}},
	{0x8000, 32, (const char *[]) {
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
	}},
};

OBJECTTYPE TypeFromName(string_param name) {
	for (auto &i : type_names) {
		for (int j = 0; j < i.num; j++) {
			if (name == i.names[j])
				return OBJECTTYPE(i.start + j);
		}
	}
	return UNUSED;
}

 const char *NameFromType(OBJECTTYPE t) {
	 for (auto &i : type_names) {
		 if (t >= i.start && t < i.start + i.num)
			return i.names[t - i.start];
	 }
	return nullptr;
}

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

//-----------------------------------------------------------------------------
// reader
//-----------------------------------------------------------------------------
class bits_reader {
protected:
	const_memory_block	b;
	const uint8			*p;
	uint8				bit;
public:
	bits_reader(const const_memory_block &b)	: b(b), p(b), bit(0) {}
	size_t			remaining()					const	{ return (const uint8*)b.end() - p - int(bit > 0); }
	streamptr		tell()						const	{ return (p - (uint8*)b.p) + int(bit > 0); }
	bool			eof()						const	{ return p >= b.end(); }
	streamptr		tell_bit()					const	{ return (p - (uint8*)b.p) * 8 + bit; }
	void			seek_bit(streamptr offset)			{ p = (const uint8*)b.p + min(offset / 8, b.n); bit = offset & 7; }
	void			seek_cur_bit(streamptr offset)		{ seek_bit(tell_bit() + offset); }

	int				getc() {
		if (!remaining())
			return EOF;
		++p;
		return bit == 0
			? p[-1]
			: uint8(((p[-1] << 8) | p[0]) >> (8 - bit));
	}

	size_t			readbuff(void *buffer, size_t size) {
		size = min(size, remaining());
		if (bit == 0) {
			memcpy(buffer, p, size);
			p += size;
		} else {
			for (auto d = (uint8*)buffer, e = d + size; d < e; ++d, ++p)
				*d = ((p[0] << 8) | p[1]) >> (8 - bit);
		}
		return size;
	}

	bool get_bit() {
		bool	ret = (*p >> (7 - bit)) & 1;
		if (!(bit = (bit + 1) & 7))
			++p;
		return ret;
	}
	
	uint8 get_bits(int n) {
		ISO_ASSERT(n < 8);
		bit += n;

		uint8	ret = bit <= 8
			? (p[0] >> (8 - bit)) & bits(n)
			: (p[0] << (bit - 8)) | (p[1] >> (16 - bit));

		p += bit >> 3;
		bit &= 7;

		return ret & bits(n);
	}
	auto&	data() { return b; }
};

struct bitsin : bits_reader, reader<bitsin> {
	using bits_reader::remaining;
	VER		ver;
	uint32	size = 0;
	bitsin(const const_memory_block &file, VER ver) : bits_reader(file), ver(ver) {}
};

struct bitsin2 : bitsin {
	bitsin&	sbits;
	uint32 soffset = 0;
	bitsin2(const bitsin &bits)	: bitsin(bits), sbits(*this) {}
	bitsin2(const bitsin &bits, bitsin &sbits, uint32 soffset)	: bitsin(bits), sbits(sbits), soffset(soffset) { sbits.seek_bit(soffset); }

	bool	check_skip_strings() {
		if (soffset && tell_bit() != soffset)
			return false;
		if (this != &sbits) {
			return sbits.tell_bit() == size - 17;
		}
		return sbits.tell_bit() == size;
	}

	template<typename T> T		get()		{ T t; ISO_VERIFY(read(t)); return t; }
	template<typename T> bool	read(T &t)	{ using iso::read; return read(*this, t); }
	template<typename T, typename... TT> bool read(T &t, TT&... tt) { return iso::read(*this, t, tt...); }
	template<typename... TT> void discard() { bool	unused[] = {(get<TT>(), false)...}; }
};


struct bitsin3 : bitsin2 {
	bitsin&	hbits;
	bitsin3(const bitsin2 &bits)				: bitsin2(bits), hbits(*this) {}
	bitsin3(const bitsin2 &bits, bitsin &hbits)	: bitsin2(bits), hbits(hbits) {}

	template<typename T> T		get()		{ T t; ISO_VERIFY(read(t)); return t; }
	template<typename T> bool	read(T &t)	{ using iso::read; return read(*this, t); }
	template<typename T, typename... TT> bool read(T &t, TT&... tt) { return iso::read(*this, t, tt...); }
	template<typename... TT> void discard() { bool	unused[] = {(get<TT>(), false)...}; }
};

struct bit_seeker {
	bitsin& bits;
	streamptr	end;
	bit_seeker(bitsin& bits, uint32 size) : bits(bits), end(bits.tell_bit() + size) {}
	~bit_seeker() { bits.seek_bit(end); }
};

template<typename T, int V> struct ifver : T {
	template<typename R> bool	read(R &r)	{ return (V < 0 ? -r.ver : r.ver) < V || T::read(r); }
};

typedef uint8		uint8;
typedef uint16		RS;
typedef double		RD;
typedef uint32		RL;
typedef uint64		RLL;
typedef double2p	RD2;
typedef double3p	RD3;

// bit (1 or 0)
struct B {
	bool	v;
	B(bool v = false)	: v(v) {}
	bool	read(bitsin &in)	{ v = in.get_bit(); return true; }
	operator bool() const		{ return v; }
	friend bool get(const B &b)	{ return b; }
};

// bitshort (16 bits)
struct BS {
	uint16	v;
	BS(uint16 v = 0) : v(v) {}
	bool	read(bitsin &in)	{
		switch (in.get_bits(2)) {
			case 0: v = in.get<uint16>(); break;
			case 1: v = in.getc(); break;
			case 2: v = 0; break;
			case 3: v = 256; break;
		}
		return true;
	}
	operator uint16() const { return v; }
	friend uint16 get(const BS &b)	{ return b; }
};

// BS R2000+, uint8 on R13,R14
struct BSV : BS {
	BSV(uint16 v = 0) : BS(v) {}
	bool	read(bitsin &in)	{
		if (in.ver < R2000) {
			v = in.get<uint8>();
			return true;
		}
		return BS::read(in);
	}
	friend uint16 get(const BSV &b)	{ return b; }
};

// bitlong (32 bits)
struct BL {
	uint32	v;
	BL(uint32 v = 0) : v(v) {}
	bool	read(bitsin &in)	{
		switch (in.get_bits(2)) {
			default:
			case 0: v = in.get<uint32>(); return true;
			case 1: v = in.getc(); return true;
			case 2: v = 0; return true;
			case 3: v = in.get<uint16>(); return true;
		}
	}
	operator uint32() const { return v; }
	friend uint32 get(const BL &b)	{ return b; }
};

// bitlonglong (64 bits) (R24)
struct BLL {
	uint64	v;
	BLL(uint64 v = 0) : v(v) {}
	bool	read(bitsin &in)	{
		auto	n = in.get_bits(3);
		for (int i = 0; i < n; i++)
			v = (v << 8) | in.getc();
		return true;
	}
	operator uint64() const { return v; }
	friend uint64 get(const BLL &b)	{ return b; }
};

// bitdouble
struct BD {
	double	v;
	BD(double v = 0) : v(v) {}

	bool	read(bitsin &in)	{
		switch (in.get_bits(2)) {
			case 0: v = in.get<double>(); return true;
			case 1: v = 1; return true;
			default:
			case 2: v = 0; return true;
			//default: return false;
		}
	}
	operator double() const { return v; }
	friend double get(const BD &b)	{ return b; }
};

typedef array<BD,2> BD2;
typedef array<BD,3> BD3;

// BitDouble With Default
double DD(bitsin& in, double v) {
	auto	p = (uint8*)&v;
	switch (in.get_bits(2)) {
		case 0: break;
		case 1:
			*(uint32*)p			= in.get<uint32>();
			break;
		case 2:
			*((uint16*)(p + 4)) = in.get<uint16>();
			*((uint32*)p)		= in.get<uint32>();
			break;
		case 3:
			v = in.get<double>();
			break;
	}
	return v;
}

bool DD(bitsin& in, double *out, const double *def, int n) {
	while (n--)
		*out++ = DD(in, *def++);
	return true;
}

// modular char
struct MC {
	uint64	v;
	MC(uint64 v = 0) : v(v) {}

	template<typename R> bool	read(R& file) {
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

// modular char (signed)
struct MCS {
	int64	v;
	MCS(uint64 v = 0) : v(v) {}

	template<typename R> bool	read(R& file) {
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

// Variable text, T for 2004 and earlier files, TU for 2007+ files.
struct TV : string16 {
	TV(string_param16 v = none) : string16(v) {}

	bool	read(bitsin2 &in)	{
		uint32	len = in.sbits.get<BS>();
		resize(len);
		for (int i = 0; i < len; i++)
			(*this)[i] = in.ver >= R2007 ? in.sbits.get<uint16>() : in.sbits.get<uint8>();
		return true;
	}
	friend string get(const TV &b)	{ return b; }
};

// BitExtrusion
struct BEXT : BD3 {
	bool	read(bitsin &in)	{
		if (in.ver >= R2000 && in.get_bit())
			return true;
		return BD3::read(in);
	}
};

// BitScale
struct BSCALE : BD3 {
	BSCALE() : BD3{1,1,1} {}

	bool	read(bitsin &bits)	{
		if (bits.ver <= R14)
			return BD3::read(bits);

		switch (bits.get_bits(2)) {
			case 0:
				(*this)[0] = (double)bits.get<RD>();
				// fallthrough
			case 1: //x default value 1, y & z can be x value
				(*this)[1] = DD(bits, (*this)[0]);
				(*this)[2] = DD(bits, (*this)[0]);
				break;
			case 2:
				(*this)[0] = (*this)[1] = (*this)[2] = (double)bits.get<RD>();
				// fallthrough
			case 3:
				break;
		}
		return true;
	}
};


// BitThickness
struct BT : BD {
	bool	read(bitsin &in)	{
		if (in.ver >= R2000 && in.get_bit())
			return true;
		return BD::read(in);
	}
};

// Object type
OBJECTTYPE readOT(bitsin &in)	{
	if (in.ver < R2007)
		return (OBJECTTYPE)in.get<uint16>();
	switch (in.get_bits(2)) {
		case 0:		return (OBJECTTYPE)in.get<uint8>(); break;
		case 1:		return (OBJECTTYPE)(in.get<uint8>() + 0x1f0); break;
		default:	return (OBJECTTYPE)in.get<uint16>(); break;
	}
}


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
		bitfield<uint32, 8, 24>	offset2;
	};

	H(uint32 u = 0)	: u(u) {}

	bool	read(bitsin &in)	{
		code_size = in.get<uint8>();
		for (int i = 0; i < size; i++)
			offset = (offset << 8) | in.get<uint8>();
		return true;
	}
	bool	read(bitsin3 &in)		{
		return read(in.hbits);
	}
	operator uint32() const			{ return offset; }
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

class DWG;
struct Entity;

struct HandleRange {
	dynamic_array<H>	handles;	//pre 2004, firstEH, lastEH, seqendH; else array + seqendH
	bool	read(bitsin& bits, int count) {
		return count < 0 || handles.read(bits, bits.ver >= R2004 ? count + 1 : 3);
	}
	H	endH()	const {
		return handles.back();
	}

	struct iterator {
		DWG			*dwg;
		uint32		h;
		const H		*p;
		Entity		*ent;
		uint32		next;
		
		iterator(DWG *dwg, const HandleRange &range, uint32 h, bool end);
		bool		operator!=(const iterator &b)	const;
		iterator&	operator++();
		Entity*		operator*();
	};

	struct Container {
		DWG			*dwg;
		const HandleRange &range;
		uint32		h;
		Container(DWG *dwg, const HandleRange &range, uint32 h) : dwg(dwg), range(range), h(h) {}
		iterator	begin()	const { return {dwg, range, h, false}; }
		iterator	end()	const { return {dwg, range, h, true}; }
	};

	Container	contents(DWG *dwg, uint32 h) const { return {dwg, *this, h}; }
};


template<typename T> struct HandleCollection : HandleRange::Container {
	struct iterator2 : HandleRange::iterator {
		typedef HandleRange::iterator B;
		iterator2(const HandleCollection &c, bool end) : B(c.dwg, c.range, c.h, end) {}
		auto		operator*()		{ return make_param_element(*B::operator*(), dwg); }
		iterator2&	operator++()	{ B::operator++(); return *this; }
	};

	HandleCollection(DWG *dwg, const HandleRange &range, uint32 h) : Container(dwg, range, h) {}
	iterator2	begin()				const { return {*this, false}; }
	iterator2	end()				const { return {*this, true}; }
	auto		operator[](int i)	const { return *nth(begin(), i); }
};

struct CMC {
	enum TYPE {
		ByLayer = 0xC0,
		ByBlock	= 0xC1,
		RGB		= 0xC2,
		ACIS	= 0xC3,
	};

	BS		index;
	BL		rgb;
	uint8	name_type = 0;
	TV		name;

	bool	read(bitsin2 &in) {
		index.read(in);
		if (in.ver >= R2004) {
			in.read(rgb);
			if (name_type = in.get<uint8>())
				name.read(in);
		}
		return true;
	}
};

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

	bool	read(bitsin &in)	{
		in.read(flags);
		if (flags.v & Complex) {
			rgb.read(in);
			if (flags.v & AcDbRef)
				h.read(in);
		}
		if (flags.v & Transparency)
			transparency.read(in);
		return true;
	}

};

struct TIME {
	BL	day, msec;
	bool	read(bitsin &in)	{
		return day.read(in) && msec.read(in);
	}
};

struct RenderMode {
	uint8	mode;
	B		use_default_lights;
	uint8	default_lighting_type;
	BD		brightness;
	BD		contrast;
	CMC		ambient;
	bool	read(bitsin2 &bits) {
		return bits.read(mode) && (bits.ver <= R2004 || (bits.read(use_default_lights, default_lighting_type, brightness, contrast) && ambient.read(bits)));
	}
};

struct UserCoords {
	BD3		origin, xdir, ydir;
	BD		elevation;
	BS		ortho_view_type;
	bool read(bitsin &bits) {
		return bits.read(origin, xdir, ydir) && (bits.ver < R2000 || bits.read(elevation, ortho_view_type));
	}
};

struct Gradient {
	struct Entry {
		BD		unkDouble;
		BS		unkShort;
		BL		rgbCol;
		uint8	ignCol;
		bool read(bitsin &bits) {
			return bits.read(unkDouble, unkShort, rgbCol, ignCol);
		}
	};
	BL		isGradient;
	BL		res;
	BD		gradAngle;
	BD		gradShift;
	BL		singleCol;
	BD		gradTint;
	dynamic_array<Entry>	entries;

	bool	read(bitsin &bits) {
		return bits.read(isGradient, res, gradAngle, gradShift, singleCol, gradTint)
			&& entries.read(bits, bits.get<BL>());
		return true;
	}
};

enum CONTENT_PROPS {
	DataType				= 1 << 0,
	DataFormat				= 1 << 1,
	Rotation				= 1 << 2,
	BlockScale				= 1 << 3,
	Alignment				= 1 << 4,
	ContentColor			= 1 << 5,
	TextStyle				= 1 << 6,
	TextHeight				= 1 << 7,
	AutoScale				= 1 << 8,
	//Cell style properties:
	BackgroundColor			= 1 << 9,
	MarginLeft				= 1 << 10,
	MarginTop				= 1 << 11,
	MarginRight				= 1 << 12,
	MarginBottom			= 1 << 13,
	ContentLayout			= 1 << 14,
	MarginHorizontalSpacing	= 1 << 17,
	MarginVerticalSpacing	= 1 << 18,
	//Row/column properties:
	MergeAll				= 1 << 15,
	//Table properties:
	FlowBottomToTop			= 1 << 16,
};

struct ValueSpec {
	enum DATA_TYPE {//Varies by type: Not present in case bit 1 in Flags is set
		Unknown		= 0,	//Unknown BL
		Long		= 1,	//Long BL
		Double		= 2,	//Double BD
		String		= 4,	//String TV
		Date		= 8,	//Date BL data size N, followed by N bytes (Int64 value)
		Point2D		= 16,	//Point BL data size, followed by 2RD 
		Point3D		= 32,	//3D Point BL data size, followed by 3RD
		Object		= 64,	//Object Id H Read from appropriate place in handles section (soft pointer).
		BufferUnk	= 128,	//Buffer Unknown.
		BufferRes	= 256,	//Result Buffer Unknown.
		General		= 512,	//General General, BL containing the byte count followed by a byte array. (introduced in R2007, use Unknown before R2007).
	};
	enum UNIT_TYPE {
		no_units	= 0,
		distance	= 1,
		angl		= 2,
		area		= 4,
		volume		= 7,
	};

	BL		flags		= 0;
	BL		data_type;
	BL		unit_type;
	TV		format;

	bool read(bitsin2 &bits) {
		if (bits.ver < R2007) {
			bits.read(data_type);
		} else {
			bits.read(flags);//Flags BL 93 Flags & 0x01 => type is kGeneral
			if (!(flags & 1))
				bits.read(data_type);
			bits.read(unit_type, format);
		}
		return true;
	}
};

struct Value : ValueSpec {
	TV		value;
	bool read(bitsin2 &bits) {
		return ValueSpec::read(bits) && (bits.ver < R2007 || bits.read(value));
	}
};

struct ContentFormat {
	enum ALIGN {
		TopLeft			= 1,
		TopCenter		= 2,
		TopRight		= 3,
		MiddleLeft		= 4,
		MiddleCenter	= 5,
		MiddleRight		= 6,
		BottomLeft		= 7,
		BottomCenter	= 8,
		BottomRight		= 9
	};
	BL		PropertyOverrideFlags;
	BL		PropertyFlags;// Contains property bit values for property Auto Scale only (0x100).
	BL		data_type;
	BL		unit_type;
	TV		format;

	BD		rotation;
	BD		scale;
	BL		alignment;
	CMC		color;
	H		TextStyle;
	BD		TextHeight;

	bool read(bitsin2 &bits) {
		return bits.read(PropertyOverrideFlags, PropertyFlags, data_type, unit_type, format, rotation, scale, alignment, color/*, TextStyle*/, TextHeight);
	}
};

struct RowStyle {
	struct Border {
		BS		line_weight;
		B		visible;
		CMC		colour;
		bool	read(bitsin2 &bits) { return bits.read(line_weight, visible, colour); }
	};
	H		text_style;
	BD		text_height;
	BS		text_align;
	CMC		text_colour, fill_colour;
	B		bk_color_enabled;
	Border	top, horizontal, bottom, left, vertical, right;
	//2007+
	BL		data_type, data_unit_type;
	TV		format;

	bool	read(bitsin3 &bits) {
		return bits.read(text_style, text_height, text_align, text_colour, fill_colour, bk_color_enabled)
			&& (bits.ver < R2007 || bits.read(data_type, data_unit_type, format));
	}
};


struct CellStyle {
	enum ID {
		title	= 1,
		header	= 2,
		data	= 3,
		table	= 4,
		Custom	= 101,//cell style ID’s are numbered starting at 101
	};
	enum CLASS {
		ClassData	= 1,
		ClassLabel	= 2,
	};
	enum STYLE {
		Cell = 1,
		Row = 2,
		Column = 3,
		Formatted = 4,
		Table = 5
	};
	enum LAYOUT_FLAGS {
		Flow = 1,
		StackedHorizontal = 2,
		StackedVertical = 4
	};
	enum EDGE_FLAGS {
		top			= 1 << 0,
		right		= 1 << 1,
		bottom		= 1 << 2,
		left		= 1 << 3,
		vertical	= 1 << 4,
		horizontal	= 1 << 5,
	};
	enum BORDER_FLAGS {
		BorderTypes = 0x1,
		LineWeight = 0x2,
		LineType = 0x4,
		Color = 0x8,
		Invisibility = 0x10,
		DoubleLineSpacing = 0x20
	};
	enum BORDER_TYPE {
		Single = 1,
		Double = 2
	};

	struct Border {
		BL	EdgeFlags;
		BL	BorderPropertyOverride;
		BL	BorderType;
		CMC	Color;
		BL	LineWeight;
		H	LineLtype;
		BL	Invisibility;//: 1 = invisible, 0 = visible.
		BD	DoubleLineSpacing;

		bool	read(bitsin3 &bits) {
			bits.read(EdgeFlags);
			return EdgeFlags == 0 || bits.read(BorderPropertyOverride, BorderType, Color, LineWeight, LineLtype, Invisibility, DoubleLineSpacing);
		}
	};

	BL		style_type;
	BL		PropertyOverrideFlags;
	BL		MergeFlags;				// only for bits 0x8000 and 0x10000
	CMC		BackgroundColor;
	BL		ContentLayoutFlags;
	ContentFormat	ContentFormat;
	BD		VerticalMargin, HorizontalMargin, BottomMargin, RightMargin, MarginHorizontalSpacing, MarginVerticalSpacing;
	dynamic_array<Border>	borders;

	BL	id;
	BL	type;
	TV	name;

	bool	read(bitsin3 &bits) {
		bits.read(style_type);
		if (bits.get<BS>()) {
			bits.read(PropertyOverrideFlags, MergeFlags, BackgroundColor, ContentLayoutFlags, ContentFormat);
			if (bits.get<BS>() & 1)
				bits.read(VerticalMargin, HorizontalMargin, BottomMargin, RightMargin, MarginHorizontalSpacing, MarginVerticalSpacing);
			borders.read(bits, bits.get<BL>());
		}
		return bits.read(id, type, name);
	}
};


//-----------------------------------------------------------------------------
// DimStyle
//-----------------------------------------------------------------------------

struct DimStyle {
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
	uint32	flags;
	TV		DIMPOST, DIMAPOST;
	TV		DIMBLK, DIMBLK1, DIMBLK2, DIMALTMZS, DIMMZS;
	BSV		DIMALTD, DIMZIN, DIMTOLJ, DIMJUST,DIMFIT, DIMTZIN, DIMALTZ, DIMALTTZ, DIMTAD;

	uint8	DIMUNIT;	//r13/14 only

	BS		DIMAUNIT, DIMDEC, DIMTDEC, DIMALTU, DIMALTTD;
	BD		DIMSCALE, DIMASZ, DIMEXO, DIMDLI, DIMEXE, DIMRND, DIMDLE, DIMTP, DIMTM, DIMTXT, DIMCEN, DIMTSZ, DIMALTF, DIMLFAC, DIMTVP, DIMTFAC,DIMGAP;
	CMC		DIMCLRD, DIMCLRE, DIMCLRT;

	//>13,14
	BD		DIMFXL, DIMJOGANG;
	BS		DIMTFILL;
	CMC		DIMTFILLCLR;
	BS		DIMAZIN, DIMARCSYM;
	BD		DIMALTRND;
	BS		DIMADEC, DIMFRAC, DIMLUNIT, DIMDSEP, DIMTMOVE;
	BD		DIMALTMZF;
	BS		DIMLWD, DIMLWE;
	BD		DIMMZF;
	H		DIMTXSTY, DIMLDRBLK;
	H		HDIMBLK, HDIMBLK1, HDIMBLK2, DIMLTYPE, DIMLTEX1, DIMLTEX2;

	bool parse(bitsin3 &bits) {
		//	R13 & R14 Only:
		if (bits.ver <= R14) {
			flags = bits.getc() | (bits.get_bits(3) << 8);
			bits.read(DIMALTD, DIMZIN);
			flags |= bits.get_bits(2) * DIMSD1;
			bits.read(DIMTOLJ, DIMJUST, DIMFIT);
			flags |= bits.get_bits(1) * DIMUPT;
			bits.read(DIMTZIN, DIMALTZ,DIMALTTZ, DIMTAD, DIMUNIT, DIMAUNIT, DIMDEC, DIMTDEC, DIMALTU, DIMALTTD);

		}
		bits.read(DIMPOST, DIMAPOST, DIMSCALE, DIMASZ, DIMEXO, DIMDLI, DIMEXE, DIMRND, DIMDLE, DIMTP, DIMTM);

		if (bits.ver >= R2007)
			bits.read(DIMFXL, DIMJOGANG, DIMTFILL, DIMTFILLCLR);

		if (bits.ver >= R2000) {
			flags = bits.get_bits(6);
			bits.read(DIMTAD, DIMZIN, DIMAZIN);
		}
		if (bits.ver >= R2007)
			bits.read(DIMARCSYM);

		bits.read(DIMTXT, DIMCEN, DIMTSZ, DIMALTF, DIMLFAC, DIMTVP, DIMTFAC, DIMGAP);

		if (bits.ver <= R14) {
			bits.read(DIMPOST, DIMAPOST, DIMBLK, DIMBLK1, DIMBLK2);
		} else {
			bits.read(DIMALTRND);
			flags |= bits.get_bit() * DIMALT;
			bits.read(DIMALTD);
			flags |= bits.get_bits(4) * DIMTOFL;
		}
		
		bits.read(DIMCLRD, DIMCLRE, DIMCLRT);

		if (bits.ver >= R2000) {
			bits.read(DIMADEC, DIMDEC, DIMTDEC, DIMALTU, DIMALTTD, DIMAUNIT, DIMFRAC, DIMLUNIT, DIMDSEP, DIMTMOVE, DIMJUST);
			flags |= bits.get_bits(2) * DIMSD1;
			bits.read(DIMTOLJ, DIMTZIN, DIMALTZ, DIMALTTZ);
			flags |= bits.get_bit() * DIMUPT;
			bits.read(DIMFIT);

			if (bits.ver >= R2007)
				flags |= bits.get_bit() * DIMFXLON;

			if (bits.ver >= R2010) {
				flags |= bits.get_bit() * DIMTXTDIRECTION;
				bits.read(DIMALTMZF, DIMALTMZS, DIMMZS, DIMMZF);
			}

			//handles
			read(bits, DIMTXSTY, DIMLDRBLK, HDIMBLK, HDIMBLK1, HDIMBLK2);
			if (bits.ver >= R2007)
				read(bits, DIMLTYPE, DIMLTEX1, DIMLTEX2);
			bits.read(DIMLWD, DIMLWE);
		}
		return true;
	}
};

//-----------------------------------------------------------------------------
// HeaderVars
//-----------------------------------------------------------------------------

struct HeaderVars {
	struct UCSstuff {
		BD3		INSBASE, EXTMIN, EXTMAX;
		RD2		LIMMIN, LIMMAX;
		BD		ELEVATION;
		BD3		ORG, XDIR, YDIR;
		H		NAME;
		//R2000+
		H		ORTHOREF;
		BS		ORTHOVIEW;
		H		BASE;
		BD3		ORGTOP, ORGBOTTOM, ORGLEFT, ORGRIGHT, ORGFRONT, ORGBACK;

		bool read(bitsin3 &bits) {
			return bits.read(INSBASE, EXTMIN, EXTMAX, LIMMIN, LIMMAX, ELEVATION, ORG, XDIR, YDIR, NAME)
				&& (bits.ver < R2000 || bits.read(ORTHOREF,ORTHOVIEW,BASE,ORGTOP, ORGBOTTOM, ORGLEFT, ORGRIGHT, ORGFRONT, ORGBACK));
		}
	};

	ifver<BLL,+R2013>	requiredVersions;

	B				DIMASO, DIMSHO;
	ifver<B,-R14>	DIMSAV;
	B				PLINEGEN, ORTHOMODE, REGENMODE, FILLMODE, QTEXTMODE, PSLTSCALE, LIMCHECK, BLIPMODE, USRTIMER, SKPOLY, ANGDIR, SPLFRAME;
	ifver<B,-R14>	ATTREQ, ATTDIA;
	B				MIRRTEXT, WORLDVIEW;
	ifver<B,-R14>	WIREFRAME;
	B				TILEMODE, PLIMCHECK, VISRETAIN;
	ifver<B,-R14>	DELOBJ;
	B				DISPSILH, PELLIPSE;
	BS				PROXIGRAPHICS;
	ifver<BS,-R14>	DRAGMODE;//RLZ short or bit??
	BS				TREEDEPTH, LUNITS, LUPREC, AUNITS, AUPREC;
	ifver<BS,-R14>	OSMODE;
	BS				ATTMODE;
	ifver<BS,-R14>	COORDS;
	BS				PDMODE;
	ifver<BS,-R14>	PICKSTYLE;

	BS				USERI1, USERI2, USERI3, USERI4, USERI5, SPLINESEGS, SURFU, SURFV, SURFTYPE, SURFTAB1, SURFTAB2, SPLINETYPE, SHADEDGE, SHADEDIF, UNITMODE, MAXACTVP, ISOLINES, CMLJUST, TEXTQLTY;
	BD				LTSCALE, TEXTSIZE, TRACEWID, SKETCHINC, FILLETRAD, THICKNESS, ANGBASE, PDSIZE, PLINEWID, USERR1, USERR2, USERR3, USERR4, USERR5, CHAMFERA, CHAMFERB, CHAMFERC, CHAMFERD, FACETRES, CMLSCALE, CELTSCALE;
	TV				MENU;

	TIME			TDCREATE, TDUPDATE, TDINDWG, TDUSRTIMER;
	CMC				CECOLOR;

	H				HANDSEED;//always present in data stream
	H				CLAYER, TEXTSTYLE, CELTYPE;
	ifver<H,+R2007>	CMATERIAL;
	H				DIMSTYLE,  CMLSTYLE;

	ifver<BD,+R2000> PSVPSCALE;
	
	UCSstuff		PUCS;
	UCSstuff		UCS;
	DimStyle		dim;

	H				BLOCK_CONTROL, LAYER_CONTROL, TEXTSTYLE_CONTROL, LINETYPE_CONTROL, VIEW_CONTROL, UCS_CONTROL, VPORT_CONTROL, APPID_CONTROL, DIMSTYLE_CONTROL;
	ifver<H,-R2000>	VP_ENT_HDR_CONTROL;
	H				GROUP_CONTROL, MLINESTYLE_CONTROL;

	//R2000+
	H				DICT_NAMED_OBJS;
	BS				TSTACKALIGN, TSTACKSIZE;
	TV				HYPERLINKBASE, STYLESHEET;
	H				LAYOUTS_CONTROL, PLOTSETTINGS_CONTROL, DICT_PLOTSTYLES;
	ifver<H,+R2004>	DICT_MATERIALS, DICT_COLORS;
	ifver<H,+R2007>	DICT_VISUALSTYLE;
	ifver<H,+R2013>	DICT_UNKNOWN;
	BS				INSUNITS, CEPSNTYPE;
	H				CPSNID;
	TV				FINGERPRINTGUID, VERSIONGUID;

	//R2004+
	uint8			SORTENTS, INDEXCTL, HIDETEXT, XCLIPFRAME, DIMASSOC, HALOGAP;
	BS				OBSCUREDCOLOR, INTERSECTIONCOLOR;
	uint8			OBSCUREDLTYPE, INTERSECTIONDISPLAY;
	TV				PROJECTNAME;

	//common
	H				BLOCK_PAPER_SPACE, BLOCK_MODEL_SPACE, LTYPE_BYLAYER, LTYPE_BYBLOCK, LTYPE_CONTINUOUS;

	//R2007+
	B				CAMERADISPLAY;
	BD				STEPSPERSEC, STEPSIZE, _3DDWFPREC, LENSLENGTH, CAMERAHEIGHT;
	uint8			SOLIDHIST, SHOWHIST;
	BD				PSOLWIDTH, PSOLHEIGHT, LOFTANG1, LOFTANG2, LOFTMAG1, LOFTMAG2;
	BS				LOFTPARAM;
	uint8			LOFTNORMALS;
	BD				LATITUDE, LONGITUDE, NORTHDIRECTION;
	BL				TIMEZONE;
	uint8			LIGHTGLYPHDISPLAY, TILEMODELIGHTSYNCH, DWFFRAME, DGNFRAME;
	CMC				INTERFERECOLOR;
	H				INTERFEREOBJVS, INTERFEREVPVS, DRAGVS;
	uint8			CSHADOW;

	bool parse(bitsin3 &bits);
};

bool HeaderVars::parse(bitsin3& bits) {
	auto	version = bits.ver;

	bits.read(requiredVersions);

	//unknown
	bits.discard<BD, BD, BD, BD, TV, TV, TV, TV, BL, BL>();

	if (version <= R14)
		bits.discard<BS>();

	if (version <= R2000)
		((bitsin&)bits).discard<H>();//hcv

	bits.read(DIMASO, DIMSHO);
	if (version <= R14)
		bits.read(DIMSAV);

	bits.read(PLINEGEN, ORTHOMODE, REGENMODE, FILLMODE, QTEXTMODE, PSLTSCALE, LIMCHECK, BLIPMODE, USRTIMER, SKPOLY, ANGDIR, SPLFRAME);
	bits.read(ATTREQ, ATTDIA, MIRRTEXT, WORLDVIEW, WIREFRAME, TILEMODE, PLIMCHECK, VISRETAIN, DELOBJ, DISPSILH, PELLIPSE, PROXIGRAPHICS, DRAGMODE);
	bits.read(TREEDEPTH, LUNITS, LUPREC, AUNITS, AUPREC, OSMODE, ATTMODE, COORDS, PDMODE, PICKSTYLE);

	if (version >= R2004)
		bits.discard<BL, BL, BL>();	//unknown

	bits.read(
		USERI1, USERI2, USERI3, USERI4, USERI5, SPLINESEGS,
		SURFU, SURFV, SURFTYPE, SURFTAB1, SURFTAB2, SPLINETYPE,
		SHADEDGE, SHADEDIF, UNITMODE, MAXACTVP,ISOLINES, CMLJUST, TEXTQLTY, LTSCALE, TEXTSIZE, TRACEWID, SKETCHINC, FILLETRAD, THICKNESS, ANGBASE, PDSIZE, PLINEWID,
		USERR1, USERR2, USERR3, USERR4, USERR5,
		CHAMFERA, CHAMFERB, CHAMFERC, CHAMFERD,
		FACETRES, CMLSCALE,CELTSCALE,
		MENU,
		TDCREATE, TDUPDATE
	);

	if (version >= R2004)
		bits.discard<BL, BL, BL>();	//unknown

	bits.read(TDINDWG, TDUSRTIMER, CECOLOR);
	((bitsin&)bits).read(HANDSEED);// HANDSEED always present in bits

	bits.read(CLAYER, TEXTSTYLE, CELTYPE, CMATERIAL, DIMSTYLE, CMLSTYLE, PSVPSCALE);

	PUCS.read(bits);
	UCS.read(bits);
	dim.parse(bits);

	bits.read(BLOCK_CONTROL, LAYER_CONTROL, TEXTSTYLE_CONTROL, LINETYPE_CONTROL, VIEW_CONTROL, UCS_CONTROL, VPORT_CONTROL, APPID_CONTROL, DIMSTYLE_CONTROL, VP_ENT_HDR_CONTROL, GROUP_CONTROL, MLINESTYLE_CONTROL);

	if (version >= R2000) {
		bits.read(DICT_NAMED_OBJS, TSTACKALIGN, TSTACKSIZE, HYPERLINKBASE, STYLESHEET, LAYOUTS_CONTROL, PLOTSETTINGS_CONTROL, DICT_PLOTSTYLES, DICT_MATERIALS, DICT_COLORS, DICT_VISUALSTYLE, DICT_UNKNOWN);
		bits.discard<BL>();//	flags(bits);//RLZ TODO change to 8 vars
		bits.read(INSUNITS, CEPSNTYPE);
		if (CEPSNTYPE == 3)
			bits.read(CPSNID);

		bits.read(FINGERPRINTGUID, VERSIONGUID);
		if (version >= R2004)
			bits.read(SORTENTS, INDEXCTL, HIDETEXT, XCLIPFRAME, DIMASSOC, HALOGAP, OBSCUREDCOLOR, INTERSECTIONCOLOR, OBSCUREDLTYPE, INTERSECTIONDISPLAY, PROJECTNAME);
	}
	bits.read(BLOCK_PAPER_SPACE, BLOCK_MODEL_SPACE, LTYPE_BYLAYER, LTYPE_BYBLOCK, LTYPE_CONTINUOUS);

	if (version >= R2007) {
		bits.read(CAMERADISPLAY);
		bits.discard<BL, BL, BD>();
		bits.read(
			STEPSPERSEC, STEPSIZE, _3DDWFPREC, LENSLENGTH, CAMERAHEIGHT, SOLIDHIST, SHOWHIST, PSOLWIDTH, PSOLHEIGHT,
			LOFTANG1, LOFTANG2, LOFTMAG1, LOFTMAG2, LOFTPARAM, LOFTNORMALS,
			LATITUDE, LONGITUDE, NORTHDIRECTION, TIMEZONE, LIGHTGLYPHDISPLAY, TILEMODELIGHTSYNCH, DWFFRAME, DGNFRAME
		);
		bits.discard<B>();
		bits.read(INTERFERECOLOR, INTERFEREOBJVS, INTERFEREVPVS, DRAGVS, CSHADOW);
		bits.discard<BD>();
	}
	if (version >= R14)
		bits.discard<BS, BS, BS, BS>();

	return true;
}

template<typename T> auto global_get(const T &t) { return get(t); }
typedef variant<string, bool, int32, uint32, double, RD2, RD3, TIME, CMC, H, malloc_block> _variant;
struct Variant : _variant {
	template<typename T> Variant(const T &t) : _variant(global_get(t)) {}
};

Variant read_extended1(bitsin& bits, uint32 remaining) {
	auto	dxfCode = bits.get<uint8>();
	switch (dxfCode + 1000) {
		case DXF_STRING:
			if (bits.ver <= R2004) {
				uint32	len = bits.get<uint8>();
				uint16	cp	= bits.get<RS>();
				return string(bits, len);
			}
			return (string)string16(bits, bits.get<RS>());

		case DXF_BRACKET:
			return bits.get<uint8>();

		case DXF_LAYER_REF:
		case DXF_ENTITY_REF: {
			uint32	v = 0;
			for (int i = 0; i < 8; i++)
				v = (v << 4) | from_digit(bits.get<uint8>());
			return H(v);
		}
		case DXF_BINARY:
			return malloc_block(bits, bits.get<uint8>());

		case DXF_POINTS: case DXF_POINTS + 1: case DXF_POINTS + 2: case DXF_POINTS + 3:
			return bits.get<RD3>();

		case DXF_REALS: case DXF_REALS + 1: case DXF_REALS + 2: 
			return bits.get<RD>();

		case DXF_SHORT:
			return bits.get<RS>();

		case DXF_LONG:
			return bits.get<RL>();

		default:
		case DXF_INVALID:
			bits.seek_cur_bit(-8);
			return malloc_block(bits, remaining);
	}
}

dynamic_array<Variant> read_extended(bitsin& bits, uint32 size) {
	dynamic_array<Variant>	vars;

	bit_seeker	bs(bits, size * 8);
	while (bits.tell_bit() < bs.end)
		vars.push_back(read_extended1(bits, bs.end - bits.tell_bit()));

	return vars;
}

uint32 get_string_offset(bitsin& bits, uint32 bsize) {
	bit_seeker	bs(bits, 0);
	uint32	offset = 0;
	if (bits.ver >= R2007) {
		bits.seek_bit(bsize - 1);
		if (bits.get_bit()) {
			bits.seek_bit(bsize - 17);
			uint32	ssize	= bits.get<RS>();
			if (ssize & 0x8000) {
				bits.seek_bit(bsize - 33);
				ssize = ((ssize & 0x7fff) + (bits.get<RS>() << 15)) + 16;
			}
			offset = bsize - ssize - 17;
		}
	}
	return offset;
}

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------

template<OBJECTTYPE T> struct ObjectT;

struct Object {
	enum FLAGS {
		no_xdict	= 1 << 16,
		xdep		= 1 << 17,	//Object only
		has_binary	= 1 << 18,	//Object only
		has_entity	= 1 << 19,	//Object only
	};
	OBJECTTYPE	type;
	uint32		flags			= 0;
	uint32		handle			= 0;
	uint32		parentH			= 0;	// Soft-pointer ID/handle to owner object

	dynamic_array<H>				reactors;
	hash_map<uint32, dynamic_array<Variant>>	extended;

protected:
	bool common_head(bitsin &bits);
	bool parse_head(bitsin &bits);
	bool parse_handles(bitsin &bits);
	bool parse_handles(bitsin2 &bits) {	// for verification
		ISO_ASSERT(bits.ver < R2007 || bits.check_skip_strings());
		return parse_handles((bitsin&)bits);
	}
public:
	bool parse(bitsin &bits)	{ return parse_head(bits) && parse_handles(bits); }
	void destroy();

	template<OBJECTTYPE T> ObjectT<T>* as() {
		ISO_ASSERT(type == T);
		return static_cast<ObjectT<T>*>(this);
	}
};

template<OBJECTTYPE T> struct ObjectT : Object {};

bool Object::common_head(bitsin &bits) {
	type = readOT(bits);

	if (between(bits.ver, R2000, R2007))
		bits.size = bits.get<uint32>();

	handle	= bits.get<H>();

	while (uint32 xsize = bits.get<BS>()) {
		uint32	ah = bits.get<H>();
		extended[ah] = read_extended(bits, xsize);
	}
	return true;
}


bool Object::parse_head(bitsin &bits) {
	common_head(bits);

	if (bits.ver <= R14)
		bits.size = bits.get<uint32>();

	reactors.resize(bits.get<BL>());

	if (bits.ver >= R2004)
		flags |= bits.get_bit() * no_xdict;

	if (bits.ver >= R2013)
		flags |= bits.get_bit() * has_binary;

	return true;
}

bool Object::parse_handles(bitsin &bits) {
	if (bits.ver >= R2007)		// skip string area
		bits.seek_bit(bits.size);

	parentH = bits.get<H>().get_offset(handle);

	for (auto &h : reactors)
		bits.read(h);

	if (!(flags & no_xdict))//linetype in 2004 seems not have XDicObjH or NULL handle
		bits.get<H>();//XDicObjH

	return true;
}


//-----------------------------------------------------------------------------
//	Entity
//-----------------------------------------------------------------------------

struct Entity : Object {
	enum FLAGS {
		entmode0		= 1 << 24,
		entmode1		= 1 << 25,
		no_next_links	= 1 << 26,
		edge_vis_style	= 1 << 27,
		face_vis_style	= 1 << 28,
		full_vis_style	= 1 << 29,
		invisible		= 1 << 30,
		is_entity		= 1 << 31,
	};
	enum LINEFLAGS : uint8 {
		BYLAYER		= 0,
		CONTINUOUS	= 1,
		BYBLOCK		= 2,
		HANDLE		= 3,
	};

	LineWidth	lWeight			= widthByLayer;
	BD			linetypeScale	= 1.0;
	ENC			color;

	LINEFLAGS	plot_flags		= BYLAYER;
	LINEFLAGS	line_flags		= BYLAYER;
	LINEFLAGS	material_flags	= BYLAYER;
	uint8		shadow_flags;//0 both, 1 receives, 2 casts, 3 no

	//handles
	uint32		linetypeH;
	uint32		plotstyleH;
	uint32		materialH;
	uint32		shadowH;
	uint32		layerH;
	uint32		next_ent = 0;
	uint32		prev_ent = 0;

	malloc_block	graphics_data;

protected:
	bool parse_head(bitsin& bits);
	bool parse_handles(bitsin &bits);
	bool parse_handles(bitsin2 &bits) {	// for verification
		ISO_ASSERT(bits.ver < R2007 || bits.check_skip_strings());
		return parse_handles((bitsin&)bits);
	}
	bool parse_embedded(bitsin &bits);
public:
	bool parse(bitsin &bits)	{ return parse_head(bits) && parse_handles(bits); }
};

bool Entity::parse_embedded(bitsin &bits) {
	flags |= (bits.get_bits(2) * entmode0) | is_entity;

	reactors.resize(bits.get<BL>());//ODA says BS

	if (bits.ver <= R14)
		line_flags = bits.get_bit() ? BYLAYER : HANDLE;

	if (bits.ver >= R2004)
		flags |= bits.get_bit() * no_xdict;

	flags |= (bits.ver <= R2007 || bits.get_bit()) * no_next_links;

	bits.read(color);
	bits.read(linetypeScale);
	if (bits.ver >= R2000) {
		line_flags		= (LINEFLAGS)bits.get_bits(2);
		plot_flags		= (LINEFLAGS)bits.get_bits(2);
	}
	if (bits.ver >= R2007) {
		material_flags	= (LINEFLAGS)bits.get_bits(2);
		shadow_flags	= bits.get<uint8>();
	}
	if (bits.ver >= R2010)
		flags |= bits.get_bits(3) * edge_vis_style;

	flags |= (bits.get<BS>() & 1) * invisible;//	invisibleFlag(bits);

	if (bits.ver >= R2000)
		lWeight = DWGtoLineWidth(bits.get<uint8>());

	return true;
}

bool Entity::parse_head(bitsin& bits) {
	common_head(bits);

	if (bits.get_bit())
		graphics_data.read(bits, bits.get<RL>());

	if (bits.ver <= R14)
		bits.size = bits.get<uint32>();

	return parse_embedded(bits);
}

bool Entity::parse_handles(bitsin &bits) {
	if (bits.ver >= R2007)		// skip string area
		bits.seek_bit(bits.size);

	if ((flags & (entmode0 | entmode1)) == 0)	//entity is in block or polyline
		parentH = bits.get<H>().get_offset(handle);

	reactors.read(bits, reactors.size());

	if (!(flags & no_xdict))
		bits.get<H>();//XDicObjH

	if (bits.ver <= R14) {
		layerH = bits.get<H>();
		if (line_flags == HANDLE)
			linetypeH = bits.get<H>();
	}
	if (bits.ver <= R2000) {
		if (flags & no_next_links) {
			next_ent = handle + 1;
			prev_ent = handle - 1;
		} else {
			prev_ent = bits.get<H>().get_offset(handle);
			next_ent = bits.get<H>().get_offset(handle);
		}
	}
	if (bits.ver >= R2004) {
		//Parses Bookcolor handle
	}

	if (bits.ver >= R2000) {
		layerH = bits.get<H>().get_offset(handle);
		if (line_flags == HANDLE)
			linetypeH = bits.get<H>().get_offset(handle);
	
		if (bits.ver >= R2007) {
			if (material_flags == HANDLE)
				materialH = bits.get<H>().get_offset(handle);
			if (shadow_flags == HANDLE)
				shadowH = bits.get<H>().get_offset(handle);
		}
		if (plot_flags == HANDLE)
			plotstyleH = bits.get<H>().get_offset(handle);
	}
	return true;
}

//-----------------------------------------------------------------------------
//	entities
//-----------------------------------------------------------------------------

template<> struct ObjectT<TEXT> : Entity {
	static double get_double(bitsin &bits) { return bits.ver >= R2000 ? bits.get<double>() : (double)bits.get<BD>(); }

	enum FLAGS {
		no_elevation	= 1 << 0,
		no_align_point	= 1 << 1,
		no_oblique		= 1 << 2,
		no_angle		= 1 << 3,
		no_width_scale	= 1 << 4,
		no_textgen		= 1 << 5,
		no_alignH		= 1 << 6,
		no_alignV		= 1 << 7,
	};

	RD2		insert_point, align_point;
	double	elevation;
	BEXT	ext_point;		// Dir extrusion normal vector, code 210, 220 & 230
	BT		thickness;		// thickness, code 39 *
	double	height;			// height text, code 40
	TV		text;			// text string, code 1
	double	angle;			// rotation angle in degrees (360), code 50
	double	width_scale;	// width factor, code 41
	double	oblique;		// oblique angle, code 51
	int		textgen;		// text generation, code 71
	HAlign	alignH;			// horizontal align, code 72
	VAlign	alignV;			// vertical align, code 73
	H		styleH;			// e->style = textstyle_map.findname(e->styleH);

	bool parse_head(bitsin2 &bits) {
		if (!Entity::parse_head(bits))
			return false;

		if (bits.ver >= R2000)
			flags |= bits.get<uint8>();

		elevation	= flags & no_elevation ? 0 : get_double(bits);

		bits.read(insert_point);

		if (bits.ver >= R2000) {
			if (!(flags & no_align_point) ) {
				align_point[0] = DD(bits, insert_point[0]);
				align_point[1] = DD(bits, insert_point[1]);
			}
		} else {
			bits.read(align_point);
		}

		bits.read(ext_point, thickness);

		oblique		= flags & no_oblique ? 0 : get_double(bits);
		angle		= flags & no_angle	 ? 0 : get_double(bits);
		height		= get_double(bits);
		width_scale	= flags & no_width_scale ? 0 : get_double(bits);

		bits.read(text);
		textgen		= flags & no_textgen ? 0 : (int)bits.get<BS>();
		alignH		= flags & no_alignH	 ? HLeft : (HAlign)get(bits.get<BS>());
		alignV		= flags & no_alignV	 ? VBaseLine : (VAlign)get(bits.get<BS>());

		return true;
	}
	bool parse_handles(bitsin &bits) {
		return Entity::parse_handles(bits)
			&& bits.read(styleH);
	}
	bool parse(bitsin2 &bits) {
		return parse_head(bits) && parse_handles(bits);
	}
};

struct AnnotScaleObject {
	BS		version;
	B		default_flag;
	H		appid;
	BL		ignore_attachment;
	BD3		x_axis_dir;
	BD3		ins_pt;
	BD		rect_width;
	BD		rect_height;
	BD		extents_height;
	BD		extents_width;

	BS		column_type;
	BL		numfragments;	// or # columns
	BD		column_width;
	BD		gutter;
	B		flow_reversed;
	dynamic_array<BD>	column_heights;

	bool	read(bitsin &bits) {
		bits.read(version, default_flag, appid, ignore_attachment, x_axis_dir, ins_pt, rect_width, rect_height, extents_height, extents_width, column_type);

		if (column_type) {
			bits.read(numfragments, column_width, gutter);
			bool	auto_height = bits.get_bit();
			bits.read(flow_reversed);
			if (!auto_height && column_type == 2)
				column_heights.read(bits, numfragments);
		}
		return true;
	}
};

template<> struct ObjectT<MTEXT> : Entity {
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
	BD3		insert_point, ext_point, x_axis;
	BD		rect_width, rect_height, text_height;
	BS		textgen;		// text generation, code 71
	BS		flow_dir;		// Drawing dir BS 72 Left to right, etc.; see DXF doc
	BD		ext_height, ext_width;
	TV		text;			// text string, code 1

	BS		LinespacingStyle;
	BD		LinespacingFactor;

	BL		bg_fill_scale;
	CMC		bg_fill_color;
	BL		bg_fill_trans;

	H		styleH;

	AnnotScaleObject	annot;

	bool parse_head(bitsin2 &bits) {
		if (!Entity::parse_head(bits))
			return false;

		bits.read(insert_point, ext_point, x_axis, rect_width);

		if (bits.ver >= R2007)
			bits.read(rect_height);

		bits.read(text_height, textgen, flow_dir, ext_height, ext_width, text);

		if (bits.ver >= R2000) {
			bits.read(LinespacingStyle, LinespacingFactor);
			bits.get_bit();
		}
		if (bits.ver >= R2004) {
			auto bg_flags = bits.get<BL>();
			if (bg_flags & (bits.ver <= R2018 ? 1 : 16))
				bits.read(bg_fill_scale, bg_fill_color, bg_fill_trans);
		}

		if (bits.ver >= R2018 && bits.get_bit())//FIELD_B (is_not_annotative, 0);
			annot.read(bits);

		return true;
	}

	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& parse_handles(bits)
			&& bits.read(styleH);
	}

	bool parse_embedded(bitsin &bits) {
		return Entity::parse_embedded(bits)
			&& parse_handles(bits)
			&& bits.read(styleH);
	}
};

template<> struct ObjectT<ATTRIB> : ObjectT<TEXT> {
	enum FLAGS {
		invisible		= 1 << 0,
		constant		= 1 << 1,
		verification	= 1 << 2,
		preset			= 1 << 3,
		lock			= 1 << 4,
	};
	enum ATTTYPE {
		Singleline =1,
		Multiline = 2,// (ATTRIB) 
		Multiline2 = 4,// (ATTDEF)
	};
	malloc_block	annotation;
	H	annotation_app;
	BS	annotation_short;
	ObjectT<MTEXT>	*mtext;
	TV	tag;
	BS	field_length;

#if 0
	bool parse(bitsin2 &bits) {
		return Entity::parse(bits);
	}
#else
	bool parse_head(bitsin2 &bits) {
		if (!ObjectT<TEXT>::parse_head(bits))
			return false;

		//SUBCLASS (AcDbAttribute)

		uint8	version		= bits.ver >= R2010 ? bits.get<uint8>() : 0;
		uint8	att_type	= bits.ver >= R2018 ? bits.get<uint8>() : 0;

		if (att_type > Singleline) {
			mtext	= new ObjectT<MTEXT>;
			mtext->parse_embedded(bits);
			//MTEXT
		}

		annotation.read(bits, bits.get<BS>());
		if (annotation)
			bits.read(annotation_app, annotation_short);

		bits.read(tag, field_length);

		flags	|= bits.get<uint8>();
		flags	|= bits.get_bit() * lock;
		return true;
	}
	bool parse(bitsin2 &bits) {
		return parse_head(bits) && parse_handles(bits);
	}
#endif
};

template<> struct ObjectT<ATTDEF> : ObjectT<ATTRIB> {
	uint8	version;
	TV		prompt;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& bits.read(version, prompt)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<SHAPE> : Entity {
	BD3		insert_point;
	BD		scale;
	BD		rotation;	//BD0?
	BD		width_factor;
	BD		oblique_angle;
	BD		thickness;	//DB0?
	BS		style_id; // STYLE index in dwg to SHAPEFILE
	BD3		extrusion;
	H		style;

	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& bits.read(insert_point, scale, rotation, width_factor, oblique_angle, thickness, style_id, extrusion)
			&& parse_handles(bits)
			&& bits.read(style);
	}
};

template<> struct ObjectT<SOLID_3D>			: Entity {
	enum FLAGS {
		has_revision_guid	= 1 << 4,
		acis_empty_bit
	};

	struct Wire {
		enum FLAGS {
			has_transform	= 1 << 0,
			has_shear		= 1 << 1,
			has_reflection	= 1 << 2,
			has_rotation	= 1 << 3,
		};
		uint8	flags;
		uint8	type;
		BL		selection_marker;
		uint32	color;
		BL		acis_index;
		BD3		axis_x, axis_y, axis_z, translation, scale;
		dynamic_array<BD3>	points;

		bool read(bitsin &bits) {
			bits.read(type, selection_marker);
			color = bits.ver <= R2004 ? bits.get<BS>() : bits.get<BL>();
			bits.read(acis_index);
			points.read(bits, bits.get<BL>());		// TODO: align num_points to 255

			if (bits.get_bit()) {
				bits.read(axis_x, axis_y, axis_z, translation, scale);
				flags	= (bits.get_bits(3) << 1) | has_transform;
			} else {
				flags	= 0;
			}
			return true;
		}
	};

	struct Silhouette {
		BL	vp_id;
		BD3 vp_target;
		BD3 vp_dir_from_target;
		BD3 vp_up_dir;
		B	vp_perspective;
		dynamic_array<Wire>			wires;

		bool read(bitsin &bits) {
			bits.read(vp_id, vp_target, vp_dir_from_target, vp_up_dir, vp_perspective);
			if (bits.get_bit())//has_wires
				wires.read(bits, bits.get<BL>());
			return true;
		}
	};

	struct Material {
		BL absref;
		H  handle;
		bool read(bitsin &bits) {
			return bits.read(absref, handle);
		}
	};

	BS				version;
	malloc_block	acis_data;

	BD3		point;
	BL		isolines;
	dynamic_array<Wire>			wires;
	dynamic_array<Silhouette>	silhouettes;
	dynamic_array<Material>		materials;

	BL		revision_major;
	BS		revision_minor1;
	BS		revision_minor2;
	uint8	revision_bytes[8];
	BL		end_marker;

	H		history_id;


	bool decode_3dsolid(bitsin2& bits, ObjectT<SOLID_3D>* _obj, bool has_ds_data) {

		if (!bits.get_bit()) {//FIELD_B(acis_empty, 290);
			bits.get_bit();//FIELD_B(unknown, 0);
			bits.read(version);

			// which is SAT format ACIS 4.0 (since r2000+)
			if (version == 1) {
				dynamic_array<malloc_block>	encr_sat_data;
				uint32	total_size = 0;
				while (uint32 size = bits.get<BL>()) {
					encr_sat_data.push_back(malloc_block(bits, size));
					total_size += size;
				}

				// de-obfuscate SAT data
				char	*dest = acis_data.resize(total_size + 1);
				for (auto &i : encr_sat_data) {
					for (char *j = i, *je = i.end(); j < je; j++)
						*dest++ = *j <= 32 ? *j : 159 - *j;
				}
				*dest = 0;

			} else if (version == 2) {
				/* version 2, SAB: binary, unencrypted SAT format for ACIS 7.0/ShapeManager.
				ACIS versions:
				R14 release            106   (ACIS 1.6)
				R15 (2000) release     400   (ACIS 4.0)
				R18 (2004) release     20800 (ASM ShapeManager, forked from ACIS 7.0)
				R21 (2007) release     21200
				R24 (2010) release     21500
				R27 (2013) release     21800
				R?? (2018) release            223.0.1.1930
				*/
				// TODO string in strhdl, even <r2007
				//  either has_ds_data (r2013+) or the blob is here
				if (!has_ds_data) {
					char* p;
					// Note that r2013+ has End-of-ASM-data (not ACIS anymore, but their fork)
					const char end[]		= "\016\003End\016\002of\016\004ACIS\r\004data";
					const char end1[]		= "\016\003End\016\002of\016\003ASM\r\004data";
					auto	pos	= bits.tell();
					acis_data.read(bits, bits.remaining() - 1);

					// Binary SAB. unencrypted, documented format until "End-of-ACIS-data"
					// TODO There exist also SAB streams with a given number of records, but I haven't seen them here. See dwg_convert_SAB_to_SAT1
					// Better read the first header line here, to check for num_records 0, or even parse the whole SAB format here, and store the SAB different to the ASCII acis_data.
					
					if (auto p = acis_data.find(end)) {
						uint32	size = (char*)p - (char*)acis_data + strlen(end);
						bits.seek_bit(pos + size);
					} else if (p = acis_data.find(end1)) {
						uint32	size = (char*)p - (char*)acis_data + strlen(end1);
						bits.seek_bit(pos + size);
					} else {
						return false;
					}
				} else {
					//LOG_WARN("SAB from AcDs blob not yet implemented");
				}
			}
		}
		return true;
	}

	bool parse(bitsin2& bits) {

		if (bits.get_bit()) {	 //  FIELD_B (wireframe_data_present, 0);
			if (bits.get_bit())	 // FIELD_B (point_present, 0);
				bits.read(point);
		}
		bits.read(isolines);
		if (bits.get_bit()) {  // isoline_present, 0);
			wires.read(bits, bits.get<BL>());
			silhouettes.read(bits, bits.get<BL>());
		}

		flags |= bits.get_bit() * acis_empty_bit;
		if (version > 1) {
			if (bits.ver >= R2007)
				materials.read(bits, bits.get<BL>());
		}
		if (bits.ver >= R2013) {
			flags |= bits.get_bit() * has_revision_guid;
			bits.read(revision_major, revision_minor1, revision_minor2,revision_bytes, end_marker);
		}

		parse_handles(bits);
		if (version > 1)
			bits.read(history_id);
		return true;
	}
};

template<> struct ObjectT<REGION>			: Entity {};
template<> struct ObjectT<BODY>				: Entity {};
template<> struct ObjectT<OLEFRAME>			: Entity {};
template<> struct ObjectT<TOLERANCE>		: Entity {};
template<> struct ObjectT<OLE2FRAME>		: Entity {};
template<> struct ObjectT<ACAD_PROXY_ENTITY>: Entity {};
template<> struct ObjectT<MLEADER>			: Entity {};

template<> struct ObjectT<POINT> : Entity {
public:
	BD3		point;			// base point, code 10, 20 & 30 */
	BT		thickness;		// thickness, code 39 */
	BEXT	ext_point;		// Dir extrusion normal vector, code 210, 220 & 230 */
	BD		x_axis;			// Angle of the X axis for the UCS in effect when the point was drawn

	bool parse(bitsin& bits) {
		return parse_head(bits)
			&& read(bits, point, thickness, ext_point, x_axis)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<LINE> : Entity {
	BD3		point1;
	BD3		point2;
	BT		thickness;
	BEXT	ext_point;

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		if (bits.ver <= R14) {
			bits.read(point1);
			bits.read(point2);

		} else {
			bool	zIsZero = bits.get_bit();
			point1[0]	= bits.get<RD>();
			point2[0]	= DD(bits, point1[0]);
			point1[1]	= bits.get<RD>();
			point2[1]	= DD(bits, point1[1]);
			if (!zIsZero) {
				point1[2] = bits.get<RD>();
				point2[2] = DD(bits, point1[2]);
			}
		}
		bits.read(thickness);
		bits.read(ext_point);
		return parse_handles(bits);
	}
};

template<> struct ObjectT<RAY> : Entity {
	BD3		point1;
	BD3		point2;

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& read(bits, point1, point2)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<XLINE> : ObjectT<RAY> {};

template<> struct ObjectT<CIRCLE> : Entity {
	BD3		centre;
	BD		radius;
	BT		thickness;
	BEXT	ext_point;

	bool parse_head(bitsin &bits) {
		return Entity::parse_head(bits) && bits.read(centre, radius, thickness, ext_point);
	}

	bool parse(bitsin &bits) {
		return parse_head(bits) && parse_handles(bits);
	}
};

template<> struct ObjectT<ARC> : ObjectT<CIRCLE> {
	BD		angle0, angle1;	// start/end angles in radians

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& bits.read(angle0, angle1)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<ELLIPSE> : Entity {
	BD3		centre, axis0;
	BD3		ext_point;
	BD		ratio;
	BD		angle0, angle1;	// start/end angles in radians

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& bits.read(centre, axis0, ext_point, ratio, angle0, angle1)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<TRACE> : Entity	{
	BT		thickness;
	BD		elevation;
	RD2		point1, point2, point3, point4;
	BEXT	ext_point;

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& bits.read(thickness, point1, point2, point3, point4, ext_point)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<SOLID> : ObjectT<TRACE> {};

template<> struct ObjectT<FACE_3D> : Entity {
	BD3		point1, point2, point3, point4;
	BS		invisibleflag = 0;	// bit per edge

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		if (bits.ver <= R14 ) {// R13 & R14
			if (!read(bits, point1, point2, point3, point4, invisibleflag))
				return false;

		} else { // 2000+
			auto has_no_flag	= bits.get_bit();
			auto z_is_zero		= bits.get_bit();
			point1[0]	= (double)bits.get<RD>();
			point1[1]	= (double)bits.get<RD>();
			point1[2]	= z_is_zero ? 0.0 : (double)bits.get<RD>();
			DD(bits, (double*)&point2, (const double*)&point1, 3);
			DD(bits, (double*)&point3, (const double*)&point2, 3);
			DD(bits, (double*)&point4, (const double*)&point3, 3);
			if (!has_no_flag)
				bits.read(invisibleflag);
		}

		return parse_handles(bits);
	}
};

template<> struct ObjectT<BLOCK> : Entity {
	TV		name;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(name);
		return parse_handles(bits);
	}
};

template<> struct ObjectT<ENDBLK> : Entity {
	bool parse(bitsin2 &bits, bool isEnd = false) {
		if (!parse_head(bits))
			return false;

		if (bits.ver >= R2007)
			(void)bits.get_bit();

		return parse_handles(bits);
	}
};

template<> struct ObjectT<SEQEND> : Entity {};


template<> struct ObjectT<INSERT> : Entity {
	BD3		insert_point, ext_point;
	BSCALE	scale;
	BD		angle;			// rotation angle in radians
	H		blockH;
	HandleRange	handles;

	bool parse_handles(bitsin &bits, int count) {
		return Entity::parse_handles(bits)
			&& bits.read(blockH)
			&& handles.read(bits, count);
	}

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& read(bits, insert_point, scale, angle, ext_point)
			&& parse_handles(bits, !bits.get_bit() ? -1 : bits.ver >= R2004 ? (uint32)bits.get<BL>() : 1);
	}
	HandleCollection<Entity> children(DWG *dwg) {
		return {dwg, handles, handle};
	}
	auto	transformation() const {
		return translate(position2(insert_point[0], insert_point[1])) * iso::scale(float2{scale[0], scale[1]}) * rotate2D(angle.v);
	}
};

template<> struct ObjectT<MINSERT> : ObjectT<INSERT> {
	BS colcount, rowcount;
	BD colspace, rowspace;

	bool parse(bitsin &bits) {
		if (!ObjectT<INSERT>::parse(bits) || !read(bits, insert_point, scale, angle, ext_point))
			return false;

		int32	count = !bits.get_bit() ? 0 : bits.ver >= R2004 ? get(bits.get<BL>()) : 2;
		return read(bits, colcount, rowcount, colspace, rowspace)
			&& parse_handles(bits, count);
	}
};

template<> struct ObjectT<LWPOLYLINE> : Entity {
	enum FLAGS {
		has_ext		= 1 << 0,
		has_thick	= 1 << 1,
		has_width	= 1 << 2,
		has_elev	= 1 << 3,
		has_bulges	= 1 << 4,
		has_widths	= 1 << 5,
		plinegen	= 1 << 7,
		open		= 1 << 9,
		has_ids		= 1 << 10,
	};
	BD		width;			// constant width, code 43
	BD		elevation;		// elevation, code 38
	BD		thickness;		// thickness, code 39
	BEXT	ext_point;		// Dir extrusion normal vector, code 210, 220 & 230

	struct Vertex {
		double	x, y;
		double	width0, width1;
		double	bulge;
		int		id;
		Vertex(): x(0), y(0), width0(0), width1(0), bulge(0), id(0) {}
	};

	dynamic_array<Vertex>	vertlist;

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		flags |= bits.get<BS>();
		bits.read(
			onlyif(flags & has_width,	width),
			onlyif(flags & has_elev,	elevation),
			onlyif(flags & has_thick,	thickness),
			onlyif(flags & has_ext,		ext_point)
		);

		uint32	vertexnum = bits.get<BL>();
		vertlist.resize(vertexnum);

		uint32	bulge_count		= flags & has_bulges	? get(bits.get<BL>()) : 0;
		uint32	id_count		= flags & has_ids		? get(bits.get<BL>()) : 0;
		uint32	widths_count	= flags & has_widths	? get(bits.get<BL>()) : 0;

		double	px = 0, py = 0;
		for (int i = 0; i < vertexnum; i++) {
			auto	&vertex = vertlist[i];
			if (i == 0 || bits.ver <= R14) {//14-
				px = bits.get<RD>();
				py = bits.get<RD>();
			} else {
				px = DD(bits, px);
				py = DD(bits, py);
			}
			vertex.x = px;
			vertex.y = py;
		}

		//add bulges
		for (int i = 0; i < bulge_count; i++) {
			double bulge = bits.get<BD>();
			if (i < vertexnum)
				vertlist[i].bulge = bulge;
		}
		//add vertexId
		for (int i = 0; i < id_count; i++) {
			auto id = bits.get<BL>();
			if (i < vertexnum)
				vertlist[i].id = id;
		}
		//add widths
		for (int i = 0; i < widths_count; i++) {
			double w0 = bits.get<BD>(), w1 = bits.get<BD>();
			if (i < vertexnum) {
				vertlist[i].width0 = w0;
				vertlist[i].width1 = w1;
			}
		}
		return parse_handles(bits);
	}
};



template<> struct ObjectT<VERTEX_PFACE_FACE> : Entity {
	array<BS,4>	index;		// polyface mesh vertex indices

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& bits.read(index)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<VERTEX_2D> : Entity {
	BD3		point;
	BD		width0, width1;
	BD		bulge;			// bulge, code 42
	BL		id;
	BD		tgdir;			// curve fit tangent direction, code 50

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		flags		|= bits.get<uint8>();
		bits.read(point);
		bits.read(width0);
		if (width0 < 0)
			width1 = width0 = abs(width0);
		else
			bits.read(width1);
		bits.read(bulge);
		if (bits.ver >= R2010) 
			bits.read(id);
		bits.read(tgdir);

		return parse_handles(bits);
	}
};

// VERTEX_3D, VERTEX_MESH, VERTEX_PFACE
struct Vertex : Entity {
	BD3		point;

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		flags	|= bits.get<uint8>();
		return bits.read(point) && parse_handles(bits);
	}
};

template<> struct ObjectT<VERTEX_3D> : Vertex {};
template<> struct ObjectT<VERTEX_MESH> : Vertex {};
template<> struct ObjectT<VERTEX_PFACE> : Vertex {};

struct Polyline : Entity {
	HandleRange				handles;
	dynamic_array<Entity*>	vertices;

	~Polyline() {
		for (auto p : vertices)
			p->destroy();
	}

	bool parse_handles(bitsin &bits) {
		int count = bits.ver >= R2004 ? (int)bits.get<BL>() : 1;

		if (!Entity::parse_handles(bits))
			return false;

		handles.read(bits, count);
		return true;
	}
	HandleCollection<Entity> children(DWG *dwg) {
		return {dwg, handles, handle};
	}
};

template<> struct ObjectT<POLYLINE_2D> : Polyline {
	BS		curve_type;		// curves & smooth surface type, code 75, default 0
	BD		width0;
	BD		width1;
	BT		thickness;
	BD		elevation;
	BEXT	ext_point;

	bool parse(bitsin &bits) {
		BS		tflags;
		if (parse_head(bits) && read(bits, tflags, curve_type, width0, width1, thickness, elevation, ext_point))
			flags |= tflags;
		return parse_handles(bits);
	}
};

template<> struct ObjectT<POLYLINE_3D> : Polyline {
	enum FLAGS {
		curve_type_mask	= 3 << 0,
		someflag		= 1 << 2,
	};
	uint8	curve_type;		// curves & smooth surface type, code 75, default 0

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		flags |= bits.get<uint8>();
		flags |= (bits.get<uint8>() & 1) << someflag;

		return parse_handles(bits);
	}
};

template<> struct ObjectT<POLYLINE_PFACE> : Polyline {
	BS		vertexcount;		// polygon mesh M vertex or polyface vertex num, code 71, default 0
	BS		facecount;			// polygon mesh N vertex or polyface face num, code 72, default 0

	bool parse(bitsin &bits) {
		return parse_head(bits)
			&& read(bits, vertexcount, facecount)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<POLYLINE_MESH> : Polyline {
	BS		curve_type;
	BS		num_m_verts;
	BS		num_n_verts;
	BS		m_density;
	BS		n_density;

	bool parse(bitsin &bits) {
		parse_head(bits);
		BS		tflags;
		bits.read(tflags, curve_type, num_m_verts, num_n_verts, m_density, n_density);
			flags |= tflags;

		uint32	count = bits.ver >= R2004 ? (uint32)bits.get<BL>() : 0;
		return parse_handles(bits) && handles.read(bits, count);
	}
};

template<> struct ObjectT<SPLINE> : Entity {
	enum FLAGS {//flipped
		periodic	= 1 << 0,
		closed		= 1 << 1,
		rational	= 1 << 2,
	};
	BD3		tangent0;
	BD3		tangent1;
	BL		degree;			// degree of the spline, code 71
	BD		knot_tol;		// knot tolerance, code 42, default 0.0000001
	BD		control_tol;	// control point tolerance, code 43, default 0.0000001
	BD		fit_tol;		// fit point tolerance, code 44, default 0.0000001

	dynamic_array<BD>	knotslist;			// knots list, code 40
	dynamic_array<BD>	weightlist;			// weight list, code 41
	dynamic_array<BD3>	controllist;		// control points list, code 10, 20 & 30
	dynamic_array<BD3>	fitlist;			// fit points list, code 11, 21 & 31

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		auto scenario = bits.get<BL>();
		if (bits.ver >= R2013) {
			if (bits.get<BL>() & 1)
				scenario = 2;
			bits.get<BL>();
		}
		bits.read(degree);

		BL		nknots;		// number of knots, code 72, default 0
		BL		ncontrol;	// number of control points, code 73, default 0
		BL		nfit;		// number of fit points, code 74, default 0
		B		weight;		// RLZ ??? flags, weight, code 70, bit 4 (16)

		if (scenario == 2) {
			bits.read(fit_tol, tangent0, tangent1, nfit);
		} else if (scenario == 1) {
			flags	|= bits.get_bits(3);
			bits.read(knot_tol, control_tol, nknots, ncontrol, weight);
		} else {
			return false; //RLZ: from doc only 1 or 2 are ok ?
		}

		knotslist.read(bits, nknots);
		
		controllist.reserve(ncontrol);
		if (weight)
			weightlist.reserve(ncontrol);

		for (int32 i = 0; i < ncontrol; ++i) {
			bits.read(controllist.push_back());
			if (weight)
				bits.read(weightlist.push_back()); //RLZ Warning: D (BD or RD)
		}
		fitlist.read(bits, nfit);
		return parse_handles(bits);
	}
};

template<> struct ObjectT<HATCH> : Entity {
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
			BD	bulge	= 0;
			Vertex(bitsin& bits, bool has_bulge) : Item(VERTEX) { bits.read(point, onlyif(has_bulge, bulge)); }
		};
		struct Line : Item {
			RD2	point1, point2;
			Line(bitsin &bits) : Item(LINE) { bits.read(point1, point2); }
		};
		struct CircleArc : Item {
			RD2	centre;
			BD	radius, angle0, angle1;
			B	isccw;
			CircleArc(bitsin &bits) : Item(CIRCLE_ARC) { bits.read(centre, radius, angle0, angle1, isccw); }
		};
		struct EllipseArc : Item {
			RD2	point1, point2;
			BD	ratio, param0, param1;
			B	isccw;
			EllipseArc(bitsin &bits) : Item(ELLIPSE_ARC) { bits.read(point1, point2, ratio, param0, param1, isccw); }
		};
		struct Spline : Item {
			BL	degree;
			B	isRational, periodic;
			RD2	tangent0, tangent1;
			dynamic_array<RD>		knotslist;
			dynamic_array<RD3>		controllist;
			dynamic_array<RD2>		fitlist;

			Spline(bitsin& bits) : Item(SPLINE) {
				bits.read(degree, isRational, periodic);
				uint32	nknots	= bits.get<BL>(), ncontrol = bits.get<BL>();

				knotslist.read(bits, nknots);

				controllist.reserve(ncontrol);
				for (int32 j = 0; j < ncontrol;++j) {
					RD	x = bits.get<RD>(), y= bits.get<RD>(), z = isRational ? bits.get<RD>() : 0;
					controllist.push_back(RD3{x, y, z});
				}
				if (bits.ver >= R2010) { 
					fitlist.read(bits, bits.get<BL>());
					bits.read(tangent0);
					bits.read(tangent1);
				}
			}
		};

		uint32	type;	// boundary path type, code 92, polyline=2, default=0 */
		bool	closed;	// only polyline
		dynamic_array<Item*> objlist;

		Loop(bitsin &bits) : type(bits.get<BL>()) {
			if (!(type & 2)) {
				for (int32 j = 0, n = bits.get<BL>(); j < n; ++j) {
					switch (bits.get<uint8>()) {
						case LINE:			objlist.push_back(new Line(bits));		break;
						case CIRCLE_ARC:	objlist.push_back(new CircleArc(bits));	break;
						case ELLIPSE_ARC:	objlist.push_back(new EllipseArc(bits));break;
						case SPLINE:		objlist.push_back(new Spline(bits));	break;
					}
				}
			} else {
				auto	has_bulge = bits.get_bit();
				closed	= bits.get_bit();;
				for (int32 j = 0, n = bits.get<BL>(); j < n; ++j)
					objlist.push_back(new Vertex(bits, has_bulge));
			}
		}
		~Loop() {
			for (auto p : objlist)
				delete p;
		}
	};

	struct Line {
		BD	angle;
		BD2 point, offset;
		dynamic_array<BD>	dash;
		bool read(bitsin &bits) {
			return bits.read(angle, point, offset) && dash.read(bits, bits.get<BS>());
		}
	};

	enum FLAGS {
		associative	= 1 << 0,//	\	switched
		solid		= 1 << 1,//	/
		use_double	= 1 << 2,
	};

	TV		name;
	BD		elevation;
	BD3		ext_point;
	BS		hstyle;			// hatch style, code 75 */
	BS		hpattern;		// hatch pattern type, code 76 */
	BD		angle;			// hatch pattern angle, code 52 */
	BD		scale;			// hatch pattern scale, code 41 */
	BD		pixsize;

	dynamic_array<Line>	deflines;	// pattern definition lines
	dynamic_array<Loop> loops;		// polyline list
	TV					grad_name;
	Gradient			grad;
	dynamic_array<RD2>	seeds;
	dynamic_array<H>	bound;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		if (bits.ver >= R2004) { 
			grad.read(bits);
			bits.read(grad_name);
		}
		bits.read(elevation, ext_point, name);
		flags	|= bits.get_bits(2);

		uint32	numloops = bits.get<BL>(), total_bound = 0;
		loops.reserve(numloops);
		bool have_pixel_size = false;
		for (int i = 0; i < numloops; i++) {
			auto&	loop = loops.push_back(bits);
			have_pixel_size	|= loop.type & 4;
			total_bound += bits.get<BL>();
		}

		bits.read(hstyle, hpattern);

		if (!(flags & solid)) {
			bits.read(angle, scale);
			flags	|= bits.get_bit() * use_double;
			deflines.read(bits, bits.get<BS>());
		}

		if (have_pixel_size)
			bits.read(pixsize);
		
		seeds.read(bits, bits.get<BL>());

		if (!parse_handles(bits))
			return false;

		bound.read(bits, total_bound);
		return true;
	}
};

template<> struct ObjectT<IMAGE> : Entity {
	enum FLAGS {
		clip_mode	= 1 << 0,
	};
	BD3		point1;
	BD3		point2;
	BD3		v_vector;		// V-vector of single pixel, x coordinate, code 12, 22 & 32 */
	RD2		size;			// image size in pixels, U value, code 13
	B		clip;			// Clipping state, code 280, 0=off 1=on
	uint8	brightness;		// Brightness value, code 281, (0-100) default 50
	uint8	contrast;		// Brightness value, code 282, (0-100) default 50
	uint8	fade;			// Brightness value, code 283, (0-100) default 0
	uint32	ref;			// Hard reference to imagedef object, code 340 */

	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		BL	classVersion;
		BS	displayProps;
		bits.read(classVersion, point1, point2, v_vector, size, displayProps, clip, brightness, contrast, fade);

		if (bits.ver >= R2010) 
			flags |= bits.get_bit() * clip_mode;

		uint32 clipType = bits.get<BS>();
		if (clipType == 1) {
			bits.discard<RD, RD>();

		} else { //clipType == 2
			for (uint32 i = 0, n = bits.get<BL>(); i < n; ++i)
				(void)bits.get<RD>();
		}

		if (!parse_handles(bits))
			return false;

		ref	= bits.get<H>();
		(void)bits.get<H>();
		return true;
	}
};

struct Dimension : Entity {
	enum FLAGS {//flipped 2-4
		non_default	= 1 << 0,
		use_block	= 1 << 1,
		flip_arrow1	= 1 << 2,
		flip_arrow2	= 1 << 3,
		has_arrow2	= 1 << 4,
		unknown		= 1 << 6,
	};
	uint8	class_version;
	BD3		extrusion;
	RD2		text_midpt;
	BD		elevation;
	TV		user_text;
	BD		text_rotation;
	BD		horiz_dir;
	BD3		ins_scale;
	BD		ins_rotation;
	BS		attachment;
	BS		lspace_style;
	BD		lspace_factor;
	BD		act_measurement;
	RD2		clone_ins_pt;

	H		styleH;
	H		blockH;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		if (bits.ver >= R2010)
			bits.read(class_version);

		uint8	tflags;
		bits.read(extrusion, text_midpt, elevation, tflags, user_text, text_rotation, horiz_dir, ins_scale, ins_rotation);
		flags |= tflags & 0x43;

		if (bits.ver >= R2000)
			bits.read(attachment, lspace_style, lspace_factor, act_measurement);

		if (bits.ver >= R2007)
			flags |= bits.get_bits(3) * flip_arrow1;

		return bits.read(clone_ins_pt);
	}
	bool parse_handles(bitsin &bits) {
		if (!Entity::parse_handles(bits))
			return false;

		bits.read(styleH);
		bits.read(blockH);
		return true;
	}
};


template<> struct ObjectT<DIMENSION_ORDINATE> : Dimension {
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	bool parse(bitsin2 &bits) {
		uint8	type2;
		//type =  (type2 & 1) ? type | 0x80 : type & 0xBF; //set bit 6
		return Dimension::parse(bits)
			&&	bits.read(defpoint, def1, def2, type2)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<DIMENSION_LINEAR> : Dimension {
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	BD	oblique;
	BD	angle;

	bool parse(bitsin2 &bits) {
		return Dimension::parse(bits)
			&&	bits.read(def1, def2, defpoint, oblique, angle)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<DIMENSION_ALIGNED> : Dimension {
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	BD	oblique;

	bool parse(bitsin2 &bits) {
		return Dimension::parse(bits)
			&&	bits.read(def1, def2, defpoint, oblique)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<DIMENSION_ANG_LN2> : Dimension {
	RD2	arcPoint;
	BD3	def1;
	BD3	def2;
	BD3	centrePoint;
	BD3	defpoint;

	bool parse(bitsin2 &bits) {
		return Dimension::parse(bits)
			&&	bits.read(arcPoint, def1, def2, centrePoint, defpoint)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<DIMENSION_ANG_PT3> : Dimension {
	BD3	defpoint;
	BD3	def1;
	BD3	def2;
	BD3	centrePoint;

	bool parse(bitsin2 &bits) {
		return Dimension::parse(bits)
			&&	bits.read(defpoint, def1, def2, centrePoint)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<DIMENSION_RADIUS> : Dimension {
	BD3	defpoint;
	BD3	circlePoint;
	BD	radius;

	bool parse(bitsin2 &bits) {
		return Dimension::parse(bits)
			&&	bits.read(defpoint, circlePoint, radius)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<DIMENSION_DIAMETER> : Dimension {
	BD3	circlePoint;
	BD3	defpoint;
	BD	radius;

	bool parse(bitsin2 &bits) {
		return Dimension::parse(bits)
			&&	bits.read(circlePoint, defpoint, radius)
			&&	parse_handles(bits);
	}
};

template<> struct ObjectT<LEADER> : Entity {
	enum FLAGS {//flipped
		arrow		= 1 << 0,
		hook_dir	= 1 << 1,
	};
	BD2		textsize;			// Text annotation height, code 40
	BEXT	extrusionPoint;		// Normal vector, code 210, 220 & 230
	BD3		horizdir;			// "Horizontal" direction for leader, code 211, 221 & 231
	BD3		offsetblock;		// Offset of last leader vertex from block, code 212, 222 & 232
	RD3		offsettext;			// Offset of last leader vertex from annotation, code 213, 223 & 233

	dynamic_array<BD3>	vertexlist;		// vertex points list, code 10, 20 & 30

	H		styleH;
	H		AnnotH;
	
	bool parse(bitsin &bits) {
		if (!parse_head(bits))
			return false;

		bits.get_bit();//unknown_bit(bits);
		BS	annot_type	= bits.get<BS>();
		BS	Path_type	= bits.get<BS>();

		vertexlist.read(bits, bits.get<BL>());

		auto	Endptproj = bits.get<BD3>();
		bits.read(extrusionPoint);
		if (bits.ver >= R2000) 
			bits.get_bits(5);

		bits.read(horizdir);
		bits.read(offsetblock);
		if (bits.ver >= R14)
			bits.get<BD3>();

		if (bits.ver <= R14) //R14 -
			bits.get<BD>();//dimgap;

		if (bits.ver <= R2007)
			bits.read(textsize);

		flags |= bits.get_bits(2);

		if (bits.ver <= R14) {
			auto	nArrow_head_type= bits.get<BS>();
			auto	dimasz			= bits.get<BD>();
			bits.get_bits(2);//nunk_bit, unk_bit
			auto	unk_short		= bits.get<BS>();
			auto	byBlock_color	= bits.get<BS>();
		} else {
			bits.get<BS>();
		}
		bits.get_bits(2);

		if (!parse_handles(bits))
			return false;

		bits.read(AnnotH, styleH);
		return true;
	}
};

template<> struct ObjectT<VIEWPORT> : Entity {
	enum FLAGS {
		ucs_per_viewport	= 1 << 6,
		ucs_at_origin		= 1 << 7,
	};
	BD3		point;
	BD2		pssize;			// Width in paper space units, code 40
	RD2		centerP;		// view center point X, code 12
	RD2		snapP;			// Snap base point X, code 13
	RD2		snapSpP;		// Snap spacing X, code 14
	int		vpstatus;		// Viewport status, code 68
	int		vpID;			// Viewport ID, code 69

	BD3		view_target;		// View target point, code 17, 27, 37
	BD3		view_dir;		// View direction vector, code 16, 26 & 36
	BD		twist_angle;	// view twist angle, code 51
	BD		view_height;	// View height in model space units, code 45
	BD		view_length;	// Perspective lens length, code 42
	BD		front_clip;		// Front clip plane Z value, code 43
	BD		back_clip;		// Back clip plane Z value, code 44
	BD		snap_angle;		// Snap angle, code 50

	BS			grid_major;
	RenderMode	render_mode;
	UserCoords	ucs;
	dynamic_array<H>	frozen;

	H	vport_entity_header, clip_boundary, named_ucs, base_ucs;
	H	background, visualstyle, shadeplot, sun;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(point, pssize);

		if (bits.ver >= R2000)
			bits.read(view_target, view_dir, twist_angle, view_height, view_length, front_clip, back_clip, snap_angle, centerP, snapP, snapSpP);
		if (bits.ver >= R2007)
			bits.read(grid_major);

		if (bits.ver >= R2000) {
			frozen.resize(bits.get<BL>());
			BL		status_flags	= bits.get<BL>();
			TV		style_sheet		= bits.get<TV>();
			uint8	render_mode		= bits.get<uint8>();
			flags	|= bits.get_bits(2) * ucs_per_viewport;
			ucs.read(bits);
		}
		if (bits.ver >= R2004)
			render_mode.read(bits);

		if (!parse_handles(bits))
			return false;

		if (bits.ver <= R14)
			bits.read(vport_entity_header);

		if (bits.ver >= R2000) {
			for (auto &h : frozen)
				bits.read(h);
			bits.read(clip_boundary);

			if (bits.ver == R2000)
				bits.read(vport_entity_header);
			
			bits.read(named_ucs, base_ucs);
		}
		if (bits.ver >= R2007)
			bits.read(background, visualstyle, shadeplot, sun);

		return true;
	}
};

//-----------------------------------------------------------------------------
// Objects
//-----------------------------------------------------------------------------

struct ObjControl : Object {
	dynamic_array<uint32>	handles;

	bool parse_handles(bitsin& bits, int n) {
		if (!Object::parse_handles(bits))
			return false;

		handles.reserve(n);
		for (int i = 0; i < n; i++) {
			if (auto h = bits.get<H>().get_offset(handle)) //in vports R14 I found some NULL handles
				handles.push_back(h);
		}

		return true;
	}

	bool	parse(bitsin& bits) {
		if (!parse_head(bits))
			return false;

		return parse_handles(bits, bits.get<BL>());
	}
};

template<> struct ObjectT<BLOCK_CONTROL_OBJ>	: ObjControl {
	bool parse(bitsin2& bits) {
		return parse_head(bits)
			&& parse_handles(bits, bits.get<BL>() + 2);
	}
};

template<> struct ObjectT<LTYPE_CONTROL_OBJ>	: ObjControl {
	bool parse(bitsin2& bits) {
		return parse_head(bits)
			&& parse_handles(bits, bits.get<BL>() + 2);
	}
};

template<> struct ObjectT<DIMSTYLE_CONTROL_OBJ>	: ObjControl {
	bool parse(bitsin2& bits) {
		if (!parse_head(bits))
			return false;

		uint32	n = bits.get<BL>();

		// V2000 dimstyle seems have one unknown byte hard handle counter??
		int		unkData = bits.ver >= R2000 ? (int)bits.get<uint8>() : 0;

		parse_handles(bits, n);
		for (int i = 0; i < unkData; i++)
			(void)bits.get<H>().get_offset(handle);
		return true;
	}
};

template<> struct ObjectT<LAYER_CONTROL_OBJ>	: ObjControl {};
template<> struct ObjectT<STYLE_CONTROL_OBJ>	: ObjControl {};
template<> struct ObjectT<VIEW_CONTROL_OBJ>		: ObjControl {};
template<> struct ObjectT<UCS_CONTROL_OBJ>		: ObjControl {};
template<> struct ObjectT<VPORT_CONTROL_OBJ>	: ObjControl {};
template<> struct ObjectT<APPID_CONTROL_OBJ>	: ObjControl {};
template<> struct ObjectT<VP_ENT_HDR_CTRL_OBJ>	: ObjControl {};

template<> struct ObjectT<DICTIONARY> : ObjControl {
	enum FLAGS {
		cloning	= 1 << 0,
	};
	TV					name;

	int parse0(bitsin2 &bits) {
		if (!parse_head(bits))
			return -1;

		uint32	n = bits.get<BL>();
		if (bits.ver <= R14) {
			(void)bits.get<uint8>();
		} else {
			flags |= bits.get_bit() * cloning;
			bits.get<uint8>();//	hardowner
		}

		bits.read(name);
		return n;
	}

	bool parse(bitsin2 &bits) {
		int	n = parse0(bits);
		return	n >= 0  && parse_handles(bits, n);

	}
};

template<> struct ObjectT<DICTIONARYWDFLT> : ObjectT<DICTIONARY> {
	H	def;

	bool parse(bitsin2 &bits) {
		int n = parse0(bits);
		if (n < 0 || !parse_handles(bits, n))
			return false;

		bits.read(def);
		return true;
	}
};

template<> struct ObjectT<ACDBDICTIONARYWDFLT> : ObjectT<DICTIONARYWDFLT> {};

struct NamedObject : Object {
	TV			name;

	bool parse_head(bitsin2 &bits) {
		if (!Object::parse_head(bits))
			return false;

		bits.read(name);

		flags	|= bits.get_bit() * has_entity;
		if (bits.ver <= R2004)
			bits.get<BS>();// xrefindex
		
		flags	|= bits.get_bit() * xdep;
		return true;
	}
};

template<> struct ObjectT<BLOCK_HEADER> : NamedObject {
	enum FLAGS {
		xrefOverlaid	= 1 << 0,
		blockIsXref		= 1 << 1,
		has_attdefs		= 1 << 2,
		anonymous		= 1 << 3,
		loaded_Xref		= 1 << 4,
		can_explode		= 1 << 5,
	};
	
	BS				insUnits;		// block insertion units, code 70 of block_record
	uint8			scaling;
	BD3				base_point;		// block insertion base point dwg only
	TV				xref_path;
	TV				description;
	malloc_block	preview;
	uint32			blockH;			// handle for block entity
	HandleRange		entities;
	dynamic_array<H> inserts;
	H				layoutH;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		flags	|= bits.get_bits(4);
		if (bits.ver >= R2000)
			flags	|= bits.get_bit() * loaded_Xref;

		//Number of objects owned by this block
		int		objectCount = bits.ver >= R2004 ? (int)bits.get<BL>() : !(flags & blockIsXref) && !(flags & xrefOverlaid) ? 1 : -1;

		bits.read(base_point, xref_path);

		uint32 insertCount = 0;
		if (bits.ver >= R2000) {
			while (uint8 i = bits.get<uint8>())
				insertCount +=i;
			bits.read(description);
			preview.read(bits, bits.get<BL>());
		}

		if (bits.ver >= R2007) {
			bits.read(insUnits);
			flags		|= bits.get_bit() * can_explode;
			bits.read(scaling);
		}

		parse_handles(bits);
		bits.get<H>();//XRefH

		blockH		= bits.get<H>().get_offset(handle);
		entities.read(bits, objectCount);

		if (bits.ver >= R2000) {
			inserts.read(bits, insertCount);
			bits.read(layoutH);
		}
		return true;
	}

	HandleCollection<Entity> children(DWG *dwg) {
		return {dwg, entities, handle};
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<LAYER> : NamedObject {
	enum FLAGS {
		frozen		= 1 << 0,
		layeron		= 1 << 1,
		frozen_new	= 1 << 2,
		locked		= 1 << 3,
		plotF		= 1 << 4,
	};
	CMC			color;
	LineWidth	lWeight;
	H			plotstyleH;			// Hard-pointer ID/handle of plotstyle, code 390
	H			materialstyleH;		// Hard-pointer ID/handle of materialstyle, code 347
	H			linetypeH;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		if (bits.ver <= R14) {
			flags |= bits.get_bit() * frozen;
			(void)bits.get_bit(); //unused, negate the color
			flags |= bits.get_bit() * frozen_new;
			flags |= bits.get_bit() * locked;
		} else {
			int16 f = bits.get<BS>();
			flags |= f & 31;
			lWeight = DWGtoLineWidth((f >> 5) & 31);
		}
		color.read(bits);

		parse_handles(bits);
		bits.get<H>();//XRefH

		if (bits.ver >= R2000)
			bits.read(plotstyleH);

		if (bits.ver >= R2007)
			bits.read(materialstyleH);

		bits.read(linetypeH);
		return true;
	}

	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<STYLE> : NamedObject {
	enum FLAGS {//flipped
		shape		= 1 << 0,
		vertical	= 1 << 1,
	};
	BD		height;			// Fixed text height (0 not set), code 40
	BD		width_scale;	// Width factor, code 41
	BD		oblique;	// Oblique angle, code 50
	uint8	genFlag;	// Text generation flags, code 71
	BD		lastHeight;	// Last height used, code 42
	TV		font;		// primary font file name, code 3
	TV		bigFont;	// bigfont file name or blank if none, code 4

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		flags	|= bits.get_bits(2);

		bits.read(height, width_scale, oblique, genFlag, lastHeight, font, bigFont);
		parse_handles((bitsin&)bits);//avoid test
		bits.get<H>();//XRefH
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<LTYPE> : NamedObject {
	struct Entry {
		enum FLAGS {
			horizontal	= 1 << 0,	// text is rotated 0 degrees, otherwise it follows the segment
			shape_index	= 1 << 1,	// complexshapecode holds the index of the shape to be drawn
			text_index	= 1 << 2,	// complexshapecode holds the index into the text area of the string to be drawn.
		};
		BD	hash_length;
		BS	code;
		RD	x_offset, y_offset;
		BD	scale, rotation;
		BS	flags;
		bool	read(bitsin &bits) {
			return bits.read(hash_length, code, x_offset, y_offset, scale, rotation, flags);
		}
	};
	TV				desc;					// descriptive string, code 3
	uint8			align;					// align code, always 65 ('A') code 72
	BD				length;					// total length of pattern, code 40
	uint8			haveShape;				// complex linetype type, code 74
	malloc_block	strarea;
	H				dashH, shapeH;
	dynamic_array<Entry> path;	// trace, point or space length sequence, code 49

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(desc);
		bits.read(length);
		bits.read(align);

		path.read(bits, bits.get<uint8>());
		bool haveStrArea = false;
		for (auto &i : path)
			haveStrArea = haveStrArea || (i.flags & Entry::shape_index);

		if (bits.ver <= R2004) 
			strarea.read(bits, 256);
		else if (haveStrArea)
			strarea.read(bits, 512);

		parse_handles(bits);
		if (path) {
			bits.get<H>();//XRefH
			bits.read(dashH);
		}
		bits.read(shapeH);
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<VIEW> : NamedObject {
	enum FLAGS {
		pspace		= 1 << 0,
		plottable	= 1 << 1,
	};
	BD		height;
	BD		width;
	RD2		center;

	BD3		view_target;		// View target point, code 17, 27, 37
	BD3		view_dir;		// View direction vector, code 16, 26 & 36
	BD		twist_angle;		// view twist angle, code 51
	BD		LensLength;
	BD		front_clip;		// Front clip plane Z value, code 43
	BD		back_clip;		// Back clip plane Z value, code 44
	uint8	ViewMode;		// 4 bits

	RenderMode	render_mode;
	UserCoords	ucs;
		
	H		BackgroundH;
	H		VisualStyleH;
	H		SunH;
	H		BaseUCSH;
	H		NamedUCSH;
	H		LiveSectionH;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(height, width, center, view_target, view_dir, twist_angle, LensLength, front_clip, back_clip);
		ViewMode = bits.get_bits(4);

		if (bits.ver >= R2000) 
			render_mode.read(bits);

		flags		|= bits.get_bit() * pspace;

		if (bits.ver >= R2000 &&  bits.get_bit())
			ucs.read(bits);

		if (bits.ver >= R2007)
			flags	|= bits.get_bit() * plottable;
		
		parse_handles(bits);
		bits.get<H>();//XRefH

		if (bits.ver >= R2007)
			bits.read(BackgroundH, VisualStyleH, SunH);

		if (bits.ver >= R2000)
			bits.read(BaseUCSH, NamedUCSH);

		if (bits.ver >= R2007)
			bits.read(LiveSectionH);
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};


template<> struct ObjectT<UCS> : NamedObject, UserCoords {
	BS		ortho_type;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		UserCoords::read(bits);

		if (bits.ver >= R2000)
			bits.read(ortho_type);

		return parse_handles(bits);
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<VPORT> : NamedObject {
	enum FLAGS {
		grid				= 1 << 0,
		ucs_icon0			= 1 << 1,
		ucs_icon1			= 1 << 2,
		fastZoom			= 1 << 3,
		snap_style			= 1 << 4,
		snap_mode			= 1 << 5,
		ucs_per_viewport	= 1 << 6,
		ucs_at_origin		= 1 << 7,
	};
	enum VIEWMODE {
		UCSFOLLOW	= 1 << 3,
	};
	RD2		lower_left;
	RD2		upper_right;
	RD2		center;			// center point in WCS, code 12 & 22 */
	RD2		snap_base;		// snap base point in DCS, code 13 & 23 */
	RD2		snap_spacing;	// snap Spacing, code 14 & 24 */
	RD2		grid_spacing;	// grid Spacing, code 15 & 25 */
	BD3		view_dir;		// view direction from target point, code 16, 26 & 36 */
	BD3		view_target;		// view target point, code 17, 27 & 37 */
	BD		height;			// view height, code 40 */
	BD		ratio;			// viewport aspect ratio, code 41 */
	BD		lensHeight;		// lens height, code 42 */
	BD		front_clip;		// front clipping plane, code 43 */
	BD		back_clip;		// back clipping plane, code 44 */
	BD		snap_angle;		// snap rotation angle, code 50 */
	BD		twist_angle;		// view twist angle, code 51 */
	uint8	view_mode;		// view mode, code 71 */
	BS		circleZoom;		// circle zoom percent, code 72 */
	BS		snap_isopair;	// snap isopair, code 78 */
	BS		gridBehavior;	// grid behavior, code 60, undocummented */

	BS		grid_major;
	RenderMode	render_mode;
	UserCoords	ucs;
	
	H	bkgrdH, visualStH, sunH;
	H	namedUCSH, baseUCSH;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(height, ratio, center, view_target, view_dir, twist_angle, lensHeight, front_clip, back_clip);
		view_mode	= bits.get_bits(4);

		if (bits.ver >= R2000) 
			render_mode.read(bits);

		bits.read(lower_left, upper_right);
		view_mode	|= bits.get_bit() << 3; //UCSFOLLOW, view mode, code 71, bit 3 (8)
		bits.read(circleZoom);
		flags		|= bits.get_bits(4);
		bits.read(grid_spacing);
		flags		|= bits.get_bits(2) * snap_style;
		bits.read(snap_isopair, snap_angle, snap_base, snap_spacing);
		if (bits.ver >= R2000) { 
			flags	|= bits.get_bits(2) * ucs_per_viewport;
			ucs.read(bits);
		}
		if (bits.ver >= R2007)
			bits.read(gridBehavior, grid_major);

		parse_handles(bits);
		bits.get<H>();//XRefH

		if (bits.ver >= R2000) { 
			if (bits.ver >= R2007)
				bits.read(bkgrdH, visualStH, sunH);
			bits.read(namedUCSH, baseUCSH);
		}
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<APPID> : NamedObject {
	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.get<uint8>();//unknown

		parse_handles(bits);
		bits.get<H>();//XRefH
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<DIMSTYLE> : NamedObject, DimStyle {
	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bitsin	hbits(bits);
		if (!parse_handles(hbits))
			return false;

		return DimStyle::parse(lvalue(bitsin3(bits, hbits)));
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<VP_ENT_HDR> : Object {
	enum {CONTROL = VP_ENT_HDR_CTRL_OBJ};
	bool parse(bitsin2 &bits) {
		return parse_head(bits) && parse_handles(bits);
	}
};

template<> struct ObjectT<IMAGEDEF> : Object {
	TV			name;
	BL			version;				// class version, code 90, 0=R14 version
	RD2			imageSize;				// image size in pixels U value, code 10
	RD2			pixelSize;				// default size of one pixel U value, code 11
	B			loaded;					// image is loaded flag, code 280, 0=unloaded, 1=loaded
	uint8		resolution;				// resolution units, code 281, 0=no, 2=centimeters, 5=inch

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;
		bits.read(version, imageSize, name, loaded, resolution, pixelSize);

		parse_handles(bits);
		bits.get<H>();//XRefH
		return true;
	}
};


template<> struct ObjectT<GROUP> : Object {
	enum FLAGS {
		unnamed	= 1 << 0,
		selectable	= 1 << 1,
	};
	TV					name;
	dynamic_array<H>	handles;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(name);
		flags	|= bits.get<BS>() * unnamed;
		flags	|= bits.get<BS>() * selectable;

		handles.resize(bits.get<BL>());

		parse_handles(bits);
		bits.get<H>();//XRefH

		handles.read(bits, handles.size32());
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<DICTIONARYVAR> : Object {
	TV	name;
	uint8	value;
	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;
		bits.read(value, name);
		return parse_handles(bits);
	}
};

template<> struct ObjectT<MLINESTYLE> : Object {
	enum MLINEFLAGS {
		fill_on						= 1 << 0,
		display_miters				= 1 << 1,
		start_square_end_line_cap	= 1 << 4,
		start_inner_arcs_cap		= 1 << 5,
		start_round_outer_arcs_cap	= 1 << 6,
		end_square_line_cap			= 1 << 8,
		end_inner_arcs_cap			= 1 << 9,
		end_round_outer_arcs_cap	= 1 << 10,
	};
	struct Item {
		BD	offset;		// BD Offset of this segment
		CMC	color;		// CMC Color of this segment
		BS	lineindex;	// (before R2018, index)
		H	linetype;	// (before R2018, index)
		bool read(bitsin2 &bits) {
			return bits.read(offset, color) && (bits.ver >= R2018 || bits.read(lineindex));
		}
	};
	TV					name;
	TV					desc;
	BS					mlineflags;
	CMC					fillcolor;
	BD					angle0;
	BD					angle1;
	dynamic_array<Item>	items;

	bool parse(bitsin2 &bits) {
		parse_head(bits);
		bits.read(name, desc);
		flags |= bits.get<BS>();
		bits.read(fillcolor, angle0, angle1);
		
		items.read(bits, bits.get<uint8>());
		if (parse_handles(bits)) {
			if (bits.ver >= R2018)
				for (auto &i : items)
					bits.read(i.linetype);
		}
		return true;
	}
	friend tag2 _GetName(const ObjectT &entry)	{ return entry.name; }
};

template<> struct ObjectT<FIELD> : Object {
	enum EVAL_FLAGS {
		Never				= 0,
		OnOpen				= 1,
		OnSave				= 2,
		OnPlot				= 4,
		OnTransmit			= 8,
		OnRegeneration		= 16,
		OnDemand			= 32,
	};
	enum FILING_FLAGS {
		None				= 0,
		NoFileResult		= 1
	};
	enum STATE_FLAG {
		Unknown				= 0,
		Initialized			= 1,
		Compiled			= 2,
		Modified			= 4,
		Evaluated			= 8,
		Cached				= 16
	};
	enum EVAL_STATE_FLAGS {
		NotEvaluated		= 1,
		Success				= 2,
		EvaluatorNotFound	= 4,
		SyntaxError			= 8,
		InvalidCode			= 16,
		InvalidContext		= 32,
		OtherError			= 64
	};

	TV		EvaluatorID;
	TV		FieldCode;
	TV		FormatString; // R2004-

	dynamic_array<H>	children;
	dynamic_array<H>	objects;

	BL		EvaluationFlags;
	BL		FilingFlags;
	BL		StateFlags;
	BL		EvalStatusFlags;
	BL		EvalErrorCode;
	TV		EvaluationError;
	Value	value;
	TV		ValueString;
	TV		ValueStringLength;

	dynamic_array<pair<TV, Value>>	child_fields;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bits.read(EvaluatorID, FieldCode);

		children.resize(bits.get<BL>());
		objects.resize(bits.get<BL>());

		if (bits.ver <= R2004)
			bits.read(FormatString);

		bits.read(EvaluationFlags, FilingFlags, StateFlags, EvalStatusFlags, EvalErrorCode, EvaluationError, value, ValueString, ValueStringLength);

		child_fields.resize(bits.get<BL>());
		for (auto &i : child_fields) {
			i.a.read(bits);
			i.b.read(bits);
		}

		parse_handles(bits);
		bits.get<H>();//XRefH

		children.read(bits, children.size32());
		objects.read(bits, objects.size32());
		return true;
	}
};

template<> struct ObjectT<PLOTSETTINGS> : Object {
	TV			name;
	BD			marginLeft;		// Size, in millimeters, of unprintable margin on left side of paper, code 40
	BD			marginBottom;	// Size, in millimeters, of unprintable margin on bottom side of paper, code 41
	BD			marginRight;	// Size, in millimeters, of unprintable margin on right side of paper, code 42
	BD			marginTop;		// Size, in millimeters, of unprintable margin on top side of paper, code 43

	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& bits.read(name, marginLeft, marginBottom, marginRight, marginTop)
			&& parse_handles(bits);
	}
};

template<> struct ObjectT<TABLESTYLE> : Object {
	enum FLAGS {
		supress_title	= 1 << 1,
		supress_header	= 1 << 2,
	};

	TV	description;

	// 2007-
	BS		flow_dir;
	BS		style_flags;
	BD		hmargin;
	BD		vmargin;
	RowStyle	data, title, header;

	// 2010+
	CellStyle					cellstyle;
	dynamic_array<CellStyle>	cellstyles;


	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		bitsin	hbits(bits);
		parse_handles(hbits);
		bitsin3	bits3(bits, hbits);

		if (bits.ver <= R2007) {
			bits3.read(description, flow_dir, style_flags, hmargin, vmargin);
			flags |= bits.get_bits(2);
			return bits3.read(data, title, header);

		}  else {

			bits3.get<uint8>();
			bits3.read(description);
			bits3.discard<BL, BL>();
			bits3.get<H>();

			bits3.read(cellstyle);
			cellstyles.resize(bits3.get<BL>());
			for (auto &i : cellstyles) {
				bits3.discard<BL>();
				bits3.read(i);
			}
			return true;
			//return cellstyles.read(bits3, bits3.get<BL>());

		}
	}
};


template<> struct ObjectT<IDBUFFER> : Object {
	uint8	unknown;
	BL		num_obj_ids;

	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;

		//SUBCLASS (AcDbIdBuffer)
		return bits.read(unknown, num_obj_ids)
			&& parse_handles(bits);
	}
};

struct AcDbObjectContextData {
	BS		class_version;
	B		is_default;
	bool	read(bitsin &bits) {
		return bits.read(class_version, is_default);
	}
};

struct AcDbAnnotScaleObjectContextData : AcDbObjectContextData {
	H		scale;
};

struct AcDbTextObjectContextData {
	BS		horizontal_mode;
	BD		rotation;
	RD2		ins_pt;
	RD2		alignment_pt;
	bool	read(bitsin &bits) {
		return bits.read(horizontal_mode, rotation, ins_pt, alignment_pt);
	}
};

template<typename T, T B> struct _bit {
	T	&t;
	_bit(T &t) : t(t) {}
	T& operator=(bool b) { return t = b ? (t | B) : (t & ~B); }
};

template<uint32 X, typename T> auto as_bit(T &t) { return read_as<B>(_bit<T, X>(t)); }

struct AcDbDimensionObjectContextData {
	enum DIM_FLAGS {
		is_def_textloc	= 1 << 0,
		b293		    = 1 << 1,
		dimtofl	        = 1 << 2,
		dimosxd	        = 1 << 3,
		dimatfit	    = 1 << 4,
		dimtix	        = 1 << 5,
		dimtmove	    = 1 << 6,
		has_arrow2	    = 1 << 7,
		flip_arrow2	    = 1 << 8,
		flip_arrow1	    = 1 << 9,
	};
	uint8	flags;
	RD2		def_pt;
	BD		text_rotation;
	H		block;
	uint8	override_code;
	bool	read(bitsin &bits) {
		return bits.read(def_pt,
			as_bit<is_def_textloc>(flags),
			text_rotation, block,
			as_bit<b293>(flags),
			as_bit<dimtofl>(flags),
			as_bit<dimosxd>(flags),
			as_bit<dimatfit>(flags),
			as_bit<dimtix>(flags),
			as_bit<dimtmove>(flags),
			override_code,
			as_bit<has_arrow2>(flags),
			as_bit<flip_arrow2>(flags),
			as_bit<flip_arrow1>(flags)
		);
	}
};

#if 0
template<> struct ObjectT<ACDB_ANNOTSCALEOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData {
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_ANGDIMOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbDimensionObjectContextData {
	BD3		arc_pt;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits)
			&& AcDbDimensionObjectContextData::read(bits)
			&& bits.read(arc_pt)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_DMDIMOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbDimensionObjectContextData {
	BD3		first_arc_pt;
	BD3		def_pt;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits) && AcDbDimensionObjectContextData::read(bits)
			&& bits.read(first_arc_pt, def_pt)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_ORDDIMOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbDimensionObjectContextData {
	BD3		feature_location_pt;
	BD3		leader_endpt;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits) && AcDbDimensionObjectContextData::read(bits)
			&& bits.read(feature_location_pt, leader_endpt)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_RADIMOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbDimensionObjectContextData {
	BD3		first_arc_pt;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits) && AcDbDimensionObjectContextData::read(bits)
			&& bits.read(first_arc_pt jog_point)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_RADIMLGOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbDimensionObjectContextData {
	BD3		ovr_center;
	BD3		jog_point;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits) && AcDbDimensionObjectContextData::read(bits)
			&& bits.read(ovr_center, jog_point)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_MLEADEROBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData {
	// ?? ...
};

template<> struct ObjectT<ACDB_ALDIMOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbDimensionObjectContextData {
	BD3		dimline_pt;
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits) && AcDbDimensionObjectContextData::read(bits)
			&& bits.read(dimline_pt)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_MTEXTOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData {
	BL	attachment;
	//MTEXT ;
	BD3	x_axis_dir;
	BD3	ins_pt;
	BD	rect_width;
	BD	rect_height;
	BD	extents_width;
	BD	extents_height;
	BL	column_type;

	bool parse(bitsin2 &bits) {
		if (FIELD_VALUE (column_type))
		{
			FIELD_BL (num_column_heights, 72);
			FIELD_BD (column_width, 44);
			FIELD_BD (gutter, 45);
			FIELD_B (auto_height, 73);
			FIELD_B (flow_reversed, 74);
			if (!FIELD_VALUE (auto_height) && FIELD_VALUE (column_type) == 2)
				FIELD_VECTOR (column_heights, BD, num_column_heights, 46);
		}
	}
};
template<> struct ObjectT<ACDB_TEXTOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbTextObjectContextData {
	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits) && AcDbTextObjectContextData::read(bits)
			&& parse_handles(bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_LEADEROBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData {
	dynamic_array<RD3> points;
	RD3		x_direction;
	B		b290;
	RD3		inspt_offset;
	RD3		endptproj;

	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits)/;.
			&& points.read(bits, bit.get<BL>())
			&& bits.read(x_direction, b290, inspt_offset, endptproj)
			&& parse_handles(bits) && bits.read(scale);
	}

};

// TOLERANCE
template<> struct ObjectT<ACDB_FCFOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData {
	BD3		location;
	BD3		horiz_dir;

	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits)
			&& bits.read(location, horiz_dir)
			&& parse_handles(bits) && bits.read(scale);
	}
};
#endif

template<> struct ObjectT<ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData, AcDbTextObjectContextData {
	bool parse(bitsin2 &bits) {
		if (!parse_head(bits))
			return false;
		AcDbObjectContextData::read(bits);
		AcDbTextObjectContextData::read(bits);

		if (bits.get_bit()) {
			//enable_context
			//dwg_add_object (dwg);
			//context = &dwg->object[dwg->num_objects - 1];
			//dwg_setup_SCALE (_obj->context);
			//CALL_ENTITY (SCALE, _obj->context);
		}
		return parse_handles((bitsin&)bits) && bits.read(scale);
	}
};

template<> struct ObjectT<ACDB_BLKREFOBJECTCONTEXTDATA_CLASS> : Object, AcDbAnnotScaleObjectContextData {
	BD		rotation;
	BD3		ins_pt;
	BD3		scale_factor;

	bool parse(bitsin2 &bits) {
		return parse_head(bits)
			&& AcDbObjectContextData::read(bits)
			&& bits.read(rotation, ins_pt, scale_factor)
			&& parse_handles((bitsin&)bits) && bits.read(scale);
	}
};

struct DeleteObject {
	Object	*obj;
	template<OBJECTTYPE T> void operator()() { delete (ObjectT<T>*)obj; }
	DeleteObject(Object	*obj) : obj(obj) {}
};

void Object::destroy() {
	switchT(DeleteObject(this), type, ALL_OBJECTTYPES());
}

//-----------------------------------------------------------------------------
// Reader
//-----------------------------------------------------------------------------

struct HeaderBase {
	char			version[11];
	uint8			maint_ver, one;
	packed<uint32>	image_seeker;	//0x0d
	uint8			app_ver;		//0x11
	uint8			app_maint_ver;	//0x12
	packed<uint16>	codepage;		//0x13

	VER	valid() const {
		uint32	v;
		return version[0] == 'A' && version[1] == 'C' && from_string(version + 2, v) == 4 ? VER(v) : BAD_VER;
	}
};

static uint8 fileheader_sentinel[16]	= {0x95,0xA0,0x4E,0x28,0x99,0x82,0x1A,0xE5,0x5E,0x41,0xE0,0x5F,0x9D,0x3A,0x4D,0x00};

static uint8 header_sentinel[16]		= {0xCF,0x7B,0x1F,0x23,0xFD,0xDE,0x38,0xA9,0x5F,0x7C,0x68,0xB8,0x4E,0x6D,0x33,0x5F};
static uint8 header_sentinel_end[16]	= {0x30,0x84,0xE0,0xDC,0x02,0x21,0xC7,0x56,0xA0,0x83,0x97,0x47,0xB1,0x92,0xCC,0xA0};

static uint8 classes_sentinel[16]		= {0x8D,0xA1,0xC4,0xB8,0xC4,0xA9,0xF8,0xC5,0xC0,0xDC,0xF4,0x5F,0xE7,0xCF,0xB6,0x8A};
static uint8 classes_sentinel_end[16]	= {0x72,0x5E,0x3B,0x47,0x3B,0x56,0x07,0x3A,0x3F,0x23,0x0B,0xA0,0x18,0x30,0x49,0x75};

bool check_sentinel(const uint8 *data, const uint8 sentinel[16]) {
	return memcmp(data, sentinel, 16) == 0;
}

bool check_sentinel(istream_ref file, const uint8 sentinel[16]) {
	uint8	data[16];
	return file.read(data) && check_sentinel(data, sentinel);
}

class DWG {
	struct Class {
		enum FLAGS {
			erase_allowed					= 1 << 0,
			transform_allowed				= 1 << 1,
			color_change_allowed			= 1 << 2,
			layer_change_allowed			= 1 << 3,
			line_type_change_allowed		= 1 << 4,
			line_type_scale_change_allowed	= 1 << 5,
			visibility_change_allowed		= 1 << 6,
			cloning_allowed					= 1 << 7,
			lineweight_change_allowed		= 1 << 8,
			plot_Style_Name_change_allowed	= 1 << 9,
			disable_proxy_warning_dialog	= 1 << 10,
			is_R13_format_proxy				= 1 << 15,
			wasazombie						= 1 << 16,
			makes_entities					= 1 << 17,
		};
		OBJECTTYPE	type;
		uint32		flags;
		TV			appName;
		TV			cName;
		TV			dxfName;
		int			count;
		BS			version, maintenance;

		bool read(bitsin2 &bits) {
			flags		= bits.get<BS>();
			bits.read(appName);
			bits.read(cName);
			bits.read(dxfName);
			flags		|= bits.get_bit() * wasazombie;
			flags		|= bits.get<BS>() == ACAD_PROXY_ENTITY ? makes_entities : 0;
			count		= bits.get<BL>();

			bits.read(version);
			bits.read(maintenance);
			bits.discard<BL, BL>();
			return true;
		}
	};

	struct ObjectHandle {
		uint32		handle;
		uint32		loc;
		Object		*obj		= 0;
		bool		extracted	= false;
		ObjectHandle(uint32 handle = 0, uint32 loc = 0) : handle(handle), loc(loc) {}
		bool operator<(uint32 h) const { return handle < h; }
	};

	sparse_array<Class>				classes;
	dynamic_array<ObjectHandle>		handles;
	
	bool	read_header(bitsin3 &&bits)	{ return vars.parse(bits); }
	bool	read_classes(bitsin2 &&bits, uint32 size);
	bool	read_tables(istream_ref file);
	bool	read_handles(istream_ref file);

	bool	read12(const HeaderBase* h, istream_ref file);
	bool	read18(const HeaderBase* h, istream_ref file);
	bool	read21(const HeaderBase* h, istream_ref file);
	

public:
	Object*	get_object(uint32 handle);

	template<OBJECTTYPE T, OBJECTTYPE CT> struct Table {
		struct iterator {
			DWG		*dwg;
			uint32	*h;
			iterator(DWG *dwg, uint32 *h) : dwg(dwg), h(h) {}
			bool		operator!=(const iterator &b) const { return h != b.h; }
			iterator&	operator++()		{ ++h; return *this; }
			auto		operator*()	const	{ return make_param_element(*(ObjectT<T>*)dwg->get_object(*h), dwg); }
		};

		ObjectT<CT>	*control;
		DWG			*dwg;

		bool		read(DWG *dwg, istream_ref file, H ctrl) {
			this->dwg	= dwg;
			control		= (ObjectT<CT>*)dwg->get_object(ctrl);
			return control && control->type == CT;
		}

		iterator	begin()				const { return {dwg, control->handles.begin()}; }
		iterator	end()				const { return {dwg, control->handles.end()}; }
		auto		operator[](int i)	const { return *nth(begin(), i); }
	};

	reader_intf				object_file;
	uint16					code_page;
	VER						version;
	uint8					maintenanceVersion;
	string					comments;
	string					name;

	HeaderVars				vars;

	Table<BLOCK_HEADER,	BLOCK_CONTROL_OBJ>		blocks;
	Table<LAYER,		LAYER_CONTROL_OBJ>		layers;
	Table<STYLE,		STYLE_CONTROL_OBJ>		textstyles;
	Table<LTYPE,		LTYPE_CONTROL_OBJ>		linetypes;
	Table<VIEW,			VIEW_CONTROL_OBJ>		views;
	Table<UCS,			UCS_CONTROL_OBJ>		ucs;
	Table<VPORT,		VPORT_CONTROL_OBJ>		vports;
	Table<APPID,		APPID_CONTROL_OBJ>		appids;
	Table<DIMSTYLE,		DIMSTYLE_CONTROL_OBJ>	dimstyles;
	Table<VP_ENT_HDR,	VP_ENT_HDR_CTRL_OBJ>	vpEntHeaders;
	Table<GROUP,		DICTIONARY>				groups;
	Table<MLINESTYLE,	DICTIONARY>				mlinestyles;
	Table<LAYOUT,		DICTIONARY>				layouts;
	Table<PLOTSETTINGS,	DICTIONARY>				plotsettings;

	bool	read(const HeaderBase* h, istream_ref file);
};

HandleRange::iterator::iterator(DWG *dwg, const HandleRange &range, uint32 h, bool end) : dwg(dwg), h(h) {
	if (dwg->version <= R2000) {
		next	= range.handles[end].get_offset(h);
		p		= 0;
	} else {
		p		= end ? range.handles.end() - 1 : range.handles.begin();
	}
}

Entity *HandleRange::iterator::operator*() {
	if (p)
		next = p->get_offset(h);
	return ent = (Entity*)dwg->get_object(next);
}

HandleRange::iterator&	HandleRange::iterator::operator++()	{
	if (p) {
		++p;
	} else {
		if (!ent)
			operator*();
		next = ent->next_ent;
	}
	ent = nullptr;
	return *this;
}

bool HandleRange::iterator::operator!=(const iterator &b) const {
	return p ? p != b.p : next != b.next;
}

bool DWG::read_tables(istream_ref file) {
	object_file	= file.clone();

	bool ret = true;

	ret &= blocks.read		(this, file, vars.BLOCK_CONTROL);
	ret &= layers.read		(this, file, vars.LAYER_CONTROL);
	ret &= textstyles.read	(this, file, vars.TEXTSTYLE_CONTROL);
	ret &= linetypes.read	(this, file, vars.LINETYPE_CONTROL);
	ret &= views.read		(this, file, vars.VIEW_CONTROL);
	ret &= ucs.read			(this, file, vars.UCS_CONTROL);
	ret &= vports.read		(this, file, vars.VPORT_CONTROL);
	ret &= appids.read		(this, file, vars.APPID_CONTROL);
	ret &= dimstyles.read	(this, file, vars.DIMSTYLE_CONTROL);

	if (version <= R2000)
		ret &= vpEntHeaders.read(this, file, vars.VP_ENT_HDR_CONTROL);

	ret &= groups.read		(this, file, vars.GROUP_CONTROL);
	ret &= mlinestyles.read	(this, file, vars.MLINESTYLE_CONTROL);
	ret &= layouts.read		(this, file, vars.LAYOUTS_CONTROL);
	ret &= plotsettings.read(this, file, vars.PLOTSETTINGS_CONTROL);

	return ret;
}

bool DWG::read_classes(bitsin2 &&bits, uint32 size) {
	while (bits.tell_bit() < size) {
		uint32	classnum = bits.get<BS>();
		auto	&c	= classes[classnum].put();
		c.read(bits);
		c.type = TypeFromName(c.dxfName);
		ISO_ASSERT(c.type);
	}
	return true;
}

bool DWG::read_handles(istream_ref file) {
	while (!file.eof()) {
		uint16 size = file.get<uint16be>();

		file.seek_cur(-2);
		temp_block		temp(file, size);
		memory_reader	mr2(temp + 2);
		uint32 handle	= 0;
		uint32 loc		= 0;

		while (!mr2.eof()) {
			handle	+= mr2.get<MC>();
			loc		+= mr2.get<MCS>();
			handles.emplace_back(handle, loc);
		}

		//verify crc
		uint16 crc_calc = crc<16>(uint16(0xc0c1)) << temp;
		uint16 crc_read = file.get<uint16be>();
		if (crc_calc != crc_read)
			return false;
	}

	return true;
}

template<typename R> struct make_read_object {
	R		&bits;

	template<OBJECTTYPE T> ObjectT<T>* operator()() {
		auto	*t = new ObjectT<T>;
		if (t->parse(bits)) {
			if (auto r = bits.remaining())
				ISO_TRACEF("remaining= ") << r << " in " << NameFromType(T) << '\n';
			return t;
		}
		delete t;
		return nullptr;
	}
	make_read_object(R &&bits) : bits(bits) {}
};

Object *DWG::get_object(uint32 handle) {
	auto mit = lower_boundc(handles, handle);
	if (mit == handles.end())
		return nullptr;

	if (auto obj = mit->obj)//.get())
		return obj;

	mit->extracted	= true;
	object_file.seek(mit->loc);

	int size = object_file.get<uint16>();
	if ((size & 0x8000) != 0)
		size += (object_file.get<uint16>() << 15) - 0x8000;

	uint32	bsize	= version >= R2010 ? size * 8 - object_file.get<MC>() : size * 8;
	auto	offset	= object_file.tell() - mit->loc;

	object_file.seek(mit->loc);
	temp_block		data(object_file, size + offset + sizeof(uint16));

	uint16 crc_calc = iso::crc<16>(uint16(0xc0c1)) << data;
	if (crc_calc)
		return nullptr;

	bitsin			bits(data.slice(offset, -2), version);
	bits.size	= bsize;

	OBJECTTYPE		type = readOT(bits);
	bits.seek_bit(0);

	if (type >= _LOOKUP) {
		auto it = classes[type];
		if (!it.exists())
			return nullptr;

		if (auto t = it->type)
			type = t;
	}

	mit->extracted	= true;
	Object	*o		= nullptr;
	if (auto soffset = get_string_offset(bits, bsize)) {
		bitsin	sbits(bits);
		return mit->obj = switchT<Object*>(make_read_object<bitsin2>(bitsin2(bits, sbits, soffset)), type, ALL_OBJECTTYPES());
	}
	return mit->obj = switchT<Object*>(make_read_object<bitsin2>(bits), type, ALL_OBJECTTYPES());
}

//-----------------------------------------------------------------------------
// R12
//-----------------------------------------------------------------------------

bool DWG::read12( const HeaderBase* h0, istream_ref file) {
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
	auto	crc	= file.get<uint16>();
	bool	ret	= crc == ckcrc && check_sentinel(file, fileheader_sentinel);
	
	{//header
		auto &si = sections[HEADER];
		file.seek(si.address);

		temp_block		data(si.size);
		ret &= check_sentinel(data, header_sentinel);
		SectionStart	*start	= data;

		bitsin			bits(data, version);
		bitsin2			bits2(bits);
		ret &= read_header(bits2);
	}
		
	{// classes
		auto &si = sections[CLASSES];
		file.seek(si.address);

		temp_block		data(si.size);
		ret &= check_sentinel(data, classes_sentinel);
		SectionStart	*start	= data;

		bitsin			bits(data, version);
		ret &= read_classes(bits, (start->size - 1) * 8);
	}

	{// handles
		auto &si = sections[HANDLES];
		file.seek(si.address);
		temp_block		data(si.size);
		ret &= read_handles(memory_reader(data));
	}

	return ret & read_tables(file);
}

//-----------------------------------------------------------------------------
// R18
//-----------------------------------------------------------------------------

struct decompress18 {
	static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
};

const uint8* decompress18::process(uint8 *&dst0, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	int litCount	= 0;

	uint8	*dst = dst0;

	while (dst < dst_end) {
		if (litCount == 0) {
			int b = *src++;
			if (b > 0x0F) {
				// no literal length, this byte is next opCode
				--src;

			} else {
				if (b == 0) {
					litCount = 0x0f;
					b = *src++;
					while (b == 0) {
						litCount += 0xFF;
						b = *src++;
					}
				}

				litCount += b + 3;
			}
		}

		for (int i = 0; i < litCount; ++i)
			*dst++ = *src++;

		int oc = *src++;
		int compBytes, compOffset;

		if (oc < 0x40) {
			if (oc < 0x10)
				break;//return false;
			if (oc == 0x11)
				break;//return true;

			if (oc == 0x10 || oc == 0x20) {
				compBytes = 0;
				int b = *src++;
				while (b == 0) {
					compBytes += 0xFF;
					b = *src++;
				}
				compBytes += b + (oc == 0x10 ? 0x09 : 0x21);

			} else {
				compBytes = oc - (oc < 0x20 ? 0x0e : 0x1e);
			}

			compOffset	= oc < 0x20 ? 0x3FFF : 0;
			oc 			= *src++;
			compOffset 	+= (*src++ << 6) | (oc >> 2);

		} else {
			compBytes	= (oc >> 4) - 1;
			compOffset	= (*src++ << 2) | ((oc & 0x0C) >> 2);
		}


		int	offset	= -(compOffset + 1);
		while (compBytes--) {
			dst[0] = dst[offset];
			++dst;
		}

		litCount = oc & 3;
	}

	dst0 = dst;
	return src;
}

uint32 checksum18(uint32 seed, uint8* data, uint32 size) {
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

bool DWG::read18(const HeaderBase* h0, istream_ref file) {
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

	static const uint8 MAGIC[] = {
		0xf8, 0x46, 0x6a, 0x04, 0x96, 0x73, 0x0e, 0xd9,
		0x16, 0x2f, 0x67, 0x68, 0xd4, 0xf7, 0x4a, 0x4a,
		0xd0, 0x57, 0x68, 0x76
	};

	struct SystemPage {
		uint32	page_type;			// SYS_SECTION or MAP_SECTION
		uint32	decompressed_size;
		uint32	compressed_size;
		uint32	compression_type;	//2
		uint32	header_checksum;

		malloc_block parse(istream_ref file) {
			SystemPage	sys	= *this;;
			sys.header_checksum	= 0;
			uint32 calcsH = checksum18(0, (uint8*)&sys, sizeof(sys));

			malloc_block	data(file, compressed_size);
			uint32 calcsD = checksum18(calcsH, data, compressed_size);

			malloc_block	out(decompressed_size);
			transcode(decompress18(), out, data);

			return move(out);
		}
	};

	struct DataPage {
		uint32	page_type;			// DATA_SECTION
		uint32	section;
		uint32	compressed_size;
		uint32	decompressed_size;
		uint32	offset;				// (in the decompressed buffer)
		uint32	header_checksum;	// section page checksum calculated from unencoded header bytes, with the data checksum as seed
		uint32	data_checksum;		// section page checksum calculated from compressed data bytes, with seed 0
		uint32	Unknown;
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
		sparse_array<Entry2, uint32, uint16>	entries;

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
					uint32	Page;	// index into SectionPageMap, starts at 1
					uint32	size;	// for this page (compressed size).
					uint64	offset;	// offset for this page
				};

				uint64	size;
				uint32	PageCount;
				uint32	MaxDecompressedSize;	// Size of a section page of this type (normally 0x7400)
				uint32	Unknown;
				uint32	compression_type;		// 1 = no, 2 = yes, normally 2
				uint32	SectionId;				// (starts at 0). The first section (empty section) is numbered 0, consecutive sections are numbered descending from (the number of sections – 1) down to 1.
				uint32	Encrypted;				// 0 = no, 1 = yes, 2 = unknown
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
					auto	h = file.get<DataPage>();
					uint32	x = 0x4164536b ^ i.address;
					for (int j = 1; j < 8; j++)
						((uint32*)&h)[j] ^= x;

					//get compressed data
					malloc_block	data(file, h.compressed_size);

					//calculate checksum
					uint32 calcsD = checksum18(0, data, h.compressed_size);
					h.header_checksum	= 0;
					uint32 calcsH = checksum18(calcsD, (uint8*)&h, sizeof(h));

					transcode(decompress18(), page_out, data);
					page_out.slice_to(size - i.offset).copy_to(out + i.offset);
				}
				return move(out);
			}
		};

		hash_map<string, Section>	sections;

		SectionMap(const_memory_block mem, const PageMap &page_map) {
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

	struct FileHeader {
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
	};

	struct SectionStart {
		uint8	sentinel[16];
		uint32	size;
		uint32	hsize;
	};

	auto	h = (Header*)h0;

	FileHeader	fh;
	file.seek(0x80);
	file.read(fh);

	int		seed = 1;
	for (auto p = (uint8*)&fh, e = p + sizeof(fh); p != e; ++p) {
		seed = (seed * 0x343fd) + 0x269ec3;
		*p ^= seed >> 16;
	}

	auto	crc = crc32(fh);
	if (memcmp(file.get<array<uint8,20>>(), MAGIC, 20) != 0)
		return false;

	malloc_block	data;

	file.seek(fh.section_page_map_addr + 0x100);
	auto	page = file.get<SystemPage>();
	if (page.page_type != SYS_SECTION || !(data = page.parse(file)))
		return false;

	PageMap	page_map(data);

	auto sectionMap = page_map.entries[fh.section_map];
	file.seek(sectionMap.get()->address);
	file.read(page);

	if (page.page_type != MAP_SECTION || !(data = page.parse(file)))
		return false;

	SectionMap	sections(data, page_map);
	bool	ret = true;

	//read_header
	{
		data	= sections.data(file, "AcDb:Header");
		SectionStart	*start = data;

		ret		&= check_sentinel(data, header_sentinel);
		ret		&= check_sentinel(data.slice(sizeof(SectionStart) + sizeof(uint16) + start->size), header_sentinel_end);

		uint16 crc_calc = iso::crc<16>(uint16(0xc0c1)) << data.slice(16, start->size + 10);
		ret		&= crc_calc == 0;

		bitsin	bits(data.slice(sizeof(SectionStart)), version);
		uint32	bitsize = bits.get<uint32>();

		bitsin			hbits(bits);
		hbits.seek_bit(bitsize);

		if (auto soffset = get_string_offset(bits, bitsize)) {
			bitsin	sbits(bits);
			bitsin2	bits2(bits, sbits, soffset);
			ret &= read_header(bitsin3(bits2, hbits));
		} else {
			bitsin2	bits2(bits);
			ret &= read_header(bitsin3(bits2, hbits));
		}
	}

	//read_classes
	{
		data	= sections.data(file, "AcDb:Classes");
		SectionStart	*start = data;

		ret		&= check_sentinel(data, classes_sentinel);
		ret		&= check_sentinel(data.slice(sizeof(SectionStart) + sizeof(uint16) + start->size), classes_sentinel_end);

		uint16 crc_calc = iso::crc<16>(uint16(0xc0c1)) << data.slice(16, start->size + 10);
		ret		&= crc_calc == 0;

		bitsin	bits(data.slice(sizeof(SectionStart)), version);
		uint32	bitsize = bits.get<uint32>();

		uint32	maxClassNum = bits.get<BS>();
		uint8	Rc1	= bits.get<uint8>();
		uint8	Rc2	= bits.get<uint8>();
		bool	Bit	= bits.get_bit();

		if (auto soffset = get_string_offset(bits, bitsize)) {
			bitsin			sbits(bits);
			ret &= read_classes(bitsin2(bits, sbits, soffset), soffset);
		} else {
			ret &= read_classes(bits, bitsize);
		}
	}

	//read_handles
	ret &= read_handles(memory_reader(sections.data(file, "AcDb:Handles")));

	//read_tables
	return ret & read_tables(memory_reader_owner(sections.data(file, "AcDb:AcDbObjects")));
}

//-----------------------------------------------------------------------------
// R21
//-----------------------------------------------------------------------------

struct decompress21 {
	enum {
		MaxBlockLength = 32,
		BlockOrderArray,
	};
	static const uint8 *CopyOrder[];
	
	static const uint8* process(uint8 *&dst0, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
};

const uint8* decompress21::process(uint8 *&dst0, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	int	length	= 0;
	int	opCode	= *src++;

	if ((opCode >> 4) == 2) {
		src		+= 2;
		length	= *src++ & 0x07;
	}

	uint8	*dst = dst0;
	while (dst < dst_end) {
		if (length == 0) {
			//litlength
			length = 8 + opCode;
			if (length == 0x17) {
				int n = *src++;
				length += n;
				if (n == 0xff) {
					do {
						n = *src++;
						n |= *src++ << 8;
						length += n;
					} while (n == 0xffff);
				}
			}
		}

		while (length != 0) {
			int	n = min(length, MaxBlockLength);
			auto order = CopyOrder[n];
			for (uint32 index = 0; n > index; ++index)
				*dst++ = src[order[index]];
			src		+= n;
			length	-= n;
		}

		length = 0;
		opCode = *src++;
		for (;;) {
			int	sourceOffset	= 0;

			switch (opCode >> 4) {
				case 0:
					length			= (opCode & 0x0f) + 0x13;
					sourceOffset	= *src++;
					opCode			= *src++;
					length			+= ((opCode >> 3) & 0x10);
					sourceOffset	+= ((opCode & 0x78) << 5) + 1;
					break;
				case 1:
					length			= (opCode & 0xf) + 3;
					sourceOffset	= *src++;
					opCode			= *src++;
					sourceOffset	+= ((opCode & 0xf8) << 5) + 1;
					break;
				case 2:
					sourceOffset	= *src++;
					sourceOffset	|= (*src++ << 8);
					length			= opCode & 7;
					if ((opCode & 8) == 0) {
						opCode		= *src++;
						length		+= opCode & 0xf8;
					} else {
						++sourceOffset;
						length		+= *src++ << 3;
						opCode		= *src++;
						length		+= ((opCode & 0xf8) << 8) + 0x100;
					}
					break;
				default:
					length			= opCode >> 4;
					sourceOffset	= opCode & 15;
					opCode			= *src++;
					sourceOffset	+= ((opCode & 0xf8) << 1) + 1;
					break;
			}

			while (length--) {
				dst[0] = dst[-sourceOffset];
				++dst;
			}

			length = opCode & 7;
			if (length != 0)
				break;

			opCode = *src++;
			if ((opCode >> 4) == 0)
				break;

			if ((opCode >> 4) == 15)
				opCode &= 15;
		}
	}

	dst0 = dst;
	return src;
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

bool DWG::read21(const HeaderBase* h0, istream_ref file) {
	struct Header : HeaderBase {
		uint8	padding[3];	//0x15
		uint32	security;	//0x18
		uint32	unknown;
		uint32	summary;
		uint32	vba_project;
		uint32	_0x80;		// offset to FileHeaderHeader?
		uint32	app_info_addr;
		uint8	padding2[0x50];
	};

	struct FileHeaderHeader {
		uint64	crc;
		uint64	unknown_key;
		uint64	compressed_crc;
		uint32	compressed_size;	//(if < 0, not compressed)
		uint32	uncompressed_size;
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
				uint64 offset;
				uint64 size;
				uint64 id;
				uint64 uncompressed_size;
				uint64 compressed_size;
				uint64 checksum;
				uint64 crc;
			};
			uint64	DataSize;
			uint64	MaxSize;
			uint64	Encryption;
			uint64	HashCode;
			uint64	SectionNameLength;
			uint64	Unknown;
			uint64	Encoding;
			uint64	NumPages;
			char16	SectionName[];
			//Page	pages[]

			auto	name()	const	{ return SectionNameLength ? SectionName : nullptr; }
			auto	pages() const	{ return make_range_n((Page*)(SectionName + SectionNameLength + (SectionNameLength != 0)), NumPages); }
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
					
					transcode(decompress21(), out.slice(i.offset, i.size), data_rs);
				}
				return move(out);
			}
		};

		hash_map<string, Section>	sections;

		SectionMap(const_memory_block mem, const PageMap &page_map) {
			uint8 nextId = 1;
			for (auto &i : make_next_range<const Description>(mem)) {
				auto&	section	= sections[i.SectionName].put();
				for (auto &p : i.pages())
					section.pages.push_back(Section::Page{p.id, p.size, p.offset, p.compressed_size});
			}
		}

		malloc_block	data(istream_ref file, const char *name) {
			auto si = sections[name];
			return si.exists() ? si.get()->parse(file) : none;
		}
	};

	auto parseSysPage = [&](uint64 sizeCompressed, uint64 sizeUncompressed, uint64 correctionFactor, uint64 offset)->malloc_block {
		uint32 chunks = div_round_up(align(sizeCompressed, 8) * correctionFactor, 239);
		uint64 fpsize = chunks * 255;

		file.seek(offset);
		temp_block	data(file, fpsize);
		temp_block	data_rs(fpsize);
		decodeI<239, 0x96, 8, 8>(data, data_rs, chunks);

		malloc_block	out(sizeUncompressed);
		transcode(decompress21(), out, data_rs);
		return out;
	};

	struct SectionStart {
		uint8	sentinel[16];
		uint32	size;
		uint32	hsize;
	};

	auto	h = (Header*)h0;

	file.seek(0x80);//h->_0x80?

	uint8 fileHdrRaw[0x2FD];//0x3D8
	uint8 fileHdrdRS[0x2CD];
 
	file.read(fileHdrRaw);
	decodeI<239, 0x96, 8, 8>(fileHdrRaw, fileHdrdRS, 3);

	FileHeaderHeader	*fhh	= (FileHeaderHeader*)fileHdrdRS;
	malloc_block		fh_data;

	if (fhh->compressed_size < 0) {
		fh_data.resize(-fhh->compressed_size);
		fh_data.copy_from(fhh + 1);
	} else {
		transcode(decompress21(), fh_data, const_memory_block(fhh + 1, fhh->compressed_size));
	}

	FileHeader		*fh	= fh_data;
	malloc_block	data;
	if (!(data = parseSysPage(fh->PagesMapSizeCompressed, fh->PagesMapSizeUncompressed, fh->PagesMapCorrectionFactor, 0x480+fh->PagesMapOffset)))
		return false;

	PageMap		page_map(data);
	auto		sectionMap = page_map.entries[fh->SectionsMapId];

	if (!(data = parseSysPage(fh->SectionsMapSizeCompressed, fh->SectionsMapSizeUncompressed, fh->SectionsMapCorrectionFactor, sectionMap.get()->address)))
		return false;

	SectionMap	sections(data, page_map);

	bool	ret = true;
	//read_header
	{
		data	= sections.data(file, "AcDb:Header");
		if (!check_sentinel(data, header_sentinel))
			return false;

		SectionStart	*start = data;
		bitsin			bits(data.slice(sizeof(SectionStart)), version);
		uint32			bitsize = bits.get<uint32>();

		bitsin			hbits(bits);
		hbits.seek_bit(bitsize);

		if (auto soffset = get_string_offset(bits, bitsize)) {
			bitsin		sbits(bits);
			return read_header(bitsin2(bits, sbits, soffset));
		}
		bitsin2	bits2(bits);
		ret &= read_header(bitsin3(bits2, hbits));
	}

	//read_classes
	{
		data	= sections.data(file, "AcDb:Classes");
		if (!check_sentinel(data, classes_sentinel))
			return false;

		SectionStart	*start = data;
		bitsin			bits(data.slice(sizeof(SectionStart)), version);
		uint32			bitsize = bits.get<uint32>();

		uint32	maxClassNum = bits.get<BS>();
		uint8	Rc1	= bits.get<uint8>();
		uint8	Rc2	= bits.get<uint8>();
		bool	Bit	= bits.get_bit();

		if (auto soffset = get_string_offset(bits, bitsize)) {
			bitsin	sbits(bits);
			ret &= read_classes(bitsin2(bits, sbits, soffset), soffset);
		} else {
			ret &= read_classes(bits, bitsize);
		}
	}


	//read_handles
	ret &= read_handles(memory_reader(sections.data(file, "AcDb:Handles")));

	//read_tables
	return ret & read_tables(memory_reader_owner(sections.data(file, "AcDb:AcDbObjects")));
}

bool DWG::read(const HeaderBase* h, istream_ref file) {
	switch (version = h->valid()) {
		case dwg::R13:
		case dwg::R14:
		case dwg::R2000:	return read12(h, file);
		case dwg::R2004:	return read18(h, file);
		case dwg::R2007:	return read21(h, file);
		case dwg::R2010:
		case dwg::R2013:
		case dwg::R2018:	return read18(h, file);
		default:			return false;
	}
}

} // namespace dwg

//-----------------------------------------------------------------------------
// ISO stuff
//-----------------------------------------------------------------------------

ISO_DEFUSERENUM(dwg::OBJECTTYPE, 113) {
	ISO_SETENUMSQ(0, 
	UNUSED,					TEXT,					ATTRIB,					ATTDEF,					BLOCK,					ENDBLK,					SEQEND,								INSERT,
	MINSERT,				VERTEX_2D,				VERTEX_3D,				VERTEX_MESH,			VERTEX_PFACE,			VERTEX_PFACE_FACE,		POLYLINE_2D,						POLYLINE_3D,
	ARC,					CIRCLE,					LINE,					DIMENSION_ORDINATE,		DIMENSION_LINEAR,		DIMENSION_ALIGNED,		DIMENSION_ANG_PT3,					DIMENSION_ANG_LN2,
	DIMENSION_RADIUS,		DIMENSION_DIAMETER,		POINT,					FACE_3D,				POLYLINE_PFACE,			POLYLINE_MESH,			SOLID,								TRACE,
	SHAPE,					VIEWPORT,				ELLIPSE,				SPLINE,					REGION,					SOLID_3D,				BODY,								RAY,
	XLINE,					DICTIONARY,				OLEFRAME,				MTEXT,					LEADER,					TOLERANCE,				MLINE,								BLOCK_CONTROL_OBJ,
	BLOCK_HEADER,			LAYER_CONTROL_OBJ,		LAYER,					STYLE_CONTROL_OBJ,		STYLE,					LTYPE_CONTROL_OBJ,		LTYPE,								VIEW_CONTROL_OBJ,
	VIEW,					UCS_CONTROL_OBJ,		UCS,					VPORT_CONTROL_OBJ,		VPORT,					APPID_CONTROL_OBJ,		APPID,								DIMSTYLE_CONTROL_OBJ
	);
	ISO_SETENUMSQ(64, 
	DIMSTYLE,				VP_ENT_HDR_CTRL_OBJ,	VP_ENT_HDR,				GROUP,					MLINESTYLE,				OLE2FRAME,				LONG_TRANSACTION,					LWPOLYLINE,
	HATCH,					XRECORD,				ACDBPLACEHOLDER,		VBA_PROJECT,			LAYOUT,					IMAGE,					IMAGEDEF,							ACAD_PROXY_ENTITY,
	ACAD_PROXY_OBJECT,
	ACAD_TABLE,				CELLSTYLEMAP,			DBCOLOR,				DICTIONARYVAR,			DICTIONARYWDFLT,		FIELD,					IDBUFFER,							IMAGEDEFREACTOR,
	LAYER_INDEX,			LWPLINE,				MATERIAL,				MLEADER,				MLEADERSTYLE,			PLACEHOLDER,			PLOTSETTINGS,						RASTERVARIABLES,
	SCALE,					SORTENTSTABLE,			SPATIAL_FILTER,			SPATIAL_INDEX,			TABLEGEOMETRY,			TABLESTYLES,			VISUALSTYLE,						WIPEOUTVARIABLE,
	ACDBDICTIONARYWDFLT,	TABLESTYLE,				EXACXREFPANELOBJECT,	NPOCOLLECTION,			ACDBSECTIONVIEWSTYLE,	ACDBDETAILVIEWSTYLE,	ACDB_BLKREFOBJECTCONTEXTDATA_CLASS,	ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS
	);
} };

//template<typename T> struct ISO::def<dwg::raw<T>> : ISO::def<T> {};
ISO_DEFUSER(dwg::B, bool);
ISO_DEFUSER(dwg::BS, uint16);
ISO_DEFUSER(dwg::BL, uint32);
ISO_DEFUSER(dwg::BD, double);
ISO_DEFUSER(dwg::TV, string16);
ISO_DEFUSER(dwg::BEXT, double[3]);
ISO_DEFUSER(dwg::BSCALE, double[3]);
ISO_DEFUSERCOMPV(dwg::H, offset2);
ISO_DEFUSERCOMPV(dwg::CMC, index, rgb, name_type, name);

ISO_DEFUSERCOMPV(dwg::ObjectT<dwg::LWPOLYLINE>::Vertex, x, y);
ISO_DEFUSERCOMPV(dwg::ObjectT<dwg::MLINESTYLE>::Item,	offset, color, linetype);

ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::UNUSED>,				"Unused",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::TEXT>,					"Text",					insert_point, text);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ATTRIB>,				"Attrib",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ATTDEF>,				"Attdef",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::BLOCK>,					"Block",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ENDBLK>,				"EndBlk",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SEQEND>,				"SeqEnd",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::INSERT>,				"Insert",				insert_point, ext_point, scale, angle, blockH);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MINSERT>,				"MInsert",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VERTEX_2D>,				"Vertex2d",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VERTEX_3D>,				"Vertex3d",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VERTEX_MESH>,			"VertexMesh",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VERTEX_PFACE>,			"VertexPface",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VERTEX_PFACE_FACE>,		"VertexPfaceFace",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::POLYLINE_2D>,			"Polyline2d",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::POLYLINE_3D>,			"Polyline3d",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ARC>,					"Arc",					centre, radius, angle0, angle1);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::CIRCLE>,				"Circle",				centre, radius);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LINE>,					"Line",					point1, point2);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_ORDINATE>,	"DimensionOrdinate",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_LINEAR>,		"DimensionLinear",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_ALIGNED>,		"DimensionAligned",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_ANG_PT3>,		"DimensionAng_pt3",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_ANG_LN2>,		"DimensionAng_ln2",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_RADIUS>,		"DimensionRadius",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMENSION_DIAMETER>,	"DimensionDiameter",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::POINT>,					"Point",				point);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::FACE_3D>,				"Face3d",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::POLYLINE_PFACE>,		"PolylinePface",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::POLYLINE_MESH>,			"PolylineMesh",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SOLID>,					"Solid",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::TRACE>,					"Trace",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SHAPE>,					"Shape",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VIEWPORT>,				"Viewport",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ELLIPSE>,				"Ellipse",				centre, axis0, ratio, angle0, angle1);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SPLINE>,				"Spline",				controllist);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::REGION>,				"Region",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SOLID_3D>,				"Solid_3d",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::BODY>,					"Body",					type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::RAY>,					"Ray",					point1, point2);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::XLINE>,					"Xline",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DICTIONARY>,			"Dictionary",			handles);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::OLEFRAME>,				"Oleframe",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MTEXT>,					"MText",				insert_point, x_axis, text);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LEADER>,				"Leader",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::TOLERANCE>,				"Tolerance",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MLINE>,					"Mline",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::BLOCK_CONTROL_OBJ>,		"BlockControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::BLOCK_HEADER>,			"BlockHeader",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LAYER_CONTROL_OBJ>,		"LayerControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LAYER>,					"Layer",				handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::STYLE_CONTROL_OBJ>,		"StyleControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::STYLE>,					"Style",				handle, font);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LTYPE_CONTROL_OBJ>,		"LTypeControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LTYPE>,					"LType",				handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VIEW_CONTROL_OBJ>,		"ViewControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VIEW>,					"View",					handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::UCS_CONTROL_OBJ>,		"UcsControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::UCS>,					"Ucs",					handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VPORT_CONTROL_OBJ>,		"VportControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VPORT>,					"Vport",				handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::APPID_CONTROL_OBJ>,		"AppidControlObj",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::APPID>,					"Appid",				handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMSTYLE_CONTROL_OBJ>,	"DimstyleControlObj",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DIMSTYLE>,				"Dimstyle",				handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VP_ENT_HDR_CTRL_OBJ>,	"VpEntHdrControlObj",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VP_ENT_HDR>,			"VpEntHdr",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::GROUP>,					"Group",				handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MLINESTYLE>,			"MLinestyle",			handle, desc, items);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::OLE2FRAME>,				"Ole2Frame",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LONG_TRANSACTION>,		"LongTransaction",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LWPOLYLINE>,			"LWPolyline",			vertlist);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::HATCH>,					"Hatch",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::XRECORD>,				"Xrecord",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACDBPLACEHOLDER>,		"ACDBplaceholder",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VBA_PROJECT>,			"VBAproject",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LAYOUT>,				"Layout",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::IMAGE>,					"Image",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::IMAGEDEF>,				"ImageDef",				version, imageSize, pixelSize, loaded, resolution);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACAD_PROXY_ENTITY>,		"Acad_proxy_entity",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACAD_PROXY_OBJECT>,		"Acad_proxy_object",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACAD_TABLE>,			"Acad_table",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::CELLSTYLEMAP>,			"CellStyleMap",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DBCOLOR>,				"Dbcolor",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DICTIONARYVAR>,			"DictionaryVar",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::DICTIONARYWDFLT>,		"DictionaryWdefault",	handles);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::FIELD>,					"Field",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::IDBUFFER>,				"IDbuffer",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::IMAGEDEFREACTOR>,		"ImageDefReactor",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LAYER_INDEX>,			"LayerIndex",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::LWPLINE>,				"LWPline",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MATERIAL>,				"Material",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MLEADER>,				"MLeader",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::MLEADERSTYLE>,			"MLeaderStyle",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::PLACEHOLDER>,			"Placeholder",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::PLOTSETTINGS>,			"PlotSettings",			handle);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::RASTERVARIABLES>,		"RasterVariables",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SCALE>,					"Scale",				type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SORTENTSTABLE>,			"SortEntStable",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SPATIAL_FILTER>,		"SpatialFilter",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::SPATIAL_INDEX>,			"SpatialIndex",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::TABLEGEOMETRY>,			"TableGeometry",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::TABLESTYLES>,			"TableStyles",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::VISUALSTYLE>,			"VisualStyle",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::WIPEOUTVARIABLE>,		"WipeoutVariable",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACDBDICTIONARYWDFLT>,	"Acdbdictionarywdflt",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::TABLESTYLE>,			"TableStyle",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::EXACXREFPANELOBJECT>,	"ExacxrefPanelObject",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::NPOCOLLECTION>,			"NPOcollection",		type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACDBSECTIONVIEWSTYLE>,	"ACDBSectionViewStyle",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACDBDETAILVIEWSTYLE>,	"ACDBDetailViewStyle",	type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACDB_BLKREFOBJECTCONTEXTDATA_CLASS>,			"ACDBBlkrefObjectContextDataClass",			type);
ISO_DEFUSERCOMPXV(dwg::ObjectT<dwg::ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS>,	"ACDBMtextAttributeobjectContextDataClass",	type);

ISO_DEFUSERCOMPPXV(dwg::ObjectT<dwg::BLOCK_HEADER>,			dwg::DWG*,	"BlockHeader",			type, children);


struct make_browser {
	const dwg::Object *p;
	template<dwg::OBJECTTYPE T> auto operator()() { return ISO::MakeBrowser(*(const dwg::ObjectT<T>*)p); }
	make_browser(const dwg::Object *p) : p(p) {}
};

template<> struct ISO::def<dwg::Entity> : ISO::VirtualT2<dwg::Entity> {
	static ISO::Browser2	Deref(const dwg::Entity &ent) { return switchT<ISO::Browser2>(make_browser(&ent), ent.type, dwg::ALL_OBJECTTYPES()); }
};

template<> struct ISO::def<dwg::Object> : ISO::VirtualT2<dwg::Object> {
	static ISO::Browser2	Deref(const dwg::Object &ent) { return switchT<ISO::Browser2>(make_browser(&ent), ent.type, dwg::ALL_OBJECTTYPES()); }
};

template<typename T> struct ISO::def<dwg::HandleCollection<T>>	: TISO_virtualarray<dwg::HandleCollection<T>>		{};
template<dwg::OBJECTTYPE T, dwg::OBJECTTYPE CT> struct ISO::def<dwg::DWG::Table<T, CT>>	: TISO_virtualarray<dwg::DWG::Table<T, CT>>	{};

ISO_DEFUSERCOMPV(dwg::DWG, blocks, layers, textstyles, linetypes, views, ucs, vports, appids, dimstyles, groups, mlinestyles, layouts, plotsettings);	

class DWGFileHandler : public FileHandler {
	const char*		GetExt()			override { return "dwg"; }
	const char*		GetDescription()	override { return "Autodesk Drawing"; }
	int				Check(const char *fn) override { return CHECK_NO_OPINION; }

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

class GeometryBuilder {
	com_ptr<ID2D1PathGeometry>	path;
	com_ptr<ID2D1GeometrySink>	sink;
	position2	p0;
	bool		open;

public:
	GeometryBuilder() {}
	GeometryBuilder(ID2D1Factory *factory) {
		factory->CreatePathGeometry(&path);
		path->Open(&sink);
		open	= false;
	}
	com_ptr<ID2D1Geometry> Finish() {
		Close();
		sink->Close();
		return path.as<ID2D1Geometry>();
	}

	void	Close() {
		if (open) {
			sink->EndFigure(D2D1_FIGURE_END_OPEN);
			open = false;
		}
	}
	void	Open(position2 p) {
		p0 = p;
		sink->BeginFigure(d2d::point(p), D2D1_FIGURE_BEGIN_HOLLOW);
		open = true;
	}

	void	MoveTo(position2 p) {
		if (!open || any(p != p0)) {
			Close();
			Open(p);
		}
	}
	void	LineTo(position2 p) {
		sink->AddLine(d2d::point(p));
		p0 = p;
	}

	void	AddLine(position2 p1, position2 p2) {
		MoveTo(p1);
		LineTo(p2);
	}

	void	AddArc(position2 p1, position2 p2, float2 radii, float angle, bool ccw, bool big) {
		MoveTo(p1);
		D2D1_ARC_SEGMENT	a;
		a.point				= d2d::point(p2);
		a.size				= d2d::point(radii);
		a.rotationAngle		= to_degrees(angle);
		a.sweepDirection	= ccw ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE;
		a.arcSize			= big ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
		sink->AddArc(&a);
	}

	void AddEntity(dwg::DWG* dwg, dwg::Entity *i);
};

void GeometryBuilder::AddEntity(dwg::DWG* dwg, dwg::Entity *i) {
	switch (i->type) {
		case dwg::POINT: {
			auto		ent		= i->as<dwg::POINT>();
			AddLine({ent->point[0], ent->point[1]}, {ent->point[0], ent->point[1]});
			break;
		}

		case dwg::LINE: {
			auto		ent		= i->as<dwg::LINE>();
			AddLine({ent->point1[0], ent->point1[1]}, {ent->point2[0], ent->point2[1]});
			break;
		}

		case dwg::ARC: {
			auto		ent		= i->as<dwg::ARC>();
			position2	centre	= {ent->centre[0], ent->centre[1]};
			float		ang0	= ent->angle0, ang1 = ent->angle1;
			if (ang0 > ang1)
				ang0 -= 2 * pi;
			float		r	= ent->radius;
			AddArc(centre + sincos(ang0) * r, centre + sincos(ang1) * r, r, 0, false, ang1 - ang0 > pi);
			break;
		}

		case dwg::CIRCLE: {
			auto		ent = i->as<dwg::CIRCLE>();
			position2	centre	= {ent->centre[0], ent->centre[1]};
			float		r	= ent->radius;
			auto		p1	= centre - float2{r, 0};
			auto		p2	= centre + float2{r, 0};
			//gb.AddArc(p1, p1, arc->radius, 0, false, true);
			AddArc(p1, p2, r, 0, false, true);
			AddArc(p2, p1, r, 0, false, true);
			break;
		}

		case dwg::ELLIPSE: {
			auto		ent = i->as<dwg::ELLIPSE>();
			ellipse		e(position2(ent->centre[0], ent->centre[1]), float2{ent->axis0[0], ent->axis0[1]}, ent->ratio);
			float		ang0	= ent->angle0, ang1 = ent->angle1;
			if (ang0 > ang1)
				ang0 -= 2 * pi;

			AddArc(e.matrix() * (position2)sincos(ang0), e.matrix() * (position2)sincos(ang1), e.radii(), atan2(e.minor()), false, ang1 - ang0 > pi);
			break;
		}

		case dwg::LWPOLYLINE: {
			auto		ent = i->as<dwg::LWPOLYLINE>();
			position2	p0(ent->vertlist.front().x, ent->vertlist.front().y);
			MoveTo(p0);
			for (auto &v : ent->vertlist.slice(1))
				LineTo({v.x, v.y});
		
			if (!(ent->flags & ent->open))
				LineTo(p0);
			break;
		}

		case dwg::HATCH: {
			auto		ent = i->as<dwg::HATCH>();
			break;
		}

		case dwg::SPLINE: {
			auto		ent = i->as<dwg::SPLINE>();
			position2	p0(ent->controllist.front()[0], ent->controllist.front()[1]);
			
			MoveTo(p0);
			for (auto &v : ent->controllist.slice(1))
				LineTo({v[0], v[1]});

			if (ent->flags & ent->closed)
				LineTo(p0);
			break;
		}

		case dwg::MTEXT: {
			auto		ent = i->as<dwg::MTEXT>();
			position2	p0(ent->insert_point[0], ent->insert_point[1]);
			d2d::Write	write;
			d2d::Font	font(write, "Arial", ent->text_height);
			d2d::TextLayout	text(write, ent->text, font, {ent->rect_width, ent->rect_height});
			break;
		}

		case dwg::ATTDEF: {
			auto		ent = i->as<dwg::ATTDEF>();
			break;
		}

		case dwg::LEADER: {
			auto		ent = i->as<dwg::LEADER>();
			break;
		}

		case dwg::TEXT: {
			auto		ent = i->as<dwg::TEXT>();
			d2d::Write	write;
			d2d::Font	font(write, "Arial", ent->height);
			d2d::TextLayout	text(write, ent->text, font, 1000);
			break;
		}


		default:
			ISO_TRACEF("Can't draw ") << NameFromType(i->type) << '\n';
			break;
	}
}

// IDWriteTextRenderer
class TextBuilder : public com<IDWriteTextRenderer> {
	ID2D1Factory *factory;
	float2		scale;
public:
	dynamic_array<com_ptr2<ID2D1Geometry>>	geoms;

	TextBuilder(ID2D1Factory *factory, float2 scale) : factory(factory), scale(scale) {}

	STDMETHOD(IsPixelSnappingDisabled)(void *context, BOOL *isDisabled) {
		*isDisabled = true;
		return S_OK;
	}
	STDMETHOD(GetCurrentTransform)(void *context, DWRITE_MATRIX *transform) {
		*(D2D1_MATRIX_3X2_F*)transform = d2d::matrix(identity);
		return S_OK;
	}
	STDMETHOD(GetPixelsPerDip)(void *context, FLOAT *pixelsPerDip) {
		*pixelsPerDip = 1;
		return S_OK;
	}
	STDMETHOD(DrawGlyphRun)(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, const DWRITE_GLYPH_RUN *glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION *glyphRunDescription, IUnknown *effect) {
		com_ptr<ID2D1PathGeometry>	path;
		com_ptr<ID2D1GeometrySink>	sink;
		factory->CreatePathGeometry(&path);
		path->Open(&sink);

		HRESULT hr = glyphRun->fontFace->GetGlyphRunOutline(
			glyphRun->fontEmSize,
			glyphRun->glyphIndices,
			glyphRun->glyphAdvances,
			glyphRun->glyphOffsets,
			glyphRun->glyphCount,
			glyphRun->isSideways,
			glyphRun->bidiLevel%2,
			sink
		);
		sink->Close();

		geoms.push_back(d2d::Geometry(factory, path, iso::scale(scale) * translate(baselineOriginX, baselineOriginY)));
		return S_OK;
	}
	STDMETHOD(DrawUnderline)(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE const *underline, IUnknown *effect) {
		return E_NOTIMPL;
	}
	STDMETHOD(DrawStrikethrough)(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *effect) {
		return E_NOTIMPL;
	}
	STDMETHOD(DrawInlineObject)(void *context, FLOAT originX, FLOAT originY, IDWriteInlineObject *inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown *effect) {
		return E_NOTIMPL;
	}
};

struct LayerBuilder {
	ID2D1Factory	*factory;
	dwg::DWG		*dwg;
	d2d::Write		write;

	LayerBuilder(ID2D1Factory *factory, dwg::DWG* dwg) : factory(factory), dwg(dwg) {
	}


	dynamic_array<com_ptr2<ID2D1Geometry>>	AddText(dwg::ObjectT<dwg::TEXT> *ent) const;
	dynamic_array<com_ptr2<ID2D1Geometry>>	AddText(dwg::ObjectT<dwg::MTEXT> *ent) const;
	com_ptr<ID2D1Geometry>					AddEntities(const dwg::HandleRange::Container &handles, hash_set<uint32> &enabled)	const;
	sparse_array<com_ptr2<ID2D1Geometry>>	AddEntitiesToLayers(const dwg::HandleRange::Container &handles)						const;
};

dynamic_array<com_ptr2<ID2D1Geometry>> LayerBuilder::AddText(dwg::ObjectT<dwg::TEXT> *ent) const {
	auto		style		= dwg->get_object(ent->styleH)->as<dwg::STYLE>();
	bool		vertical	= style->flags & style->vertical;
	float		height		= ent->height						? ent->height					: (double)style->height;
	float		width_scale	= ent->flags & ent->no_width_scale	? (double)style->width_scale	: ent->width_scale;
	float		oblique		= ent->flags & ent->no_oblique		? (double)style->oblique		: ent->oblique;

	d2d::Font	font(write, "Arial", height);


	DWRITE_TEXT_ALIGNMENT		alignH;
	DWRITE_PARAGRAPH_ALIGNMENT	alignV;
	switch (ent->alignH) {
		case dwg::HLeft:	alignH = DWRITE_TEXT_ALIGNMENT_LEADING; break;
		case dwg::HCenter:	alignH = DWRITE_TEXT_ALIGNMENT_CENTER; break;
		case dwg::HRight:	alignH = DWRITE_TEXT_ALIGNMENT_TRAILING; break;
		case dwg::HAligned:	alignH = DWRITE_TEXT_ALIGNMENT_LEADING; break;
		case dwg::HMiddle:	alignH = DWRITE_TEXT_ALIGNMENT_CENTER; break;
		case dwg::HFit:		alignH = DWRITE_TEXT_ALIGNMENT_JUSTIFIED; break;
	};

	switch (ent->alignV) {
		case dwg::VBaseLine:alignV	= DWRITE_PARAGRAPH_ALIGNMENT_NEAR; break;
		case dwg::VBottom:	alignV	= DWRITE_PARAGRAPH_ALIGNMENT_FAR; break;
		case dwg::VMiddle:	alignV	= DWRITE_PARAGRAPH_ALIGNMENT_CENTER; break;
		case dwg::VTop:		alignV	= DWRITE_PARAGRAPH_ALIGNMENT_NEAR; break;
	}

	font->SetTextAlignment(alignH);
	font->SetParagraphAlignment(alignV);

	d2d::point		size(1000, 0);
	if (!(ent->flags & ent->no_align_point))
		size = {abs(ent->align_point[0] - ent->insert_point[0]) / width_scale, (ent->align_point[1] - ent->insert_point[1])};

	d2d::TextLayout	text(write, ent->text, font, size);

	TextBuilder		tb(factory, float2{width_scale, 1});
	text->Draw(0, &tb, ent->insert_point[0] / width_scale, ent->insert_point[1]);
	return move(tb.geoms);
}

dynamic_array<com_ptr2<ID2D1Geometry>> LayerBuilder::AddText(dwg::ObjectT<dwg::MTEXT> *ent) const {
	auto		style		= dwg->get_object(ent->styleH)->as<dwg::STYLE>();
	bool		vertical	= style->flags & style->vertical;
	float		height		= ent->text_height	? (double)ent->text_height : (double)style->height;
	float		width_scale	= style->width_scale;
	float		oblique		= style->oblique;

	d2d::Font		font(write, "Arial", height);

	d2d::point		size(ent->rect_width / width_scale, ent->rect_height);

	d2d::TextLayout	text(write, ent->text, font, size);

	TextBuilder		tb(factory, float2{width_scale, 1});
	text->Draw(0, &tb, ent->insert_point[0] / width_scale, ent->insert_point[1]);
	return move(tb.geoms);
}

com_ptr<ID2D1Geometry> LayerBuilder::AddEntities(const dwg::HandleRange::Container &handles, hash_set<uint32> &enabled) const {
	GeometryBuilder	gb(factory);

	dynamic_array<com_ptr2<ID2D1Geometry>>	geoms;

	for (auto i : handles) {
		if (i->type == dwg::INSERT) {
			auto	ent		= i->as<dwg::INSERT>();
			auto	block	= dwg->get_object(ent->blockH)->as<dwg::BLOCK_HEADER>();
			auto	geom	= AddEntities(block->entities.contents(dwg, block->handle), enabled);

			position2	pos0(block->base_point[0], block->base_point[1]);
			d2d::matrix	trans = ent->transformation() * translate(-pos0);
			geoms.push_back(d2d::Geometry(factory, geom, trans));

		} else if (!enabled.size() || enabled.count(i->layerH)) {
			if (i->type == dwg::TEXT) {
				geoms.append(AddText(i->as<dwg::TEXT>()));

			} else if (i->type == dwg::MTEXT) {
				geoms.append(AddText(i->as<dwg::MTEXT>()));

			} else {
				gb.AddEntity(dwg, i);
			}
		}
	}

	if (geoms) {
		geoms.push_back(gb.Finish());
		return d2d::Geometry(factory,  (ID2D1Geometry**)geoms.begin(), geoms.size32());
	}

	return gb.Finish();
}

sparse_array<com_ptr2<ID2D1Geometry>> LayerBuilder::AddEntitiesToLayers(const dwg::HandleRange::Container &handles) const {
	sparse_array<dynamic_array<com_ptr2<ID2D1Geometry>>>	geoms;
	sparse_array<GeometryBuilder>		gbs;

	for (auto i : handles) {
		if (i->type == dwg::INSERT) {
			auto	ent		= i->as<dwg::INSERT>();
			auto	block	= dwg->get_object(ent->blockH)->as<dwg::BLOCK_HEADER>();
			auto	geoms2	= AddEntitiesToLayers(block->entities.contents(dwg, block->handle));

			position2	pos0(block->base_point[0], block->base_point[1]);
			d2d::matrix	trans = ent->transformation() * translate(-pos0);

			for (auto &i : geoms2)
				geoms[i.index()]->push_back(d2d::Geometry(factory, *i, trans));

		} else if (i->type == dwg::TEXT) {
			geoms[i->layerH]->append(AddText(i->as<dwg::TEXT>()));

		} else if (i->type == dwg::MTEXT) {
			geoms[i->layerH]->append(AddText(i->as<dwg::MTEXT>()));

		} else {
			auto	gb = gbs[i->layerH];
			if (!gb.exists())
				gb = factory;
			gb->AddEntity(dwg, i);

		}
	}

	sparse_array<com_ptr2<ID2D1Geometry>>	result;
	for (auto &gb : gbs) {
		auto	i	= gb.index();
		auto	geom = geoms[i];
		if (geom.exists()) {
			geom->push_back(gb->Finish());
			com_ptr<ID2D1GeometryGroup>	group;
			factory->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, (ID2D1Geometry**)geom->begin(), geom->size32(), &group);
			result[i] = group.as<ID2D1Geometry>();
		} else {
			result[i] = gb->Finish();
		}
	}

	for (auto &geom : geoms) {
		auto	i	= geom.index();
		if (!gbs[i].exists())
			result[i] = d2d::Geometry(factory,  (ID2D1Geometry**)geom->begin(), geom->size32());
	}
	return result;
}


class ViewDWG : public win::Inherit<ViewDWG, Viewer2D> {
	ISO_ptr_machine<void>	p;
	uint32	num		= 0xffffffff;
	uint32	layer	= 0;
	com_ptr<ID2D1Geometry> geom;
	sparse_array<com_ptr2<ID2D1Geometry>> layer_geoms;

//	void	MakeGeom();

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	Paint(d2d::PAINT_INFO *info);
	ViewDWG(const win::WindowPos &wpos, const ISO_ptr_machine<void> &p);
};

ViewDWG::ViewDWG(const win::WindowPos &wpos, const ISO_ptr_machine<void> &_p) : p(_p) {

	if (p.IsType<dwg::DWG>()) {
		ISO::Browser2	b(p);
		b = b/"blocks"/"*Model_Space";
		p = b.GetPtr();
	}

	if (p.IsType<param_element<dwg::ObjectT<dwg::BLOCK_HEADER>&, dwg::DWG*>>()) {
		param_element<dwg::ObjectT<dwg::BLOCK_HEADER>&, dwg::DWG*>	*block0 = p;
		auto&	block	= block0->t;
		auto	dwg		= block0->p;

		float2	centre(zero);
		uint32	ninserts	= 0;
		for (auto i : block.entities.contents(dwg, block.handle)) {
			if (i->type == dwg::INSERT) {
				auto	ent = i->as<dwg::INSERT>();
				centre += float2{ent->insert_point[0], ent->insert_point[1]};
				++ninserts;
			}
		}
		if (ninserts)
			pos = -centre / float(ninserts);

		LayerBuilder	lb(factory, dwg);
		layer_geoms = lb.AddEntitiesToLayers(block.entities.contents(dwg, block.handle));
	}

	Create(wpos, (tag)p.ID(), CHILD | VISIBLE | CLIPCHILDREN | CLIPSIBLINGS, NOEX);
}

//void ViewDWG::MakeGeom() {
//	param_element<dwg::ObjectT<dwg::BLOCK_HEADER>&, dwg::DWG*>	*block0 = p;
//	auto&	block	= block0->t;
//	auto	dwg		= block0->p;
//
//	hash_set<uint32>	enabled;
//	if (layer)
//		enabled.insert(layer);
//
//	geom = AddEntities(factory, dwg, block.entities.contents(dwg, block.handle), enabled);
//}

LRESULT ViewDWG::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			break;


		case WM_MOUSEACTIVATE:
			SetFocus();
			SetAccelerator(*this, Accelerator());
			return MA_NOACTIVATE;

		case WM_KEYDOWN:
			switch (wParam) {
				case VK_ADD:
					++num;
					Invalidate();
					break;
				case VK_SUBTRACT:
					if (num)
						--num;
					Invalidate();
					break;
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case d2d::PAINT_INFO::CODE: {
					Paint((d2d::PAINT_INFO*)nmh);
					break;
				}
			}
			break;
		}


		case WM_COMMAND:
			switch (uint16 id = LOWORD(wParam)) {
				case ID_EDIT_SELECT: {
					ISO::Browser b	= *(ISO::Browser*)lParam;
					if (b.Is<param_element<dwg::ObjectT<dwg::LAYER>&, dwg::DWG*>>()) {
						param_element<dwg::ObjectT<dwg::BLOCK_HEADER>&, dwg::DWG*>	*layer0 = b;
						layer	= layer0->t.handle;
					} else {
						layer	= 0;
					}
					//MakeGeom();
					Invalidate();
					break;
				}
				default:
					break;
			}
			break;


		case WM_NCDESTROY:
			delete this;
			break;
	}
	return Super(message, wParam, lParam);
}

void ViewDWG::Paint(d2d::PAINT_INFO *info) {
	auto	screen	= transformation();
	SetTransform(screen);

	float	line_width = max(0.1f, 0.5f / zoom);//.01f;

	if (layer) {
		auto geom = layer_geoms[layer];
		if (geom.exists()) {
			d2d::SolidBrush	black(*this, colour(0,0,0));
			Draw(*geom, black, line_width);
		}
		return;
	}
#if 1
	auto	h = halton2(2, 3);
	int		i = 0;
	for (auto &geom : layer_geoms) {
		auto	c = uniform_surface(unit_sphere, h(i, 0));
		d2d::SolidBrush	brush(*this, colour(c.v));//colour(c.v.x, c.y, c.z));
		Draw(*geom, brush, line_width);
		++i;
	}
#else
	if (geom) {
		d2d::SolidBrush	black(*this, colour(0,0,0));
		Draw(geom, black, line_width);
	}
#endif
}


class EditorDWG : public app::Editor {
	bool Matches(const ISO::Browser &b) override {
		return b.Is<dwg::DWG>()
			|| b.Is<param_element<dwg::ObjectT<dwg::BLOCK_HEADER>&, dwg::DWG*>>();
	}
	win::Control Create(app::MainWindow &main, const win::WindowPos &wpos, const ISO_ptr_machine<void> &p) override {
		return *new ViewDWG(wpos, p);
	}
} editor_dwg;

#endif
