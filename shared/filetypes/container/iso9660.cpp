#include "iso/iso_files.h"
#include "base/algorithm.h"
#include "filetypes/sound/sample.h"
#include "extra/date.h"
#include "archive_help.h"

using namespace iso;

typedef anything directory;

struct directory_info {
	ISO_ptr<directory>	dir;
	directory_info*		parent;
	int					record;
	uint32				start;
	uint32				length;
};

#define	CD_SECTOR_SIZE	2048
#define CDF_HIDDEN		1
#define CDF_DIRECTORY	2

#pragma pack(1)

template<int N> struct pad_string {
	char	buffer[N];
	void	operator=(const char *s)	{ int len = min(int(s ? strlen(s) : 0), N); memcpy(buffer, s, len); memset(buffer + len, ' ', N - len); }
	bool	operator==(const char *s)	{ int len = s ? int(strlen(s)) : 0; return len <= N && memcmp(buffer, s, len) == 0 && (len == N || buffer[len] == ' '); }
};

struct VOLUMEDESCRIPTOR {
	uint8			type;
	pad_string<5>	id;
	uint8			version;
	uint8			data[2041];
};

template<int N> struct UDF_dstring {
	char	buffer[N];
	void	operator=(const char *s)	{ int len = buffer[N-1] = min(int(s ? strlen(s) : 0), N - 1); memcpy(buffer, s, len); memset(buffer + len, 0, N - 1 - len); }
};

struct UDF_Charspec {
	uint8			CharacterSetType;
	uint8			CharacterSetInfo[63];
};

struct UDF_EntityID {
	uint8			flags;
	char			identifier[23];
	char			identifiersuffix[8];
};

struct UDF_ExtentAd {
	uint32			length;
	uint32			location;
};

struct UDF_lb_addr {
	uint32			location;
	uint16			partition;
};

struct UDF_ShortAd {
	uint32			length:30, flags:2;
	uint32			location;
};

struct UDF_LongAd {
	uint32			length:30, flags:2;
	uint32			location;
	uint16			partition;
	uint8			implementation[6];
};

struct UDF_ExtAd {
	uint32			length:30, flags:2;
	uint32			reclength;
	uint32			infolength;
	uint32			location;
	uint16			partition;
};

struct UDF_tag {
	uint16			TagIdentifier;
	uint16			DescriptorVersion;
	uint8			TagChecksum;
	uint8			Reserved;
	uint16			TagSerialNumber;
	uint16			DescriptorCRC;
	uint16			DescriptorCRCLength;
	uint32			TagLocation;
};

struct UDF_AnchorVolumeDescriptorPointer {
	UDF_tag			DescriptorTag;
	UDF_ExtentAd	MainVolumeDescriptorSequenceExtent;
	UDF_ExtentAd	ReserveVolumeDescriptorSequenceExtent;
	uint8			reserved[480];
};

struct UDF_timestamp {
	uint16			typeandtimezone;
	uint16			year;
	uint8			month;
	uint8			day;
	uint8			hour;
	uint8			minute;
	uint8			second;
	uint8			centiseconds;
	uint8			hundredsofmicroseconds;
	uint8			microseconds;
};

struct UDF_icbtag {
	uint32			PriorRecordedNumberofDirectEntries;
	uint16			StrategyType;
	uint8			StrategyParameter[2];
	uint16			NumberofEntries;
	uint8			Reserved;
	uint8			FileType;
	UDF_lb_addr		ParentICBLocation;
	uint16			Flags;
};

struct UDF_LogicalVolumeDescriptor { /* ISO 13346 3/10.6 */
	UDF_tag			DescriptorTag;
	uint32			VolumeDescriptorSequenceNumber;
	UDF_Charspec	DescriptorCharacterSet;
	UDF_dstring<128>LogicalVolumeIdentifier;
	uint32			LogicalBlockSize;
	UDF_EntityID	DomainIdentifier;
	uint8			LogicalVolumeContentsUse[16];
	uint32			MapTableLength;
	uint32			NumberofPartitionMaps;
	UDF_EntityID	ImplementationIdentifier;
	uint8			ImplementationUse[128];
	UDF_ExtentAd	IntegritySequenceExtent;
//	uint8			PartitionMaps[??];
};

struct UDF_PrimaryVolumeDescriptor {
	UDF_tag 		DescriptorTag;
	uint32			VolumeDescriptorSequenceNumber;
	uint32			PrimaryVolumeDescriptorNumber;
	UDF_dstring<32>	VolumeIdentifier;
	uint16			VolumeSequenceNumber;
	uint16			MaximumVolumeSequenceNumber;
	uint16			InterchangeLevel;
	uint16			MaximumInterchangeLevel;
	uint32			CharacterSetList;
	uint32			MaximumCharacterSetList;
	UDF_dstring<128>VolumeSetIdentifier;
	UDF_Charspec	DescriptorCharacterSet;
	UDF_Charspec	ExplanatoryCharacterSet;
	UDF_ExtentAd	VolumeAbstract;
	UDF_ExtentAd	VolumeCopyrightNotice;
	UDF_EntityID	ApplicationIdentifier;
	UDF_timestamp	RecordingDateandTime;
	UDF_EntityID	ImplementationIdentifier;
	uint8			ImplementationUse[64];
	uint32			PredecessorVolumeDescriptorSequenceLocation;
	uint16			Flags;
	uint8			reserved[22];
};

