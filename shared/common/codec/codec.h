#ifndef CODEC_H
#define CODEC_H

#include "base/defs.h"
#include "stream.h"

namespace iso {

//	A codec needs to provide a function with this signature:
//		const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
//		undates dst position
//		returns src position
//
//	A codec may provide a function with this signature:
//		size_t	estimate_output(const void *src, size_t src_size);
//		return estimated number of bytes
//
//	A codec may provide a function with this signature for streaming from a file:
//		const void*	process(void *dst, void *dst_end, S infile, TRANSCODE_FLAGS flags);
//
//	A codec may provide a function with this signature for streaming to a file:
//		const void*	process(S outfile, const void *src, const void *src_end, TRANSCODE_FLAGS flags);


enum TRANSCODE_FLAGS {
	TRANSCODE_NONE			= 0,
	TRANSCODE_PARTIAL		= 1,
	TRANSCODE_SRC_VOLATILE	= 2,
	TRANSCODE_DST_VOLATILE	= 4,
	TRANSCODE_ALIGN			= 8,
	TRANSCODE_FLUSH			= 16,
	TRANSCODE_RESET			= 32,
};
ENUM_FLAGOPS(TRANSCODE_FLAGS);

struct codec_defaults {
	static size_t	estimate_output(const void* src, size_t src_size) { return src_size; }
};

template<typename T, typename R> using is_memory_transcoder = exists_t<decltype(declval<T>().process(declval<uint8*&>(), nullptr, nullptr, nullptr, TRANSCODE_NONE)), R>;
template<typename T, typename R> using is_stream_transcoder = exists_t<decltype(declval<T>().process(nullptr, nullptr, declval<istream_ref>(), TRANSCODE_NONE)), R>;
 
template<typename T> size_t transcode(T&& transcoder, void* dst, size_t dst_size, const void* src, size_t src_size, size_t* bytes_written = 0, TRANSCODE_FLAGS flags = TRANSCODE_NONE) {
	auto	dst1 = (uint8*)dst;
	auto	src1 = (const uint8*)src;
	auto	read = transcoder.process(dst1, dst1 + dst_size, src1, src1 + src_size, flags) - src1;
	if (bytes_written)
		*bytes_written = dst1 - (uint8*)dst;
	return read;
}

template<typename T> is_memory_transcoder<T, size_t> transcode(T&& transcoder, memory_block dst, const_memory_block src, size_t* bytes_written = 0, TRANSCODE_FLAGS flags = TRANSCODE_NONE) {
	auto	dst1 = (uint8*)dst;
	auto	read = transcoder.process(dst1, dst.end(), src, src.end(), flags) - (const uint8*)src;
	if (bytes_written)
		*bytes_written = dst1 - (uint8*)dst;
	return read;
}
template<typename T> is_stream_transcoder<T, size_t> transcode(T&& transcoder, memory_block dst, const_memory_block src, size_t* bytes_written = 0, TRANSCODE_FLAGS flags = TRANSCODE_NONE) {
	memory_reader	r(src);
	auto	end = transcoder.process(dst, dst.end(), r, flags);
	if (bytes_written)
		*bytes_written = end - (uint8*)dst;
	return r.tell();
}

template<typename T> malloc_block transcode(T&& transcoder, const_memory_block src, size_t initial_size) {
	malloc_block	dst(initial_size);
	uint64			dst_offset	= 0;
	uint64			src_offset	= 0;

	for (;;) {
		size_t	written, read = transcode(transcoder, dst + dst_offset, src + src_offset, &written);
		src_offset += read;
		dst_offset += written;
		if (src_offset == src.length() && dst_offset != dst.length())
			break;

		dst.resize(dst_offset * 2);
	}

	return move(dst.resize(dst_offset));
}

template<typename T> malloc_block transcode(T&& transcoder, const_memory_block src) {
	return transcode(transcoder, src, transcoder.estimate_output(src, src.length()));
}


template<typename T> is_stream_transcoder<T, size_t> transcode(T&& transcoder, memory_block dst, istream_ref file, TRANSCODE_FLAGS flags = TRANSCODE_NONE) {
	return transcoder.process(dst, dst.end(), file, flags) - (const uint8*)dst;
}

//-----------------------------------------------------------------------------
//	uncompressed_decoder
//-----------------------------------------------------------------------------

struct uncompressed_decoder {
	const uint8* process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
		uint64	copy_size	= min(src_end - src, dst_end - dst);
		memcpy(dst, src, copy_size);
		dst += copy_size;
		return src + copy_size;
	}
};

