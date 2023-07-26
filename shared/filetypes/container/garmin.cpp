#include "archive_help.h"
#include "iso/iso_files.h"

using namespace iso;

#pragma pack(1)

typedef uintn<3,false>	uint24;
typedef uintn<3,false>	union_uint24;

struct TIMEDATE {
	uint16	year;
	uint8	month;
	uint8	day;
	uint8	hour;
	uint8	minute;
	uint8	second;
};

struct HEADER {	// 0x200
	uint8		eor;
	uint8		_0[9];
	uint8		expire_month;	//@ 0x0A:
	uint8		expire_year;	//@ 0x0B:
	uint32		checksum;		//@ 0x0F:
	char		string[8];		//@ 0x10:
	uint16		sectors;		//@ 0x18:
	uint16		heads;			//@ 0x1A:
	uint16		cylinders;		//@ 0x1C:
	uint16		_3;				//@ 0x1E:
	uint8		_4[0x19];
	TIMEDATE	created;
	uint8		fat0_block;		//@ 0x40:
	uint8		string2[8];		//@ 0x41:
	char		map_name[0x14];
	uint16		heads2;			//@ 0x5D:
	uint16		sectors2;		//@ 0x5F:
	uint8		E1;				//@ 0x61:
	uint8		E2;				//@ 0x62:
	uint16		nClusters;		//@ 0x63:
	char		map_comment[0x20];

	uint8	_padding[0x1be - 0x85];

	struct PARTITION {//16
		uint8	boot_flag;
		uint8	start_head;
		uint8	start_sector;
		uint8	start_cylinder;
		uint8	filesystem;
		uint8	end_head;
		uint8	end_sector;
		uint8	end_cylinder;
		uint32	rel_sectors;
		uint32	nsectors;
	} partitions[4];

	uint16		part_end;
};

struct ENTRY {
	uint8	flags;
	char	filename[8], ext[3];
	uint32	file_size;
	uint8	_0;
	uint8	sequence;
	uint8	_padding[14];
	uint16	clusters[240];
};

struct SUBFILE_HEADER {
	uint16		header_length;
	char		signature[10];	// 'GARMIN RGN', 'GARMIN TRE', 'GARMIN LBL', 'GARMIN NET' rtc
	uint8		_1;				// 0x01
	uint8		version;		// 0x0 for most maps, 0x80 for locked TRE.
	TIMEDATE	created;
};

struct GMP_HEADER : public SUBFILE_HEADER {
	uint32	_015;

	uint32	TreOffset;
	uint32	RgnOffset;
	uint32	LblOffset;
	uint32	NetOffset;
	uint32	NodOffset;

	uint32	_02d;
	uint32	_031;
};

struct TRE_HEADER : public SUBFILE_HEADER {
	// The map boundaries.
	uint24	MapNorth;
	uint24	MapEast;
	uint24	MapSouth;
	uint24	MapWest;

	// Zoom levels definitions (TRE_Level).
	uint32	LevelsOffset;
	uint32	LevelsLength;

	// Object groups (TRE_Group).
	uint32	GroupsOffset;
	uint32	GroupsLength;

	// Copyright strings (stored in LBL).
	uint32	CopyrightsOffset;
	uint32	CopyrightsLength;
	uint16	CopyrightsRecSize;

	uint16	Zero003b;
	uint16	Zero003d;

	// Display flags.
	uint8	Detailed			: 1, // 0001=R&R 0001=World 1101=MG-Austr. 0000=overview map
			Transparent			: 1,
			StreetBeforeNumber	: 1,
			ZipBeforeCity		: 1,
			Unknown0			: 4;
	uint8	DrawPriority		: 5, // 19=R&R 05=World 1E=MG-Austr. 00=overview map
			Transparent2		: 1,
			Unknown1			: 2;
	uint16	Zero0041;
	uint8	Unknown10;	// 43: 01=R&R 01=World 01=MG-Austr. 01=overview map
	uint8	Unknown11;	// 44: 04=R&R 01=World 03=MG-Austr. 01=overview map
	uint8	Unknown12;	// 45: 0d=R&R 05=World 0E=MG-Austr. 04=overview map
	uint8	Zero046;
	uint16	_047;		// 47: 0x0001
	uint8	Zero049;	// 49: 0

