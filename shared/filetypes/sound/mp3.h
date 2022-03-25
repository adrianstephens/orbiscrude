#ifndef MP3_H

#include "base/strings.h"
#include "base/array.h"
#include "base/vector.h"
#include "codec/vlc.h"

namespace iso {

#define SBLIMIT	32
#define SSLIMIT 18

struct rawbuffer : memory_block {
	rawbuffer()		{}
	~rawbuffer()	{ if (p) aligned_free(p); }

	rawbuffer&	create(size_t _size, size_t a) {
		if (_size != size()) {
			clear_contents();
			memory_block::operator=(memory_block(aligned_alloc(_size, a), _size));
		}
		return *this;
	}
};

struct outbuffer : rawbuffer {
	size_t		fill;
	outbuffer() : fill(0)			{}
	void		*end()				{ return (char*)p + fill; }
	void		*add(size_t n)		{ void *t = (char*)p + fill; fill += n; return t; }
};

struct RVA {
	float	gain, peak;
	RVA() : gain(0), peak(0)				{}
	void	set(float _gain, float _peak)	{ gain = _gain; peak = _peak; }
};

struct RVAsetting : RVA {
	enum LEVEL {NONE, LAME, COMMENT, EXTRA, RVA2};
	LEVEL	level;

	void	set(LEVEL _level, float _gain, float _peak) {
		if (level <= _level) {
			level = _level;
			RVA::set(_gain, _peak);
		}
	}
	RVAsetting() : level(NONE)	{}
};

struct LAME {
	uint8					ver;
	enum {CBR, VBR, ABR}	vbr:8;
	uint8					lpf;
	uint8					enc_flags;
	uint8					abr_rate;
    int						vbr_scale;
	streamptr				track_length;
	streamptr				track_frames;
	streamptr				skipbegin;
	streamptr				skipend;
	rawbuffer				xing_toc;	// The seek TOC from Xing header.

	LAME()	: ver(0), vbr(CBR), abr_rate(0), vbr_scale(0), track_length(0)	{}
	bool	read(const void *p, size_t size, size_t offset, RVAsetting *rva);
	int64	lookup_frame(int64 &frame, int64 filesize);
};

struct ID3v1 {
	enum GENRE {
		Blues, Classic_Rock, Country, Dance, Disco, Funk, Grunge, Hip_Hop, Jazz, Metal,
		New_Age, Oldies, Other, Pop, R_B, Rap, Reggae, Rock, Techno, Industrial,
		Alternative, Ska, Death_Metal, Pranks, Soundtrack, Euro_Techno, Ambient, Trip_Hop, Vocal, Jazz_Funk,
		Fusion, Trance, Classical, Instrumental, Acid, House, Game, Sound_Clip, Gospel, Noise,
		Alternative_Rock, Bass, Soul, Punk, Space, Meditative, Instrumental_Pop, Instrumental_Rock, Ethnic, Gothic,
		Darkwave, Techno_Industrial, Electronic, Pop_Folk, Eurodance, Dream, Southern_Rock, Comedy, Cult, Gangsta,
		Top_40, Christian_Rap, Pop_Funk, Jungle, Native_US, Cabaret, New_Wave, Psychadelic, Rave, Showtunes,
		Trailer, Lo_Fi, Tribal, Acid_Punk, Acid_Jazz, Polka, Retro, Musical, Rock_Roll, Hard_Rock,
		Folk, Folk_Rock, National_Folk, Swing, Fast_Fusion, Bebob, Latin, Revival, Celtic, Bluegrass,
		Avantgarde, Gothic_Rock, Progressive_Rock, Psychedelic_Rock, Symphonic_Rock, Slow_Rock, Big_Band, Chorus, Easy_Listening, Acoustic,
		Humour, Speech, Chanson, Opera, Chamber_Music, Sonata, Symphony, Booty_Bass, Primus, Porn_Groove,
		Satire, Slow_Jam, Club, Tango, Samba, Folklore, Ballad, Power_Ballad, Rhythmic_Soul, Freestyle,
		Duet, Punk_Rock, Drum_Solo, Acapella, Euro_House, Dance_Hall, Goa, Drum_Bass, Club_House, Hardcore,
		Terror, Indie, BritPop, Negerpunk, Polsk_Punk, Beat, Christian_Gangsta_Rap, Heavy_Metal, Black_Metal, Crossover,
		Contemporary_Christian, Christian_Rock, Merengue, Salsa, Thrash_Metal, Anime, JPop, Synthpop,
		undef,
	};
	static const char *genre_names[];

	char	tag[3];		// Always the string "TAG", the classic intro
	char	title[30];
	char	artist[30];
	char	album[30];
	char	year[4];
	char	comment[30];
	GENRE	genre:8;

	ID3v1() { clear(*this); }
	int			track()		const { return comment[28] == 0 ? comment[29] : 0; }
	const char*	get_genre()	const { return genre < undef ? genre_names[genre] : 0; }

