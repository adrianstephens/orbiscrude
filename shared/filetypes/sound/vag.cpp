#include "vag.h"
#include "wav.h"
#include "samplefile.h"
#include "iso/iso_convert.h"
#include "extra/filters.h"

using namespace iso;

double VAGBLOCK::f[8][2] = {
	{    0.0,			0.0			},
	{   60.0 / 64.0,    0.0			},
	{  115.0 / 64.0,  -52.0 / 64.0	},
	{   98.0 / 64.0,  -55.0 / 64.0	},
	{  122.0 / 64.0,  -60.0 / 64.0	},
	{    0.0,			0.0			},
	{    0.0,			0.0			},
	{    0.0,			0.0			},
};

int16 *VAGBLOCK::Unpack(iso::int16 *dest, double &_s1, double &_s2, int chans) {
	int		predictor	= (pack_info >> 4) & 7,
			shift		= pack_info & 0xf;

	//ISO_ASSERT(predictor < 5 && shift <= 12);

	double	p0			= f[predictor][0],
			p1			= f[predictor][1],
			s1			= _s1,
			s2			= _s2;

	for (int i = 0; i < 14; i++) {
		int		d	= packed[i];
		s2		= ((int16)((d & 0x0f) << 12) >> shift) + s1 * p0 + s2 * p1;
		*dest	= (int)(s2 + 0.5);
		dest += chans;

		s1		= ((int16)((d & 0xf0) << 8) >> shift) + s2 * p0 + s1 * p1;
		*dest	= (int)(s1 + 0.5);
		dest += chans;
	}

	_s1 = s1;
	_s2 = s2;

	return dest;
}

void VAGBLOCK::Pack(iso::int16 * samples, int _flags, double &_s1, double &_s2) {
	double	s1			= _s1, s2 = _s2;
	int		_pack_info	= FindPredict(samples, s1, s2);
	int		predictor	= _pack_info >> 4,
			shift		= _pack_info & 0xf;
	double	p0			= f[predictor][0],
			p1			= f[predictor][1];

	pack_info	= _pack_info;
	flags		= _flags == VAG_1_SHOT_END	? 1	// (or maybe 7?)
				: _flags == VAG_LOOP_START	? 6
				: _flags == VAG_LOOP_BODY	? 2
				: _flags == VAG_LOOP_END	? 3
				: 0;

	for (int i = 0; i < 28; i++ ) {
		double	s0 = samples[i];
		if (s0 > 30719.0)
			s0 = 30719.0;
		if (s0 < -30720.0)
			s0 = -30720.0;
		double	ds = (s0 - s1 * p0 - s2 * p1) * (double)(1 << shift);
		int		di = ((int)ds + 0x800) & 0xfffff000;

		if (di > +0x7000)
			di = +0x7000;
		if (di < -0x8000)
			di = -0x8000;

		packed[i>>1] = i & 1 ? (packed[i>>1] | ((di >> 8) & 0xf0)) : (di >> 12) & 0x0f;

		s0 = (double)(di >> shift) + s1 * p0 + s2 * p1;
		s2 = s1;
		s1 = s0;
	}

	_s1 = s1;
	_s2 = s2;
}

int VAGBLOCK::FindPredict(iso::int16 *samples, double s1, double s2) {
	double	min			= 1e10;
	int		predictor	= 0;

	for (int i = 0; i < 5; i++ ) {
		double	p0	= f[i][0],
				p1	= f[i][1],
				max	= 0.0;
		for (int j = 0; j < 28; j ++ ) {
			double	s0 = samples[j];					// s[t-0]
			if (s0 > 30719.0)
				s0 = 30719.0;
			if (s0 < -30720.0)
				s0 = -30720.0;
			double	ds = s0 - s1 * p0 - s2 * p1;

			if (ds < 0)
				ds = -ds;
			if (ds > max)
				max = ds;
			s2 = s1;									// new s[t-2]
			s1 = s0;									// new s[t-1]
		}

		if (max < min) {
			min			= max;
			predictor	= i;
		}
/*
		if (min <= 7) {
			predictor	= 0;
			break;
		}
*/
	}

	int	min2		= (int)min;
	int	shift_mask	= 0x4000;
	int	shift		= 0;

	while (shift < 12) {
		if (shift_mask  & (min2 + (shift_mask >> 3)))
			break;
		shift++;
		shift_mask = shift_mask >> 1;
	}

	return shift | (predictor << 4);
}

