#include "chacha.h"

using namespace iso;

template<int A, int B, int C, int D> inline void ChaCha20::quarter_round(uint32 x[16]) {
	uint32	a	= x[A];
	uint32	b	= x[B];
	uint32	c	= x[C];
	uint32	d	= x[D];

	a += b;
	d = rotate_left(d ^ a, 16);
	c += d;
	b = rotate_left(b ^ c, 12);

	a += b;
	d = rotate_left(d ^ a, 8);
	c += d;
	b = rotate_left(b ^ c, 7);

	x[A]	= a;
	x[B]	= b;
	x[C]	= c;
	x[D]	= d;
}

void ChaCha20::next(void *data) {//64 bytes
	uint32	x[16];
	memcpy(x, s, sizeof(s));

	// 10 * 8 quarter rounds = 20 rounds
	for(int i = 0; i < 10; ++i) {
		// Column quarter rounds
		quarter_round<0, 4,  8, 12>(x);
		quarter_round<1, 5,  9, 13>(x);
		quarter_round<2, 6, 10, 14>(x);
		quarter_round<3, 7, 11, 15>(x);
		// Diagonal quarter rounds
		quarter_round<0, 5, 10, 15>(x);
		quarter_round<1, 6, 11, 12>(x);
		quarter_round<2, 7,  8, 13>(x);
		quarter_round<3, 4,  9, 14>(x);
	}

	for(int i = 0; i < 16; ++i)
		x[i] += s[i];

	memcpy(data, x, sizeof(x));

	if (++s[12] == 0)
		++s[13];
}

template<int A, int B, int C, int D> inline void Salsa20::quarter_round(uint32 x[16]) {
	uint32	a	= x[A];
	uint32	b	= x[B];
	uint32	c	= x[C];
	uint32	d	= x[D];

	b ^= rotate_left(a + d,  7);
	c ^= rotate_left(b + a,  9);
	d ^= rotate_left(c + b, 13);
	a ^= rotate_left(d + c, 18);

	x[A]	= a;
	x[B]	= b;
	x[C]	= c;
	x[D]	= d;
}

void Salsa20::next(void *data) {
	uint32	x[16];
	memcpy(x, s, sizeof(s));

	// 10 * 8 quarter rounds = 20 rounds
	for(int i = 0; i < 10; ++i) {
		quarter_round< 0,  4,  8, 12>(x);
		quarter_round< 5,  9, 13,  1>(x);
		quarter_round<10, 14,  2,  6>(x);
		quarter_round<15,  3,  7, 11>(x);
		quarter_round< 0,  1,  2,  3>(x);
		quarter_round< 5,  6,  7,  4>(x);
		quarter_round<10, 11,  8,  9>(x);
		quarter_round<15, 12, 13, 14>(x);
	}

	for(int i = 0; i < 16; ++i)
		x[i] += s[i];

	memcpy(data, x, sizeof(x));

	if (++s[8] == 0)
		++s[9];
}

# define CONSTANT_TIME_CARRY(a,b) ((a ^ ((a ^ b) | ((a - b) ^ b))) >> (sizeof(a) * 8 - 1))

void poly1305::process(const uint8 *input, size_t len, uint32 padbit) {
	uint64	r0 = r[0];
	uint64	r1 = r[1];
	uint64	s1 = r1 + (r1 >> 2);

	uint64	h0 = h[0];
	uint64	h1 = h[1];
	uint64	h2 = h[2];

	bool	c;
	while (len >= 16) {
		// h += m[i]

//		h0 = (uint64)(d0 = (uint128)h0 + ((uint64le*)input)[0]);
		c	= addc(h0, ((uint64le*)input)[0], false, h0);

//		h1 = (uint64)(d1 = (uint128)h1 + (d0 >> 64) + ((uint64le*)input)[1]);
		c	= addc(h1, ((uint64le*)input)[1], c, h1);

		// padbit can be zero only when original len was POLY1306_BLOCK_SIZE, but we don't check
		h2	+= uint64(c) + padbit;

		// h *= r "%" p, where "%" stands for "partial remainder"
//		d0 = ((uint128)h0 * r0) + ((uint128)h1 * s1);
		uint64	d0, d1;
		uint64	t0, t1;
		t0	= mulc(h0, r0, h0);
		t1	= mulc(h1, s1, h1);
		c	= addc(h0, h1, false, d0);
//		d1 = ((uint128)h0 * r1) + ((uint128)h1 * r0) + (h2 * s1);

		t0	= maddc(h0, r1, t0, h0);
		t1	= maddc(h1, r0, t1, h1);
		c	= addc(h0, h1, c, d1);
		c	|= addc(d1, h2 * s1, false, d1);		// **CHECK**
		h2	= h2 * r0 + uint64(c);

		// last reduction step:
		// a) h2:h0 = h2<<128 + d1<<64 + d0
		h0	= d0;
		h1	= d1;

		// b) (h2:h0 += (h2:h0>>130) * 5) %= 2^130
		uint64	t = (h2 >> 2) + (h2 & ~3);
		c = addc(h0, t, false, h0);
		c = addc(h1, uint64(0), c, h1);
		h2 = (h2 & 3) + uint64(c);
//		h1 += (t = CONSTANT_TIME_CARRY(h0, t));
//		h2 += CONSTANT_TIME_CARRY(h1, t);
		// Occasional overflows to 3rd bit of h2 are taken care of "naturally"
		// If after this point we end up at the top of this loop, then the overflow bit will be accounted for in next iteration
		// If we end up in poly1305_emit, then comparison to modulus below will still count as "carry into 131st bit", so that properly reduced value will be picked in conditional move
		input += 16;
		len -= 16;
	}

	h[0] = h0;
	h[1] = h1;
	h[2] = h2;
}

void poly1305::emit(uint8 mac[16], const uint32 nonce[4]) {
	uint64	h0 = h[0];
	uint64	h1 = h[1];
	uint64	h2 = h[2];

	// compare to modulus by computing h + -p
//	uint64	g0 = (uint64)(t = (uint128)h0 + 5);
//	uint64	g1 = (uint64)(t = (uint128)h1 + (t >> 64));
//	uint64	g2 = h2 + (uint64)(t >> 64);
	uint64	g0, g1, g2;
	bool	c;
	c	= addc(h0, uint64(5), false, g0);
	c	= addc(h1, uint64(0), c, g1);
	g2	= h2 + uint64(c);

	// if there was carry into 131st bit, h1:h0 = g1:g0
	uint64	mask = 0 - (g2 >> 2);
	h0	= (h0 & ~mask) | (g0 & mask);
	h1	= (h1 & ~mask) | (g1 & mask);

	// mac = (h + nonce) % (2^128)
//	h0 = (uint64)(t = (uint128)h0 + nonce[0] + ((uint64)nonce[1] << 32));
//	h1 = (uint64)(t = (uint128)h1 + nonce[2] + ((uint64)nonce[3] << 32) + (t >> 64));
	c	= addc(h0, nonce[0] + ((uint64)nonce[1] << 32), false, h0);
	h1	+= nonce[2] + ((uint64)nonce[3] << 32) + uint64(c);

	((uint64le*)mac)[0] = h0;
	((uint64le*)mac)[1] = h1;
}

Poly1305::CODE Poly1305::digest() {
	CODE	code;
	if (uint32 n = p % sizeof(block)) {
		block[n++] = 1;   // pad bit
		while (n < 16)
			block[n++] = 0;
		process(block, sizeof(block), 0);
	}

	emit(code.x, nonce);
	clear(*this);
	return code;
}
