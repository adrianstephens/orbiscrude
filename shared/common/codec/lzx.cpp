#include "lzx.h"

using namespace iso;

//-----------------------------------------------------------------------------
// LZX compression / decompression definitions
//-----------------------------------------------------------------------------

// Huffman decoding macros

// READ_HUFFSYM_LZX(tablename, var) decodes one huffman symbol from the bitstream using the stated table
uint16 read_huffman(uint16 vlc, uint16* table, int bits, int maxsyms) {
	uint16	sym = table[vlc >> (16 - bits)];
	vlc <<= bits;
	while (sym >= maxsyms) {
		sym = table[sym * 2 + (vlc >> 15)];
		vlc <<= 1;
	}
	return sym;
}

uint16 LZX_decoder::read_huffman(LZX_decoder::vlc_t& vlc, uint16* table, uint8 *lens, int bits, int maxsyms) {
	uint16	sym = ::read_huffman(vlc.peek<uint16>(), table, bits, maxsyms);
	vlc.discard(lens[sym]);
	return sym;
}

// make_decode_table(nsyms, nbits, length[], table[])
static bool make_decode_table(uint32 nsyms, uint32 nbits, uint8 *length, uint16 *table) {
	uint32	pos			= 0;				// the current position in the decode table
	uint32	table_mask	= 1 << nbits;
	uint32	bit_mask	= table_mask >> 1;	// don't do 0 length codes
	uint32	next_symbol = bit_mask;			// base of allocation for long codes

	// fill entries for codes short enough for a direct mapping
	for (int bit_num = 1; bit_num <= nbits; bit_num++) {
		for (int sym = 0; sym < nsyms; sym++) {
			if (length[sym] != bit_num)
				continue;
			int	leaf = pos;
			if ((pos += bit_mask) > table_mask)
				return false; // table overrun
			// fill all possible lookups of this symbol with the symbol itself
			for (int fill = bit_mask; fill--;)
				table[leaf++] = sym;
		}
		bit_mask >>= 1;
	}

	// full table already?
	if (pos == table_mask)
		return true;

	// clear the remainder of the table
	for (int sym = pos; sym < table_mask; sym++)
		table[sym] = 0xFFFF;

	// allow codes to be up to nbits+16 long, instead of nbits
	pos			<<= 16;
	table_mask	<<= 16;
	bit_mask	= 1 << 15;

	for (int bit_num = nbits + 1; bit_num <= 16; bit_num++, bit_mask >>= 1) {
		for (int sym = 0; sym < nsyms; sym++) {
			if (length[sym] != bit_num)
				continue;

			int	leaf = pos >> 16;
			for (int fill = 0; fill < bit_num - nbits; fill++) {
				// if this path hasn't been taken yet, 'allocate' two entries
				if (table[leaf] == 0xFFFF) {
					table[(next_symbol << 1)]		= 0xFFFF;
					table[(next_symbol << 1) + 1]	= 0xFFFF;
					table[leaf]						= next_symbol++;
				}
				// follow the path and select either left or right for next bit
				leaf = (table[leaf] << 1) + ((pos >> (15 - fill)) & 1);
			}
			table[leaf] = sym;

			if ((pos += bit_mask) > table_mask)
				return false; // table overflow
		}
	}

	// full table?
	if (pos < table_mask) {
		// either erroneous table, or all elements are 0 - let's find out.
		for (int sym = 0; sym < nsyms; sym++)
			if (length[sym])
				return false;
	}
	return true;
}

bool LZX_decoder::read_lens(vlc_t &vlc, uint8 *lens, uint32 first, uint32 last) {
	uint8	PRETREE_len		[PRETREE_MAXSYMBOLS			+ LENTABLE_SAFETY];
	uint16	PRETREE_table	[(1 << PRETREE_TABLEBITS)	+ PRETREE_MAXSYMBOLS * 2];

	for (uint32 x = 0; x < PRETREE_MAXSYMBOLS; x++)
		PRETREE_len[x] = vlc.get(4);

	if (!make_decode_table(PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, PRETREE_len, PRETREE_table))
		return false;

	for (uint32 x = first; x < last; ) {
		int	z = ::read_huffman(vlc.peek<uint16>(), PRETREE_table, PRETREE_TABLEBITS, PRETREE_MAXSYMBOLS);
		vlc.discard(PRETREE_len[z]);

		if (z == 17) {
			// code = 17, run of ([read 4 bits]+4) zeros
			for (int y = vlc.get(4) + 4; y--;)
				lens[x++] = 0;

		} else if (z == 18) {
			// code = 18, run of ([read 5 bits]+20) zeros
			for (int y = vlc.get(5) + 20; y--;)
				lens[x++] = 0;

		} else if (z == 19) {
			// code = 19, run of ([read 1 bit]+4) [read huffman symbol]
			int	y = vlc.get(1) + 4;
			int	z = ::read_huffman(vlc.peek<uint16>(), PRETREE_table, PRETREE_TABLEBITS, PRETREE_MAXSYMBOLS);
			vlc.discard(PRETREE_len[z]);

			z = lens[x] - z;
			if (z < 0)
				z += 17;
			while (y--)
				lens[x++] = z;

		} else {
			// code = 0 to 16, delta current length entry
			z = lens[x] - z;
			if (z < 0)
				z += 17;
			lens[x++] = z;
		}
	}

	return true;
}

