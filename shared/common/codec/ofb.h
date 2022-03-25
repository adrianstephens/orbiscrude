#ifndef OFB_H
#define OFB_H

#include "cipher_mode.h"

namespace iso {

template<int N> struct OFB {
	uint8	iv[N];
	uint32	s;
	OFB(uint32 _s, const uint8 *_iv = 0) : s(_s) {
		if (_iv)
			memcpy(iv, _iv, N);
		else
			clear(iv);
	}
	template<typename T> static void process(T &t, const void *input, void *output, void *iv) {
		memcpy(output, iv, N);
		t.process(output);
		memmove(iv, iv + s, N - s);
		memcpy(iv + N - s, output, s);
		xor_block(output, output, input, N);
	}
};

template<typename T, int N, typename W> struct OFB_encrypt : OFB, block_writer<OFB_encrypt<T, N, W>, N> {
	T		t;
	W		writer;

	template<typename TP, typename WP> OFB_encrypt(TP tp, WP wp, uint32 s, const uint8 *iv = 0) : OFB<N>(s, iv), t(tp), writer(wp) {}
	void process(const void *data) {
		uint8		output[N];
		OFB<N>::process(t, data, output);
		writer.writebuff(output, N);
	}
};

template<typename T, int N, typename R> struct OFB_decrypt : OFB, block_reader<OFB_decrypt<T, N, R>, N> {
	T		t;
	R		reader;

	template<typename TP, typename RP> OFB_decrypt(TP tp, RP rp, uint32 s, const uint8 *iv = 0) : OFB<N>(s, iv), t(tp), reader(rp) {}
	void process(void *data) {
		uint8		input[N];
		reader.readbuff(input, N);
		OFB<N>::process(t, input, data);
	}
};

} //namespace iso

#endif //OFB_H
