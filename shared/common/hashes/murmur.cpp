#include "base/bits.h"

//-----------------------------------------------------------------------------
// MurmurHash
// by Austin Appleby
//-----------------------------------------------------------------------------

namespace iso {

//-----------------------------------------------------------------------------

uint32 MurmurHash1_32(const void * key, int len, uint32 seed) {
	static const uint32	m = 0xc6a4a793;
	static const int	r = 16;

	uint32		h		= seed ^ (len * m);
	const uint8 *data	= (const uint8*)key;

	while (len >= 4)   {
		h += *(uint32*)data;
		h *= m;
		h ^= h >> r;

		data	+= 4;
		len		-= 4;
	}

	switch(len)  {
		case 3: h += data[2] << 16;
		case 2:	h += data[1] << 8;
		case 1:	h += data[0];
				h *= m;
				h ^= h >> r;
	};

	h *= m;
	h ^= h >> 10;
	h *= m;
	h ^= h >> 17;
	return h;
}

//-----------------------------------------------------------------------------

uint32 MurmurHash2_32(const void * key, int len, uint32 seed) {
	static const uint32	m = 0x5bd1e995;
	static const int	r = 24;

	uint32			h		= seed ^ len;
	const uint8		*data	= (const uint8*)key;
	while (len >= 4) {
		uint32 k = *(uint32*)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 

		h *= m; 
		h ^= k;

		data	+= 4;
		len		-= 4;
	}

	switch(len) {
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
				h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
} 

uint64 MurmurHash2_64(const void * key, int len, uint32 seed) {
	static const uint64	m = 0xc6a4a7935bd1e995;
	static const int	r = 47;

	uint64			h		= seed ^ (len * m);
	const uint64	*data	= (const uint64*)key;
	const uint64	*end	= data + len / 8;

	while (data != end) {
		uint64 k = *data++;

		k *= m; 
		k ^= k >> r; 
		k *= m; 
		
		h ^= k;
		h *= m; 
	}

	const uint8 * data2 = (const uint8*)data;
	switch(len & 7) {
		case 7: h ^= uint64(data2[6]) << 48;
		case 6: h ^= uint64(data2[5]) << 40;
		case 5: h ^= uint64(data2[4]) << 32;
		case 4: h ^= uint64(data2[3]) << 24;
		case 3: h ^= uint64(data2[2]) << 16;
		case 2: h ^= uint64(data2[1]) << 8;
		case 1: h ^= uint64(data2[0]);
				h *= m;
	};
 
	h ^= h >> r;
	h *= m;
	h ^= h >> r;
	return h;
} 

//-----------------------------------------------------------------------------

force_inline uint32 fmix(uint32 h) {
	h = (h ^ (h >> 16)) * 0x85ebca6b;
	h = (h ^ (h >> 13)) * 0xc2b2ae35;
	return h ^ (h >> 16);
}

force_inline uint64 fmix(uint64 k) {
	k = (k ^ (k >> 33)) * 0xff51afd7ed558ccdull;
	k = (k ^ (k >> 33)) * 0xc4ceb9fe1a85ec53ull;
	return k ^ (k >> 33);
}

uint32 MurmurHash3_32(const void * key, int len, uint32 seed) {
	const int	nblocks = len / 4;

	uint32 h1 = seed;
	uint32 c1 = 0xcc9e2d51, c2 = 0x1b873593;

	const uint32 *blocks = (const uint32*)((const uint8*)key + nblocks * 4);
	for(int i = -nblocks; i; i++) {
		h1 ^= rotate_left(blocks[i] * c1, 15) * c2;
		h1 = rotate_left(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	const uint8	*tail = (const uint8*)key + nblocks * 4;
	uint32 k1 = 0;
	switch(len & 3) {
		case 3: k1 ^= tail[2] << 16;
		case 2: k1 ^= tail[1] << 8;
		case 1: k1 ^= tail[0];
				k1 = rotate_left(k1 * c1, 15) * c2;
				h1 ^= k1;
	};

	return fmix(h1 ^ len);
} 

void MurmurHash3_128(const void *key, const int len, uint32 seed, void *out) {
	const int nblocks = len / 16;

	uint32 h1 = seed, h2 = seed, h3 = seed, h4 = seed;
	uint32 c1 = 0x239b961b, c2 = 0xab0e9789, c3 = 0x38b34ae5, c4 = 0xa1e38b93;

	const uint32 * blocks = (const uint32*)((const uint8*)key + nblocks * 16);
	for (int i = -nblocks; i; i++) {
		h1 ^= rotate_left(blocks[i * 4 + 0] * c1, 15) * c2;
		h1 = (rotate_left(h1, 19) + h2) * 5 + 0x561ccd1b;

		h2 ^= rotate_left(blocks[i * 4 + 1] * c2, 16) * c3;
		h2 = (rotate_left(h2, 17) + h3) *5 + 0x0bcaa747;

		h3 ^= rotate_left(blocks[i * 4 + 2] * c3, 17) * c4;
		h3 = (rotate_left(h3, 15) + h4) * 5 + 0x96cd1c35;

		h4 ^= rotate_left(blocks[i * 4 + 3] * c4, 18) * c1;
		h4 = (rotate_left(h4, 13) + h1) * 5 + 0x32ac3b17;
	}

	const uint8 * tail = (const uint8*)key + nblocks * 16;
	uint32 k1 = 0, k2 = 0, k3 = 0, k4 = 0;
	switch(len & 15) {
		case 15: k4 ^= tail[14] << 16;
		case 14: k4 ^= tail[13] << 8;
		case 13: k4 ^= tail[12] << 0;
			h4 ^= rotate_left(k4 * c4, 18) * c1;

		case 12: k3 ^= tail[11] << 24;
		case 11: k3 ^= tail[10] << 16;
		case 10: k3 ^= tail[ 9] << 8;
		case  9: k3 ^= tail[ 8] << 0;
			h3 ^= rotate_left(k3 * c3, 17) * c4;

		case  8: k2 ^= tail[ 7] << 24;
		case  7: k2 ^= tail[ 6] << 16;
		case  6: k2 ^= tail[ 5] << 8;
		case  5: k2 ^= tail[ 4] << 0;
			h2 ^= rotate_left(k2 * c2, 16) * c3;

		case  4: k1 ^= tail[ 3] << 24;
		case  3: k1 ^= tail[ 2] << 16;
		case  2: k1 ^= tail[ 1] << 8;
		case  1: k1 ^= tail[ 0] << 0;
			h1 ^= rotate_left(k1 * c1, 15) * c2;
	};

	h1 ^= len;
	h2 ^= len;
	h3 ^= len;
	h4 ^= len;

	h1 += h2 + h3 + h4;
	h2 += h1;
	h3 += h1;
	h4 += h1;

	h1 = fmix(h1);
	h2 = fmix(h2);
	h3 = fmix(h3);
	h4 = fmix(h4);

	h1 += h2 + h3 + h4;
	h2 += h1;
	h3 += h1;
	h4 += h1;

	((uint32*)out)[0] = h1;
	((uint32*)out)[1] = h2;
	((uint32*)out)[2] = h3;
	((uint32*)out)[3] = h4;
}

void MurmurHash3_64(const void *key, const int len, const uint32 seed, void *out) {
	const int	nblocks = len / 16;

	uint64 h1 = seed, h2 = seed;
	uint64 c1 = 0x87c37b91114253d5ull, c2 = 0x4cf5ad432745937full;

	const uint64 * blocks = (const uint64*)key;
	for (int i = 0; i < nblocks; i++) {
		h1 ^= rotate_left(blocks[i*2+0] * c1, 31) * c2;
		h1 = (rotate_left(h1, 27) + h2) * 5 + 0x52dce729;

		h2 ^= rotate_left(blocks[i*2+1] * c2, 33) * c1;
		h2 = (rotate_left(h2, 31) + h1) * 5 + 0x38495ab5;
	}

	const uint8 * tail = (const uint8*)key + nblocks * 16;
	uint64 k1 = 0;
	uint64 k2 = 0;
	switch(len & 15) {
		case 15: k2 ^= uint64(tail[14]) << 48;
		case 14: k2 ^= uint64(tail[13]) << 40;
		case 13: k2 ^= uint64(tail[12]) << 32;
		case 12: k2 ^= uint64(tail[11]) << 24;
		case 11: k2 ^= uint64(tail[10]) << 16;
		case 10: k2 ^= uint64(tail[ 9]) << 8;
		case  9: k2 ^= uint64(tail[ 8]) << 0;
			h2 ^= rotate_left(k2 * c2, 33) * c1;

		case  8: k1 ^= uint64(tail[ 7]) << 56;
		case  7: k1 ^= uint64(tail[ 6]) << 48;
		case  6: k1 ^= uint64(tail[ 5]) << 40;
		case  5: k1 ^= uint64(tail[ 4]) << 32;
		case  4: k1 ^= uint64(tail[ 3]) << 24;
		case  3: k1 ^= uint64(tail[ 2]) << 16;
		case  2: k1 ^= uint64(tail[ 1]) << 8;
		case  1: k1 ^= uint64(tail[ 0]) << 0;
			h1 ^= rotate_left(k1 * c1, 31) * c2;
	};

	h1 ^= len;
	h2 ^= len;
	h1 += h2;
	h2 += h1;
	h1 = fmix(h1);
	h2 = fmix(h2);
	h1 += h2;
	h2 += h1;
	((uint64*)out)[0] = h1;
	((uint64*)out)[1] = h2;
}

} // namespace iso