struct UDF_PartitionDescriptor {
	UDF_tag			DescriptorTag;
	uint32			VolumeDescriptorSequenceNumber;
	uint16			PartitionFlags;
	uint16			PartitionNumber;
	UDF_EntityID	PartitionContents;
	uint8			PartitionContentsUse[128];
	uint32			AccessType;
	uint32			PartitonStartingLocation;
	uint32			PartitionLength;
	UDF_EntityID	ImplementationIdentifier;
	uint8			ImplementationUse[128];
	uint8			reserved[156];
};

struct UDF_PartitionHeaderDescriptor {
	UDF_ShortAd		UnallocatedSpaceTable;
	UDF_ShortAd		UnallocatedSpaceBitmap;
	UDF_ShortAd		PartitionIntegrityTable;
	UDF_ShortAd		FreedSpaceTable;
	UDF_ShortAd		FreedSpaceBitmap;
	uint8			reserved[88];
};

struct UDF_FileSetDescriptor {
	UDF_tag			DescriptorTag;
	UDF_timestamp	RecordingDateandTime;
	uint16			InterchangeLevel;
	uint16			MaximumInterchangeLevel;
	uint32			CharacterSetList;
	uint32			MaximumCharacterSetList;
	uint32			FileSetNumber;
	uint32			FileSetDescriptorNumber;
	UDF_Charspec	LogicalVolumeIdentifierCharacterSet;
	UDF_dstring<128>LogicalVolumeIdentifier;
	UDF_Charspec	FileSetCharacterSet;
	UDF_dstring<32>	FileSetIdentifer;
	UDF_dstring<32>	CopyrightFileIdentifier;
	UDF_dstring<32>	AbstractFileIdentifier;
	UDF_LongAd		RootDirectoryICB;
	UDF_EntityID	DomainIdentifier;
	UDF_LongAd		NextExtent;
	uint8			reserved[48];
};

struct UDF_FileEntry {
	UDF_tag			DescriptorTag;
	UDF_icbtag		ICBTag;
	uint32			Uid;
	uint32			Gid;
	uint32			Permissions;
	uint16			FileLinkCount;
	uint8			RecordFormat;
	uint8			RecordDisplayAttributes;
	uint32			RecordLength;
	uint64			InformationLength;
	uint64			LogicalBlocksRecorded;
	UDF_timestamp	AccessTime;
	UDF_timestamp	ModificationTime;
	UDF_timestamp	AttributeTime;
	uint32			Checkpoint;
	UDF_LongAd		ExtendedAttributeICB;
	UDF_EntityID	ImplementationIdentifier;
	uint64			UniqueID;
	uint32			LengthofExtendedAttributes;
	uint32			LengthofAllocationDescriptors;
	//byte ExtendedAttributes[??];
	//byte AllocationDescriptors[??];

	uint32			GetAddress() const;
};

struct UDF_FileIdentifierDescriptor {
	UDF_tag			DescriptorTag;
	uint16			FileVersionNumber;
	uint8			FileCharacteristics;
	uint8			LengthofFileIdentifier;
	UDF_LongAd		ICB;
	uint16			LengthofImplementationUse;
	//byte			ImplementationUse[??];
	//char			FileIdentifier[??];
	//byte			Padding[??];
};

uint32 UDF_FileEntry::GetAddress() const {
	uint32	ad	= 0;
	for (uint8 *p = (uint8*)(this + 1) + LengthofExtendedAttributes, *e = p + LengthofAllocationDescriptors; p < e; ) {
		switch (ICBTag.Flags & 7) {
			case 0: ad = ((UDF_ShortAd*)p)->location;	p += sizeof(UDF_ShortAd);	break;
			case 1: ad = ((UDF_LongAd*)p)->location;	p += sizeof(UDF_LongAd);	break;
			case 2: ad = ((UDF_ExtAd*)p)->location;		p += sizeof(UDF_ExtAd);		break;
/*			case 3:
				switch (LengthofAllocationDescriptors) {
					case 8: ad = ((UDF_ShortAd*)p)->location;	p += sizeof(UDF_ShortAd);	break;
					case 16: ad = ((UDF_LongAd*)p)->location;	p += sizeof(UDF_LongAd);	break;
					case 32: ad = ((UDF_ExtAd*)p)->location;	p += sizeof(UDF_ExtAd);		break;
				}
*/			default:
				p = e;
				break;
		}
	}
	return ad;
}

