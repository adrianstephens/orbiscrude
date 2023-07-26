#ifndef SPOOKY_H
#define SPOOKY_H

#include "base/defs.h"

#define ALLOW_UNALIGNED_READS 1

namespace iso {

//-------------------------------------------------------------------------------
//lookup3.c, by Bob Jenkins, May 2006, Public Domain.
//-------------------------------------------------------------------------------

struct jenkins_state {
	uint32  a, b, c;

	jenkins_state(size_t length, uint32 seed) {
		a = b = c = 0xdeadbeef + ((uint32)length << 2) + seed;
	}
	jenkins_state(size_t length, uint64 seed) : jenkins_state(length, uint32(seed >> 32)) {
		c += seed;
	}
	void mix() {
		a -= c; a ^= rotate_left(c, 4); c += b;
		b -= a; b ^= rotate_left(a, 6); a += c;
		c -= b; c ^= rotate_left(b, 8); b += a;
		a -= c; a ^= rotate_left(c,16); c += b;
		b -= a; b ^= rotate_left(a,19); a += c;
		c -= b; c ^= rotate_left(b, 4); b += a;
	}
	void add(uint32 k0, uint32 k1, uint32 k2) {
		a += k0;
		b += k1;
		c += k2;
	}
	void addmix(uint32 k0, uint32 k1, uint32 k2) {
		a += k0;
		b += k1;
		c += k2;
		mix();
	}
	void final() {
		c ^= b; c -= rotate_left(b,14);
		a ^= c; a -= rotate_left(c,11);
		b ^= a; b -= rotate_left(a,25);
		c ^= b; c -= rotate_left(b,16);
		a ^= c; a -= rotate_left(c, 4); 
		b ^= a; b -= rotate_left(a,14);
		c ^= b; c -= rotate_left(b,24);
	}
	
