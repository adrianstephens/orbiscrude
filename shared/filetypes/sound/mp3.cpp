#include "mp3.h"
#include "samplefile.h"
#include "iso/iso_convert.h"
#include "base/vector.h"
#include "base/bits.h"
#include "codec/vlc.h"
#include "comms/compand.h"
#include "LAME/lame.h"
#include "directory.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	mpg_header
//-----------------------------------------------------------------------------

const int mpg_header::bitrate_table[2][3][16] = { {
	{0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320},	// III
	{0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384}, // II
	{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},	// I
}, {
	{0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160},	// III	lsf
	{0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}, // II	lsf
	{0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256},	// I	lsf
} };

const int mpg_header::samplerate_table[9] = {
	44100, 48000, 32000,	//
	22050, 24000, 16000,	// 	lsf
	11025, 12000, 8000		//	mpg25
};

//-----------------------------------------------------------------------------
//	LAME
//-----------------------------------------------------------------------------

int64 LAME::lookup_frame(int64 &frame, int64 filesize) {
	if ((void*)xing_toc && track_frames > 0) {
		int	i = clamp(frame * 100 / track_frames, 0, 99);
		frame = i * track_frames / 100;
		return ((uint8*)xing_toc)[i] * filesize / 256;
	}
	return 0;
}

