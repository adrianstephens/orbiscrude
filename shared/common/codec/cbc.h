#ifndef CBC_H
#define CBC_H

#include "cipher_mode.h"

// Cipher block chaining (CBC)
// Each block of plaintext is XORed with the previous ciphertext block before being encrypted.
// This way, each ciphertext block depends on all plaintext blocks processed up to that point.
// To make each message unique, an initialization vector must be used in the first block.

namespace iso {

template<int N> struct CBC {
	uint8	iv[N];

	CBC(const uint8 *_iv) {
		if (_iv)
			memcpy(iv, _iv, N);
		else
			clear(iv);
	}

	template<typename T> void encrypt(T &&t, const void *input, void *output) {
		xor_block((uint8*)output, (const uint8*)input, (uint8*)iv, N);
		t.process(output);
		memcpy(iv, output, N);
	}
	template<typename T> void encrypt(T &&t, const void *input, void *output, size_t length) {
		for (size_t offset = 0; offset < length; offset += N)
			encrypt(t, (const uint8*)input + offset, (uint8*)output + offset);
	}
	template<typename T> void decrypt(T &&t, const void *input, void *output) {
		uint8		block[N];
		memcpy(block, input, N);
		t.process(block);
		xor_block((uint8*)output, block, (uint8*)iv, N);
		memcpy(iv, input, N);
	}
	template<typename T> void decrypt(T &&t, const void *input, void *output, size_t length) {
		for (size_t offset = 0; offset < length; offset += N)
			decrypt(t, (const uint8*)input + offset, (uint8*)output + offset);
	}
};

template<typename T, int N, typename W> struct CBC_encrypt : CBC<N>, block_writer<CBC_encrypt<T, N, W>, N> {
	T		t;
	W		writer;

	template<typename T1, typename W1> CBC_encrypt(T1 &&t1, W1 &&w, const uint8 *iv = 0) : CBC<N>(iv), t(forward<T1>(t1)), writer(forward<W1>(w)) {}
	void process(const void *data) {
		uint8		output[N];
		CBC<N>::encrypt(t, data, output);
		writer.writebuff(output, N);
	}
};

template<typename T, int N, typename R> struct CBC_decrypt : CBC<N>, block_reader<CBC_decrypt<T, N, R>, N> {
	T		t;
	R		reader;
	uint8	iv[N];

	template<typename T1, typename R1> CBC_decrypt(T1&& t1, R1&& r, const uint8 *iv = 0) : CBC<N>(iv), t(forward<T1>(t1)), reader(forward<R1>(r)) {}
	void process(void *data) {
		uint8		input[N];
		reader.readbuff(input, N);
		CBC<N>::decrypt(t, input, data);
	}
};

} //namespace iso

#endif //CBC_H
