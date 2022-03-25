#ifndef SHA_H
#define SHA_H

#include "_sha.h"
#include "base/array.h"
#include "base/strings.h"
#include "hash_stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	SHA1
//-----------------------------------------------------------------------------

class SHA1 : public SHA1_const, public block_writer<SHA1, 64> {
	State	state;
public:
	void			process(const void *data);
	const uint32*	terminate();

	struct CODE : array<uint8, 20> {
		CODE() {}
		CODE(const uint32 s[5]) { (array<uint32be, 5>&)*this = s; }
	};

	SHA1()									: state(init_state())	{}
	SHA1(const void *buffer, size_t size)	: state(init_state())	{ writebuff(buffer, size); }
	template<typename X> SHA1(const X &x)	: state(init_state())	{ this->write(x); }

	operator CODE()		{ return terminate(); }
	CODE	digest()	{ return terminate(); }
	void	reset()		{ state = init_state(); p = 0; }
};

inline auto to_string(const SHA1::CODE &sha) {
	return to_hex_string<20>(sha.begin());
}

//-----------------------------------------------------------------------------
//	SHA2
//-----------------------------------------------------------------------------

template<typename T> class SHA2 : public block_writer<SHA2<T>, sizeof(T) * 16> {
	array<T, 8>	state;
public:
	enum {BLOCK_SIZE = sizeof(T) * 16};
	void		process(const void *data);
	const T*	terminate();
	SHA2(const T i[8]) : state((array<T, 8>)i)	{}
	void		reset(const T i[8]) { state = array<T, 8>(i); }
};

template<typename T> const T *SHA2<T>::terminate() {
	memory_block	b	= this->buffered();
	uint8			*e	= b.end();
	*e	= 0x80;
	memset(e + 1, 0, BLOCK_SIZE - b.length() - 1);
	if (b.length() > BLOCK_SIZE - sizeof(T) * 2) {
		process(b);
		memset(b, 0, BLOCK_SIZE);
	}
	uint8	*len = (uint8*)b + BLOCK_SIZE;
	for (uint64 t = this->tell() * 8; t; t = t >> 8)
		*--len = t & 0xff;
	process(b);
	return state.begin();
}

template<typename T, int N> class SHA2_N : public SHA2<T> {
	static const T init_state[8];
public:
	struct CODE : array<uint8, N * sizeof(T)> {
		CODE() {}
		CODE(const T s[N]) { (array<bigendian<T>, N>&)*this = s; }
	};
	SHA2_N()								: SHA2<T>(init_state) {}
	SHA2_N(const void *buffer, size_t size)	: SHA2<T>(init_state)	{ this->writebuff(buffer, size); }
	template<typename X> SHA2_N(const X &x)	: SHA2<T>(init_state)	{ this->write(x); }

	operator CODE()		{ return this->terminate(); }
	CODE	digest()	{ return this->terminate(); }
	void	reset()		{ SHA2<T>::reset(init_state); this->p = 0; }
};

typedef SHA2_N<uint32, 7>	SHA224;
typedef SHA2_N<uint32, 8>	SHA256;
typedef SHA2_N<uint64, 6>	SHA384;
typedef SHA2_N<uint64, 8>	SHA512;

} //namespace iso

#endif// SHA_H