bool LAME::read(const void *p, size_t size, size_t offset, RVAsetting *rva) {
	enum {
		HF_FRAMES		= 1,
		HF_BYTES		= 2,
		HF_TOC			= 4,
		HF_VBR_SCALE	= 8,
	};

	if (size < 120 + offset)
		return false;

	byte_reader	br(p);
	br.get<uint16>();
	for (int i = 2; i < offset; ++i) {
		if (br.getc() != 0)
			return false;
	}

	uint32	id = br.get<uint32be>();
	if (id != 'Info') {
		if (id != 'Xing')
			return false;
		vbr = VBR; // Xing header means always VBR
	}

	uint32	xing_flags = br.get<uint32be>();
	if (xing_flags & HF_FRAMES)
		track_frames = br.get<uint32be>();

	if (xing_flags & HF_BYTES)
		track_length = br.get<uint32be>();

	if (xing_flags & HF_TOC)
		br.readbuff(xing_toc.create(100, 4), 100);

	if (xing_flags & HF_VBR_SCALE)
		vbr_scale = br.get<uint32be>();

	// Either 0 or LAME extra data follows
	if (br.peekc()) {
		struct LAMEextra {
			char		id[4];
			char		ver[5];
			uint8		vbr:4, rev:4;
			uint8		lpf;
			float32be	peak;
			uint16be	gain[2];
			uint8		enc_flags;
			uint8		abr_rate;
			uint32		skip_begin:12, skip_end:12;
		};

		float	gain_offset = 0; // going to be +6 for old lame that used 83dB
		char	nb[10];
		br.readbuff(nb, 9);
		nb[9] = 0;
		if (str(nb, 4) == "LAME") {
			uint32	major, minor;
			if (sscanf(nb + 4, "%u.%u", &major, &minor) == 2) {
				if (major < 3 || (major == 3 && minor < 95))
					gain_offset = 6;
			}
		}
		// the 4 big bits are tag revision, the small bits vbr method
		uint8	v	= br.getc();
		ver	= v >> 4;
		switch(v & 15) {
			case 1:	case 8: vbr = CBR; break;
			case 2:	case 9:	vbr = ABR; break;
			default: vbr = VBR; // 00==unknown is taken as VBR
		}
		lpf	= br.getc();

		float	peak		= br.get<floatbe>();
		float	replay_gain[2] = {0,0};
		for (int i = 0; i < 2; ++i) {
			uint16	w = br.get<uint16be>();
			if (uint8 origin = (w >> 10) & 0x7) { /* the 3 bits after that... */
				uint8 gt = w >> 13;
				if (gt == 1 || gt == 2) {
					replay_gain[gt - 1] = sign_extend<10>(w & 0x3ff) * .1f;
					if (origin == 3)
						replay_gain[gt - 1] += gain_offset;
				}
			}
		}
		for (int i = 0; i < 2; ++i)
			rva[i].set(RVAsetting::LAME, replay_gain[i], peak);

		enc_flags	= br.getc();
		abr_rate	= br.getc();

		{// encoder delay and padding, two 12 bit values
			uint8	a	= br.getc(), b = br.getc(), c = br.getc();
			skipbegin	= (a << 4) | (b >> 4);
			skipend		= ((b << 8) | c) & 0xfff;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//	ID3v1
//-----------------------------------------------------------------------------

const char *ID3v1::genre_names[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
	"Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
	"Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
	"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native US", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes",
	"Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
	"Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass",
	"Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic",
	"Humour", "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove",
	"Satire", "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle",
	"Duet", "Punk Rock", "Drum Solo", "Acapella", "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club - House", "Hardcore",
	"Terror", "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover",
	"Contemporary Christian", "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "Synthpop",
};

//-----------------------------------------------------------------------------
//	ID3v2
//-----------------------------------------------------------------------------

uint8 *ID3v2::next_text(uint8* prev, ENCODING enc, size_t limit) {
	if (enc == ENC_LATIN1 || enc == ENC_UTF8) {
		for (uint8 *text = prev, *end = text + limit; text < end; ++ text) {
			if (*text == 0)
				return text + 1;
		}
	} else {
		if (uintptr_t(prev) & 1)
			prev++;
		for (uint16 *text = (uint16*)prev, *end = text + limit / 2; text < end; ++ text) {
			if (*text == 0)
				return (uint8*)(text + 1);
		}
	}
	return 0;
}

string ID3v2::get_string(ENCODING enc, const uint8 *source, size_t source_size) {
	if (source_size) {
		switch (enc) {
			case ENC_LATIN1:
			case ENC_UTF8:
				return str(source, source_size);

			case ENC_UTF16BOM:
				if (source[0] == 0xff && source[1] == 0xfe)
					return str((const char16le*)source + 1, source_size / 2 - 1);

				if (source[0] != 0xfe || source[1] != 0xff)
					break;

				source += 2;
				source_size -= 2;
			case ENC_UTF16BE:
				return str((const char16be*)source, source_size / 2);
		}
	}
	return string();
}

bool ID3v2::read(istream_ref file, uint32 first4bytes, RVAsetting *rva) {
	uint8	buf[6];
	uint32	length	= 0;
	uint8	ver		= first4bytes & 0xff;

	if (ver == 0xff || !file.read(buf) || buf[0] == 0xff || !synchsafe(buf + 2, length))
		return false;

	bool	ret		= true;
	uint8	flags	= buf[1];

	if ((flags & HF_UNKNOWN) || ver > 4 || ver < 2) {
		file.seek_cur(length);

	} else {
		version	= ver;
		uint8* tagdata	= (uint8*)malloc(length + 1);
		file.readbuff(tagdata, length);
		tagdata[length] = 0;

		uint32 tagpos = 0;
		if ((flags & HF_EXTENDED) && !get_long(ver, tagdata, tagpos))
			ret = false;

		while (ret && tagpos < length - 10) {
			int		head_part	= ver == 2 ? 3 : 4;
			for (int i = 0; i < head_part; ++i) {
				char	b = tagdata[tagpos + i];
				if (!(b >= '0' && b <= '9') && !(b >= 'A' && b <= 'Z')) {
					ret = false;
					break;
				}
			}
			if (!ret)
				break;

			uint32	id, framesize;
			if (!get_id(ver, tagdata + tagpos, id)
			||	!get_long(ver, tagdata + tagpos + head_part, framesize))
				break;

			uint32	pos	= tagpos + head_part * 2;

			tagpos	= pos + framesize;
			if (tagpos > length)
				break;

			uint16	fflags = 0;
			if (ver > 2) {
				fflags	= ((uint32)tagdata[pos] << 8) | (uint32)tagdata[pos + 1];
				pos		+= 2;
				tagpos	+= 2;
				if (fflags & (FF_UNKNOWN | FF_COMPRESS | FF_ENCRYPT))
					continue;
			}

			if (head_part < 4) {
				struct {uint32 oldid, newid; } ids[] = {
					{'COM','COMM'},{'TAL','TALB'},{'TBP','TBPM'},{'TCM','TCOM'},
					{'TCO','TCON'},{'TCR','TCOP'},{'TDA','TDAT'},{'TDY','TDLY'},
					{'TEN','TENC'},{'TFT','TFLT'},{'TIM','TIME'},{'TKE','TKEY'},
					{'TLA','TLAN'},{'TLE','TLEN'},{'TMT','TMED'},{'TOA','TOPE'},
					{'TOF','TOFN'},{'TOL','TOLY'},{'TOR','TORY'},{'TOT','TOAL'},
					{'TP1','TPE1'},{'TP2','TPE2'},{'TP3','TPE3'},{'TP4','TPE4'},
					{'TPA','TPOS'},{'TPB','TPUB'},{'TRC','TSRC'},{'TDA','TRDA'},
					{'TRK','TRCK'},{'TSI','TSIZ'},{'TSS','TSSE'},{'TT1','TIT1'},
					{'TT2','TIT2'},{'TT3','TIT3'},{'TXT','TEXT'},{'TXX','TXXX'},
					{'TYE','TYER'},
				};
				for (int i = 0; i < num_elements(ids); ++i) {
					if (id == ids[i].oldid) {
						id = ids[i].newid;
						break;
					}
				}
				if (id < 0x1000000)
					continue;
			}

			uint8	*realdata	= tagdata + pos;
			uint32	realsize	= framesize;
			if ((flags & HF_UNSYNC) || (fflags & FF_UNSYNC)) {
				uint8	*p = realdata + 1;
				for (uint8 *i = realdata + 1, *e = realdata + framesize; i < e; ++i) {
					if (i[0] != 0 || i[-1] != 0xff)
						*p++ = *i;
				}
				realsize = p - realdata;
			}
			uint8	*realend	= realdata + realsize;

			switch (id) {
				case 'APIC': {	//Attached picture
					ENCODING	enc		= (ENCODING)realdata[0];
					uint8		*mime	= realdata + 1;
					uint8		*descr	= next_text(mime, enc, realend - mime);
					IMAGE		id		= IMAGE(*descr++);
					uint8		*data	= (uint8*)next_text(descr, enc, realend - (uint8*)descr);
					if (data < realend) {
						image	*x = new(images) image;
						size_t	s	= realend - (uint8*)data;
						memcpy(x->create(s, 4), realend, s);
						x->descr	= (char*)descr;
						x->mime		= (char*)mime;
						x->id		= id;
					}
					break;
				}
				case 'MCDI':	//Music CD identifier
					memcpy(cd_toc.create(realsize, 4), realdata, realsize);
					break;

				case 'COMM':	//Comments
					if (realsize > 4) {
						ENCODING	enc		= (ENCODING)realdata[0];
						uint8		*descr	= realdata + 4;
						if (uint8 *txt = next_text(descr, enc, realsize - 4)) {
							text	*x	= new(comments)	text;
							x->set_lang_id(realdata + 1, id);
							x->descr	= get_string(enc, descr, txt - descr);
							x->s		= get_string(enc, txt, realsize - (txt - realdata));

							int rva_mode = istr(x->descr) == "rva"
										|| istr(x->descr) == "rva_mix"
										|| istr(x->descr) == "rva_track"
										|| istr(x->descr) == "rva_radio"
							? 0 :		   istr(x->descr) == "rva_album"
										|| istr(x->descr) == "rva_audiophile"
										|| istr(x->descr) == "rva_user"
							? 1 : -1;
							if (rva_mode > -1)
								rva[rva_mode].set(RVAsetting::COMMENT, (float)atof(x->s), 0);
						}
					}
					break;

				case 'TXXX':	//User defined text information
					if (realsize > 1) {
						ENCODING	enc		= (ENCODING)realdata[0];
						uint8		*descr	= realdata + 1;
						if (uint8 *txt = next_text(descr, enc, realsize - 1)) {
							text	*x	= new(extras) text;
							x->id		= id;
							x->descr	= get_string(enc, (uint8*)descr, txt - descr);
							x->s		= get_string(enc, (uint8*)txt - 1, realend - (uint8*)txt);

							if (x->descr) {
								bool	is_peak		= false;
								int		rva_mode	= -1;
								if (istr(x->descr).begins("replaygain_track_") && istr(x->descr) != "replaygain_track_gain") {
									rva_mode = 0;
									is_peak = istr(x->descr) == "replaygain_track_peak";
								} else if (istr(x->descr).begins("replaygain_album_") && istr(x->descr) != "replaygain_album_gain") {
									rva_mode = 1;
									is_peak = istr(x->descr) == "replaygain_album_peak";
								}
								if (rva_mode > -1) {
									(is_peak ? rva[rva_mode].peak : rva[rva_mode].gain) = (float)atof(x->s);
									rva[rva_mode].level = RVAsetting::EXTRA;
								}
							}
						}
					}
					break;

				case 'RVA2': {	//Relative volume adjustment
					int	rva_mode = int(
						istr((char*)realdata).begins("album")
					||	istr((char*)realdata).begins("audiophile")
					||	istr((char*)realdata).begins("user")
					);
					pos += uint32(strlen((char*)realdata) + 1);
					if (realdata[pos] == 1) {
						rva[rva_mode].set(RVAsetting::RVA2, float(short((realdata[pos + 1] << 8) | realdata[pos + 2])) / 512, 0);
						pos += 3;
					}
					break;
				}
				case 'USLT':	//Unsynchronized lyric/text transcription
					 if (realsize > 4) {
						ENCODING	enc		= (ENCODING)realdata[0];
						uint8		*descr	= realdata + 4;
						if (uint8 *txt = next_text(descr, enc, realsize - 4)) {
							text	*x	= new(texts) text;
							x->set_lang_id(realdata + 1, id);
							x->descr	= get_string(enc, (uint8*)descr, txt - descr);
							x->s		= get_string(enc, (uint8*)txt, realend - (uint8*)txt);
						}
					}
					break;

				case 'PRIV':	//Private
					if (uint8 *end = next_text(realdata, ENC_LATIN1, realsize)) {
						blob	*b	= new(blobs) blob;
						b->owner	= str((char*)realdata, end - realdata - 1);
						size_t	s	= realend - (uint8*)end;
						memcpy(b->create(s, 4), end, s);
					}
					break;

				default:
					switch (id >> 24) {
						case 'T': {	// text
							text *t = new(texts) text;
							t->id	= id;
							t->s	= get_string(ENCODING(realdata[0]), realdata + 1, realsize - 1);
							break;
						}
						case 'W': {	// url
							text *t = new(urls) text;
							t->id	= id;
							t->s	= get_string(ENC_LATIN1, realdata, realsize);
							break;
						}
						default:
							ISO_TRACEF("Unhandled id3 tag:%c%c%c%c\n", id >> 24, id >> 16, id >> 8, id);
					}
					break;
			}
		}
		free(tagdata);
	}
	// skip footer if present
	if (ret && (flags & HF_FOOTER))
		file.seek_cur(length);

	return ret;
}

//-----------------------------------------------------------------------------
//	mpg123_pars
//-----------------------------------------------------------------------------

const int mpg123_pars::rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

const ENCODING	mpg123_pars::encodings[] = {
	ENC_16|ENC_FIXED|ENC_SIGNED,
	ENC_32|ENC_FLOAT|ENC_SIGNED,
};

ENCODING mpg123_pars::cap_fit(int chans, int rate) const {
	int ir = rate2num(rate);
	if (ir >= 0) {
		uint16	caps = audio_caps[chans - 1][ir];
		for (int i = 0; i < num_elements(encodings); ++i) {
			if ((encodings[i] & caps) == encodings[i])
				return encodings[i];
		}
	}
	return ENC_NONE;
}

int mpg123_pars::set_format(audioformat &af, int chans, int rate) const {
	if (_PARAM_FORCE_MIX)
		chans = params.test(_PARAM_FORCE_MONO) ? 1 : 2;

	ENCODING	enc;
	if (!(enc = cap_fit(chans, rate))) {
		if (params.test(_PARAM_FORCE_MIX))
			return -1;
		chans = 3 - chans;
		if (!(enc = cap_fit(chans, rate)))
			return -1;
	}
	if (rate == af.rate && chans == af.channels && enc == af.encoding)
		return 0;

	return af.set(enc, rate, chans) ? 1 : -1;
}

//-----------------------------------------------------------------------------
//	decode_tables
//-----------------------------------------------------------------------------

void decode_tables::set(float _scale, bool force) {
	static const int base[] = {
		    0,    -1,    -1,    -1,    -1,    -1,    -1,    -2,    -2,    -2,    -2,    -3,    -3,    -4,    -4,    -5,
		   -5,    -6,    -7,    -7,    -8,    -9,   -10,   -11,   -13,   -14,   -16,   -17,   -19,   -21,   -24,   -26,
		  -29,   -31,   -35,   -38,   -41,   -45,   -49,   -53,   -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97,
		 -104,  -111,  -117,  -125,  -132,  -139,  -147,  -154,  -161,  -169,  -176,  -183,  -190,  -196,  -202,  -208,
		 -213,  -218,  -222,  -225,  -227,  -228,  -228,  -227,  -224,  -221,  -215,  -208,  -200,  -189,  -177,  -163,
		 -146,  -127,  -106,   -83,   -57,   -29,     2,    36,    72,   111,   153,   197,   244,   294,   347,   401,
		  459,   519,   581,   645,   711,   779,   848,   919,   991,  1064,  1137,  1210,  1283,  1356,  1428,  1498,
		 1567,  1634,  1698,  1759,  1817,  1870,  1919,  1962,  2001,  2032,  2057,  2075,  2085,  2087,  2080,  2063,
		 2037,  2000,  1952,  1893,  1822,  1739,  1644,  1535,  1414,  1280,  1131,   970,   794,   605,   402,   185,
		  -45,  -288,  -545,  -814, -1095, -1388, -1692, -2006, -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788,
		-5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597, -7910, -8209, -8491, -8755, -8998, -9219, -9416, -9585,
		-9727, -9838, -9916, -9959, -9966, -9935, -9863, -9750, -9592, -9389, -9139, -8840, -8492, -8092, -7640, -7134,
		-6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082,   -70,   998,  2122,  3300,  4533,  5818,  7154,  8540,
		 9975, 11455, 12980, 14548, 16155, 17799, 19478, 21189, 22929, 24694, 26482, 28289, 30112, 31947, 33791, 35640,
		37489, 39336, 41176, 43006, 44821, 46617, 48390, 50137, 51853, 53534, 55178, 56778, 58333, 59838, 61289, 62684,
		64019, 65290, 66494, 67629, 68692, 69679, 70590, 71420, 72169, 72835, 73415, 73908, 74313, 74630, 74856, 74992,
		75038
	};
	if (_scale != scale || force) {
		scale = _scale;
		float	s = _scale / 65536;
		for (int i = 0; i < 512; i++) {
			int		j		= i < 256 ? i : 512 - i;
			int		idx		= (i & 31) * 32 + (i >> 5);
			float	sign	= i & 64 ? 1 : -1;
			if (idx < 512 + 16)
				table[idx + 16] = table[idx] = base[j] * s * sign;
		}
	}
}

//-----------------------------------------------------------------------------
//	mpg123
//-----------------------------------------------------------------------------

mpg123::mpg123(istream_ref _file) : file(_file), vlc(buffer)
	,state(STATE_ACCURATE | STATE_FRESH | STATE_DECODERCHANGE | STATE_OWNBUFFER)
	,to_decode(false)
{
	static bool initialised = false;
	if (!initialised) {
		init();
		initialised = true;
	}

	bsnum			= 0;
	num				= -1;
	ff_framesize	= maximum;
	framesize		= 0;
	audio_start		= 0;
	clip			= 0;
	bo				= 1;

	float	*p		= rawbuffs.create(4 * 0x110 * sizeof(float), 16);
	memset(p, 0, 4 * 0x110 * sizeof(float));
	real_buffs[0][0] = p + 0x110 * 0;
	real_buffs[0][1] = p + 0x110 * 1;
	real_buffs[1][0] = p + 0x110 * 2;
	real_buffs[1][1] = p + 0x110 * 3;
}

size_t mpg123::set_header(mpg_header nh, int *ff_count) {
	size_t	size  = 0;

	if (nh.check()) {
		if (nh.freeformat()) {
			state.set(STATE_FREEFORMAT);
			if (ff_framesize < 0) {
				if (++*ff_count > 5)
					return false;

				streamptr	save	= file.tell();
				uint32		head	= file.get();
				for (uint32 i = 4; i < MAXFRAMESIZE + 4; i++) {
					head = (head << 8) | file.getc();
					if (nh.very_similar(head)) {
						size = i - 3;
						break;
					}
				}
				file.seek(save);
				if (size)
					ff_framesize = size - nh.padding;

			} else {
				size = ff_framesize + nh.padding;
			}
		} else {
			state.clear(STATE_FREEFORMAT);
			switch (nh.layer) {
				case mpg_header::LAY_I:		size = ((nh.get_bitrate() * 12000 / nh.get_samplerate() + nh.padding) << 2) - 4; break;
				case mpg_header::LAY_II:	size = nh.get_bitrate() * 144000 / nh.get_samplerate() + nh.padding - 4; break;
				case mpg_header::LAY_III:	size = (nh.get_bitrate() * 144000 / (nh.get_samplerate() << int(nh.lsf())) + nh.padding) - 4; break;
			}
		}
	}
	return size <= MAXFRAMESIZE ? size : 0;
}

// adjust the volume, taking both outscale and rva values into account
void mpg123::adjust_volume() {
	float peak		= 0;
	float rvafact	= 1;
	if (params.test(PARAM_RVA)) {
		int rt = params.test(PARAM_RVA_ALBUM) && rva[1].level != RVAsetting::NONE ? 1 : 0;
		if (rva[rt].level != RVAsetting::NONE) {
			peak	= rva[rt].peak;
			rvafact = pow(10.f, rva[rt].gain / 20.f);
		}
	}

	float newscale = outscale * rvafact;
	if (peak * newscale > 1)
		newscale = 1 / peak;

	decode.set(newscale, state.test(STATE_DECODERCHANGE));
}

mpg123::RETURNVALS mpg123::read_frame() {
	int			ff_count	= 0;
	bool		reread		= true;
	mpg_header	nh;

	for (;;) {
		if (reread) {
			uint32be	u;
			if (!file.read(u))
				return RET_DONE;
			nh = uint32(u);
			reread	= false;
		}

		if (size_t size = set_header(nh, &ff_count)) {
			streamptr	fp	= file.tell() - 4;
			uint8		*p	= bsspace[bsnum] + 512;
			bsnum		= 1 - bsnum;
			file.readbuff(p, size);
			set_pointer(p);

			if (firsthead == 0) {
				uint32	nexthead = file.get<uint32be>();
				if (!mpg_header(nexthead).check() || !nh.similar(nexthead)) {
					file.seek(fp + 1);
					reread	= true;
					continue;
				}

				file.seek_cur(-4);
				firsthead = nh;
				if (num < 0) {
					audio_start = fp;
					if (nh.layer == mpg_header::LAY_III && !params.test(PARAM_SKIP_LAME) && lame.read(buffer.p, size, h.get_offset(), rva)) {
						if (params.test(PARAM_GAPLESS))
							gapless_info.set_range(lame.skipbegin, lame.track_frames * size - lame.skipend);
						continue;
					}
				}
			}

			if (h.samplerate != nh.samplerate || h.mono() != nh.mono())
				state.set(STATE_DECODERCHANGE);

			h			= nh;
			framesize	= size;
			return RET_OK;

		} else {
			uint32	head = nh;
			if (head >> 8 == 'TAG') {			// id3v1
				id3v1.read(file, head);
				reread	= true;

			} else if ((head >> 8) == 'ID3') {	// id3v2
				if (!params.test(PARAM_SKIP_ID3V2))
					id3v2.read(file, head, rva);
				reread	= true;

			} else if (!params.test(PARAM_NO_RESYNC)) {
				vlc.reset();
				bool	gothead = false;
				for (uint32 limit = resync_limit; !gothead && --limit; ) {
					nh		= (head = (head << 8) | file.getc());
					gothead = nh.check();
				}
				if (!gothead)
					return RET_RESYNC_FAIL;

			} else {
				return RET_OUT_OF_SYNC;
			}
		}
	}
}

mpg123::RETURNVALS mpg123::init_decoder() {
	// Select the new output format based on given constraints
	if (int b = set_format(af, h.mono() ? 1 : 2, h.get_samplerate())) {
		if (b < 0)
			return RET_BAD_OUTFORMAT;

		state.set(STATE_FORMATCHANGE);
	}

	mix	= params.test(PARAM_MONO_LEFT)						? MIX_LEFT
		: params.test(PARAM_MONO_RIGHT)						? MIX_RIGHT
		: params.test(PARAM_MONO_MIX) || af.channels == 1	? MIX_MIX
		: MIX_STEREO;

	size_t	outsize	= af.samples_to_storage(h.get_numsamples());
	if (!state.test(STATE_OWNBUFFER) && out.length() < outsize)
		return RET_BAD_BUFFER;

	out.create(outsize, 16);
	layerscratch.clear_contents();
	adjust_volume();
	return RET_OK;
}

mpg123::RETURNVALS mpg123::decode_frame(int64 *outnum, void **outdata, size_t *outlen) {
	if (outlen)
		*outlen = 0;
	if (outdata)
		*outdata = out;

	out.fill = 0;
	for (;;) {
		if (to_decode) {
			if (state.test_clear(STATE_FORMATCHANGE))
				return RET_NEW_FORMAT;

			if (outnum)
				*outnum = num;

			clip += do_layer();

			size_t nsamp = h.get_numsamples();
			if (size_t n = af.samples_to_storage(nsamp) - out.fill)
				memset(out.add(n), 0, n);

			to_decode = false;

			if (state.test(STATE_ACCURATE) && params.test(PARAM_GAPLESS)) {
				if (size_t o = gapless_info.buffer_adjust(num, nsamp))
					memcpy((char*)out, (char*)out + af.samples_to_bytes(o), out.fill = af.samples_to_bytes(nsamp));
			}

			if (outlen)
				*outlen = out.fill;
			return RET_OK;
		}

		do {
			if (gapless_info.preframe(num , h.layer == mpg_header::LAY_III ? max(preframes, 1) : min(preframes, 2))) {
				do_layer();
				out.fill	= 0;
			}
			if (RETURNVALS b = read_frame())
				return b;

			++num;
			if (lame.track_frames && num >= lame.track_frames)
				state.set(STATE_FRANKENSTEIN);

			if (h.error_prot())
				crc = vlc.get(16); // skip crc
		} while (num < gapless_info.firstframe);

		to_decode = true;

		if (state.test_clear(STATE_DECODERCHANGE)) {
			if (RETURNVALS b = init_decoder())
				return b;
			if (state.test_clear(STATE_FRESH))
				gapless_info.frameseek(num, h.get_numsamples());
		}
	}
}

//-----------------------------------------------------------------------------
//	DCT routines
//-----------------------------------------------------------------------------

bool close(float a, float b, float part=1000) {
	return iso::abs(a - b) * part <= iso::abs(a + b);
}

//-----------------------------------------------------------------------------
//	synth
//-----------------------------------------------------------------------------

struct rand_xorshift32 {
	static const uint32 init_seed = 2463534242UL;
	uint32	seed;

	rand_xorshift32()	{ seed = init_seed; }
	void	reset()		{ seed = init_seed; }
	uint32	next()		{ seed ^= seed << 13; seed ^= seed >> 17; return seed ^= seed << 5; }
	operator float()	{ return iorf((next() >> 9) | 0x3f800000).f() - 1.5f;	}
};

void make_white_noise(float *table, size_t count) {
	rand_xorshift32	r;
	for (size_t i = 0; i < count; ++i)
		table[i] = r;
}

void make_tpdf_noise(float *table, size_t count) {
	rand_xorshift32	r;
	for (size_t i = 0; i < count; ++i)
		table[i] = r + r;
}

void make_highpass_tpdf_noise(float *table, size_t count, size_t lap) {
	rand_xorshift32	r;

	float	xv[9], yv[9];
	clear(xv);
	clear(yv);

	lap	= min(lap, count / 2);	// Ensure some minimum lap for keeping the high-pass filter circular.
	for (int i = 0; i < count + lap; i++) {
		if (i == count)
			r.reset();

		float	input_noise = r + r;	// generate and add 2 random numbers, to make a TPDF noise distribution

		// apply 8th order Chebyshev high-pass IIR filter
		// Coefficients are from http://www-users.cs.york.ac.uk/~fisher/mkfilter/trad.html
		// Given parameters are: Chebyshev, Highpass, ripple=-1, order=8, samplerate=44100, corner1=19000
		xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4]; xv[4] = xv[5]; xv[5] = xv[6]; xv[6] = xv[7]; xv[7] = xv[8];
		xv[8] = input_noise / 1.382814179e+07;
		yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4]; yv[4] = yv[5]; yv[5] = yv[6]; yv[6] = yv[7]; yv[7] = yv[8];
		yv[8] =	   (xv[0] + xv[8])
			-  8 * (xv[1] + xv[7])
			+ 28 * (xv[2] + xv[6])
			- 56 * (xv[3] + xv[5]) + 70 * xv[4]
			+  -0.6706204984 * yv[0] +  -5.3720827038 * yv[1]
			+ -19.0865382480 * yv[2] + -39.2831607860 * yv[3]
			+ -51.2308985070 * yv[4] + -43.3590135780 * yv[5]
			+ -23.2632305320 * yv[6] +  -7.2370122050 * yv[7];
		if (i >= lap)
			table[i - lap] = yv[8] * 3;
	}
}

void dither_filter::init(type t) {
	buffer	= (float*)malloc(DITHERSIZE * sizeof(float));
	switch (t) {
		case white_noise:			make_white_noise(buffer, DITHERSIZE); break;
		case tpdf_noise:			make_tpdf_noise(buffer, DITHERSIZE); break;
		case highpass_tpdf_noise:	make_highpass_tpdf_noise(buffer, DITHERSIZE, 100); break;
	}
}

template<class O, typename T, typename FILTER> void synth0(O out, const T *window1, const T *window2, T *b0, FILTER &filter) {
	static const int BLOCK = 64;
	for (int j = BLOCK / 4; j--; b0 += 0x400 / BLOCK, window1 += 0x800 / BLOCK)
		out.put(filter(
			  window1[0x0] * b0[0x0]
			- window1[0x1] * b0[0x1]
			+ window1[0x2] * b0[0x2]
			- window1[0x3] * b0[0x3]
			+ window1[0x4] * b0[0x4]
			- window1[0x5] * b0[0x5]
			+ window1[0x6] * b0[0x6]
			- window1[0x7] * b0[0x7]
			+ window1[0x8] * b0[0x8]
			- window1[0x9] * b0[0x9]
			+ window1[0xA] * b0[0xA]
			- window1[0xB] * b0[0xB]
			+ window1[0xC] * b0[0xC]
			- window1[0xD] * b0[0xD]
			+ window1[0xE] * b0[0xE]
			- window1[0xF] * b0[0xF]
		));

	out.put(filter(
		  window1[0x0] * b0[0x0]
		+ window1[0x2] * b0[0x2]
		+ window1[0x4] * b0[0x4]
		+ window1[0x6] * b0[0x6]
		+ window1[0x8] * b0[0x8]
		+ window1[0xA] * b0[0xA]
		+ window1[0xC] * b0[0xC]
		+ window1[0xE] * b0[0xE]
	));
	b0		-= 0x400 / BLOCK;

	for (int j = BLOCK / 4 - 1; j--; b0 -= 0x400 / BLOCK, window2 -= 0x800 / BLOCK)
		out.put(filter(
			- window2[-0x1] * b0[0x0]
			- window2[-0x2] * b0[0x1]
			- window2[-0x3] * b0[0x2]
			- window2[-0x4] * b0[0x3]
			- window2[-0x5] * b0[0x4]
			- window2[-0x6] * b0[0x5]
			- window2[-0x7] * b0[0x6]
			- window2[-0x8] * b0[0x7]
			- window2[-0x9] * b0[0x8]
			- window2[-0xA] * b0[0x9]
			- window2[-0xB] * b0[0xA]
			- window2[-0xC] * b0[0xB]
			- window2[-0xD] * b0[0xC]
			- window2[-0xE] * b0[0xD]
			- window2[-0xF] * b0[0xE]
			- window2[-0x0] * b0[0xF] // Is that right? 0x0? Just wondering...
		));
}

template<typename T, int STEP> struct sample_out;

template<int STEP> struct sample_out<float, STEP> {
	fixed_stride_iterator<float,STEP>	p;
	int		clip;
	sample_out(float *_p) : p(_p), clip(0) {}
	inline void	put(float f) { *p++ = f; }
};
template<int STEP> struct sample_out<int8, STEP> {
	fixed_stride_iterator<int8,STEP>	p;
	int		clip;
	sample_out(int8 *_p) : p(_p), clip(0) {}
	inline int min(int a, int b) { clip += a > b; return a > b ? b : a; }
	inline void	put(float f) {
		*p	= f < 0
			? -min(iorf(65536 - f).m, 0x80)
			:  min(iorf(65536 + f).m, 0x7f);
		p += STEP;
	}
};
template<int STEP> struct sample_out<int16, STEP> {
	fixed_stride_iterator<int16,STEP>	p;
	int		clip;
	sample_out(int16 *_p) : p(_p), clip(0) {}
	inline int min(int a, int b) { clip += a > b; return a > b ? b : a; }
	inline void	put(float f) {
		*p++	= f < 0
			? -min(iorf(256 - f).m, 0x8000)
			:  min(iorf(256 + f).m, 0x7fff);
	}
};
template<int STEP> struct sample_out<int32, STEP> {
	fixed_stride_iterator<int32,STEP>	p;
	int		clip;
	sample_out(int32 *_p) : p(_p), clip(0) {}
	inline float min(float a, float b) { clip += a > b; return a > b ? b : a; }
	inline void	put(float f) {
		*p++	= (f < 0
			? -iorf(min(1 - f, 2)).m
			:  iorf(min(f + 1, 2)).m
		) << 7;
	}
};
template<int STEP> struct sample_out<alaw, STEP> {
	fixed_stride_iterator<alaw,STEP>	p;
	int		clip;
	sample_out(int32 *_p) : p(_p), clip(0) {}
	inline void	put(float f) {
		*p++	= int16(f > 32767 ? (++clip, 32767) : f < -32768 ? (++clip, -32768) : f);
	}
};
template<int STEP> struct sample_out<ulaw, STEP> {
	fixed_stride_iterator<ulaw,STEP>	p;
	int		clip;
	sample_out(int32 *_p) : p(_p), clip(0) {}
	inline void	put(float f) {
		*p++	= int16(f > 32767 ? (++clip, 32767) : f < -32768 ? (++clip, -32768) : f);
	}
};

int mpg123::synth_mono(float *bandPtr) {
	if (params.test(PARAM_EQUALIZER)) {
		for (int i = 0; i < 32; i++)
			bandPtr[i] = bandPtr[i] * equalizer[0][i];
	}

	bo	= (bo - 1) & 0xf;

	int		bo0		= (bo + 1) & 0xe;
	int		bo1		= bo | 1;
	float	*b0		= real_buffs[0][bo & 1];
	float	*b1		= real_buffs[0][~bo & 1];

	mdct64<float>::process(bandPtr, b0 + bo0, b1 + bo1);
	sample_out<int16, 1>	samples((int16*)out.add(32 * sizeof(int16)));

	if (params.test(PARAM_DITHER)) {
		dither.need(32);
		synth0(samples, decode + 0x10 - bo1, decode + 0x1f0 + bo1, b0, dither);
	} else {
		null_filter n;
		synth0(samples, decode + 0x10 - bo1, decode + 0x1f0 + bo1, b0, n);
	}
	return samples.clip;
}

int mpg123::synth_stereo(float *bandPtr_l, float *bandPtr_r) {
	if (params.test(PARAM_EQUALIZER)) {
		for (int i = 0; i < 32; i++) {
			bandPtr_l[i] = bandPtr_l[i] * equalizer[0][i];
			bandPtr_r[i] = bandPtr_r[i] * equalizer[1][i];
		}
	}

	bo = (bo - 1) & 0xf;
	int		bo0		= (bo + 1) & 0xe;
	int		bo1		= bo | 1;
	float	*b0		= real_buffs[0][bo & 1];
	float	*b1		= real_buffs[0][~bo & 1];

	mdct64<float>::process(bandPtr_l, b0 + bo0, b1 + bo1);

	int16*	s		= (int16*)out.add(32 * 2 * sizeof(int16));
	sample_out<int16, 2>	samples(s);

	if (params.test(PARAM_DITHER)) {
		int	di	= dither.need(32);
		synth0(samples, decode + 0x10 - bo1, decode + 0x1f0 + bo1, b0, dither);
		b0		= real_buffs[1][bo & 1];
		b1		= real_buffs[1][~bo & 1];
		mdct64<float>::process(bandPtr_r, b0 + bo0, b1 + bo1);
		dither.set(di);
		samples.p = s + 1;
		synth0(samples, decode + 0x10 - bo1, decode + 0x1f0 + bo1, b0, dither);
	} else {
		null_filter n;
		synth0(samples, decode + 0x10 - bo1, decode + 0x1f0 + bo1, b1, n);
		b0		= real_buffs[1][bo & 1];
		b1		= real_buffs[1][~bo & 1];
		mdct64<float>::process(bandPtr_r, b0 + bo0, b1 + bo1);
		samples.p = s + 1;
		synth0(samples, decode + 0x10 - bo1, decode + 0x1f0 + bo1, b1, n);
	}
	return samples.clip;
}

//-----------------------------------------------------------------------------
//	Layer12 common
//-----------------------------------------------------------------------------

struct Layer12 {
	enum {SCALE_BLOCK = 12};
	struct al_table { int16 bits, d; };

	static const al_table	alloc_0[], alloc_1[], alloc_2[], alloc_3[], alloc_4[];
	static int				grp_3tab[32 * 3], grp_5tab[128 * 3], grp_9tab[1024 * 3];
	static const float		mulmul[27];

	struct scratch {
		float	muls[27][64];
	};

	static void init_scratch(scratch *s);
	static void	init();
};

const Layer12::al_table Layer12::alloc_0[] = {
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767}
};
const Layer12::al_table Layer12::alloc_1[] = {
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767}
};
const Layer12::al_table Layer12::alloc_2[] = {
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63}
};
const Layer12::al_table Layer12::alloc_3[] = {
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63}
};
const Layer12::al_table Layer12::alloc_4[] = {
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9}
};

