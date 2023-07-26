#include "iso/iso_files.h"
#include "extra/date.h"

using namespace iso;

#ifndef _WIN32
struct GUID {
    uint32		a;
	uint16		b, c;
    uint8		d[8];
};

bool operator==(const GUID &a, const GUID &b)	{ return memcmp(&a, &b, sizeof(GUID)) == 0; }
bool operator!=(const GUID &a, const GUID &b)	{ return memcmp(&a, &b, sizeof(GUID)) != 0; }

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};
#endif

struct Filetime : FILETIME {
	enum { SECOND = 10000000 };// since January 1, 1601
	uint64	time()			const	{ return *(uint64*)this;	}
	double	seconds()		const	{ return double(time() / (SECOND / 10000)) / 10000;	}
	float	secs_in_day()	const	{ return (time() % (SECOND * uint64(60 * 60 * 24))) / float(SECOND);	}
	uint32	day()			const	{ return time() / (SECOND * uint64(60 * 60 * 24));	} //25 bits
	Date	date()			const	{ return Date(day());	}
};

namespace ShellLink {

//SHELL_LINK = SHELL_LINK_HEADER [LINKTARGET_IDLIST] [LINKINFO] [STRING_DATA] *EXTRA_DATA

struct Header {
	enum FLAGS {
		HasLinkTargetIDList			= 1 << 0,	// The shell link is saved with an item ID list (IDList). If this bit is set, a LinkTargetIDList structure (section 2.2) MUST follow the ShellLinkHeader.
		HasLinkInfo					= 1 << 1,	// The shell link is saved with link information. If this bit is set, a LinkInfo structure (section 2.3) MUST be present.
		HasName						= 1 << 2,	// The shell link is saved with a name string. If this bit is set, a NAME_STRING StringData structure (section 2.4) MUST be present.
		HasRelativePath				= 1 << 3,	// The shell link is saved with a relative path string. If this bit is set, a RELATIVE_PATH StringData structure (section 2.4) MUST be present.
		HasWorkingDir				= 1 << 4,	// The shell link is saved with a working directory string. If this bit is set, a WORKING_DIR StringData structure (section 2.4) MUST be present.
		HasArguments				= 1 << 5,	// The shell link is saved with command line arguments. If this bit is set, a COMMAND_LINE_ARGUMENTS StringData structure (section 2.4) MUST be present.
		HasIconLocation				= 1 << 6,	// The shell link is saved with an icon location string. If this bit is set, an ICON_LOCATION StringData structure (section 2.4) MUST be present.
		IsUnicode					= 1 << 7,	// The shell link contains Unicode encoded strings. This bit SHOULD be set.
		ForceNoLinkInfo				= 1 << 8,	// The LinkInfo structure (section 2.3) is ignored.
		HasExpString				= 1 << 9,	// The shell link is saved with an EnvironmentVariableDataBlock (section 2.5.4).
		RunInSeparateProcess		= 1 << 10,	// The target is run in a separate virtual machine when launching a link target that is a 16-bit application.
		Unused1						= 1 << 11,	// A bit that is undefined and MUST be ignored.
		HasDarwinID					= 1 << 12,	// The shell link is saved with a DarwinDataBlock (section 2.5.3).
		RunAsUser					= 1 << 13,	// The application is run as a different user when the target of the shell link is activated.
		HasExpIcon					= 1 << 14,	// The shell link is saved with an IconEnvironmentDataBlock (section 2.5.5).
		NoPidlAlias					= 1 << 15,	// The file system location is represented in the shell namespace when the path to an item is parsed into an IDList.
		Unused2						= 1 << 16,	// A bit that is undefined and MUST be ignored.
		RunWithShimLayer			= 1 << 17,	// The shell link is saved with a ShimDataBlock (section 2.5.8).
		ForceNoLinkTrack			= 1 << 18,	// The TrackerDataBlock (section 2.5.10) is ignored.
		EnableTargetMetadata		= 1 << 19,	// The shell link attempts to collect target properties and store them in the PropertyStoreDataBlock (section 2.5.7) when the link target is set.
		DisableLinkPathTracking		= 1 << 20,	// The EnvironmentVariableDataBlock is ignored.
		DisableKnownFolderTracking	= 1 << 21,	// The SpecialFolderDataBlock (section 2.5.9) and the KnownFolderDataBlock (section 2.5.6) are ignored when loading the shell link. If this bit is set, these extra data blocks SHOULD NOT be saved when saving the shell link.
		DisableKnownFolderAlias		= 1 << 22,	// If the link has a KnownFolderDataBlock (section 2.5.6), the unaliased form of the known folder IDList SHOULD be used when translating the target IDList at the time that the link is loaded.
		AllowLinkToLink				= 1 << 23,	// Creating a link that references another link is enabled. Otherwise, specifying a link as the target IDList SHOULD NOT be allowed.
		UnaliasOnSave				= 1 << 24,	// When saving a link for which the target IDList is under a known folder, either the unaliased form of that known folder or the target IDList SHOULD be used.
		PreferEnvironmentPath		= 1 << 25,	// The target IDList SHOULD NOT be stored; instead, the path specified in the EnvironmentVariableDataBlock (section 2.5.4) SHOULD be used to refer to the target.
		KeepLocalIDListForUNCTarget	= 1 << 26,	// When the target is a UNC name that refers to a location on a local machine, the local path IDList in the PropertyStoreDataBlock (section 2.5.7) SHOULD be stored, so it can be used when the link is loaded on the local machine.
	};
#ifndef _WIN32
	enum FILE_ATTRIBUTES {
		FILE_ATTRIBUTE_READONLY				= 1 << 0,	// The file or directory is read-only. For a file, if this bit is set, applications can read the file but cannot write to it or delete it. For a directory, if this bit is set, applications cannot delete the directory.
		FILE_ATTRIBUTE_HIDDEN				= 1 << 1,	// The file or directory is hidden. If this bit is set, the file or folder is not included in an ordinary directory listing.
		FILE_ATTRIBUTE_SYSTEM				= 1 << 2,	// The file or directory is part of the operating system or is used exclusively by the operating system.
	//	Reserved1							= 1 << 3,	// A bit that MUST be zero.
		FILE_ATTRIBUTE_DIRECTORY			= 1 << 4,	// The link target is a directory instead of a file.
		FILE_ATTRIBUTE_ARCHIVE				= 1 << 5,	// The file or directory is an archive file. Applications use this flag to mark files for backup or removal.
	//	Reserved2							= 1 << 6,	// A bit that MUST be zero.
		FILE_ATTRIBUTE_NORMAL				= 1 << 7,	// The file or directory has no other flags set. If this bit is 1, all other bits in this structure MUST be clear.
		FILE_ATTRIBUTE_TEMPORARY			= 1 << 8,	// The file is being used for temporary storage.
		FILE_ATTRIBUTE_SPARSE_FILE			= 1 << 9,	// The file is a sparse file.
		FILE_ATTRIBUTE_REPARSE_POINT		= 1 << 10,	// The file or directory has an associated reparse point.
		FILE_ATTRIBUTE_COMPRESSED			= 1 << 11,	// The file or directory is compressed. For a file, this means that all data in the file is compressed. For a directory, this means that compression is the default for newly created files and subdirectories.
		FILE_ATTRIBUTE_OFFLINE				= 1 << 12,	// The data of the file is not immediately available.
		FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	= 1 << 13,	// The contents of the file need to be indexed.
		FILE_ATTRIBUTE_ENCRYPTED			= 1 << 14,	// The file or directory is encrypted. For a file, this means that all data in the file is encrypted. For a directory, this means that encryption is the default for newly created files and subdirectories.
	};
#endif
	enum HOTKEY_FLAGS {
		HOTKEYF_SHIFT	= 0x01,	//The "SHIFT" key on the keyboard.
		HOTKEYF_CONTROL	= 0x02,	//The "CTRL" key on the keyboard.
		HOTKEYF_ALT		= 0x04,	//The "ALT" key on the keyboard.
	};