	// Types of polylines.
	uint32	PolylineInfoOffset;
	uint32	PolylineInfoLength;
	uint16	PolylineInfoRecSize;

	uint16	_054;
	uint16	Zero056;

	// Types of polygons.
	uint32	PolygonInfoOffset;
	uint32	PolygonInfoLength;
	uint16	PolygonInfoRecSize;

	uint16	_062;
	uint16	Zero064;

	// Types of POI and points.
	uint32	PointInfoOffset;
	uint32	PointInfoLength;
	uint16	PointInfoRecSize;

	uint16	_070;
	uint16	Zero072;
// 74:

	// ID of the map.
	// NOTE: TRE header for WorldMap-99 does not contain this field!
	uint32	MapID;
// 78:
};

struct TRE_HEADER2 : public TRE_HEADER {
	uint32	Zero078;

	// Object groups V2 (TRE_Group2).
	uint32	Groups2Offset;
	uint32	Groups2Length;
	uint16	Group2RecSize; // 13

	uint32	_086;	// 0x487

	uint32	Tre8Offset;	// Order: polyline, polygon, POI; each sorted by type (1 type 1 levels 1 subtype)
	uint32	Tre8Length;
	uint16	Tre8RecSize;	// 3

	uint16	Polylines2TypesNum;
	uint16	Polygons2TypesNum;
	uint16	Points2TypesNum;
// 9A:
};

// TRE1: zoom level info.
// NOTE: come in sorted order, less detailed level is first.
struct TRE_ZoomLevel {
	uint8	Zoom	: 4, // Zoom level for MapSource.
			unknown	: 3, // Always 0?
			ExtPrev : 1; // 1 means no own data but the data from previous level are applied
	uint8	Bits;	// Number of bits per coordinate.
	uint16	Groups;	// Number of TRE groups for this zoom level.
};

// TRE2: object group info.
// NOTE: 16 bytes for all zoom levels except for the most detailed (stored last) which has 14-byte size.
struct TRE_Group {
	uint32	RgnOffset	: 28,
			Kinds		: 4; // 0x1 - has points, 0x2 - has indexed points, 0x4 - has polylines, 0x8 - has polygons.

	// Reference point for all objects in the group.
	uint24	X0, Y0;	// NOTE: all coordinates are defined as difference from this point.

// 0a:
	// The group area is rectangle with the ref point in the center, the size is
	// (wWidth*2 + 1) x (wHeight*2 + 1) in variable grid units for this zoom level.
	uint16	Width	: 15,
			EndMark : 1;	// 1 for last group in chain referred by 'parent' group
	uint16	Height	: 16;

// 0e:
	// 1-based index of the first group in chain for the next zoom level.
	// NOTE: omited for last (i.e most detailed 0-th) zoom level.
	uint16	NextLevelIndex;
};

// TRE3: copyright strings in LBL sub-file.
struct TRE_Copyright {
	uint24	Offset;
};

// TRE4: types of polylines.
struct TRE_PolylineInfo {
	uint8	Type;
	uint8	Level;
};

// TRE5: types of polygons.
struct TRE_PolygonInfo {
	uint8	Type;
	uint8	Level;
};

// TRE6: types of points.
struct TRE_PointInfo {
	uint8	Type;
	uint8	Level;
	uint8	SubType;
};

struct TRE_Group2_size13 {
	uint32	PolygonsOffset;
	uint32	PolylinesOffset;
	uint32	PointsOffset;
	uint8	Objects;
};

struct TRE_Group2_size17 {
	uint32	Size;		// ??
	uint8	Unknown;	// ??
	uint32	PolygonsOffset;
	uint32	PolylinesOffset;
	uint32	PointsOffset;
};

// Header of RGN sub-file.
struct RGN_HEADER : public SUBFILE_HEADER {
	uint32	DataOffset; // NOTE: just equal to	HeaderLength
	uint32	DataLength;
// 1d:
};

struct RGN_HEADER2 : public RGN_HEADER {
	uint32	Polygons2Offset;
	uint32	Polygons2Length;

