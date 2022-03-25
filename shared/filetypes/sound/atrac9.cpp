#include "samplefile.h"
#include "iso/iso_convert.h"
#include "wav.h"
#include "extra/filters.h"

#include "SCE\Common\External Tools\ATRAC9 Windows Library\at9\include\libatrac9.h"

using namespace iso;

static const GUID GUID_SUBTYPE_ATRAC9 = { 0x47e142d2, 0x36ba, 0x4d8d, { 0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c } };

class ATRAC9FileHandler : public SampleFileHandler, public WAVformat {
	struct Header : ISO_WAVEFORMATEXTENSIBLE {
		enum {
			SONY_ATRAC9_WAVEFORMAT_VERSION		= 1,
			SONY_ATRAC9_WAVEFORMAT_VERSION_BEX	= 2,
			AT9_MULTICHVERSION_PARAM_LF			= 1,
			AT9_MULTICHVERSION_PARAM_FF			= 2,
			AT9_MULTICHVERSION_PARAM_CF			= 0,
		};
		uint32	wVersionInfo;	// Version Information of the ATRAC-X WAVE Header
		uint8	configData[SCE_AT9_CONFIG_DATA_SIZE];
		uint8	reserved[4];

		Header()	{}
		Header(const SceAt9CodecInfo &info) {
			static const uint32 channel_masks[] = {
				0,
				ISO_WAVEFORMATEXTENSIBLE::MONO,
				ISO_WAVEFORMATEXTENSIBLE::STEREO,
				ISO_WAVEFORMATEXTENSIBLE::CHANS_2_1,
				ISO_WAVEFORMATEXTENSIBLE::CHANS_4_0,
				ISO_WAVEFORMATEXTENSIBLE::CHANS_5_0,
				ISO_WAVEFORMATEXTENSIBLE::CHANS_5_1,
				ISO_WAVEFORMATEXTENSIBLE::CHANS_7_0,
				ISO_WAVEFORMATEXTENSIBLE::CHANS_7_1
			};
			clear(*this);
			int blockSamples		= info.frameSamples * info.framesInSuperframe;

			wFormatTag		= ISO_WAVEFORMAT::EXTENSIBLE;
			nChannels		= info.channels;
			nSamplesPerSec	= info.samplingRate;
			nAvgBytesPerSec	= (info.superframeSize * info.samplingRate + blockSamples / 2) / blockSamples;
			nBlockAlign		= info.superframeSize;
			wBitsPerSample	= 0;
			cbSize			= 0x22;//sizeof(header);

			Samples.wSamplesPerBlock	= blockSamples;
			dwChannelMask	= channel_masks[info.channels];
			SubFormat		= GUID_SUBTYPE_ATRAC9;

			wVersionInfo	= SONY_ATRAC9_WAVEFORMAT_VERSION;
			memcpy(configData, info.configData, SCE_AT9_CONFIG_DATA_SIZE);
		}

	};

	const char*		GetExt() override { return "at9";	}
	bool					Read(istream_ref file, sample *sm, uint8 *config);
	bool					Write(ostream_ref file, sample *sm, int superframeSize, int framesInSuperframe);
	bool					Write(ostream_ref file, sample *sm, HANDLE_ATRAC9	h);

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;

public:
	bool operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm) {
		Header	*header = (Header*)fmt;
		return header->wFormatTag == ISO_WAVEFORMAT::EXTENSIBLE && header->SubFormat == GUID_SUBTYPE_ATRAC9 && Read(chunk, sm, header->configData);
	}
	ATRAC9FileHandler() : WAVformat(this) {}
} atrac9;

struct factChunk {
	uint32	total;
	uint32	_;
};
struct factChunkExt : factChunk {
	uint32	encdelay;

	factChunkExt()	{}
	factChunkExt(uint32 _total, uint32 _encdelay) {
		total		= _total;
		_			= _encdelay;
	}
};

ISO_ptr<void> ATRAC9FileHandler::Read(tag id, istream_ref file) {
	RIFF_chunk	riff(file);
	if (riff.id != "RIFF"_u32 || file.get<uint32>() != "WAVE"_u32)
		return ISO_NULL;

	ISO_ptr<sample>		sm(id);
	malloc_block		fmt;
	uint32				total = 0, encdelay = 0;

	while (riff.remaining()) {
		RIFF_chunk	chunk(file);
		switch (chunk.id) {
			case "fmt "_u32:
				fmt = malloc_block::unterminated(chunk);
				break;

			case "fact"_u32:
				if (chunk.remaining() >= sizeof(factChunkExt)) {
					factChunkExt	fact;
					file.read(fact);
					total		= fact.total;
					encdelay	= fact.encdelay;
				} else {
					factChunk	fact;
					file.read(fact);
					total		= fact.total;
				}
				break;

			case "data"_u32: {
				Header	*h = fmt;
				if (h->wFormatTag != ISO_WAVEFORMAT::EXTENSIBLE || h->SubFormat != GUID_SUBTYPE_ATRAC9 || !Read(chunk, sm, h->configData))
					return ISO_NULL;
			}
		}
	};
	return sm;
}