	uint32		HeaderSize;
	GUID		ClsID;	//	00021401-0000-0000-C000-000000000046.
	uint32		Flags;
	uint32		FileAttributes;
	Filetime	CreationTime;
	Filetime	AccessTime;
	Filetime	WriteTime;
	uint32		FileSize;
	uint32		IconIndex;
	uint32		ShowCommand;
	uint16		HotKey, Reserved1;
	uint32		Reserved2;
	uint32		Reserved3;
};

struct IDList {
	uint16	size;
	uint16	list[];
};

struct ItemID {
	uint16	size;
	uint16	data[];
};

struct LinkInfo {
	uint32	LinkInfoSize;
	uint32	LinkInfoHeaderSize;
	uint32	LinkInfoFlags;
	uint32	VolumeIDOffset;
	uint32	LocalBasePathOffset;
	uint32	CommonNetworkRelativeLinkOffset;
	uint32	CommonPathSuffixOffset;
	uint32	LocalBasePathOffsetUnicode;		// (optional)
	uint32	CommonPathSuffixOffsetUnicode;	// (optional)
	//VolumeID (variable)...
	//LocalBasePath (variable)...
	//CommonNetworkRelativeLink (variable)...
	//CommonPathSuffix (variable)...
	//LocalBasePathUnicode (variable)...
	//CommonPathSuffixUnicode (variable)
};

//STRING_DATA = [NAME_STRING] [RELATIVE_PATH] [WORKING_DIR] [COMMAND_LINE_ARGUMENTS] [ICON_LOCATION]
//NAME_STRING: An optional structure that specifies a description of the shortcut that is displayed to end users to identify the purpose of the shell link. This structure MUST be present if the HasName flag is set.
//RELATIVE_PATH: An optional structure that specifies the location of the link target relative to the file that contains the shell link. When specified, this string SHOULD be used when resolving the link. This structure MUST be present if the HasRelativePath flag is set.
//WORKING_DIR: An optional structure that specifies the file system path of the working directory to be used when activating the link target. This structure MUST be present if the HasWorkingDir flag is set.
//COMMAND_LINE_ARGUMENTS: An optional structure that stores the command-line arguments that should be specified when activating the link target. This structure MUST be present if the HasArguments flag is set.
//ICON_LOCATION: An optional structure that specifies the location of the icon to be used when displaying a shell link item in an icon view. This structure MUST be present if the HasIconLocation flag is set.

struct StringItem {
	uint16	count;
	uint16	string[];	// not NULL terminated
};

}

class LNKFileHandler : public FileHandler {
	const char*		GetExt() override { return "lnk";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} lnk;

ISO_ptr<void> LNKFileHandler::Read(tag id, istream_ref file) {
	using namespace ShellLink;

	GUID	linkid	= {0x00021401, 0x0000, 0x0000, {0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

	Header	header = file.get();
	if (header.ClsID != linkid)
		return ISO_NULL;

	buffer_accum<256>	ba;
	ba << header.CreationTime.date();
	ba << "  ";
	ba << TimeOfDay(header.CreationTime.secs_in_day());

	return ISO_NULL;
}

