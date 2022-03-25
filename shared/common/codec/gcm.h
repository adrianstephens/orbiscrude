#ifndef GCM_H
#define GCM_H

//-----------------------------------------------------------------------------
//Galois/Counter Mode
//-----------------------------------------------------------------------------

#include "cipher_mode.h"

namespace iso {

//GCM
//GCM is defined for block ciphers with a block size of 128 bits.
//Galois message authentication code (GMAC) is an authentication-only variant of the GCM which can form an incremental message authentication code.
//Both GCM and GMAC can accept initialization vectors of arbitrary length. GCM can take full advantage of parallel processing and implementing GCM can make efficient use of an instruction pipeline or a hardware pipeline.
//Like in CTR, blocks are numbered sequentially, and then this block number is combined with an IV and encrypted with a block cipher E, usually AES.
//The result of this encryption is then XORed with the plaintext to produce the ciphertext.

template<int N> struct GCM {
	static const uint32 r[N];
	uint32	m[N][4];			// Precalculated table
	uint8	s[N], counter[N];
	uint8	final[N], mac[N];

	void init_table(uint32be *_h) {
		//Pre-compute M(0) = H * 0
		m[0][0] = 0;
		m[0][1] = 0;
		m[0][2] = 0;
		m[0][3] = 0;

		//Pre-compute M(1) = H * 1
		m[8][0] = _h[3];
		m[8][1] = _h[2];
		m[8][2] = _h[1];
		m[8][3] = _h[0];

		//Pre-compute all 4-bit multiples of H

		for (int i = 2; i < N; i += 2) {
			//even
			//Compute M(i) = M(i / 2) * x
			uint32	*s = m[reverse_bits4(i / 2)];
			uint32	h0 = s[0];
			uint32	h1 = s[1];
			uint32	h2 = s[2];
			uint32	h3 = s[3];

			//If the highest term of the result is equal to one, then perform reduction
			uint32	c		= h0 & 1 ? r[8] : 0;
			h0 = (h0 >> 1) | (h1 << 31);
			h1 = (h1 >> 1) | (h2 << 31);
			h2 = (h2 >> 1) | (h3 << 31);
			h3 = (h3 >> 1) ^ c;

			uint32	*d0	= m[reverse_bits4(i)];
			//The multiplication of a polynomial by x in GF(2^128) corresponds to a shift of indices
			d0[0] = h0;
			d0[1] = h1;
			d0[2] = h2;
			d0[3] = h3;

			//odd
			uint32	*d1	= m[reverse_bits4(i + 1)];
			//Compute M(i) = M(i - 1) + H
			d1[0] = h0 ^ m[8][0];
			d1[1] = h1 ^ m[8][1];
			d1[2] = h2 ^ m[8][2];
			d1[3] = h3 ^ m[8][3];
		}
	}

	void mul(uint8 *x) const {
		//Reduction table
		uint32 z0 = 0, z1 = 0, z2 = 0, z3 = 0;

		//Fast table-driven implementation
		for (int i = N; i--;) {
			//Process the lower nibble
			uint8	b = x[i] & 0x0F;
			uint8	c = z0 & 0x0F;
			z0 = (z0 >> 4) | (z1 << 28);
			z1 = (z1 >> 4) | (z2 << 28);
			z2 = (z2 >> 4) | (z3 << 28);
			z3 = (z3 >> 4) ^ r[c];

			z0 ^= m[b][0];
			z1 ^= m[b][1];
			z2 ^= m[b][2];
			z3 ^= m[b][3];

			//Process the upper nibble
			b = (x[i] >> 4) & 0x0F;

			c = z0 & 0x0F;
			z0 = (z0 >> 4) | (z1 << 28);
			z1 = (z1 >> 4) | (z2 << 28);
			z2 = (z2 >> 4) | (z3 << 28);
			z3 = (z3 >> 4) ^ r[c];

			z0 ^= m[b][0];
			z1 ^= m[b][1];
			z2 ^= m[b][2];
			z3 ^= m[b][3];
		}

		//Save the result
		uint32be	*x2 = (uint32be*)x;
		x2[0] = z3;
		x2[1] = z2;
		x2[2] = z1;
		x2[3] = z0;
	}

	template<typename T> GCM(T &&t, const const_memory_block &nonce, const const_memory_block &additional, streamptr length) {
		uint32 h[4] = {0, 0, 0, 0};
		t.process(h);
		init_table((uint32be*)h);

		init(nonce, additional, length);
		memcpy(mac, counter, N);
		t.process(mac);
	}

	void init(const const_memory_block &nonce, const const_memory_block &additional, streamptr length) {
		uint32	n = nonce.size32();
		if (n == 12) {
			memcpy(counter, nonce, 12);
			((uint32be*)counter)[3] = 1;

		} else {
			clear(counter);
			for (uint32 i = 0; i < n; i += N) {
				xor_block(counter, counter, nonce + i, min(n - i, N));
				mul(counter);
			}

			((uint32be*)counter)[3] ^= n * 8;
			mul(counter);
		}

		clear(s);
		n = additional.size32();
		for (uint32 i = 0; i < n; i += N) {
			xor_block(s, s, additional + i, min(n - i, N));
			mul(s);
		}

		//initialise final block
		*(uint64be*)(final + 0)	= n * 8;
		*(uint64be*)(final + 8)	= length * 8;
	}

	template<typename T> void encrypt(T &&t, const void *input, void *output, int n) {
		//Increment counter
		((uint32be*)counter)[3] += 1;

		//Encrypt plaintext
		uint8	temp[N];
		memcpy(temp, counter, N);
		t.process(temp);
		xor_block((uint8*)output, temp, (const uint8*)input, n);

		//Apply GHASH function
		xor_block(s, s, (uint8*)output, n);
		mul(s);
	}

	template<typename T> void decrypt(T &&t, const void *input, void *output, int n) {
		//Apply GHASH function
		xor_block(s, s, (const uint8*)input, n);
		mul(s);

		//Increment counter
		((uint32be*)counter)[3] += 1;

		//Decrypt ciphertext
		uint8	temp[N];
		memcpy(temp, counter, N);
		t.process(temp);
		xor_block((uint8*)output, temp, (const uint8*)input, n);
	}

	uint8* digest() {
		xor_block(s, s, final, N);
		mul(s);
		xor_block(mac, mac, s, N);
		return mac;
	}
};

template<int N> const uint32 GCM<N>::r[N] = {
	0x00000000,
	0x1C200000,
	0x38400000,
	0x24600000,
	0x70800000,
	0x6CA00000,
	0x48C00000,
	0x54E00000,
	0xE1000000,
	0xFD200000,
	0xD9400000,
	0xC5600000,
	0x91800000,
	0x8DA00000,
	0xA9C00000,
	0xB5E00000
};


template<typename T, int N, typename W> struct GCM_encrypt : block_writer<GCM_encrypt<T, N, W>, N> {
	T		t;
	W		writer;
	GCM<N>	gcm;
	uint32	mac_size;

	template<typename T1, typename W1> GCM_encrypt(T1 &&t1, W1 &&w, const const_memory_block &nonce, const const_memory_block &additional, uint32 mac_size, streamptr length)
		: t(forward<T1>(t1)), writer(forward<W1>(w)), gcm(t, nonce, additional, length), mac_size(mac_size)
	{}
	~GCM_encrypt() {
	}

	void process(const void *data) {
		uint8	output[N];
		gcm.encrypt(t, data, output, N);
		writer.writebuff(output, N);
	}
	const_memory_block digest() {
		memory_block	b	= this->buffered();
		if (int n = b.size32()) {
			uint8	output[N];
			gcm.encrypt(t, b, output, n);
			writer.writebuff(output, n);
		}
		return const_memory_block(gcm.digest(), mac_size);
	}
};

template<typename T, int N, typename R> struct GCM_decrypt : block_reader<GCM_decrypt<T, N, R>, N> {
	T		t;
	R		reader;
	GCM<N>	gcm;
	uint32	mac_size;

	template<typename T1, typename R1> GCM_decrypt(T1 &&t1, R1 &&r, const const_memory_block &nonce, const const_memory_block &additional, uint32 mac_size, streamptr length)
		: t(forward<T1>(t1)), reader(forward<R1>(r)), gcm(t, nonce, additional, length), mac_size(mac_size)
	{}
	void process(void *data) {
		uint8		input[N];
		auto		n = reader.readbuff(input, N);
		gcm.decrypt(t, input, data, n);
	}
	const_memory_block digest() {
		return const_memory_block(gcm.digest(), mac_size);
	}
};

} //namespace iso

#endif // GCM_H