	bool		read(istream_ref file, uint32 first4bytes) {
		*(uint32be*)this = first4bytes;
		return file.readbuff((uint8*)this + 4, 124) == 124;
	}
};

struct CD_TOC {
	uint8		length[2];
	uint8		first_track;
	uint8		last_track;
	struct TRACK {
		uint8	reserved1;
		uint8	control:4, adr:4;
		uint8	track;
		uint8	reserved2;
		uint8	address[4];
	} tracks[];
};

struct ID3v2 {
	enum HEADER_FLAGS {
		HF_UNSYNC			= 128,
		HF_EXTENDED			= 64,
		HF_EXPERIMENTAL		= 32,
		HF_FOOTER			= 16,
		HF_UNKNOWN			= 15,
	};
	enum EXTHEADER_FLAGS {
		XHF_UPDATE			= 64,
		XHF_CRC				= 32,
		XHF_RESTRICT		= 16,
	};
	enum FRAME_FLAGS {
		FF_TAG_DISCARD		= 16384,
		FF_FILE_DISCARD		= 8192,
		FF_READ_ONLY		= 4096,
		FF_GROUP			= 64,
		FF_COMPRESS			= 8,
		FF_ENCRYPT			= 4,
		FF_UNSYNC			= 2,
		FF_DATLEN			= 1,
		FF_UNKNOWN			= 36784,
	};
	enum ENCODING {
		ENC_LATIN1			= 0,
		ENC_UTF16BOM		= 1,
		ENC_UTF16BE			= 2,
		ENC_UTF8			= 3,
	};
	enum IMAGE {
		IMG_OTHER			= 0,
		IMG_ICON_32X32		= 1,
		IMG_ICON_OTHER		= 2,
		IMG_COVER_FRONT		= 3,
		IMG_COVER_BACK		= 4,
		IMG_LEAFLET			= 5,
		IMG_MEDIA			= 6,
		IMG_LEAD			= 7,
		IMG_ARTIST			= 8,
		IMG_CONDUCTOR		= 9,
		IMG_BAND			= 10,
		IMG_COMPOSER		= 11,
		IMG_LYRICIST		= 12,
		IMG_LOCATION		= 13,
		IMG_RECORDING		= 14,
		IMG_PERFORMANCE		= 15,
		IMG_VIDEO_CAPTURE	= 16,
		IMG_FISH			= 17,
		IMG_ILLUSTRATION	= 18,
		IMG_ARTIST_LOGO		= 19,
		IMG_PUBLISHER_LOGO	= 20,
	};
	struct text {
		uint32	id, lang;
		string	descr, s;
		text() : id(0), lang(0) {}
		void	set_lang_id(const void *p, uint32 _id) { memcpy(&lang, p, 3); id = _id; }
	};
	struct blob : rawbuffer {
		string	owner;
	};
	struct image : rawbuffer {
		IMAGE	id;
		string	descr, mime;
	};

	static uint8*	next_text(uint8* prev, ENCODING enc, size_t limit);
	static string	get_string(ENCODING enc, const uint8 *source, size_t source_size);
	static bool		synchsafe(uint8 *p, uint32 &r) {
		return !((p[0]|p[1]|p[2]|p[3]) & 0x80)
			&& (r = ((uint32)p[0] << 21) | ((uint32)p[1] << 14) | ((uint32)p[2] << 7) | (uint32)p[3], true);
	}
	static bool		get_id(int ver, uint8 *p, uint32 &r) {
		r = ((uint32)p[0] << 16) | ((uint32)p[1] << 8) | (uint32)p[2];
		if (ver > 2)
			r = (r << 8) | p[3];
		return true;
	}
	static bool		get_long(int ver, uint8 *p, uint32 &r) {
		switch (ver) {
			case 2:	r = ((uint32)p[0] << 16) | ((uint32)p[1] << 8) | (uint32)p[2]; return true;
			case 3: r = ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3]; return true;
			default: return synchsafe(p, r);
		}
	}

	uint8					version; // 3 or 4 for ID3v2.3 or ID3v2.4.
	dynamic_array<text>		comments;
	dynamic_array<text>		texts;
	dynamic_array<text>		extras;
	dynamic_array<text>		urls;
	dynamic_array<blob>		blobs;
	dynamic_array<image>	images;
	rawbuffer				cd_toc;

	const text*	get_text(uint32 id) const {
		for (auto &i : texts) {
			if (i.id == id)
				return &i;
		}
		return nullptr;
	}
	const rawbuffer* get_image_data(IMAGE id) const {
		for (auto &i : images) {
			if (i.id == id)
				return &i;
		}
		return nullptr;
	}

	const char*	get_string(uint32 id)		const { const text *t = get_text(id); return t ? (const char*)t->s : 0; }
	CD_TOC*		get_CD_TOC()				const { return cd_toc; }

	ID3v2() : version(0) {}
	bool		read(istream_ref file, uint32 first4bytes, RVAsetting *rva);
};