void UDF_DecodeUnicode(char *dest, char *srce, int len) {
	char	*end = srce + len;
	int		type = *srce++;
	if (type == 8 || type == 16) do {
		if (type == 16)
			srce++;  // Ignore MSB of unicode16
		if (srce < end)
			*dest++ = *srce++;
	} while (srce < end);

	*dest = 0;
}

class BOTHW {
	uint16le	le;
	uint16be	be;
public:
	void	operator=(uint16 x)	{ le = x; be = x;	}
	operator	uint16()		{ return le; }
};

class BOTHD {
	uint32le	le;
	uint32be	be;
public:
	void	operator=(uint32 x)	{ le = x; be = x;	}
	operator	uint32()		{ return le; }
};

class DATETIME {
	char		year[4];					// 0x00
	char		month[2];					// 0x04
	char		day[2];						// 0x06
	char		hour[2];					// 0x08
	char		minute[2];					// 0x0a
	char		second[2];					// 0x0c
	char		hundredths[2];				// 0x0e
	int8		GMToffset;					// 0x10

public:
	void		Blank()	{ memset(this, '0', sizeof(*this)); GMToffset = 0; }
	void		Set(DateTime &time, int8 gmt);
};											// 0x11

void DATETIME::Set(DateTime &time, int8 gmt) {
	Date		d(time.Day());
	TimeOfDay	t(time.TimeOfDay());
	int			s	= int(t.Sec());

	sprintf(year, "%04i%02i%02i%02i%02i%02i%02i",
		d.year,
		d.month,
		d.day,
		t.Hour(),
		t.Min(),
		s,
		int((t.Sec() - s) * 100)
	);
	GMToffset = gmt;
}

struct CD_DIRENTRY {
	uint8		size;						// 0x00
	uint8		EARlen;						// 0x01
	BOTHD		start;						// 0x02
	BOTHD		length;						// 0x0a
	uint8		year;						// 0x12
	uint8		month;						// 0x13
	uint8		day;						// 0x14
	uint8		hour;						// 0x15
	uint8		minute;						// 0x16
	uint8		second;						// 0x17
	int8		GMToffset;					// 0x18
	uint8		flags;						// 0x19
	uint8		unitsize;					// 0x1a
	uint8		gapsize;					// 0x1b
	BOTHW		seq_no;						// 0x1c
	uint8		namelen;					// 0x20
//	char		name[];						// 0x21
};											// 0x22 (min)

struct CD_DIRENTRY2 : CD_DIRENTRY {
#pragma warning(suppress:4200)
	char		name[];
};

struct PRIMARYVOLUMEDESCRIPTOR {
	fixed_string<8>	header;					// 0x00
	pad_string<32>	sys_ident;				// 0x08
	pad_string<32>	vol_ident;				// 0x28
	char			pad0[8];				// 0x48
	BOTHD			num_sectors;			// 0x50
	char			pad1[32];				// 0x58
	BOTHW			set_size;				// 0x78
	BOTHW			seq_no;					// 0x7c
	BOTHW			sector_size;			// 0x80
	BOTHD			pathtable_len;			// 0x84
	uint32le		le_pathtable[2];		// 0x8c
	uint32be		be_pathtable[2];		// 0x94
	CD_DIRENTRY		root;					// 0x9c
	char			root_name[1];			// 0x21
	pad_string<128>	set_ident;				// 0xbe
	pad_string<128>	pub_ident;				// 0x13e
	pad_string<128>	prep_ident;				// 0x1be
	pad_string<128>	app_ident;				// 0x23e
	pad_string<37>	copyright_file;			// 0x2be
	pad_string<37>	abstract_file;			// 0x2e3
	pad_string<37>	bibliographical_file;	// 0x308
	DATETIME		create;					// 0x32d
	DATETIME		modify;					// 0x33e
	DATETIME		expire;					// 0x34f
	DATETIME		effective;				// 0x360
	char			pad2[2];				// 0x371
};											// 0x373

template<typename endian> struct PATHTABLE : endian {
	uint8		namelen;
	uint8		EARlen;
	uint32		sectorstart;
	uint16		record;
//	char		name[1];
};

#pragma pack()

class ISO9660FileHandler : public FileHandler {
	friend class CUEFileHandler;
	const char*			GetExt() override { return "iso"; }

	static void					MakePathTable(directory_info *dirs, ISO_ptr<directory> root);
	template<typename E> void	WritePathTable(ostream_ref file, directory_info *dirs, int n);
	void						WriteFile(ostream_ref file, ISO_ptr<void> p);
	void						Read2(ISO_ptr<anything> &pak, istream_ref file);

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything> t;
		t.Create(id);

		char					test[20];
		static unsigned char	match1[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
		static char				match2[] = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00};
		file.readbuff(test, 20);

