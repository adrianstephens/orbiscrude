#include "samplefile.h"
#include "iso/iso_convert.h"
#include "wav.h"

#include "ogg/ogg.h"
#include "vorbis/vorbisenc.h"
#include "vorbis/codec.h"
#include "opus.h"

using namespace iso;

class OGGFileHandler : public SampleFileHandler {
	struct OGG {
		ogg_sync_state		sync;		// sync and verify incoming physical bitstream
		ogg_stream_state	stream;		// take physical pages, weld into a logical stream of packets
		ogg_page			page;		// one Ogg bitstream page. Vorbis packets are inside
		ogg_packet			packet;		// one raw packet of data for decode

		OGG()	{ ogg_sync_init(&sync); }
		~OGG()	{ ogg_sync_clear(&sync); }

		bool	get_page(istream_ref file) {
			int	result;
			while ((result = ogg_sync_pageout(&sync, &page)) != 1) {
				char	*buffer	= ogg_sync_buffer(&sync, 8192);
				if (auto bytes = file.readbuff(buffer, 8192))
					ogg_sync_wrote(&sync, bytes);
				else
					break;
			}
			return result == 1;
		}

		ogg_packet	*next_packet(istream_ref file) {
			for (;;) {
				int result = ogg_stream_packetout(&stream, &packet);
				if (result == 1)
					return &packet;
				if (!get_page(file) || ogg_stream_pagein(&stream, &page))
					return 0;
			}
		}

		bool	begin_stream(istream_ref file) {
			return get_page(file)
				&& ogg_stream_init(&stream, ogg_page_serialno(&page)) == 0	// Get the serial number and set up the rest of decode. serialno first; use it to set up a logical stream
				&& ogg_stream_pagein(&stream, &page) == 0;
		}
		void	end_stream() {
			ogg_stream_clear(&stream);
		}

		bool	eos()		{ return !!ogg_page_eos(&page); }

	};
	ISO_ptr<void>			ReadVorbis(tag id, istream_ref file, OGG &ogg);
	ISO_ptr<void>			ReadOpus(tag id, istream_ref file, OGG &ogg);
protected:
	ISO_ptr<void>			Read(tag id, istream_ref file) override;
};

ISO_ptr<void> OGGFileHandler::Read(tag id, istream_ref file) {
	ISO_ptr<sample>	sm;
	OGG				ogg;
#if 0
	while (ogg.Begin(file)) {
		if (vorbis_synthesis_idheader(&ogg.packet)) {
			sm = ReadVorbis(id, file, ogg);
		} else {
			sm = ReadOpus(id, file, ogg);
		}
		ogg.End();
	}
#else
	while (ogg.begin_stream(file)) {
		ogg_packet	*packet = ogg.next_packet(file);
		if (vorbis_synthesis_idheader(packet)) {
			sm = ReadVorbis(id, file, ogg);
		} else {
			sm = ReadOpus(id, file, ogg);
		}
		ogg.end_stream();
	}
#endif
	return sm;
}

