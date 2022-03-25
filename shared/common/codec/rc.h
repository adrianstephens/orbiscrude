#ifndef RC_H
#define RC_H

#include "base/bits.h"
#include "stream.h"
#include "extra/random.h"

namespace iso {

struct RC2 {
	uint16 xkey[64];

	RC2(const const_memory_block &key, uint32 bits)	{ init(key, bits); }
	RC2(const const_memory_block &key)				{ init(key); }

	void init(const const_memory_block &key, uint32 bits) {
		// 256-entry permutation table, probably derived somehow from pi
		static const uint8 permute[256] = {
			217, 120, 249, 196, 25, 221, 181, 237, 40, 233, 253, 121, 74, 160, 216, 157,
			198, 126, 55, 131, 43, 118, 83, 142, 98, 76, 100, 136, 68, 139, 251, 162,
			23, 154, 89, 245, 135, 179, 79, 19, 97, 69, 109, 141, 9, 129, 125, 50,
			189, 143, 64, 235, 134, 183, 123, 11, 240, 149, 33, 34, 92, 107, 78, 130,
			84, 214, 101, 147, 206, 96, 178, 28, 115, 86, 192, 20, 167, 140, 241, 220,
			18, 117, 202, 31, 59, 190, 228, 209, 66, 61, 212, 48, 163, 60, 182, 38,
			111, 191, 14, 218, 70, 105, 7, 87, 39, 242, 29, 155, 188, 148, 67, 3,
			248, 17, 199, 246, 144, 239, 62, 231, 6, 195, 213, 47, 200, 102, 30, 215,
			8, 232, 234, 222, 128, 82, 238, 247, 132, 170, 114, 172, 53, 77, 106, 42,
			150, 26, 210, 113, 90, 21, 73, 116, 75, 159, 208, 94, 4, 24, 164, 236,
			194, 224, 65, 110, 15, 81, 203, 204, 36, 145, 175, 80, 161, 244, 112, 57,
			153, 124, 58, 133, 35, 184, 180, 122, 252, 2, 54, 91, 37, 85, 151, 49,
			45, 93, 250, 152, 227, 138, 146, 174, 5, 223, 41, 16, 103, 108, 186, 201,
			211, 0, 230, 207, 225, 158, 168, 44, 99, 22, 1, 63, 88, 226, 137, 169,
			13, 56, 52, 27, 171, 51, 255, 176, 187, 72, 12, 95, 185, 177, 205, 46,
			197, 243, 219, 71, 229, 165, 156, 119, 10, 166, 32, 104, 254, 127, 193, 173
		};

		uint8 tmp[128];

		int	keylen = key.size32();
		memcpy(tmp, key, keylen);

		// Phase 1: Expand input key to 128 bytes
		for (int i = keylen; i < 128; i++)
			tmp[i] = permute[(tmp[i - 1] + tmp[i - keylen]) & 255];

		// Phase 2 - reduce effective key size to "bits"
		keylen = (bits + 7) >> 3;
		tmp[128 - keylen] = permute[tmp[128 - keylen] & (255 >> uint32(7 & -bits))];

		for (int i = 128 - keylen; i--;)
			tmp[i] = permute[tmp[i + 1] ^ tmp[i + keylen]];

		// Phase 3 - copy to xkey in little-endian order
		const uint16le	*p = (const uint16le*)tmp;
		for (int i = 0; i < 64; i++)
			xkey[i] = *p++;
	}

	void init(const const_memory_block &key) {
		init(key, key.size32() << 3);
	}

	void encrypt(const uint8 *plain, uint8 *cipher) const {
		const uint16le	*p = (const uint16le*)plain;
		uint16	x0 = p[0];
		uint16	x1 = p[1];
		uint16	x2 = p[2];
		uint16	x3 = p[3];

		for (int i = 0; i < 16; i++) {
			x0 = rotate_left(x0 + masked_write(x1, x2, x3) + xkey[4 * i + 0], 1);
			x1 = rotate_left(x1 + masked_write(x2, x3, x0) + xkey[4 * i + 1], 2);
			x2 = rotate_left(x2 + masked_write(x3, x0, x1) + xkey[4 * i + 2], 3);
			x3 = rotate_left(x3 + masked_write(x0, x1, x2) + xkey[4 * i + 3], 5);
			if (i == 4 || i == 10) {
				x0 += xkey[x3 & 63];
				x1 += xkey[x0 & 63];
				x2 += xkey[x1 & 63];
				x3 += xkey[x2 & 63];
			}
		}

		uint16le	*c = (uint16le*)cipher;
		c[0] = x0;
		c[1] = x1;
		c[2] = x2;
		c[3] = x3;
	}

