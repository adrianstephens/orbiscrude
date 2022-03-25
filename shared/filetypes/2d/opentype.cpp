#include "iso/iso_files.h"

//EOT was documented by Microsoft here:
//	http://www.w3.org/Submission/2008/SUBM-EOT-20080305
//TTLoadEmbeddedFont is described here:
//	http://msdn.microsoft.com/en-us/library/dd145155(VS.85).aspx
//Chromium:
//	http://src.chromium.org/viewvc/chrome/trunk/deps/third_party/WebKit/WebCore/platform/graphics/win/OpenTypeUtilities.cpp?view=log&pathrev=7591

using namespace iso;

typedef uint32 Fixed;

#define DEFAULT_CHARSET 1

#pragma pack(1)

struct EOTPrefix {
	uint32		eotSize;
	uint32		fontDataSize;
	uint32		version;
	uint32		flags;
	uint8		fontPANOSE[10];
	uint8		charset;
	uint8		italic;
	uint32		weight;
	uint16		fsType;
	uint16		magicNumber;
	uint32		unicodeRange[4];
	uint32		codePageRange[2];
	uint32		checkSumAdjustment;
	uint32		reserved[4];
	uint16		padding1;
};

struct TableDirectoryEntry {
	uint32be	tag;
	uint32be	checkSum;
	uint32be	offset;
	uint32be	length;
};

struct sfntHeader {
	Fixed		version;
	uint16be	numTables;
	uint16be	searchRange;
	uint16be	entrySelector;
	uint16be	rangeShift;
	TableDirectoryEntry	tables[1];
};

struct OS2Table {
	uint16be	version;
	uint16be	avgCharWidth;
	uint16be	weightClass;
	uint16be	widthClass;
	uint16be	fsType;
	uint16be	subscriptXSize;
	uint16be	subscriptYSize;
	uint16be	subscriptXOffset;
	uint16be	subscriptYOffset;
	uint16be	superscriptXSize;
	uint16be	superscriptYSize;
	uint16be	superscriptXOffset;
	uint16be	superscriptYOffset;
	uint16be	strikeoutSize;
	uint16be	strikeoutPosition;
	uint16be	familyClass;
	uint8		panose[10];
	uint32be	unicodeRange[4];
	uint8		vendID[4];
	uint16be	fsSelection;
	uint16be	firstCharIndex;
	uint16be	lastCharIndex;
	uint16be	typoAscender;
	uint16be	typoDescender;
	uint16be	typoLineGap;
	uint16be	winAscent;
	uint16be	winDescent;
	uint32be	codePageRange[2];
	uint16be	xHeight;
	uint16be	capHeight;
	uint16be	defaultChar;
	uint16be	breakChar;
	uint16be	maxContext;
};

struct headTable {
	Fixed		version;
	Fixed		fontRevision;
	uint32be	checkSumAdjustment;
	uint32be	magicNumber;
	uint16be	flags;
	uint16be	unitsPerEm;
	int64		created;
	int64		modified;
	uint16be	xMin, xMax, yMin, yMax;
	uint16be	macStyle;
	uint16be	lowestRectPPEM;
	uint16be	fontDirectionHint;
	uint16be	indexToLocFormat;
	uint16be	glyphDataFormat;
};

struct nameRecord {
	uint16be	platformID;
	uint16be	encodingID;
	uint16be	languageID;
	uint16be	nameID;
	uint16be	length;
	uint16be	offset;
};

struct nameTable {
	uint16be	format;
	uint16be	count;
	uint16be	stringOffset;
	nameRecord	nameRecords[1];
};

#pragma pack()

static void appendBigEndianString(ostream_ref file, const uint16be* string, uint16 length) {
	file.write(length);
	file.writebuff(string, length);
	file.write(uint16(0));
}

