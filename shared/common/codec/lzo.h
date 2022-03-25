#ifndef LZO_H
#define LZO_H

#include "codec.h"

namespace iso { namespace LZO {

enum {
	D_BITS			= 14,

	M1_MAX_OFFSET	= 0x0400,
	M2_MAX_OFFSET	= 0x0800,
	M3_MAX_OFFSET	= 0x4000,
	M4_MAX_OFFSET	= 0xbfff,

	MX_MAX_OFFSET	= (M1_MAX_OFFSET + M2_MAX_OFFSET),

	M1_MIN_LEN		= 2,
	M1_MAX_LEN		= 2,
	M2_MIN_LEN		= 3,
	M2_MAX_LEN		= 8,
	M3_MIN_LEN		= 3,
	M3_MAX_LEN		= 33,
	M4_MIN_LEN		= 3,
	M4_MAX_LEN		= 9,

	M1_MARKER		= 0,
	M2_MARKER		= 64,
	M3_MARKER		= 32,
	M4_MARKER		= 16,

	MIN_LOOKAHEAD	= M2_MAX_LEN + 1,
};

class decoder {
public:
	static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
};

class encoder {
	uint16			dict[1 << D_BITS];
public:
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	static uint32 lzo1_maxoutput(uint32 len) { return len + (len / 16) + 64 + 3; }
	static uint32 lzo2_maxoutput(uint32 len) { return len + (len / 8) + 128 + 3; }
};


} } // namespace LZO::iso

#endif // LZO_H
