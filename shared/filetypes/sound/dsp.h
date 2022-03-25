#ifndef DSP_H
#define DSP_H

#include "base/defs.h"

namespace iso {
enum {
	FMT_ADPCM	= 0x0000,	// ADPCM mode
	FMT_PCM16	= 0x000A,	// 16-bit PCM mode
	FMT_PCM8	= 0x0009,	// 8-bit PCM mode (UNSIGNED)
};

template<bool BE> struct _ADPCMINFO {
	typedef endian_t<int16, BE>	int16;
	typedef endian_t<uint16,BE>	uint16;
	typedef endian_t<uint32,BE>	uint32;
	// start context
	int16	coef[16];
	uint16	gain;
	uint16	pred_scale;
	int16	yn1;
	int16	yn2;

	// loop context
	uint16	loop_pred_scale;
	int16	loop_yn1;
	int16	loop_yn2;

	_ADPCMINFO()	{}
	template<bool BE2> _ADPCMINFO(const _ADPCMINFO<BE2> &a) {
		for (int i = 0; i < num_elements(coef); i++)
			coef[i] = a.coef[i];
		gain			= a.gain;
		pred_scale		= a.pred_scale;
		yn1				= a.yn1;
		yn2				= a.yn2;
		loop_pred_scale	= a.loop_pred_scale;
		loop_yn1		= a.loop_yn1;
		loop_yn2		= a.loop_yn2;
	}
};

typedef _ADPCMINFO<false> ADPCMINFO;

struct DSP {
	HINSTANCE dll;
	dll_function<uint32(uint32)>	getBytesForAdpcmBuffer;
	dll_function<uint32(uint32)>	getBytesForAdpcmSamples;
	dll_function<uint32(uint32)>	getBytesForPcmBuffer;
	dll_function<uint32(uint32)>	getBytesForPcmSamples;
	dll_function<uint32(uint32)>	getSampleForAdpcmNibble;
	dll_function<uint32(uint32)>	getNibbleAddress;
	dll_function<uint32()>			getBytesForAdpcmInfo;
	dll_function<void(int16*, uint8*, ADPCMINFO*, uint32)>	encode;
	dll_function<void(uint8*, int16*, ADPCMINFO*, uint32)>	decode;
	dll_function<void(uint8*, ADPCMINFO*, uint32)>			getLoopContext;
	DSP() {
		dll = LoadLibraryA("dsptool.dll");
		if (!dll)
			throw_accum("Cannot load Wii ADPCM dll (dsptool.dll)");

		getBytesForAdpcmBuffer	.bind(dll, "getBytesForAdpcmBuffer");
		getBytesForAdpcmSamples	.bind(dll, "getBytesForAdpcmSamples");
		getBytesForPcmBuffer	.bind(dll, "getBytesForPcmBuffer");
		getBytesForPcmSamples	.bind(dll, "getBytesForPcmSamples");
		getSampleForAdpcmNibble	.bind(dll, "getSampleForAdpcmNibble");
		getNibbleAddress		.bind(dll, "getNibbleAddress");
		getBytesForAdpcmInfo	.bind(dll, "getBytesForAdpcmInfo");
		encode					.bind(dll, "encode");
		decode					.bind(dll, "decode");
		getLoopContext			.bind(dll, "getLoopContext");
	}
};

extern singleton<DSP>	dsp_singleton;

} //namespace iso

#endif// DSP_H