int Layer12::grp_3tab[32 * 3], Layer12::grp_5tab[128 * 3], Layer12::grp_9tab[1024 * 3];

const float Layer12::mulmul[27] = {
	+0,			-2/3.f,		+2/3.f,
	+2/7.f,		+2/15.f,	+2/31.f,	2/63.f,		2/127.f,	2/255.f,
	+2/511.f,	+2/1023.f,	+2/2047.f,	2/4095.f,	2/8191.f,
	+2/16383.f,	+2/32767.f,	+2/65535.f,
	-4/5.f,		-2/5.f,		+2/5.f,		4/5.f,
	-8/9.f,		-4/9.f,		-2/9.f,		2/9.f,		4/9.f,		8/9.f
};

void Layer12::init() {
	static int base3[] = {1 , 0, 2};
	static int base5[] = {17, 18, 0, 19, 20};
	static int base9[] = {21, 1, 22, 23, 0, 24, 25, 2, 26};

	struct { int len, *table, *base; } tables[3] = {
		{3, grp_3tab, base3},
		{5, grp_5tab, base5},
		{9, grp_9tab, base9},
	};

	for (int i = 0; i < 3; i++) {
		int	*table	= tables[i].table;
		int	*base	= tables[i].base;
		int	len		= tables[i].len;
		for (int j = 0; j < len; j++) {
			for (int k = 0; k < len; k++) {
				for (int l = 0; l < len; l++) {
					*table++ = base[l];
					*table++ = base[k];
					*table++ = base[j];
				}
			}
		}
	}
}

void Layer12::init_scratch(scratch *s) {
	for (int k = 0; k < 27; k++) {
		for (int j = 3, i = 0; i < 63; i++, j--)
			s->muls[k][i] = mulmul[k] * iso::pow(2.0, j / 3.0);
	}
}

//-----------------------------------------------------------------------------
//	Layer1
//-----------------------------------------------------------------------------

struct Layer1 : Layer12 {
	uint32	bit_alloc[2 * SBLIMIT];
	uint32	scale_index[2 * SBLIMIT];
	int		jsbound;

	struct scratch : Layer12::scratch {
		float	fraction[2][SBLIMIT];
	} *s;

	Layer1(mpg123 *fr);

	void	step_one(vlc_reader &vlc, int channels);
	void	step_two(vlc_reader &vlc, int channels);
	int		do_layer(mpg123 *fr);
};

Layer1::Layer1(mpg123 *fr) {
	jsbound = fr->h.joint_stereo() ? (fr->h.chanex << 2) + 4 : 32;
	s		= fr->layerscratch;
	if (!s) {
		s = fr->layerscratch.create(sizeof(scratch), 4);
		clear(s->fraction);
		init_scratch(s);
	}
}

void Layer1::step_one(vlc_reader &vlc, int channels) {
	uint32	*ba		= bit_alloc;
	uint32	*sca	= scale_index;

	if (channels == 2) {
		for (int i = 0; i < jsbound; i++) {
			*ba++ = vlc.get(4);
			*ba++ = vlc.get(4);
		}
		for (int i = jsbound; i < SBLIMIT; i++)
			*ba++ = vlc.get(4);

		ba = bit_alloc;

		for (int i = 0; i < jsbound; i++) {
			if (*ba++)
				*sca++ = vlc.get(6);
			if (*ba++)
				*sca++ = vlc.get(6);
		}
		for (int i = jsbound; i < SBLIMIT; i++) {
			if (*ba++) {
				*sca++ = vlc.get(6);
				*sca++ = vlc.get(6);
			}
		}
	} else {
		for (int i = 0; i < SBLIMIT; i++)
			*ba++ = vlc.get(4);

		ba = bit_alloc;
		for (int i = 0; i < SBLIMIT; i++) {
			if (*ba++)
				*sca++ = vlc.get(6);
		}
	}
}

void Layer1::step_two(vlc_reader &vlc, int channels) {
	int		smpb[2 * SBLIMIT], *sample = smpb;
	uint32	*ba		= bit_alloc;
	uint32	*sca	= scale_index;

	if (channels == 2) {
		for (int i = 0; i < jsbound; i++) {
			if (int n = *ba++)
				*sample++ = vlc.get(n + 1);
			if (int n = *ba++)
				*sample++ = vlc.get(n + 1);
		}
		for (int i = jsbound; i < SBLIMIT; i++) {
			if (int n = *ba++)
				*sample++ = vlc.get(n + 1);
		}

		float	*f0	= s->fraction[0];
		float	*f1	= s->fraction[1];
		ba			= bit_alloc;
		sample		= smpb;
		for (int i = 0; i < jsbound; i++) {
			if (int n = *ba++)
				*f0++ = ((-1 << n) + *sample++ + 1) * s->muls[n + 1][*sca++];
			else
				*f0++ = 0;

			if (int n = *ba++)
				*f1++ = ((-1 << n) + *sample++ + 1) * s->muls[n + 1][*sca++];
			else
				*f1++ = 0;
		}
		for (int i = jsbound; i < SBLIMIT; i++) {
			if (int n = *ba++) {
				float samp = (-1 << n) + *sample++ + 1;
				*f0++ = samp * s->muls[n + 1][*sca++];
				*f1++ = samp * s->muls[n + 1][*sca++];
			} else {
				*f0++ = *f1++ = 0;
			}
		}
	} else {
		for (int i = 0; i < SBLIMIT; i++) {
			if (int n = *ba++)
				*sample++ = vlc.get(n + 1);
		}

		float	*f0	= s->fraction[0];
		ba			= bit_alloc;
		sample		= smpb;
		for (int i = 0; i < SBLIMIT; i++) {
			if (int n = *ba++)
				*f0++ = ((-1 << n) + *sample++ + 1) * s->muls[n + 1][*sca++];
			else
				*f0++ = 0;
		}
	}
}

int Layer1::do_layer(mpg123 *fr) {
	int	channels = fr->h.mono() ? 1 : 2;
	step_one(fr->vlc, channels);

	mpg123::MIX	mix = fr->mix;
	if (channels == 1 || mix == mpg123::MIX_MIX)
		mix = mpg123::MIX_LEFT;

	int		clip = 0;
	for (int i = 0; i < SCALE_BLOCK; i++) {
		step_two(fr->vlc, channels);
		if (mix != mpg123::MIX_STEREO)
			clip += fr->synth_mono(s->fraction[mix]);
		else
			clip += fr->synth_stereo(s->fraction[0], s->fraction[1]);
	}
	return clip;
}

//-----------------------------------------------------------------------------
//	Layer2
//-----------------------------------------------------------------------------

struct Layer2 : Layer12 {
	uint32	bit_alloc[64];
	uint32	scale_index[192];
	int		jsbound;
	int		sblimit;
	const	al_table *alloc;

	struct scratch : Layer12::scratch {
		float	fraction[2][4][SBLIMIT];
	} *s;

	Layer2(mpg123 *fr);

	void	step_one(vlc_reader &vlc, int channels);
	void	step_two(vlc_reader &vlc, int channels, int x1);
	int		do_layer(mpg123 *fr);
};

