#ifndef CAMELLIA_H
#define CAMELLIA_H

#include "base/bits.h"

namespace iso {

struct CAMELLIA {
	static uint64 SP[8][256];

	double_int<uint64>	Kw[2];
	double_int<uint64>	Ke[3];
	double_int<uint64>	K[12];
	int		key_size;

	inline uint64 F(uint64 x, uint64 k) {
		k	^= x;
		return	SP[0][k >> 56]
			^	SP[1][(k >> 48) & 0xff]
			^	SP[2][(k >> 40) & 0xff]
			^	SP[3][(k >> 32) & 0xff]
			^	SP[4][(k >> 24) & 0xff]
			^	SP[5][(k >> 16) & 0xff]
			^	SP[6][(k >> 8) & 0xff]
			^	SP[7][k & 0xff];
	}

	inline uint64 FL(uint64 x, uint64 k) {
		uint32	x1 = x >> 32,	x2 = (uint32)x;
		uint32	k1 = k >> 32,	k2 = (uint32)k;
		x2 ^= rotate_left((x1 & k1), 1);
		x1 ^= x2 | k2;
		return ((uint64)x1 << 32) | x2;
	}

	inline uint64 FLINV(uint64 x, uint64 k) {
		uint32	x1 = x >> 32,	x2 = (uint32)x;
		uint32	k1 = k >> 32,	k2 = (uint32)k;
		x1 ^= x2 | k2;
		x2 ^= rotate_left((x1 & k1), 1);
		return ((uint64)x1 << 32) | x2;
	}

	static void computeSP();

	CAMELLIA()	{}
	CAMELLIA(const const_memory_block &key) { init(key); }

	int		init(const const_memory_block &key);
	void	encrypt(uint64be *dst, const uint64be *src, uint64 *iv = 0);
	void	decrypt(uint64be *dst, const uint64be *src, uint64 *iv = 0);
};

struct CAMELLIA_with_iv : CAMELLIA {
	uint64	iv[2];
	CAMELLIA_with_iv(const const_memory_block &key, uint64be *_iv) : CAMELLIA(key) {
		iv[0] = _iv[0];
		iv[1] = _iv[1];
	}
	void	encrypt(uint64be *dst, const uint64be *src) { CAMELLIA::encrypt(dst, src, iv); }
	void	decrypt(uint64be *dst, const uint64be *src) { CAMELLIA::decrypt(dst, src, iv); }
};


} //namespace iso

#endif //CAMELLIA_H