bool getEOTHeader(uint8* fontData, size_t fontSize, ostream_ref file, size_t &overlayDst, size_t &overlaySrc, size_t &overlayLength) {
	overlayDst		= 0;
	overlaySrc		= 0;
	overlayLength	= 0;

	EOTPrefix prefix;
	clear(prefix);
	prefix.fontDataSize = uint32(fontSize);
	prefix.version		= 0x00020001;
	prefix.charset		= DEFAULT_CHARSET;
	prefix.magicNumber	= 0x504c;

	if (fontSize < iso_offset(sfntHeader, tables))
		return false;

	const sfntHeader* sfnt = reinterpret_cast<const sfntHeader*>(fontData);

	if (fontSize < iso_offset(sfntHeader, tables) + sfnt->numTables * sizeof(TableDirectoryEntry))
		return false;

	bool haveOS2	= false;
	bool haveHead	= false;
	bool haveName	= false;

	const uint16be*	familyName = 0;
	uint16			familyNameLength = 0;
	const uint16be*	subfamilyName = 0;
	uint16			subfamilyNameLength = 0;
	const uint16be*	fullName = 0;
	uint16			fullNameLength = 0;
	const uint16be*	versionString = 0;
	uint16			versionStringLength = 0;

	for (uint32 i = 0; i < sfnt->numTables; i++) {
		uint32 tableOffset = sfnt->tables[i].offset;
		uint32 tableLength = sfnt->tables[i].length;

		if (fontSize < tableOffset || fontSize < tableLength || fontSize < tableOffset + tableLength)
			return false;

		switch (sfnt->tables[i].tag) {
			case 'OS/2': {
				if (fontSize < tableOffset + sizeof(OS2Table))
					return false;

				haveOS2 = true;
				const OS2Table* OS2 = reinterpret_cast<const OS2Table*>(fontData + tableOffset);
				for (uint32 j = 0; j < 10; j++)
					prefix.fontPANOSE[j] = OS2->panose[j];
				prefix.italic = OS2->fsSelection & 0x01;
				prefix.weight = OS2->weightClass;
				// FIXME: Should use OS2->fsType, but some TrueType fonts set it to an over-restrictive value.
				// Since ATS does not enforce this on Mac OS X, we do not enforce it either.
				prefix.fsType = 0;
				for (uint32 j = 0; j < 4; j++)
					prefix.unicodeRange[j] = OS2->unicodeRange[j];
				for (uint32 j = 0; j < 2; j++)
					prefix.codePageRange[j] = OS2->codePageRange[j];
				break;
			}
			case 'head': {
				if (fontSize < tableOffset + sizeof(headTable))
					return false;

				haveHead = true;
				const headTable* head = reinterpret_cast<const headTable*>(fontData + tableOffset);
				prefix.checkSumAdjustment = head->checkSumAdjustment;
				break;
			}
			case 'name': {
				if (fontSize < tableOffset + iso_offset(nameTable, nameRecords))
					return false;

				haveName = true;
				const nameTable* name = reinterpret_cast<const nameTable*>(fontData + tableOffset);
				for (int j = 0; j < name->count; j++) {
					if (fontSize < tableOffset + iso_offset(nameTable, nameRecords) + (j + 1) * sizeof(nameRecord))
						return false;
					if (name->nameRecords[j].platformID == 3 && name->nameRecords[j].encodingID == 1 && name->nameRecords[j].languageID == 0x0409) {
						if (fontSize < tableOffset + name->stringOffset + name->nameRecords[j].offset + name->nameRecords[j].length)
							return false;

						uint16 nameLength = name->nameRecords[j].length;
						const uint16be* nameString = reinterpret_cast<const uint16be*>(fontData + tableOffset + name->stringOffset + name->nameRecords[j].offset);

						switch (name->nameRecords[j].nameID) {
							case 1:
								familyNameLength = nameLength;
								familyName = nameString;
								break;
							case 2:
								subfamilyNameLength = nameLength;
								subfamilyName = nameString;
								break;
							case 4:
								fullNameLength = nameLength;
								fullName = nameString;
								break;
							case 5:
								versionStringLength = nameLength;
								versionString = nameString;
								break;
							default:
								break;
						}
					}
				}
				break;
			}
			default:
				break;
		}
		if (haveOS2 && haveHead && haveName)
			break;
	}

	file.seek(sizeof(EOTPrefix));

	appendBigEndianString(file, familyName, familyNameLength);
	appendBigEndianString(file, subfamilyName, subfamilyNameLength);
	appendBigEndianString(file, versionString, versionStringLength);

	// If possible, ensure that the family name is a prefix of the full name.
	if (fullNameLength >= familyNameLength && memcmp(familyName, fullName, familyNameLength)) {
		overlaySrc = (uint8*)fullName	- fontData;
		overlayDst = (uint8*)familyName	- fontData;
		overlayLength = familyNameLength;
	}

	appendBigEndianString(file, fullName, fullNameLength);

	file.write(uint16(0));
	prefix.eotSize = file.size32() + uint32(fontSize);

	file.seek(0);
	file.write(prefix);

	return true;
}
