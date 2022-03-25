#include "bitmap.h"
#include "iso/iso_files.h"

//-----------------------------------------------------------------------------
//	DeluxeAnimator bitmap animations
//-----------------------------------------------------------------------------

using namespace iso;

bool PlayDeltaFrame(bitmap &bm, char *data, int size);

#pragma pack(1)
struct lpfHdr {
	uint32	id;					// ID for LargePageFile == MakeID('L','P','F',' ').
	uint16	maxLps;				// 256 FOR NOW.  max # largePages allowed in this file.
	uint16	nLps;				// # largePages currently in this file.  0..maxLps. lps #d from 0.  NOTE: In RAM, we don't keep this field up to date.
	uint32	nRecords;			// # records currently in this file.  65535 is  current limit.  Allowing one for last-to-first delta, that restricts
								// to 65534 frames of lo-res animation.  Records #d from 0.  NOTE: In RAM, we don't keep this field up to date.
	uint16	maxRecsPerLp;		// 256 FOR NOW.  # records permitted in an lp. So reader knows table size to allocate.
	uint16	lpfTableOffset;		// 1280 FOR NOW.  Absolute Seek position of lpfTable. This allows content-specific header to be variable-size in the future.
	uint32	contentType;		// for ANIM == MakeID('A','N','I','M').  All following info is specific to ANIM format.
	uint16	width;				// in pixels.
	uint16	height;				// in pixels.
	uint8	variant;			// 0==ANIM.
	uint8	version;			// 0== cycles stored for 18x/sec timer. 1== cycles stored for 70x/sec timer.
	uint8	hasLastDelta;		// 1==Record(s) representing final frame are a delta from last-to-first frame.  nFrames == (nRecords/recordsPerFrame)-hasLastDelta.
	uint8	lastDeltaValid;		// So needn't actually remove delta. (When hasLastDelta==1)  0==there is a last delta, but it hasn't been updated to match the
								//current first&last frames, so it should be ignored. This is done so don't have to physically remove the delta from the file when the first frame changes. */
	uint8	pixelType;			// ==0 for 256-color.
	uint8	highestBBComp;		// 1 (RunSkipDump) FOR NOW. highest #d "Record Bitmap.Body.compressionType". So reader can determine whether it can handle this Anim.
	uint8	otherRecordsPerFrame;	// 0 FOR NOW.
	uint8	bitmapRecordsPerFrame;	// ==1 for 320x200,256-color.
	uint8	recordTypes[32];		// Not yet implemented.
	uint32	nFrames;			// In case future version adds other records at end of file, we still know how many frames.
								// NOTE: DOES include last-to-first delta when present.
								// NOTE: In RAM, we don't keep this field up to date.
	uint16	framesPerSecond;	// # frames to play per second.
	uint16	pad2[29];
};	/* total is 128 words == 256 bytes. */
#pragma pack()

struct LP_DESCRIPTOR {
	uint16	baseRecord;	// First record in lp.
	uint16	nRecords;	// # records in lp.
	   /* bit 15 of "nRecords" == "has continuation from previous lp".
	    * bit 14 of "nRecords" == "final record continues on next lp".
	    * File format thus permits 16383 records/lp.
		* Only MAX_RECORDS_PER_LP is currently supported by the code,
		* to minimize table size in DGROUP in DP Animation.
	    * TBD: we're not handling non-zero bits 14&15.
	    */
	uint16	nBytes;	// # bytes of contents, excluding header. Gap of "64KB - nBytesUsed - headerSize" at end of lp.
					// headerSize == "8 + 2*nRecords". Header is followed by record #baseRecord.
};

struct LPageHeaderFixed {
	LP_DESCRIPTOR lpd;			// Duplicate of lpfTable[CUR_LP].  NOT relied on, just here to help scavenge damaged files.
	uint16	nByteContinuation;	// ==0.  FUTURE: allows record from previous lp to extend on to this lp, so don't waste file space.
};

