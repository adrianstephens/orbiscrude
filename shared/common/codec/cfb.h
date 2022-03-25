#ifndef CFB_H
#define CFB_H

#include "cipher_mode.h"

//Cipher feedback (CFB)
// The cipher feedback (CFB) mode, in its simplest form is using the entire output of the block cipher.
// In this variation, it is very similar to CBC, makes a block cipher into a self-synchronizing stream cipher.
// CFB decryption in this variation is almost identical to CBC encryption performed in reverse

namespace iso {

template<int N> struct CFB {
	template<typename T> static void encrypt(T &t, const void *input, void *output, void *iv) {
		enum { B = T::BLOCK_SIZE };
		uint8	block[B];
		memcpy(block, iv, B);
		t.process(block);

		xor_block(output, block, (const uint8*)input, N);
		memmove(iv, (uint8*)iv + N, B - N);
		memcpy((uint8*)iv + B - N, output, N);
	}
	template<typename T> static void encrypt(T &t, const void *input, void *output, size_t length, void *iv) {
		for (size_t offset = 0; offset < length; offset += N)
			encrypt(t, (const uint8*)input + offset, (uint8*)output + offset, iv);
	}
	template<typename T> static void decrypt(T &t, const void *input, void *output, void *iv) {
		enum { B = T::BLOCK_SIZE };
		uint8	block[B];
		memcpy(block, iv, B);
		t.process(block);

		memmove(iv, (uint8*)iv + N, B - N);
		memcpy((uint8*)iv + B - N, input, N);
		xor_block((uint8*)output, (uint8*)iv + B - N, block, N);
	}
	template<typename T> static void decrypt(T &t, const void *input, void *output, size_t length, void *iv) {
		for (size_t offset = 0; offset < length; offset += N)
			decrypt(t, (const uint8*)input + offset, (uint8*)output + offset, iv);
	}
};

template<typename T, int N, typename W> struct CFB_encrypt : block_writer<CFB_encrypt<T, N, W>, N> {
	enum { B = T::BLOCK_SIZE };
	T		t;
	W		writer;
	uint8	iv[B];

	template<typename T1, typename W1> CFB_encrypt(T1 &&t, W1 &&w, const uint8 *_iv = 0) : t(forward<T1>(t)), writer(forward<W1>(w)) {
		if (_iv)
			memcpy(iv, _iv, B);
		else
			clear(iv);
	}
	void process(const void *data) {
		uint8	output[N];
		CFB<N>::encrypt(t, data, output, iv);
		writer.write(output, N);
	}
};

template<typename T, int N, typename R> struct CFB_decrypt : block_reader<CFB_decrypt<T, N, R>, N> {
	enum { B = T::BLOCK_SIZE };
	T		t;
	R		reader;
	uint8	iv[B];

	template<typename T1, typename R1> CFB_decrypt(T &&t, R &&r, const uint8 *_iv = 0) : t(forward<T1>(t)), reader(forward<R1>(r)) {
		if (_iv)
			memcpy(iv, _iv, B);
		else
			clear(iv);
	}
	void process(void *data) {
		uint8		input[N];
		reader.readbuff(input, N);
		CFB<N>::decrypt(t, input, data, iv);
	}
};

} //namespace iso

#endif //CFB_H
