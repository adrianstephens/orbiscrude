#ifndef CTR_H
#define CTR_H

#include "cipher_mode.h"

// CTR
//Note: CTR mode (CM) is also known as integer counter mode (ICM) and segmented integer counter (SIC) mode.
//Like OFB, counter mode turns a block cipher into a stream cipher.
//It generates the next keystream block by encrypting successive values of a "counter".
//The counter can be any function which produces a sequence which is guaranteed not to repeat for a long time, although an actual increment-by-one counter is the simplest and most popular.

namespace iso {

template<int N> struct CTR {
	uint8	counter[N];
	uint32	counter_size;

	CTR(uint32 _counter_size, const uint8 *_counter) : counter_size(_counter_size) {
		if (_counter)
			memcpy(counter, _counter, N);
		else
			clear(counter);
	}

	template<typename T> void process(T &&t, const void *input, void *output) {
		memcpy(output, counter, N);
		t.process(output);
		xor_block((uint8*)output, (const uint8*)output, (const uint8*)input, N);
		inc_block(counter + N - counter_size, counter_size);
	}
};

template<typename T, int N, typename W> struct CTR_encrypt : CTR<N>, block_writer<CTR_encrypt<T, N, W>, N> {
	T		t;
	W		writer;

	template<typename W1> CTR_encrypt(const uint8 *key, const uint8 *iv, W1&& w, uint32 counter_size = N, const uint8 *counter = 0) : CTR<N>(counter_size, counter), t(key, iv), writer(forward<W1>(w)) {}

	~CTR_encrypt() {
		if (uint32 rem = this->p % N) {
			uint8	output[N];
			CTR<N>::process(t, this->block, output);
			writer.writebuff(output, rem);
		}
	}

	void process(const void *data) {
		uint8	output[N];
		CTR<N>::process(t, data, output);
		writer.writebuff(output, N);
	}
};

template<typename T, int N, typename R> struct CTR_decrypt : CTR<N>, block_reader<CTR_decrypt<T, N, R>, N> {
	T		t;
	R		reader;

	template<typename R1> CTR_decrypt(const uint8 *key, const uint8 *iv, R1&& r, int counter_size = N, const uint8 *counter = 0) : CTR<N>(counter_size, counter), t(key, iv), reader(forward<R1>(r)) {}

	void process(void *data) {
		uint8		input[N];
		reader.readbuff(input, N);
		CTR<N>::process(t, input, data);
	}
};


} //namespace iso

#endif //CTR_H