struct LPF_COLOUR {
	uint8	b, g, r, a;
	operator ISO_rgba() { return ISO_rgba(r,g,b,255);}
};

#define LARGE_PAGE_SIZE					0x10000
#define MAX_RECORDS_PER_LP				256	// TBD: why restrict, other than RAM usage?
#define MAX_COLORS						256
#define PALETTE_SIZE					(MAX_COLORS * sizeof(LPF_COLOUR))
#define LPF_HEADER_HEAD_SIZE_IN_FILE	256	// First few fields at start of a Large Page File
#define MAX_LARGE_PAGE					256
#define SIZEOF_LPF_TABLE_IN_FILE		(sizeof(LP_DESCRIPTOR) * MAX_LARGE_PAGE)
#define LPF_HEADER_SIZE_IN_FILE			(LPF_HEADER_HEAD_SIZE_IN_FILE + PALETTE_SIZE + SIZEOF_LPF_TABLE_IN_FILE ) // Everything in Large Page File before the first lp. */

#define LP_HEADER_SIZE(nRecords)		(sizeof(LPageHeaderFixed) + (nRecords) * sizeof(LP_TABLE_ELEMENT))


#define FASTEST_RATE			70		// Fastest rate anim can be played at.
#define MAXNCYCS				16

struct Range {
	uint16	count;
	uint16	rate;
	uint16	flags;
	uint8	low, high;
};

class LPAGE {
	LPageHeaderFixed	header;
	uint16				lpTable[MAX_RECORDS_PER_LP];
	char				data[LARGE_PAGE_SIZE];

public:
	LPAGE() { header.lpd.nRecords = 0;}
	void	Read(istream_ref file, LP_DESCRIPTOR &lpd);
	bool	Contains(int nRecord) { return nRecord >= header.lpd.baseRecord && nRecord < header.lpd.baseRecord + header.lpd.nRecords;}
	char*	GetRecord(int nRecord, int *size);
};

void LPAGE::Read(istream_ref file, LP_DESCRIPTOR &lpd) {
	file.read(header);
	file.readbuff(lpTable, sizeof(uint16) * lpd.nRecords);
	file.readbuff(data, lpd.nBytes);
	header.lpd = lpd;
}

char* LPAGE::GetRecord(int nRecord, int *size) {
	char	*p		= data;
	uint16	*sizes	= lpTable;
	nRecord -= header.lpd.baseRecord;
	while (nRecord--)
		p += *sizes++;

	*size = *sizes;
	return p;
}

class LPF {
	lpfHdr			header;
	istream_ref			file;
	Range			cycles[MAXNCYCS];
	LP_DESCRIPTOR	lpfTable[MAX_LARGE_PAGE];
	LPF_COLOUR		palette[MAX_COLORS];
	LPAGE				lp;

	void		SeekLP(int nLp) { file.seek(nLp * LARGE_PAGE_SIZE + LPF_HEADER_SIZE_IN_FILE);}

public:
	LPF(istream_ref _file);
	bool		Valid()		{ return header.id == ' FPL' && header.contentType == 'MINA';}
	int			NumFrames()	{ return header.nRecords - header.hasLastDelta;}
	int			Width()		{ return header.width;}
	int			Height()	{ return header.height;}
	int			FPS()		{ return header.framesPerSecond; }

	void		ReadLP(int nLp);

	int			FindLPForRecord(int nRecord);
	void		ReadLPForRecord(int nRecord);

	bool		ReadFrame(bitmap &bm, int nFrame);
	void		CreateBitmap(bitmap &bm);
};

LPF::LPF(istream_ref _file) : file(_file) {
	file.read(header);
	file.read(cycles);
//	file.seek(LPF_HEADER_HEAD_SIZE_IN_FILE);
	file.read(palette);
	file.read(lpfTable);
}

void LPF::ReadLP(int nLp) {
	SeekLP(nLp);
	lp.Read(file, lpfTable[nLp]);
}