	uint32	Zero025;
	uint32	Zero029;
	uint32	Zero02D;
	uint32	Zero031;
	uint32	Zero035;

	uint32	Polylines2Offset;
	uint32	Polylines2Length;

	uint32	Zero041;
	uint32	Zero045;
	uint32	Zero049; // ?? 0x00000001
	uint32	Zero04D; // ?? 0x00000001
	uint32	Zero051;

	uint32	Points2Offset;
	uint32	Points2Length;

	uint32	Zero05D;
	uint32	Zero061;
	uint32	Zero065; // ?? 0x20000001 or 0x00000001
	uint32	Zero069; // ?? 0x00000003
	uint32	Zero06D;
	uint32	Zero071;
	uint32	Zero075;
	uint32	Zero079; // ?? 0x00000001
};

struct RGN_POIData {
	uint32	Type		: 8,
			LabelOffset : 22,	// Extra=0 => label in LBL1, Extra=1 => offset in LBL6
			Extra		: 1,
			HasSubType	: 1;	// NOTE: sometimes 0.

	uint16	X, Y;				// NOTE: relative to TRE group.
	uint8	SubType;			// NOTE: available if bHasSubType=1.
};

struct RGN_PointData {
	uint32	Type		: 8,
			LabelOffset : 22,
			Extra		: 1,	// Always 0 ?
			HasSubType	: 1;	// Always 0 ?
	uint16	X, Y;				// NOTE: relative to TRE group.
};

struct RGN_PolylineData {
	uint32	Type			: 6,
			DirIndicator	: 1,
			TwoBytesInLen	: 1,
			LabelOffset		: 22,
			ExtraBit		: 1,	// 1=one more bit per point in bitstream
			InNet			: 1;	// 1=b3LabelOffset is offset in NET sub-file
	uint16	X0, Y0;					// NOTE: relative to TRE group.
	union {
		uint8	BitstreamLen8;	// if bTwoBytesInLen=0
		uint16	BitstreamLen16;	// if bTwoBytesInLen=1
	};
};

struct RGN_PolygonData {
	uint32	Type			: 7,
			TwoBytesInLen	: 1,
			LabelOffset		: 22,
			Unknown			: 1, // ??
			InNet			: 1; // ??
	uint16	X0, Y0;				// NOTE: relative to TRE group.
	union {
		uint8	BitstreamLen8;	// if bTwoBytesInLen=0
		uint16	BitstreamLen16;	// if bTwoBytesInLen=1
	};
};

struct RGN_POI2Data {
	uint8	Type;
	uint8	SubType			: 5,
			HasLabel		: 1,
			Unknown0		: 1,
			HasExtraByte	: 1;	// 1 if ExtraByte presented
	uint16	X, Y;					// NOTE: relative to TRE group.
	uint32	LabelOffset		: 22,	// bExtra=0 - label in LBL1, bExtra=1 - offset in LBL6
			Extra			: 1,
			Unknown1		: 1,
			ExtraByte		: 8;	// Missed if HasExtraByte==0.
};

struct RGN_Poly2Data {
	uint8	Type;
	uint8	SubType			: 5,
			HasLabel		: 1,
			Unknown0		: 1,
			Unknown1		: 1;	// This bit is sometimes presented in BlueChart, it shows some additional data.
	uint16	X0, Y0;					// NOTE: relative to TRE group.
};

//////////////////////////////////////////////////////////

union LBL_POIExtraDef {
	uint8	bt;
	uint8	HasNumber		: 1,
			HasStreet		: 1,
			HasCity			: 1,
			HasZip			: 1,
			HasPhone		: 1,
			HasExit			: 1,
			HasTidePrediction : 1,
			NumberIsInPlace	: 1; // 0x1 - number is stored in-place as 11-base, 0x0 - number is 3-byte ref to LBL
};

// Header of LBL sub-file.
struct LBL_HEADER : public SUBFILE_HEADER {
	// Reference to the labels bitstream.
	uint32	DataOffset;
	uint32	DataLength;
	uint8	LblOffsetMultiplierPowerOf2;
	uint8	LblCoding;	// 6 by 6-bit coding, 9 for 8-bit SBCS, 10 for MBCS.