enum ENCODING {
	ENC_NONE		= 0,
	ENC_8			= 0x0001,	ENC_16			= 0x0002,	ENC_24	= 0x0004,	ENC_32	= 0x0008,	ENC_64	= 0x0010,
	ENC_SIGNED		= 0x0020,	ENC_UNSIGNED	= 0x0040,
	ENC_FIXED		= 0x0080,	ENC_FLOAT		= 0x0100,
	ENC_ULAW		= 0x0200,	ENC_ALAW		= 0x0400,
	ENC_ANY			= 0xffff
};

ENCODING operator|(ENCODING a, ENCODING b) { return ENCODING(int(a) | int(b)); }

struct audioformat {
	ENCODING	encoding;
	int			rate;
	uint8		channels;
	uint8		encsize;

	audioformat() : encoding(ENC_NONE), rate(0), channels(0), encsize(0) {}

	bool		set(ENCODING e, int r, int c) {
		int	s =	e & ENC_8	? 1
			:	e & ENC_16	? 2
			:	e & ENC_24	? 3
			:	e & ENC_32	? 4
			:	e & ENC_64	? 8
			:	0;
		if (s) {
			encoding	= e;
			rate		= r;
			channels	= c;
			encsize		= s;
			return true;
		}
		return false;
	}

	streamptr	samples_to_bytes(streamptr s)	const	{ return s * (encsize * channels); }
	streamptr	bytes_to_samples(streamptr b)	const	{ return b / (encsize * channels); }
	streamptr	samples_to_storage(streamptr s)	const	{ return encoding & ENC_24 ? s * 4 * channels : samples_to_bytes(s); }
};

struct gapless {
	enum {GAPLESS_DELAY = 529};

	int64	begin;			// overall begin offset in samples
	int64	end;			// overall end offset in samples

	int		firstframe;		// start decoding from here
	int		firstoff;		// number of samples to ignore from firstframe
	int		lastframe;		// last frame to decode (for gapless or num_frames limit)
	int		lastoff;		// number of samples to use from lastframe

	gapless() : begin(0), end(0), firstframe(0), lastframe(0) {}

	void	clear() {
		begin			= end = 0;
		firstframe		= 0;
		lastframe		= -1;
	}
	void	set_range(int64 _begin, int64 _end) {
		begin	= _begin + GAPLESS_DELAY;
		end		= _end + GAPLESS_DELAY;
	}

	bool	preframe(int64 f, int preframes) {
		return f >= 0 && f < firstframe && firstframe - f  < preframes;
	}

	void	frameseek(int f, int spf) {
		firstframe = f;
		int	beg_f = begin / spf;
		if (f <= beg_f) {
			firstframe	= beg_f;
			firstoff	= begin - beg_f * spf;
		} else {
			firstoff	= 0;
		}
		if (end > 0) {
			lastframe	= end / spf;
			lastoff		= end - lastframe * spf;
		} else {
			lastframe	= -1;
			lastoff		= 0;
		}
	}

	size_t	buffer_adjust(int n, size_t &nsamps) {
		size_t	off = 0;
		if (lastframe >= 0 && n >= lastframe)
			nsamps = min(nsamps, n == lastframe ? lastoff : 0);

		if (n == firstframe) {
			off		= firstoff;
			nsamps	= nsamps < off ? 0 : nsamps - off;
		}
		return off;
	}
};

struct mpg123_pars {
	enum {RATES = 9, CHANNELS = 2};
	static const int		rates[];
	static const ENCODING	encodings[];

	enum PARAM {
		PARAM_MONO_LEFT			= 0x1,		// Force playback of left channel only.
		PARAM_MONO_RIGHT		= 0x2,		// Force playback of right channel only.
		PARAM_MONO_MIX			= 0x4,		// Force playback of mixed mono.
		PARAM_FORCE_STEREO		= 0x8,		// Force stereo output.
		PARAM_DITHER			= 0x10,
		PARAM_EQUALIZER			= 0x20,
		PARAM_GAPLESS			= 0x40,		// Enable gapless decoding (default on if libmpg123 has support).
		PARAM_NO_RESYNC			= 0x80,		// Disable resync stream after error.
		PARAM_IGNORE_STREAMLENGTH=0x100,	// Ignore any stream length information contained in the stream, which can be contained in a 'TLEN' frame of an ID3v2 tag or a Xing tag
		PARAM_SKIP_ID3V2		= 0x200,	// Do not parse ID3v2 tags, just skip them.
		PARAM_SKIP_LAME			= 0x400,	// Do not parse the LAME/Xing info frame, treat it as normal MPEG data.
		PARAM_RVA				= 0x1000,	// use RVA
		PARAM_RVA_ALBUM			= 0x2000,	// use album/audiophile gain

		_PARAM_FORCE_MONO		= PARAM_MONO_LEFT | PARAM_MONO_RIGHT | PARAM_MONO_MIX,
		_PARAM_FORCE_MIX		= _PARAM_FORCE_MONO | PARAM_FORCE_STEREO,
	};

	flags<PARAM>	params;
	int				resync_limit;
	int				preframes;
	float			outscale;
	float			equalizer[2][32];
	uint16			audio_caps[CHANNELS][RATES + 1];

