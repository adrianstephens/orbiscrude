#include "samplefile.h"
#include "iso/iso_convert.h"
#include "extra/filters.h"

namespace iso {

struct WAVformat;

//template<> struct static_list_first<WAVformat> {
//	typedef WAVformat T;
//	static iso_local T*&	first()	{ static T *first; return first; }
//};

void resample_chans(int16 *out, size_t out_size, int16 *in, size_t in_size, int channels) {
	if (channels == 1) {
		resample(out, in, int(out_size), int(in_size));

	} else for (int i = 0; i < channels; i++) {
		resample(
			strided(out + i, sizeof(int16) * channels),
			strided(in  + i, sizeof(int16) * channels),
			int(out_size), int(in_size)
		);
	}
}

ISO_openarray<int16> resample(sample *sm, float newfreq) {
	int		channels	= sm->Channels();
	int		bits		= sm->Bits();
	int		oldlength	= sm->Length();
	int		newlength	= int(oldlength * newfreq / sm->Frequency() + .5f);

	ISO_openarray<int16>	samples(newlength * channels);
	resample_chans(samples.begin(), newlength, sm->Samples(), oldlength, channels);
	return samples;
}

ISO_ptr<sample> resample(ISO_ptr<sample> &sm, float newfreq) {
	if (newfreq == sm->Frequency())
		return sm;

	int		channels	= sm->Channels();
	int		bits		= sm->Bits();
	int		oldlength	= sm->Length();
	int		newlength	= int(oldlength * newfreq / sm->Frequency() + .5f);

	ISO_ptr<sample> sm2;
	sm2.Create(sm.ID())->Create(newlength, channels, bits);
	sm2->SetFrequency(newfreq);
	sm2->flags = sm->flags;

	resample_chans(sm2->Samples(), newlength, sm->Samples(), oldlength, channels);
	return sm2;
}

}

#include "wav.h"

using namespace iso;

