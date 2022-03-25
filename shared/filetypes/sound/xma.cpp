#include "samplefile.h"
#include "xma.h"
#include "iso/iso_convert.h"
#include "filetypes/RIFF.h"

#if defined PLAT_PC && defined CROSS_PLATFORM
#include "xmaencoder.h"
#endif

using namespace iso;

class XMAFileHandler : public SampleFileHandler {

	struct XMASTREAMFORMAT {
		uint32le	PsuedoBytesPerSec;
		uint32le	SampleRate;
		uint32le	LoopStart;
		uint32le	LoopEnd;
		uint8		SubframeData;
		uint8		Channels;
		uint16		ChannelMask;
	};

	struct XMAWAVEFORMAT {
		uint16le	FormatTag;
		uint16le	BitsPerSample;
		uint16le	EncodeOptions;
		uint16le	LargestSkip;
		uint16le	NumStreams;
		uint8		LoopCount;
		uint8		Version;
		XMASTREAMFORMAT XmaStreams[1]; // Format info for each stream (can grow based on wNumStreams).
	};
	struct XMA2STREAMFORMAT {
		uint8		Channels;
		uint8		RESERVED;
		uint16be	ChannelMask;
	};
	struct XMA2WAVEFORMAT {
		uint8		Version;
		uint8		NumStreams;
		uint8		RESERVED;

		uint8		LoopCount;
		uint32be	LoopBegin;
		uint32be	LoopEnd;

		uint32be	SampleRate;

		uint16be	EncodeOptions;
		uint32be	PsuedoBytesPerSec;

		uint32be	BlockSizeInBytes;
		uint32be	SamplesEncoded;
		uint32be	SamplesInSource;
		uint32be	BlockCount;

		XMA2STREAMFORMAT	Streams[1]; // Format info for each stream (can grow based on NumStreams)
	};

	const char*			GetExt() override { return "xma";	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
#if defined PLAT_PC && defined CROSS_PLATFORM
	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
#endif
} xma;

ISO_ptr<void> XMAFileHandler::Read(tag id, istream_ref file)
{
	RIFF_chunk	riff(file);
	if (riff.id != "RIFF"_u32 || file.get<uint32>() != "WAVE"_u32)
		return ISO_NULL;

	unique_ptr<XMAWAVEFORMAT>		fmt;
	unique_ptr<XMA2WAVEFORMAT>	fmt2;
	unique_ptr<uint32>			seek;
	uint32			data_offset = 0, data_size = 0;

	while (riff.remaining()) {
		RIFF_chunk	chunk(file);
		switch (chunk.id) {
			case "fmt "_u32:
				fmt			= malloc_block::unterminated(chunk);
				break;

			case "XMA2"_u32:
				fmt2		= malloc_block::unterminated(chunk);
				break;

			case "seek"_u32:
				seek		= malloc_block::unterminated(chunk);
				break;

			case "data"_u32:
				data_offset = uint32(file.tell());
				data_size	= chunk.remaining();
				break;
		}
	};
	ISO_ptr<XMA> s(id);

	if (fmt2) {
		s->Create(data_size, fmt2->NumStreams);
		for (int i = 0; i < fmt2->NumStreams; i++) {
			s->streams[i].channels = fmt2->Streams[i].Channels;
			s->streams[i].frequency = float(fmt2->SampleRate);
		}
		if (fmt2->LoopCount)
			s->flags |= XMA::LOOP;
		s->numsamples	= fmt2->SamplesInSource;

	} else if (fmt) {
		s->Create(data_size, fmt->NumStreams);
		for (int i = 0; i < fmt->NumStreams; i++) {
			s->streams[i].channels = fmt->XmaStreams[i].Channels;
			s->streams[i].frequency = float(fmt->XmaStreams[i].SampleRate);
		}
		if (fmt->LoopCount)
			s->flags |= XMA::LOOP;
	}

	file.seek(data_offset);
	file.readbuff(s->data, data_size);

	return s;
}

#if defined PLAT_PC && defined CROSS_PLATFORM
bool XMAFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (p.GetType()->SameAs<sample>()) {
		sample			*s = p;
		XMAENCODERSTREAM	stream;
		void*				buffer;
		DWORD				buffer_size;
//		::XMA2WAVEFORMAT*	format;
		XMA2WAVEFORMATEX*	format;
		DWORD				format_size;
		DWORD*				seeks;
		DWORD				seeks_size;

		clear(stream);
		stream.Format.Format.wFormatTag		= WAVE_FORMAT_PCM;
		stream.Format.Format.nChannels		= s->Channels();
		stream.Format.Format.nSamplesPerSec	= DWORD(s->Frequency());
		stream.Format.Format.nAvgBytesPerSec= DWORD(s->Frequency() * s->BytesPerSample());
		stream.Format.Format.nBlockAlign	= s->BytesPerSample();
		stream.Format.Format.wBitsPerSample	= s->Bits();

		stream.pBuffer		= s->Samples();
		stream.BufferSize	= s->Length() * s->BytesPerSample();
		switch (s->Channels()) {
			case 1:
				stream.SpeakerAssignment[0] = XMA_SPEAKER_CENTER;
				break;
			case 2:
				stream.SpeakerAssignment[0] = XMA_SPEAKER_LEFT;
				stream.SpeakerAssignment[1] = XMA_SPEAKER_RIGHT;
				break;
		}

		XAudio2XMAEncoder(1, &stream, 60, 0, 64, &buffer, &buffer_size, &format, &format_size, &seeks, &seeks_size);

		RIFF_Wchunk	riff(file, "RIFF"_u32);
		file.write("WAVE"_u32);

		RIFF_Wchunk(file, "XMA2"_u32).writebuff(format,	format_size);
		RIFF_Wchunk(file, "seek"_u32).writebuff(seeks,	seeks_size);
		RIFF_Wchunk(file, "data"_u32).writebuff(buffer,	buffer_size);

		free(buffer);
		free(format);
		free(seeks);
		return true;
	} else if (p.GetType()->SameAs<XMA>()) {
		XMA		*s = p;

		RIFF_Wchunk	riff(file, "RIFF"_u32);
		file.write("WAVE"_u32);

		XMA2WAVEFORMAT	format;
		clear(format);
		format.Version		= 3;
		format.NumStreams	= 1;
		format.SampleRate	= uint32(s->streams[0].frequency);
		format.Streams[0].Channels		= 1;
		format.Streams[0].ChannelMask	= 1;//??

		RIFF_Wchunk(file, "XMA2"_u32).writebuff(&format,	sizeof(format));
//		RIFF_Wchunk(file, "seek"_u32).writebuff(seeks,		seeks_size);
		RIFF_Wchunk(file, "data"_u32).writebuff(s->data,	s->data.Count());

		return true;
	}
	return false;
}

