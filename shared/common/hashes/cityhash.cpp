#include "cityhash.h"

#ifdef __SSE4_2__
#include <nmmintrin.h>
#endif


// Copyright (c) 2011 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// CityHash, by Geoff Pike and Jyrki Alakuijala
//
// This file provides CityHash64() and related functions.
//
// It's probably possible to create even faster hash functions by
// writing a program that systematically explores some of the space of
// possible hash functions, by using SIMD instructions, or by
// compromising on hash quality.

namespace iso {
namespace CityHash {

//-----------------------------------------------------------------------------
// 32 bit
//-----------------------------------------------------------------------------

// from murmur3:
static const uint32 c1 = 0xcc9e2d51;
static const uint32 c2 = 0x1b873593;

static uint32 fmix(uint32 h) {
	h = (h ^ (h >> 16)) * 0x85ebca6b;
	h = (h ^ (h >> 13)) * 0xc2b2ae35;
	return h ^ (h >> 16);
}

static uint32 mur(uint32 a, uint32 h) {
	return rotate_right(h ^ rotate_right(a * c1, 17) * c2, 19) * 5 + 0xe6546b64;
}

uint32 CityHash32(const char* s, uint32 len) {
	if (len > 24) {
		uint32 h = len, g = c1 * len, f = g;
		uint32 a0 = rotate_right(load_packed<uint32le>(s + len - 4) * c1, 17) * c2;
		uint32 a1 = rotate_right(load_packed<uint32le>(s + len - 8) * c1, 17) * c2;
		uint32 a2 = rotate_right(load_packed<uint32le>(s + len - 16) * c1, 17) * c2;
		uint32 a3 = rotate_right(load_packed<uint32le>(s + len - 12) * c1, 17) * c2;
		uint32 a4 = rotate_right(load_packed<uint32le>(s + len - 20) * c1, 17) * c2;
		h	= rotate_right(h ^ a0, 19) * 5 + 0xe6546b64;
		h	= rotate_right(h ^ a2, 19) * 5 + 0xe6546b64;
		g	= rotate_right(g ^ a1, 19) * 5 + 0xe6546b64;
		g	= rotate_right(g ^ a3, 19) * 5 + 0xe6546b64;
		f	= rotate_right(f + a4, 19) * 5 + 0xe6546b64;

		uint32 iters = (len - 1) / 20;
		do {
			uint32 _a0 = rotate_right(load_packed<uint32le>(s) * c1, 17) * c2;
			uint32 _a1 = load_packed<uint32le>(s + 4);
			uint32 _a2 = rotate_right(load_packed<uint32le>(s + 8) * c1, 17) * c2;
			uint32 _a3 = rotate_right(load_packed<uint32le>(s + 12) * c1, 17) * c2;
			uint32 _a4 = load_packed<uint32le>(s + 16);
			h	= rotate_right(h ^ _a0, 18) * 5 + 0xe6546b64;
			f	= rotate_right(f + _a1, 19) * c1 + _a0;
			g	= rotate_right(g + _a2, 18) * 5 + 0xe6546b64;
			h	= rotate_right(h ^ (_a3 + _a1), 19) * 5 + 0xe6546b64;
			g	= swap_endian(g ^ _a4) * 5;
			h	= swap_endian(h	+ _a4 * 5);

			swap(f, h);
			swap(f, g);

			s += 20;
		} while (--iters != 0);

		g = rotate_right(rotate_right(g, 11) * c1, 17) * c1;
		f = rotate_right(rotate_right(f, 11) * c1, 17) * c1;
		h = rotate_right(rotate_right(h + g, 19) * 5 + 0xe6546b64, 17) * c1;
		return rotate_right(rotate_right(h + f, 19) * 5 + 0xe6546b64, 17) * c1;
	}

	if (len > 12) {
		uint32 a = load_packed<uint32le>(s - 4 + (len >> 1));
		uint32 b = load_packed<uint32le>(s + 4);
		uint32 c = load_packed<uint32le>(s + len - 8);
		uint32 d = load_packed<uint32le>(s + (len >> 1));
		uint32 e = load_packed<uint32le>(s);
		uint32 f = load_packed<uint32le>(s + len - 4);
		uint32 h = len;
		return fmix(mur(f, mur(e, mur(d, mur(c, mur(b, mur(a, h)))))));
	}

	if (len > 4) {
		uint32 a = len, b = len * 5, c = 9, d = b;
		a += load_packed<uint32le>(s);
		b += load_packed<uint32le>(s + len - 4);
		c += load_packed<uint32le>(s + ((len >> 1) & 4));
		return fmix(mur(c, mur(b, mur(a, d))));
	}

	uint32 b = 0;
	uint32 c = 9;
	for (uint32 i = 0; i < len; i++) {
		b	= b * c1 + (int8)s[i];
		c	^= b;
	}
	return fmix(mur(b, mur(len, c)));
}

//-----------------------------------------------------------------------------
// 64 bit
//-----------------------------------------------------------------------------

// Some primes between 2^63 and 2^64 for various uses.
static const uint64 k0 = 0xc3a5c85c97cb3127ULL;
static const uint64 k1 = 0xb492b66fbe98f273ULL;
static const uint64 k2 = 0x9ae16a3b2f90404fULL;

// Quick and dirty 16-byte hash for 48 bytes
static double_int<uint64> WeakHashLen32WithSeeds(uint64 w, uint64 x, uint64 y, uint64 z, uint64 a, uint64 b) {
	a	+= w;
	uint64 c = a + x + y;
	b	= rotate_right(b + a + z, 21) + rotate_right(c, 44);
	return {(c + z), (b + a)};
}

// Quick and dirty 16-byte hash for s[0] ... s[31], a, and b
static double_int<uint64> WeakHashLen32WithSeeds(const char* s, uint64 a, uint64 b) {
	return WeakHashLen32WithSeeds(load_packed<uint64le>(s), load_packed<uint64le>(s + 8), load_packed<uint64le>(s + 16), load_packed<uint64le>(s + 24), a, b);
}

uint64 HashLen0to16(const char *s, uint32 len) {
	if (len >= 8) {
		uint64 mul = k2 + len * 2;
		uint64 a   = load_packed<uint64le>(s) + k2;
		uint64 b   = load_packed<uint64le>(s + len - 8);
		uint64 c   = rotate_right(b, 37) * mul + a;
		uint64 d   = (rotate_right(a, 25) + b) * mul;
		return HashLen16(c, d, mul);
	}

	if (len >= 4) {
		uint64 mul = k2 + len * 2;
		uint64 a   = load_packed<uint32le>(s);
		return HashLen16(len + (a << 3), load_packed<uint32le>(s + len - 4), mul);
	}

	if (len > 0) {
		uint8  a = s[0];
		uint8  b = s[len >> 1];
		uint8  c = s[len - 1];
		uint32 y = static_cast<uint32>(a) + (static_cast<uint32>(b) << 8);
		uint32 z = len + (static_cast<uint32>(c) << 2);
		return shift_mix(y * k2 ^ z * k0) * k2;
	}
	return k2;
}

uint64 CityHash64(const char* s, uint32 len) {
	if (len > 64) {
		// For strings over 64 bytes we hash the end first, and then as we loop we keep 56 bytes of state: v, w, x, y, and z
		uint64	x	= load_packed<uint64le>(s + len - 40);
		uint64	y	= load_packed<uint64le>(s + len - 16) + load_packed<uint64le>(s + len - 56);
		uint64	z	= HashLen16(load_packed<uint64le>(s + len - 48) + len, load_packed<uint64le>(s + len - 24));
		auto	v	= WeakHashLen32WithSeeds(s + len - 64, len, z);
		auto	w	= WeakHashLen32WithSeeds(s + len - 32, y + k1, x);
		x			= x * k1 + load_packed<uint64le>(s);

		// Decrease len to the nearest multiple of 64, and operate on 64-byte chunks
		len = (len - 1) & ~63;
		do {
			x	= rotate_right(x + y + v.lo + load_packed<uint64le>(s + 8), 37) * k1;
			y	= rotate_right(y + v.hi + load_packed<uint64le>(s + 48), 42) * k1;
			x	^= w.hi;
			y	+= v.lo + load_packed<uint64le>(s + 40);
			z	= rotate_right(z + w.lo, 33) * k1;
			v	= WeakHashLen32WithSeeds(s, v.hi * k1, x + w.lo);
			w	= WeakHashLen32WithSeeds(s + 32, z + w.hi, y + load_packed<uint64le>(s + 16));
			swap(z, x);
			s	+= 64;
			len	-= 64;
		} while (len != 0);
		return HashLen16(HashLen16(v.lo, w.lo) + shift_mix(y) * k1 + z, HashLen16(v.hi, w.hi) + x);
	}

	if (len > 32) {
		uint64 mul	= k2 + len * 2;
		uint64 a	= load_packed<uint64le>(s) * k2;
		uint64 b	= load_packed<uint64le>(s + 8);
		uint64 c	= load_packed<uint64le>(s + len - 24);
		uint64 d	= load_packed<uint64le>(s + len - 32);
		uint64 e	= load_packed<uint64le>(s + 16) * k2;
		uint64 f	= load_packed<uint64le>(s + 24) * 9;
		uint64 g	= load_packed<uint64le>(s + len - 8);
		uint64 h	= load_packed<uint64le>(s + len - 16) * mul;
		uint64 u	= rotate_right(a + g, 43) + (rotate_right(b, 30) + c) * 9;
		uint64 v	= ((a + g) ^ d) + f + 1;
		uint64 w	= swap_endian((u + v) * mul) + h;
		uint64 x	= rotate_right(e + f, 42) + c;
		uint64 y	= (swap_endian((v + w) * mul) + g) * mul;
		uint64 z	= e + f + c;
		a			= swap_endian((x + z) * mul + y) + b;
		b			= shift_mix((z + a) * mul + d + h) * mul;
		return b + x;
	}

	if (len > 16) {
		// This probably works well for 16-byte strings as well, but it may be overkill in that case
		uint64 mul = k2 + len * 2;
		uint64 a   = load_packed<uint64le>(s) * k1;
		uint64 b   = load_packed<uint64le>(s + 8);
		uint64 c   = load_packed<uint64le>(s + len - 8) * mul;
		uint64 d   = load_packed<uint64le>(s + len - 16) * k2;
		return HashLen16(rotate_right(a + b, 43) + rotate_right(c, 30) + d, a + rotate_right(b + k2, 18) + c, mul);
	}
	return HashLen0to16(s, len);
}

//-----------------------------------------------------------------------------
// 128 bit
//-----------------------------------------------------------------------------

double_int<uint64> CityHash128WithSeed(const char* s, size_t len, uint64 seedlo, uint64 seedhi) {
	if (len >= 128) {
		// keep 56 bytes of state: v, w, x, y, and z
		double_int<uint64> v, w;
		uint64	x = seedlo;
		uint64	y = seedhi;
		uint64	z = len * k1;
		v.lo	= rotate_right(y ^ k1, 49) * k1 + load_packed<uint64le>(s);
		v.hi	= rotate_right(v.lo, 42) * k1 + load_packed<uint64le>(s + 8);
		w.lo	= rotate_right(y + z, 35) * k1 + x;
		w.hi	= rotate_right(x + load_packed<uint64le>(s + 88), 53) * k1;

		// This is the same inner loop as CityHash64(), manually unrolled
		do {
			x = rotate_right(x + y + v.lo + load_packed<uint64le>(s + 8), 37) * k1;
			y = rotate_right(y + v.hi + load_packed<uint64le>(s + 48), 42) * k1;
			x ^= w.hi;
			y += v.lo + load_packed<uint64le>(s + 40);
			z = rotate_right(z + w.lo, 33) * k1;
			v = WeakHashLen32WithSeeds(s, v.hi * k1, x + w.lo);
			w = WeakHashLen32WithSeeds(s + 32, z + w.hi, y + load_packed<uint64le>(s + 16));
			swap(z, x);
			s += 64;
			x = rotate_right(x + y + v.lo + load_packed<uint64le>(s + 8), 37) * k1;
			y = rotate_right(y + v.hi + load_packed<uint64le>(s + 48), 42) * k1;
			x ^= w.hi;
			y += v.lo + load_packed<uint64le>(s + 40);
			z = rotate_right(z + w.lo, 33) * k1;
			v = WeakHashLen32WithSeeds(s, v.hi * k1, x + w.lo);
			w = WeakHashLen32WithSeeds(s + 32, z + w.hi, y + load_packed<uint64le>(s + 16));
			swap(z, x);
			s	+= 64;
			len -= 128;
		} while (likely(len >= 128));

		x += rotate_right(v.lo + z, 49) * k0;
		y = y * k0 + rotate_right(w.hi, 37);
		z = z * k0 + rotate_right(w.lo, 27);
		w.lo *= 9;
		v.lo *= k0;
		// If 0 < len < 128, hash up to 4 chunks of 32 bytes each from the end of s
		for (size_t tail_done = 0; tail_done < len;) {
			tail_done += 32;
			y = rotate_right(x + y, 42) * k0 + v.hi;
			w.lo += load_packed<uint64le>(s + len - tail_done + 16);
			x = x * k0 + w.lo;
			z += w.hi + load_packed<uint64le>(s + len - tail_done);
			w.hi += v.lo;
			v = WeakHashLen32WithSeeds(s + len - tail_done, v.lo + z, v.hi);
			v.lo *= k0;
		}
		// At this point our 56 bytes of state should contain more than enough information for a strong 128-bit hash
		// We use two different 56-byte-to-8-byte hashes to get a 16-byte final result
		x = HashLen16(x, v.lo);
		y = HashLen16(y + z, w.lo);
		return double_int<uint64>(HashLen16(x + v.hi, w.hi) + y, HashLen16(x + w.hi, y + v.hi));
	}

	// return a decent 128-bit hash for strings of any length representable in signed long
	uint64	a	= seedlo;
	uint64	b	= seedhi;
	uint64	c	= 0;
	uint64	d	= 0;
	int		l	= len - 16;

	if (l <= 0) {  // len <= 16
		a	= shift_mix(a * k1) * k1;
		c	= b * k1 + HashLen0to16(s, len);
		d	= shift_mix(a + (len >= 8 ? load_packed<uint64le>(s) : c));
	} else {  // len > 16
		c	= HashLen16(load_packed<uint64le>(s + len - 8) + k1, a);
		d	= HashLen16(b + len, c + load_packed<uint64le>(s + len - 16));
		a	+= d;
		do {
			a	= (a ^ shift_mix(load_packed<uint64le>(s) * k1) * k1) * k1;
			b	^= a;
			c	= (c ^ shift_mix(load_packed<uint64le>(s + 8) * k1) * k1) * k1;
			d	^= c;
			s	+= 16;
			l	-= 16;
		} while (l > 0);
	}
	a = HashLen16(a, c);
	b = HashLen16(d, b);
	return {a ^ b, HashLen16(b, a)};

}

double_int<uint64> CityHash128(const char* s, size_t len) {
	return len >= 16
		? CityHash128WithSeed(s + 16, len - 16, load_packed<uint64le>(s), load_packed<uint64le>(s + 8) + k0)
		: CityHash128WithSeed(s, len, k0, k1);
}

//-----------------------------------------------------------------------------
// 256 bit
//-----------------------------------------------------------------------------

#ifdef __SSE4_2__

#undef CHUNK
#define CHUNK(r)						\
	permute3(x, z, y);					\
	b += load_packed<uint64le>(s);		\
	c += load_packed<uint64le>(s + 8);	\
	d += load_packed<uint64le>(s + 16);	\
	e += load_packed<uint64le>(s + 24);	\
	f += load_packed<uint64le>(s + 32);	\
	a += b;								\
	h += f;								\
	b += c;								\
	f += d;								\
	g += e;								\
	e += z;								\
	g += x;								\
	z = _mm_crc32_u64(z, b + g);		\
	y = _mm_crc32_u64(y, e + h);		\
	x = _mm_crc32_u64(x, f + a);		\
	e = rotate_right(e, r);				\
	c += e;								\
	s += 40


