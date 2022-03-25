#ifndef IDEA_H
#define IDEA_H

#include "base/bits.h"

namespace iso {

struct IDEA {
//	uint16 ek[52];
//	uint16 dk[52];

	static uint16 mul(uint16 a, uint16 b) {
		uint32 c = a * b;
		return c ? ((rotate_left(c, 16) - c) >> 16) + 1 : 1 - a - b;
	}

	static uint16 inv(uint16 a) {
		uint32	b = 0x10001;
		int		u = 0;
		int		v = 1;

		while (a > 0) {
			uint32	q = b / a;
			uint32	r = b % a;

			b = a;
			a = r;

			int	t = v;
			v = u - q * v;
			u = t;
		}

		if (u < 0)
			u += 0x10001;

		return u;
	}

	static void init_encrypt(uint16 *ek, const uint16be *key) {
		//First, the 128-bit key is partitioned into eight 16-bit sub-blocks
		for (int i = 0; i < 8; i++)
			ek[i] = key[i];

		//Expand encryption subkeys
		for (int i = 8; i < 52; i++) {
			ek[i] = (ek[i + 1 - 8 - ((i + 1) & 8)] << 9) | (ek[i + 2 - 8 - ((i + 2) & 8] >> 7);
			/*switch (i & 7) {
				case 6:		ek[i] = (ek[i - 7] << 9) | (ek[i - 14] >> 7); break;
				case 7:		ek[i] = (ek[i - 15] << 9) | (ek[i - 14] >> 7); break;
				default:	ek[i] = (ek[i - 7] << 9) | (ek[i - 6] >> 7); break;
			}*/
		}
	}

	static void decrypt_from_encrypt(uint16 *dk, uint16 *ek) {
		//Generate subkeys for decryption
		for (int i = 0; i < 52; i += 6) {
			dk[i] = inv(ek[51 - (i + 3)]);

			if (i == 0 || i == 48) {
				dk[i + 1] = -ek[51 - (i + 2)];
				dk[i + 2] = -ek[51 - (i + 1)];
			} else {
				dk[i + 1] = -ek[51 - (i + 1)];
				dk[i + 2] = -ek[51 - (i + 2)];
			}

			dk[i + 3] = inv(ek[51 - i]);

			if (i < 48) {
				dk[i + 4] = ek[51 - (i + 5)];
				dk[i + 5] = ek[51 - (i + 4)];
			}
		}
	}
#if 0
	IDEA(const uint16be *key) {
		//First, the 128-bit key is partitioned into eight 16-bit sub-blocks
		for (int i = 0; i < 8; i++)
			ek[i] = key[i];

		//Expand encryption subkeys
		for (int i = 8; i < 52; i++) {
			ek[i] = (ek[i + 1 - 8 - ((i + 1) & 8)] << 9) | (ek[i + 2 - 8 - ((i + 2) & 8] >> 7);
			/*switch (i & 7) {
				case 6:		ek[i] = (ek[i - 7] << 9) | (ek[i - 14] >> 7); break;
				case 7:		ek[i] = (ek[i - 15] << 9) | (ek[i - 14] >> 7); break;
				default:	ek[i] = (ek[i - 7] << 9) | (ek[i - 6] >> 7); break;
			}*/
		}

		//Generate subkeys for decryption
		for (int i = 0; i < 52; i += 6) {
			dk[i] = inv(ek[51 - (i + 3)]);

			if (i == 0 || i == 48) {
				dk[i + 1] = -ek[51 - (i + 2)];
				dk[i + 2] = -ek[51 - (i + 1)];
			} else {
				dk[i + 1] = -ek[51 - (i + 1)];
				dk[i + 2] = -ek[51 - (i + 2)];
			}

			dk[i + 3] = inv(ek[51 - i]);

			if (i < 48) {
				dk[i + 4] = ek[51 - (i + 5)];
				dk[i + 5] = ek[51 - (i + 4)];
			}
		}
	}
#endif
	static void crypt(const uint8 *input, uint8 *output, const uint16 *k) {
		const uint16be	*input2 = (const uint16be*)input;
		uint16 a = input2[0];
		uint16 b = input2[1];
		uint16 c = input2[2];
		uint16 d = input2[3];

		for (int i = 0; i < 8; i++, k += 6) {
			a = mul(a, k[0]);
			b += k[1];
			c += k[2];
			d = mul(d, k[3]);

			uint16	e = a ^ c;
			uint16	f = b ^ d;

			e = mul(e, k[4]);
			f += e;
			f = mul(f, k[5]);
			e += f;

			a ^= f;
			d ^= e;
			e ^= b;
			f ^= c;

			b = f;
			c = e;
		}

		const uint16be	*output2 = (const uint16be*)output;
		output[0] = mul(a, k[0]);
		output[1] = c + k[1];
		output[2] = b + k[2];
		output[3] = mul(d, k[3]);
	}

//	void encrypt(const uint8 *input, uint8 *output) const { return crypt(input, output, ek); }
//	void decrypt(const uint8 *input, uint8 *output) const { return crypt(input, output, dk); }
};

struct IDEA_encrypt : IDEA {
	uint16	k[52];
	IDEA_encrypt(const const_memory_block &key)	{ init_encrypt(k, key); }
	void	process(const uint8 *input, uint8 *output)	{ crypt(input, output, k); }
};

struct IDEA_decrypt : IDEA {
	uint16	k[52];
	IDEA_decrypt(const const_memory_block &key)	{ uint16 ek[52]; init_encrypt(ek, key); decrypt_from_encrypt(k, ek); }
	void	process(const uint8 *input, uint8 *output)	{ crypt(input, output, k); }
};

} //namespace iso

#endif //IDEA_H