class XMA_conversion : public ISO_conversion {
public:
	ISO_ptr<void> operator()(ISO_ptr<void> p, const ISO::Type *type, int flags) {
		if (type->SameAs<XMA>()) {
			if (ISO_ptr<sample> s = convert<sample>(p)) {
				bool	loop = !!(s->flags & sample::LOOP);
				if (loop && (s->Length() & 127)) {
					ISO_ptr<void> Resample(ISO_ptr<void> p, float newfreq);
					s = Resample(s, ((s->Length() + 64) & ~127) * s->Frequency() / s->Length());
				}

				ISO_ptr<XMA>		xma(s.ID());

				XMAENCODERSTREAM	stream;
				void*				buffer	= NULL;
				DWORD				buffer_size;
				XMA2WAVEFORMATEX*	format	= NULL;
				DWORD				format_size;
				DWORD*				seeks	= NULL;
				DWORD				seeks_size;

				clear(stream);
				stream.Format.Format.wFormatTag		= WAVE_FORMAT_PCM;
				stream.Format.Format.nChannels		= s->Channels();
				stream.Format.Format.nSamplesPerSec	= DWORD(s->Frequency());
				stream.Format.Format.nAvgBytesPerSec= DWORD(s->Frequency() * s->BytesPerSample());
				stream.Format.Format.nBlockAlign	= s->BytesPerSample();
				stream.Format.Format.wBitsPerSample	= s->Bits();

				stream.pBuffer		= s->Samples();
				stream.BufferSize	= s->Length() * s->BytesPerSample();
				stream.LoopLength	= s->Length();

				switch (s->Channels()) {
					case 1:
						stream.SpeakerAssignment[0] = XMA_SPEAKER_CENTER;
						break;
					case 2:
						stream.SpeakerAssignment[0] = XMA_SPEAKER_LEFT;
						stream.SpeakerAssignment[1] = XMA_SPEAKER_RIGHT;
						break;
				}
#if 1
				if (FAILED(XAudio2XMAEncoder(1, &stream, 60, loop ? XMAENCODER_LOOP : 0, 64,
					&buffer, &buffer_size,
					&format, &format_size,
					&seeks, &seeks_size
				)))
					return ISO_NULL;
				xma->Create(buffer_size, 1);
				xma->streams[0].channels	= s->Channels();
				xma->streams[0].frequency	= s->Frequency();
				xma->numsamples				= s->Length();
				xma->flags					= loop ? XMA::LOOP : 0;
				memcpy(xma->data, buffer, buffer_size);

				free(buffer);
				free(format);
				free(seeks);
#endif
				return xma;
			}
		} else if (p.GetType()->SameAs<XMA>()) {
			//decode here
		}
		return ISO_NULL;
	}
	XMA_conversion() : ISO_conversion(this)	{}
} xma_conversion;

#endif