	operator uint32() const { return c; }
	operator uint64() const { return (uint64(c) << 32) | b; }
};

uint32 lookup3(const uint32* k, size_t length, uint32 seed) {
	jenkins_state state(length, seed);
	while (length > 3) {
		state.addmix(k[0], k[1], k[2]);
		length	-= 3;
		k		+= 3;
	}
	switch (length) {
		case 3: state.c += k[2];
		case 2: state.b += k[1];
		case 1: state.a += k[0]; state.final();
		default: return state;
	}
}

uint64 lookup3(const uint32* k, size_t length, uint64 seed) {
	jenkins_state state(length, seed);
	while (length > 3) {
		state.addmix(k[0], k[1], k[2]);
		length	-= 3;
		k		+= 3;
	}
	switch (length) {
		case 3: state.c += k[2];
		case 2: state.b += k[1];
		case 1: state.a += k[0]; state.final();
		default: return state;
	}
}

template<bool be> struct lookup3_s;

template<> struct lookup3_s<false> {
	template<typename T> static T f(const void* key, size_t length, T seed) {
		jenkins_state state(length, seed);

	#ifndef ISO_BIGENDIAN
		if ((intptr_t(key) & 3) == 0) {
			auto	k = (const uint32le*)key; // read 32-bit chunks 
			while (length > 12) {
				state.addmix(k[0], k[1], k[2]);
				length -= 12;
				k += 3;
			}
			switch (length) {
				case 12:state.add(k[0], k[1], k[2]);				break;
				case 11:state.add(k[0], k[1], k[2] & 0xffffff);		break;
				case 10:state.add(k[0], k[1], k[2] & 0xffff);		break;
				case 9:	state.add(k[0], k[1], k[2] & 0xff);			break;
				case 8:	state.add(k[0], k[1], 0);					break;
				case 7:	state.add(k[0], k[1] & 0xffffff, 0);		break;
				case 6:	state.add(k[0], k[1] & 0xffff, 0);			break;
				case 5:	state.add(k[0], k[1] & 0xff, 0);			break;
				case 4: state.a += k[0];							break;
				case 3: state.a += k[0] & 0xffffff;					break;
				case 2: state.a += k[0] & 0xffff;					break;
				case 1: state.a += k[0] & 0xff;						break;
				case 0: return state; // zero length strings require no mixing
			}
			state.final();
			return state;
		}
	#endif
		auto	k = (const uint8*)key;	// read the key one byte at a time
		while (length > 12) {
			state.addmix(
				load_packed<uint32le>(k),
				load_packed<uint32le>(k + 4),
				load_packed<uint32le>(k + 8)
			);
			length -= 12;
			k += 12;
		}
		switch (length) {
			case 12: state.c += (uint32)k[11] << 24;
			case 11: state.c += (uint32)k[10] << 16;
			case 10: state.c += (uint32)k[9] << 8;
			case 9:	 state.c += k[8];
			case 8:	 state.b += (uint32)k[7] << 24;
			case 7:	 state.b += (uint32)k[6] << 16;
			case 6:	 state.b += (uint32)k[5] << 8;
			case 5:	 state.b += k[4];
			case 4:	 state.a += (uint32)k[3] << 24;
			case 3:	 state.a += (uint32)k[2] << 16;
			case 2:	 state.a += (uint32)k[1] << 8;
			case 1:	 state.a += k[0]; state.final();
			default: return state;
		}
	}
};

template<> struct lookup3_s<true> {
	template<typename T> static T f(const void* key, size_t length, T seed) {
		jenkins_state state(length, seed);
	#ifdef ISO_BIGENDIAN
		if ((intptr_t(key) & 3) == 0) {
			auto	k = (const uint32be*)key; // read 32-bit chunks 
			while (length > 12) {
				state.addmix(k[0], k[1], k[2]);
				length -= 12;
				k += 3;
			}
			switch (length) {
				case 12:state.add(k[0], k[1], k[2]);				break;
				case 11:state.add(k[0], k[1], k[2] & 0xffffff00);	break;
				case 10:state.add(k[0], k[1], k[2] & 0xffff0000);	break;
				case 9:	state.add(k[0], k[1], k[2] & 0xff000000);	break;
				case 8:	state.add(k[0], k[1], 0);					break;
				case 7:	state.add(k[0], k[1] & 0xffffff00, 0);		break;
				case 6:	state.add(k[0], k[1] & 0xffff0000, 0);		break;
				case 5:	state.add(k[0], k[1] & 0xff000000, 0);		break;
				case 4: state.a += k[0];							break;
				case 3: state.a += k[0] & 0xffffff00;				break;
				case 2: state.a += k[0] & 0xffff0000;				break;
				case 1: state.a += k[0] & 0xff000000;				break;
				case 0: return state; // zero length strings require no mixing
			}
			state.final();
			return state;
		}
	#endif
		auto	k = (const uint8*)key;	// read the key one byte at a time
		while (length > 12) {
			state.addmix(
				load_packed<uint32be>(k),
				load_packed<uint32be>(k + 4),
				load_packed<uint32be>(k + 8)
			);
			length -= 12;
			k += 12;
		}
		switch (length) {
			case 12: state.c += k[11];
			case 11: state.c += (uint32)k[10] << 8;
			case 10: state.c += (uint32)k[9] << 16;
			case 9:  state.c += (uint32)k[8] << 24;
			case 8:  state.b += k[7];
			case 7:  state.b += (uint32)k[6] << 8;
			case 6:  state.b += (uint32)k[5] << 16;
			case 5:  state.b += (uint32)k[4] << 24;
			case 4:  state.a += k[3];
			case 3:  state.a += (uint32)k[2] << 8;
			case 2:  state.a += (uint32)k[1] << 16;
			case 1:  state.a += (uint32)k[0] << 24; state.final();
			default: return state;
		}
	}
};

auto lookup3(const void* key, size_t length, uint32le seed) { return lookup3_s<false>::f(key, length, native_endian(seed)); }
auto lookup3(const void* key, size_t length, uint64le seed) { return lookup3_s<false>::f(key, length, native_endian(seed)); }
auto lookup3(const void* key, size_t length, uint32be seed) { return lookup3_s<true>::f(key, length, native_endian(seed)); }
auto lookup3(const void* key, size_t length, uint64be seed) { return lookup3_s<true>::f(key, length, native_endian(seed)); }

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

constexpr const uint64 spooky_const	= 0xdeadbeefdeadbeefLL;

//-----------------------------------------------------------------------------
// spooky_short has lower startup cost; the cost crossover is at about 192 bytes
//-----------------------------------------------------------------------------

struct SpookyShortState {
	uint64 a, b, c, d;