	int		rate2num(int r) const {
		for (int i = 0; i < RATES; i++) {
			if (rates[i] == r)
				return i;
		}
		return -1;
	}
	ENCODING	cap_fit(int chans, int rate) const;

	void	allow_none() {
		clear(audio_caps);
	}
	void	allow_all()	{
		for (int c = 0; c < CHANNELS; ++c) {
			for (int r = 0; r < RATES + 1; ++r)
				audio_caps[c][r] = ENC_ANY;
		}
	}
	bool	allow(int rate, int chans, uint16 enc) {
		if ((chans & 3) == 0)
			return false;
		if (rate) {
			int	ir = rate2num(rate);
			if (ir < 0)
				return false;
			for (int ic = (chans & 1 ? 0 : 1); ic < (chans & 2 ? 2 : 1); ++ic)
				audio_caps[ic][ir] |= enc;
		} else {
			for (int ic = (chans & 1 ? 0 : 1); ic < (chans & 2 ? 2 : 1); ++ic)
				for (int ir = 0; ir < RATES; ++ir)
					audio_caps[ic][ir] |= enc;
		}
		return true;
	}
	bool	dis_allow(int rate, int chans, uint16 enc) {
		if ((chans & 3) == 0)
			return false;
		if (rate) {
			int	ir = rate2num(rate);
			if (ir < 0)
				return false;
			for (int ic = (chans & 1 ? 0 : 1); ic < (chans & 2 ? 2 : 1); ++ic)
				audio_caps[ic][ir] &= ~enc;
		} else {
			for (int ic = (chans & 1 ? 0 : 1); ic < (chans & 2 ? 2 : 1); ++ic)
				for (int ir = 0; ir < RATES; ++ir)
					audio_caps[ic][ir] &= ~enc;
		}
		return true;
	}

	int			set_format(audioformat &af, int chans, int rate) const;

	void		set_equalizer(float (&vals)[2][32]) {
		memcpy(equalizer, vals, sizeof(equalizer));
		params.set(PARAM_EQUALIZER);
	}
	void		set_scale(float _scale)			{ outscale = _scale; }
	void		set_preframes(int _preframes)	{ preframes = _preframes; }

	mpg123_pars() : params(PARAM_GAPLESS), resync_limit(1024), preframes(4), outscale(1) {
		allow_all();
	}
};

struct null_filter {
	float	operator()(float v)	{ return v; }
};

struct dither_filter {
	enum { DITHERSIZE = 65536 };
	enum type {
		white_noise,
		tpdf_noise,
		highpass_tpdf_noise,
	};
	float	*buffer;
	int		index;

	dither_filter() : buffer(0), index(0)	{}
	~dither_filter()	{ free(buffer); }
	void	init(type t);
	int		need(int n)			{ if (DITHERSIZE - index < n) index = 0; return index; }
	void	set(int n)			{ index = n; }
	float	operator()(float v)	{ return v + buffer[index++]; }
};

struct decode_tables {
	float	table[512 + 32];
	float	scale;
	decode_tables() : scale(-1)	{}
	void	set(float _scale, bool force);
	operator const float*() { return table; }
};

struct mpg_header {
 	enum {VER_2_5, VER_RES, VER_2, VER_1};
	enum {LAY_RES, LAY_III, LAY_II, LAY_I};
	enum {CRC_ON, CRC_OFF};
	enum {STEREO, JOINT_STEREO, DUAL_CHANNEL, MONO};

	static const int bitrate_table[2][3][16];
	static const int samplerate_table[9];

	union {
		uint32	u;
		struct { uint32 ISO_BITFIELDS13(
			sync:		11,
			version:	2,
			layer:		2,
			crc:		1,
			bitrate:	4,
			samplerate:	2,
			padding:	1,
			privte:		1,
			channel:	2,
			chanex:		2,
			copyright:	1,
			original:	1,
			emphasis:	2
		); };
	};
	mpg_header(uint32 u = 0) : u(u)		{}
	operator uint32()			const	{ return u;	}

	bool	check()				const	{ return sync == 0x7ff && layer != 0 && bitrate != 15 && samplerate != 3; }
	bool	mono()				const	{ return channel == MONO;			}
	bool	joint_stereo()		const	{ return channel == JOINT_STEREO;	}
	bool	freeformat()		const	{ return bitrate == 0;				}
	bool	mpeg25()			const	{ return !(version & 2);			}
	bool	lsf()				const	{ return version != VER_1;			}
	bool	error_prot()		const	{ return crc == CRC_ON;				}
	int		get_offset()		const	{ return lsf() ? (mono() ? 9 : 17) : (mono() ? 17 : 32);}
	int		get_bitrate()		const	{ return bitrate_table[lsf()][layer-1][bitrate];		}
	int		get_freqindex()		const	{ return samplerate + (mpeg25() ? 6 : lsf() ? 3 : 0);	}
	int		get_samplerate()	const	{ return samplerate_table[get_freqindex()];				}
	int		get_numsamples()	const	{ return layer == LAY_I ? 384 : layer == LAY_II ? 1152 : mpeg25() ? 576 : 1152;	}