//-----------------------------------------------------------------------------
//	helpers
//-----------------------------------------------------------------------------

struct dst_with_end {
	uint8	*&start, *endp;
	constexpr dst_with_end(uint8 *&start, uint8 *end)  : start(start), endp(end) {}
	constexpr uint8*	begin()				const	{ return start; }
	constexpr uint8*	end()				const	{ return endp; }
	constexpr bool		check(uint8 *p)		const	{ return p <= endp; }
	void				set(uint8 *p)				{ start = p; }
};
struct dst_without_end {
	uint8	*&start;
	constexpr dst_without_end(uint8 *&start)  : start(start) {}
	constexpr uint8*	begin()				const	{ return start; }
	constexpr uint8*	end()				const	{ return nullptr; }
	constexpr bool		check(uint8 *p)		const	{ return true; }
	void				set(uint8 *p)				{ start = p; }
};

struct src_with_end {
	const uint8	*start, *endp;
	constexpr src_with_end(const uint8 *start, const uint8 *end)  : start(start), endp(end) {}
	constexpr const uint8*	begin()			const	{ return start; }
	constexpr const uint8*	end()			const	{ return endp; }
	constexpr bool	check(const uint8 *p)	const	{ return p <= endp; }
};
struct src_without_end {
	const uint8	*start;
	constexpr src_without_end(const uint8 *start)  : start(start) {}
	constexpr const uint8*	begin()			const	{ return start; }
	constexpr const uint8*	end()			const	{ return nullptr; }
	constexpr bool	check(const uint8 *p)	const	{ return true; }
};

inline const uint8 *match_end(const uint8* src, intptr_t d, const uint8* src_end) {
	while (likely(src < src_end - 7)) {
		if (size_t diff = load_packed<uint64>(src + d) ^ load_packed<uint64>(src))
			return src + num_zero_bytes(diff);
		src += 8;
	}
	if (src < src_end - 3 && load_packed<uint32>(src + d) == load_packed<uint32>(src))
		src += 4;
	if (src < src_end - 1 && load_packed<uint16>(src + d) == load_packed<uint16>(src))
		src += 2;
	if (src < src_end && src[d] == src[0])
		++src;

	return src;
}

inline const uint8 *match_end_loose(const uint8* src, intptr_t d, const uint8* src_end) {
	while (likely(src < src_end)) {
		if (size_t diff = load_packed<uint64>(src + d) ^ load_packed<uint64>(src))
			return min(src + num_zero_bytes(diff), src_end);
		src += 8;
	}
	return src_end;
}

inline const uint8 *match_begin(const uint8* src, intptr_t d, const uint8* src_begin) {
	while (src > src_begin && src[-1] == src[d - 1])
		--src;
	return src;
}

inline const uint8 *match_end(const uint8* src, const uint8* ref, const uint8* src_end)			{ return match_end(src, ref - src, src_end); }
inline const uint8 *match_end_loose(const uint8* src, const uint8* ref, const uint8* src_end)	{ return match_end_loose(src, ref - src, src_end); }
inline size_t		match_len(const uint8* src, intptr_t d, const uint8* src_end)				{ return match_end(src, d, src_end) - src; }
inline size_t		match_len_loose(const uint8* src, intptr_t d, const uint8* src_end)			{ return match_end_loose(src, d, src_end) - src; }
inline size_t		match_len(const uint8* src, const uint8* ref, const uint8* src_end)			{ return match_end(src, ref - src, src_end) - src; }
inline size_t		match_len_loose(const uint8* src, const uint8* ref, const uint8* src_end)	{ return match_end_loose(src, ref - src, src_end) - src; }
inline const uint8 *match_begin(const uint8* src, const uint8* ref, const uint8* src_begin)		{ return match_begin(src, ref - src, src_begin); }

template<typename T> inline uint8* loose_copy(uint8 *dst, intptr_t d, uint8 *end) {
	while (dst < end) {
		copy_packed<T>(dst, dst + d);
		dst += sizeof(T);
	}
	return dst;
}
template<> inline uint8* loose_copy<uint8>(uint8 *dst, intptr_t d, uint8 *end) {
	while (dst < end) {
		dst[0] = dst[d];
		++dst;
	}
	return dst;
}

