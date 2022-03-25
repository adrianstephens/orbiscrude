#ifndef WAV_H
#define WAV_H

#include "sample.h"
#include "filetypes/riff.h"

namespace iso {

struct ISO_WAVEFORMAT {
	enum FORMAT {
		UNKNOWN				= 0,
		PCM					= 1,
		ADPCM				= 2,
		IEEE_FLOAT			= 3,
		MPEGLAYER3			= 0x0055, // ISO/MPEG Layer3
		DOLBY_AC3_SPDIF		= 0x0092, // Dolby Audio Codec 3 over S/PDIF
		WMAUDIO2			= 0x0161, // Windows Media Audio
		WMAUDIO3			= 0x0162, // Windows Media Audio Pro
		WMASPDIF			= 0x0164, // Windows Media Audio over S/PDIF
		EXTENSIBLE			= 0xfffe,
		AT9					= 9,//0xFFFC,
		AAC					= 0xAAC0,
	};

	uint16le	wFormatTag;				// format type
	uint16le	nChannels;				// number of channels (i.e. mono, stereo...)
	uint32le	nSamplesPerSec;			// sm rate
	uint32le	nAvgBytesPerSec;		// for buffer estimation
	uint16le	nBlockAlign;			// block size of data
	uint16le	wBitsPerSample;			// number of bits per sm of mono data
};

struct ISO_WAVEFORMATEX : ISO_WAVEFORMAT {
	uint16le	cbSize;					// the count in bytes of the size of following structure
};

struct ISO_WAVEFORMATEX1: ISO_WAVEFORMAT {
	uint16le	cbSize;
	uint16le	unknown;
};

struct ISO_ADPCMWAVEFORMAT : ISO_WAVEFORMAT {
	uint16le	cbSize;

	uint16le	wSamplesPerBlock;
	uint16le	wNumCoef;
	struct ADPCMCOEFSET {
		int16le iCoef1;
		int16le iCoef2;
	} aCoef[];  // Always 7 coefficient pairs for MS ADPCM
};

struct ISO_WAVEFORMATEXTENSIBLE : ISO_WAVEFORMAT {
	enum SPEAKER {
		FRONT_LEFT				= 0x00000001,
		FRONT_RIGHT				= 0x00000002,
		FRONT_CENTER			= 0x00000004,
		LOW_FREQUENCY			= 0x00000008,
		BACK_LEFT				= 0x00000010,
		BACK_RIGHT				= 0x00000020,
		FRONT_LEFT_OF_CENTER	= 0x00000040,
		FRONT_RIGHT_OF_CENTER	= 0x00000080,
		BACK_CENTER				= 0x00000100,
		SIDE_LEFT				= 0x00000200,
		SIDE_RIGHT				= 0x00000400,
		TOP_CENTER				= 0x00000800,
		TOP_FRONT_LEFT			= 0x00001000,
		TOP_FRONT_CENTER		= 0x00002000,
		TOP_FRONT_RIGHT			= 0x00004000,
		TOP_BACK_LEFT			= 0x00008000,
		TOP_BACK_CENTER			= 0x00010000,
		TOP_BACK_RIGHT			= 0x00020000,
		RESERVED				= 0x7FFC0000,
		ALL						= 0x80000000,

		MONO					= FRONT_CENTER,
		STEREO					= FRONT_LEFT	| FRONT_RIGHT,
		CHANS_2_1				= STEREO		| LOW_FREQUENCY,
		SURROUND				= STEREO		| FRONT_CENTER			| BACK_CENTER,
		CHANS_4_0				= STEREO		| BACK_LEFT				| BACK_RIGHT,
		CHANS_4_1				= CHANS_4_0		| LOW_FREQUENCY,
		CHANS_5_0				= CHANS_4_0		| FRONT_CENTER,
		CHANS_5_1				= CHANS_5_0		| LOW_FREQUENCY,
		CHANS_7_0				= CHANS_5_1		| BACK_CENTER,
		CHANS_7_1				= CHANS_5_1		| FRONT_LEFT_OF_CENTER	| FRONT_RIGHT_OF_CENTER,
		CHANS_5_1_SURROUND		= CHANS_5_1,
		CHANS_7_1_SURROUND		= CHANS_5_1		| SIDE_LEFT				| SIDE_RIGHT,
	};

	uint16le	cbSize;
	union {
		uint16le wValidBitsPerSample;	// Valid bits in each sm container
		uint16le wSamplesPerBlock;		// Samples per block of audio data; valid if wBitsPerSample=0 (but rarely used).
		uint16le wReserved;				// Zero if neither case above applies.
	} Samples;
	uint32le	dwChannelMask;			// Positions of the audio channels
	GUID		SubFormat;				// Format identifier GUID
};

typedef	bool WAVreader(const ISO_WAVEFORMATEX *fmt, RIFF_chunk &chunk, sample *sm);

struct WAVformat : static_list<WAVformat>, virtfunc<WAVreader> {
	template<typename T> WAVformat(T *me) : virtfunc<WAVreader>(me) {}
};

} //namespace iso

#endif	// WAV_H