	bool	similar(const mpg_header &b)		const	{ return sync == b.sync && version == b.version && layer == b.layer && samplerate == b.samplerate; }
	bool	very_similar(const mpg_header &b)	const	{ return similar(b) && bitrate == b.bitrate && channel == b.channel && chanex == b.chanex; }
};

typedef vlc_in<uint32, true, byte_reader&>	vlc_reader;

struct mpg123 : mpg123_pars {
	istream_ref			file;
	byte_reader		buffer;
	vlc_reader		vlc;

	enum {MAXFRAMESIZE		= 3456};

	enum RETURNVALS {
		RET_OK				= 0, 	// Success
		RET_DONE,					// Track ended. Stop decoding
		RET_NEW_FORMAT,				// Output format will be different on next call
		RET_BAD_OUTFORMAT, 			// Unable to set up output format
		RET_BAD_BUFFER,				// Bad buffer given -- invalid pointer or too small size.
		RET_READER_ERROR,			// Error reading the stream.
		RET_OUT_OF_SYNC,			// Lost track in bytestream and did not try to resync.
		RET_RESYNC_FAIL,			// Resync failed to find valid MPEG data.
	};

	enum STATE_FLAGS {
		STATE_ACCURATE		= 1 << 0,	//0001 Positions are considered accurate.
		STATE_FRANKENSTEIN	= 1 << 1,	// This stream is concatenated.
		STATE_FRESH			= 1 << 2,
		STATE_FREEFORMAT	= 1 << 3,
		STATE_DECODERCHANGE	= 1 << 4,
		STATE_FORMATCHANGE	= 1 << 5,
		STATE_OWNBUFFER		= 1 << 6,
	};
	enum MIX {
		MIX_LEFT	= 0,
		MIX_RIGHT	= 1,
		MIX_MIX		= 2,
		MIX_STEREO	= 3,
	};

	flags<STATE_FLAGS>	state;
	mpg_header		firsthead;		// The first header of the stream.
	mpg_header		h;

	MIX				mix;
	int64			num;			// frame number
	streamptr		audio_start;	// The byte offset in the file where audio data begins.
	size_t			ff_framesize;	// freeform framesize
	size_t			framesize;		// computed framesize
	gapless			gapless_info;
	decode_tables	decode;
	dither_filter	dither;
	rawbuffer		layerscratch;

	// input data
	uint8			bsspace[2][512 + MAXFRAMESIZE];
	uint8			bsnum;

	// output data
	outbuffer		out;
	audioformat		af;
	bool			to_decode;		// this frame holds data to be decoded
	uint16			crc;
	int				clip;

	// synth buffers
	rawbuffer		rawbuffs;
	float			*real_buffs[2][2];
	int				bo;				// output buffer index?

	// the meta crap
	LAME			lame;
	ID3v1			id3v1;
	ID3v2			id3v2;
	RVAsetting		rva[2];

	int				synth(float*, int, int);
	int				synth_mono(float*);
	int				synth_stereo(float*, float*);
	void			set_pointer(uint8 *p)	{ buffer = p; vlc.reset(); }

private:
	static void		init();
	size_t			set_header(mpg_header nh, int *ff_count);
	void			adjust_volume();
	RETURNVALS		init_decoder();
	RETURNVALS		read_frame();
	int				do_layer();

public:
	mpg123(istream_ref _file);

	RETURNVALS		decode_frame(int64 *outnum, void **outdata, size_t *outlen);
	audioformat		get_format()	const { return af; }

	const char*		get_title()		const { if (const char *s = id3v2.get_string('TIT2')) return s; return id3v1.title; }
	const char*		get_album()		const { if (const char *s = id3v2.get_string('TALB')) return s; return id3v1.album; }
	const char*		get_artist()	const { if (const char *s = id3v2.get_string('TPE1')) return s; return id3v1.artist; }
	const char*		get_year()		const { if (const char *s = id3v2.get_string('TYER')) return s; return id3v1.year; }
	const char*		get_genre()		const { if (const char *s = id3v2.get_string('TCON')) return s; return id3v1.get_genre(); }
};

//-----------------------------------------------------------------------------
//	modified-DCT routines
//-----------------------------------------------------------------------------

template<typename T> struct mdct64 {
	static T cos64[16], cos32[8], cos16[4], cos8[2], cos4[1];
	static void init();
	static void process(const T *in, T *prev, T *next);
};

template<typename T> T mdct64<T>::cos64[16];
template<typename T> T mdct64<T>::cos32[8];
template<typename T> T mdct64<T>::cos16[4];
template<typename T> T mdct64<T>::cos8[2];
template<typename T> T mdct64<T>::cos4[1];

template<typename T> void mdct64<T>::init() {
	T *pnts[5] = {cos64, cos32, cos16, cos8, cos4};
	for (int i = 0; i < 5; i++) {
		int		kr		= 16 >> i;
		int		divv	= 64 >> i;
		T	*tab	= pnts[i];
		for (int k = 0; k < kr; k++)
			tab[k] = half / cos(pi * (k * 2 + 1) / divv);
	}
}