		if (memcmp(test, match1, 12) == 0)
			Read2(t, interleaved_reader<istream_ref>(file, CD_SECTOR_SIZE, 0x10, 2352));
		else if (memcmp(test, match1 + 4, 8) == 0 || memcmp(test + 12, match2, 8) == 0)
			Read2(t, interleaved_reader<istream_ref>(file, CD_SECTOR_SIZE, 0x14, 2348));
		else
			Read2(t, file);

		return t;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} iso9660;

static int GetString(istream_ref file, char *buffer, int len) {
	int	i;
	for (i = 0; i < len; ) {
		int	c = file.getc();
		if (c == EOF || (c == '\n' && i > 0))
			break;
		if (c >= ' ')
			buffer[i++] = c;
	}
	if (i < len)
		buffer[i] = 0;
	return i;
}

static const char *Is(const char *buffer, const char *command) {
	size_t	len = strlen(command);
	return istr(buffer, len) == command && buffer[len] <= 32 ? buffer + len : NULL;
}

class CUEFileHandler : public FileHandler {
	enum MODE {AUDIO_LE, AUDIO_BE, CDG, MODE1, MODE2, CDI};

	ISO_ptr<void> ReadTrack(istream_ref file, streamptr start, streamptr end, MODE mode, int sectlen, char *name);

	const char*		GetExt() override { return "cue"; }
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
} cue;

ISO_ptr<void> CUEFileHandler::ReadTrack(istream_ref file, streamptr start, streamptr end, MODE mode, int sectlen, char *name) {
	file.seek(start);
	if (mode == AUDIO_LE || mode == AUDIO_BE) {
		ISO_ptr<sample>	s(name);
		s->Create((end - start) / 4, 2, 16);
		s->SetFrequency(44100);
		int16	*p = s->Samples();
		file.readbuff(p, end - start);
		if (mode == AUDIO_BE) {
			for (int n = (end - start) / 2; n--; p++)
				*p = *(int16be*)p;
		}
		return s;
	} else {
		ISO_ptr<anything> t(name);
		iso9660.Read2(t, interleaved_reader<istream_ref>(file, CD_SECTOR_SIZE, 0x10, sectlen));
		return t;
	}
}

ISO_ptr<void> CUEFileHandler::ReadWithFilename(tag id, const filename &fn) {
	FileInput	binfile;
	char		buffer[256];
	char		binname[256], modename[256], format[256], title[256], prev_title[256];
	int			track	= -1, prev_track = -1, sectlen = -1, prev_sectlen;
	MODE		mode	= AUDIO_LE, prev_mode;
	streamptr	fp	= 0, start = 0;

	ISO_ptr<anything>	t(id);
	FileInput			file(fn);

	while (GetString(file, buffer, sizeof(buffer))) {
		const char	*p = buffer, *p2;
		while (*p == ' ') p++;
		if (p2 = Is(p, "FILE")) {
			if (sscanf(p2, "%s %s", &binname, format) != 2 || !binfile.open(filename(fn).rem_dir().add_dir(filename(binname))))
				return ISO_NULL;
		} else if (p2 = Is(p, "TRACK")) {
			prev_track		= track;
			prev_sectlen	= sectlen;
			prev_mode		= mode;
			strcpy(prev_title, title);
			if (sscanf(p2, "%d %[^/]/%d", &track, modename, &sectlen) >= 2) {
				if (strcmp(modename, "AUDIO") == 0) {
					mode	= strcmp(format, "MOTOROLA") == 0 ? AUDIO_BE : AUDIO_LE;
					sectlen = 2352;
				} else if (strcmp(modename, "CDG") == 0) {
					mode	= CDG;
					sectlen	= 2448;
				} else if (strcmp(modename, "MODE1") == 0) {
					mode	= MODE1;
				} else if (strcmp(modename, "MODE2") == 0) {
					mode	= MODE2;
				} else if (strcmp(modename, "CDI") == 0) {
					mode	= CDI;
				}
				start	= fp;
				sprintf(title, "TRACK%02i", track);
			}
		} else if (p2 = Is(p, "INDEX")) {
			int	indexno, mm, ss, ff;
			if (sscanf(p2, "%d %d:%d:%d", &indexno, &mm, &ss, &ff) == 4)
				fp = ((mm * 60 + ss) * 75 + ff) * sectlen;
			if (prev_track != -1) {
				prev_track = -1;
				t->Append(ReadTrack(binfile, start, fp, prev_mode, prev_sectlen, prev_title));
			}
		} else if (p2 = Is(p, "TITLE")) {
			sscanf(p2, "%s", title);
		}
	}

	if (track != -1)
		t->Append(ReadTrack(binfile, fp, binfile.length(),  mode, sectlen, title));
	return t;
}