GUID WAV_GUID(ISO_WAVEFORMAT::FORMAT fmt) {
	GUID guid = {uint32(fmt),	0x0000, 0x0010,		{0x80, 0x00,	0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
	return guid;
}

static const GUID GUID_SUBTYPE_PCM			= {uint32(ISO_WAVEFORMAT::PCM),			0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID GUID_SUBTYPE_IEEE_FLOAT	= {uint32(ISO_WAVEFORMAT::IEEE_FLOAT),	0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

static const GUID GUID_SUBTYPE_AMBISONIC_B_FORMAT_PCM			= {uint32(ISO_WAVEFORMAT::PCM),			0x0721, 0x11d3, {0x86, 0x44, 0xC8, 0xC1, 0xCA, 0x00, 0x00, 0x00}};
static const GUID GUID_SUBTYPE_AMBISONIC_B_FORMAT_IEEE_FLOAT	= {uint32(ISO_WAVEFORMAT::IEEE_FLOAT),	0x0721, 0x11d3, {0x86, 0x44, 0xC8, 0xC1, 0xCA, 0x00, 0x00, 0x00}};

class WAVFileHandler : public SampleFileHandler, public WAVformat {
	struct CuePoint {
		int32le		dwIdentifier;
		int32le		dwPosition;
		int32le		fccChunk;
		int32le		dwChunkStart;
		int32le		dwBlockStart;
		int32le		dwSampleOffset;
	};

	struct SampleLoop {
		int32le		dwIdentifier;
		int32le		dwType;
		int32le		dwStart;
		int32le		dwEnd;
		int32le		dwFraction;
		int32le		dwPlayCount;
	};

	struct SamplerChunk {
		int32le		dwManufacturer;			// 0
		int32le		dwProduct;				// 0
		int32le		dwSamplePeriod;			// 5893
		int32le		dwMIDIUnityNote;		// 3c
		int32le		dwMIDIPitchFraction;	// 0
		int32le		dwSMPTEFormat;			// 0
		int32le		dwSMPTEOffset;			// 0
		int32le		cSampleLoops;			// 1
		int32le		cbSamplerData;			// 0
//		SampleLoop	Loops[];
	};

	struct ltxtChunk {
		int32le		dwIdentifier;
		int32le		dwLength;
		int32le		dwPurpose;
		int16le		wCountry;
		int16le		wLanguage;
		int16le		wDialect;
		int16le		wCodePage;
	};

	const char*		GetExt()				override { return "wav";		}
	const char*		GetMIME()				override { return "audio/wave"; }
	const char*		GetDescription()		override { return "Wave";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		if (file.get<uint32>() != "RIFF"_u32)
			return CHECK_DEFINITE_NO;
		file.get<uint32>();
		return file.get<uint32>() == "WAVE"_u32 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;

public:
	bool operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm) {
		bool	pcm = fmt->wFormatTag == ISO_WAVEFORMAT::PCM || fmt->wFormatTag == 0xfe02;
		if (!pcm) {
			if (fmt->wFormatTag == ISO_WAVEFORMAT::EXTENSIBLE) {
				if (fmt->cbSize >= sizeof(ISO_WAVEFORMATEXTENSIBLE))
					pcm = ((ISO_WAVEFORMATEXTENSIBLE*)fmt)->SubFormat == GUID_SUBTYPE_PCM;
				else if (fmt->cbSize >= sizeof(ISO_WAVEFORMATEX1))
					pcm = ((ISO_WAVEFORMATEX1*)fmt)->unknown == 0x10;
			}
		}

		if (pcm) {
			sm->Create(chunk.remaining() / ((fmt->wBitsPerSample + 7) / 8 * fmt->nChannels), fmt->nChannels, 16);//fmt->wBitsPerSample);
			sm->SetFrequency(float(fmt->nSamplesPerSec));

			if (fmt->wBitsPerSample == 8) {
				uint32	len		= chunk.remaining();
				malloc_block	buffer(len);
				chunk.readbuff(buffer, len);
				for (unsigned i = 0; i < len; i++)
					sm->Samples()[i] = ((int8*)buffer)[i] * 256;

			} else {
				chunk.readbuff(sm->Samples(), chunk.remaining());
				if (fmt->wBitsPerSample == 16) {
					uint16		*p = (uint16*)sm->Samples();
					if (fmt->wFormatTag == 0xfe02) {
#ifndef ISO_BIGENDIAN
						for (int n = sm->Length() * sm->Channels(); --n; p++)
							*p	= *(uint16be*)p;
#endif
					} else {
#ifdef ISO_BIGENDIAN
						for (int n = sm->Length() * sm->Channels(); --n; p++)
							*p	= *(uint16le*)p;
#endif
					}
				}
			}
			return true;
		}
		return false;
	}

	WAVFileHandler() : WAVformat(this)		{ ISO::getdef<sample>(); }
} wav;

struct WAVformatIEEE : public WAVformat {
	bool operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm) {
		if (fmt->wFormatTag != ISO_WAVEFORMAT::IEEE_FLOAT)
			return false;

		sm->Create(chunk.remaining() / ((fmt->wBitsPerSample + 7) / 8 * fmt->nChannels), fmt->nChannels, 16);//fmt->wBitsPerSample);
		sm->SetFrequency(float(fmt->nSamplesPerSec));

		int16		*p = sm->Samples();
		for (int n = sm->Length() * sm->Channels(); --n; p++)
			*p	= int16(clamp(chunk.get<floatle>(), -1.f, +1.f) * 32767);

		return true;
	}
	WAVformatIEEE() : WAVformat(this)		{}
} wav_ieee;

ISO_ptr<void> WAVFileHandler::Read(tag id, istream_ref file) {
	RIFF_chunk	riff(file);
	if (riff.id != "RIFF"_u32 || file.get<uint32>() != "WAVE"_u32)
		return ISO_NULL;

	ISO_ptr<sample>		sm(id);
	malloc_block		fmt;

	while (riff.remaining()) {
		RIFF_chunk	chunk(file);
		switch (chunk.id) {
			case "fmt "_u32:
				fmt = malloc_block::unterminated(chunk);
				break;
			case "smpl"_u32: {
				SamplerChunk	smpl;
				file.read(smpl);
				if (int n = smpl.cSampleLoops)
					sm->flags |= sample::LOOP;
				break;
			}
			case "data"_u32:
				for (WAVformat::iterator i = WAVformat::begin(), e = WAVformat::end(); i != e; ++i) {
					if ((*i)(fmt, chunk, sm))
						break;
				}
				break;
#if 0
			case "LIST"_u32: {
				void	*data;
				uint32	id = file.get();
				while (chunk.remaining()) {
					RIFF_chunk	subchunk(file);
					printf("%.4s\n", &subchunk.id);
					data = malloc_block::unterminated(subchunk);
				};
				break;
			}
#endif
		}
	};
	return sm;
}

bool WAVFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p)) {
		if (sm->Samples()) {
			RIFF_Wchunk	riff(file, "RIFF"_u32);
			riff.write("WAVE"_u32);

			ISO_WAVEFORMATEX	fmt;
			fmt.wFormatTag		= ISO_WAVEFORMAT::PCM;
			fmt.nChannels		= sm->Channels();
			fmt.nSamplesPerSec	= uint32(sm->Frequency());
			fmt.nAvgBytesPerSec	= uint32(sm->Frequency() * sm->BytesPerSample());
			fmt.nBlockAlign		= sm->BytesPerSample();
			fmt.wBitsPerSample	= sm->Bits();
			fmt.cbSize			= 0;
			RIFF_Wchunk(file, "fmt "_u32).write(fmt);

			RIFF_Wchunk(file, "data"_u32).writebuff(sm->Samples(), sm->Length() * sm->BytesPerSample());

			if (sm->Flags() & sample::LOOP) {
				RIFF_Wchunk	chunk(file, "smpl"_u32);

			SamplerChunk	smpl;
			clear(smpl);
			smpl.dwSamplePeriod		= int32(1e9f / sm->Frequency());
			smpl.dwMIDIUnityNote	= 60;
			smpl.cSampleLoops		= 1;
			chunk.write(smpl);

				SampleLoop		loop;
				clear(loop);
				loop.dwIdentifier	= 1;
				loop.dwEnd			= sm->Length() - 1;
				chunk.write(loop);
			}
			return true;
		}
	}
	return false;
}
