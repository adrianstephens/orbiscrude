#ifndef SPOOKY_H
#define SPOOKY_H

#include "base/defs.h"
#include "base/bits.h"

#define ALLOW_UNALIGNED_READS 1

namespace iso {

//
// SpookyHash: a 128-bit noncryptographic hash function
// By Bob Jenkins, public domain
//		Oct 31 2010: alpha, framework + SpookyHash::Mix appears right
//		Oct 31 2011: alpha again, Mix only good to 2^^69 but rest appears right
//		Dec 31 2011: beta, improved Mix, tested it for 2-bit deltas
//		Feb  2 2012: production, same bits as beta
//		Feb  5 2012: adjusted definitions of uint* to be more portable
//		Mar 30 2012: 3 bytes/cycle, not 4.  Alpha was 4 but wasn't thorough enough.
//		August 5 2012: SpookyV2 (different results)
//
// Up to 3 bytes/cycle for long messages. Reasonably fast for short messages.
// All 1 or 2 bit deltas achieve avalanche within 1% bias per output bit.
//
// This was developed for and tested on 64-bit x86-compatible processors.
// It assumes the processor is little-endian. There is a macro controlling whether unaligned reads are allowed (by default they are).
// This should be an equally good hash on big-endian machines, but it will compute different results on them than on little-endian machines.
//
// Google's CityHash has similar specs to SpookyHash, and CityHash is faster on new Intel boxes.
// MD4 and MD5 also have similar specs, but they are orders of magnitude slower.
// CRCs are two or more times slower, but unlike SpookyHash, they have nice math for combining the CRCs of pieces to form the CRCs of wholes.
// There are also cryptographic hashes, but those are even slower than MD5.

class SpookyHash {
	static const size_t NumVars		= 12;				// number of uint64's in internal state
	static const size_t BlockSize	= NumVars * 8;		// size of the internal state
	static const size_t BufferSize	= 2 * BlockSize;	// size of buffer of unhashed data, in bytes

	// spooky_const: a constant which:
	//	* is not zero
	//	* is odd
	//	* is a not-very-regular mix of 1's and 0's
	//	* does not need any other special mathematical properties
	static const uint64 spooky_const	= 0xdeadbeefdeadbeefLL;

	uint64	data[2 * NumVars];	// unhashed data, for partial messages
	uint64	state[NumVars];		// internal state of the hash
	size_t	total;				// total length of the input so far
	uint8	remainder;			// length of unhashed data stashed in data

	// Short is used for messages under 192 bytes in length
	// Short has a low startup cost, the normal mode is good for long keys, the cost crossover is at about 192 bytes. The two modes were held to the same quality bar.
	static void Short(const void *message, size_t length, uint64 *hash1, uint64 *hash2);

	// The goal is for each bit of the input to expand into 128 bits of apparent entropy before it is fully overwritten.
	// n trials both set and cleared at least m bits of h0 h1 h2 h3
	//	n: 2   m: 29
	//	n: 3   m: 46
	//	n: 4   m: 57
	//	n: 5   m: 107
	//	n: 6   m: 146
	//	n: 7   m: 152
	// when run forwards or backwards
	// for all 1-bit and 2-bit diffs
	// with diffs defined by either xor or subtraction
	// with a base of all zeros plus a counter, or plus another bit, or random
	static inline void ShortMix(uint64 &h0, uint64 &h1, uint64 &h2, uint64 &h3) {
		h2 = rotate_left(h2, 50) + h3; h0 ^= h2;
		h3 = rotate_left(h3, 52) + h0; h1 ^= h3;
		h0 = rotate_left(h0, 30) + h1; h2 ^= h0;
		h1 = rotate_left(h1, 41) + h2; h3 ^= h1;
		h2 = rotate_left(h2, 54) + h3; h0 ^= h2;
		h3 = rotate_left(h3, 48) + h0; h1 ^= h3;
		h0 = rotate_left(h0, 38) + h1; h2 ^= h0;
		h1 = rotate_left(h1, 37) + h2; h3 ^= h1;
		h2 = rotate_left(h2, 62) + h3; h0 ^= h2;
		h3 = rotate_left(h3, 34) + h0; h1 ^= h3;
		h0 = rotate_left(h0,  5) + h1; h2 ^= h0;
		h1 = rotate_left(h1, 36) + h2; h3 ^= h1;
	}

