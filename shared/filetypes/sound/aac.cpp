#include "samplefile.h"
#include "faad2-2.7\include\neaacdec.h"

using namespace iso;

int id3v2_tag(uint8 *p) {
	return p[0] == 'I' && p[1] == 'D' && p[2] == '3'
		? ((p[6] << 21) | (p[7] << 14) | (p[8] <<  7) | p[9]) + 10
		: 0;
}

class AACFileHandler : public SampleFileHandler {
	const char*		GetExt()			{ return "aac";	}
	const char*		GetDescription()	{ return "AAC";	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		uint8	*buffer = (uint8*)malloc(16384), *p = buffer, *end = buffer + 16384;
		file.readbuff(buffer, 16384);

		if (int tagsize = id3v2_tag(buffer))
			p += tagsize;

		NeAACDecHandle				decoder	= NeAACDecOpen();
		NeAACDecConfigurationPtr	config	= NeAACDecGetCurrentConfiguration(decoder);

//		config->defObjectType	= opt->object_type;
//		config->outputFormat	= opt->output_format;
//		NeAACDecSetConfiguration(decoder, config);

		uint32	consumed;
		ulong	samplerate;
		uint8	channels;
		if ((consumed = NeAACDecInit(decoder, p, end - p, &samplerate, &channels)) < 0) {
			NeAACDecClose(decoder);
			return ISO_NULL;
		}

		ISO_ptr<sample> sm(id);
		sm->SetFrequency(samplerate);
		sm->Create(0, channels, 16);

		NeAACDecFrameInfo info;
		for (;;) {
			void *samples = NeAACDecDecode(decoder, &info, p, end - p);
			if (info.error == 0 && info.samples > 0) {
				memcpy(sm->Extend(info.samples), samples, info.samples * sizeof(uint16) * channels);
			}
			p += info.bytesconsumed;
		}
		return sm;
	}
} aac3;
