#include "md5.h"
#include "base/bits.h"

using namespace iso;

template<typename S> S terminate(S state, uint64 count, const uint8 temp[64]) {
	uint8	t2[64];
	uint32	index	= (count >> 3) & 0x3f;

	memcpy(t2, temp, index);

	t2[index++]	= 0x80;
	if (index >= 55) {
		memset(t2 + index, 0, 64 - index);
		state.transform(t2);
		index = 0;
	}

	memset(t2 + index, 0, 56 - index);
	*(uint64le*)(t2 + 56) = count;
	state.transform(t2);
	return state;
}

//-----------------------------------------------------------------------------
//	MD5
//-----------------------------------------------------------------------------

namespace md5 {
template<int N> struct R;
template<> struct R<0> {
	enum {S0 = 7, S1 = 12, S2 = 17, S3 = 22, B0 = 0, B1 = 4, B2 = 8, B3 = 12, OS = 1};
	static const uint32 ac[16];
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return ((y ^ z) & x) ^ z; }
};
const uint32 R<0>::ac[16] = {
	0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
	0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
};
template<> struct R<1> {
	enum {S0 = 5, S1 = 9, S2 = 14, S3 = 20, B0 = 1, B1 = 5, B2 = 9, B3 = 13, OS = 5};
	static const uint32 ac[16];
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return ((x ^ y) & z) ^ y; }
};
const uint32 R<1>::ac[16] = {
	0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
	0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
};
template<> struct R<2> {
	enum {S0 = 4, S1 = 11, S2 = 16, S3 = 23, B0 = 5, B1 = 1, B2 = 13, B3 = 9, OS = 3};
	static const uint32 ac[16];
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return x ^ y ^ z; }
};
const uint32 R<2>::ac[16] = {
	0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
	0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
};
template<> struct R<3> {
	enum {S0 = 6, S1 = 10, S2 = 15, S3 = 21, B0 = 0, B1 = 12, B2 = 8, B3 = 4, OS = 7};
	static const uint32 ac[16];
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return (x | ~z) ^ y; }
};
const uint32 R<3>::ac[16] = {
	0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
	0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391,
};

template<int N> inline uint32 F1(uint32 a, uint32 b, uint32 c, uint32 d, uint32 x, uint32 s, uint32 ac) {
	return rotate_left(a + R<N>::f(b, c, d) + x + ac, s) + b;
}

template<int N, int I, int B> inline void F4(MD45_State &s, const uint32 *x) {
	uint32	a = s.a, b = s.b, c = s.c, d = s.d;
	s.a = a = F1<N>(a, b, c, d, x[(B + R<N>::OS * 0) & 15], R<N>::S0, R<N>::ac[I * 4 + 0]);
	s.d = d = F1<N>(d, a, b, c, x[(B + R<N>::OS * 1) & 15], R<N>::S1, R<N>::ac[I * 4 + 1]);
	s.c = c = F1<N>(c, d, a, b, x[(B + R<N>::OS * 2) & 15], R<N>::S2, R<N>::ac[I * 4 + 2]);
	s.b = b = F1<N>(b, c, d, a, x[(B + R<N>::OS * 3) & 15], R<N>::S3, R<N>::ac[I * 4 + 3]);
}

template<int N> inline void F16(MD45_State &s, const uint32 *x) {
	F4<N, 0, R<N>::B0>(s, x);
	F4<N, 1, R<N>::B1>(s, x);
	F4<N, 2, R<N>::B2>(s, x);
	F4<N, 3, R<N>::B3>(s, x);
}

struct State : MD45_State {
	void transform(const uint8 block[64]) {
		uint32	x[16];
		for (int i = 0; i < 16; i++)
			x[i] = ((uint32le*)block)[i];

		MD45_State	s2 = *this;
		md5::F16<0>(s2, x);
		md5::F16<1>(s2, x);
		md5::F16<2>(s2, x);
		md5::F16<3>(s2, x);

		a += s2.a;
		b += s2.b;
		c += s2.c;
		d += s2.d;
	}
	operator const uint32*() const { return &a; }
};

} // namespace md5

