#ifndef CCM_H
#define CCM_H

#include "cipher_mode.h"

// Counter with cipher block chaining message authentication code (CCM)
// An authenticated encryption algorithm designed to provide both authentication and confidentiality
// CCM mode is only defined for block ciphers with a block length of 128 bits

namespace iso {

template<int N> struct CCM {
	uint32	nonce_size;
	uint32	mac_size;
	uint8	b[N];
	uint8	y[N];
	uint8	mac[N];

	CCM(const const_memory_block &nonce, uint32 _mac_size, streamptr length) : nonce_size(nonce.size32()), mac_size(_mac_size) {
		clear(y);
		y[0] = (((mac_size - 2) / 2) << 3) | (N - 2 - nonce_size);
		*(uint64be*)(y + N - 8) = length;
		nonce.copy_to(y + 1);

		clear(b);
		b[0] = N - 2 - nonce_size;
		nonce.copy_to(b + 1);
	}

	template<typename T> void do_additional(T &&t, const const_memory_block &additional) {
		uint8	b[N];
		clear(b);

		int i;
		uint32	aLen = additional.size32();
		if (aLen < 0xFF00) {
			*(uint16be*)b = aLen;
			i = 2;
		} else {
			b[0] = 0xFF;
			b[1] = 0xFE;
			*(uint32be*)(b + 2) = aLen;
			i = 6;
		}

		int	m = min(additional.size32(), N - i);
		memcpy(b + i, additional, m);

		xor_block(y, y, b, N);
		t.process(y);

		//Process the remaining data bytes
		for (const uint8 *a = additional + m, *e = additional.end(); a < e; a += m) {
			m = min(e - a, N);
			xor_block(y, y, a, m);
			t.process(y);
		}
	}

	template<typename T> void encrypt(T &&t, const void *input, void *output) {
		xor_block(y, y, (const uint8*)input, N);
		t.process(y);

		inc_block(b, N - nonce_size);
		memcpy(output, b, N);
		t.process(output);
		xor_block((uint8*)output, (const uint8*)output, (const uint8*)input, N);
	}

	template<typename T> void decrypt(T &&t, const void *input, void *output) {
		inc_block(b, N - nonce_size);

		memcpy(output, b, N);
		t.process(output);
		xor_block((uint8*)output, (const uint8*)output, (const uint8*)input, N);

		xor_block(y, y, (const uint8*)output, N);
		t.process(y);
	}

	const_memory_block	digest() {
		xor_block(mac, mac, y, mac_size);
		return const_memory_block(mac, mac_size);
	}
};

template<typename T, int N, typename W> struct CCM_encrypt : CCM<N>, block_writer<CCM_encrypt<T, N, W>, N> {
	T		t;
	W		writer;

	template<typename T1, typename W1> CCM_encrypt(T1 &&t1, W1 &&w, const const_memory_block &nonce, const const_memory_block &additional, uint32 mac_size, streamptr length) : CCM<N>(nonce, mac_size, length), t(forward<T1>(t1)), writer(forward<W1>(w)) {
		if (additional)
			this->y[0] |= 0x40;
		t.process(this->y);
		if (additional)
			CCM<N>::do_additional(t, additional);

		memcpy(this->mac, this->b, N);
		t.process(this->mac);
	}

	void process(const void *data) {
		uint8		output[N];
		CCM<N>::encrypt(t, data, output);
		writer.writebuff(output, N);
	}
};

template<typename T, int N, typename R> struct CCM_decrypt : CCM<N>, block_reader<CCM_decrypt<T, N, R>, N> {
	T		t;
	R		reader;

	template<typename T1, typename R1> CCM_decrypt(T1 &&t1, R1 &&r, const const_memory_block &nonce, const const_memory_block &additional, uint32 mac_size, streamptr length) : CCM<N>(nonce, mac_size, length), t(forward<T1>(t1)), reader(forward<R1>(r)) {
		if (additional)
			this->y[0] |= 0x40;
		t.process(this->y);
		if (additional)
			CCM<N>::do_additional(t, additional);

		memcpy(this->mac, this->b, N);
		t.process(this->mac);
	}

	void process(void *data) {
		uint8		input[N];
		reader.readbuff(input, N);
		CCM<N>::decrypt(t, input, data);
	}
};


} //namespace iso

#endif // CCM_H