int LPF::FindLPForRecord(int nRecord) {
	for (int nLp = 0; nLp < header.nLps; nLp++) {
		int baseRecord = lpfTable[nLp].baseRecord;
		if (baseRecord <= nRecord && nRecord < baseRecord + lpfTable[nLp].nRecords)
			return nLp;
	}
	return -1;
}

void LPF::ReadLPForRecord(int nRecord) {
	ReadLP(FindLPForRecord(nRecord));
}

bool LPF::ReadFrame(bitmap &bm, int nFrame) {
	int nRecord = nFrame;
	if (!lp.Contains(nRecord))
		ReadLPForRecord(nRecord);

	int		size;
	char*	data = lp.GetRecord(nRecord, &size);

	if (size) {
		if (*data != 'B')
			return false;
		int	flags	= data[1];
		int	extra	= ((uint16*)data)[1];
		int	extra2	= flags & 4 ? 4 + ((extra + 1) & ~1) : 2;
		data += extra2;
		size -= extra2;
		PlayDeltaFrame(bm, data, size);
	}
	return true;
}

void LPF::CreateBitmap(bitmap &bm) {
	bm.Create(header.width, header.height);
	bm.CreateClut(MAX_COLORS);
	for (int i = 0; i < MAX_COLORS; i++)
		bm.Clut(i) = palette[i];
}


bool PlayDeltaFrame(bitmap &bm, char *data, int size) {
	ISO_rgba	*dest	= bm.ScanLine(0);
	int			type	= *(uint16*)data;
	size -= 2;
	data += 2;

	if (type == 0) {							// --- Raw
		int	n = bm.Width() * bm.Height();
		if (n != size)
			return false;
		while (size--)
			*dest++ = *data++;
		return true;

	} else if (type == 1) {						// --- RunSkipDump
		if (size == 0)
			return true;						// empty delta == no change

		for(;;) {
			if (size < 3)
				return false;					// "RunSkipDump body elided stop code");

			char	cnt = *data++;
			if (cnt > 0) {						// dump
				size -= cnt + 1;
				while (cnt--)
					*dest++ = *data++;

			} else if (cnt == 0) {
				int		wordCnt = (uint8)*data++;
				uint8	pixel	= *data++;
				size	-= 3;
				while (wordCnt--)
					*dest++ = pixel;

			} else if (cnt -= (char)0x80) {		// shortSkip
				size--;
				dest += cnt;

			} else {							//longOp
				uint16 wordCnt = *(uint16*)data;
				data += 2;
				if (wordCnt == 0)
					break;

				if (!(wordCnt & 0x8000)) {
					size -= 3;					// longSkip.
					dest += wordCnt;

				} else {
					wordCnt -= 0x8000;
					if (wordCnt < 0x4000) {		// longDump.
						size -= 3 + wordCnt;
						while (wordCnt--)
							*dest++ = *data++;

					} else {					// longRun:
						uint8	pixel = *data++;
						wordCnt -= 0x4000;
						size	-= 4;
						while (wordCnt--)
							*dest++ = pixel;
					}
				}
			}
		}
		return true;
	} else {
		return false;
	}
}


class LPFFileHandler : FileHandler {
	const char	*GetExt()			{ return "anm";						}
	const char	*GetDescription()	{ return "Deluxe Animation File";	}

	ISO_ptr<void>	Read(tag id, istream_ref file)
	{
		LPF		lpf(file);
		if (!lpf.Valid())
			return ISO_NULL;

		ISO_ptr<bitmap_anim>	anim(id);

		for (int i = 0; i < lpf.NumFrames(); i++) {
			bitmap_frame	&frame = anim->Append();
			if (i) {
				frame.a = Duplicate((*anim)[i - 1].a);
			} else {
				lpf.CreateBitmap(*frame.a.Create());
			}
			frame.b = 1.f / lpf.FPS();
			if (!lpf.ReadFrame(*frame.a, i))
				return ISO_NULL;
		}
		return anim;
	}
} lpf;