	// Mix all 4 inputs together so that h0, h1 are a hash of them all.
	// For two inputs differing in just the input bits
	// Where "differ" means xor or subtraction
	// And the base value is random, or a counting value starting at that bit
	// The final result will have each bit of h0, h1 flip
	// For every input bit,				with probability 50 +- .3% (it is probably better than that)
	// For every pair of input bits,	with probability 50 +- .75% (the worst case is approximately that)
	static inline void ShortEnd(uint64 &h0, uint64 &h1, uint64 &h2, uint64 &h3) {
		h3 ^= h2; h2 = rotate_left(h2, 15); h3 += h2;
		h0 ^= h3; h3 = rotate_left(h3, 52); h0 += h3;
		h1 ^= h0; h0 = rotate_left(h0, 26); h1 += h0;
		h2 ^= h1; h1 = rotate_left(h1, 51); h2 += h1;
		h3 ^= h2; h2 = rotate_left(h2, 28); h3 += h2;
		h0 ^= h3; h3 = rotate_left(h3, 9);  h0 += h3;
		h1 ^= h0; h0 = rotate_left(h0, 47); h1 += h0;
		h2 ^= h1; h1 = rotate_left(h1, 54); h2 += h1;
		h3 ^= h2; h2 = rotate_left(h2, 32); h3 += h2;
		h0 ^= h3; h3 = rotate_left(h3, 25); h0 += h3;
		h1 ^= h0; h0 = rotate_left(h0, 63); h1 += h0;
	}

	// This is used if the input is 96 bytes long or longer. The internal state is fully overwritten every 96 bytes.
	// Every input bit appears to cause at least 128 bits of entropy before 96 other bytes are combined, when run forward or backward
	//	For every input bit,
	//	Two inputs differing in just that input bit
	//	Where "differ" means xor or subtraction
	//	And the base value is random
	//	When run forward or backwards one Mix
	// I tried 3 pairs of each; they all differed by at least 212 bits.
	static inline void Mix(
		const uint64 *data,
		uint64 &s0, uint64 &s1, uint64 &s2, uint64 &s3,
		uint64 &s4, uint64 &s5, uint64 &s6, uint64 &s7,
		uint64 &s8, uint64 &s9, uint64 &s10, uint64 &s11
	) {
		s0  += data[0];		s2  ^= s10;	s11 ^=  s0;	s0	= rotate_left(s0,  11);	s11	+= s1;
		s1  += data[1];		s3  ^= s11;	s0  ^=  s1;	s1	= rotate_left(s1,  32);	s0	+= s2;
		s2  += data[2];		s4  ^= s0;	s1  ^=  s2;	s2	= rotate_left(s2,  43);	s1	+= s3;
		s3  += data[3];		s5  ^= s1;	s2  ^=  s3;	s3	= rotate_left(s3,  31);	s2	+= s4;
		s4  += data[4];		s6  ^= s2;	s3  ^=  s4;	s4	= rotate_left(s4,  17);	s3	+= s5;
		s5  += data[5];		s7  ^= s3;	s4  ^=  s5;	s5	= rotate_left(s5,  28);	s4	+= s6;
		s6  += data[6];		s8  ^= s4;	s5  ^=  s6;	s6	= rotate_left(s6,  39);	s5	+= s7;
		s7  += data[7];		s9  ^= s5;	s6  ^=  s7;	s7	= rotate_left(s7,  57);	s6	+= s8;
		s8  += data[8];		s10 ^= s6;	s7  ^=  s8;	s8	= rotate_left(s8,  55);	s7	+= s9;
		s9  += data[9];		s11 ^= s7;	s8  ^=  s9;	s9	= rotate_left(s9,  54);	s8	+= s10;
		s10 += data[10];	s0  ^= s8;	s9  ^= s10;	s10 = rotate_left(s10, 22);	s9	+= s11;
		s11 += data[11];	s1  ^= s9;	s10 ^= s11;	s11 = rotate_left(s11, 46);	s10	+= s0;
	}

	// Mix all 12 inputs together so that h0, h1 are a hash of them all.
	// For two inputs differing in just the input bits
	// Where "differ" means xor or subtraction
	// And the base value is random, or a counting value starting at that bit
	// The final result will have each bit of h0, h1 flip
	// For every input bit,				with probability 50 +- .3%
	// For every pair of input bits,	with probability 50 +- 3%
	static inline void EndPartial(
		uint64 &h0, uint64 &h1, uint64 &h2, uint64 &h3,
		uint64 &h4, uint64 &h5, uint64 &h6, uint64 &h7,
		uint64 &h8, uint64 &h9, uint64 &h10, uint64 &h11
	) {
		h11 += h1;	h2	^= h11;	h1  = rotate_left(h1,  44);
		h0  += h2;	h3	^= h0;	h2  = rotate_left(h2,  15);
		h1  += h3;	h4	^= h1;	h3  = rotate_left(h3,  34);
		h2  += h4;	h5	^= h2;	h4  = rotate_left(h4,  21);
		h3  += h5;	h6	^= h3;	h5  = rotate_left(h5,  38);
		h4  += h6;	h7	^= h4;	h6  = rotate_left(h6,  33);
		h5  += h7;	h8	^= h5;	h7  = rotate_left(h7,  10);
		h6  += h8;	h9	^= h6;	h8  = rotate_left(h8,  13);
		h7  += h9;	h10 ^= h7;	h9  = rotate_left(h9,  38);
		h8  += h10;	h11 ^= h8;	h10 = rotate_left(h10, 53);
		h9  += h11;	h0	^= h9;	h11 = rotate_left(h11, 42);
		h10 += h0;	h1	^= h10;	h0  = rotate_left(h0,  54);
	}