Layer2::Layer2(mpg123 *fr) {
	static const int		translate[3][2][16] = { {
		{0,2,2,2,2,2,2,0,0,0,1,1,1,1,1,0}, {0,2,2,0,0,0,1,1,1,1,1,1,1,1,1,0}
	}, {
		{0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0}, {0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0}
	}, {
		{0,3,3,3,3,3,3,0,0,0,1,1,1,1,1,0}, {0,3,3,0,0,0,1,1,1,1,1,1,1,1,1,0}
	} };
	static const al_table*	tables[5] = { alloc_0, alloc_1, alloc_2, alloc_3 , alloc_4 };
	static const int		sblims[5] = { 27 , 30 , 8, 12 , 30 };

	s		= fr->layerscratch;
	if (!s) {
		s = fr->layerscratch.create(sizeof(scratch), 4);
		clear(s->fraction);
		init_scratch(s);
	}

	int		table	= fr->h.samplerate >= 3 ? 4 : translate[fr->h.samplerate][fr->h.mono()][fr->h.bitrate];
	alloc	= tables[table];
	sblimit	= sblims[table];
	jsbound = fr->h.joint_stereo() ? (fr->h.chanex << 2) + 4 : sblimit;
	if (jsbound > sblimit)
		jsbound = sblimit;
}

int Layer2::do_layer(mpg123 *fr) {
	int			channels	= fr->h.mono() ? 1 : 2;
	mpg123::MIX	mix			= fr->mix;
	if (channels == 1 || mix == mpg123::MIX_MIX)
		mix = mpg123::MIX_LEFT;

	step_one(fr->vlc, channels);

	int		clip = 0;
	for (int i = 0; i < SCALE_BLOCK; i++) {
		step_two(fr->vlc, channels, i >> 2);
		for (int j = 0; j < 3; j++) {
			if (mix != mpg123::MIX_STEREO)
				clip += fr->synth_mono(s->fraction[mix][j]);
			else
				clip += fr->synth_stereo(s->fraction[0][j], s->fraction[1][j]);
		}
	}
	return clip;
}

void Layer2::step_one(vlc_reader &vlc, int channels) {
	uint32	scfsi_buf[64], *scfsi = scfsi_buf;	// Scalefactor select information
	int		sblimit2	= sblimit;
	uint32	*ba			= bit_alloc;

	if (channels == 2) {
		sblimit2 <<= 1;

		for (int i = jsbound; i--; ba += 2) {
			int	step	= alloc->bits;
			ba[0]		= vlc.get(step);
			ba[1]		= vlc.get(step);
			alloc		+= uint32(1 << step);
		}
		for (int i = sblimit - jsbound; i--; ba += 2) {
			int	step	= alloc->bits;
			ba[1]		= ba[0] = vlc.get(step);
			alloc		+= uint32(1 << step);
		}

		ba	= bit_alloc;
		for (int i = sblimit2; i--;) {
			if (*ba++)
				*scfsi++ = vlc.get(2);
		}
	} else {// mono
		for (int i = sblimit; i--;) {
			int	step	= alloc->bits;
			*ba++		= vlc.get(step);
			alloc		+= uint32(1 << step);
		}

		ba	= bit_alloc;
		for (int i = sblimit; i--;) {
			if (*ba++)
				*scfsi++ = vlc.get(2);
		}
	}

	uint32 *sca	= scale_index;
	ba			= bit_alloc;
	scfsi		= scfsi_buf;
	for (int i = sblimit2; i; i--) {
		if (*ba++) {
			switch(*scfsi++) {
				case 0:
					sca[0] = vlc.get(6);
					sca[1] = vlc.get(6);
					sca[2] = vlc.get(6);
					break;
				case 1 :
					sca[1] = sca[0] = vlc.get(6);
					sca[2] = vlc.get(6);
					break;
				case 2:
					sca[2] = sca[1] = sca[0] = vlc.get(6);
					break;
				default:
					sca[0] = vlc.get(6);
					sca[2] = sca[1] = vlc.get(6);
					break;
			}
			sca += 3;
		}
	}
}

void Layer2::step_two(vlc_reader &vlc, int channels, int x1) {
	uint32	*ba		= bit_alloc;
	uint32	*sca	= scale_index;

	for (int i = 0; i < jsbound; i++) {
		int	step = alloc->bits;
		for (int j = 0; j < channels; j++) {
			if (int b = *ba++) {
				const al_table	*alloc2 = alloc + b;
				int		k	= alloc2->bits;
				int		d1	= alloc2->d;
				if (d1 < 0) {
					float cm = s->muls[k][sca[x1]];
					s->fraction[j][0][i] = (vlc.get(k) + d1) * cm;
					s->fraction[j][1][i] = (vlc.get(k) + d1) * cm;
					s->fraction[j][2][i] = (vlc.get(k) + d1) * cm;
				} else {
					const int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
					uint32	m	= sca[x1];
					uint32	idx	= vlc.get(k);
					uint32*	tab	= (uint32*)(table[d1] + idx + idx + idx);
					s->fraction[j][0][i] = s->muls[tab[0]][m];
					s->fraction[j][1][i] = s->muls[tab[1]][m];
					s->fraction[j][2][i] = s->muls[tab[2]][m];
				}
				sca += 3;
			} else {
				s->fraction[j][0][i] = s->fraction[j][1][i] = s->fraction[j][2][i] = 0;
			}
		}
		alloc += uint32(1 << step);
	}

	for (int i = jsbound; i < sblimit; i++) {
		int	step = alloc->bits;
		ba++;	// channel 1 and channel 2 bitalloc are the same
		if (int b = *ba++) {
			const al_table	*alloc2 = alloc + b;
			int		k	= alloc2->bits;
			int		d1	= alloc2->d;
			if (d1 < 0) {
				float cm = s->muls[k][sca[x1 + 3]];
				s->fraction[0][0][i] = vlc.get(k) + d1;
				s->fraction[0][1][i] = vlc.get(k) + d1;
				s->fraction[0][2][i] = vlc.get(k) + d1;
				s->fraction[1][0][i] = s->fraction[0][0][i] * cm;
				s->fraction[1][1][i] = s->fraction[0][1][i] * cm;
				s->fraction[1][2][i] = s->fraction[0][2][i] * cm;
				cm = s->muls[k][sca[x1]];
				s->fraction[0][0][i] = s->fraction[0][0][i] * cm;
				s->fraction[0][1][i] = s->fraction[0][1][i] * cm;
				s->fraction[0][2][i] = s->fraction[0][2][i] * cm;
			} else {
				const int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
				uint32	m1	= sca[x1];
				uint32	m2	= sca[x1 + 3];
				uint32	idx	= (uint32)vlc.get(k);
				uint32*	tab = (uint32*)(table[d1] + idx + idx + idx);
				s->fraction[0][0][i] = s->muls[tab[0]][m1];
				s->fraction[0][1][i] = s->muls[tab[1]][m1];
				s->fraction[0][2][i] = s->muls[tab[2]][m1];
				s->fraction[1][0][i] = s->muls[tab[0]][m2];
				s->fraction[1][1][i] = s->muls[tab[1]][m2];
				s->fraction[1][2][i] = s->muls[tab[2]][m2];
			}
			sca += 6;
		} else {
			s->fraction[0][0][i] = s->fraction[0][1][i] = s->fraction[0][2][i] =
			s->fraction[1][0][i] = s->fraction[1][1][i] = s->fraction[1][2][i] = 0;
		}
		alloc += uint32(1 << step);
	}

	for (int i = sblimit; i < SBLIMIT;i++) {
		for (int j = 0; j < channels; j++)
			s->fraction[j][0][i] = s->fraction[j][1][i] = s->fraction[j][2][i] = 0;
	}
}

//-----------------------------------------------------------------------------
//	Layer3
//-----------------------------------------------------------------------------

struct Layer3 {
	static float	ispow[8207];
	static float	aa_ca[8], aa_cs[8];
	static float	win0[4][36], win1[4][36];

	static float	tan1_1[16], tan2_1[16], tan1_2[16], tan2_2[16];
	static float	pow1_1[2][16], pow2_1[2][16], pow1_2[2][16], pow2_2[2][16];
	static float	gainpow2[256 + 118 + 4];
	static int		mapbuf0[9][152], mapbuf1[9][156], mapbuf2[9][44], *map[9][3][2];
	static uint32	n_slen2[512]; // MPEG 2.0 slen for 'normal' mode
	static uint32	i_slen2[256]; // MPEG 2.0 slen for intensity stereo

	struct BandInfo {
		uint16	long_idx[23];
		uint8	long_diff[22];
		uint16	short_idx[14];
		uint8	short_diff[13];
	};
	static const BandInfo band_info[9];

	struct hufftable {
		uint32		linbits;
		const short	*table;
		short		lookup(vlc_reader &vlc) const {
			const short	*p = table;
			short a;
			while ((a = *p++) < 0) {
				if (vlc.get_bit())
					p -= a;
			}
			return a;
		}
	};
	static const hufftable ht[], htc[];

	struct scratch {
		int		long_limit[9][23];
		int		short_limit[9][14];
		float	hybrid_in[2][SBLIMIT][SSLIMIT];
		float	hybrid_out[2][SSLIMIT][SBLIMIT];
		float	hybrid_block[2][2][SBLIMIT*SSLIMIT];
		int		block_index;
		uint32	bitreservoir;
		uint8	*reservoir_end;
	} *s;

	struct granule {
		int8	scfsi;	// Scalefactor select information
		uint8	block_type;
		uint16	part2_3_length;
		uint16	big_values;
		uint16	scalefac_compress;
		bool	mixed_block_flag;
		bool	preflag;
		bool	scalefac_scale;
		bool	count1table_select;
		uint8	table_select[3];
		int		maxband[3], maxbandl;
		uint32	maxb;
		uint32	region1start, region2start;
		float	*full_gain[3], *pow2gain;

		bool	read(vlc_reader &vlc, const BandInfo &bi, bool lsf, bool mpeg25, bool ms_stereo, int powdiff);
		void	get_scale_factors(vlc_reader &vlc, int *scf, bool lsf, bool i_stereo);
		int		dequantize_sample(vlc_reader &vlc, float xr[SBLIMIT][SSLIMIT], int *long_limit, int *short_limit, int *map[3][2], int *scf);
		void	i_stereo(float *in0, float *in1, int *scalefac, const BandInfo &bi, bool ms_stereo, bool lsf);
		void	antialias(float xr[SBLIMIT][SSLIMIT]);
		void	hybrid(float in[SBLIMIT][SSLIMIT], float *ts, const float *prev, float *next);
	};

	granule		granules[2][2];

	static void	init();

	Layer3(mpg123 *fr);
	void	init_scratch(scratch *s);
	int		do_layer(mpg123 *fr);
};

