#include "samplefile.h"
#include "iso/iso_convert.h"
#include "dsp.h"

using namespace iso;

singleton<DSP>	iso::dsp_singleton;

struct DSPADPCMHEADER : bigendian_types {
	uint32 num_samples;
	uint32 num_adpcm_nibbles;
	uint32 sample_rate;

	uint16 loop_flag;
	uint16 format;
	uint32 sa;     // loop start address
	uint32 ea;     // loop end address
	uint32 ca;     // current address

	uint16 coef[16];

	// start context
	uint16 gain;
	uint16 ps;
	uint16 yn1;
	uint16 yn2;

	// loop context
	uint16 lps;
	uint16 lyn1;
	uint16 lyn2;

	uint16 pad[11];
};

class DSPFileHandler : public SampleFileHandler {

	const char*			GetExt() override { return "dsp";	}
	const char*			GetDescription() override { return "GameCube/Wii DSP-ADPCM sound file";	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	DSPFileHandler()	{}
} dsp;


bool DSPFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p);

	if (!sm)
		return false;

	auto	dsp	= dsp_singleton();
	if (!dsp.dll)
		return false;

	bool			looping		= !!(sm->Flags() & sample::LOOP);
	int				loop_start	= 0;
//	if (looping && (sm->Length() % BLOCKSIZE)) {
//		ISO_ptr<void> Resample(ISO_ptr<void> p, float newfreq);
//		sm = Resample(sm, ((sm->Length() + BLOCKSIZE / 2) / BLOCKSIZE * BLOCKSIZE) * sm->Frequency() / sm->Length());
//	}


	uint32				length	= sm->Length();
	uint32				outlen	= dsp.getBytesForAdpcmBuffer(length);
	malloc_block		out(outlen);

	ADPCMINFO	info;
	dsp.encode(sm->Samples(), out, &info, length);

	if (looping)
		dsp.getLoopContext(out, &info, loop_start);

	DSPADPCMHEADER	header;
	clear(header);

	header.num_samples		= length;
	header.num_adpcm_nibbles= dsp.getNibbleAddress(length);
	header.sample_rate		= uint32(sm->Frequency());

	header.loop_flag		= looping ? 1 : 0;
	header.format			= FMT_ADPCM;
	header.sa				= dsp.getNibbleAddress(loop_start);
	header.ea				= length - loop_start;
	header.ca				= header.sa;

	for (int i = 0; i < 16; i++)
		header.coef[i] = info.coef[i];

	header.gain				= info.gain;
	header.ps				= info.pred_scale;
	header.yn1				= info.yn1;
	header.yn2				= info.yn2;

	header.lps				= info.loop_pred_scale;
	header.lyn1				= info.loop_yn1;
	header.lyn2				= info.loop_yn2;

	file.write(header);
	file.writebuff(out, dsp.getBytesForAdpcmSamples(length));

#if 0
	uint32	nBytesToStore			= ;
	uint32	nibbleStartOffset		= getNibbleAddress(0);
	uint32	nibbleLoopStartOffset	= getNibbleAddress(loop_start);
	uint32	nibbleEndAddress		= getNibbleAddress(length);
#endif
	return true;
}

ISO_ptr<void> DSPFileHandler::Read(tag id, istream_ref file) {
	auto	dsp	= dsp_singleton();
	if (!dsp.dll)
		return ISO_NULL;

	DSPADPCMHEADER	header = file.get();
	uint32			length	= header.num_samples;

	ISO_ptr<sample> sm(id);
	sm->Create(length, 1, 16);
	sm->SetFrequency(header.sample_rate);

	ADPCMINFO		info;

	for (int i = 0; i < 16; i++)
		info.coef[i] = header.coef[i];

	info.gain				= header.gain;
	info.pred_scale			= header.ps;
	info.yn1				= header.yn1;
	info.yn2				= header.yn2;

	info.loop_pred_scale	= header.lps;
	info.loop_yn1			= header.lyn1;
	info.loop_yn2			= header.lyn2;

	uint32				inlen	= dsp.getBytesForAdpcmBuffer(length);
	malloc_block		in(inlen);
	malloc_block		out(dsp.getBytesForPcmBuffer(length));

	file.readbuff(in, inlen);
	dsp.decode(in, out, &info, length);
	memcpy(sm->Samples(), out, dsp.getBytesForPcmSamples(length));

	return sm;
}
