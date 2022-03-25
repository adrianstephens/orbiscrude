#ifndef DES_H
#define DES_H

#include "base/bits.h"

namespace iso {

struct DES {
	uint32 ks[32];

	DES(const const_memory_block &key);
	void encrypt(const uint32be *input, uint32be *output);
	void decrypt(const uint32be *input, uint32be *output);
};

struct DESx3 {
	DES	k1, k2, k3;
	DESx3(const const_memory_block &key)
		: k1(key.slice_to(key.length() / 3))
		, k2(key.slice(key.length() / 3, key.length() * 2 / 3))
		, k3(key.slice(key.length() * 2 / 3))
	{}

	void encrypt(const uint32be *input, uint32be *output) {
		k1.encrypt(input, output);
		k2.decrypt(output, output);
		k3.encrypt(output, output);
	}
	void decrypt(const uint32be *input, uint32be *output) {
		k3.decrypt(input, output);
		k2.encrypt(output, output);
		k1.decrypt(output, output);
	}
};

} //namespace iso

#endif //DES_H