	// NOTE: empty entries have	LengthX=0.

	// Countries list.
	uint32	CountriesOffset;
	uint32	CountriesLength;
	uint16	CountriesRecSize; // 3

	uint32	_1;

	// Regions/Provinces/States list.
	uint32	RegionsOffset;
	uint32	RegionsLength;
	uint16	RegionsRecSize;	// 5

	uint32	_2;

	// Cities list.
	uint32	CitiesOffset;
	uint32	CitiesLength;
	uint16	CitiesRecSize;	// 5

	uint16	_3;
	uint16	_4;	// 8000 R&R+MG Europe, 0000 Overview map

	// Index of POIs sorted by types and name.
	uint32	POIIndexOffset;
	uint32	POIIndexLength;
	uint16	POIIndexRecSize;	// 4

	uint32	_5;

	// POI extra properties.
	uint32	POIExtraOffset;
	uint32	POIExtraLength;
	uint8	POIExtraOffsetMultiplierPowerOf2;
	LBL_POIExtraDef POIExtraDef;

	uint8	_6[3];

	// Index of POI types.
	uint32	POITypeIndexOffset;
	uint32	POITypeIndexLength;
	uint16	POITypeIndexRecSize; // 4

	uint32	_7;

	// Zip codes.
	uint32	ZipsOffset;
	uint32	ZipsLength;
	uint16	ZipsRecSize;	// 3

	uint32	_8;

	//
	// NOTE: the following fields (LBL9 - LBL12) are not empty if highway exits with facilities are presented.
	//

	// Highways.
	uint32	HighwaysOffset;
	uint32	HighwaysLength;
	uint16	HighwaysRecSize;	// 6

	uint32	_9;

	// Exits.
	uint32	ExitsOffset;
	uint32	ExitsLength;
	uint16	ExitsRecSize;	// 5

	uint32	_10;	// 0, 1, 5 ??

	// Highway extra properties.
	uint32	HighwayExtraOffset;
	uint32	HighwayExtraLength;
	uint16	HighwayExtraRecSize; // 3

	uint32	_11;

	//
	// NOTE: the fields below are optional (e.g. not defined in WorldMap-99).
	// Header length is to be checked before access.
	//

// 0xAA:
	uint16	CodePage;
	uint16	_12[2];

// 0xB0:
	// Reference to some info string (plain 8-bit coding). E.g. "American", "Western European Sort".
	uint32	SortingOffset;
	uint32	SortingLength;

// 0xB8:
	uint32	Offset14;
	uint32	Length14;
	uint16	RecSize14;

	uint16	_14;

// 0xC4:
	uint32	TidePredictionsOffset;
	uint32	TidePredictionsLength;
	uint16	TidePredictionsRecSize;

	uint16	_15;
};

// LBL2.
struct LBL_Country {
	uint24	NameOffset;	// In LBL
};

// LBL3.
struct LBL_Region {
	uint16	CountryIndex; // 1-based
	uint24	NameOffset;	// In LBL
};

// LBL4.
struct LBL_City {
	union {
		// bPoint=1
		struct {
			uint8	PointIndex;	// 1-based
			uint16	GroupIndex;	// 1-based
		};
		// bPoint=0
		union_uint24	LabelOffset;
	};
	uint16	RegionIndex : 14,	// 1-based, 0 is special value
			NoRegion	: 1,	// 0 -	RegionIndex is region index, 1 -	RegionIndex is country index
			point		: 1;
};

// LBL5.
struct LBL_POIIndexItem {
	uint8	POIIndex;	// 1-based index of POI inside group.
	uint16	GroupIndex;	// 1-based index of group containing POI.
	uint8	POISubType;	// Subtype of referenced-POI.
};

// LBL6 for POI postal address and phone.
// NOTE: the size is variable.
struct LBL_POIExtra {
	uint32	LabelOffset	: 22,
			Unknown		: 1, // always 0 ?
			HasFlags	: 1, // bHasFlags=0 means that bits from	Flags should be used.
			// The following 8 bits are available if bHasFlags=1.
			Flags		: 8;
};

