#include "iso/iso_convert.h"
#include "codec/aes.h"
#include "codec/cbc.h"

using namespace iso;

ISO_openarray<uint8> AESencrypt(const ISO_openarray<uint8> &bin, ISO::Array<uint8,16> key) {
	uint32	size	= bin.Count();
	ISO_openarray<uint8>	b(size);

	CBC_encrypt<AES_encrypt, 16, memory_writer>	aes(const_memory_block(key, 16), memory_block(b, size));
	aes.writebuff(bin, size);
	return b;
}

ISO_openarray<uint8> AESdecrypt(const ISO_openarray<uint8> &bin, ISO::Array<uint8,16> key) {
	uint32	size	= bin.Count();
	ISO_openarray<uint8>	b(size);

	CBC_decrypt<AES_decrypt, 16, memory_reader> aes(const_memory_block(key, 16), const_memory_block(bin, size));
	aes.readbuff(b, size);
	return b;
}

static initialise init(
	ISO_get_operation(AESencrypt),
	ISO_get_operation(AESdecrypt)
);