	// Two iterations was almost good enough for a 64-bit result, but a 128-bit result is reported, so End() does three iterations.
	static inline void End(
		const uint64 *data,
		uint64 &h0, uint64 &h1, uint64 &h2, uint64 &h3,
		uint64 &h4, uint64 &h5, uint64 &h6, uint64 &h7,
		uint64 &h8, uint64 &h9, uint64 &h10, uint64 &h11
	) {
		h0 += data[0];   h1 += data[1];   h2 += data[2];   h3 += data[3];
		h4 += data[4];   h5 += data[5];   h6 += data[6];   h7 += data[7];
		h8 += data[8];   h9 += data[9];   h10 += data[10]; h11 += data[11];
		EndPartial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
		EndPartial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
		EndPartial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
	}

public:
	// Hash128: hash a single message in one call, produce 128-bit output
	static void Hash128(const void *message, size_t length, uint64 *hash1, uint64 *hash2);

	// Hash64: hash a single message in one call, return 64-bit output
	static uint64 Hash64(const void *message, size_t length, uint64 seed) {
		uint64 hash1 = seed;
		Hash128(message, length, &hash1, &seed);
		return hash1;
	}

	// Hash32: hash a single message in one call, produce 32-bit output
	static uint32 Hash32(const void *message, size_t length, uint32 seed) {
		uint64 hash1 = seed, hash2 = seed;
		Hash128(message, length, &hash1, &hash2);
		return (uint32)hash1;
	}

	void Init(uint64 seed1, uint64 seed2) {
		total		= 0;
		remainder	= 0;
		state[0]	= seed1;
		state[1]	= seed2;
	}

	void Update(const void *message, size_t length);