bool CUEFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
	char	temp[256];
	if (p.IsType<anything>()) {
		uint8				zero[588 * 4]	= {0};
		filename			binfn			= filename(fn).set_ext("iso");
		FileOutput			cue(fn);
		FileOutput			bin(binfn);
		ISO_ptr<anything>	t(p);

		uint32				time = 0;
		cue.writebuff(temp, sprintf(temp, "FILE \"%s\" BINARY\n\n", (char*)binfn));
		for (int i = 0, n = t->Count(); i < n; i++) {

			ISO_ptr<void>	track = (*t)[i];
			if (track.IsType<sample>()) {
				ISO_ptr<sample>	s = resample((ISO_ptr<sample>&)track, 44100);

				cue.writebuff(temp, sprintf(temp, "    TRACK %02d AUDIO\n      INDEX 01 %02d:%02d:%02d\n", i, time / (60 * 75), (time / 75) %60, time % 75));

				time += int(s->Length() + 587) / 588;
				bin.writebuff(s->Samples(), s->Length() * 4);
				if (int n = (s->Length() % 588))
					bin.writebuff(zero, (588 - n) * 4);
			} else {
			}
		}
		return true;
	}
	return false;
}

//-------------------------------------
//	read
//-------------------------------------

void ISO9660FileHandler::Read2(ISO_ptr<anything> &pak, istream_ref file) {
	uint8	sector[CD_SECTOR_SIZE];
	bool	raw = WantRaw();

	file.seek(16 * CD_SECTOR_SIZE);
	PRIMARYVOLUMEDESCRIPTOR	pvd;

	int	udf = 0;
	for (bool ea = false;;) {
		file.read(sector);
		if (!sector[1])
			break;
		VOLUMEDESCRIPTOR	*volume	= (VOLUMEDESCRIPTOR*)sector;
		if (volume->id == "CD001" && volume->type == 1)
			pvd = *(PRIMARYVOLUMEDESCRIPTOR*)volume;
		else if (volume->id == "BEA01")
			ea = true;
		else if (volume->id == "TEA01")
			ea = false;
		else if (ea && volume->id == "NSR02")
			udf = 2;
		else if (ea && volume->id == "NSR03")
			udf = 3;
	}

	if (udf) {
		uint32	pstart, plength;

		file.seek(256 * CD_SECTOR_SIZE);
		UDF_AnchorVolumeDescriptorPointer	avdp(file.get());

		for (int i = avdp.MainVolumeDescriptorSequenceExtent.location, n = avdp.MainVolumeDescriptorSequenceExtent.length / CD_SECTOR_SIZE; n--; i++) {
			file.seek(i * CD_SECTOR_SIZE);
			file.read(sector);

			UDF_tag	*tag = (UDF_tag*)sector;
			switch (tag->TagIdentifier) {
				case 1: {
					UDF_PrimaryVolumeDescriptor	*vol	= (UDF_PrimaryVolumeDescriptor*)sector;
					break;
				}
				case 5:	{//partition
					UDF_PartitionDescriptor		*part	= (UDF_PartitionDescriptor*)sector;
					pstart	= part->PartitonStartingLocation;
					plength	= part->PartitionLength;
					break;
				}
				case 6:	{//logical volume descriptor
					UDF_LogicalVolumeDescriptor	*logvol	= (UDF_LogicalVolumeDescriptor*)sector;
					break;
				}
				case 8:	// terminating descriptor
					n = 0;
					break;
			}

		}
		file.seek(pstart * CD_SECTOR_SIZE);
		file.read(sector);
		UDF_FileSetDescriptor	*fsd = (UDF_FileSetDescriptor*)sector;

		file.seek(streamptr(pstart + fsd->RootDirectoryICB.location) * CD_SECTOR_SIZE);
		file.read(sector);
		UDF_FileEntry			*fe	= (UDF_FileEntry*)sector;

		struct STACK {
			uint32		start, length;
			ISO_ptr<directory>	dir;
		};
		dynamic_array<STACK>	stack;

		STACK	*sp	= new(stack) STACK;
		sp->start	= (pstart + fe->GetAddress()) * CD_SECTOR_SIZE;
		sp->length	= fe->InformationLength;
		sp->dir		= pak;
		sp++;

		while (!stack.empty()) {
			sp	= &stack.pop_back_retref();
			ISO_ptr<directory> dir = sp->dir;
			file.seek(sp->start);
			malloc_block	buffer(sp->length);
			file.readbuff(buffer, sp->length);

			for (uint8 *p = (uint8*)buffer, *e = p + sp->length; p < e; ) {
				UDF_FileIdentifierDescriptor	*f = (UDF_FileIdentifierDescriptor*)p;

				if (f->LengthofFileIdentifier) {
					char	name[256];
					UDF_DecodeUnicode(name, (char*)(f + 1) + f->LengthofImplementationUse, f->LengthofFileIdentifier);

					file.seek((pstart + f->ICB.location) * CD_SECTOR_SIZE);
					file.read(sector);

					UDF_FileEntry	*fe = (UDF_FileEntry*)sector;
					uint32			ad	= fe->GetAddress();

					switch (fe->ICBTag.FileType) {
						case 4: { //dir
							ISO_ptr<anything>	subdir(name);
							dir->Append(subdir);
							sp			= new(stack) STACK;
							sp->dir		= subdir;
							sp->length	= fe->InformationLength;
							sp->start	= ad ? (pstart + ad) * CD_SECTOR_SIZE : (pstart + f->ICB.location) * CD_SECTOR_SIZE + sizeof(UDF_FileEntry) + fe->LengthofExtendedAttributes;
							break;
						}
						case 5: //file
							if (ad) {
								if (fe->InformationLength < 0x1000000) {
									file.seek((pstart + ad) * CD_SECTOR_SIZE);
									dir->Append(ReadData2(name, file, fe->InformationLength, raw));
								}
							} else {
								dir->Append(ReadData1(name, memory_reader(memory_block((uint8*)(fe + 1) + fe->LengthofExtendedAttributes, fe->InformationLength)).me(), fe->InformationLength, raw));
							}
							break;
					}
				}

				p	+= (sizeof(UDF_FileIdentifierDescriptor) + f->LengthofImplementationUse + f->LengthofFileIdentifier + 3) & ~3;
			}
		}

	} else {
		struct STACK {
			uint32		start, length;
			ISO_ptr<directory>	dir;
		} stack[256], *sp = stack;

		sp->start	= pvd.root.start;
		sp->length	= pvd.root.length;
		sp->dir		= pak;
		sp++;

		while (sp-- > stack) {
			ISO_ptr<directory> dir = sp->dir;
			file.seek(streamptr(sp->start) * CD_SECTOR_SIZE);

			for (int length = sp->length; length > 0;) {
				file.read(sector);

				CD_DIRENTRY2	*cd_dir = (CD_DIRENTRY2*)sector;
				while ((uint8*)cd_dir - sector < CD_SECTOR_SIZE && cd_dir->size) {
					char	name[128], c;
					int		i;
					for (i = 0; i < cd_dir->namelen && (c = cd_dir->name[i]) != ';'; name[i++] = c);
					name[i] = 0;

					if (cd_dir->flags & CDF_DIRECTORY) {
						if (cd_dir->name[0] > 1) {
							ISO_ptr<anything>	subdir(name);
							dir->Append(subdir);
							sp->dir		= subdir;
							sp->start	= cd_dir->start;
							sp->length	= cd_dir->length;
							sp++;
						}
					} else {
						streamptr	save = file.tell();
						file.seek(cd_dir->start * CD_SECTOR_SIZE);

						dir->Append(ReadData2(name, file, cd_dir->length, raw));
						file.seek(save);
					}
					cd_dir = (CD_DIRENTRY2*)((char*)cd_dir + cd_dir->size);
				}
				length -= CD_SECTOR_SIZE;
			}
		}
	}
}

