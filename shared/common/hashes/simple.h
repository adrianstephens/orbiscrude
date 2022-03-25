#ifndef SIMPLE_H
#define SIMPLE_H

#include "base/defs.h"

namespace iso {

namespace {

template<typename T, T INIT, T MUL, typename W> uint32 sum1(const W *buf, size_t len) {
	T hash = INIT;
	while (len--)
		hash = hash * MUL + *buf++;
	return hash;
}

template<typename T, T MOD, typename W> T sum2(T prev, const W *buf, size_t len) {
	static const int	SHIFT	= sizeof(T) * 4;
	static const T		BLOCK	= T((T(1) << ((sizeof(T) - sizeof(W)) * 4)) * 1.414f) - (MOD >> (sizeof(W) * 8)) - 2;

	T	sum1 = prev & bits<T>(SHIFT);
	T	sum2 = prev >> SHIFT;
	if (len == 1) {
		if ((sum1 += buf[0]) >= MOD)
			sum1	-= MOD;
		if ((sum2 += sum1) >= MOD)
			sum2	-= MOD;

	} else {
		while (len) {
			size_t n = min(len, BLOCK);
			len		-= n;
			while (n--) {
				sum1	+= *buf++;
				sum2	+= sum1;
			}
			sum1	%= MOD;
			sum2	%= MOD;
		}
	}
	return T(sum1 | (sum2 << SHIFT));
}

template<typename T, T MOD> inline auto sum2_create(T prev) {
	static const int	SHIFT	= sizeof(T) * 4;

	T	sum1	= prev & bits<T>(SHIFT);
	T	sum2	= prev >> SHIFT;
	T	check1	= MOD - (sum1 + sum2) % MOD;
	T	check2	= MOD - (sum1 + check1) % MOD;
	return T(check1 | (check2 << SHIFT));
}


template<typename T, int B> inline T mod_pow2m1(T t) {
	return (t & bits<T>(B)) + (t >> B);
}

}

inline uint32 adler32(uint32 prev, const uint8 *buf, size_t len) {
	return sum2<uint32, 65521>(prev, buf, len);
}

template<typename W> inline auto fletcher(const W *buf, size_t len, uint_t<sizeof(W) * 2> prev = 0) {
	return sum2<uint_t<sizeof(W) * 2>, W(~W(0))>(prev, buf, len);
}

template<typename T> inline auto fletcher_create(T prev) { return sum2_create<T, (T(1) << (sizeof(T) * 4)) - 1>(prev); }

inline uint16 fletcher16(const uint8  *buf, size_t len, uint16 prev = 0) { return fletcher(buf, len, prev); }
inline uint32 fletcher32(const uint16 *buf, size_t len, uint32 prev = 0) { return fletcher(buf, len, prev); }
inline uint64 fletcher64(const uint32 *buf, size_t len, uint64 prev = 0) { return fletcher(buf, len, prev); }

inline uint32 djb2(const uint8 *buf, size_t len) { return sum1<uint32, 5381, 33>(buf, len); }
inline uint32 sdbm(const uint8 *buf, size_t len) { return sum1<uint32, 0, 65599>(buf, len); }

} // namespace iso

#endif // SIMPLE_H
