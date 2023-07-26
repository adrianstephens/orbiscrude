#ifndef USAGE_H
#define USAGE_H

#include "hashes/fnv.h"
#include "crc32.h"

namespace iso {

enum USAGE : uint8 {
	USAGE_UNKNOWN			= 0,
	USAGE_POSITION,
	USAGE_NORMAL,
	USAGE_COLOR,
	USAGE_TEXCOORD,
	USAGE_TANGENT,
	USAGE_BINORMAL,
	USAGE_BLENDWEIGHT,
	USAGE_BLENDINDICES,
	USAGE_PSIZE,
	USAGE_TESSFACTOR,
	USAGE_FOG,
	USAGE_DEPTH,
	USAGE_SAMPLE,
	USAGE_COLOUR	= USAGE_COLOR,
};

const char *get_name(USAGE u);

struct USAGE2 {
	USAGE	usage;
	uint8	index;
	constexpr USAGE2(USAGE usage = USAGE_UNKNOWN, int index = 0) : usage(usage), index(index) {}
	USAGE2(crc32 id);
	USAGE2(const char *id);
	//	constexpr operator USAGE() const { return usage; }
	constexpr operator uint16()			const { return usage | (index << 8); }
	constexpr USAGE2 add_index(int i)	const { return {usage, index + i}; }
};

constexpr USAGE get_usage(uint32 usage) {
	return	usage == "position"_fnv		? USAGE_POSITION
		:	usage == "normal"_fnv		? USAGE_NORMAL
		:	usage == "color"_fnv		? USAGE_COLOR
		:	usage == "colour"_fnv		? USAGE_COLOR
		:	usage == "texcoord"_fnv		? USAGE_TEXCOORD
		:	usage == "tangent"_fnv		? USAGE_TANGENT
		:	usage == "binormal"_fnv		? USAGE_BINORMAL
		:	usage == "weights"_fnv		? USAGE_BLENDWEIGHT
		:	usage == "bones"_fnv		? USAGE_BLENDINDICES
		:	usage == "indices"_fnv		? USAGE_BLENDINDICES
		:	usage == "psize"_fnv		? USAGE_PSIZE
		:	usage == "tessfactor"_fnv	? USAGE_TESSFACTOR
		:	usage == "fog"_fnv			? USAGE_FOG
		:	usage == "depth"_fnv		? USAGE_DEPTH
		:	usage == "sample"_fnv		? USAGE_SAMPLE
		:	USAGE_UNKNOWN;
}

constexpr USAGE2 get_usage(const char *usage, size_t N, size_t n) {
	return is_digit(usage[n - 1])
		? get_usage(usage, N, n - 1)
		: USAGE2(get_usage(FNV_const<uint32>(usage, n)), uint8(from_base_string<10>(usage, N, n)));
}

constexpr USAGE2 operator "" _usage(const char* usage, size_t len)	{
	return get_usage(usage, len, len);
}

} // namespace iso
#endif // USAGE_H