ISO_ptr<void> OGGFileHandler::ReadVorbis(tag id, istream_ref file, OGG &ogg) {
	vorbis_info			vi; // struct that stores all the static vorbis bitstream settings
	vorbis_comment		vc; // struct that stores all the bitstream user comments
	vorbis_dsp_state	vd; // central working state for the packet->PCM decoder
	vorbis_block		vb; // local working space for packet->PCM decode

	ISO_ptr<sample>		sm;

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	if (vorbis_synthesis_headerin(&vi, &vc, &ogg.packet) < 0)
		return sm;

	if (vorbis_synthesis_headerin(&vi, &vc, ogg.next_packet(file)) < 0)
		return sm;

	if (vorbis_synthesis_headerin(&vi, &vc, ogg.next_packet(file)) < 0)
		return sm;

	// Initialize the Vorbis packet->PCM decoder.
	if (vorbis_synthesis_init(&vd, &vi) == 0) {
		vorbis_block_init(&vd, &vb);	// local state for most of the decode so multiple block decodes can proceed in parallel. We could init multiple vorbis_block structures for vd here

		malloc_chain	chain;
		while (!ogg.eos()) {
			while (ogg_packet *packet = ogg.next_packet(file)) {
				if (vorbis_synthesis(&vb, &ogg.packet) == 0) // test for success!
					vorbis_synthesis_blockin(&vd, &vb);

				float	**pcm;
				int		samples;
				while ((samples = vorbis_synthesis_pcmout(&vd, &pcm)) > 0) {
					auto	mem = chain.push_back(sizeof(uint16) * samples * vi.channels);
					for (int i = 0; i < vi.channels; i++) {
						ogg_int16_t	*ptr	= (ogg_int16_t*)mem + i;
						float		*mono	= pcm[i];
						for (int j = 0; j < samples; j++) {
							*ptr = clamp(int(mono[j] * 32767.f + .5f), -32768, 32767);
							ptr += vi.channels;
						}
					}
					vorbis_synthesis_read(&vd, samples); // tell libvorbis how many samples we actually consumed
				}
			}
		}

		// ogg_page and ogg_packet structs always point to storage in libvorbis.  They're never freed or manipulated directly
		vorbis_block_clear(&vb);
		vorbis_dsp_clear(&vd);

		sm.Create(id);
		sm->SetFrequency(vi.rate);
		chain.copy_to(sm->Create(uint32(chain.total() / (vi.channels * sizeof(int16))), vi.channels, 16));
	}

	// clean up this logical bitstream; before exit we see if we're followed by another [chained] */
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);  // must be called last
	return sm;
}

ISO_ptr<void> OGGFileHandler::ReadOpus(tag id, istream_ref file, OGG &ogg) {
	const uint8 *data	= ogg.packet.packet;
	int			bw		= opus_packet_get_bandwidth(data);
	int			ch		= opus_packet_get_nb_channels(data);

	int			error;
	OpusDecoder *dec = opus_decoder_create(48000, ch, &error);

	malloc_chain	chain;
	while (!ogg.eos()) {
		while (ogg_packet *packet = ogg.next_packet(file)) {
			int16	pcm[5760 * 2];
			int		samples	= opus_decode(dec, packet->packet, packet->bytes, pcm, 5760, 0);
			if (samples > 0)
				chain.push_back(const_memory_block(pcm, sizeof(uint16) * samples * ch));
		}
	}

	opus_decoder_destroy(dec);

	ISO_ptr<sample>		sm(id);
	sm->SetFrequency(48000);
	chain.copy_to(sm->Create(uint32(chain.total() / (ch * sizeof(int16))), ch, 16));
	return sm;
}


//-----------------------------------------------------------------------------
class OGGVorbisFileHandler : public OGGFileHandler, public WAVformat {
	enum { ISO_WAVE_FORMAT_VORBIS  = 0xFFFF };

	struct AkVorbisLoopInfo {
		uint32 dwLoopStartPacketOffset;	// File offset of packet containing loop start sample
		uint32 dwLoopEndPacketOffset;	// File offset of packet following the one containing loop end sample
		uint16 uLoopBeginExtra;			// Number of extra audio frames in loop begin
		uint16 uLoopEndExtra;			// Number of extra audio frames in loop end
	};

	struct AkVorbisSeekTableItem {
		uint16 uPacketFrameOffset;		// Granule position (first PCM frame) of Ogg packet
		uint16 uPacketFileOffset;		// File offset of packet in question
	};

	struct AkVorbisHeaderBase {
		uint32 dwTotalPCMFrames;		// Total number of encoded PCM frames
	};

	struct AkVorbisInfo {
		AkVorbisLoopInfo LoopInfo;		// Looping related information
		uint32 dwSeekTableSize;			// Size of seek table items (0 == not present)
		uint32 dwVorbisDataOffset;		// Offset in data chunk to first audio packet
		uint16 uMaxPacketSize;			// Maximum packet size
		uint16 uLastGranuleExtra;		// Number of extra audio frames in last granule
		uint32 dwDecodeAllocSize;		// Decoder expected allocation size
		uint32 dwDecodeX64AllocSize;	// Decoder expected allocation size on platform with 64bits pointers.
		uint32 uHashCodebook;
		uint8  uBlockSizes[2];
	};

