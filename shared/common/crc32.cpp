#include "crc32.h"
#include "hashes/crc.h"
//#include "maths/galois2.h"

namespace iso {

template<typename C> size_t default_cb(C, char*, size_t) {
	return 0;
}

template<> crc<32>::cb_t	crc<32>::cb = &default_cb<crc32>;

#if 0
template<> iso_export uint16 crc<16>::raw(char c, uint16 crc)							{ return CRC16::calc(c, crc);			}
template<> iso_export uint16 crc<16>::raw(const void *buffer, size_t len, uint16 crc)	{ return CRC16::calc(buffer, len, crc);	}
template<> iso_export uint16 crc<16>::raw(const char *buffer, uint16 crc)				{ return CRC16::calc(buffer, crc);		}

template<> iso_export uint32 crc<32>::raw(char c, uint32 crc)							{ return CRC32::calc(c, crc);			}
template<> iso_export uint32 crc<32>::raw(const void *buffer, size_t len, uint32 crc)	{ return CRC32::calc(buffer, len, crc);	}
template<> iso_export uint32 crc<32>::raw(const char *buffer, uint32 crc)				{ return CRC32::calc(buffer, crc);		}

template<> iso_export uint64 crc<64>::raw(char c, uint64 crc)							{ return CRC64::calc(c, crc);			}
template<> iso_export uint64 crc<64>::raw(const void *buffer, size_t len, uint64 crc)	{ return CRC64::calc(buffer, len, crc);	}
template<> iso_export uint64 crc<64>::raw(const char *buffer, uint64 crc)				{ return CRC64::calc(buffer, crc);		}
#endif

#if 0
class testcrc {
public:
	bool recurse(uint32 rcrc, uint32 crc, char *buffer, int depth, char *start) {
		if (depth == 0) {
			uint32	rcrc2		= rcrc ^ crc;
			buffer[0] = rcrc2;
			buffer[1] = rcrc2 >> 8;
			buffer[2] = rcrc2 >> 16;
			buffer[3] = rcrc2 >> 24;
			if (is_lower(buffer[0]) &&	is_lower(buffer[1]) && is_lower(buffer[2]) && is_lower(buffer[3])) {
				buffer[4] = 0;
				ISO_TRACEF("%s\n", start);
				return true;
			}
			return false;
		}
		for (int i = 'a'; i <= 'z'; i++) {
			buffer[0] = i;
			if (recurse(rcrc, crc32_table.calc(i, crc), buffer + 1, depth - 1, start)) {
	//			return true;
			}
		}
		return false;
	}

	const char *find(uint32 crc) {
		static char	buffer[17];
		uint32	rcrc = crc32_table.inverse(crc);

		for (int d = 0; d < 4; d++) {
			if (recurse(rcrc, 0, buffer, d, buffer))
				;//ISO_TRACEF("%s\n", buffer);
		}
		return NULL;
	}

	testcrc() {
		ISO_TRACE(find(0x8390d9e0));
	}
} testcrc;
#endif

#if 0
// Take a length and build four lookup tables for applying the zeros operator for that length, byte-by-byte on the operand
struct zeros {
	gf2_vec<32>	v[4][256];

	zeros(size_t len, gf2_poly<32> poly) {
		gf2_mat<32, 32> op;

		op[0]		= poly;
		gf2_poly<32>	row(1);
		for (uint32 i = 1; i < 32; i++, row.shift())
			op[i] = row;

		while (len >>= 1)
			op = op * op;

		for (uint32 i = 0; i < 256; i++) {
			v[0][i] = op * gf2_vec<32>(i << 0);
			v[1][i] = op * gf2_vec<32>(i << 8);
			v[2][i] = op * gf2_vec<32>(i << 16);
			v[3][i] = op * gf2_vec<32>(i << 24);
		}
	}

	auto shift(uint32 crc) {
		return (v[0][crc & 0xff] + v[1][(crc >> 8) & 0xff] + v[2][(crc >> 16) & 0xff] + v[3][crc >> 24]).u;
	}
};

template<typename T> uint32 crc32c_hw(uint32 crc, const void *buf, size_t len) {
	// Tables for hardware crc that shift a crc by long_stride and short_stride zeros.
	static const uint32	short_stride	= 256;
	static const uint32	long_stride		= 8192;
	static zeros	crc32c_long(long_stride, CRC32C_poly);
	static zeros	crc32c_short(short_stride, CRC32C_poly);

	const uint8 *next = (const uint8*)buf;
	T	crc0 = ~crc;

	// compute the crc for up to seven leading bytes to bring the data pointer to an eight-byte boundary
	while (len && ((uintptr_t)next & (sizeof(T) - 1)) != 0) {
		crc0 = _mm_crc32_u8(crc0, *next++);
		len--;
	}

	// compute the crc on sets of long_stride*3 bytes, executing three independent crc instructions, each on long_stride bytes
	// throughput of one crc per cycle, but a latency of three cycles
	const T *p = (const T*)next;
	while (len >= long_stride * 3) {
		T	crc1 = 0, crc2 = 0;
		for (const T *end = p + long_stride / sizeof(T); p < end; ++p) {
			crc0 = crc32c_hw(crc0, p[long_stride * 0 / sizeof(T)]);
			crc1 = crc32c_hw(crc1, p[long_stride * 1 / sizeof(T)]);
			crc2 = crc32c_hw(crc2, p[long_stride * 2 / sizeof(T)]);
		}
		crc0 = crc32c_long.shift(crc32c_long.shift(crc0) ^ crc1) ^ crc2;
		p	+= long_stride * 2 / sizeof(T);
		len -= long_stride * 3;
	}

	// do the same thing, but now on short_stride*3 blocks for the remaining data less than a long_stride*3 block
	while (len >= short_stride * 3) {
		T	crc1 = 0, crc2 = 0;
		for (const T *end = p + short_stride / sizeof(T); p < end; ++p) {
			crc0 = crc32c_hw(crc0, p[short_stride * 0 / sizeof(T)]);
			crc1 = crc32c_hw(crc1, p[short_stride * 1 / sizeof(T)]);
			crc2 = crc32c_hw(crc2, p[short_stride * 2 / sizeof(T)]);
		}
		crc0 = crc32c_short.shift(crc32c_short.shift(crc0) ^ crc1) ^ crc2;
		p	+= short_stride * 2 / sizeof(T);
		len -= short_stride * 3;
	}

	// compute the crc on the remaining eight-byte units less than a short_stride*3	block
	for (const T *end = p + len / sizeof(T); p < end; ++p)
		crc0 = crc32c_hw(crc0, *p);
	len &= 7;

	// compute the crc for up to seven trailing bytes
	for (uint8 *p1 = (uint8*)p; len; --len)
		crc0 = _mm_crc32_u8(crc0, *p1++);

	return ~(uint32)crc0;
}
#endif
} // namespace iso