const Layer3::BandInfo Layer3::band_info[9] = {
	{	// MPEG 1.0
		{0,4,8,12,16,20,24,30,36,44,52,62,74, 90,110,134,162,196,238,288,342,418,576},
		{4,4,4,4,4,4,6,6,8, 8,10,12,16,20,24,28,34,42,50,54, 76,158},
		{0,4*3,8*3,12*3,16*3,22*3,30*3,40*3,52*3,66*3, 84*3,106*3,136*3,192*3},
		{4,4,4,4,6,8,10,12,14,18,22,30,56}
	},	{
		{0,4,8,12,16,20,24,30,36,42,50,60,72, 88,106,128,156,190,230,276,330,384,576},
		{4,4,4,4,4,4,6,6,6, 8,10,12,16,18,22,28,34,40,46,54, 54,192},
		{0,4*3,8*3,12*3,16*3,22*3,28*3,38*3,50*3,64*3, 80*3,100*3,126*3,192*3},
		{4,4,4,4,6,6,10,12,14,16,20,26,66}
	},	{
		{0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
		{4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102, 26},
		{0,4*3,8*3,12*3,16*3,22*3,30*3,42*3,58*3,78*3,104*3,138*3,180*3,192*3},
		{4,4,4,4,6,8,12,16,20,26,34,42,12}
	},	{ // MPEG 2.0
		{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
		{6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54 } ,
		{0,4*3,8*3,12*3,18*3,24*3,32*3,42*3,56*3,74*3,100*3,132*3,174*3,192*3} ,
		{4,4,4,6,6,8,10,14,18,26,32,42,18 }
	},	{ // Twiddling 3 values here (not just 330->332!) fixed bug 1895025
		{0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278,332,394,464,540,576},
		{6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,54,62,70,76,36 },
		{0,4*3,8*3,12*3,18*3,26*3,36*3,48*3,62*3,80*3,104*3,136*3,180*3,192*3},
		{4,4,4,6,8,10,12,14,18,24,32,44,12 }
	},	{
		{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
		{6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54 },
		{0,4*3,8*3,12*3,18*3,26*3,36*3,48*3,62*3,80*3,104*3,134*3,174*3,192*3},
		{4,4,4,6,8,10,12,14,18,24,30,40,18 }
	},	{ // MPEG 2.5
		{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
		{6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54},
		{0,12,24,36,54,78,108,144,186,240,312,402,522,576},
		{4,4,4,6,8,10,12,14,18,24,30,40,18}
	},	{
		{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
		{6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54},
		{0,12,24,36,54,78,108,144,186,240,312,402,522,576},
		{4,4,4,6,8,10,12,14,18,24,30,40,18}
	},	{
		{0,12,24,36,48,60,72,88,108,132,160,192,232,280,336,400,476,566,568,570,572,574,576},
		{12,12,12,12,12,12,16,20,24,28,32,40,48,56,64,76,90,2,2,2,2,2},
		{0, 24, 48, 72,108,156,216,288,372,480,486,492,498,576},
		{8,8,8,12,16,20,24,28,36,2,2,2,26}
	}
};

float	Layer3::ispow[8207];
float	Layer3::aa_ca[8], Layer3::aa_cs[8];
float	Layer3::win0[4][36], Layer3::win1[4][36];
float	Layer3::tan1_1[16], Layer3::tan2_1[16], Layer3::tan1_2[16], Layer3::tan2_2[16];
float	Layer3::pow1_1[2][16], Layer3::pow2_1[2][16], Layer3::pow1_2[2][16], Layer3::pow2_2[2][16];
float	Layer3::gainpow2[256 + 118 + 4];
int		Layer3::mapbuf0[9][152], Layer3::mapbuf1[9][156], Layer3::mapbuf2[9][44], *Layer3::map[9][3][2];
uint32	Layer3::n_slen2[512], Layer3::i_slen2[256];

static const short tab0[] = {
   0
};
static const short tab1[] = {
  -5,  -3,  -1,  17,   1,  16,   0
};
static const short tab2[] = {
 -15, -11,  -9,  -5,  -3,  -1,  34,   2,  18,  -1,  33,  32,  17,  -1,   1,  16,   0
};
static const short tab3[] = {
 -13, -11,  -9,  -5,  -3,  -1,  34,   2,  18,  -1,  33,  32,  16,  17,  -1,   1,   0
};
static const short tab5[] = {
 -29, -25, -23, -15,  -7,  -5,  -3,  -1,  51,  35,  50,  49,  -3,  -1,  19,   3,  -1,  48,  34,  -3,  -1,  18,  33,  -1,   2,  32,  17,  -1,   1,  16,  0
};
static const short tab6[] = {
 -25, -19, -13,  -9,  -5,  -3,  -1,  51,   3,  35,  -1,  50,  48,  -1,  19,  49,  -3,  -1,  34,   2,  18,  -3,  -1,  33,  32,   1,  -1,  17,  -1,  16,  0
};
static const short tab7[] = {
 -69, -65, -57, -39, -29, -17, -11,  -7,  -3,  -1,  85,  69,  -1,  84,  83,  -1,  53,  68,  -3,  -1,  37,  82,  21,  -5,  -1,  81,  -1,   5,  52,  -1, 80, -1,
  67,  51,  -5,  -3,  -1,  36,  66,  20,  -1,  65,  64, -11,  -7,  -3,  -1,   4,  35,  -1,  50,   3,  -1,  19,  49,  -3,  -1,  48,  34,  18,  -5,  -1, 33, -1,
   2,  32,  17,  -1,   1,  16,   0
};
static const short tab8[] = {
 -65, -63, -59, -45, -31, -19, -13,  -7,  -5,  -3,  -1,  85,  84,  69,  83,  -3,  -1,  53,  68,  37,  -3,  -1,  82,   5,  21,  -5,  -1,  81,  -1,  52, 67,  -3,
  -1,  80,  51,  36,  -5,  -3,  -1,  66,  20,  65,  -3,  -1,   4,  64,  -1,  35,  50,  -9,  -7,  -3,  -1,  19,  49,  -1,   3,  48,  34,  -1,   2,  32, -1,  18,
  33,  17,  -3,  -1,   1,  16,   0
};
static const short tab9[] = {
 -63, -53, -41, -29, -19, -11,  -5,  -3,  -1,  85,  69,  53,  -1,  83,  -1,  84,   5,  -3,  -1,  68,  37,  -1,  82,  21,  -3,  -1,  81,  52,  -1,  67, -1,  80,
   4,  -7,  -3,  -1,  36,  66,  -1,  51,  64,  -1,  20,  65,  -5,  -3,  -1,  35,  50,  19,  -1,  49,  -1,   3,  48,  -5,  -3,  -1,  34,   2,  18,  -1, 33,  32,
  -3,  -1,  17,   1,  -1,  16,   0
};
static const short tab10[] = {
-125,-121,-111, -83, -55, -35, -21, -13,  -7,  -3,  -1, 119, 103,  -1, 118,  87,  -3,  -1, 117, 102,  71,  -3,  -1, 116,  86,  -1, 101,  55,  -9,  -3,  -1, 115,
  70,  -3,  -1,  85,  84,  99,  -1,  39, 114, -11,  -5,  -3,  -1, 100,   7, 112,  -1,  98,  -1,  69,  53,  -5,  -1,   6,  -1,  83,  68,  23, -17,  -5,  -1, 113,
  -1,  54,  38,  -5,  -3,  -1,  37,  82,  21,  -1,  81,  -1,  52,  67,  -3,  -1,  22,  97,  -1,  96,  -1,   5,  80, -19, -11,  -7,  -3,  -1,  36,  66,  -1,  51,
   4,  -1,  20,  65,  -3,  -1,  64,  35,  -1,  50,   3,  -3,  -1,  19,  49,  -1,  48,  34,  -7,  -3,  -1,  18,  33,  -1,   2,  32,  17,  -1,   1,  16,   0
};
static const short tab11[] = {
-121,-113, -89, -59, -43, -27, -17,  -7,  -3,  -1, 119, 103,  -1, 118, 117,  -3,  -1, 102,  71,  -1, 116,  -1,  87,  85,  -5,  -3,  -1,  86, 101,  55,  -1, 115,
  70,  -9,  -7,  -3,  -1,  69,  84,  -1,  53,  83,  39,  -1, 114,  -1, 100,   7,  -5,  -1, 113,  -1,  23, 112,  -3,  -1,  54,  99,  -1,  96,  -1,  68,  37, -13,
  -7,  -5,  -3,  -1,  82,   5,  21,  98,  -3,  -1,  38,   6,  22,  -5,  -1,  97,  -1,  81,  52,  -5,  -1,  80,  -1,  67,  51,  -1,  36,  66, -15, -11,  -7,  -3,
  -1,  20,  65,  -1,   4,  64,  -1,  35,  50,  -1,  19,  49,  -5,  -3,  -1,   3,  48,  34,  33,  -5,  -1,  18,  -1,   2,  32,  17,  -3,  -1,   1,  16,   0
};
static const short tab12[] = {
-115, -99, -73, -45, -27, -17,  -9,  -5,  -3,  -1, 119, 103, 118,  -1,  87, 117,  -3,  -1, 102,  71,  -1, 116, 101,  -3,  -1,  86,  55,  -3,  -1, 115,  85,  39,
  -7,  -3,  -1, 114,  70,  -1, 100,  23,  -5,  -1, 113,  -1,   7, 112,  -1,  54,  99, -13,  -9,  -3,  -1,  69,  84,  -1,  68,  -1,   6,   5,  -1,  38,  98,  -5,
  -1,  97,  -1,  22,  96,  -3,  -1,  53,  83,  -1,  37,  82, -17,  -7,  -3,  -1,  21,  81,  -1,  52,  67,  -5,  -3,  -1,  80,   4,  36,  -1,  66,  20,  -3,  -1,
  51,  65,  -1,  35,  50, -11,  -7,  -5,  -3,  -1,  64,   3,  48,  19,  -1,  49,  34,  -1,  18,  33,  -7,  -5,  -3,  -1,   2,  32,   0,  17,  -1,   1,  16
};
static const short tab13[] = {
-509,-503,-475,-405,-333,-265,-205,-153,-115, -83, -53, -35, -21, -13,  -9,  -7,  -5,  -3,  -1, 254, 252, 253, 237, 255,  -1, 239, 223,  -3,  -1, 238, 207,  -1,
 222, 191,  -9,  -3,  -1, 251, 206,  -1, 220,  -1, 175, 233,  -1, 236, 221,  -9,  -5,  -3,  -1, 250, 205, 190,  -1, 235, 159,  -3,  -1, 249, 234,  -1, 189, 219,
 -17,  -9,  -3,  -1, 143, 248,  -1, 204,  -1, 174, 158,  -5,  -1, 142,  -1, 127, 126, 247,  -5,  -1, 218,  -1, 173, 188,  -3,  -1, 203, 246, 111, -15,  -7,  -3,
  -1, 232,  95,  -1, 157, 217,  -3,  -1, 245, 231,  -1, 172, 187,  -9,  -3,  -1,  79, 244,  -3,  -1, 202, 230, 243,  -1,  63,  -1, 141, 216, -21,  -9,  -3,  -1,
  47, 242,  -3,  -1, 110, 156,  15,  -5,  -3,  -1, 201,  94, 171,  -3,  -1, 125, 215,  78, -11,  -5,  -3,  -1, 200, 214,  62,  -1, 185,  -1, 155, 170,  -1,  31,
 241, -23, -13,  -5,  -1, 240,  -1, 186, 229,  -3,  -1, 228, 140,  -1, 109, 227,  -5,  -1, 226,  -1,  46,  14,  -1,  30, 225, -15,  -7,  -3,  -1, 224,  93,  -1,
 213, 124,  -3,  -1, 199,  77,  -1, 139, 184,  -7,  -3,  -1, 212, 154,  -1, 169, 108,  -1, 198,  61, -37, -21,  -9,  -5,  -3,  -1, 211, 123,  45,  -1, 210,  29,
  -5,  -1, 183,  -1,  92, 197,  -3,  -1, 153, 122, 195,  -7,  -5,  -3,  -1, 167, 151,  75, 209,  -3,  -1,  13, 208,  -1, 138, 168, -11,  -7,  -3,  -1,  76, 196,
  -1, 107, 182,  -1,  60,  44,  -3,  -1, 194,  91,  -3,  -1, 181, 137,  28, -43, -23, -11,  -5,  -1, 193,  -1, 152,  12,  -1, 192,  -1, 180, 106,  -5,  -3,  -1,
 166, 121,  59,  -1, 179,  -1, 136,  90, -11,  -5,  -1,  43,  -1, 165, 105,  -1, 164,  -1, 120, 135,  -5,  -1, 148,  -1, 119, 118, 178, -11,  -3,  -1,  27, 177,
  -3,  -1,  11, 176,  -1, 150,  74,  -7,  -3,  -1,  58, 163,  -1,  89, 149,  -1,  42, 162, -47, -23,  -9,  -3,  -1,  26, 161,  -3,  -1,  10, 104, 160,  -5,  -3,
  -1, 134,  73, 147,  -3,  -1,  57,  88,  -1, 133, 103,  -9,  -3,  -1,  41, 146,  -3,  -1,  87, 117,  56,  -5,  -1, 131,  -1, 102,  71,  -3,  -1, 116,  86,  -1,
 101, 115, -11,  -3,  -1,  25, 145,  -3,  -1,   9, 144,  -1,  72, 132,  -7,  -5,  -1, 114,  -1,  70, 100,  40,  -1, 130,  24, -41, -27, -11,  -5,  -3,  -1,  55,
  39,  23,  -1, 113,  -1,  85,   7,  -7,  -3,  -1, 112,  54,  -1,  99,  69,  -3,  -1,  84,  38,  -1,  98,  53,  -5,  -1, 129,  -1,   8, 128,  -3,  -1,  22,  97,
  -1,   6,  96, -13,  -9,  -5,  -3,  -1,  83,  68,  37,  -1,  82,   5,  -1,  21,  81,  -7,  -3,  -1,  52,  67,  -1,  80,  36,  -3,  -1,  66,  51,  20, -19, -11,
  -5,  -1,  65,  -1,   4,  64,  -3,  -1,  35,  50,  19,  -3,  -1,  49,   3,  -1,  48,  34,  -3,  -1,  18,  33,  -1,   2,  32,  -3,  -1,  17,   1,  16,   0
};
static const short tab15[] = {
-495,-445,-355,-263,-183,-115, -77, -43, -27, -13,  -7,  -3,  -1, 255, 239,  -1, 254, 223,  -1, 238,  -1, 253, 207,  -7,  -3,  -1, 252, 222,  -1, 237, 191,  -1,
 251,  -1, 206, 236,  -7,  -3,  -1, 221, 175,  -1, 250, 190,  -3,  -1, 235, 205,  -1, 220, 159, -15,  -7,  -3,  -1, 249, 234,  -1, 189, 219,  -3,  -1, 143, 248,
  -1, 204, 158,  -7,  -3,  -1, 233, 127,  -1, 247, 173,  -3,  -1, 218, 188,  -1, 111,  -1, 174,  15, -19, -11,  -3,  -1, 203, 246,  -3,  -1, 142, 232,  -1,  95,
 157,  -3,  -1, 245, 126,  -1, 231, 172,  -9,  -3,  -1, 202, 187,  -3,  -1, 217, 141,  79,  -3,  -1, 244,  63,  -1, 243, 216, -33, -17,  -9,  -3,  -1, 230,  47,
  -1, 242,  -1, 110, 240,  -3,  -1,  31, 241,  -1, 156, 201,  -7,  -3,  -1,  94, 171,  -1, 186, 229,  -3,  -1, 125, 215,  -1,  78, 228, -15,  -7,  -3,  -1, 140,
 200,  -1,  62, 109,  -3,  -1, 214, 227,  -1, 155, 185,  -7,  -3,  -1,  46, 170,  -1, 226,  30,  -5,  -1, 225,  -1,  14, 224,  -1,  93, 213, -45, -25, -13,  -7,
  -3,  -1, 124, 199,  -1,  77, 139,  -1, 212,  -1, 184, 154,  -7,  -3,  -1, 169, 108,  -1, 198,  61,  -1, 211, 210,  -9,  -5,  -3,  -1,  45,  13,  29,  -1, 123,
 183,  -5,  -1, 209,  -1,  92, 208,  -1, 197, 138, -17,  -7,  -3,  -1, 168,  76,  -1, 196, 107,  -5,  -1, 182,  -1, 153,  12,  -1,  60, 195,  -9,  -3,  -1, 122,
 167,  -1, 166,  -1, 192,  11,  -1, 194,  -1,  44,  91, -55, -29, -15,  -7,  -3,  -1, 181,  28,  -1, 137, 152,  -3,  -1, 193,  75,  -1, 180, 106,  -5,  -3,  -1,
  59, 121, 179,  -3,  -1, 151, 136,  -1,  43,  90, -11,  -5,  -1, 178,  -1, 165,  27,  -1, 177,  -1, 176, 105,  -7,  -3,  -1, 150,  74,  -1, 164, 120,  -3,  -1,
 135,  58, 163, -17,  -7,  -3,  -1,  89, 149,  -1,  42, 162,  -3,  -1,  26, 161,  -3,  -1,  10, 160, 104,  -7,  -3,  -1, 134,  73,  -1, 148,  57,  -5,  -1, 147,
  -1, 119,   9,  -1,  88, 133, -53, -29, -13,  -7,  -3,  -1,  41, 103,  -1, 118, 146,  -1, 145,  -1,  25, 144,  -7,  -3,  -1,  72, 132,  -1,  87, 117,  -3,  -1,
  56, 131,  -1, 102,  71,  -7,  -3,  -1,  40, 130,  -1,  24, 129,  -7,  -3,  -1, 116,   8,  -1, 128,  86,  -3,  -1, 101,  55,  -1, 115,  70, -17,  -7,  -3,  -1,
  39, 114,  -1, 100,  23,  -3,  -1,  85, 113,  -3,  -1,   7, 112,  54,  -7,  -3,  -1,  99,  69,  -1,  84,  38,  -3,  -1,  98,  22,  -3,  -1,   6,  96,  53, -33,
 -19,  -9,  -5,  -1,  97,  -1,  83,  68,  -1,  37,  82,  -3,  -1,  21,  81,  -3,  -1,   5,  80,  52,  -7,  -3,  -1,  67,  36,  -1,  66,  51,  -1,  65,  -1,  20,
   4,  -9,  -3,  -1,  35,  50,  -3,  -1,  64,   3,  19,  -3,  -1,  49,  48,  34,  -9,  -7,  -3,  -1,  18,  33,  -1,   2,  32,  17,  -3,  -1,   1,  16,   0
};
static const short tab16[] = {
-509,-503,-461,-323,-103, -37, -27, -15,  -7,  -3,  -1, 239, 254,  -1, 223, 253,  -3,  -1, 207, 252,  -1, 191, 251,  -5,  -1, 175,  -1, 250, 159,  -3,  -1, 249,
 248, 143,  -7,  -3,  -1, 127, 247,  -1, 111, 246, 255,  -9,  -5,  -3,  -1,  95, 245,  79,  -1, 244, 243, -53,  -1, 240,  -1,  63, -29, -19, -13,  -7,  -5,  -1,
 206,  -1, 236, 221, 222,  -1, 233,  -1, 234, 217,  -1, 238,  -1, 237, 235,  -3,  -1, 190, 205,  -3,  -1, 220, 219, 174, -11,  -5,  -1, 204,  -1, 173, 218,  -3,
  -1, 126, 172, 202,  -5,  -3,  -1, 201, 125,  94, 189, 242, -93,  -5,  -3,  -1,  47,  15,  31,  -1, 241, -49, -25, -13,  -5,  -1, 158,  -1, 188, 203,  -3,  -1,
 142, 232,  -1, 157, 231,  -7,  -3,  -1, 187, 141,  -1, 216, 110,  -1, 230, 156, -13,  -7,  -3,  -1, 171, 186,  -1, 229, 215,  -1,  78,  -1, 228, 140,  -3,  -1,
 200,  62,  -1, 109,  -1, 214, 155, -19, -11,  -5,  -3,  -1, 185, 170, 225,  -1, 212,  -1, 184, 169,  -5,  -1, 123,  -1, 183, 208, 227,  -7,  -3,  -1,  14, 224,
  -1,  93, 213,  -3,  -1, 124, 199,  -1,  77, 139, -75, -45, -27, -13,  -7,  -3,  -1, 154, 108,  -1, 198,  61,  -3,  -1,  92, 197,  13,  -7,  -3,  -1, 138, 168,
  -1, 153,  76,  -3,  -1, 182, 122,  60, -11,  -5,  -3,  -1,  91, 137,  28,  -1, 192,  -1, 152, 121,  -1, 226,  -1,  46,  30, -15,  -7,  -3,  -1, 211,  45,  -1,
 210, 209,  -5,  -1,  59,  -1, 151, 136,  29,  -7,  -3,  -1, 196, 107,  -1, 195, 167,  -1,  44,  -1, 194, 181, -23, -13,  -7,  -3,  -1, 193,  12,  -1,  75, 180,
  -3,  -1, 106, 166, 179,  -5,  -3,  -1,  90, 165,  43,  -1, 178,  27, -13,  -5,  -1, 177,  -1,  11, 176,  -3,  -1, 105, 150,  -1,  74, 164,  -5,  -3,  -1, 120,
 135, 163,  -3,  -1,  58,  89,  42, -97, -57, -33, -19, -11,  -5,  -3,  -1, 149, 104, 161,  -3,  -1, 134, 119, 148,  -5,  -3,  -1,  73,  87, 103, 162,  -5,  -1,
  26,  -1,  10, 160,  -3,  -1,  57, 147,  -1,  88, 133,  -9,  -3,  -1,  41, 146,  -3,  -1, 118,   9,  25,  -5,  -1, 145,  -1, 144,  72,  -3,  -1, 132, 117,  -1,
  56, 131, -21, -11,  -5,  -3,  -1, 102,  40, 130,  -3,  -1,  71, 116,  24,  -3,  -1, 129, 128,  -3,  -1,   8,  86,  55,  -9,  -5,  -1, 115,  -1, 101,  70,  -1,
  39, 114,  -5,  -3,  -1, 100,  85,   7,  23, -23, -13,  -5,  -1, 113,  -1, 112,  54,  -3,  -1,  99,  69,  -1,  84,  38,  -3,  -1,  98,  22,  -1,  97,  -1,   6,
  96,  -9,  -5,  -1,  83,  -1,  53,  68,  -1,  37,  82,  -1,  81,  -1,  21,   5, -33, -23, -13,  -7,  -3,  -1,  52,  67,  -1,  80,  36,  -3,  -1,  66,  51,  20,
  -5,  -1,  65,  -1,   4,  64,  -1,  35,  50,  -3,  -1,  19,  49,  -3,  -1,   3,  48,  34,  -3,  -1,  18,  33,  -1,   2,  32,  -3,  -1,  17,   1,  16,   0
};
static const short tab24[] = {
-451,-117, -43, -25, -15,  -7,  -3,  -1, 239, 254,  -1, 223, 253,  -3,  -1, 207, 252,  -1, 191, 251,  -5,  -1, 250,  -1, 175, 159,  -1, 249, 248,  -9,  -5,  -3,
  -1, 143, 127, 247,  -1, 111, 246,  -3,  -1,  95, 245,  -1,  79, 244, -71,  -7,  -3,  -1,  63, 243,  -1,  47, 242,  -5,  -1, 241,  -1,  31, 240, -25,  -9,  -1,
  15,  -3,  -1, 238, 222,  -1, 237, 206,  -7,  -3,  -1, 236, 221,  -1, 190, 235,  -3,  -1, 205, 220,  -1, 174, 234, -15,  -7,  -3,  -1, 189, 219,  -1, 204, 158,
  -3,  -1, 233, 173,  -1, 218, 188,  -7,  -3,  -1, 203, 142,  -1, 232, 157,  -3,  -1, 217, 126,  -1, 231, 172, 255,-235,-143, -77, -45, -25, -15,  -7,  -3,  -1,
 202, 187,  -1, 141, 216,  -5,  -3,  -1,  14, 224,  13, 230,  -5,  -3,  -1, 110, 156, 201,  -1,  94, 186,  -9,  -5,  -1, 229,  -1, 171, 125,  -1, 215, 228,  -3,
  -1, 140, 200,  -3,  -1,  78,  46,  62, -15,  -7,  -3,  -1, 109, 214,  -1, 227, 155,  -3,  -1, 185, 170,  -1, 226,  30,  -7,  -3,  -1, 225,  93,  -1, 213, 124,
  -3,  -1, 199,  77,  -1, 139, 184, -31, -15,  -7,  -3,  -1, 212, 154,  -1, 169, 108,  -3,  -1, 198,  61,  -1, 211,  45,  -7,  -3,  -1, 210,  29,  -1, 123, 183,
  -3,  -1, 209,  92,  -1, 197, 138, -17,  -7,  -3,  -1, 168, 153,  -1,  76, 196,  -3,  -1, 107, 182,  -3,  -1, 208,  12,  60,  -7,  -3,  -1, 195, 122,  -1, 167,
  44,  -3,  -1, 194,  91,  -1, 181,  28, -57, -35, -19,  -7,  -3,  -1, 137, 152,  -1, 193,  75,  -5,  -3,  -1, 192,  11,  59,  -3,  -1, 176,  10,  26,  -5,  -1,
 180,  -1, 106, 166,  -3,  -1, 121, 151,  -3,  -1, 160,   9, 144,  -9,  -3,  -1, 179, 136,  -3,  -1,  43,  90, 178,  -7,  -3,  -1, 165,  27,  -1, 177, 105,  -1,
 150, 164, -17,  -9,  -5,  -3,  -1,  74, 120, 135,  -1,  58, 163,  -3,  -1,  89, 149,  -1,  42, 162,  -7,  -3,  -1, 161, 104,  -1, 134, 119,  -3,  -1,  73, 148,
  -1,  57, 147, -63, -31, -15,  -7,  -3,  -1,  88, 133,  -1,  41, 103,  -3,  -1, 118, 146,  -1,  25, 145,  -7,  -3,  -1,  72, 132,  -1,  87, 117,  -3,  -1,  56,
 131,  -1, 102,  40, -17,  -7,  -3,  -1, 130,  24,  -1,  71, 116,  -5,  -1, 129,  -1,   8, 128,  -1,  86, 101,  -7,  -5,  -1,  23,  -1,   7, 112, 115,  -3,  -1,
  55,  39, 114, -15,  -7,  -3,  -1,  70, 100,  -1,  85, 113,  -3,  -1,  54,  99,  -1,  69,  84,  -7,  -3,  -1,  38,  98,  -1,  22,  97,  -5,  -3,  -1,   6,  96,
  53,  -1,  83,  68, -51, -37, -23, -15,  -9,  -3,  -1,  37,  82,  -1,  21,  -1,   5,  80,  -1,  81,  -1,  52,  67,  -3,  -1,  36,  66,  -1,  51,  20,  -9,  -5,
  -1,  65,  -1,   4,  64,  -1,  35,  50,  -1,  19,  49,  -7,  -5,  -3,  -1,   3,  48,  34,  18,  -1,  33,  -1,   2,  32,  -3,  -1,  17,   1,  -1,  16,   0
};
const Layer3::hufftable Layer3::ht[] = {
	{0, tab0},  {0, tab1},  {0, tab2},  {0, tab3},  {0, tab0},  {0, tab5},  {0,  tab6},  {0,  tab7},
	{0, tab8},  {0, tab9},  {0, tab10}, {0, tab11}, {0, tab12}, {0, tab13}, {0,  tab0},  {0,  tab15},
	{1, tab16}, {2, tab16}, {3, tab16}, {4, tab16}, {6, tab16}, {8, tab16}, {10, tab16}, {13, tab16},
	{4, tab24}, {5, tab24}, {6, tab24}, {7, tab24}, {8, tab24}, {9, tab24}, {11, tab24}, {13, tab24}
};

static const short tab_c0[] = {
 -29, -21, -13,  -7,  -3,  -1,  11,  15,  -1,  13,  14,  -3,  -1,   7,   5,   9,  -3,  -1,   6,   3,  -1,  10,  12,  -3,  -1,   2,   1,  -1,   4,   8,   0
};
static const short tab_c1[] = {
 -15,  -7,  -3,  -1,  15,  14,  -1,  13,  12,  -3,  -1,  11,  10,  -1,   9,   8,  -7,  -3,  -1,   7,   6,  -1,   5,   4,  -3,  -1,   3,   2,  -1,   1,   0
};
const Layer3::hufftable Layer3::htc[] = { {0, tab_c0} , {0, tab_c1}};

Layer3::Layer3(mpg123 *fr) {
	s		= fr->layerscratch;
	if (!s) {
		s = fr->layerscratch.create(sizeof(scratch), 4);
		init_scratch(s);
	}
}

void Layer3::init() {
	for (int i = 0; i < 8207; i++)
		ispow[i] = iso::pow(float(i), 4/3.f);

	for (int i = -256; i < 118 + 4; i++)
		gainpow2[i + 256] = iso::pow(2.0, -0.25 * (i + 210));

	for (int i = 0; i < 8; i++) {
		const float Ci[8] = {-0.6f, -0.535f, -0.33f, -0.185f, -0.095f, -0.041f, -0.0142f, -0.0037f};
		float rsq = rsqrt(1 + square(Ci[i]));
		aa_cs[i] = rsq;
		aa_ca[i] = Ci[i] * rsq;
	}

	for (int i = 0; i < 18; i++) {
		win0[0][i +  0] = win0[1][i +  0] = half * sin(pi / 72 * (2 * (i +  0) + 1)) / cos(pi / 72 * (2 * (i +  0) + 19));
		win0[0][i + 18] = win0[3][i + 18] = half * sin(pi / 72 * (2 * (i + 18) + 1)) / cos(pi / 72 * (2 * (i + 18) + 19));
	}
	for (int i = 0; i < 6; i++) {
		win0[1][i+18] = half / cos(pi / 72 * (2 * (i + 18) + 19));
		win0[3][i+12] = half / cos(pi / 72 * (2 * (i + 12) + 19));
		win0[1][i+24] = half * sin(pi / 24 * (2 * i + 13)) / cos(pi * (2 * (i + 24) + 19) / 72);
		win0[1][i+30] = win0[3][i] = 0;
		win0[3][i+6 ] = half * sin(pi / 24 * (2 * i + 1 )) / cos(pi * (2 * (i +  6) + 19) / 72);
	}

	for (int i = 0; i < 12; i++)
		win0[2][i] = half * sin(pi / 24 * (2 * i + 1)) / cos(pi / 24 * (2 * i + 7));

	for (int i = 0; i < 16; i++) {
		float t = tan(pi * i / 12);
		tan1_1[i] = t / (1 + t);
		tan2_1[i] = 1 / (1 + t);
		tan1_2[i] = sqrt2 * t / (1 + t);
		tan2_2[i] = sqrt2 / (1 + t);

		for (int j = 0; j < 2; j++) {
			float base = iso::pow(2.0f, -0.25f * (j + 1));
			float p1	= 1, p2 = 1;
			if (i > 0) {
				if (i & 1)
					p1 = iso::pow(base, (i + 1) * 0.5f);
				else
					p2 = iso::pow(base, i * 0.5f);
			}
			pow1_1[j][i] = p1;
			pow2_1[j][i] = p2;
			pow1_2[j][i] = sqrt2 * p1;
			pow2_2[j][i] = sqrt2 * p2;
		}
	}

	for (int j = 0; j < 4; j++) {
		static const int len[4] = {36, 36, 12, 36};
		for (int i = 0; i < len[j]; i += 2) {
			win1[j][i + 0]	= +win0[j][i + 0];
			win1[j][i + 1]	= -win0[j][i + 1];
		}
	}

	for (int j = 0; j < 9; j++) {
		const BandInfo	*bi		= &band_info[j];
		const uint8		*bdf	= bi->long_diff;
		int				*mp		= map[j][0][0] = mapbuf0[j];
		int				i		= 0;

		for (int cb = 0; cb < 8 ; cb++, i += *bdf++) {
			*mp++ = (*bdf) >> 1;
			*mp++ = i;
			*mp++ = 3;
			*mp++ = cb;
		}
		bdf = bi->short_diff+3;
		for (int cb = 3; cb < 13; cb++) {
			int l = (*bdf++) >> 1;
			for (int lwin = 0; lwin < 3; lwin++) {
				*mp++ = l;
				*mp++ = i + lwin;
				*mp++ = lwin;
				*mp++ = cb;
			}
			i += 6 * l;
		}
		map[j][0][1] = mp;

		mp	= map[j][1][0] = mapbuf1[j];
		bdf = bi->short_diff+0;
		for (int i = 0, cb = 0; cb < 13; cb++) {
			int l = (*bdf++) >> 1;
			for (int lwin = 0; lwin < 3; lwin++) {
				*mp++ = l;
				*mp++ = i + lwin;
				*mp++ = lwin;
				*mp++ = cb;
			}
			i += 6 * l;
		}
		map[j][1][1] = mp;

		mp	= map[j][2][0] = mapbuf2[j];
		bdf	= bi->long_diff;
		for (int cb = 0; cb < 22; cb++) {
			*mp++ = *bdf++ >> 1;
			*mp++ = cb;
		}
		map[j][2][1] = mp;
	}

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 6; j++)
			for (int k = 0; k < 6; k++)
				i_slen2[k + j * 6 + i * 36] = i|(j<<3)|(k<<6)|(3<<12);
	}
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++)
			for (int k = 0; k < 4; k++)
				i_slen2[k + j * 4 + i * 16 + 180] = i|(j<<3)|(k<<6)|(4<<12);
	}
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 3; j++) {
			int n = j + i * 3;
			i_slen2[n + 244] = i|(j<<3) | (5<<12);
			n_slen2[n + 500] = i|(j<<3) | (2<<12) | (1<<15);
		}
	}
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++)
			for (int k = 0; k < 4; k++)
				for (int l = 0; l < 4; l++)
					n_slen2[l + k * 4 + j * 16 + i * 80] = i|(j<<3)|(k<<6)|(l<<9)|(0<<12);
	}
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++)
			for (int k = 0; k < 4; k++)
				n_slen2[k + j * 4 + i * 20 + 400] = i|(j<<3)|(k<<6)|(1<<12);
	}

	mdct36<float>::init();
	mdct12<float>::init();
}