	void decrypt(const uint8 *cipher, uint8 *plain) const {
		const uint16le	*c = (const uint16le*)cipher;
		uint16	x0 = c[0];
		uint16	x1 = c[1];
		uint16	x2 = c[2];
		uint16	x3 = c[3];

		for (int i = 16; i--;) {
			x3 = rotate_right(x3, 5) - masked_write(x0, x1, x2) - xkey[4 * i + 3];
			x2 = rotate_right(x2, 3) - masked_write(x3, x0, x1) - xkey[4 * i + 2];
			x1 = rotate_right(x1, 2) - masked_write(x2, x3, x0) - xkey[4 * i + 1];
			x0 = rotate_right(x0, 1) - masked_write(x1, x2, x3) - xkey[4 * i + 0];
			if (i == 5 || i == 11) {
				x3 -= xkey[x2 & 63];
				x2 -= xkey[x1 & 63];
				x1 -= xkey[x0 & 63];
				x0 -= xkey[x3 & 63];
			}
		};

		uint16le	*p = (uint16le*)plain;
		p[0] = x0;
		p[1] = x1;
		p[2] = x2;
		p[3] = x3;
	}
};


struct RC4 : xor_crypt_mixin<RC4> {
	static const int BITS = 8;
	uint8	i, j;
	uint8	s[256];

	uint8 next() {
		uint8 si = s[++i];
		uint8 sj = s[j += si];
		s[i]	 = sj;
		s[j]	 = si;
		return s[(si + sj) & 0xff];
	}
	void add_entropy(const const_memory_block &key) {
		const uint8	*k		= key;
		uint32		keylen	= key.size32();
		for (int x = 256; x--; ++i) {
			uint8 si = s[i];
			j		+= si + k[i % keylen];
			s[i]	= s[j];
			s[j]	= si;
		}
		--i;
	}
	RC4() : i(0), j(0) {
		for (int x = 0; x < 256; x++)
			s[x] = x;
	}
	RC4(const const_memory_block &key) : RC4() {
		add_entropy(key);
	}
	/*
	void process(const uint8 *input, uint8 *output, size_t length) {
		uint8 i = this->i;
		uint8 j = this->j;

		while (length--) {
			uint8	si	= s[++i];
			uint8	sj	= s[j += si];
			s[i] = sj;
			s[j] = si;

			//XOR the input data with the RC4 stream
			*output++ = *input++ ^ s[(sj + si) & 0xff];
		}

		//Save context
		this->i = i;
		this->j = j;
	}
	void encrypt(uint8 *data, size_t length) { process(data, data, length); }
	void decrypt(uint8 *data, size_t length) { process(data, data, length); }
	*/
};

struct RC4a {
	static const int BITS = 8;
	uint8	i, j1, j2;
	uint8	s1[256], s2[256];

	uint16 next() {
		uint8	s1i	= s1[++i];
		uint8	sj1 = s1[j1 += s1i];
		s1[i]	= sj1;
		s1[j1]	= s1i;
		uint8	out1 = s2[(sj1 + s1i) & 0xff];

		uint8	s2i	= s2[i];
		uint8	sj2	= s2[j2 += s2i];
		s2[i]	= sj2;
		s2[j2]	= s2i;
		uint8	out2 = s1[(sj2 + s2i) & 0xff];

		return out1 + (out2 << 8);
	}

	void add_entropy(const const_memory_block &key) {
		const uint8	*k		= key;
		uint32		keylen	= key.size32();
		i--;
		for (int x = 0; x < 256; x++) {
			++i;
			uint8	s1i	= s1[i];
			j1		+= s1i + k[x % keylen];
			s1[i]	= s1[j1];
			s1[j1]	= s1i;

			uint8	s2i	= s2[i];
			j2		+= s2i + k[x % keylen];
			s2[i]	= s2[j2];
			s2[j2]	= s2i;
		}
	}

	RC4a() : i(0), j1(0), j2(0) {
		for (int x = 0; x < 256; x++)
			s1[x] = s2[x] = x;
	}
	RC4a(const const_memory_block &key) : RC4a() {
		add_entropy(key);
	}
};


} //namespace iso

#endif //RC_H
