#ifndef MD5_H
#define MD5_H

#include "base/array.h"
#include "base/strings.h"
#include "hash_stream.h"

namespace iso {

struct MD {
	struct CODE : array<uint8, 16> {
		CODE()					{}
		CODE(const uint8 s[16])	: array<uint8, 16>(s)	{}
		CODE(const uint32 s[4]) { (array<uint32le, 4>&)*this = s; }
	};
};

fixed_string<33> to_string(MD::CODE c);

struct MD2 : MD, block_writer<MD2, 16> {
	struct State {
		uint8	state[16], checksum[16];
		State() { clear(*this); }
		void transform(const uint8 block[16]);
	} state;

	MD2() {}
	MD2(const void *buffer, size_t size)	{ writebuff(buffer, size); }
	template<typename X> MD2(const X &x)	{ write(x); }
	void			reset()					{ state = State(); p = 0; }
	void			process(const void *data){ state.transform((const uint8*)block); }
	CODE			terminate()	const;
	CODE			digest()	const		{ return terminate(); }
	operator CODE()				const		{ return terminate(); }
};

struct MD45_State {
	uint32	a, b, c, d;
	MD45_State() : a(0x67452301), b(0xefcdab89), c(0x98badcfe), d(0x10325476) {}
};

struct MD4 : MD, block_writer<MD4, 64> {
	MD45_State	state;

	MD4()									{}
	MD4(const void *buffer, size_t size)	{ writebuff(buffer, size); }
	template<typename X> MD4(const X &x)	{ write(x); }
	void			reset()					{ state = MD45_State(); p = 0; }
	void			process(const void *data);
	CODE			terminate()	const;
	CODE			digest()	const		{ return terminate(); }
	operator CODE()				const		{ return terminate(); }
};

struct MD5 : MD, block_writer<MD5, 64> {
	MD45_State	state;

	MD5()									{}
	MD5(const void *buffer, size_t size)	{ writebuff(buffer, size); }
	template<typename X> MD5(const X &x)	{ write(x); }
	void			reset()					{ state = MD45_State(); p = 0; }
	void			process(const void *data);
	CODE			terminate()	const;
	CODE			digest()	const		{ return terminate(); }
	operator CODE()				const		{ return terminate(); }
};


} //namespace iso

#endif //MD5_H
