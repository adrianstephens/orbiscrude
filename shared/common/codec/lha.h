#ifndef LHA_H
#define LHA_H

#include "codec.h"
#include "vlc.h"
#include "huffman.h"

namespace iso {
namespace LHA {

enum METHOD {
	METHOD_UNKNOWN	= -1,
	METHOD_LZHUFF0	= 0,
	METHOD_LZHUFF1	= 1,
	METHOD_LZHUFF2	= 2,
	METHOD_LZHUFF3	= 3,
	METHOD_LZHUFF4	= 4,
	METHOD_LZHUFF5	= 5,
	METHOD_LZHUFF6	= 6,
	METHOD_LZHUFF7	= 7,
	METHOD_LARC		= 8,
	METHOD_LARC5	= 9,
	METHOD_LARC4	= 10,
	METHOD_LZHDIRS	= 11,
};



template<int BITS> struct SLIDE_hash {
	struct HASH {
		uint32		pos:31, too_many:1;
	};

	uint32			dicsiz;
	uint32			*chains;
	HASH			hashes[1 << BITS];

	void	insert_hash(uint32 hash, uint32 pos) {
		chains[pos & (dicsiz - 1)] = hashes[hash].pos; // chain the previous pos
		hashes[hash].pos = pos;
	}

	SLIDE_hash(uint32 dicsiz) : dicsiz(dicsiz) {
		chains	= new uint32[dicsiz];
		clear(hashes);
	}
	~SLIDE_hash() {
		delete[] chains;
	}

	void	slide_dict(uint32 off) {
		for (int i = 0; i < (1 << BITS); i++) {
			hashes[i].pos		= max(hashes[i].pos - off, 0);
			hashes[i].too_many	= false;
		}
		for (int i = 0; i < dicsiz; i++)
			chains[i] = max(chains[i] - off, 0);
	}
	uint32		search_dict_offset(const uint8 *src, const uint8 *src_end, uint32 pos, uint32 ref, uint32 off, uint32 &match_len, uint32 &match_off);
	template<typename H> uint32	search_dict(const uint8 *src, const uint8 *src_end, uint32 pos, uint32 hash, uint32 match_min, uint32 &match_len, int chain_limit, H&& next_hash);
};


template<int BITS> uint32 SLIDE_hash<BITS>::search_dict_offset(const uint8 *src, const uint8 *src_end, uint32 pos, uint32 ref, uint32 off, uint32 &match_len, uint32 &match_off) {
	const uint8	*match_end = src + match_len;

	uint32		chain	= 0;
	for (uint32	end = pos - dicsiz; ref > end; ref = chains[ref & (dicsiz - 1)], ++chain) {
		intptr_t	d	= pos - ref;

		if (match_end[0] == match_end[d]) {
			auto	new_end = iso::match_end(src, d, src_end);
			if (new_end > match_end) {
				match_end	= new_end;
				match_off	= d;
				if (new_end == src_end)
					break;
			}
		}
	}
	match_len	= match_end - src;
	return chain;
}

template<int BITS> template<typename H> uint32 SLIDE_hash<BITS>::search_dict(const uint8 *src, const uint8 *src_end, uint32 pos, uint32 hash, uint32 match_min, uint32 &match_len, int chain_limit, H&& next_hash) {
	uint32	off = 0, hash1 = hash;

	while (hashes[hash1].too_many && src + off + match_min < src_end)
		hash1	= next_hash(hash1, src + ++off) & ((1 << BITS) - 1);

	uint32	match_off;
	if (src + off + match_min < src_end) {
		if (search_dict_offset(src, src_end, pos + off, hashes[hash1].pos, off, match_len, match_off) >= chain_limit)
			hashes[hash1].too_many = true;

		if (off == 0 || match_len > off + 2)
			return match_off;

		src_end = src + off + 2;
	}

	search_dict_offset(src, src_end, pos, hashes[hash].pos, 0, match_len, match_off);
	return match_off;
}

void decode_lzhuf(istream_ref ifile, ostream_ref ofile, size_t size, METHOD method);
void encode_lzhuf(istream_ref ifile, ostream_ref ofile, size_t size, METHOD method);

} }	// namespace iso::LHA

#endif //LHA_H