//-------------------------------------
//	write
//-------------------------------------

static unsigned char	dir_extra[]	= {0, 0, 0, 0, 0x8D, 0x55, 0x58, 0x41, 0, 0, 0, 0, 0, 0};
static char				file_extra[]= {0, 0, 0, 0, 0x09, 0x11, 0x58, 0x41, 0, 0, 0, 0, 0, 0};

int CountDirectories(ISO_ptr<void> p) {
	int	n = 0;
	if (p.IsType<directory>()) {
		n = 1;
		directory *d = p;
		for (int i = 0, count = d->Count(); i < count; i++)
			n += CountDirectories((*d)[i]);
	}
	return n;
}

uint32 GetDirectoryLength(const directory *dir) {
	uint32		num_secs	= 1;
	uint32		length		= (sizeof(CD_DIRENTRY) + 1 + sizeof(file_extra)) * 2;	// for '.' & '..'

	for (int i = 0, count = dir->Count(); i < count; i++) {
		ISO_ptr<void>	p		= (*dir)[i];
		uint32			size	= (sizeof(CD_DIRENTRY) + strlen(p.ID().get_tag()) + sizeof(file_extra) + 1) & ~1;
		if (length + size > num_secs * CD_SECTOR_SIZE) {
			length	= num_secs * CD_SECTOR_SIZE;
			num_secs++;
		}
		length += size;
	}

	return length;
}

struct path_compare {
	bool operator()(const directory_info &a, const directory_info &b) {
		return a.dir.ID().get_tag() < b.dir.ID().get_tag();
	}
};