//-----------------------------------------------------------------------------

class VAGFileHandler : public SampleFileHandler, public WAVformat {
	enum { ISO_WAVE_FORMAT_VAG = 0xFFFB };
	const char*			GetExt() override { return "vag";	}
	int					Check(istream_ref file) override {
		file.seek(0);
		VAGheader		header;
		return file.read(header) && header.validate() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	bool operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm);
	VAGFileHandler() : WAVformat(this) {}
} vag;

ISO_ptr<void> VAGFileHandler::Read(tag id, istream_ref file) {
	VAGheader		header;
	if (!file.read(header) || !header.validate())
		return ISO_NULL;

	int		size	= (header.size / 16 - 1) * VAG_BLOCKSIZE;
	int		pos		= 0;
	uint32	loopstart, loopend = 0;

	ISO_ptr<sample> sm(id);
	sm->Create(size, 1, 16);
	sm->SetFrequency(header.fs);

	VAGBLOCK2	block;
	int16		*dest	= sm->Samples();

	file.seek_cur(16);//skip blank block

	while (pos < size) {
		block.Read(file);

		if (block.Flags() == 7)
			break;

		dest = block.Unpack(dest);

		pos += VAG_BLOCKSIZE;

		if (block.Flags() == 6)
			loopstart = pos - VAG_BLOCKSIZE;
		else if (block.Flags() == 3)
			loopend		= pos;
		else if (block.Flags() == 1)
			break;
	}
/*
	if (pos != size) {
		sm.frames	= pos;
		sm.CreateSamples();
	}
	if (loopend) {
		sm.Mark(0).Set(0, loopstart, loopend - loopstart);
		sm.SetFlag(SMF_LOOP);
	}
*/
	return sm;
}

void WriteVAGData(sample *sm, ostream_ref file) {
	bool			looping		= !!(sm->Flags() & sample::LOOP);
	unsigned long	bsframe		= 0;
	int				rest		= sm->Length();
	int16			*in			= sm->Samples();
	VAGBLOCK2		out;
	uint32			loopstart	= 0, loopend = rest;
	int				channels	= sm->Channels();

	out.Write(file);		// block of all zeroes to start

	do {
		int16	wave[VAG_BLOCKSIZE];
		int16	flag;
		int		n = rest < VAG_BLOCKSIZE ? rest : VAG_BLOCKSIZE;
		int		i;

		for (int c = 0; c < channels; c++) {
			int16	*in2	= in + c;
			for (i = 0; i < n; i++, in2 += channels)
				wave[i] = *in2;
			while (i < VAG_BLOCKSIZE)
				wave[i++] = 0;

			if (looping) {
				if (loopstart <= bsframe && loopstart + VAG_BLOCKSIZE > bsframe)
					flag = VAG_LOOP_START;
				else
					flag = VAG_LOOP_BODY;
				if (loopend > bsframe && loopend <= bsframe + VAG_BLOCKSIZE) {
					flag = VAG_LOOP_END;
					rest = 0;
				}
			} else
				flag = rest <= VAG_BLOCKSIZE ? VAG_1_SHOT_END : VAG_1_SHOT;

			out.Pack(wave, flag).Write(file);
		}

		in		+= VAG_BLOCKSIZE * channels;
		rest	-= VAG_BLOCKSIZE;
		bsframe	+= VAG_BLOCKSIZE;
	} while (rest > 0);

	if (!looping)
		out.Final().Write(file);
}