template<typename T> void mdct64<T>::process(const T *in, T *prev, T *next) {
	T	tmp[64];
	T	*costab;

	const T	*b1		= in;
	const T	*b2		= b1 + 32;
	T		*bs		= tmp;

	costab	= cos64 + 16;
	while (bs < tmp + 16)
		*bs++ = (*b1++ + *--b2);
	while (bs < tmp + 32)
		*bs++ = (*--b2 - *b1++) * *--costab;

	b1		= tmp;
	b2		= b1 + 16;
	costab	= cos32 + 8;
	while (bs < tmp + 40)
		*bs++ = (*b1++ + *--b2);
	while (bs < tmp + 48)
		*bs++ = (*--b2 - *b1++) * *--costab;

	b2		= b1 + 16;
	costab	= cos32 + 8;
	while (bs < tmp + 56)
		*bs++ = (*b1++ + *--b2);
	while (bs < tmp + 64)
		*bs++ = (*b1++ - *--b2) * *--costab;

	b2		= b1 + 8;
	bs		= tmp;
	costab	= cos16;
	for (int j = 2; j; j--) {
		for (int i = 4; i--;)
			*bs++ = (*b1++ + *--b2);
		for (int i = 4; i--;)
			*bs++ = (*--b2 - *b1++) * costab[i];
		b2 += 16;
		for (int i = 4; i--;)
			*bs++ = (*b1++ + *--b2);
		for (int i = 4; i--;)
			*bs++ = (*b1++ - *--b2) * costab[i];
		b2 += 16;
	}

	b1		= tmp;
	costab	= cos8;
	for (int i = 4; i--;) {
		b2		= b1 + 4;
		*bs++ = (*b1++ + *--b2);
		*bs++ = (*b1++ + *--b2);
		*bs++ = (*--b2 - *b1++) * costab[1];
		*bs++ = (*--b2 - *b1++) * costab[0];
		b2		= b1 + 4;
		*bs++ = (*b1++ + *--b2);
		*bs++ = (*b1++ + *--b2);
		*bs++ = (*b1++ - *--b2) * costab[1];
		*bs++ = (*b1++ - *--b2) * costab[0];
	}

	bs		= tmp;
	costab	= cos4;
	for (int i = 8; i--;) {
		T v0 = *b1++;
		T v1 = *b1++;
		*bs++ = v0 + v1;
		*bs++ = (v0 - v1) * costab[0];
		v0 = *b1++;
		v1 = *b1++;
		*bs++ = (v0 + v1);
		*bs++ = (v1 - v0) * costab[0];
	}

	for (T *b1 = tmp; b1 < tmp + 32; b1 += 4)
		b1[2] += b1[3];

	for (T *b1 = tmp; b1 < tmp + 32; b1 += 8) {
		b1[4] += b1[6];
		b1[6] += b1[5];
		b1[5] += b1[7];
	}

	for (T *b1 = tmp; b1 < tmp + 32; b1 += 16) {
		b1[8]  += b1[12];
		b1[12] += b1[10];
		b1[10] += b1[14];
		b1[14] += b1[9];
		b1[9]  += b1[13];
		b1[13] += b1[11];
		b1[11] += b1[15];
	}

	prev[16 * 16] = tmp[0];
	prev[16 * 15] = tmp[16 +  0] + tmp[16 +  8];
	prev[16 * 14] = tmp[8];
	prev[16 * 13] = tmp[16 +  8] + tmp[16 +  4];
	prev[16 * 12] = tmp[4];
	prev[16 * 11] = tmp[16 +  4] + tmp[16 + 12];
	prev[16 * 10] = tmp[12];
	prev[16 *  9] = tmp[16 + 12] + tmp[16 +  2];
	prev[16 *  8] = tmp[2];
	prev[16 *  7] = tmp[16 +  2] + tmp[16 + 10];
	prev[16 *  6] = tmp[10];
	prev[16 *  5] = tmp[16 + 10] + tmp[16 +  6];
	prev[16 *  4] = tmp[6];
	prev[16 *  3] = tmp[16 +  6] + tmp[16 + 14];
	prev[16 *  2] = tmp[14];
	prev[16 *  1] = tmp[16 + 14] + tmp[16 +  1];
	prev[16 *  0] = tmp[1];

	next[16 *  0] = tmp[1];
	next[16 *  1] = tmp[16 +  1] + tmp[16 +  9];
	next[16 *  2] = tmp[9];
	next[16 *  3] = tmp[16 +  9] + tmp[16 +  5];
	next[16 *  4] = tmp[5];
	next[16 *  5] = tmp[16 +  5] + tmp[16 + 13];
	next[16 *  6] = tmp[13];
	next[16 *  7] = tmp[16 + 13] + tmp[16 +  3];
	next[16 *  8] = tmp[3];
	next[16 *  9] = tmp[16 +  3] + tmp[16 + 11];
	next[16 * 10] = tmp[11];
	next[16 * 11] = tmp[16 + 11] + tmp[16 +  7];
	next[16 * 12] = tmp[7];
	next[16 * 13] = tmp[16 +  7] + tmp[16 + 15];
	next[16 * 14] = tmp[15];
	next[16 * 15] = tmp[16 + 15];
}