MD::CODE MD5::terminate() const {
	return (const uint32*)::terminate((md5::State&)state, p * 8, block);
}
void MD5::process(const void *data) {
	((md5::State&)state).transform((uint8*)data);
}

//-----------------------------------------------------------------------------
//	MD4
//-----------------------------------------------------------------------------

namespace md4 {
template<int N> struct R;
template<> struct R<0> {
	enum {S0 = 3, S1 = 7, S2 = 11, S3 = 19, I1 = 1, I2 = 2, I3 = 3, B1 = 4, B2 = 8, B3 = 12};
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return ((y ^ z) & z) ^ z; }
};
template<> struct R<1> {
	enum {S0 = 3, S1 = 5, S2 = 9, S3 = 13, I1 = 4, I2 = 8, I3 = 12, B1 = 1, B2 = 2, B3 =  3};
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return (x & (y | z)) | (y & z); }
};
template<> struct R<2> {
	enum {S0 = 3, S1 = 9, S2 = 11, S3 = 15, I1 = 8, I2 = 4, I3 = 12, B1 = 2, B2 = 1, B3 = 3};
	static inline uint32 f(uint32 x, uint32 y, uint32 z) { return x ^ y ^ z; }
};

template<int N> inline uint32 F1(uint32 a, uint32 b, uint32 c, uint32 d, uint32 k, uint32 s, uint32 t) {
	return rotate_left(a + k + t + R<N>::f(b, c, d), s);
}

template<int N, int I> inline void F4(MD45_State &s, const uint32 *x, uint32 t) {
	uint32	a = s.a, b = s.b, c = s.c, d = s.d;
	s.a = a = F1<N>(a, b, c, d, x[I],					R<N>::S0, t);
	s.d = d = F1<N>(d, a, b, c, x[(I + R<N>::I1) & 15],	R<N>::S1, t);
	s.c = c = F1<N>(c, d, a, b, x[(I + R<N>::I2) & 15],	R<N>::S2, t);
	s.b = b = F1<N>(b, c, d, a, x[(I + R<N>::I3) & 15],	R<N>::S3, t);
}

template<int N> inline void F16(MD45_State &s, const uint32 *x, uint32 t) {
	F4<N, 0>		(s, x, t);
	F4<N, R<N>::B1>	(s, x, t);
	F4<N, R<N>::B2>	(s, x, t);
	F4<N, R<N>::B3>	(s, x, t);
}

struct State : MD45_State {
	void transform(const uint8 block[64]) {
		uint32	x[16];

		for (int i = 0; i < 16; i++)
			x[i] = ((uint32le*)block)[i];

		MD45_State	s2 = *this;
		md4::F16<0>(s2, x, 0);
		md4::F16<1>(s2, x, 0x5A827999);
		md4::F16<2>(s2, x, 0x6ED9EBA1);

		a += s2.a;
		b += s2.b;
		c += s2.c;
		d += s2.d;
	}
	operator const uint32*() const { return &a; }
};

} // namespace md4

MD::CODE MD4::terminate() const {
	return (const uint32*)::terminate((md4::State&)state, p * 8, block);
}
void MD4::process(const void *data) {
	((md4::State&)state).transform((uint8*)data);
}

//-----------------------------------------------------------------------------
//	MD2
//-----------------------------------------------------------------------------