	// compute the hash for the current SpookyHash state
	// This does not modify the state; you can keep updating it afterward
	void Final(uint64 *hash1, uint64 *hash2);
};

//
// short hash ... it could be used on any message, but it's used by Spooky just for short messages.
//
void SpookyHash::Short(const void *message, size_t length,  uint64 *hash1, uint64 *hash2) {
	uint64 buf[2 * NumVars];

	const uint64 *p = (const uint64*)message;

	if (!ALLOW_UNALIGNED_READS && (uintptr_t(p) & 0x7)) {
		memcpy(buf, message, length);
		p = buf;
	}

	uint64 a	= *hash1;
	uint64 b	= *hash2;
	uint64 c	= spooky_const;
	uint64 d	= spooky_const;

	// handle all complete sets of 32 bytes
	for (const uint64 *end = p + (length / 32) * 4; p < end; p += 4) {
		c	+= p[0];
		d	+= p[1];
		ShortMix(a, b, c, d);
		a	+= p[2];
		b	+= p[3];
	}

	//Handle the case of 16+ remaining bytes.
	if (length & 16) {
		c	+= p[0];
		d	+= p[1];
		ShortMix(a, b, c, d);
		p	+= 2;
	}

	// Handle the last 0..15 bytes, and its length
	d += ((uint64)length) << 56;
	const uint8	*p8 = (const uint8*)p;
	switch (length & 15) {
		case 15:	d += ((uint64)p8[14]) << 48;
		case 14:	d += ((uint64)p8[13]) << 40;
		case 13:	d += ((uint64)p8[12]) << 32;
		case 12:	d += ((const uint32*)p)[2]; c += p[0];
			break;
		case 11:	d += ((uint64)p8[10]) << 16;
		case 10:	d += ((uint64)p8[9])  << 8;
		case 9:		d += (uint64)p8[8];
		case 8:		c += p[0];
			break;
		case 7:		c += ((uint64)p8[6])  << 48;
		case 6:		c += ((uint64)p8[5])  << 40;
		case 5:		c += ((uint64)p8[4])  << 32;
		case 4:		c += ((const uint32*)p)[0];
			break;
		case 3:		c += ((uint64)p8[2])  << 16;
		case 2:		c += ((uint64)p8[1])  << 8;
		case 1:		c += (uint64)p8[0];
			break;
		case 0:		c += spooky_const; d += spooky_const;
			break;
	}
	ShortEnd(a, b, c, d);
	*hash1 = a;
	*hash2 = b;
}

// do the whole hash in one call
void SpookyHash::Hash128(const void *message, size_t length, uint64 *hash1, uint64 *hash2) {
	if (length < BufferSize) {
		Short(message, length, hash1, hash2);
		return;
	}

	uint64 h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
	uint64 buf[NumVars];

	h0 = h3 = h6 = h9	= *hash1;
	h1 = h4 = h7 = h10	= *hash2;
	h2 = h5 = h8 = h11	= spooky_const;

	const uint64 *p		= (const uint64*)message;
	const uint64 *end	= p + (length / BlockSize) * NumVars;

	// handle all whole BlockSize blocks of bytes
	if (ALLOW_UNALIGNED_READS || ((uintptr_t(p) & 0x7) == 0)) {
		while (p < end) {
			Mix(p, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
			p += NumVars;
		}
	} else {
		while (p < end) {
			memcpy(buf, p, BlockSize);
			Mix(buf, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
			p += NumVars;
		}
	}

	// handle the last partial block of BlockSize bytes
	size_t rem = length - ((const uint8*)end - (const uint8*)message);
	memcpy(buf, end, rem);
	memset((uint8*)buf + rem, 0, BlockSize - rem);
	((uint8*)buf)[BlockSize - 1] = rem;

	// do some final mixing
	End(buf, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
	*hash1 = h0;
	*hash2 = h1;
}

// add a message fragment to the state
void SpookyHash::Update(const void *message, size_t length) {
	size_t newLength = length + remainder;

	// Is this message fragment too short?  If it is, stuff it away.
	if (newLength < BufferSize) {
		memcpy((uint8*)data + remainder, message, length);
		total		+= length;
		remainder	= (uint8)newLength;
		return;
	}

	uint64 h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;

	// init the variables
	if (total < BufferSize) {
		h0	= h3 = h6 = h9	= state[0];
		h1	= h4 = h7 = h10 = state[1];
		h2	= h5 = h8 = h11 = spooky_const;
	} else {
		h0	= state[0];
		h1	= state[1];
		h2	= state[2];
		h3	= state[3];
		h4	= state[4];
		h5	= state[5];
		h6	= state[6];
		h7	= state[7];
		h8	= state[8];
		h9	= state[9];
		h10	= state[10];
		h11	= state[11];
	}
	total += length;

	// if we've got anything stuffed away, use it now
	if (remainder) {
		uint8 prefix = BufferSize - remainder;
		memcpy((uint8*)data + remainder, message, prefix);
		Mix(data, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
		Mix(data + NumVars, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
		length	-= prefix;
		message	= ((const uint8*)message) + prefix;
	}

	// handle all whole blocks of BlockSize bytes
	const uint64	*p		= (const uint64*)message;
	const uint64	*end	= p + (length / BlockSize) * NumVars;

	if (ALLOW_UNALIGNED_READS || (uintptr_t(p) & 0x7) == 0) {
		while (p < end) {
			Mix(p, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
			p += NumVars;
		}
	} else {
		while (p < end) {
			memcpy(data, p, BlockSize);
			Mix(data, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
			p += NumVars;
		}
	}

	// stuff away the last few bytes
	remainder = length - ((const uint8*)end - (const uint8*)message);
	memcpy(data, end, remainder);

	// stuff away the variables
	state[0]	= h0;
	state[1]	= h1;
	state[2]	= h2;
	state[3]	= h3;
	state[4]	= h4;
	state[5]	= h5;
	state[6]	= h6;
	state[7]	= h7;
	state[8]	= h8;
	state[9]	= h9;
	state[10]	= h10;
	state[11]	= h11;
}


// report the hash for the concatenation of all message fragments so far
void SpookyHash::Final(uint64 *hash1, uint64 *hash2) {
	// init the variables
	if (total < BufferSize) {
		*hash1 = state[0];
		*hash2 = state[1];
		Short(data, total, hash1, hash2);
		return;
	}

	uint64 h0	= state[0];
	uint64 h1	= state[1];
	uint64 h2	= state[2];
	uint64 h3	= state[3];
	uint64 h4	= state[4];
	uint64 h5	= state[5];
	uint64 h6	= state[6];
	uint64 h7	= state[7];
	uint64 h8	= state[8];
	uint64 h9	= state[9];
	uint64 h10	= state[10];
	uint64 h11	= state[11];

	const uint64	*p		= data;
	uint8			rem		= remainder;
	if (rem >= BlockSize) {
		// data can contain two blocks; handle any whole first block
		Mix(p, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
		p		+= NumVars;
		rem		-= BlockSize;
	}

	// mix in the last partial block, and the length mod BlockSize
	memset((uint8*)p + rem, 0, BlockSize - rem);
	((uint8*)p)[BlockSize - 1] = rem;

	// do some final mixing
	End(p, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);

	*hash1 = h0;
	*hash2 = h1;
}

} // namespace iso

#endif //SPOOKY_H

