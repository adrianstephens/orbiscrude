#ifndef CRC_H
#define CRC_H

#include "base/bits.h"

namespace iso {
/*
template<typename T, T POLY, bool REVERSE> struct CRC_table {
	T	t[256];
	CRC_table() {
		for (uint32 i = 0; i < 256; i++) {
			T	crc = T(i) << (sizeof(T) * 8 - 8);
			for (int j = 0; j < 8; j++)
				crc = (crc << 1) ^ (crc >> (sizeof(T) * 8 - 1) ? POLY : 0);
			t[i] = crc;
		}
	}
	constexpr T operator[](int i) const { return t[i]; }
};
template<typename T, T POLY> struct CRC_table<T, POLY, true> {
	T	t[256];
	CRC_table() {
		for (uint32 i = 0; i < 256; i++) {
			T	crc = T(i);
			for (int j = 0; j < 8; j++)
				crc = (crc >> 1) ^ (crc & 1 ? POLY : 0);
			t[i] = crc;
		}
	}
	constexpr T operator[](int i) const { return t[i]; }
};
*/
template<typename T, T POLY, bool REVERSE> struct CRC_table_generator {
	static constexpr T f(T crc, int j) {
		return j == 8 ? crc : f((crc << 1) ^ (crc >> (sizeof(T) * 8 - 1) ? POLY : 0), j + 1);
	}
	static constexpr T f(T crc) {
		return f(crc << ((sizeof(T) - 1) * 8), 0);
	}
};
template<typename T, T POLY> struct CRC_table_generator<T, POLY, true> {
	static constexpr T f(T crc, int j = 0) {
		return j == 8 ? crc : f((crc >> 1) ^ (crc & 1 ? POLY : 0), j + 1);
	}
};

template<typename T, typename TABLE, size_t...I> constexpr meta::array<T, sizeof...(I)> make_crc_table(meta::index_list<I...>&&) {
	return {{TABLE::f(I)...}};
}

template<typename T, T POLY, bool REVERSE> struct crc_table {
	static constexpr meta::array<T,256> t = make_crc_table<T, CRC_table_generator<T, POLY, REVERSE>>(meta::make_index_list<256>());
};
template<typename T, T POLY, bool REVERSE> constexpr meta::array<T,256> crc_table<T,POLY,REVERSE>::t;

template<typename T, bool REVERSE>	struct CRC_next {
	static inline T _next(T crc, uint8 c, const T *table) {	// table lookup
		return table[(crc >> ((sizeof(T) - 1) * 8)) ^ c] ^ (crc << 8);
	}
	template<typename C> static T _slow(T crc, C c, T poly) { // one bit at a time
		for (uint32 i = 0; i < sizeof(C) * 8; i++, c >>= 1) {
			crc = (c ^ crc) >> (sizeof(T) * 8 - 1)
				? (crc << 1) ^ poly
				: (crc << 1);
		}
		return crc;
	}
	static T _inverse(T crc, T poly) {	// reverse
		for (uint32 i = 0; i < sizeof(T) * 8; i++) {
			crc = crc & 1
				? ((crc ^ poly) >> 1) | (1 << (sizeof(T) * 8 - 1))
				: crc >> 1;
		}
		return crc;
	}
};

template<typename T> struct CRC_next<T, true> {
	static inline T _next(T crc, uint8 c, const T *table) {	// table lookup
		return table[(crc ^ c) & 0xff] ^ (crc >> 8);
	}
	template<typename C> static T _slow(T crc, C c, T poly) { // one bit at a time
		for (uint32 i = 0; i < sizeof(C) * 8; i++, c >>= 1) {
			crc = (c ^ crc) & 1
				? (crc >> 1) ^ poly
				: (crc >> 1);
		}
		return crc;
	}
	static T _inverse(T crc, T poly) {	// reverse
		for (uint32 i = 0; i < sizeof(T) * 8; i++) {
			crc = crc & (1 << (sizeof(T) * 8 - 1))
				? ((crc ^ poly) << 1) | 1
				: crc << 1;
		}
		return crc;
	}
};

template<typename T, T POLY, bool REVERSE, bool INVERT> class CRC_def;

template<typename T, T POLY, bool REVERSE> class CRC_def<T, POLY, REVERSE, false> {
	typedef CRC_next<T,REVERSE>	N;
public:
	typedef T	digest_t;

	static T	calc(char c, T crc = 0) {
		return N::_next(crc, c, crc_table<T, POLY, REVERSE>::t.begin());
	}
	static T	calc(const void *buffer, size_t len, T crc = 0) {
		if (const uint8 *p = (const uint8*)buffer) {
			for (const uint8 *e = p + len; p < e; ++p)
				crc = N::_next(crc, *p, crc_table<T, POLY, REVERSE>::t.begin());
		}
		return crc;
	}
	static T	calc(const char *buffer, T crc = 0) {
		if (buffer) {
			while (uint8 c = (uint8)*buffer++)
				crc = N::_next(crc, c, crc_table<T, POLY, REVERSE>::t.begin());
		}
		return crc;
	}
	static T	combine(T crc1, T crc2, int len2) {
		for (int n = len2; n--;)
			crc1 = N::_next(crc1, 0);
		return crc1 ^ crc2;
	}
	template<typename C> static T	slow(C c, T crc = 0) {
		return N::_slow(crc, c, POLY);
	}
	static T	inverse(T crc) {
		return N::_inverse(crc, POLY);
	}
};

template<typename T, T POLY, bool REVERSE> class CRC_def<T, POLY, REVERSE, true> {
	typedef	CRC_def<T, POLY, REVERSE, false>	B;
public:
	typedef T	digest_t;

	static T	calc(char c, T crc = 0)							{ return ~B::calc(c, ~crc); }
	static T	calc(const void *buffer, size_t len, T crc = 0) { return ~B::calc(buffer, len, ~crc); }
	static T	calc(const char *buffer, T crc = 0)				{ return ~B::calc(buffer, ~crc); }
	static T	combine(T crc1, T crc2, int len2)				{ return ~B::combine(~crc1, ~crc2, len2); }
	static T	inverse(T crc)									{ return ~B::inverse(~crc, POLY); }
	template<typename C> static T	slow(C c, T crc = 0)		{ return ~B::slow(c, ~crc); }
};

// for hw supported case:

template<typename D, typename T> typename D::digest_t crc_hw(T t, typename D::digest_t crc);
template<typename D, typename T> bool crc_hw_test();

template<typename D, typename T> class CRC_hw {
	typedef typename D::digest_t digest_t;

	static digest_t (*f)(const void *buffer, size_t len, digest_t crc);

	static digest_t hw(const void *buffer, size_t len, digest_t crc) {
		const uint8	*p1 = (const uint8*)buffer;
		while (len && ((uintptr_t)p1 & (sizeof(T) - 1)) != 0) {
			crc = crc_hw<D, uint8>(crc, *p1++);
			len--;
		}
		T	crc2 = crc;
		const T *p = (const T*)p1, *e = p + len / sizeof(T);
		while (p != e)
			crc2 = crc_hw<D, T>(crc, *p++);

		p1	= (const uint8*)p;
		crc	= (uint32)crc2;
		for (len &= (sizeof(T) - 1); len; --len)
			crc = crc_hw<D, uint8>(crc, *p1++);
		return crc;
	}
	static digest_t	sw(const void *buffer, size_t len, digest_t crc) {
		return D::calc(buffer, len, crc);
	}
	static digest_t test(const void* buffer, size_t len, digest_t crc) {
		f = crc_hw_test<D, T> ? hw : sw;
		return f(buffer, len, crc);
	}
	static digest_t	calc(const void *buffer, size_t len, digest_t crc = 0) {
		return f(buffer, len, crc);
	}
	static digest_t	calc(const char *buffer, digest_t crc = 0) {
		return f(buffer, strlen(buffer), crc);
	}
};
template<typename D, typename T> typename D::digest_t (*CRC_hw<D, T>::f)(const void *buffer, size_t len, typename D::digest_t crc) = CRC_hw<D, T>::test;

template<typename D> struct CRC_hasher {
	typedef typename D::digest_t	T;
	T		crc;
	CRC_hasher() : crc(0) {}
	void	process(const void* buffer, size_t len) { crc = D::calc(buffer, len, crc); }
	T		digest()	const						{ return crc; }
};

template<typename D> struct CRC_const_s;

template<typename T, T POLY, bool REVERSE> struct CRC_const_s<CRC_def<T, POLY, REVERSE, false>> {
	static constexpr T f(const char *s, size_t len, size_t i = 0, T crc = 0) {
		return i == len ? crc : f(s, len, i + 1, CRC_table_generator<T, POLY, REVERSE>::f((crc ^ s[i]) & 0xff) ^ (crc >> 8));
	}
};
template<typename T, T POLY, bool REVERSE> struct CRC_const_s<CRC_def<T, POLY, REVERSE, true>> {
	static constexpr T f(const char *s, size_t len) {
		return ~CRC_const_s<CRC_def<T, POLY, REVERSE, false>>::f(s, len, 0, ~0);
	}
};

template<typename D> constexpr uint32 CRC_const(const char *s, size_t len)		{ return CRC_const_s<D>::f(s, len); }
template<typename D, size_t N> constexpr uint32 CRC_const(const char (&s)[N])	{ return CRC_const_s<D>::f(s, N - 1); }

} // namespace iso

#endif // CRC_H