void ISO9660FileHandler::MakePathTable(directory_info *dirs, ISO_ptr<directory> root) {
	int	n				= 1;
	int	levelstart		= 0;

	dirs[0].dir			= root;
	dirs[0].parent		= NULL;
	dirs[0].record		= 0;

	do {
		int	end	= n;

		for (int i = levelstart; i < end; i++) {
			int			start	= n;
			directory	*parent = dirs[i].dir;

			for (int j = 0, count = parent->Count(); j < count; j++) {
				ISO_ptr<void>	p	= (*parent)[j];
				if (p.IsType<directory>()) {
					dirs[n].dir		= p;
					dirs[n].parent	= &dirs[i];
					dirs[n].record	= i + 1;
					n++;
				}
			}

			sort(dirs + start, dirs + n, path_compare());
		}

		levelstart	= end;
	} while (n > levelstart);
}

template<typename E> void ISO9660FileHandler::WritePathTable(ostream_ref file, directory_info *dirs, int n) {
	PATHTABLE<E>	p;
	for (int i = 0; i < n; i++) {
		const char	*name	= dirs[i].dir.ID().get_tag();
		int			len		= i ? int(strlen(name)) : 1;

		p.namelen		= len;
		p.EARlen		= 0;
		p.sectorstart	= dirs[i].start;
		p.record		= dirs[i].record;
		file.write(p);
		if (i)
			file.writebuff(name, len);
		else
			file.putc(0);
		if (len & 1)
			file.putc(0);
	}
	file.align(CD_SECTOR_SIZE, 0);
}

void ISO9660FileHandler::WriteFile(ostream_ref file, ISO_ptr<void> p) {
	if (TypeType(p.GetType()) == ISO::OPENARRAY && ((ISO::TypeOpenArray*)p.GetType())->subtype->GetType() == ISO::INT) {
		ISO_openarray<char>	*array	= (ISO_openarray<char>*)p;
		file.writebuff(*array, array->Count() * ((ISO::TypeOpenArray*)p.GetType())->subsize);
	} else {
		FileHandler *fh = FileHandler::Get(filename(p.ID().get_tag()).ext());
		if (!fh)
			fh = FileHandler::Get("ib");
		fh->Write(p, ostream_offset(file).me());
	}
}

