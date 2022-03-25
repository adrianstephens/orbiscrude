#include "samplefile.h"
#include "iso/iso_convert.h"
#include "include/FLAC/all.h"

using namespace iso;

class FLACFileHandler : public SampleFileHandler {
	const char*			GetExt() override { return "flac"; }
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
} flac;

class FLAC {
	tag					id;
	istream_ref				file;
	ISO_ptr<sample>	p;
	int					tf;

	FLAC__StreamDecoderReadStatus	read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes) {
		if (*bytes > 0) {
			*bytes = file.readbuff(buffer, *bytes);
			return *bytes == 0
				? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
				: FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	FLAC__StreamDecoderWriteStatus	write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[]) {
//		for (int i = 0, c = p->Channels(); i < c; i++) {
//			p->Samples() + c * frame + i = buffer[i];
//		}
		int		nf	= frame->header.blocksize;
		int		nc	= frame->header.channels;
		int16	*s = p->Samples() + tf * nc;
		for (int c = 0; c < nc; c++) {
			for (int f = 0; f < nf; f++)
				s[f * nc + c] = buffer[c][f];
		}
		tf += nf;
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	void							metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata) {
		if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
			const FLAC__StreamMetadata_StreamInfo	&si = metadata->data.stream_info;
			p = ISO_ptr<sample>(id);
			p->SetFrequency(si.sample_rate);
			p->Create(si.total_samples, si.channels, si.bits_per_sample);
			tf = 0;
		}
	}

	void							error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status) {
	}

	static FLAC__StreamDecoderReadStatus	_read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
		return ((FLAC*)client_data)->read_callback(decoder, buffer, bytes);
	}
	static FLAC__StreamDecoderWriteStatus	_write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data) {
		return ((FLAC*)client_data)->write_callback(decoder, frame, buffer);
	}
	static void								_metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
		return ((FLAC*)client_data)->metadata_callback(decoder, metadata);
	}
	static void								_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
		return ((FLAC*)client_data)->error_callback(decoder, status);
	}
public:
	FLAC(tag _id, istream_ref _file) : id(_id), file(_file)	{
		FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();

		if (decoder == NULL)
			return;

		FLAC__stream_decoder_set_md5_checking(decoder, true);
		if (FLAC__stream_decoder_init_stream(
			decoder,
			_read_callback,
			NULL,//seek_callback,      // or NULL
			NULL,//tell_callback,      // or NULL
			NULL,//length_callback,    // or NULL
			NULL,//eof_callback,       // or NULL
			_write_callback,
			_metadata_callback,  // or NULL
			_error_callback,
			this
			) == FLAC__STREAM_DECODER_INIT_STATUS_OK) {
			FLAC__stream_decoder_process_until_end_of_stream(decoder);
		}
		FLAC__stream_decoder_delete(decoder);
	}
	operator ISO_ptr<void>()	{ return p; }
};

/*
FLAC__StreamDecoderReadStatus	read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
FLAC__StreamDecoderSeekStatus	seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamDecoderTellStatus	tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
FLAC__StreamDecoderLengthStatus	length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
FLAC__bool						eof_callback(const FLAC__StreamDecoder *decoder, void *client_data);
FLAC__StreamDecoderWriteStatus	write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
void							metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
void							error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

read_callback,
seek_callback,      // or NULL
tell_callback,      // or NULL
length_callback,    // or NULL
eof_callback,       // or NULL
write_callback,
metadata_callback,  // or NULL
error_callback,
client_data
*/

ISO_ptr<void> FLACFileHandler::Read(tag id, istream_ref file)
{
	return FLAC(id, file);
}

bool FLACFileHandler::Write(ISO_ptr<void> p, ostream_ref file)
{
	if (ISO_ptr<sample> s = ISO_conversion::convert<sample>(p)) {
		return true;
	}
	return false;
}
