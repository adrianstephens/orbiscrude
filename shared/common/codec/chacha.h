#ifndef CHACHA_H
#define CHACHA_H

#include "hashes/hash_stream.h"
#include "base/array.h"
#include "base/bits.h"
#include "hashes/hash_stream.h"

namespace iso {

/// Implementation of the ChaCha20 cipher with a 96-bit nonce, as specified in RFC 7539
/// https://tools.ietf.org/html/rfc7539

struct ChaCha20 {
	enum { BlockSize = 64 };
	uint32	s[16];

	template<int A, int B, int C, int D> inline void quarter_round(uint32 x[16]);

	ChaCha20(uint8 key[32], uint8 nonce[12]) {
		static const uint32 sigma[] = {0x61707865, 0x3320646E, 0x79622D32, 0x6B206574};
		memcpy(s + 0, sigma,	sizeof(sigma));
		memcpy(s + 4, key,		32);

		s[12] = 0; // Counter
		memcpy(s + 13, nonce,	12);	// IV setup
	}

	void next(void *data);
};


struct Salsa20 {
	enum { BlockSize = 64 };
	uint32	s[16];

	template<int A, int B, int C, int D> inline void quarter_round(uint32 x[16]);

	Salsa20(uint8 key[32], uint8 iv[8]) {
		// Key setup
		memcpy(s +  1, key,			4  * sizeof(uint32));
		memcpy(s + 11, key + 16,	4  * sizeof(uint32));
		s[ 0] = 0x61707865;
		s[ 5] = 0x3320646E;
		s[10] = 0x79622D32;
		s[15] = 0x6B206574;

		// IV setup
		memcpy(s + 6, iv, 8);
		s[ 8] = 0; // Counter, low
		s[ 9] = 0; // Counter, high
	}

	void next(void *data);
};

#if 0
template<typename T> struct XorCipher {
	T		t;
	uint8	block[T::BlockSize];
	uint32	offset;

	template<typename... PP> XorCipher(const PP&...pp) : t(pp...), offset(0) {}

	int		read(istream &stream, void *buffer, size_t size) {
		uint8	read_block[T::BlockSize];
		uint32	done = 0;
		while (done < size) {
			uint32	block_offset	= offset % T::BlockSize;
			uint32	block_size		= min(size - done, T::BlockSize - block_offset);
			if (offset == 0)
				T::next(block);

			uint32	read_size		= stream.readbuff(read_block, block_size);
			xor((uint8*)buffer + done, read_block, block + block_offset, read_size);
			offset	+= read_size;
			done	+= read_size;
			if (read_size != block_size)
				break;
		}
		return done;
	}
	int		write(ostream &stream, const void *buffer, size_t size) {
		uint8	write_block[T::BlockSize];
		uint32	done = 0;
		while (done < size) {
			uint32	block_offset	= offset % T::BlockSize;
			uint32	block_size		= min(size - done, T::BlockSize - block_offset);
			if (offset == 0)
				T::next(block);

			xor(write_block, (const uint8*)buffer + done, block + block_offset);
			uint32	write_size		= stream.writebuff(write_block, block_size);
			offset	+= write_size;
			done	+= write_size;
			if (write_size != block_size)
				break;
		}
		return done;
	}
};
#endif

struct poly1305 {
	uint64 h[3];
	uint64 r[2];

	poly1305(const uint8 key[16]) {
		// h = 0
		h[0] = 0;
		h[1] = 0;
		h[2] = 0;

		// r &= 0xffffffc0ffffffc0ffffffc0fffffff
		r[0] = ((uint64le*)key)[0] & 0x0ffffffc0fffffff;
		r[1] = ((uint64le*)key)[1] & 0x0ffffffc0ffffffc;
	}

	void process(const uint8 *inp, size_t len, uint32 padbit = 1);
	void emit(uint8 mac[16], const uint32 nonce[4]);
};

struct Poly1305 : poly1305, block_writer2<Poly1305, 16> {
	struct CODE { uint8 x[16]; };
    uint32	nonce[4];

	Poly1305(const uint8 key[16]) : poly1305(key) {}
	CODE digest();
};

} //namespace iso

#endif
