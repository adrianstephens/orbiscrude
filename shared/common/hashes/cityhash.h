#ifndef CITYHASH_H
#define CITYHASH_H

#include "base/bits.h"

namespace iso {
namespace CityHash {

//-----------------------------------------------------------------------------
// 32 bit
//-----------------------------------------------------------------------------

uint32 CityHash32(const char* s, uint32 len);

//-----------------------------------------------------------------------------
// 64 bit
//-----------------------------------------------------------------------------

uint64 CityHash64(const char* s, uint32 len);

inline uint64 shift_mix(uint64 val) {
	return val ^ (val >> 47);
}
inline uint64 HashLen16(uint64 u, uint64 v, uint64 mul = 0x9ddfea08eb382d69ULL) {
	return shift_mix((v ^ (shift_mix((u ^ v) * mul))) * mul) * mul;
}
inline uint64 CityHash64WithSeeds(const char* s, uint32 len, uint64 seed0, uint64 seed1) {
	return HashLen16(CityHash64(s, len) - seed0, seed1);
}
inline uint64 CityHash64WithSeed(const char* s, uint32 len, uint64 seed) {
	return CityHash64WithSeeds(s, len, 0x9ae16a3b2f90404fULL, seed);
}

//-----------------------------------------------------------------------------
// 128 bit
//-----------------------------------------------------------------------------

double_int<uint64> CityHash128WithSeed(const char* s, size_t len, uint64 seedlo, uint64 seedhi);
double_int<uint64> CityHash128(const char* s, size_t len);

} }  // namespace iso::CityHash

#endif	// CITYHASH_H