// LBL6 for highway exits.
// NOTE: the size is variable.
struct LBL_ExitExtra {
	uint24	LabelOffset;

//	uint8	Flags;	// Defined only if 23-th bit of	LabelOffset is 1.

//	uint24	Street;	// NOTE: just 0-21 bits
		// 22-nd means 'overnight parking'
							// 23-rd means that facilities are defined

//	uint16	HighwayIndex;	// ? 1 or 2 byte, depending on the amount of highways (LBL9)
//	uint16	FirstFacility;	// ? 1 or 2 byte, depending on the amount of facilities (LBL10)
};

// LBL7.
struct LBL_POITypeIndexItem {
	uint8	Type;	// POI type.
	uint24	StartIndex;	// 1-based index of starting position in array of LBL_POIIndexItem.
};

struct LBL_Zip {
	uint24	CodeOffset;	// In LBL.
};

// LBL9.
struct LBL_Highway {
	uint24	NameOffset;	// In LBL
	uint16	ExtraIndex;	// 1-based index in LBL11.
	uint8	Zero;	// ??
};

// LBL10.
struct LBL_ExitFacility {
	uint32	LabelOffset : 22, // In LBL
			Unknown		: 1,
			Last		: 1,	// =1 for last facility for the given exit

	// 0x00 - Truck Stop/24-hour Diesel Fuel With Restaurant
	// 0x01 - Truck Fuel/Diesel Fuel With Large Vehicle Clearance
	// 0x02 - Gas/Automobile Fuel
	// 0x03 - Food/Restaurant
	// 0x04 - Lodging/Hotel/Motel - Call 1-866-394-8768 For Reservations
	// 0x05 - Auto service/Vehicle Repair and Service
	// 0x06 - Auto service/Diesel Engine Service
	// 0x07 - Auto service/Commercial Vehicle Wash
	// 0x08 - Camp/Campground and RV Service
	// 0x09 - Hospital/Medical Facilities
	// 0x0a - Store/Automated Teller Machines
	// 0x0b - Park/Forest, Park, Preserve, or Lake
	// 0x0c - point Of Interest/Useful Services, Sites, or Attractions
	// 0x0d - Fast Food
			Type		: 4,
			Unknown2	: 1,

	// 0x0 - north
	// 0x1 - south
	// 0x2 - east
	// 0x3 - west
	// 0x4 - inner side
	// 0x5 - outer side
	// 0x6 - both sides
	// 0x7 - not defined
			Direction	: 3;

	// 0x01 - Truck/RV Parking
	// 0x02 - Convenience Story
	// 0x04 - Diesel Fuel
	// 0x08 - Car Wash
	// 0x10 - Liquid Propane
	// 0x20 - Truck Scales
	// 0x40 - Open 24 Hours
	// 0x80 - not used
	uint8	Facilities;
};

	// LBL11.
struct LBL_HighwayExtra {
	uint8	Zero;	// 0
	uint16	RegionIndex;	// 1-based.
};


// Header of NET sub-file.
struct NET_HEADER : public SUBFILE_HEADER {
	uint32	DataOffset; // NOTE: just equal to	HeaderLength
	uint32	DataLength;
	uint8	DataOffsetMultiplierPowerOf2;

	// NET2 - segmented roads offsets.
	uint32	Offset;
	uint32	Length;
	uint8	MultiplierPowerOf2;

	// NET3 - sorted roads offsets.
	uint32	SortedRoadsOffset;
	uint32	SortedRoadsLength;
	uint16	SortedRoadsRecSize;	// 0x3

	uint32	Unknown;	// 0
	uint8	Unknown0;	// 0x1
	uint8	Unknown1;
// 37:

	// NOTE: the fields below are optional (e.g. not defined in WorldMap-99).
	// Header length is to be checked before access.
	uint32	Unknown2;	// 0
// 3b:
	uint32	Unknown3;	// 0x1
	uint32	Unknown4;	// 0
};

	// NET1.
struct NET_Data {
	uint32	LabelOffset		: 22,
			Segmented		: 1, // Pointer to segmented roads follows
			LastLabel		: 1; // At most 4 labels.
};