void Layer3::init_scratch(scratch *s) {
	clear(s->hybrid_in);
	clear(s->hybrid_out);
	clear(s->hybrid_block);
	s->block_index	= 0;;
	s->bitreservoir = 0;

	for (int j = 0; j < 9; j++) {
		const BandInfo &bi = band_info[j];
		for (int i = 0; i < 23; i++)
			s->long_limit[j][i] = min((bi.long_idx[i] - 1 + 8) / 18 + 1, SBLIMIT);
		for (int i = 0; i < 14; i++)
			s->short_limit[j][i] = min((bi.short_idx[i] - 1) / 18 + 1, SBLIMIT);
	}
}

// read additional side information (for MPEG 1 and MPEG 2)
bool Layer3::granule::read(vlc_reader &vlc, const BandInfo &bi, bool lsf, bool mpeg25, bool ms_stereo, int powdiff) {
	part2_3_length	= vlc.get(12);
	big_values		= min(vlc.get(9), 288);
	pow2gain		= gainpow2 + 256 - vlc.get(8) + powdiff + (ms_stereo ? 2 : 0);
	scalefac_compress = vlc.get(lsf ? 9 : 4);

	if (vlc.get_bit()) { // window switch flag
		block_type			= vlc.get(2);
		mixed_block_flag	= vlc.get_bit();
		table_select[0]		= vlc.get(5);
		table_select[1]		= vlc.get(5);
		table_select[2]		= 0;

		for (int i = 0; i < 3; i++)
			full_gain[i] = pow2gain + (vlc.get(3) << 3);

		if (block_type == 0)
			return true;

		// region_count/start parameters are implicit in this case.
		if ((!lsf || block_type == 2) && !mpeg25) {
			region1start = 36  >> 1;
			region2start = 576 >> 1;
		} else {
			if (mpeg25) {
				int	r0c = block_type == 2 && !mixed_block_flag ? 5 : 7;
				int r1c = 20 - r0c;
				region1start = bi.long_idx[r0c + 1] >> 1 ;
				region2start = bi.long_idx[r0c + 1 + r1c + 1] >> 1;
			} else {
				region1start = 54  >> 1;
				region2start = 576 >> 1;
			}
		}
	} else {
		for (int i = 0; i < 3; i++)
			table_select[i] = vlc.get(5);

		int	r0c = vlc.get(4);
		int	r1c = vlc.get(3);
		region1start = bi.long_idx[r0c + 1] >> 1 ;

		// max(r0c+r1c+2) = 15+7+2 = 24
		if (r0c + r1c > 20)
			region2start = 576 >> 1;
		else
			region2start = bi.long_idx[r0c + r1c + 2] >> 1;

		block_type			= 0;
		mixed_block_flag	= false;
	}
	if (!lsf)
		preflag = vlc.get_bit();

	scalefac_scale		= vlc.get_bit();
	count1table_select	= vlc.get_bit();
	return false;
}