// LZX static data tables:
static uint8	extra_bits[51] = {
	 0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6, 
	 7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
	15, 15, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17
};
static uint32	position_base[51] = {
      0,      1,      2,      3,      4,      6,      8,     12,     16,     24,     32,     48,     64,     96,    128,    192,
    256,    384,    512,    768,   1024,   1536,   2048,   3072,   4096,   6144,   8192,  12288,  16384,  24576,  32768,  49152,
  65536,  98304, 131072, 196608, 262144, 393216, 524288, 655360, 786432, 917504,1048576,1179648,1310720,1441792,1572864,1703936,
1835008,1966080,2097152
};
//for (int i = 0, j = 0; i < 51; i++) {
//	position_base[i] = j;
//	j += 1 << extra_bits[i];
//}

static const unsigned int position_slots[11] = {
	30, 32, 34, 36, 38, 42, 50, 66, 98, 162, 290
};

void LZX_decoder::reset_state() {
	header_read		= false;
	block_remaining = 0;
	block_type		= BLOCKTYPE_INVALID;

	// initialise tables to 0 (because deltas will be applied to them)
	for (int i = 0; i < MAINTREE_MAXSYMBOLS; i++)
		MAINTREE_len[i] = 0;
	for (int i = 0; i < LENGTH_MAXSYMBOLS; i++)
		LENGTH_len[i]	= 0;
}

LZX_decoder::LZX_decoder(int window_bits, size_t length, uint64 _reset_interval) : window(1 << window_bits), reset_interval(_reset_interval), offset(0), length(length) {
	intel_filesize	= 0;
	intel_started	= false;

	num_offsets		= position_slots[window_bits - 15] << 3;
	R0 = R1 = R2	= 1;

	reset_state();
}