struct NET_DataFlags {
	uint8	b0				: 1, // ?
			OneWay			: 1,
			LockOnRoad		: 1, // ?
			b3				: 1, // ?
			HasAddress		: 1,
			AddrStartRight	: 1,
			HasNodInfo		: 1,
			MajorHW			: 1; // ?
};

struct NET_AddressFlags {
	uint8	Unknown			: 2,
			ZipsFmt			: 2,
			CitiesFmt		: 2,
			NumbersFmt		: 2;
};

	// NET2.
struct NET_2 {
	// TODO: research.
};

	// NET3.
struct NET_Sorted {
	uint24	NetDataOffset; // Bits 22-23 define label number (0-3).
};


// Header of NOD sub-file.
struct NOD_HEADER : public SUBFILE_HEADER {
	// The routing graph: nodes and groups of nodes.
	uint32	NodesOffset;
	uint32	NodesLength;
	uint8	NodesFlag0;	// ?? 0x01, 0x03, 0x07, 0x27; lower bit is always set
	uint8	LeftSideTraffic : 1,
			NodesFlags1	: 7;	// ?? 0 or 1
	uint16	NodesExtra;	// ?? 0
	uint8	GroupMultiplierPowerOf2; // 6 in most cases, somewhere 4
	uint8	NodesMultiplierPowerOf2; // 0 or 1
	uint8	LinkRefSize;	// 0x4 or 0x5==sizeof(NOD_LinkRef)
	uint8	Nodes0;	// ?? 0

	// Extra routing data for links.
	uint32	LinksOffset;
	uint32	LinksLength;
	uint32	LinksExtra;	// 0

	// External nodes.
	uint32	ExternalNodesOffset;
	uint32	ExternalNodesLength;
	uint8	ExternalNodesRecSize;	// Always 9

	uint8	_3[5];	// 0
// 3f:
};

// NOD1, per node.
struct NOD_Node {
	// To be aligned and multiplied by (1<<NOD_Header::GroupMultiplierPowerOf2);
	uint8	GroupOffset;
	uint8	Unknown012			: 3, // ??
			IsExternal			: 1,
			HasTurnRestrictions : 1,
			LargePosOffset		: 1,
			HasLinks			: 1,
			Unknown7			: 1;
};

// NOD1, per neighbour node.
struct NOD_LinkHdr {
	uint16	DestClass	: 3,
			CurveLen	: 3,
			Forward		: 1,
			NewHeading	: 1,

			Node		: 6, // If bFarLink==1, this is index in group's table of nodes, else this is high byte of relative offset to neighbour node.
			FarNode		: 1, // Link is to different group.
			Last		: 1;
};

// NOD1, per node group.
struct NOD_Group {
	uint8	RestrictionsBytesInSize : 2,
			Unknown2	: 1,
			Unknown3	: 1, // ?? 0, sometimes =1
			Unknown4567	: 4;

	// Center point for all nodes in the group.
	uint24	Lon0;
	uint24	Lat0;

	uint8	Links;	// Number of NOD_LinkRef records.
	uint8	Nodes;	// Number of 3-byte offsets to nodes in other groups.
};

	// NOD1, record in the link table of group.
struct NOD_LinkRef {
	uint32	NetLabelOffset : 22,
			NoDelivery		: 1,
			NoEmergency		: 1,
			SpeedLimit		: 3,
			OneWay			: 1,
			RouteClass		: 3,
			Toll			: 1;

	// NOTE: optional byte, presented only if NOD_Header::btLinkRefSize==5.
	uint8	NoCar			: 1,
			NoBus			: 1,
			NoTaxi			: 1,
			Unknown3		: 1,	// ??
			NoPedestrian	: 1,
			NoBicycle		: 1,
			NoTruck			: 1,
			Unknown7		: 1;	// ??
};

	// NOD1, header of record in the turn restrictions table of group.