void Layer3::granule::get_scale_factors(vlc_reader &vlc, int *scf, bool lsf, bool i_stereo) {
	if (lsf) {
		static const uint8 stab[3][6][4] =	{ {
			{ 6, 5, 5, 5}, { 6, 5, 7, 3}, {11,10, 0, 0},
			{ 7, 7, 7, 0}, { 6, 6, 6, 3}, { 8, 8, 5, 0}
		},	{
			{ 9, 9, 9, 9}, { 9, 9,12, 6}, {18,18, 0, 0},
			{12,12,12, 0}, {12, 9, 9, 6}, {15,12, 9, 0}
		},	{
			{ 6, 9, 9, 9}, { 6, 9,12, 6}, {15,18, 0, 0},
			{ 6,15,12, 0}, { 6,12, 9, 6}, { 6,18, 9, 0}
		} };

		uint32	slen	= i_slen2[scalefac_compress >> int(i_stereo)];
		int		n		= block_type == 2 ? (mixed_block_flag ? 2 : 1) : 0;

		preflag = !!(slen & 0x8000);

		const uint8 *pnt = stab[n][(slen >> 12) & 7];
		for (int i = 0; i < 4; i++) {
			int num = slen & 0x7;
			slen >>= 3;
			if (num) {
				for (int j = 0; j < pnt[i]; j++)
					*scf++ = vlc.get(num);
			} else {
				for(int j = 0; j < pnt[i]; j++)
					*scf++ = 0;
			}
		}

		for (int i = (n << 1) + 1; i--;)
			*scf++ = 0;

	} else {
		static const uint8 slen[2][16] = {
			{0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4},
			{0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3}
		};
		int num0 = slen[0][scalefac_compress];
		int num1 = slen[1][scalefac_compress];

		if (block_type == 2) {
			for (int i = mixed_block_flag ? 17 : 18; i--; *scf++ = vlc.get(num0));
			for (int i = 18; i--; *scf++ = vlc.get(num1));
			*scf++ = 0;
			*scf++ = 0;
			*scf++ = 0;

		} else {
			if (scfsi < 0) { // scfsi < 0 => granule == 0
				for (int i = 11; i--; *scf++ = vlc.get(num0));
				for (int i = 10; i--; *scf++ = vlc.get(num1));
				*scf++ = 0;
			} else {
				if (!(scfsi & 8)) {
					for (int i = 0; i < 6; i++)
						*scf++ = vlc.get(num0);
				} else {
					scf += 6;
				}
				if (!(scfsi & 4)) {
					for (int i = 0; i < 5; i++)
						*scf++ = vlc.get(num0);
				} else {
					scf += 5;
				}
				if (!(scfsi & 2)) {
					for (int i = 0; i < 5; i++)
						*scf++ = vlc.get(num1);
				} else {
					scf += 5;
				}
				if (!(scfsi & 1)) {
					for (int i = 0; i < 5; i++)
						*scf++ = vlc.get(num1);
				} else {
					scf += 5;
				}
				*scf++ = 0;	// no l[21] in original sources
			}
		}
	}
}

int Layer3::granule::dequantize_sample(vlc_reader &vlc, float xr[SBLIMIT][SSLIMIT], int *long_limit, int *short_limit, int *map[3][2], int *scf) {
	int		bv			= big_values;
	int		region1		= region1start;
	int		region2		= region2start;
	int		l3			= ((576 >> 1) - bv) >> 1;
	int		l[3];

	if (bv <= region1) {
		l[0] = bv;
		l[1] = 0;
		l[2] = 0;
	} else {
		l[0] = region1;
		if (bv <= region2) {
			l[1] = bv - l[0];
			l[2] = 0;
		} else {
			l[1] = region2 - l[0];
			l[2] = bv - region2;
		}
	}

	int		shift	= 1 + int(scalefac_scale);
	float	*xrpnt	= xr[0];

	if (block_type == 2) {	// decoding with short or mixed mode BandIndex table
		int		mx[4] = {-1,-1,-1,-1};
		int		step = 0, lwin = 3, cb = 0;
		float	v	= 0;
		int		mi	= 1;

		if (mixed_block_flag) {
			mx[0]	= mx[1] = mx[2] = 2;
			mi		= 0;
		}
		int	*m		= map[mi][0];
		int	*me		= map[mi][1];
		int	mc		= 0;

		for (int i = 0; i < 2; i++) {
			const hufftable &h = ht[table_select[i]];
			uint32	linbits		= h.linbits;

			for (int lp = l[i]; lp; lp--, mc--) {
				if (!mc) {
					mc		= *m++;
					xrpnt	= ((float*)xr) + *m++;
					lwin	= *m++;
					cb		= *m++;
					if (lwin == 3) {
						v = pow2gain[*scf++ << shift];
						step = 1;
					} else {
						v = full_gain[lwin][*scf++ << shift];
						step = 3;
					}
				}
				int y = h.lookup(vlc);
				int	x = y >> 4;
				y &= 0xf;

				if (x == 15 && linbits) {
					mx[lwin] = cb;
					x		+= vlc.get(linbits);
					*xrpnt	= (vlc.get_bit() ? -ispow[x] : ispow[x]) * v;
				} else if (x) {
					mx[lwin] = cb;
					*xrpnt	= (vlc.get_bit() ? -ispow[x] : ispow[x]) * v;
				} else {
					*xrpnt = 0;
				}

				xrpnt += step;
				if (y == 15 && linbits) {
					mx[lwin] = cb;
					y		+= vlc.get(linbits);
					*xrpnt	= (vlc.get_bit() ? -ispow[y] : ispow[y]) * v;
				} else if (y) {
					mx[lwin] = cb;
					*xrpnt	= (vlc.get_bit() ? -ispow[y] : ispow[y]) * v;
				} else {
					*xrpnt = 0;
				}

				xrpnt += step;
			}
		}

		for (;l3 > 0; l3--) {
			if (xrpnt >= &xr[SBLIMIT][0] + 5)//overrun check
				return 2;

			short a	= htc[int(count1table_select)].lookup(vlc);

			for (int i = 0; i < 4; i++) {
				if (!(i & 1)) {
					if (!mc) {
						mc		= *m++;
						xrpnt	= (float*)xr + *m++;
						lwin	= *m++;
						cb		= *m++;
						if (lwin == 3) {
							v		= pow2gain[(*scf++) << shift];
							step	= 1;
						} else {
							v		= full_gain[lwin][(*scf++) << shift];
							step	= 3;
						}
					}
					mc--;
				}
				if (a & (0x8 >> i)) {
					mx[lwin] = cb;
					*xrpnt	= vlc.get_bit() ? -v : v;
				} else {
					*xrpnt = 0;
				}
				xrpnt += step;
			}
		}

		if (lwin < 3) { // short band?
			for (;;) {
				for (;mc > 0; mc--)	{
					*xrpnt = 0; xrpnt += 3; // short band -> step=3
					*xrpnt = 0; xrpnt += 3;
				}
				if (m >= me)
					break;

				mc		= *m++;
				xrpnt	= (float*)xr + *m++;
				if (*m++ == 0)
					break; // optimize: field will be set to zero at the end of the function
				m++; // cb
			}
		}

		maxband[0]	= mx[0] + 1;
		maxband[1]	= mx[1] + 1;
		maxband[2]	= mx[2] + 1;
		maxbandl	= mx[3] + 1;

		if (int rmax = max(max(mx[0], mx[1]), mx[2]) + 1)
			maxb = short_limit[rmax];
		else
			maxb = long_limit[mx[3]+1];

	} else {
		// decoding with 'long' BandIndex table (block_type != 2)
		static const uint8 pretab_choice[2][22] = {
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2,0}
		};
		const uint8 *pretab = pretab_choice[preflag];
		int		mx = -1;
		int		cb	= 0;
		int		*m	= map[2][0];
		float	v	= 0;
		int		mc	= 0;

		// long hash table values
		for (int i = 0; i < 3; i++) {
			const hufftable &h	= ht[table_select[i]];
			uint32	linbits		= h.linbits;

			for (int lp = l[i]; lp; lp--, mc--) {
				if (!mc) {
					mc	= *m++;
					cb	= *m++;
					v	= pow2gain[(*scf++ + *pretab++) << shift];
				}
				int y = h.lookup(vlc);
				int	x = y >> 4;
				y &= 0xf;

				if (x == 15 && linbits) {
					mx	= cb;
					x		+= vlc.get(linbits);
					*xrpnt++= (vlc.get_bit() ? -ispow[x] : ispow[x]) * v;
				} else if (x) {
					mx		= cb;
					*xrpnt++= (vlc.get_bit() ? -ispow[x] : ispow[x]) * v;
				} else {
					*xrpnt++ = 0;
				}

				if (y == 15 && linbits) {
					mx	= cb;
					y		+= vlc.get(linbits);
					*xrpnt++= (vlc.get_bit() ? -ispow[y] : ispow[y]) * v;
				} else if (y) {
					mx		= cb;
					*xrpnt++= (vlc.get_bit() ? -ispow[y] : ispow[y]) * v;
				} else {
					*xrpnt++ = 0;
				}
			}
		}

		// short (count1table) values
		for (;l3 > 0; l3--) {
			short a = htc[count1table_select].lookup(vlc);

			for (int i = 0; i < 4; i++, a << 1) {
				if (!(i & 1)) {
					if (!mc) {
						mc	= *m++;
						cb	= *m++;
						v	= pow2gain[(*scf++ + *pretab++) << shift];
					}
					mc--;
				}
				if (a & 8) {
					mx = cb;
					*xrpnt	= vlc.get_bit() ? -v : v;
				} else {
					*xrpnt++ = 0;
				}
			}
		}

		maxbandl	= mx + 1;
		maxb		= long_limit[maxbandl];
	}

	while (xrpnt < &xr[SBLIMIT][0])
		*xrpnt++ = 0;

	return 0;
}

// calculate float channel values for Joint-I-Stereo-mode
void Layer3::granule::i_stereo(float *in0, float *in1, int *scalefac, const BandInfo &bi, bool ms_stereo, bool lsf) {
	static const float *tabs[3][2][2] =	{
		{{tan1_1,	 tan2_1},		{tan1_2,	tan2_2}},
		{{pow1_1[0], pow2_1[0]},	{pow1_2[0],	pow2_2[0]}},
		{{pow1_1[1], pow2_1[1]},	{pow1_2[1],	pow2_2[1]}}
	};

	int			tab		= int(lsf) + (scalefac_compress & int(lsf));
	const float *tab1	= tabs[tab][ms_stereo][0];
	const float *tab2	= tabs[tab][ms_stereo][1];

	if (block_type == 2) {
		bool	do_l = mixed_block_flag;

		for (int lwin = 0; lwin < 3; lwin++) {	// process each window
			// get first band with zero values
			int sfb = maxband[lwin];		// sfb is minimal 3 for mixed mode
			if (sfb > 3)
				do_l = false;

			for (;sfb < 12; sfb++) {
				int	is_p = scalefac[sfb * 3 + lwin - int(mixed_block_flag)]; // scale: 0-15
				if (is_p != 7) {
					float	t1 = tab1[is_p], t2 = tab2[is_p];
					for (int sb = bi.short_diff[sfb], idx = bi.short_idx[sfb] + lwin; sb--; idx += 3) {
						float v = in0[idx];
						in0[idx] = v * t1;
						in1[idx] = v * t2;
					}
				}
			}

			int	is_p = scalefac[11 * 3 + lwin - int(mixed_block_flag)]; // scale: 0-15
			if (is_p != 7) {
				float t1 = tab1[is_p], t2 = tab2[is_p];
				for (int sb = bi.short_diff[12], idx = bi.short_idx[12] + lwin; sb--; idx += 3) {
					float v = in0[idx];
					in0[idx] = v * t1;
					in1[idx] = v * t2;
				}
			}
		}

		// also check l-part, if ALL bands in the three windows are 'empty' and mode = mixed_mode
		if (do_l) {
			int	sfb = maxbandl;
			if (sfb > 21)
				return; // similarity fix related to CVE-2006-1655

			for (int idx = bi.long_idx[sfb]; sfb < 8; sfb++) {
				int sb		= bi.long_diff[sfb];
				int is_p	= scalefac[sfb]; // scale: 0-15
				if (is_p != 7) {
					float t1 = tab1[is_p], t2 = tab2[is_p];
					for (; sb--; idx++) {
						float v = in0[idx];
						in0[idx] = v * t1;
						in1[idx] = v * t2;
					}
				} else {
					idx += sb;
				}
			}
		}
	} else { // block_type != 2
		int sfb = maxbandl;
		if (sfb > 21)
			return; // tightened fix for CVE-2006-1655

		for (int idx = bi.long_idx[sfb]; sfb < 21; sfb++) {
			int	sb		= bi.long_diff[sfb];
			int	is_p	= scalefac[sfb]; // scale: 0-15
			if (is_p != 7) {
				float t1 = tab1[is_p], t2 = tab2[is_p];
				for (; sb--; idx++) {
					 float v = in0[idx];
					 in0[idx] = v * t1;
					 in1[idx] = v * t2;
				}
			} else {
				idx += sb;
			}
		}

		int	is_p = scalefac[20];
		if (is_p != 7) {	// copy l-band 20 to l-band 21
			float t1 = tab1[is_p], t2 = tab2[is_p];
			for (int sb = bi.long_diff[21], idx = 21; sb--; idx++) {
				float v = in0[idx];
				in0[idx] = v * t1;
				in1[idx] = v * t2;
			}
		}
	}
}

