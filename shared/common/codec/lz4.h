#ifndef LZ4_H
#define LZ4_H

#include "window.h"

namespace iso {
namespace LZ4 {

enum {
	DISTANCE_BITS	= 16,
	HASH_BITS		= 15,
	WSIZE			= 1 << DISTANCE_BITS,

	MINMATCH		= 4,
	LASTLITERALS	= 5,
	MFLIMIT			= 8 + MINMATCH,

	ML_BITS			= 4,
	RUN_BITS		= 8 - ML_BITS,
	ML_MASK			= (1 << ML_BITS) - 1,
	RUN_MASK		= (1 << RUN_BITS) - 1,

	OPTIMAL_ML		= (ML_MASK - 1) + MINMATCH,
};

//-----------------------------------------------------------------------------
//	LZ4 decoder
//-----------------------------------------------------------------------------

class _decoder : public codec_defaults {
protected:
	template<typename WIN> static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win);
	template<typename WIN> static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, TRANSCODE_FLAGS flags, const WIN win);
};

using decoder = decoder_with_window<_decoder, WSIZE>;

//-----------------------------------------------------------------------------
//	LZ4 encoder
//-----------------------------------------------------------------------------

class _encoder : public codec_defaults {
protected:
	enum {
		SKIPSTRENGTH	= 6,	// Increasing this value will make the compression run slower on incompressible data
		MAX_INPUT_SIZE	= 0x7E000000,
	};

	uint32	hashes[1 << HASH_BITS];
	uint32	offset;

	const uint8*	lookup(const uint8* base, uint32 h)			const	{ return hashes[h] + base; }
	void			put(const uint8* base, uint32 h, const uint8 *p)	{ hashes[h] = (uint32)(p - base); }

	void	check_overflow() {
		if (offset > 0x80000000) {
			uint32	delta = offset - WSIZE;
			for (auto &i : hashes)
				i = i < delta ? 0 : i - delta;
			offset = WSIZE;
		}
	}
public:
	_encoder() {
		clear(*this);
	}
	template<typename DST, typename WIN> const uint8*	process(DST dst, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win);
	template<typename WIN> const uint8* process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win) {
		return dst_end
			? process(dst_with_end(dst, dst_end), src, src_end, flags, win)
			: process(dst_without_end(dst), src, src_end, flags, win);
	}
	//	size_t	process(void *dst, size_t dst_size, const void* src, size_t src_size, size_t *bytes_written, TRANSCODE_FLAGS flags); // handles dst_size == 0
};

using encoder = encoder_with_window<_encoder, WSIZE>;

//-----------------------------------------------------------------------------
//	LZ4HC encoder
//-----------------------------------------------------------------------------

class _HCencoder : _encoder {
	enum {
		DEFAULT_COMPRESSION	= 8,
		MAX_COMPRESSION		= 16,
	};

	int			attempts;
	uint16		chains[WSIZE];

public:
	_HCencoder(int compressionLevel = DEFAULT_COMPRESSION) {
		attempts	= 1 << min(compressionLevel, MAX_COMPRESSION);
		for (auto &i : hashes)
			i = -1;
		for (auto &i : chains)
			i = 0xffff;
	}
	template<typename DST, typename WIN> const uint8*	process(DST dst, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win);
	template<typename WIN> const uint8* process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win) {
		return dst_end
			? process(dst_with_end(dst, dst_end), src, src_end, flags, win)
			: process(dst_without_end(dst), src, src_end, flags, win);
	}
//	size_t	process(void* dst, size_t dst_size, const void* src, size_t src_size, size_t *bytes_written, TRANSCODE_FLAGS flags); // handles dst_size == 0
};

using HCencoder = encoder_with_window<_HCencoder, WSIZE>;

} //namespace LZ4
} //namespace iso

#endif //LZ4_H