struct NOD_TurnRestriction {
	uint8	Hdr;	// ?? 0x05 or 0x15 or 0x25
	uint8	TimeData	: 1, // Time-depending restriction.
			Unknown1	: 1, // ??
			Unknown2	: 1, // ??
			Vehicles	: 1, // Additional vehicle 1-byte bitmask follows, ?? probably defines kinds of vehicles.
			Unknown4	: 1, // ??
			Links		: 3; // [fLinks+1] nodes follows (2 bytes per node), then [fLinks] link indices (1 byte per link)
	uint8	Extra;	// ?? always 00 - may be size of extra data?
};

	// NOD2.
struct NOD_Link {
	uint32	Unknown0	: 1, // ?? 1
			SpeedLimit	: 3,
			RouteClass	: 3,
			Unknown7	: 1, // ?? 0
			NodeOffset	: 24;
	uint16	Nodes;
};

	// NOD3.
struct NOD_External {
	uint24	Lon;
	uint24	Lat;
	uint24	NodeOffset;
};


// Header of MDR sub-file.
struct MDR_HEADER : public SUBFILE_HEADER {
	// TODO: research.
};


// Header of SRT sub-file.
struct SRT_HEADER : public SUBFILE_HEADER {
	// TODO: research.
};

// Header of TYP sub-file.
struct TYP_HEADER : public SUBFILE_HEADER {
	uint16	code_page;		// 1250+ (character set)
	uint32	poi_offset;
	uint32	poi_length;
	uint32	polyline_offset;
	uint32	polyline_length;
	uint32	polygon_offset;
	uint32	polygon_length;
	uint16	FID;
	uint16	ProductCode;

	struct TYP_BLOCK {
		uint32	offset;
		uint16	size;
		uint32	count;
	} poi, polyline, polygon, draworder;
};

#pragma pack()

static int yr(int y) {
	return y >= 99 ? y+1900 : y+2000;
}

void memxor(uint8 *bp, int len, uint8 x) {
	if (x) while (len > 0) {
		*bp++ ^= x;
		len--;
	}
}
template<typename T> void memxor(T &t, uint8 x) { memxor((uint8*)&t, sizeof(T), x); }

class istream_cluster : public istream_chain {
	size_t					bytes_per_cluster;
	streamptr				pos, end;
	dynamic_array<uint16>	clusters;
public:
	istream_cluster(istream_ref stream, uint32 bytes_per_cluster, streamptr end)
		: istream_chain(stream)
		, bytes_per_cluster(bytes_per_cluster)
		, pos(0), end(end)
	{}
	void	add_clusters(uint16 *p, uint32 n) {
		clusters.append(make_range_n(p, n));
	}
	size_t	readbuff(void *buffer, size_t size) {
		if (pos + size > end)
			size = size_t(end - pos);
		size_t	read	= 0;
		for (size_t c = pos / bytes_per_cluster, o = pos % bytes_per_cluster; read < size; c++, o = 0) {
			istream_chain::seek(clusters[c] * bytes_per_cluster + o);
			uint32	s = istream_chain::readbuff((uint8*)buffer + read, min(bytes_per_cluster - o, size - read));
			pos		+= s;
			read	+= s;
		}
		return int(read);
	}
	void		seek(streamptr offset)	{ pos = offset;	}
	streamptr	tell(void)				{ return pos;	}
	streamptr	length(void)			{ return end;	}
};

class istream_xor : public istream_chain {
	uint8	eor;
public:
	istream_xor(istream_ref stream, uint8 eor) : istream_chain(stream), eor(eor)	{}
	size_t		readbuff(void *buffer, size_t size)	{ auto r = istream_chain::readbuff(buffer, size); memxor((uint8*)buffer, r, eor); return r; }
	int			getc()								{ return istream_chain::getc() ^ eor; }
};

ISO_ptr<void> ReadTRE(tag id, istream_ref file) {
	return ReadRaw(id, file, file.length());
}
ISO_ptr<void> ReadRGN(tag id, istream_ref file) {
	return ReadRaw(id, file, file.length());
}
ISO_ptr<void> ReadLBL(tag id, istream_ref file) {
	return ReadRaw(id, file, file.length());
}
ISO_ptr<void> ReadNET(tag id, istream_ref file) {
	return ReadRaw(id, file, file.length());
}
ISO_ptr<void> ReadNOD(tag id, istream_ref file) {
	return ReadRaw(id, file, file.length());
}
ISO_ptr<void> ReadTYP(tag id, istream_ref file) {
	TYP_HEADER	h = file.get();
	return ReadRaw(id, file, file.length());
}

