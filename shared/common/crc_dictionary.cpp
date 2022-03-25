#include "crc_dictionary.h"
#include "iso/iso.h"
#include "base/algorithm.h"

namespace iso {
	struct DictionaryEntry {
		crc32 crc;
		const char *symbol;
		bool operator<(crc32 _crc) const { return crc < _crc; }
	};
	typedef ISO_openarray<DictionaryEntry> Dictionary;

	DictionaryEntry* _LookupCRC32(crc32 crc) {
		static ISO_ptr<Dictionary> dictionary = ISO::root("data")["dictionary"];
		if (dictionary) {
			Dictionary::iterator i = lower_bound(dictionary->begin(), dictionary->end(), crc);
			if (i != dictionary->end() && i->crc == crc)
				return i;
		}
		return 0;
	}

	const char* LookupCRC32(crc32 crc, const char *fallback) {
		if (DictionaryEntry *e = _LookupCRC32(crc))
			return e->symbol;
		return fallback;
	}

	fixed_string<64> LookupCRC32(crc32 crc) {
		if (DictionaryEntry *e = _LookupCRC32(crc))
			return e->symbol;
		return format_string<64>("crc_%08x", (uint32)crc);
	}

	string_accum& operator<<(string_accum &a, const crc32 crc) {
		if (DictionaryEntry *e = _LookupCRC32(crc))
			return a << e->symbol;
		return a.format("crc_%08x", (uint32)crc);
	}

}