template<typename T> struct mdct36 {
	static T cos6[2], cos9[3], cos18[3];
	static T tfcos36[9];

	force_inline static void chunk(T *out, T *x) {
		T	t3 = x[0];
		T	t0 = cos6[1] * (x[4] + x[8] - x[2]);
		T	t1 = cos6[1] * x[6];
		T	t2 = t3 - t1 - t1;

		out[4]	= t2 + t0 + t0;
		t0		= t2 - t0;
		t2		= cos6[0] * (x[5] + x[7] - x[1]);
		out[1]	= t0 - t2;
		out[7]	= t0 + t2;

		t3 += t1;

		t0 = cos9[0] * (x[2] + x[4]);
		t1 = cos9[1] * (x[4] - x[8]);
		t2 = cos9[2] * (x[2] + x[8]);

		T	r0 = t3 + t0 + t1;
		T	r1 = t3 - t0 - t2;
		T	r2 = t3 - t1 + t2;

		t0 = cos18[0] * (x[1] + x[5]);
		t1 = cos18[1] * (x[5] - x[7]);
		t2 = cos6[0] * x[3];
		t3 = t0 + t1 + t2;

		out[0] = r0 + t3;
		out[8] = r0 - t3;

		t3 = cos18[2] * (x[1] + x[7]);
		t0 += t3 - t2;
		t1 -= t3 + t2;

		out[3] = r2 + t0;
		out[5] = r2 - t0;

		out[2] = r1 + t1;
		out[6] = r1 - t1;
	}

	static void init();
	template<typename O> static void process(const T *in, const T *prev, T *next, T *win, O out);
};

template<typename T> T mdct36<T>::cos6[2];
template<typename T> T mdct36<T>::cos9[3];
template<typename T> T mdct36<T>::cos18[3];
template<typename T> T mdct36<T>::tfcos36[9];

template<typename T> void mdct36<T>::init() {
	for (int i = 0; i < 9; i++)
		tfcos36[i] = half / cos(pi * (i * 2 + 1) / 36);

	cos6[0]	=	cos(pi *  1 / 6);
	cos6[1]	=	cos(pi *  2 / 6);

	cos9[0]	=	cos(pi *  1 / 9);
	cos9[1]	=	cos(pi *  5 / 9);
	cos9[2]	=	cos(pi *  7 / 9);

	cos18[0] =	cos(pi *  1 / 18);
	cos18[1] =	cos(pi * 11 / 18);
	cos18[2] =	cos(pi * 13 / 18);
}

template<typename T> template<typename O> void mdct36<T>::process(const T *in, const T *prev, T *next, T *win, O out) {
	T	tmp0[9], tmp1[9];
	T	x[9];

	x[8] = in[16] + in[15];
	x[7] = in[14] + in[13];
	x[6] = in[12] + in[11];
	x[5] = in[10] + in[9];
	x[4] = in[8]  + in[7];
	x[3] = in[6]  + in[5];
	x[2] = in[4]  + in[3];
	x[1] = in[2]  + in[1];
	x[0] = in[0];
	chunk(tmp0, x);

	x[8] = in[17] + in[16];
	x[7] = in[15] + in[14];	x[8] += x[7];
	x[6] = in[13] + in[12];	x[7] += x[6];
	x[5] = in[11] + in[10];	x[6] += x[5];
	x[4] = in[9]  + in[8];	x[5] += x[4];
	x[3] = in[7]  + in[6];	x[4] += x[3];
	x[2] = in[5]  + in[4]; 	x[3] += x[2];
	x[1] = in[3]  + in[2];	x[2] += x[1];
	x[0] = in[1]  + in[0];	x[1] += x[0];
	chunk(tmp1, x);

	for (int i = 0; i < 9; i++) {
		T	x	= tmp0[i], y = tmp1[i] * tfcos36[i];
		T	t0	= x + y;
		T	t1	= x - y;
		next[9 + i] = t0 * win[18 + 9 + i];
		next[8 - i] = t0 * win[18 + 8 - i];
		out [9 + i] = prev[9 + i] + t1 * win[9 + i];
		out [8 - i] = prev[8 - i] + t1 * win[8 - i];
	}
}