	SpookyShortState(uint64 a, uint64 b, uint64 c, uint64 d) : a(a), b(b), c(c), d(d) {}

	// The goal is for each bit of the input to expand into 128 bits of apparent entropy before it is fully overwritten.
	// n trials both set and cleared at least m bits of a b c d
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
	void Mix() {
		c = rotate_left(c, 50) + d; a ^= c;
		d = rotate_left(d, 52) + a; b ^= d;
		a = rotate_left(a, 30) + b; c ^= a;
		b = rotate_left(b, 41) + c; d ^= b;
		c = rotate_left(c, 54) + d; a ^= c;
		d = rotate_left(d, 48) + a; b ^= d;
		a = rotate_left(a, 38) + b; c ^= a;
		b = rotate_left(b, 37) + c; d ^= b;
		c = rotate_left(c, 62) + d; a ^= c;
		d = rotate_left(d, 34) + a; b ^= d;
		a = rotate_left(a,  5) + b; c ^= a;
		b = rotate_left(b, 36) + c; d ^= b;
	}
	// Mix all 4 inputs together so that a, b are a hash of them all.
	// For two inputs differing in just the input bits
	// Where "differ" means xor or subtraction
	// And the base value is random, or a counting value starting at that bit
	// The final result will have each bit of a, b flip
	// For every input bit,				with probability 50 +- .3% (it is probably better than that)
	// For every pair of input bits,	with probability 50 +- .75% (the worst case is approximately that)
	void End() {
		d ^= c; c = rotate_left(c, 15); d += c;
		a ^= d; d = rotate_left(d, 52); a += d;
		b ^= a; a = rotate_left(a, 26); b += a;
		c ^= b; b = rotate_left(b, 51); c += b;
		d ^= c; c = rotate_left(c, 28); d += c;
		a ^= d; d = rotate_left(d, 9);  a += d;
		b ^= a; a = rotate_left(a, 47); b += a;
		c ^= b; b = rotate_left(b, 54); c += b;
		d ^= c; c = rotate_left(c, 32); d += c;
		a ^= d; d = rotate_left(d, 25); a += d;
		b ^= a; a = rotate_left(a, 63); b += a;
	}
};

inline void spooky_short(const void *message, size_t length,  uint64 *hash1, uint64 *hash2) {
	const uint64 *p = (const uint64*)message;
	if (!ALLOW_UNALIGNED_READS && (uintptr_t(p) & 7)) {
		auto	buf = alloc_auto(uint64, (length + 7) / 8);
		memcpy(buf, message, length);
		p = buf;
	}

	SpookyShortState	state(*hash1, *hash2, spooky_const, spooky_const);

	// handle all complete sets of 32 bytes
	for (const uint64 *end = p + (length / 32) * 4; p < end; p += 4) {
		state.c	+= p[0];
		state.d	+= p[1];
		state.Mix();
		state.a	+= p[2];
		state.b	+= p[3];
	}

	//Handle the case of 16+ remaining bytes.
	if (length & 16) {
		state.c	+= p[0];
		state.d	+= p[1];
		state.Mix();
		p	+= 2;
	}

	// Handle the last 0..15 bytes, and its length
	state.d += ((uint64)length) << 56;
	const uint8	*p8 = (const uint8*)p;
	switch (length & 15) {
		case 15:	state.d += ((uint64)p8[14]) << 48;
		case 14:	state.d += ((uint64)p8[13]) << 40;
		case 13:	state.d += ((uint64)p8[12]) << 32;
		case 12:	state.d += ((const uint32*)p)[2];
			state.c += p[0];
			break;
		case 11:	state.d += ((uint64)p8[10]) << 16;
		case 10:	state.d += ((uint64)p8[9])  << 8;
		case 9:		state.d += (uint64)p8[8];
		case 8:		state.c += p[0];
			break;
		case 7:		state.c += ((uint64)p8[6])  << 48;
		case 6:		state.c += ((uint64)p8[5])  << 40;
		case 5:		state.c += ((uint64)p8[4])  << 32;
		case 4:		state.c += ((const uint32*)p)[0];
			break;
		case 3:		state.c += ((uint64)p8[2])  << 16;
		case 2:		state.c += ((uint64)p8[1])  << 8;
		case 1:		state.c += (uint64)p8[0];
			break;
		case 0:		state.c += spooky_const;
			state.d += spooky_const;
			break;
	}
	state.End();
	*hash1 = state.a;
	*hash2 = state.b;
}

//-----------------------------------------------------------------------------
// spooky
//-----------------------------------------------------------------------------

struct SpookyState {
	uint64	s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
		