bool VAGFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p)) {
//		sm->MakeMono().MakeBits(16);
		bool			looping		= !!(sm->Flags() & sample::LOOP);
		if (looping && (sm->Length() % VAG_BLOCKSIZE)) {
			int		channels	= sm->Channels();
			int		bits		= sm->Bits();
			int		oldlength	= sm->Length();
			int		newlength	= (sm->Length() + VAG_BLOCKSIZE / 2) / VAG_BLOCKSIZE * VAG_BLOCKSIZE;
			float	oldfreq		= sm->Frequency();
			float	newfreq		= newlength * oldfreq / oldlength;

			ISO_ptr<sample> sm2;
			sm2.Create(sm.ID())->Create(newlength, channels, bits);
			sm2->SetFrequency(newfreq);
			sm2->flags = sm->flags;

			memset(sm2->Samples(), 0, sm2->BytesPerSample() * newlength);

			if (channels == 1) {
				resample(sm2->Samples(), sm->Samples(), newlength, oldlength);

			} else for (int i = 0; i < channels; i++) {
				resample(
					strided(sm2->Samples() + i, sizeof(int16) *channels),
					strided(sm->Samples()  + i, sizeof(int16) *channels),
					newlength, oldlength
				);
			}
			sm = sm2;
		}
/*
		if (looping) {
			sm->SortMarks();
			looping	= false;
			for (int i = 0; i < sm->num_marks; i++) {
				if (sm->Mark(i).length) {
					loopstart	= sm->Mark(i).frame;
					loopend		= loopstart + sm->Mark(i).length;
					looping		= true;
				}
			}

		}
*/
//		if (!(flags & SMWF_NOHEADER)) {
			VAGheader		header;
			clear(header);
			header.format	= 'VAGp';
			header.ver		= 0x20;
			header.size		= ((sm->Length() + VAG_BLOCKSIZE-1) / VAG_BLOCKSIZE + 2 - (int)looping) * 16;
			header.fs		= sm->Frequency();
			if (tag2 id = p.ID())
				strcpy(header.name, id.get_tag());
			else
				strcpy(header.name, "Isopod Made");
			file.write(header);
//		}

		WriteVAGData(sm, file);
		return true;
	}
	return false;
}

