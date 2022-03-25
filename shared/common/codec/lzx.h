#ifndef LZX_H
#define LZX_H

#include "codec.h"
#include "vlc.h"
#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
// LZX
//-----------------------------------------------------------------------------

class LZX_decoder {
	enum {
		MIN_MATCH				= 2,
		MAX_MATCH				= 257,
		NUM_CHARS				= 256,
		PRETREE_NUM_ELEMENTS	= 20,
		ALIGNED_NUM_ELEMENTS	= 8,		// aligned offset tree #elements
		NUM_PRIMARY_LENGTHS		= 7,		// this one missing from spec!
		NUM_SECONDARY_LENGTHS	= 249,		// length tree #elements

		PRETREE_MAXSYMBOLS		= PRETREE_NUM_ELEMENTS,
		PRETREE_TABLEBITS		= 6,
		MAINTREE_MAXSYMBOLS		= NUM_CHARS + 50*8,
		MAINTREE_TABLEBITS		= 12,
		LENGTH_MAXSYMBOLS		= NUM_SECONDARY_LENGTHS+1,
		LENGTH_TABLEBITS		= 12,
		ALIGNED_MAXSYMBOLS		= ALIGNED_NUM_ELEMENTS,
		ALIGNED_TABLEBITS		= 7,
		LENTABLE_SAFETY			= 64,

		FRAME_SIZE				= 32768,
	};
	enum BLOCKTYPE : uint8 {
		BLOCKTYPE_INVALID		= 0,		// also blocktypes 4-7 invalid
		BLOCKTYPE_VERBATIM		= 1,
		BLOCKTYPE_ALIGNED		= 2,
		BLOCKTYPE_UNCOMPRESSED	= 3,
	};

	struct bits : bit_stack_count<uint32, true> {
		template<typename S> void fill(S& file, int n) {
			while (bits_left < n) {
				int	b0 = file.getc(), b1 = file.getc();
				bits_left	+= 16;
				bit_buffer	|= ((b1 << 8) | b0) << (32 - bits_left);
			}
		}
	};
	typedef	vlc_in0<bits, reader_intf>	vlc_t;

	malloc_block	window;		// decoding window
	uint64		reset_interval;	// which frame do we reset the compressor?

	uint32		R0, R1, R2;		// for the LRU offset system
	uint32		block_length;	// uncompressed length of this LZX block
	uint32		block_remaining; // uncompressed bytes still left to decode

	int			intel_filesize;	// magic header value used for transform
	bool		intel_started;	// has intel E8 decoding started?

	BLOCKTYPE	block_type;		// type of the current block
	bool		header_read;	// have we started decoding at all yet?
	uint32		num_offsets;	// number of match_offset entries in table

	vlc_t::state_t vlc0;

	// huffman code lengths
	uint8		MAINTREE_len	[MAINTREE_MAXSYMBOLS	+ LENTABLE_SAFETY];
	uint8		LENGTH_len		[LENGTH_MAXSYMBOLS		+ LENTABLE_SAFETY];
	uint8		ALIGNED_len		[ALIGNED_MAXSYMBOLS		+ LENTABLE_SAFETY];

	// huffman decoding tables
	uint16		MAINTREE_table	[(1 << MAINTREE_TABLEBITS)	+ MAINTREE_MAXSYMBOLS * 2];
	uint16		LENGTH_table	[(1 << LENGTH_TABLEBITS)	+ LENGTH_MAXSYMBOLS * 2];
	uint16		ALIGNED_table	[(1 << ALIGNED_TABLEBITS)	+ ALIGNED_MAXSYMBOLS * 2];

	static uint16	read_huffman(vlc_t& vlc, uint16* table, uint8 *lens, int bits, int maxsyms);
	static bool		read_lens(vlc_t &vlc, uint8 *lens, uint32 first, uint32 last);
	void			reset_state();
public:
	size_t		offset;			// number of bytes actually output
	size_t		length;			// overall decompressed length of stream

	uint8*		process(uint8 *dst, uint8 *dst_end, reader_intf file, TRANSCODE_FLAGS flags = TRANSCODE_NONE);
	LZX_decoder(int window_bits, size_t length, uint64 reset_interval = 0);
};

}

#endif	// LZX_H