	SpookyState(uint64 a, uint64 b, uint64 c) {
		s0 = s3 = s6 = s9	= a;
		s1 = s4 = s7 = s10	= b;
		s2 = s5 = s8 = s11	= c;
	}

	// Every input bit appears to cause at least 128 bits of entropy before 96 other bytes are combined, when run forward or backward
	//	For every input bit, two inputs differing in just that input bit (where "differ" means xor or subtraction) and the base value is random
	//	When run forward or backwards one Mix
	// I tried 3 pairs of each; they all differed by at least 212 bits.
	void Mix(const uint64 *data) {
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
	// For two inputs differing in just the input bits (where "differ" means xor or subtraction) and the base value is random, or a counting value starting at that bit,
	// The final result will have each bit of h0, h1 flip
	// For every input bit,				with probability 50 +- .3%
	// For every pair of input bits,	with probability 50 +- 3%
	void EndPartial() {
		s11 += s1;	s2	^= s11;	s1  = rotate_left(s1,  44);
		s0  += s2;	s3	^= s0;	s2  = rotate_left(s2,  15);
		s1  += s3;	s4	^= s1;	s3  = rotate_left(s3,  34);
		s2  += s4;	s5	^= s2;	s4  = rotate_left(s4,  21);
		s3  += s5;	s6	^= s3;	s5  = rotate_left(s5,  38);
		s4  += s6;	s7	^= s4;	s6  = rotate_left(s6,  33);
		s5  += s7;	s8	^= s5;	s7  = rotate_left(s7,  10);
		s6  += s8;	s9	^= s6;	s8  = rotate_left(s8,  13);
		s7  += s9;	s10 ^= s7;	s9  = rotate_left(s9,  38);
		s8  += s10;	s11 ^= s8;	s10 = rotate_left(s10, 53);
		s9  += s11;	s0	^= s9;	s11 = rotate_left(s11, 42);
		s10 += s0;	s1	^= s10;	s0  = rotate_left(s0,  54);
	}

	// Two iterations was almost good enough for a 64-bit result, but a 128-bit result is reported, so End() does three iterations.
	void End(const uint64 *data) {
		s0 += data[0];   s1 += data[1];   s2 += data[2];   s3 += data[3];
		s4 += data[4];   s5 += data[5];   s6 += data[6];   s7 += data[7];
		s8 += data[8];   s9 += data[9];   s10 += data[10]; s11 += data[11];
		EndPartial();
		EndPartial();
		EndPartial();
	}
};


// spooky128: hash a single message in one call, produce 128-bit output
void spooky128(const void *message, size_t length, uint64 *hash1, uint64 *hash2) {
	if (length < 12 * 8 * 2) {
		spooky_short(message, length, hash1, hash2);
		return;
	}

	uint64 buf[12];

	SpookyState	state(*hash1, *hash2, spooky_const);
	const uint64 *p		= (const uint64*)message;
	const uint64 *end	= p + length / (12 * 8) * 12;

	// handle all whole BlockSize blocks of bytes
	if (ALLOW_UNALIGNED_READS || ((uintptr_t(p) & 7) == 0)) {
		while (p < end) {
			state.Mix(p);
			p += 12;
		}
	} else {
		while (p < end) {
			memcpy(buf, p, sizeof(buf));
			state.Mix(buf);
			p += 12;
		}
	}

	// handle the last partial block of BlockSize bytes
	size_t rem = length - ((const uint8*)end - (const uint8*)message);
	memcpy(buf, end, rem);
	memset((uint8*)buf + rem, 0, sizeof(buf) - rem);
	((uint8*)buf)[sizeof(buf) - 1] = rem;

	// do some final mixing
	state.End(buf);
	*hash1 = state.s0;
	*hash2 = state.s1;
}

// spooky64: hash a single message in one call, return 64-bit output
inline uint64 spooky64(const void *message, size_t length, uint64 seed) {
	uint64 hash1 = seed;
	spooky128(message, length, &hash1, &seed);
	return hash1;
}

// spooky32: hash a single message in one call, produce 32-bit output
inline uint32 spooky32(const void *message, size_t length, uint32 seed) {
	uint64 hash1 = seed, hash2 = seed;
	spooky128(message, length, &hash1, &hash2);
	return (uint32)hash1;
}

class SpookyHash {
	static const size_t NumVars		= 12;				// number of uint64's in internal state
	static const size_t BlockSize	= NumVars * 8;		// size of the internal state

