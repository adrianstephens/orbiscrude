#ifndef AES_H
#define AES_H

#include "base/defs.h"
#include "base/array.h"
#include "base/deferred.h"

namespace iso {


struct AES {
	typedef deferred<array<uint32le, 4>> block;
	enum {BLOCK_SIZE = sizeof(block)};
	int		nr;
	uint32	rk[68];
	bool	setkey_enc(const uint8 *key, int keysize);
	bool	setkey_dec(const uint8 *key, int keysize);
	void	encrypt(const block &in, block &out);
	void	decrypt(const block &in, block &out);
	void	encrypt(block &x)	{ encrypt(x, x); }
	void	decrypt(block &x)	{ decrypt(x, x); }
};

// may wrap these with CBC or GCM

struct AES_encrypt : AES {
	AES_encrypt(const const_memory_block &key)	{ setkey_enc(key, key.size32() * 8); }
	void	process(const void *buffer)			{ encrypt(*(block*)buffer); }
};

struct AES_decrypt : AES {
	AES_decrypt(const const_memory_block &key)	{ setkey_dec(key, key.size32() * 8); }
	void	process(const void *buffer)			{ decrypt(*(block*)buffer); }
};

} //namespace iso

#endif