bool ATRAC9FileHandler::Read(istream_ref file, sample *sm, uint8 *config) {
	HANDLE_ATRAC9	h = sceAt9GetHandle();
	sceAt9DecInit(h, config, SCE_AT9_WORD_LENGTH_16BIT);

	SceAt9CodecInfo		info;
	sceAt9GetCodecInfo(h, &info);

	int		channels			= info.channels;
	uint32	frameSamples		= info.frameSamples;
	uint32	framesInSuperframe	= info.framesInSuperframe;
	uint32	superframeSize		= info.superframeSize;

	uint32	superframes	= file.length() / superframeSize;
	int16	*samples	= sm->Create(superframes * framesInSuperframe * frameSamples, channels, 16);
	sm->SetFrequency(info.samplingRate);

	malloc_block	buffer(superframeSize);
	for (int s = 0; s < superframes; s++) {
		uint32	r		= file.readbuff(buffer, superframeSize);
		uint8	*input	= buffer;
		for (int i = 0; i < framesInSuperframe; i++) {
			int		used;
			sceAt9DecDecode(h, input, &used, samples, 0, frameSamples);
			samples	+= frameSamples * channels;
			input	+= used;
		}
	}

	sceAt9ReleaseHandle(h);
	return true;
}

bool ATRAC9FileHandler::Write(ostream_ref file, sample *sm, HANDLE_ATRAC9 h) {
	SceAt9CodecInfo		info;
	sceAt9GetCodecInfo(h, &info);

	malloc_block	output(info.superframeSize);
	int16			*input		= sm->Samples();
	uint32			remaining	= sm->Length();
	int				outsize, terminate_flag;

	uint32			samps	= info.framesInSuperframe * info.frameSamples;
	while (remaining) {
		uint32	num = min(remaining, samps);
		sceAt9EncEncode(h, input, num, output, &outsize);
		file.writebuff(output, outsize);
		input		+= num * info.channels;
		remaining	-= num;
	}

	sceAt9EncFlush(h, output, &outsize, &terminate_flag);
	return true;
}

bool ATRAC9FileHandler::Write(ostream_ref file, sample *sm, int superframeSize, int framesInSuperframe) {
	HANDLE_ATRAC9	h = sceAt9GetHandle();

	SceAt9EncInitParam	init;
	SceAt9EncExtParam	ext;

	clear(init);
	init.channels			= sm->Channels();
	init.samplingRate		= sm->Frequency();
	init.superframeSize		= superframeSize;
	init.framesInSuperframe	= framesInSuperframe;
	init.wlength			= SCE_AT9_WORD_LENGTH_16BIT;

	ext.nbands				= SCE_AT9_PARAM_UNSET;
	ext.isband				= SCE_AT9_PARAM_UNSET;
	ext.gradMode			= SCE_AT9_PARAM_UNSET;

	sceAt9EncSetEncParam(h, 1, 0, 0, 0);
	sceAt9EncInit(h, &init, &ext);
	Write(file, sm, h);
	sceAt9ReleaseHandle(h);
	return true;
}

bool ATRAC9FileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p)) {
		uint32	rate	= sm->Frequency() <= 24000 ? 24000
						: sm->Frequency() <= 12000 ? 12000
						: 48000;

		sm = resample(sm, rate);

		int	channels			= sm->Channels();
		int superframeSize		= 128;
		int framesInSuperframe	= 1;

		switch (channels) {
			case 1:
				superframeSize		= 32;
				framesInSuperframe	= 1;
				break;
			case 2:
				superframeSize		= 48;
				framesInSuperframe	= 1;
				break;
		}


		HANDLE_ATRAC9	h = sceAt9GetHandle();
		SceAt9EncInitParam	init;
		SceAt9EncExtParam	ext;
		SceAt9CodecInfo		info;

		clear(init);
		init.channels			= sm->Channels();
		init.samplingRate		= rate;
		init.superframeSize		= superframeSize;
		init.framesInSuperframe	= framesInSuperframe;
		init.wlength			= SCE_AT9_WORD_LENGTH_16BIT;

		ext.nbands				= SCE_AT9_PARAM_UNSET;
		ext.isband				= SCE_AT9_PARAM_UNSET;
		ext.gradMode			= SCE_AT9_PARAM_UNSET;

		sceAt9EncSetEncParam(h, 1, 0, 0, 0);
		sceAt9EncInit(h, &init, &ext);
		sceAt9GetCodecInfo(h, &info);

		int	frameSamples		= info.frameSamples;

		RIFF_Wchunk	riff(file, "RIFF"_u32);
		riff.write("WAVE"_u32);

		RIFF_Wchunk(file, "fmt "_u32).write(Header(info));
		RIFF_Wchunk(file, "fact"_u32).write(factChunkExt(sm->Length(), frameSamples + SCE_AT9_ENCODER_DELAY_SAMPLES));

		Write(lvalue(RIFF_Wchunk(file, "data"_u32)), sm, h);
		sceAt9ReleaseHandle(h);
		return true;
	}
	return false;
}

