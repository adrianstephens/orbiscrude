#include "base/defs.h"

namespace iso {

uint32	MurmurHash1_32(const void * key, int len, uint32 seed);
uint32	MurmurHash2_32(const void * key, int len, uint32 seed);
uint64	MurmurHash2_64(const void * key, int len, uint32 seed);
uint32	MurmurHash3_32(const void * key, int len, uint32 seed);
void	MurmurHash3_128(const void *key, const int len, uint32 seed, void *out);
void	MurmurHash3_64(const void *key, const int len, const uint32 seed, void *out);

} // namespace iso