ISO_ptr<void> ReadGMP(tag id, istream_ref file) {
	GMP_HEADER	h = file.get();
	ISO_ptr<anything>	p(id);

	file.seek(h.TreOffset);
	p->Append(ReadTRE("TRE", make_reader_offset(file, h.RgnOffset - h.TreOffset)));
	file.seek(h.RgnOffset);
	p->Append(ReadRGN("RGN", make_reader_offset(file, h.LblOffset - h.RgnOffset)));
	file.seek(h.LblOffset);
	p->Append(ReadLBL("LBL", make_reader_offset(file, h.NetOffset - h.LblOffset)));
	file.seek(h.NetOffset);
	p->Append(ReadNET("NET", make_reader_offset(file, h.NodOffset - h.NetOffset)));
	file.seek(h.NodOffset);
	p->Append(ReadNOD("NOD", make_reader_offset(file)));

	return p;
}
class IMGFileHandler : public FileHandler {
	const char*		GetExt() override { return "img";	}
	const char*		GetDescription() override { return "Garmin DiskImage"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} img;

struct Garmin : anything {};
ISO_DEFUSER(Garmin, anything);

ISO_ptr<void> IMGFileHandler::Read(tag id, istream_ref file) {
	HEADER	h = file.get();
	istream_xor	xfile(file, h.eor);
	memxor(h, h.eor);

	xfile.seek(h.fat0_block * 0x200);
	ENTRY	root			= file.get();
	uint32	bytespercluster	= 1 << (h.E1 + h.E2);
	int		num_clusters	= (root.file_size + bytespercluster - 1) / bytespercluster;

	//auto	file2	= make_reader_offset(xfile);
	istream_cluster	file4(xfile, bytespercluster, root.file_size);
	file4.add_clusters(root.clusters, min(num_clusters, num_elements(root.clusters)));
	file4.seek(0x200);

	ISO_ptr<Garmin>	p(id);

	while (file4.tell() < root.file_size) {
		ENTRY	entry	= file4.get();

		if (entry.flags == 0x01) {
			int		num_clusters	= (entry.file_size + bytespercluster - 1) / bytespercluster;

			istream_cluster	file3(xfile, bytespercluster, entry.file_size);
			file3.add_clusters(entry.clusters, min(num_clusters, num_elements(entry.clusters)));

			while ((num_clusters -= int(num_elements(entry.clusters))) > 0)
				file3.add_clusters(file4.get<ENTRY>().clusters, min(num_clusters, num_elements(entry.clusters)));

			fixed_string<256>	name;
			name.format("%.8s.%.3s", entry.filename, entry.ext);

			if (str(entry.ext,3) == "GMP") {
				p->Append(ReadGMP(name, file3));
			} else if (str(entry.ext,3) == "TRE") {
				p->Append(ReadTRE(name, file3));
			} else if (str(entry.ext,3) == "RGN") {
				p->Append(ReadRGN(name, file3));
			} else if (str(entry.ext,3) == "LBL") {
				p->Append(ReadLBL(name, file3));
			} else if (str(entry.ext,3) == "NET") {
				p->Append(ReadNET(name, file3));
			} else if (str(entry.ext,3) == "NOD") {
				p->Append(ReadNOD(name, file3));
			} else if (str(entry.ext,3) == "TYP") {
				p->Append(ReadTYP(name, file3));
			} else {
				p->Append(ReadRaw(name, file3, entry.file_size));
			}
		}
	}

	return p;
}

class TYPFileHandler : public FileHandler {
	const char*		GetExt()			override { return "typ";	}
	const char*		GetDescription()	override { return "Garmin TYP file"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} typ;

ISO_ptr<void> TYPFileHandler::Read(tag id, istream_ref file) {
	TYP_HEADER	h = file.get();
	if (str(h.signature, 10) != "GARMIN TYP")
		return ISO_NULL;
	return ReadRaw(id, file, file.length());
};
