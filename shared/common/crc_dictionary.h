#ifndef CRC_DICTIONARY_H
#define CRC_DICTIONARY_H

#include "crc32.h"
#include "base/strings.h"

namespace iso {
	const char*			LookupCRC32(crc32 crc, const char *fallback);
	fixed_string<64>	LookupCRC32(crc32 crc);
	string_accum& operator<<(string_accum &a, const crc32 crc);
}

#endif // CRC_DICTIONARY_H