	uint64		data[2 * NumVars];	// unhashed data, for partial messages
	SpookyState	state;				// internal state of the hash
	size_t		total;				// total length of the input so far
	uint8		remainder;			// length of unhashed data stashed in data

public:
	SpookyHash(uint64 seed1, uint64 seed2) : state(seed1, seed2, spooky_const), total(0), remainder(0) {}

	void Update(const void *message, size_t length) {
		size_t newLength = length + remainder;

		// If this message fragment is too short, stuff it away
		if (newLength < sizeof(data)) {
			memcpy((uint8*)data + remainder, message, length);
			total		+= length;
			remainder	= (uint8)newLength;
			return;
		}

		total += length;

		// if we've got anything stuffed away, use it now
		if (remainder) {
			uint8 prefix = sizeof(data) - remainder;
			memcpy((uint8*)data + remainder, message, prefix);
			state.Mix(data);
			state.Mix(data + NumVars);
			length	-= prefix;
			message	= ((const uint8*)message) + prefix;
		}

		// handle all whole blocks of BlockSize bytes
		const uint64	*p		= (const uint64*)message;
		const uint64	*end	= p + (length / BlockSize) * NumVars;

		if (ALLOW_UNALIGNED_READS || (uintptr_t(p) & 0x7) == 0) {
			while (p < end) {
				state.Mix(p);
				p += NumVars;
			}
		} else {
			while (p < end) {
				memcpy(data, p, BlockSize);
				state.Mix(data);
				p += NumVars;
			}
		}

		// stuff away the last few bytes
		remainder = length - ((const uint8*)end - (const uint8*)message);
		memcpy(data, end, remainder);
	}

	void Final(uint64 *hash1, uint64 *hash2) const {
		// init the variables
		if (total < sizeof(data)) {
			*hash1 = state.s0;
			*hash2 = state.s1;
			spooky_short(data, total, hash1, hash2);
			return;
		}

		const uint64	*p		= data;
		uint8			rem		= remainder;
		auto			state2	= state;
		if (rem >= BlockSize) {
			// data can contain two blocks; handle any whole first block
			state2.Mix(p);
			p		+= NumVars;
			rem		-= BlockSize;
		}

		// mix in the last partial block, and the length mod BlockSize
		memset((uint8*)p + rem, 0, BlockSize - rem);
		((uint8*)p)[BlockSize - 1] = rem;

		// do some final mixing
		state2.End(p);

		*hash1 = state2.s0;
		*hash2 = state2.s1;
	}
};

} // namespace iso

#endif //SPOOKY_H