void MD2::State::transform(const uint8 block[16]) {
	static const uint8 table[256] = {
		0x29, 0x2E, 0x43, 0xC9, 0xA2, 0xD8, 0x7C, 0x01,
		0x3D, 0x36, 0x54, 0xA1, 0xEC, 0xF0, 0x06, 0x13,
		0x62, 0xA7, 0x05, 0xF3, 0xC0, 0xC7, 0x73, 0x8C,
		0x98, 0x93, 0x2B, 0xD9, 0xBC, 0x4C, 0x82, 0xCA,
		0x1E, 0x9B, 0x57, 0x3C, 0xFD, 0xD4, 0xE0, 0x16,
		0x67, 0x42, 0x6F, 0x18, 0x8A, 0x17, 0xE5, 0x12,
		0xBE, 0x4E, 0xC4, 0xD6, 0xDA, 0x9E, 0xDE, 0x49,
		0xA0, 0xFB, 0xF5, 0x8E, 0xBB, 0x2F, 0xEE, 0x7A,
		0xA9, 0x68, 0x79, 0x91, 0x15, 0xB2, 0x07, 0x3F,
		0x94, 0xC2, 0x10, 0x89, 0x0B, 0x22, 0x5F, 0x21,
		0x80, 0x7F, 0x5D, 0x9A, 0x5A, 0x90, 0x32, 0x27,
		0x35, 0x3E, 0xCC, 0xE7, 0xBF, 0xF7, 0x97, 0x03,
		0xFF, 0x19, 0x30, 0xB3, 0x48, 0xA5, 0xB5, 0xD1,
		0xD7, 0x5E, 0x92, 0x2A, 0xAC, 0x56, 0xAA, 0xC6,
		0x4F, 0xB8, 0x38, 0xD2, 0x96, 0xA4, 0x7D, 0xB6,
		0x76, 0xFC, 0x6B, 0xE2, 0x9C, 0x74, 0x04, 0xF1,
		0x45, 0x9D, 0x70, 0x59, 0x64, 0x71, 0x87, 0x20,
		0x86, 0x5B, 0xCF, 0x65, 0xE6, 0x2D, 0xA8, 0x02,
		0x1B, 0x60, 0x25, 0xAD, 0xAE, 0xB0, 0xB9, 0xF6,
		0x1C, 0x46, 0x61, 0x69, 0x34, 0x40, 0x7E, 0x0F,
		0x55, 0x47, 0xA3, 0x23, 0xDD, 0x51, 0xAF, 0x3A,
		0xC3, 0x5C, 0xF9, 0xCE, 0xBA, 0xC5, 0xEA, 0x26,
		0x2C, 0x53, 0x0D, 0x6E, 0x85, 0x28, 0x84, 0x09,
		0xD3, 0xDF, 0xCD, 0xF4, 0x41, 0x81, 0x4D, 0x52,
		0x6A, 0xDC, 0x37, 0xC8, 0x6C, 0xC1, 0xAB, 0xFA,
		0x24, 0xE1, 0x7B, 0x08, 0x0C, 0xBD, 0xB1, 0x4A,
		0x78, 0x88, 0x95, 0x8B, 0xE3, 0x63, 0xE8, 0x6D,
		0xE9, 0xCB, 0xD5, 0xFE, 0x3B, 0x00, 0x1D, 0x39,
		0xF2, 0xEF, 0xB7, 0x0E, 0x66, 0x58, 0xD0, 0xE4,
		0xA6, 0x77, 0x72, 0xF8, 0xEB, 0x75, 0x4B, 0x0A,
		0x31, 0x44, 0x50, 0xB4, 0x8F, 0xED, 0x1F, 0x1A,
		0xDB, 0x99, 0x8D, 0x33, 0x9F, 0x11, 0x83, 0x14,
	};
	uint8	temp[48];

	for (uint8 i = 0, j = checksum[15]; i < 16; i++) {
		uint8		t	= block[i];
		temp[i]			= state[i];
		temp[i + 16]	= t;
		temp[i + 32]	= t ^ state[i];
		j = checksum[i] ^= table[t ^ j];
	}
	for (uint8 i = 0, t = 0; i < 18; i++) {
		for (int j = 0; j < 48; j++)
			t = temp[j] ^= table[t];
		t = (t + i) & 0xff;
	}
	memcpy(state, temp, 16);
}

MD::CODE MD2::terminate() const {
	uint8	t2[16];
	State	s2		= state;
	uint8	index	= p & 0xf;

	memcpy(t2, block, index);
	memset(t2 + index, 16 - index, 16 - index);

	s2.transform(t2);

	memcpy(t2, state.checksum, 16);
	s2.transform(t2);
	return CODE(s2.state);
}

namespace iso {
fixed_string<33> to_string(MD::CODE c) {
	fixed_string<33>	r;
	for (int i = 0; i < 16; i++) {
		uint8	t = c[i];
		r[i * 2 + 0] = to_digit(t >> 4, 'a');
		r[i * 2 + 1] = to_digit(t & 15, 'a');
	}
	r[32] = 0;
	return r;
}
}