	// Requires len >= 240.
static void CityHashCrc256Long(const char* s, size_t len, uint32 seed, uint64* result) {
	uint64 a	= load_packed<uint64le>(s + 56) + k0;
	uint64 b	= load_packed<uint64le>(s + 96) + k0;
	uint64 c	= result[0] = HashLen16(b, len);
	uint64 d	= result[1] = load_packed<uint64le>(s + 120) * k0 + len;
	uint64 e	= load_packed<uint64le>(s + 184) + seed;
	uint64 f	= 0;
	uint64 g	= 0;
	uint64 h	= c + d;
	uint64 x	= seed;
	uint64 y	= 0;
	uint64 z	= 0;

	// 240 bytes of input per iter.
	size_t iters = len / 240;
	len -= iters * 240;
	do {
		CHUNK(0);
		permute3(a, h, c);
		CHUNK(33);
		permute3(a, h, f);
		CHUNK(0);
		permute3(b, h, f);
		CHUNK(42);
		permute3(b, h, d);
		CHUNK(0);
		permute3(b, h, e);
		CHUNK(33);
		permute3(a, h, e);
	} while (--iters > 0);

	while (len >= 40) {
		CHUNK(29);
		e ^= rotate_right(a, 20);
		h += rotate_right(b, 30);
		g ^= rotate_right(c, 40);
		f += rotate_right(d, 34);
		permute3(c, h, g);
		len -= 40;
	}
	if (len > 0) {
		s = s + len - 40;
		CHUNK(33);
		e ^= rotate_right(a, 43);
		h += rotate_right(b, 42);
		g ^= rotate_right(c, 41);
		f += rotate_right(d, 40);
	}
	result[0] ^= h;
	result[1] ^= g;

	g += h;
	a = HashLen16(a, g + z);
	x += y << 32;
	b += x;
	c = HashLen16(c, z) + h;
	d = HashLen16(d, e + result[0]);
	g += e;
	h += HashLen16(x, f);
	e = HashLen16(a, d) + g;
	z = HashLen16(b, c) + a;
	y = HashLen16(g, h) + c;

	result[0] = e + z + y + x;
	a		  = shift_mix((a + y) * k0) * k0 + b;
	result[1] += a + result[0];
	a		  = shift_mix(a * k0) * k0 + c;
	result[2] = a + result[1];
	a		  = shift_mix((a + e) * k0) * k0;
	result[3] = a + result[2];
}

void CityHashCrc256(const char* s, size_t len, uint64* result) {
	if (likely(len >= 240)) {
		CityHashCrc256Long(s, len, 0, result);
	} else {
		char buf[240];
		memcpy(buf, s, len);
		memset(buf + len, 0, 240 - len);
		CityHashCrc256Long(buf, 240, ~static_cast<uint32>(len), result);
	}
}

double_int<uint64> CityHashCrc128WithSeed(const char* s, size_t len, uint64 seedlo, uint64 seedhi) {
	if (len <= 900)
		return CityHash128WithSeed(s, len, seedlo, seedhi);

	uint64 result[4];
	CityHashCrc256(s, len, result);
	uint64 u = seedhi + result[0];
	uint64 v = seedlo + result[1];
	return double_int<uint64>(HashLen16(u, v + result[2]), HashLen16(rotate_right(v, 32), u * k0 + result[3]));
}

double_int<uint64> CityHashCrc128(const char* s, size_t len) {
	if (len <= 900)
		return CityHash128(s, len);

	uint64 result[4];
	CityHashCrc256(s, len, result);
	return {result[2], result[3]};
}

#endif

}  // namespace CityHash
}  // namespace iso
