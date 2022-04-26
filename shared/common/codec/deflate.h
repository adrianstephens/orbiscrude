#ifndef DEFLATE_H
#define DEFLATE_H

#include "window.h"
#include "utilities.h"
#include "vlc.h"

namespace iso {

struct deflate_base {
	enum {
		MAX_BITS		= 15,	// All codes must not exceed MAX_BITS bits
		MAX_BL_BITS		= 7,

		LITERALS		= 256,						// number of literal bytes 0..255
		END_BLOCK		= 256,
		LENGTH_CODES	= 29,						// number of length codes, not counting the special END_BLOCK code
		L_CODES			= LITERALS+1+LENGTH_CODES,	// number of Literal or Length codes, including the END_BLOCK code
		L_CODES_STATIC	= L_CODES + 2,				// +2 illegal codes for static trees
		D_CODES			= 30,						// number of distance codes
		BL_CODES		= 19,						// number of codes used to transfer the bit lengths
		MIN_MATCH		= 3,
		MAX_MATCH		= 258,
		MIN_LOOKAHEAD	= MAX_MATCH + MIN_MATCH + 1,
		MAX_BLOCK_SIZE	= 0xffff,

	};

	enum BLOCK_TYPE {
		STORED_BLOCK	= 0,
		STATIC_TREES	= 1,
		DYN_TREES		= 2,
	};
	enum {
		REP_3_6		= 16,	// repeat previous bit length 3-6 times (2 bits of repeat count)
		REPZ_3_10	= 17,	// repeat a zero length 3-10 times  (3 bits of repeat count)
		REPZ_11_138	= 18,	// repeat a zero length 11-138 times  (7 bits of repeat count)
	};

	static const uint8		order[19];
	static const uint16		len_base[], dist_base[];
	static const uint8		len_extra[], dist_extra[], bl_extra[];
	static const uint8		static_len_bits[L_CODES_STATIC], static_dist_bits[D_CODES];
};

//-----------------------------------------------------------------------------
//  deflate_decoder
//-----------------------------------------------------------------------------

class deflate_decoder : deflate_base, public codec_defaults {
protected:
public:
	struct code {
		uint8		op;		// operation, extra bits, table bits
		uint8		bits;	// bits in thisc part of the code
		uint16		val;	// offset in table or code value
	};

	enum MODE {
		HEAD,				// i: waiting for magic header
		DONE,				// done -- remain here until reset
		BAD,				// got a data error -- remain here until reset
		BLOCK,				// block type
			COPY,			// i/o: waiting for input or output to copy stored block
			LEN,			// i: waiting for length/lit code
			MATCH,			// o: waiting for output space to copy string
			LIT,			// o: waiting for output space to write literal
	};

	static const uint8	len_ops[];
	static const uint8	dist_ops[];
	static code			static_len[512];
	static code			static_dist[32];

	typedef vlc_in<uint32, false> VLC;
	VLC::state_t	vlc0;

	MODE			mode;
	bool			last;			// true if processing last block
	uint32			L;				// literal or length of data to copy
	uint32			D;				// distance back to copy string from
	size_t			offset;

	// sliding window
	uint32			wbits;			// log base 2 of requested window size
	malloc_block	window;			// allocated sliding window, if needed

	// code tables
	const code		*lencode;		// starting table for length/literal codes
	const code		*distcode;		// starting table for distance codes
	uint32			lenbits;		// index bits for lencode
	uint32			distbits;		// index bits for distcode
	code			codes[2048];	// space for code tables

	static int		inflate_table(const uint8 *lens, uint32 codes, code *table, uint32 &bits, uint16 *work, int max_entries, const uint16 *base, const uint8 *extra_bits, int end);
	static bool		make_static_tables();

	template<typename WIN> uint8*	process1(VLC &vlc, uint8 *dst, uint8 *dst_end, WIN win);

public:
	uint8*			process(uint8* dst, uint8 *dst_end, istream_ref file, TRANSCODE_FLAGS flags);
	void			restore_unused(istream_ref file)		{ file.seek_cur(-int(vlc0.bits_held()) / 8); vlc0.reset(); }

	deflate_decoder(int wbits = 15) : mode(BLOCK), last(false), offset(0), wbits(wbits) {
		static bool unused = make_static_tables();
	}
};

//-----------------------------------------------------------------------------
//  deflate_encoder
//-----------------------------------------------------------------------------

class deflate_encoder : deflate_base, public codec_defaults {
public:
	enum LEVEL {
		NO_COMPRESSION		= 0,
		BEST_SPEED			= 1,
		BEST_COMPRESSION	= 9,
	};
	enum STRATEGY {
		STORED				= 0,
		FAST				= 1,
		SLOW				= 2,
		FILTERED			= 3,
		HUFFMAN_ONLY		= 4,
		RLE					= 5,
	};

	typedef vlc_out<uint32, false>	VLC;

	struct entry {
		uint8	lc;
		uint16	dist;
	};
	// Data structure describing a single value and its code string
	struct ct_data {
		union {
			uint16	freq;		// frequency count
			uint16	code;		// bit string
		};
		union {
			uint16	parent;		// parent node in Huffman tree
			uint16	len;		// length of bit string
			uint16	depth;
		};
		void send(VLC &vlc) const { vlc.put(code, len); }
		friend bool operator<(const ct_data &n, const ct_data &m) {
			return n.freq < m.freq || (n.freq == m.freq && n.depth <= m.depth);
		}
	};