bool ISO9660FileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO::Browser	vars		= ISO::root("variables");

	char			blank_sector[CD_SECTOR_SIZE];
	clear(blank_sector);
	bool			cd			= false;
	uint32			pathtable_start	= cd ? 18 : 257;
	int				nfolders	= CountDirectories(p);
	directory_info	*dirs		= new directory_info[nfolders];
	MakePathTable(dirs, p);

	uint32		pathtable_len	= 2;
	for (int i = 0; i < nfolders; i++)
		pathtable_len += sizeof(PATHTABLE<littleendian_types>) + ((strlen(dirs[i].dir.ID().get_tag()) + 1) & ~1);

	uint32		pathtable_numsectors	= (pathtable_len + CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE;
	uint32		dirstart				= pathtable_start + pathtable_numsectors * 4;
	uint32		filestart				= dirstart;

	for (int i = 0; i < nfolders; i++) {
		directory_info &dir	= dirs[i];
		dir.start			= filestart;
		dir.length			= GetDirectoryLength(dir.dir);
		filestart			+= (dir.length	+ CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE;
	}

	// primary volume descriptor
	PRIMARYVOLUMEDESCRIPTOR	pvd;
	pvd.header			= "\x01" "CD001" "\x01";
	pvd.sys_ident		= vars["SYSTEM"].GetString();
	pvd.vol_ident		= vars["VOLUME"].GetString();
	clear(pvd.pad0);
	pvd.num_sectors		= 0;//end + 150;
	clear(pvd.pad1);
	pvd.set_size		= 1;
	pvd.seq_no			= 1;
	pvd.sector_size		= CD_SECTOR_SIZE;
	pvd.pathtable_len	= pathtable_len;

	pvd.le_pathtable[0]	= pathtable_start + pathtable_numsectors * 0;
	pvd.le_pathtable[1]	= pathtable_start + pathtable_numsectors * 1;
	pvd.be_pathtable[0]	= pathtable_start + pathtable_numsectors * 2;
	pvd.be_pathtable[1]	= pathtable_start + pathtable_numsectors * 3;

	pvd.root.size		= sizeof(CD_DIRENTRY);
	pvd.root.EARlen		= 0;
	pvd.root.start		= dirstart;
	pvd.root.length		= GetDirectoryLength(p);

#ifdef PLAT_PC
	DateTime	now		= DateTime::Now();
	Date		day(now.Day());
	TimeOfDay	time(now.TimeOfDay());

	pvd.root.year		= day.year - 1900;
	pvd.root.month		= day.month;
	pvd.root.day		= day.day;
	pvd.root.hour		= time.Hour();
	pvd.root.minute		= time.Min();
	pvd.root.second		= uint8(time.Sec());
	pvd.root.GMToffset	= uint8(DateTime::TimeZone() / Duration::Mins(15));
	pvd.create.Set(now, pvd.root.GMToffset);
#endif

	pvd.root.flags		= 2;
	pvd.root.unitsize	= 0;
	pvd.root.gapsize	= 0;
	pvd.root.seq_no		= 1;
	pvd.root.namelen	= 1;
	pvd.root_name[0]	= 0;

	pvd.set_ident		= vars["VOLUMESET"].GetString();
	pvd.pub_ident		= vars["PUBLISHER"].GetString();
	pvd.prep_ident		= vars["PREPARER"].GetString();
	pvd.app_ident		= vars["APPLICATION"].GetString();
	pvd.copyright_file	= vars["COPYRIGHT"].GetString();
	pvd.abstract_file	= NULL;
	pvd.bibliographical_file = NULL;

	pvd.modify.Blank();
	pvd.expire.Blank();
	pvd.effective.Blank();
	pvd.pad2[0]		= 1;
	pvd.pad2[1]		= 0;

	for (int i = 0; i < 16; i++)
		file.write(blank_sector);

	file.write(pvd);
	file.align(0x400, 0);
	file.write("CD-XA001");
	file.align(CD_SECTOR_SIZE, 0);

	// volume descriptor set terminator
	file.write("\xff" "CD001" "\x01");
	file.align(CD_SECTOR_SIZE, 0);

	// path tables
	while (file.tell() < pathtable_start * CD_SECTOR_SIZE)
		file.write(blank_sector);

	WritePathTable<littleendian_types>	(file, dirs, nfolders);
	WritePathTable<littleendian_types>	(file, dirs, nfolders);
	WritePathTable<bigendian_types>		(file, dirs, nfolders);
	WritePathTable<bigendian_types>		(file, dirs, nfolders);

	for (int f = 0; f < nfolders; f++) {
		directory_info	&dir	= dirs[f];
		directory_info	*child	= &dir;
		CD_DIRENTRY		*cde	= new CD_DIRENTRY[dir.dir->Count()];

		for (int i = 0, count = dir.dir->Count(); i < count; i++) {
			ISO_ptr<void>	p		= (*dir.dir)[i];
			CD_DIRENTRY		&c		= cde[i];

			c				= pvd.root;
			if (p.IsType<directory>()) {
				child++;
				c.start		= child->start;
				c.length	= child->length;
				c.flags		= 2;
			} else {
				file.seek(streamptr(filestart) * CD_SECTOR_SIZE);
				WriteFile(file, p);
				c.start		= filestart;
				c.length	= uint32(file.tell() - streamptr(filestart) * CD_SECTOR_SIZE);
				c.flags		= 0;
				filestart	+= (c.length	+ CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE;
			}

			c.size			= (sizeof(CD_DIRENTRY) + c.namelen + sizeof(file_extra) + 1) & ~1;
		}

		file.seek(streamptr(dir.start) * CD_SECTOR_SIZE);

		CD_DIRENTRY		c;
		c			= pvd.root;
		c.size		= sizeof(CD_DIRENTRY) + 1 + sizeof(file_extra);
		c.start		= dir.start;
		c.length	= dir.length;
		c.flags		= 2;
		c.namelen	= 1;

		file.write(c);
		file.putc(0);
		file.write(dir_extra);

		if (directory_info *parent = dir.parent) {
			c.start		= parent->start;
			c.length	= parent->length;
		}
		c.flags		= 2;
		c.namelen	= 1;

		file.write(c);
		file.putc(1);
		file.write(dir_extra);

		uint32	size	= (sizeof(CD_DIRENTRY) + 1 + sizeof(file_extra)) * 2;	// for '.' & '..'
		for (int i = 0, count = dir.dir->Count(); i < count; i++) {
			ISO_ptr<void>	p		= (*dir.dir)[i];
			CD_DIRENTRY		&c		= cde[i];
			bool			isdir	= p.IsType<directory>();
			size_t			namelen	= strlen(p.ID().get_tag());

			c.namelen		= uint8(isdir ? namelen : namelen + 2);
			c.size			= (sizeof(CD_DIRENTRY) + c.namelen + sizeof(file_extra) + 1) & ~1;
			if (size + c.size > CD_SECTOR_SIZE) {
				file.align(CD_SECTOR_SIZE, 0);
				size	= 0;
			}
			size	+= c.size;

			file.write(c);
			file.writebuff(p.ID().get_tag(), namelen);
			if (!isdir)
				file.writebuff(";1", 2);
			if (!(namelen & 1))
				file.putc(0);

			if (isdir)
				file.write(dir_extra);
			else
				file.write(file_extra);
		}
		delete[] cde;

	}

	delete[] dirs;

	pvd.num_sectors	= filestart + 150;
	file.seek(16 * CD_SECTOR_SIZE);
	file.write(pvd);
	file.seek(streamptr(filestart) * CD_SECTOR_SIZE);
	for (int i = 0; i < 150; i++)
		file.write(blank_sector);
	return true;
}
