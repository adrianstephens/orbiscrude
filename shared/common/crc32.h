#ifndef CRC32_H
#define CRC32_H

//#define __SSE4_2__

#include "base/strings.h"
#include "hashes/crc.h"
#include "hashes/hash_stream.h"
#ifdef __SSE4_2__
#undef __STDC_HOSTED__
#include "extra/intel_cpu.h"
#include <nmmintrin.h>
#endif

namespace iso {

typedef CRC_def<uint16, 0xA001, true, false>			CRC16;	//0x8005
typedef CRC_def<uint32, 0xEDB88320, true, true>			CRC32;	//0x04C11DB7
typedef CRC_def<uint64, 0xC96C5795D7870F42, true, true>	CRC64;	//0x42F0E1EBA9EA3693
typedef CRC_def<uint32, 0x82f63b78, true, true>			CRC32C;	//0x1EDC6F41

template<bool INVERT> using _CRC32C = CRC_def<uint32, 0x82f63b78, true, INVERT>;

#ifdef __SSE4_2__
template<> inline bool		crc_hw_test<_CRC32C<false>, uint32>()					{ return cpu_features().SSE4_2; }
template<> inline uint32	crc_hw<_CRC32C<false>, uint8>(uint8 b, uint32 crc)		{ return _mm_crc32_u8(crc, b); }
#ifdef _M_X64
template<> inline uint32	crc_hw<_CRC32C<false>, uint64>(uint64 b, uint32 crc)	{ return _mm_crc32_u64(crc, b); }
#else
template<> inline uint32	crc_hw<_CRC32C<false>, uint32>(uint32 b, uint32 crc)	{ return _mm_crc32_u32(crc, b); }
#endif

typedef CRC_hw<CRC_def<uint32, 0x82f63b78, true, true>, uint64>		CRC32Chw;	//0x1EDC6F41
#endif


template<int BITS> struct CRC_def_bits;
template<> struct CRC_def_bits<16> : CRC16 {};
template<> struct CRC_def_bits<32> : CRC32 {};
template<> struct CRC_def_bits<64> : CRC64 {};

template<int BITS> struct crc {
	typedef size_t (*cb_t)(crc, char*, size_t);
	static cb_t cb;

	typedef	CRC_def_bits<BITS>	D;
	uint_bits_t<BITS>	id;

public:
	typedef uint_bits_t<BITS> representation;

	constexpr crc()							: id(0)			{}
	constexpr crc(representation id)		: id(id)		{}
	template<typename P> crc(const P &p)	: id(0)			{ this->write(p); }

	constexpr operator representation()	const				{ return id; }
	void	clear()											{ id = 0; }
	size_t	writebuff(const void *p, size_t size)			{ id = D::calc(p, size, id); return size; }

	template<typename T> bool	write(const T &t)			{ return global_write(*this, t); }
	template<typename P> void	set(const P &p)				{ clear(); this->write(p); }
	template<typename P> crc	operator+(const P &p) const { crc b(*this); b.write(p); return b; }

	static cb_t set_callback(cb_t p)						{ return exchange(cb, p); }
	size_t	string_len()							const	{ return 0; }
	size_t	string_get(char *s, size_t len)			const	{ return cb(*this, s, len); }
	size_t	string_get(char16 *s, size_t len)		const	{ return string_getter_transform<char>(*this, s, len); }

	friend bool	operator==(const crc a, const char *p) { return a == crc(p); }
};

template<int BITS> typename crc<BITS>::cb_t crc<BITS>::cb;
typedef crc<32> crc32;

#if 0
template<> iso_export uint16 crc<16>::raw(char c, uint16 crc);
template<> iso_export uint16 crc<16>::raw(const void *buffer, size_t len, uint16 crc);
template<> iso_export uint16 crc<16>::raw(const char *buffer, uint16 crc);

template<> iso_export uint32 crc<32>::raw(char c, uint32 crc);
template<> iso_export uint32 crc<32>::raw(const void *buffer, size_t len, uint32 crc);
template<> iso_export uint32 crc<32>::raw(const char *buffer, uint32 crc);

template<> iso_export uint64 crc<64>::raw(char c, uint64 crc);
template<> iso_export uint64 crc<64>::raw(const void *buffer, size_t len, uint64 crc);
template<> iso_export uint64 crc<64>::raw(const char *buffer, uint64 crc);
#endif

#define ISO_CRC(a, b)		iso::uint32(b)

template<size_t N> constexpr uint32 crc32_const(const char (&s)[N]) { return CRC_const<CRC32>(s, N - 1); }

uint32 constexpr operator"" _crc32(const char* s, size_t len)	{ return CRC_const<CRC32>(s, len); }
uint16 constexpr operator"" _crc16(const char* s, size_t len)	{ return CRC_const<CRC16>(s, len); }
uint32 constexpr operator"" _crc32c(const char* s, size_t len)	{ return CRC_const<CRC32C>(s, len); }

}//namespace iso

#endif // CRC32_H
