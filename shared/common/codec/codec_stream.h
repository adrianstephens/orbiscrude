#ifndef CODEC_STREAM_H
#define CODEC_STREAM_H

#include "codec.h"
#include "stream.h"

namespace iso {

template<typename T, typename S, typename = void>	constexpr bool can_process_file_v = false;
template<typename T, typename S>					constexpr bool can_process_file_v<T, S, void_t<decltype(declval<T>().process(arbitrary_ptr(), arbitrary_ptr(), declval<S>(), TRANSCODE_NONE))>> = true;

template<typename T, typename S> enable_if_t<can_process_file_v<T,S>, size_t> transcode_from_file(T&& t, memory_block dst, S &&file, size_t block_size = 0) {
	return t.process(dst, dst.end(), file, TRANSCODE_SRC_VOLATILE) - (uint8*)dst;
}

template<typename T, typename S> enable_if_t<!can_process_file_v<T,S>, size_t> transcode_from_file(T&& t, memory_block dst, S &&file, size_t block_size = 1024) {
	uint64		dst_offset	= 0;
	for (;;) {
		auto	src = file.get_block(block_size);
		size_t	written, read = transcode(t, dst + dst_offset, src, &written, TRANSCODE_SRC_VOLATILE);
		dst_offset += written;
		if (src.size() < block_size || read < src.size())
			break;
	}
	return dst_offset;
}

template<typename T, typename S, typename = void>	constexpr bool can_process_to_file_v = false;
template<typename T, typename S>					constexpr bool can_process_to_file_v<T, S, void_t<decltype(declval<T>().process(declval<S>(), arbitrary_const_ptr(), arbitrary_const_ptr(), TRANSCODE_NONE))>> = true;

template<typename T, typename S> enable_if_t<can_process_to_file_v<T,S>, size_t>  transcode_to_file(T&& t, S &&file, const_memory_block src, TRANSCODE_FLAGS flags, size_t block_size = 0) {
	return t.process(file, src, src.end(), flags) - (const uint8*)src;
}

template<typename T, typename S> enable_if_t<!can_process_to_file_v<T,S>, size_t> transcode_to_file(T&& t, S &&file, const_memory_block src, TRANSCODE_FLAGS flags, size_t block_size = 1024) {
	malloc_block	dst(block_size);
	uint64			src_offset	= 0;

	for (;;) {
		size_t	written, read = transcode(t, dst, src + src_offset, &written, flags);
		file.writebuff(dst, written);
		src_offset += read;
		if (src_offset == src.length() && written != block_size)
			break;
	}

	return src_offset;
}


template<typename T, typename S = reader_intf, int N = 1024> class codec_reader : public reader_mixin<codec_reader<T, S, N>>, public buffered_reader0<N> {
	S			file;
	T			codec;
	streamptr	len;
public:
	template<typename S1> codec_reader(S1&& file, T&& codec, streamptr len = 0) : file(forward<S1>(file)), codec(forward<T>(codec)), len(len)  {}
	template<typename S1> codec_reader(S1&& file, streamptr len = 0) : file(forward<S1>(file)), len(len)  {}
	size_t	readbuff(void *buffer, size_t size)	{
		return this->buffered_read(
			[this](void *buffer, size_t size) {
				return transcode_from_file(codec, memory_block(buffer, size), file);
			},
			buffer, size
		);
	}
	streamptr	tell()		const			{ return this->current; }
	void		seek(streamptr offset)		{ if (this->buffered_preseek(offset)) stream_skip(*this, offset - this->tell()); }
	void		seek_cur(streamptr offset)	{ seek(this->tell() + offset); }
	void		seek_end(streamptr offset)	{ seek(len + offset); }
	streamptr	length()					{ return len; }
	inline S&	get_stream()				{ return file; }
};

template<typename T, typename S> class codec_reader<T, S, 0> : public reader_mixin<codec_reader<T, S, 0>> {
	S			file;
	T			codec;
public:
	codec_reader(codec_reader&)		= default;
	codec_reader(codec_reader&&)	= default;
	template<typename S1, typename...P> codec_reader(S1&& file, P&&... p) : file(forward<S1>(file)), codec(forward<P>(p)...)  {}
	size_t		readbuff(void *buffer, size_t size)	{ return (int)transcode_from_file(codec, memory_block(buffer, size), file);}
	inline S&	get_stream()	{ return file; }
};

template<int N, typename S, typename T> codec_reader<T,S,N> make_codec_reader(T&& t, S&& s) {
	return {forward<S>(s), forward<T>(t)};
}

template<typename T, typename S = writer_intf, int N = 1024> class codec_writer : public writer_mixin<codec_writer<T, S, N>>, public buffered_writer0<N> {
	S	file;
	T	codec;
public:
	template<typename S1, typename...P> codec_writer(S1&& file, P&&... p) : file(forward<S1>(file)), codec(forward<P>(p)...)  {}
	~codec_writer() { flush(); }
	size_t	writebuff(const void *buffer, size_t size)	{
		return this->buffered_write(
			[this](const void *buffer, size_t size) {
				return transcode_to_file(codec, file, const_memory_block(buffer, size), TRANSCODE_PARTIAL|TRANSCODE_SRC_VOLATILE);
			},
			buffer, size
		);
	}
	streamptr	tell()		const	{ return this->current; }
	void		flush()				{ transcode_to_file(codec, file, none, TRANSCODE_NONE); }
	inline S&	get_stream()		{ return file; }
};

template<typename T, typename S> class codec_writer<T, S, 0> : public writer_mixin<codec_writer<T, S, 0>> {
	S	file;
	T	codec;
public:
	template<typename S1, typename...P> codec_writer(S1&& file, P&&... p) : file(forward<S1>(file)), codec(forward<P>(p)...)  {}
	codec_writer(codec_writer&)		= default;
	codec_writer(codec_writer&&)	= default;
	~codec_writer()				{ flush(); }
	size_t		writebuff(const void *buffer, size_t size)	{ return (int)transcode_to_file(codec, file, const_memory_block(buffer, size), TRANSCODE_PARTIAL|TRANSCODE_SRC_VOLATILE); }
	void		flush()			{ transcode_to_file(codec, file, none, TRANSCODE_NONE); }
	inline S&	get_stream()	{ return file; }
};

template<int N, typename S, typename T> codec_writer<T,S,N> make_codec_writer(T&& t, S&& s) {
	return {forward<S>(s), forward<T>(t)};
}

} //namespace iso
#endif //CODEC_STREAM_H
