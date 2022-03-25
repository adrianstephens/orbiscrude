/*
BLAKE2 reference source code package - reference C implementations

Written in 2012 by Samuel Neves <sneves@dei.uc.pt>

To the extent possible under law, the author(s) have dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication along with
this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "codec.h"

using namespace iso;

template<int BITS> struct Blake2 {
	typedef uint_bits_t<BITS>	U;
	enum {
		ALIGNMENT	= 64,
		BLOCKBYTES	= BITS * 2,
		OUTBYTES	= BITS,
	};
	static const U		IV[8];
	static const uint8	sigma[][16];

	static void round(U m0, U m1, U &a, U &b, U &c, U &d) {
		a = a + b + m0;
		d = rotate_right(d ^ a, BITS * 4 / 8);
		c = c + d;
		b = rotate_right(b ^ c, BITS * 3 / 8);
		a = a + b + m1;
		d = rotate_right(d ^ a, BITS * 2 / 8);
		c = c + d;
		b = rotate_right(b ^ c, 7);		//63
	}

	littleendian<U> block[16 * 2];
	U				h[8], t[2], f[2];
	size_t			buflen;
	bool			last_node;

	void init_param(uint32 node_offset, uint32 node_depth) {
		clear(block);
		buflen	  = 0;
		last_node = false;

		for (int i = 0; i < 8; ++i)
			h[i] = IV[i];

		h[0] ^= 0x02080020;	 // We use BLAKE2sp parameters block.
		h[2] ^= node_offset;
		h[3] ^= (node_depth << 16) | 0x20000000;
	}

	void increment_counter(const uint32 inc) {
		t[0] += inc;
		t[1] += (t[0] < inc);
	}

	void compress(const littleendian<U> block[16]) {
		U	m[16];
		U	v[16];

		for (size_t i = 0; i < 16; ++i)
			m[i] = block[i];//load_packed<U>(block + i * sizeof(U));

		for (size_t i = 0; i < 8; ++i)
			v[i] = h[i];

		v[8]  = IV[0];
		v[9]  = IV[1];
		v[10] = IV[2];
		v[11] = IV[3];
		v[12] = t[0] ^ IV[4];	//t[0]
		v[13] = t[1] ^ IV[5];	//t[1]
		v[14] = f[0] ^ IV[6];	//t[0]
		v[15] = f[1] ^ IV[7];	//t[1]

		//64->12
		for (auto s : sigma) {
			round(m[s[ 0]], m[s[ 1]], v[0], v[4], v[ 8], v[12]);
			round(m[s[ 2]], m[s[ 3]], v[1], v[5], v[ 9], v[13]);
			round(m[s[ 4]], m[s[ 5]], v[2], v[6], v[10], v[14]);
			round(m[s[ 6]], m[s[ 7]], v[3], v[7], v[11], v[15]);
			round(m[s[ 8]], m[s[ 9]], v[0], v[5], v[10], v[15]);
			round(m[s[10]], m[s[11]], v[1], v[6], v[11], v[12]);
			round(m[s[12]], m[s[13]], v[2], v[7], v[ 8], v[13]);
			round(m[s[14]], m[s[15]], v[3], v[4], v[ 9], v[14]);
		}

		for (size_t i = 0; i < 8; ++i)
			h[i] = h[i] ^ v[i] ^ v[i + 8];
	}

	void update(const uint8* in, size_t inlen) {
		uint8	*buf = (uint8*)block;
		while (inlen > 0) {
			size_t left = buflen;
			size_t fill = 2 * BLOCKBYTES - left;

			if (inlen > fill) {
				memcpy(buf + left, in, fill);  // Fill buffer
				buflen += fill;
				increment_counter(BLOCKBYTES);
				compress(block);

				memcpy(buf, buf + BLOCKBYTES, BLOCKBYTES);	// Shift buffer left
				buflen	-= BLOCKBYTES;
				in		+= fill;
				inlen	-= fill;
			} else {	// inlen <= fill
				memcpy(buf + left, in, (size_t)inlen);
				buflen	+= (size_t)inlen;	 // Be lazy, do not compress
				in		+= inlen;
				inlen	= 0;
			}
		}
	}

	void final(littleendian<U> digest[8]) {
		uint8	*buf = (uint8*)block;

		if (buflen > BLOCKBYTES) {
			increment_counter(BLOCKBYTES);
			compress(block);
			buflen -= BLOCKBYTES;
			memcpy(buf, buf + BLOCKBYTES, buflen);
		}

		increment_counter((uint32)buflen);
		
		if (last_node)
			f[1] = ~0U;
		f[0] = ~0U;

		// Padding
		memset(buf + buflen, 0, 2 * BLOCKBYTES - buflen);
		compress(block);

		// Output full hash
		for (int i = 0; i < 8; ++i)
			digest[i] = h[i];
	}
};

template<>	const uint32 Blake2<32>::IV[8] = {
	0x6A09E667UL, 0xBB67AE85UL,
	0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL,
	0x1F83D9ABUL, 0x5BE0CD19UL
};
template<>	const uint8 Blake2<32>::sigma[10][16] = {
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
	{11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
	{7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
	{9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
	{2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
	{12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
	{13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
	{6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
	{10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
};

template<>	const uint64 Blake2<64>::IV[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};
template<>	const uint8 Blake2<64>::sigma[12][16] = {
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};

template<int BITS> struct Blake2Parallel {
	typedef Blake2<BITS>	B;
	typedef typename B::U	U;
	enum {
		PARALLELISM	= 256 / BITS,
	};

	B		S[PARALLELISM];
	B		R;
	uint8	buf[PARALLELISM * B::BLOCKBYTES];
	size_t	buflen;

	struct ThreadData {
		B* S;
		const uint8*   in;
		size_t		   inlen;
		void Update() {
			const uint8* p	= (const uint8*)in;
			for (size_t  len = inlen; len >= PARALLELISM * B::BLOCKBYTES; len -= PARALLELISM * B::BLOCKBYTES) {
				S->update(p, B::BLOCKBYTES);
				p += PARALLELISM * B::BLOCKBYTES;
			}
		}
	};

	Blake2Parallel() {
		clear(buf);
		buflen = 0;

		R.init_param(0, 1);
		for (int i = 0; i < PARALLELISM; ++i)
			S[i].init_param(i, 0);

		R.last_node						= true;
		S[PARALLELISM - 1].last_node	= true;
	}

	void update(const uint8* in, size_t inlen) {
		size_t left = buflen;
		size_t fill = sizeof(buf) - left;

		if (left && inlen >= fill) {
			memcpy(buf + left, in, fill);

			for (size_t i = 0; i < PARALLELISM; ++i)
				S[i].update(buf + i * B::BLOCKBYTES, B::BLOCKBYTES);

			in		+= fill;
			inlen	-= fill;
			left	= 0;
		}

		ThreadData btd_array[PARALLELISM];

#ifdef RAR_SMP
		uint ThreadNumber = inlen < 0x1000 ? 1 : MaxThreads;
		if (ThreadNumber == 6 || ThreadNumber == 7)	 // 6 and 7 threads work slower than 4 here.
			ThreadNumber = 4;
#else
		int ThreadNumber = 1;
#endif

		for (size_t i = 0; i < PARALLELISM;) {
			for (int Thread = 0; Thread < ThreadNumber && i < PARALLELISM; Thread++) {
				ThreadData* btd = btd_array + Thread;

				btd->inlen = inlen;
				btd->in	   = in + i * B::BLOCKBYTES;
				btd->S	   = &S[i];

#ifdef RAR_SMP
				if (ThreadNumber > 1)
					ThPool->AddTask(Blake2Thread, (void*)btd);
				else
					btd->Update();
#else
				btd->Update();
#endif
				i++;
			}
#ifdef RAR_SMP
			if (ThPool != NULL)	// Can be NULL in -mt1 mode.
				ThPool->WaitDone();
#endif
		}

		in		+= inlen - inlen % (PARALLELISM * B::BLOCKBYTES);
		inlen	%= PARALLELISM * B::BLOCKBYTES;

		if (inlen > 0)
			memcpy(buf + left, in, inlen);

		buflen = left + inlen;
	}

	void final(uint8* digest) {
		littleendian<U> hash[PARALLELISM][8];

		for (size_t i = 0; i < PARALLELISM; ++i) {
			if (S->buflen > i * B::BLOCKBYTES)
				S[i].update(buf + i * B::BLOCKBYTES, min(S->buflen - i * B::BLOCKBYTES, B::BLOCKBYTES));
			S[i].final(hash[i]);
		}

		for (auto h : hash)
			R.update((uint8*)h, B::OUTBYTES);

		R.final((littleendian<U>*)digest);
	}
};

template	struct Blake2<32>;
template	struct Blake2<64>;

template	struct Blake2Parallel<32>;
template	struct Blake2Parallel<64>;