bool VAGFileHandler::operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm) {
	if (fmt->wFormatTag != ISO_WAVE_FORMAT_VAG)
		return false;

	if (ISO_ptr<sample> p = Read(none, chunk)) {
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	XVAG
//-----------------------------------------------------------------------------

struct XVAG_chunk {
	uint32		id;
	uint32		length;
	uint32		get_length(bool bigendian) const { return bigendian ? swap_endian(length) : length; }
};

struct XVAG_CPan {
	uint32	numChannels;
	uint32	mode;
	uint32	speakers;
	float	sphereSize;
};
struct XVAG_CChanPan {
	uint32	azimuth;
	int32	elevation;
	uint32	azimtuhSpread;
	uint32	elevationSpread;
};
struct XVAG_Header {
	enum {bigendian = 1};
	uint8	flags;				// flags concerning this file, including endianness
	uint8	targetPlatform;	// id of the targetted platform, informational only since targets can be compatible
	uint8	fileVersionMajor;	// file spec major version number	MM.mm
	uint8	fileVersionMinor;	// file spec minor version number	MM.mm
	uint32	nextHdr;			// offset from beginning of this header to to next header in file, 0 if no more headers
	uint32	future1;			// reserved for future expansion
	uint32	future2;
	uint32	future3;
	uint32	future4;
};
struct XVAG_FmatChunk {
	enum {
		PCM8	= 1,
		PCM16	= 2,
		PCM24	= 3,
		PCM32	= 4,
		FLT32	= 5,
		VAG		= 6,
		VAG2	= 7,
		MP3		= 8,
	};
	enum {
		MAX_CHANNELS = 8,
	};
	uint32	uChannels;			// num channels
	uint32	uEncoding;			// what kind of data is here?
	uint32	uSampleFrames;		// total number of decoded sample frames present after decoding
	uint32	uValidSampleFrames; // total number of decoded sample frames before encoding
	uint32	uInterleave;		// number of granular units in each channel block
	uint32	uRate;				// sample rate in samples per second
	uint32	uSize;				// total size of wavesample section in bytes
};
struct XVAG_PmodChunk {
	uint16	uFlags;				// Various bit flags for controlling effects.
	int8	sVol;				// Volume setting (0 to 127).
	int8	sPriority;			// Voice allocation priority (0 to 127).
	int16	sPan;				// Pan azimuth (0 to 359).
	int8	sRandVolMin;		// Random volume minimum (-127 to 127).
	int8	sRandVolMax;		// Random volume maximum (-127 to 127).
	int16	sRandPanMin;		// Random pan minimum (-359 to 359).
	int16	sRandPanMax;		// Random pan maximum (-359 to 359).
	int16	sRandDetuneMin;		// Random detune minimum (-6096 to 6096).
	int16	sRandDetuneMax;		// Random detune maximum (-6096 to 6096).
};

inline void	endian(XVAG_FmatChunk &t, bool bigendian) {
	if (bigendian)
		swap_endian_inplace(t.uChannels, t.uEncoding, t.uInterleave, t.uRate, t.uSampleFrames, t.uSize, t.uValidSampleFrames);
}
inline void	endian(XVAG_CPan &t, bool bigendian) {
	if (bigendian)
		swap_endian_inplace(t.numChannels, t.mode, t.speakers);
};

class XVAGFileHandler : public FileHandler {
	enum { ISO_WAVE_FORMAT_VAG = 0xFFFB };
	const char*			GetExt() override { return "xvag";	}
	int					Check(istream_ref file) override { file.seek(0); return file.get<XVAG_chunk>().id == "XVAG"_u32 ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
//	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	XVAGFileHandler() {}
} xvag;

ISO_ptr<void> XVAGFileHandler::Read(tag id, istream_ref file) {
	XVAG_CChanPan	XVAG_CPanChannelMap[XVAG_FmatChunk::MAX_CHANNELS];
	XVAG_FmatChunk	format;
	XVAG_chunk		chunk;
	streamptr		next	= 0;

	ISO_ptr<sample> sm(id);

	do {
		file.seek(next);
		if (!file.read(chunk) || chunk.id != "XVAG"_u32)
			return ISO_NULL;

		XVAG_Header		header;
		file.read(header);

		bool			bigendian	= !!(header.flags & XVAG_Header::bigendian);
		streamptr		next		= header.nextHdr ? file.tell() + endian(header.nextHdr, bigendian) - sizeof(header) : 0;
		clear(format);

		for (bool stop = false; !stop && file.read(chunk); ) {
			uint32		len = chunk.get_length(bigendian);
			streamptr	end = file.tell() + align(len, 4);
			switch (chunk.id) {
				case "0000"_u32:
					stop = true;
					break;

				case "fmat"_u32: {
					file.read(format);
					endian(format, bigendian);
					break;
				}
				case "pmod"_u32: {
					XVAG_PmodChunk pmod;
					file.read(pmod);
					break;
				}
				case "cpan"_u32: {
					malloc_block	block(file, len - 8);
					XVAG_CPan		*cpan = block;
					for (int i = 0; i < cpan->numChannels; i++) {
						XVAG_CPanChannelMap[i].elevation		= endian(XVAG_CPanChannelMap[i].elevation, bigendian);
						XVAG_CPanChannelMap[i].azimuth			= endian(XVAG_CPanChannelMap[i].azimuth, bigendian);
						XVAG_CPanChannelMap[i].azimtuhSpread	= endian(XVAG_CPanChannelMap[i].azimtuhSpread, bigendian);
						XVAG_CPanChannelMap[i].elevationSpread	= endian(XVAG_CPanChannelMap[i].elevationSpread, bigendian);
					}
					break;
				}
				case "cue "_u32:
				case "smrk"_u32:
				default:
					break;
			}
			file.seek(end);
		}

		uint32	frames	= format.uValidSampleFrames;
		uint32	chans	= format.uChannels;
		int16	*dest	= sm->Create(frames, chans, 16);
		sm->SetFrequency(format.uRate);

		VAGBLOCK2	block;
		for (uint32 pos = 0; pos < frames; pos += VAG_BLOCKSIZE) {
			uint32	n = min(frames - pos, VAG_BLOCKSIZE);

			for (int i = 0; i < chans; i++) {
				block.Read(file);
				block.Unpack(dest + i, chans);
			}
		}

	} while (next);

	return sm;
}