template<typename T> inline uint8* loose_copy(uint8 *dst, const uint8 *src, uint8 *end) {
	return loose_copy<T>(dst, src - dst, end);
}

//-----------------------------------------------------------------------------
//	test
//-----------------------------------------------------------------------------

template<typename T> void test_codec_chunks(T& codec, const_memory_block src, const_memory_block compare) {
	malloc_block	out(compare.length() + 1024);

	for (size_t i = 8; i < src.length(); i += 7) {
		out.fill(0);
		const uint8	*s = src.begin();
		uint8		*d = out.begin();
		while (d < out + compare.length()) {
			for (size_t j = i; ; j *= 2) {
				auto	s2 = codec.process(d, out.end(), s, min(s + j, src.end()), TRANSCODE_PARTIAL * (s + j < src.end()));
				if (s2 != s) {
					s = s2;
					break;
				}
			}
		}
		if (s < src.end())
			s = codec.process(d, out.end(), s, src.end(), TRANSCODE_NONE);
		ISO_ASSERT(s == src.end() && d - out == compare.length() && memcmp(compare, out, compare.length()) == 0);
	}
}

template<int N, typename E, typename D> bool test_codec(E&& encoder, D&& decoder, const_memory_block src, bool chunks = true) {
	size_t	src_size = src.length();
	uint8	cmp_buffer[N];
	uint8	out_buffer[N];

	//encode
	uint8	*cmp		= cmp_buffer;
	auto	src_end		= encoder.process(cmp, end(cmp_buffer), src, src.end(), TRANSCODE_NONE);
	ISO_ASSERT(src_end == src.end());

	//single decode
	uint8	*out		= out_buffer;
	uint8	*out_end	= out_buffer + src_size;
	auto	cmp_end		= decoder.process(out, out_end, cmp_buffer, cmp, TRANSCODE_NONE);
	ISO_ASSERT(cmp == cmp_end && out == out_end && memcmp(src, out_buffer, src_size) == 0);

	if (chunks) {
		//decode in chunks
		test_codec_chunks(decoder, {cmp_buffer, cmp}, src);

		//encode in chunks
		//test_codec_chunks(encoder, src, {cmp_buffer, cmp_size});
	}
	return true;
}

template<int N, typename E, typename D> bool test_stream_codec(E&& encoder, D&& decoder, const_memory_block src) {
	size_t	src_size = src.length();
	uint8	cmp_buffer[N];
	uint8	out_buffer[N];

	//encode
	memory_writer	mo(cmp_buffer);
	const uint8		*end	= encoder.process(mo, src, src.end(), TRANSCODE_NONE);
	size_t			written	= mo.tell();
	ISO_ASSERT(end == src.end());

	//single decode
	memory_reader	mi(const_memory_block(cmp_buffer, written));
	end		= decoder.process(out_buffer, out_buffer + src_size, mi, TRANSCODE_NONE);
	ISO_ASSERT(end - out_buffer == src_size && memcmp(src, out_buffer, src_size) == 0);

	return true;
}

inline const_memory_block get_test_data() {
	static const char source[] =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod\n"
		"tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim\n"
		"veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea\n"
		"commodo consequat. Duis aute irure dolor in reprehenderit in voluptate\n"
		"velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat\n"
		"cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id\n"
		"est laborum.\n"
		"\n"
		"Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium\n"
		"doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore\n"
		"veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim\n"
		"ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia\n"
		"consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt. Neque\n"
		"porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur,\n"
		"adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore\n"
		"et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis\n"
		"nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid\n"
		"ex ea commodi consequatur? Quis autem vel eum iure reprehenderit qui in ea\n"
		"voluptate velit esse quam nihil molestiae consequatur, vel illum qui\n"
		"dolorem eum fugiat quo voluptas nulla pariatur?\n";
	return source;
}

template<typename E, typename D> bool test_codec(E&& encoder, D&& decoder, bool chunks = true) {
	return test_codec<2048>(encoder, decoder, get_test_data(), chunks);
}
template<typename E, typename D> bool test_stream_codec(E&& encoder, D&& decoder) {
	return test_stream_codec<2048>(encoder, decoder, get_test_data());
}

} //namespace iso
#endif //CODEC_H