void Layer3::granule::antialias(float xr[SBLIMIT][SSLIMIT]) {
	if (block_type == 2 && !mixed_block_flag)
		return;

	float *xr1 = (float*)xr[1];
	for (int sb = block_type == 2 ? 1 : maxb - 1; sb--; xr1 += 10) {
		float *cs = aa_cs, *ca = aa_ca, *xr2 = xr1 - 1;
		for (int ss = 8; ss--; ++xr1, --xr2, ca++, cs++) { // upper and lower butterfly inputs
			float bu = *xr2, bd = *xr1, va = *ca, vs = *cs;
			*xr2 = bu * vs - bd * va;
			*xr1 = bd * vs + bu * va;
		}
	}
}

void Layer3::granule::hybrid(float in[SBLIMIT][SSLIMIT], float *ts, const float *prev, float *next) {
	size_t	sb		= 0;

	if (mixed_block_flag) {
		mdct36<float>::process(in[0], prev,			next,		win0[0],	column((float(*)[SBLIMIT])(ts + 0)));
		mdct36<float>::process(in[1], prev + 18,	next + 18,	win1[0],	column((float(*)[SBLIMIT])(ts + 1)));
		prev	+= 36;
		next	+= 36;
		ts		+= 2;
		sb		= 2;
	}

	float	*w0 = win0[block_type];
	float	*w1 = win1[block_type];
	if (block_type == 2) {
		for (; sb < maxb; sb += 2, ts += 2) {
			fixed_stride_iterator<float, SBLIMIT>	out0(ts + 0);
			out0[0] = prev[0]; out0[1] = prev[1]; out0[2] = prev[2]; out0[3] = prev[3]; out0[4] = prev[4]; out0[5] = prev[5];
			mdct12<float>::process(in[sb + 0],	prev + 6,	next,	w0,	out0 + 6);
			memset(next + 12, 0, 6 * sizeof(float));
			prev += 18;
			next += 18;

			fixed_stride_iterator<float, SBLIMIT>	out1(ts + 1);
			out1[0] = prev[0]; out1[1] = prev[1]; out1[2] = prev[2]; out1[3] = prev[3]; out1[4] = prev[4]; out1[5] = prev[5];
			mdct12<float>::process(in[sb + 1],	prev + 6,	next,	w1,	out1 + 6);
			memset(next + 12, 0, 6 * sizeof(float));
			prev += 18;
			next += 18;
		}
	} else {
		for (; sb < maxb; sb += 2, ts += 2, prev += 36, next += 36) {
			mdct36<float>::process(in[sb + 0],	prev,		next,		w0,	column<SBLIMIT>(ts + 0));
			mdct36<float>::process(in[sb + 1],	prev + 18,	next + 18,	w1,	column<SBLIMIT>(ts + 1));
		}
	}

	for (;sb < SBLIMIT; sb++, ts++) {
		for (int i = 0 ; i < SSLIMIT; i++) {
			ts[i * SBLIMIT] = *prev++;
			*next++ = 0;
		}
	}
}

int Layer3::do_layer(mpg123 *fr) {
	uint8	*start		= const_cast<uint8*>(fr->buffer.p);
	int		channels	= fr->h.mono() ? 1 : 2;
	mpg123::MIX	mix		= channels == 1 ? mpg123::MIX_LEFT : fr->mix;
	int		channels1	= channels == 1 || mix != mpg123::MIX_STEREO ? 1 : 2;
	int		sfreq		= fr->h.get_freqindex();
	bool	lsf			= fr->h.lsf();
	bool	mpeg25		= fr->h.mpeg25();
	bool	ms_stereo	= fr->h.joint_stereo() && (fr->h.chanex & 2);
	bool	i_stereo	= fr->h.joint_stereo() && (fr->h.chanex & 1);
	int		ssize		= fr->h.get_offset() + (fr->h.error_prot() ? 2 : 0);

	int		num_granules;
	uint32	main_data_begin;
	uint32	private_bits;

	if (lsf) {
		main_data_begin	= fr->vlc.get(8);
		private_bits	= fr->vlc.get(channels);
		num_granules	= 1;
	} else {
		main_data_begin	= fr->vlc.get(9);
		private_bits	= fr->vlc.get(channels == 1 ? 5 : 3);
		num_granules	= 2;
		for (int ch = 0; ch < channels; ch++) {
			granules[ch][0].scfsi = -1;
			granules[ch][1].scfsi = fr->vlc.get(4);
		}
	}

	if (main_data_begin > s->bitreservoir) {
		main_data_begin = s->bitreservoir;
		memset(const_cast<uint8*>(fr->buffer.p), 0, ssize);
	}
	s->bitreservoir = min(s->bitreservoir + fr->framesize - ssize, lsf ? 255 : 511);

	int powdiff = mix == mpg123::MIX_MIX ? 4 : 0;
	for (int g = 0; g < num_granules; g++) {
		for (int ch = 0; ch < channels; ch++) {
			if (granules[ch][g].read(fr->vlc, band_info[sfreq], lsf, mpeg25, ms_stereo, powdiff))
				return 0;
		}
	}

	{
		uint8	*p = start + ssize - main_data_begin;
		if (main_data_begin)
			memcpy(p, s->reservoir_end - main_data_begin, main_data_begin);
		s->reservoir_end = start + fr->framesize;
		start	= p;
	}

	int		clip		= 0;
	uint32	bitstart	= 0;
	int		scalefacs[2][39];

	for (int g = 0; g < num_granules; g++) {
		granule	&gr			= granules[0][g];

		fr->set_pointer(start + (bitstart >> 3));
		fr->vlc.get(bitstart & 7);
		bitstart	+= gr.part2_3_length;

		gr.get_scale_factors(fr->vlc, scalefacs[0], lsf, false);
		if (gr.dequantize_sample(fr->vlc, s->hybrid_in[0], s->long_limit[sfreq], s->short_limit[sfreq], map[sfreq], scalefacs[0]))
			return clip;

		if (channels == 2) {
			granule	&gr			= granules[1][g];

			fr->set_pointer(start + (bitstart >> 3));
			fr->vlc.get(bitstart & 7);
			bitstart	+= gr.part2_3_length;

			gr.get_scale_factors(fr->vlc, scalefacs[1], lsf, i_stereo);
			if (gr.dequantize_sample(fr->vlc, s->hybrid_in[1], s->long_limit[sfreq], s->short_limit[sfreq], map[sfreq], scalefacs[1]))
				return clip;

			float *in0 = s->hybrid_in[0][0], *in1 = s->hybrid_in[1][0];

			if (ms_stereo) {
				uint32 maxb = max(granules[0][g].maxb, granules[1][g].maxb);
				for (int i = 0; i < SSLIMIT * maxb; i++) {
					float t0 = in0[i];
					float t1 = in1[i];
					in0[i] = t0 + t1;
					in1[i] = t0 - t1;
				}
			}

			if (i_stereo)
				gr.i_stereo(in0, in1, scalefacs[1], band_info[sfreq], ms_stereo, lsf);

			if (ms_stereo || i_stereo || mix == mpg123::MIX_MIX) {
				if (gr.maxb > granules[0][g].maxb)
					granules[0][g].maxb = gr.maxb;
				else
					gr.maxb = granules[0][g].maxb;
			}

			switch (mix) {
				case mpg123::MIX_MIX:
					for (int i = 0; i < SSLIMIT * gr.maxb; i++)
						in0[i] = in0[i] + in1[i]; // *0.5 done by pow-scale
					break;

				case mpg123::MIX_RIGHT:
					for (int i = 0; i < SSLIMIT * gr.maxb; i++)
						in0[i] = in1[i];
					break;

				default:
					break;
			}
		}

		int	bi	= (s->block_index++) & 1;
		for (int c = 0; c < channels1; c++) {
			granule &gr	= granules[c][g];
			gr.antialias(s->hybrid_in[c]);
			gr.hybrid(s->hybrid_in[c], s->hybrid_out[c][0], s->hybrid_block[bi][c], s->hybrid_block[1 - bi][c]);
		}

		for (int ss = 0; ss < SSLIMIT; ss++) {
			clip += mix == mpg123::MIX_STEREO
				? fr->synth_stereo(s->hybrid_out[0][ss], s->hybrid_out[1][ss])
				: fr->synth_mono(s->hybrid_out[0][ss]);
		}
	}

	return clip;
}

//-----------------------------------------------------------------------------
//	layer-picking stubs
//-----------------------------------------------------------------------------

int mpg123::do_layer() {
	switch (h.layer) {
		case mpg_header::LAY_I:		return Layer1(this).do_layer(this);
		case mpg_header::LAY_II:	return Layer2(this).do_layer(this);
		case mpg_header::LAY_III:	return Layer3(this).do_layer(this);
		default:					return -1;
	}
}

void mpg123::init() {
	mdct64<float>::init();
	Layer12::init();
	Layer3::init();
}

//-----------------------------------------------------------------------------
//	SHOUTcast ICY
//-----------------------------------------------------------------------------

size_t icy_stream::readbuff(void *buffer, size_t size) {
	int	cnt = 0;
	while (cnt < size) {
		if (next < size - cnt) {
			// we are near icy-metaint boundary, read up to the boundary
			if (next > 0) {
				auto	r = istream_chain::readbuff((char*)buffer + cnt, next);
				cnt		+= r;
				next	-= r;
				if (next > 0)
					break;
			}

			// one byte icy-meta size (must be multiplied by 16 to get icy-meta length)
			if (int	t = istream_chain::getc()) {
				size_t	meta_size = t * 16;
				if (void *meta_buff = malloc(meta_size)) {
					auto	r = istream_chain::readbuff(meta_buff, meta_size);
					if (meta)
						free(meta);
					meta = meta_buff;
				} else {
					istream_chain::seek_cur(meta_size);
				}
			}
			next = interval;
		} else {
			auto	r	= istream_chain::readbuff((char*)buffer + cnt, size - cnt);
			cnt		+= r;
			next	-= r;
		}
	}
	return cnt;
}
//-----------------------------------------------------------------------------
//	MP3FileHandler
//-----------------------------------------------------------------------------

class MP3FileHandler : public SampleFileHandler {
	const char*		GetExt() override { return "mp3";	}
	int				Check(istream_ref file) override;
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} mp3;

static bool skipFirstPacket = false;
void MP3SkipFirstPacket(bool skip) { skipFirstPacket = skip; }

void scandir_mp3(const char *d) {
	return;
	for (directory_iterator di(filename(d).add_dir("*.*")); di; ++di) {
		if (di.is_dir()) {
			if (di[0] != '.')
				scandir_mp3(filename(d).add_dir(di));
		} else {
			FileInput	file(filename(d).add_dir(di));
			mpg123		m(file);
			m.decode_frame(0, 0, 0);
			ISO_TRACEF(m.get_title()) << '\n';
		}
	}
}

int MP3FileHandler::Check(istream_ref file) {
	file.seek(0);
	uint32	u	= file.get<uint32be>();
	if (mpg_header(u).check())
		return CHECK_PROBABLE;
	if ((u >> 8) == 'ID3')
		return CHECK_PROBABLE;
	if ((u >> 8) == 'TAG')
		return CHECK_POSSIBLE;
	return CHECK_UNLIKELY;
}

ISO_ptr<void> MP3FileHandler::Read(tag id, istream_ref file) {
	mpg123		m(file);
	/*
	m.params.set(mpg123::PARAM_MONO_LEFT);
	m.allow_none();
	m.allow(44100, 3, ENC_32|ENC_FLOAT|ENC_SIGNED);
	*/
	int64		outnum;
	void		*outdata;
	size_t		outlen;
	malloc_chain	ch;
	for (int r; (r = m.decode_frame(&outnum, &outdata, &outlen)) != mpg123::RET_DONE;)
		ch.push_back(const_memory_block(outdata, outlen));

/*	ISO_TRACE(m.get_title());
	ISO_TRACE(m.get_album());
	ISO_TRACE(m.get_artist());
	ISO_TRACE(m.get_year());
	ISO_TRACE(m.get_genre());
	CD_TOC *toc = m.id3v2.get_CD_TOC();
*/

	ISO_ptr<sample>	p(id);
	audioformat		af	= m.get_format();
	ch.copy_to(p->Create(uint32(ch.total() / (af.encsize * af.channels)), af.channels, 16));
	p->SetFrequency(af.rate);

#if 0
	mp3data_struct		mp3data;
	int					len		= file.length();
	uint8				*buffer	= (uint8*)malloc(len);
	lame_global_flags	*gf		= lame_init();
	int16				pcm_l[1024], pcm_r[1024];

	file.readbuff(buffer, len);
	int				r		=lame_decode1_headers(buffer, len, pcm_l, pcm_r, &mp3data);
#endif
	return p;
}

bool MP3FileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (ISO_ptr<sample> s = ISO_conversion::convert<sample>(p)) {
		lame_global_flags *gf = lame_init();

		lame_set_num_channels(gf,	s->Channels());
		lame_set_in_samplerate(gf,	s->Frequency());
		lame_set_num_samples(gf,	s->Length());
		lame_set_bWriteVbrTag(gf,	0);
		lame_set_disable_reservoir(gf, 1 );
	#if 0
		if (SMWF_GETQUALITY(flags)) {
	//			lame_set_quality(gf, (100 - SMWF_GETQUALITY(flags)) / 10);
	//			lame_set_VBR_q(gf, (100 - SMWF_GETQUALITY(flags)) / 10);
	//			lame_set_VBR_mean_bitrate_kbps(gf, SMWF_GETQUALITY(flags) * 128 / 50);
			lame_set_compression_ratio(gf, 400.f / SMWF_GETQUALITY(flags));
	//			lame_set_brate(gf, SMWF_GETQUALITY(flags) * 2);
		}
	#endif

		lame_init_params(gf);

//		sm.MakeBits(16);

		uint8	mp3buffer[LAME_MAXMP3BUFFER];

		int				rest		= s->Length();
		int16			*in			= s->Samples();
		int				iread, imp3;

		do {
			iread = rest < 1152 ? rest : 1152;

			if (s->Channels() == 2) {
				imp3 = lame_encode_buffer_interleaved(gf, in, iread, mp3buffer, sizeof(mp3buffer));
				in += iread * 2;
			} else {
				imp3 = lame_encode_buffer(gf, in, in, iread, mp3buffer, sizeof(mp3buffer));
				in += iread;
			}

			if (skipFirstPacket && imp3)
				skipFirstPacket = false;
			else
				file.writebuff(mp3buffer, imp3);

			rest	-= iread;
		} while (rest > 0);

		imp3 = lame_encode_flush(gf, mp3buffer, sizeof(mp3buffer)); // may return one more mp3 frame
		lame_mp3_tags_fid(gf, NULL);
		file.writebuff(mp3buffer, imp3);

		return true;
	}
	return false;
}
