#ifndef ZLIB_STREAM_H
#define ZLIB_STREAM_H

#include "codec/deflate.h"
#include "codec/codec_stream.h"

namespace iso {

using deflate_reader = codec_reader<deflate_decoder, reader_intf, 1024>;
using deflate_writer = codec_writer<deflate_encoder, writer_intf, 1024>;

using zlib_reader = codec_reader<zlib_decoder, reader_intf, 1024>;
using zlib_writer = codec_writer<zlib_encoder, writer_intf, 1024>;

}	//namespace iso

#endif	// ZLIB_STREAM_H
