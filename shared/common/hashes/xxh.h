#ifndef XXH_H
#define XXH_H

#include "hash_stream.h"

namespace iso {

// xxHash - Fast Hash algorithm Copyright (C) 2012-2016, Yann Collet

//  32-bit

struct XXH32 : block_writer<XXH32, 16> {
	static const uint32 PRIME_1 = 2654435761U;
	static const uint32 PRIME_2 = 2246822519U;
	static const uint32 PRIME_3 = 3266489917U;
	static const uint32 PRIME_4 =  668265263U;
	static const uint32 PRIME_5 =  374761393U;

	static uint32 round(uint32 seed, uint32 v)		{ return rotate_bits(seed + v * PRIME_2, 13) * PRIME_1; }
	static uint32 process1(uint8 b, uint32 h)		{ return rotate_bits(h + b * PRIME_5, 11) * PRIME_1; }
	static uint32 process4(uint32 b4, uint32 h)		{ return rotate_bits(h + b4 * PRIME_3, 17) * PRIME_4; }
	static uint32 finalize(uint32 h, const_memory_block data);

	uint32		v1, v2, v3, v4;

	XXH32(uint32 seed = 0) : v1(seed + PRIME_1 + PRIME_2), v2(seed + PRIME_2), v3(seed), v4(seed - PRIME_1) {}

	void process(const void* data) {
		uint32le	*p = (uint32le*)data;
		v1 = round(v1, p[0]);
		v2 = round(v2, p[1]);
		v3 = round(v3, p[2]);
		v4 = round(v4, p[3]);
	}

	uint32 digest() const {
		uint32 h	= tell() > 16
			?	rotate_bits(v1, 1) + rotate_bits(v2, 7) + rotate_bits(v3, 12) + rotate_bits(v4, 18)
			:	v3 + PRIME_5;
		return finalize(h + tell(), buffered());
	}
};

//  64-bit

struct XXH64 : block_writer<XXH32, 32> {
	static const uint64 PRIME_1 = 11400714785074694791ULL;
	static const uint64 PRIME_2 = 14029467366897019727ULL;
	static const uint64 PRIME_3 =  1609587929392839161ULL;
	static const uint64 PRIME_4 =  9650029242287828579ULL;
	static const uint64 PRIME_5 =  2870177450012600261ULL;

	static uint64 round(uint64 acc, uint64 v)		{ return rotate_bits(acc + v * PRIME_2, 31) * PRIME_1; }
	static uint64 merge(uint64 acc, uint64 v)		{ return (acc ^ round(0, v)) * PRIME_1 + PRIME_4; }
	static uint64 process1(uint8 b, uint64 h)		{ return rotate_bits(h ^ (b * PRIME_5), 11) * PRIME_1; }
	static uint64 process4(uint32 b4, uint64 h)		{ return rotate_bits(h ^ (b4 * PRIME_1), 23) * PRIME_2 + PRIME_3;}
	static uint64 process8(uint32 b8, uint64 h)		{ return rotate_bits(h ^ round(0, b8), 27) * PRIME_1 + PRIME_4; }
	static uint64 finalize(uint64 h, const_memory_block data);

	uint64		v1, v2, v3, v4;

	XXH64(uint64 seed = 0) : v2(seed + PRIME_2), v3(seed), v4(seed - PRIME_1) {}

	void process(const void* data) {
		uint64le	*p = (uint64le*)data;
		v1 = round(v1, p[0]);
		v2 = round(v2, p[1]);
		v3 = round(v3, p[2]);
		v4 = round(v4, p[3]);
	}
	uint64 digest() const {
		uint64 h	= tell() >= 32
			? merge(merge(merge(merge(rotate_bits(v1, 1) + rotate_bits(v2, 7) + rotate_bits(v3, 12) + rotate_bits(v4, 18), v1), v2), v3), v4)
			: v3 + PRIME_5;
		return finalize(h + tell(), buffered());
	}
};

} //namespace iso

#endif //XXH_H