uint8 *LZX_decoder::process(uint8 *dst, uint8 *dst_end, reader_intf file, TRANSCODE_FLAGS flags) {
	// load local state
	auto	vlc			= make_vlc_in(copy(file), vlc0);
	uint8	*window		= this->window;
	uint32	window_size	= this->window.size32();
	uint32	R0			= this->R0;
	uint32	R1			= this->R1;
	uint32	R2			= this->R2;

	while (dst < dst_end) {
		uint32	frame_posn	= offset & (window_size - FRAME_SIZE);
		size_t	frame_end	= min((offset & -FRAME_SIZE) + FRAME_SIZE, length);
		int		frame_remaining	= frame_end - offset;
		
		uint32	window_posn	= offset & (window_size - 1);

		// start of frame?
		if ((offset & FRAME_SIZE - 1) == 0) {
			// have we reached the reset interval? (if there is one?)
			uint32	frame	= offset / FRAME_SIZE;
			if (reset_interval && ((frame % reset_interval) == 0)) {
				ISO_ASSERT(block_remaining == 0);
				reset_state();
				R0 = R1 = R2	= 1;
			}

			// read header if necessary
			if (!header_read) {
				// read 1 bit; if set read intel filesize (32 bits)
				intel_filesize	= 0;
				if (vlc.get_bit()) {
					auto	hi = vlc.get(16), lo = vlc.get(16);
					intel_filesize = lo + (hi << 16);
				}
				header_read		= true;
			}
		}

		// decode until one more frame is available
		while (frame_remaining > 0) {
			// initialise new block, if one is needed
			if (block_remaining == 0) {
				// realign if previous block was an odd-sized UNCOMPRESSED block
				if (block_type == BLOCKTYPE_UNCOMPRESSED && (block_length & 1))
					vlc.discard(8);

				// read block type (3 bits) and block length (24 bits)
				block_type		= (BLOCKTYPE)vlc.get(3);
				auto	hi		= vlc.get(8), lo = vlc.get(16);
				block_remaining = block_length = (hi << 16) | lo;

				// read individual block headers
				switch (block_type) {
					case BLOCKTYPE_ALIGNED:
						// read lengths of and build aligned huffman decoding tree
						for (int i = 0; i < 8; i++)
							ALIGNED_len[i] = vlc.get(3);

						if (!make_decode_table(ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, ALIGNED_len, ALIGNED_table))
							return nullptr;
						// fall through

					case BLOCKTYPE_VERBATIM:
						// read lengths of and build main huffman decoding tree
						if (!read_lens(vlc, MAINTREE_len, 0, 256))
							return nullptr;

						if (!read_lens(vlc, MAINTREE_len, 256, NUM_CHARS + num_offsets))
							return nullptr;

						if (!make_decode_table(MAINTREE_MAXSYMBOLS,MAINTREE_TABLEBITS, MAINTREE_len, MAINTREE_table))
							return nullptr;

						// if the literal 0xE8 is anywhere in the block...
						if (MAINTREE_len[0xE8] != 0)
							intel_started = true;

						// read lengths of and build lengths huffman decoding tree
						if (!read_lens(vlc, LENGTH_len, 0, NUM_SECONDARY_LENGTHS))
							return nullptr;

						if (!make_decode_table(LENGTH_MAXSYMBOLS,LENGTH_TABLEBITS, LENGTH_len,LENGTH_table))
							return nullptr;
						break;

					case BLOCKTYPE_UNCOMPRESSED:
						// because we can't assume otherwise
						intel_started = true;
						// read 12 bytes of stored R0 / R1 / R2 values
						vlc.restore_unused();
						R0 = file.get<uint32le>();
						R1 = file.get<uint32le>();
						R2 = file.get<uint32le>();
						break;

					default:
						return nullptr;
				}
			}

			// decode more of the block
			int	this_run	= min(block_remaining, frame_remaining);

			// assume we decode exactly this_run bytes, for now
			frame_remaining	-= this_run;
			block_remaining -= this_run;

			// decode at least this_run bytes
			switch (block_type) {
				case BLOCKTYPE_VERBATIM:
					while (this_run > 0) {
						int	main_element = read_huffman(vlc, MAINTREE_table, MAINTREE_len, MAINTREE_TABLEBITS, MAINTREE_MAXSYMBOLS);
						if (main_element < NUM_CHARS) {
							// literal: 0 to NUM_CHARS-1
							window[window_posn++] = main_element;
							this_run--;

						} else {
							// match: NUM_CHARS + ((slot<<3) | length_header (3 bits))
							main_element -= NUM_CHARS;

							// get match length
							int	match_length = main_element & NUM_PRIMARY_LENGTHS;
							if (match_length == NUM_PRIMARY_LENGTHS)
								match_length += read_huffman(vlc, LENGTH_table, LENGTH_len, LENGTH_TABLEBITS, LENGTH_MAXSYMBOLS);

							match_length += MIN_MATCH;

							// get match offset
							uint32	match_offset = main_element >> 3;
							switch (match_offset) {
								case 0: match_offset = R0;	break;
								case 1: match_offset = R1;	R1 = R0; R0 = match_offset; break;
								case 2: match_offset = R2;	R2 = R0; R0 = match_offset; break;
								case 3: match_offset = 1;	R2 = R1; R1 = R0; R0 = match_offset; break;
								default: {
									match_offset = position_base[match_offset] - 2 + vlc.get(extra_bits[match_offset]);
									R2 = R1;
									R1 = R0;
									R0 = match_offset;
								}
							}

							if ((window_posn + match_length) > window_size)
								return nullptr;

							// copy match
							uint8	*rundest	= window + window_posn;
							int		i			= match_length;
							// does match offset wrap the window?
							if (match_offset > window_posn) {
								// j = length from match offset to end of window
								int	j = match_offset - window_posn;
								if (j > (int)window_size)
									return nullptr;
								uint8	*runsrc = window + window_size - j;
								if (j < i) {
									// if match goes over the window edge, do two copy runs
									i -= j;
									while (j-- > 0)
										*rundest++ = *runsrc++;
									runsrc = window;
								}
								while (i-- > 0)
									*rundest++ = *runsrc++;
							} else {
								uint8	*runsrc = rundest - match_offset;
								while (i-- > 0)
									*rundest++ = *runsrc++;
							}

							this_run	-= match_length;
							window_posn += match_length;
						}
					}
					break;

				case BLOCKTYPE_ALIGNED:
					while (this_run > 0) {
						int	main_element = read_huffman(vlc, MAINTREE_table, MAINTREE_len, MAINTREE_TABLEBITS, MAINTREE_MAXSYMBOLS);
						if (main_element < NUM_CHARS) {
							// literal: 0 to NUM_CHARS-1
							window[window_posn++] = main_element;
							this_run--;

						} else {
							// match: NUM_CHARS + ((slot<<3) | length_header (3 bits))
							main_element -= NUM_CHARS;

							// get match length
							int	match_length = main_element & NUM_PRIMARY_LENGTHS;
							if (match_length == NUM_PRIMARY_LENGTHS)
								match_length += read_huffman(vlc, LENGTH_table, LENGTH_len, LENGTH_TABLEBITS, LENGTH_MAXSYMBOLS);

							match_length += MIN_MATCH;

							// get match offset
							uint32	match_offset = main_element >> 3;
							switch (match_offset) {
								case 0: match_offset = R0;	break;
								case 1: match_offset = R1; R1 = R0; R0 = match_offset; break;
								case 2: match_offset = R2; R2 = R0; R0 = match_offset; break;
								default: {
									int	extra = extra_bits[match_offset];
									match_offset = position_base[match_offset] - 2;
									if (extra > 3) {
										// verbatim and aligned bits
										extra -= 3;
										match_offset += vlc.get(extra) << 3;
										match_offset += read_huffman(vlc, ALIGNED_table, ALIGNED_len, ALIGNED_TABLEBITS, ALIGNED_MAXSYMBOLS);

									} else if (extra == 3) {
										// aligned bits only
										match_offset += read_huffman(vlc, ALIGNED_table, ALIGNED_len, ALIGNED_TABLEBITS, ALIGNED_MAXSYMBOLS);

									} else if (extra > 0) { // extra==1, extra==2
										// verbatim bits only
										match_offset += vlc.get(extra);;

									} else {// extra == 0 {
										// ??? not defined in LZX specification!
										match_offset = 1;
									}
									// update repeated offset LRU queue
									R2 = R1;
									R1 = R0;
									R0 = match_offset;
								}
							}

							if (window_posn + match_length > window_size)
								return nullptr;

							// copy match
							uint8	*rundest	= window + window_posn;
							int		i			= match_length;
							// does match offset wrap the window?
							if (match_offset > window_posn) {
								// j = length from match offset to end of window
								int	j = match_offset - window_posn;
								if (j > (int)window_size)
									return nullptr;

								uint8	*runsrc = window + window_size - j;
								if (j < i) {
									// if match goes over the window edge, do two copy runs
									i -= j;
									while (j-- > 0)
										*rundest++ = *runsrc++;
									runsrc = window;
								}
								while (i-- > 0)
									*rundest++ = *runsrc++;
							} else {
								uint8	*runsrc = rundest - match_offset;
								while (i-- > 0)
									*rundest++ = *runsrc++;
							}

							this_run	-= match_length;
							window_posn += match_length;
						}
					}
					break;

				case BLOCKTYPE_UNCOMPRESSED:
					// as this_run is limited not to wrap a frame, this also means it won't wrap the window (as the window is a multiple of 32k)
					file.readbuff(window + window_posn, this_run);
					window_posn		+= this_run;
					this_run		= 0;
					break;

				default:
					return nullptr; // might as well
			}

			// did the final match overrun our desired this_run length?
			if (this_run < 0) {
				if (-this_run > (int)block_remaining)
					return nullptr;
				block_remaining += this_run;
			}
		} // while (frame_remaining > 0)

		// streams don't extend over frame boundaries
		uint32	frame_size	= window_posn - frame_posn;
		if (frame_size != FRAME_SIZE && offset + frame_size != length)
			return nullptr;

		// re-align input bitstream
		vlc.align(16);

		// copy out a frame
		memcpy(dst, window + frame_posn, frame_size);

		// does this intel block _really_ need decoding?
		if (intel_started && intel_filesize && frame_size > 10) {
			for (uint8 *i = dst, *end = i + frame_size - 10; i < end;) {
				if (*i++ == 0xE8) {
					int	curpos	= offset + (i - dst);
					int	abs_off = load_packed<int32le>(i);
					store_packed<int32le>(i, 
						abs_off < -curpos || abs_off >= intel_filesize ? abs_off
						: abs_off >= 0 ? abs_off - curpos
						: abs_off + intel_filesize
					);
					i	+= 4;
				}
			}
		}
		dst			+= frame_size;
		offset		+= frame_size;
	}

	// save local state
	vlc0		= vlc.get_state();
	this->R0	= R0;
	this->R1	= R1;
	this->R2	= R2;

	return dst;
}