	struct TreeDesc {
		const uint8		*extra_bits;	// extra bits for each code or NULL
		int				extra_base;		// base index for extra_bits
		int				num_elems;		// max number of elements in the tree
		int				max_length;		// max bit length for the codes
		int				build_tree(ct_data *tree, uint32 &len) const;
		uint32			total_len(const ct_data *tree) const;
		static void		gen_codes(ct_data *tree, int max_code, const uint16 *counts);
		static void		scan_tree(ct_data *tree, int max_code, ct_data *bl_tree);
		static void		send_tree(VLC &vlc, const ct_data *tree, int max_code, const ct_data *bl_tree);
	};

	struct SearchParams {
		STRATEGY		strategy;
		bool			fixed;			// use static tree only
		uint16			good_length;	// reduce lazy search above this match length
		uint16			max_lazy;		// do not perform lazy search above this match length
		uint16			nice_length;	// quit search above this match length
		uint16			max_chain;		// hash chains are never searched beyond this length

		// max hash chain length
		uint32	calc_max_chain(uint32 best_len)	const { return best_len >= good_length ? max_chain >> 2 : max_chain; }
	};

	static const TreeDesc	len_desc, dist_desc, bl_desc;
	static ct_data			static_len_tree[L_CODES_STATIC], static_dist_tree[D_CODES];
//	static const uint8		_dist_code[512];
	static const uint8		_length_code[256];
	static const SearchParams configuration_table[];

	// Mapping from a distance to a distance code
	static constexpr uint8 dist_code(uint32 dist);
	static constexpr uint8 length_code(uint32 len);
	
	SearchParams	params;

	VLC::state_t	vlc0;
	uint32			wsize;
	malloc_block	window;

	//hash
	uint32			hash_size;				// number of elements in hash table
	uint32			hash_shift;				// Number of bits by which ins_h must be shifted at each input step
	temp_array<uint32>	head;				// Heads of the hash chains or NIL
	temp_array<uint32>	prev;				// Link to older string with same hash index

	int32			block_start;			// offset of the beginning of the current output block
	uint32			match_length;			// length of best match
	uint32			match_start;			// start of matching string
	bool			match_available;		// set if previous match exists

	temp_array<entry>	buf;
	uint32			num_lits;				// running index in buf
	uint32			offset;

	bool		tally_lit(uint8 c) {
		buf[num_lits]	= {c, 0};
		return ++num_lits == buf.size() - 1;
	}
	bool		tally_dist(uint32 dist, uint32 length) {
		ISO_ASSERT(length <= MAX_MATCH);
		buf[num_lits]	= {uint8(length - MIN_MATCH), uint16(dist)};
		return ++num_lits == buf.size() - 1;
	}

	// hash
	void clear_hash() {
		memset(head.begin(), 0, hash_size * sizeof(uint16));
	}
	uint32 update_hash(uint32 h, uint8 c) {
		return ((h << hash_shift) ^ c) & (hash_size - 1);
	}
	uint32 insert_string(uint32 h, uint32 pos) {
		auto	r	= prev[pos & (wsize - 1)] = head[h];
		head[h]		= pos;
		return r;
	}

	constexpr size_t	MAX_DIST() { return min(wsize - MIN_LOOKAHEAD, 32768); }

	static void	compress_block(VLC &vlc, const ct_data *ltree, const ct_data *dtree, entry *buf, uint32 num_lits);
	void	flush_block(VLC &vlc, const uint8* src, const uint8* base, bool last);
	void	longest_match(const uint8 *src, uint32 lookahead, uint32 match_pos, external_window win, uint32 best_len, uint32 nice_len, uint32 max_chain);

	const uint8* deflate_stored	(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win);
	const uint8* deflate_fast	(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win);
	const uint8* deflate_slow	(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win, bool filtered);
	const uint8* deflate_rle	(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win);
	const uint8* deflate_huff	(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win);
	
	static bool make_static_tables();

public:
	const uint8*	process(ostream_ref file, const uint8* src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	void			flush(ostream_ref file)		{ VLC(file).flush(); }

	deflate_encoder(int level = BEST_COMPRESSION, int wbits = 15, int mem_level = 8);
	deflate_encoder(deflate_encoder&&) = default;
	deflate_encoder(deflate_encoder&) : deflate_encoder() { ISO_ASSERT(0); }

};

//-----------------------------------------------------------------------------
//  zlib
//-----------------------------------------------------------------------------

struct zlib_base {
	struct header {
		uint8	method:4, info:4;
		uint8	check:5, dict:1, level:2;
		bool	verify() const {
			uint8	*p	= (uint8*)this;
			return (((p[0] << 8) + p[1]) % 31) == 0;
		}
		void	set_check() {
			uint8	*p	= (uint8*)this;
			check	= 0;
			check	= 31 - (((p[0] << 8) + p[1]) % 31);
		}
	};
	enum {
		METHOD_DEFLATED = 8
	};
	enum {
		LEVEL_FASTEST	= 0,
		LEVEL_FAST		= 1,
		LEVEL_DEFAULT	= 2,
		LEVEL_MAXIMUM	= 3,
	};

	uint32	adler = 1;
};

class zlib_decoder : public deflate_decoder, zlib_base {
public:
	uint8*			process(uint8* dst, uint8 *dst_end, reader_intf file, TRANSCODE_FLAGS flags);
	zlib_decoder() : deflate_decoder(0) { mode = HEAD; }
};

class zlib_encoder : public deflate_encoder, zlib_base {
	header	h;
public:
	const uint8*	process(ostream_ref file, const uint8* src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	zlib_encoder(int wbits = 15, bool dict = false, int level = BEST_COMPRESSION) : deflate_encoder(level, wbits) {
		h.method	= METHOD_DEFLATED;
		h.info		= wbits - 8;
		h.dict		= dict;
		h.level		= LEVEL_DEFAULT;
		h.set_check();
	}
	zlib_encoder(zlib_encoder&)		= default;
	zlib_encoder(zlib_encoder&&)	= default;
};

} // namespace iso
#endif // DEFLATE_H