	struct AkVorbisHeader : public AkVorbisHeaderBase, public AkVorbisInfo {};

	const char*		GetExt()				override { return "ogg";		}
	const char*		GetMIME()				override { return "audio/ogg"; }
	const char*		GetDescription()		override { return "Ogg Vorbis";	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	bool operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm) {
		if (fmt->wFormatTag != ISO_WAVE_FORMAT_VORBIS)
			return false;
		if (ISO_ptr<sample> p = Read(0, chunk))
			return true;
		return false;
	}
	OGGVorbisFileHandler() : WAVformat(this) {}
} vorbis;


bool OGGVorbisFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p);
	if (!sm)
		return false;

	ogg_stream_state	os; /* take physical pages, weld into a logical stream of packets */
	ogg_page			og; /* one Ogg bitstream page.  Vorbis packets are inside */
	ogg_packet			op; /* one raw packet of data for decode */
	vorbis_info			vi; /* struct that stores all the static vorbis bitstream settings */
	vorbis_comment		vc; /* struct that stores all the user comments */
	vorbis_dsp_state	vd; /* central working state for the packet->PCM decoder */
	vorbis_block		vb; /* local working space for packet->PCM decode */

	float	quality	= 0.0f;
	if (vorbis_encode_init_vbr(&vi, sm->Channels(), sm->Frequency(), quality))
		return false;

	vorbis_comment_init(&vc);
	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);
	ogg_stream_init(&os, 0);
	{
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
		ogg_stream_packetin(&os, &header); // automatically placed in its own page
		ogg_stream_packetin(&os, &header_comm);
		ogg_stream_packetin(&os, &header_code);

		// This ensures the actual audio data will start on a new page, as per spec
		while (ogg_stream_flush(&os, &og)) {
			file.writebuff(og.header, og.header_len);
			file.writebuff(og.body, og.body_len);
		}
	}

	int16	*in		= (int16*)sm->Samples();
	int		rest	= sm->Length();
	int		iread;
	do {
		if (iread = min(rest, 1024 * 10)) {
			float **buffer = vorbis_analysis_buffer(&vd, iread);
			for (int i = 0; i < iread; i++) {
				for (int c = 0; c < sm->Channels(); c++)
					buffer[c][i] = *in++ / 32768.f;
			}

			// tell the library how much we actually submitted
			vorbis_analysis_wrote(&vd, iread);
		} else {
			// EOS
			vorbis_analysis_wrote(&vd, 0);
		}

		while (vorbis_analysis_blockout(&vd, &vb) == 1) {
			// analysis, assume we want to use bitrate management
			vorbis_analysis(&vb, NULL);
			vorbis_bitrate_addblock(&vb);

			while (vorbis_bitrate_flushpacket(&vd, &op)) {
				// weld the packet into the bitstream
				ogg_stream_packetin(&os, &op);

				// write out pages (if any)
				do {
					if (ogg_stream_pageout(&os, &og) == 0)
						break;
					file.writebuff(og.header, og.header_len);
					file.writebuff(og.body, og.body_len);

					// this could be set above, but for illustrative purposes, I do it here (to show that vorbis does know where the stream ends)
				} while (!ogg_page_eos(&og));
			}
		}
		rest -= iread;
	} while (iread);

	return true;
}

//-----------------------------------------------------------------------------

class OGGOpusFileHandler : public OGGFileHandler, public WAVformat {
	enum { ISO_WAVE_FORMAT_VORBIS  = 0xFFFF };

	const char*		GetExt()				override { return "opus"; }
	const char*		GetMIME()				override { return "audio/opus"; }
	const char*		GetDescription()		override { return "Ogg Opus"; }
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p);
		if (!sm)
			return false;
		return false;
	}
public:
	bool operator()(const ISO_WAVEFORMATEX *fmt, RIFF_chunk	&chunk, sample *sm) {
		return false;
	}
	OGGOpusFileHandler() : WAVformat(this) {}
} opus;