template<typename T> struct mdct12 {
	static T cos6[2];
	static T tfcos12[3];

	struct blob {
		T	t0, t1, t2, t3, t4, t5;
		inline blob(const T *in) {
			T	in5 = in[5 * 3];
			T	in4 = in[4 * 3];
			T	in3 = in[3 * 3];
			T	in2 = in[2 * 3];
			T	in1 = in[1 * 3];
			T	in0 = in[0 * 3];

			in5 += in4;
			in4 += in3;
			in3 += in2;
			in2 += in1;
			in1 += in0;
			in5 += in3;
			in3 += in1;

			T	x	= in0 - in4;
			T	y	= (in1 - in5) * tfcos12[1];
			t0	= x + y;
			t1	= x - y;

			in2	= in2 * cos6[0];
			in3	= in3 * cos6[0];
			in0	+= in4 * cos6[1];
			in1	+= in5 * cos6[1];

			in4	= in0 + in2;
			in5	= (in1 + in3) * tfcos12[0];
			t3	= in4 + in5;
			t5	= in4 - in5;

			in0	= in0 - in2;
			in1	= (in1 - in3) * tfcos12[2];
			t2	= in0 + in1;
			t4	= in0 - in1;
		}
	};

	static void init();
	template<typename O> static void process(const T *in, const T *prev, T *next, T *win, O out);
};

template<typename T> T mdct12<T>::cos6[2];
template<typename T> T mdct12<T>::tfcos12[3];

template<typename T> void mdct12<T>::init() {
	for (int i = 0; i < 3; i++)
		tfcos12[i] = half / cos(pi * (i * 2 + 1) / 12);

	cos6[0] = cos(pi * 1 / 6);
	cos6[1] = cos(pi * 2 / 6);
}

template<typename T> template<typename O> void mdct12<T>::process(const T *in, const T *prev, T *next, T *win, O out) {
	{
		blob	b(in + 0);
		out [11 - 1]	 = prev[11 - 1] + b.t0 * win[11 - 1];
		out [ 6 + 1]	 = prev[ 6 + 1] + b.t0 * win[ 6 + 1];
		out [ 0 + 1]	 = prev[ 0 + 1] + b.t1 * win[ 0 + 1];
		out [ 5 - 1]	 = prev[ 5 - 1] + b.t1 * win[ 5 - 1];

		out [11 - 0]	 = prev[11 - 0] + b.t2 * win[11 - 0];
		out [ 6 + 0]	 = prev[ 6 + 0] + b.t2 * win[ 6 + 0];
		out [ 6 + 2]	 = prev[ 6 + 2] + b.t3 * win[ 6 + 2];
		out [11 - 2]	 = prev[11 - 2] + b.t3 * win[11 - 2];

		out [ 0 + 0]	 = prev[ 0 + 0] + b.t4 * win[ 0 + 0];
		out [ 5 - 0]	 = prev[ 5 - 0] + b.t4 * win[ 5 - 0];
		out [ 0 + 2]	 = prev[ 0 + 2] + b.t5 * win[ 0 + 2];
		out [ 5 - 2]	 = prev[ 5 - 2] + b.t5 * win[ 5 - 2];
	}

	{
		blob	b(in + 1);
		next[ 5 - 1]	 = b.t0 * win[11 - 1];
		next[ 0 + 1]	 = b.t0 * win[ 6 + 1];
		out [ 6 + 1]	+= b.t1 * win[ 0 + 1];
		out [11 - 1]	+= b.t1 * win[ 5 - 1];

		next[ 5 - 0]	 = b.t2 * win[11 - 0];
		next[ 0 + 0]	 = b.t2 * win[ 6 + 0];
		next[ 0 + 2]	 = b.t3 * win[ 6 + 2];
		next[ 5 - 2]	 = b.t3 * win[11 - 2];

		out [ 6 + 0]	+= b.t4 * win[ 0 + 0];
		out [11 - 0]	+= b.t4 * win[ 5 - 0];
		out [ 6 + 2]	+= b.t5 * win[ 0 + 2];
		out [11 - 2]	+= b.t5 * win[ 5 - 2];
	}

	{
		blob	b(in + 2);
		next[11 - 1]	 = b.t0 * win[11 - 1];
		next[ 6 + 1]	 = b.t0 * win[ 6 + 1];
		next[ 0 + 1]	+= b.t1 * win[ 0 + 1];
		next[ 5 - 1]	+= b.t1 * win[ 5 - 1];

		next[11 - 0]	 = b.t2 * win[11 - 0];
		next[ 6 + 0]	 = b.t2 * win[ 6 + 0];
		next[ 6 + 2]	 = b.t3 * win[ 6 + 2];
		next[11 - 2]	 = b.t3 * win[11 - 2];

		next[ 0 + 0]	+= b.t4 * win[ 0 + 0];
		next[ 5 - 0]	+= b.t4 * win[ 5 - 0];
		next[ 0 + 2]	+= b.t5 * win[ 0 + 2];
		next[ 5 - 2]	+= b.t5 * win[ 5 - 2];
	}
}

//-----------------------------------------------------------------------------
//	SHOUTcast ICY
//-----------------------------------------------------------------------------

struct icy_stream : istream_chain {
	void		*meta;
	streamptr	interval, next;

	icy_stream(istream_ref file, streamptr interval) : istream_chain(file), meta(0), interval(interval), next(0) {}
	~icy_stream() { free(meta); }

	size_t	readbuff(void *buffer, size_t size);
};

} // namespace iso

#endif // MP3_H
