#ifndef YARROW_H
#define YARROW_H

#include "hashes/SHA.h"
#include "aes.h"

namespace iso {

struct Yarrow {
	enum {
		N				= 3,
		K				= 2,
		PG				= 10,
		FAST_THRESHOLD	= 100,
		SLOW_THRESHOLD	= 160,
	};

	bool	ready;
	bool	is_fast[N];

	SHA256	fast, slow;
	uint8	fast_entropy[N], slow_entropy[N];

	AES			aes;
	array<uint8,32>	key;
	AES::block	counter;	//Counter block
	size_t		block_count;				//Number of blocks that have been generated

	Yarrow() : ready(false), block_count(0) { clear(counter); }

	void seed(const uint8 *input, size_t length) {
		fast.writebuff(input, length);
		fast_reseed();
	}

	void add_entropy(uint32 source, const uint8 *input, size_t length, size_t entropy) {
		//Entropy from samples are collected into two pools
		if (is_fast[source]) {
			fast.writebuff(input, length);
			if (fast_entropy[source] += entropy >= FAST_THRESHOLD)
				fast_reseed();

			is_fast[source] = false;

		} else {
			slow.writebuff(input, length);
			slow_entropy[source] = min(slow_entropy[source] + entropy, SLOW_THRESHOLD);

			//At least two different sources must be over 160 bits in the slow pool before the slow pool reseeds
			int	k = 0;
			for (int i = 0; i < N; i++) {
				if (slow_entropy[i] >= SLOW_THRESHOLD)
					k++;
			}
			if (k >= K)
				slow_reseed();

			is_fast[source] = true;
		}
	}

	int	readbuff(uint8 *output, size_t length) {
		uint8	buffer[AES::BLOCK_SIZE];

		//Generate random data in a block-by-block fashion
		for (size_t remaining = length; remaining > 0;) {
			//Number of bytes to process at a time
			size_t	n = min(length, AES::BLOCK_SIZE);

			generate_block(buffer);			//Generate a random block
			memcpy(output, buffer, n);		//Copy data to the output buffer

			//We keep track of how many blocks we have output
			block_count++;

			//Next block
			output += n;
			remaining -= n;
		}

		//Apply generator gate?
		if (block_count >= PG) {
			generate_block(key);					//Generate some random bytes
			aes.setkey_enc(key, sizeof(key) * 8);	//Use them as the new key
			block_count = 0;						//Reset block counter
		}
		return int(length);
	}

	void generate_block(uint8 *output) {
		//Encrypt counter block
		aes.encrypt(counter, *(AES::block*)output);

		//Increment counter value
		for (int i = AES::BLOCK_SIZE; i--;) {
			if (++counter[i] != 0)
				break;
		}
	}

	void fast_reseed() {
		//use the current key and the hash of all inputs to the fast pool since the last reseed, to generate a new key
		fast.writebuff(key, sizeof(key));
		key = fast.digest();

		aes.setkey_enc(key, sizeof(key) * 8);

		//Define the new value of the counter
		memset(counter, 0, sizeof(counter));
		aes.encrypt(counter, counter);

		fast.reset();
		clear(fast_entropy);
		ready = true;
	}

	void slow_reseed() {
		slow.writebuff(key, sizeof(key));
		slow.write(fast.digest());
		key = slow.digest();

		aes.setkey_enc(key, sizeof(key) * 8);

		//Define the new value of the counter
		clear(counter);
		aes.encrypt(counter, counter);

		//Reset the hash contexts
		fast.reset();
		slow.reset();

		//The entropy estimates for both pools are reset to zero
		for (int i = 0; i < N; i++) {
			fast_entropy[i] = 0;
			slow_entropy[i] = 0;
		}
		ready = true;
	}
};

} //namespace iso

#endif // YARROW_H
