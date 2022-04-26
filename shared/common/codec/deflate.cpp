#include "deflate.h"
#include "hashes/simple.h"
#include "base/algorithm.h"
#include "vlc.h"

using namespace iso;

#define MAXD	592

// permutation of code lengths
const uint8 deflate_base::order[19] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// base for length codes 256..285 base
const uint16 deflate_base::len_base[] = {
	0,		3,		4,		5,		6,		7,		8,		9,
	10,		11,		13,		15,		17,		19,		23,		27,
	31,		35,		43,		51,		59,		67,		83,		99,
	115,	131,	163,	195,	227,	258
};
// extra bits for length codes 256..285
const uint8 deflate_base::len_extra[] = {
	0,		0,		0,		0,		0,		0,		0,		0,
	0,		1,		1,		1,		1,		2,		2,		2,
	2,		3,		3,		3,		3,		4,		4,		4,
	4,		5,		5,		5,		5
};

// base for distance codes 0..29
const uint16 deflate_base::dist_base[] = {
	1,		2,		3,		4,		5,		7,		9,		13,
	17,		25,		33,		49,		65,		97,		129,	193,
	257,	385,	513,	769,	1025,	1537,	2049,	3073,
	4097,	6145,	8193,	12289,	16385,	24577
};
// extra bits for distance codes 0..29
const uint8 deflate_base::dist_extra[] = {
	0,		0,		0,		0,		1,		1,		2,		2,
	3,		3,		4,		4,		5,		5,		6,		6,
	7,		7,		8,		8,		9,		9,		10,		10,
	11,		11,		12,		12,		13,		13
};

// extra bits for bit length codes
const uint8 deflate_base::bl_extra[] = {
	0,		0,		0,		0,		0,		0,		0,		0,
	0,		0,		0,		0,		0,		0,		0,		0,
	2,		3,		7
};

// size of length codes in static tree
const uint8 deflate_base::static_len_bits[] = {
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x00
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x10
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x20
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x30
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x40
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x50
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x60
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x70
	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	8,	//0x80
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0x90
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0xa0
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0xb0
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0xc0
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0xd0
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0xe0
	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	9,	//0xf0
	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	7,	//0x100
	7,	7,	7,	7,	7,	7,	7,	7,	8,	8,	8,	8,	8,	8,	8,	8	//0x110
};

// size of distance codes in static tree
const uint8 deflate_base::static_dist_bits[] = {
	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,
	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5,	5
};

//-----------------------------------------------------------------------------
//  deflate_decoder
//-----------------------------------------------------------------------------

const uint8 deflate_decoder::len_ops[32] = { // Length codes 256..285 extra
	0x60,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	0x10,	0x11,	0x11,	0x11,	0x11,	0x12,	0x12,	0x12,
	0x12,	0x13,	0x13,	0x13,	0x13,	0x14,	0x14,	0x14,
	0x14,	0x15,	0x15,	0x15,	0x15,	0x10,	0x40,	0x40,
};
const uint8 deflate_decoder::dist_ops[32] = { // Distance codes 0..29 extra
	0x10,	0x10,	0x10,	0x10,	0x11,	0x11,	0x12,	0x12,
	0x13,	0x13,	0x14,	0x14,	0x15,	0x15,	0x16,	0x16,
	0x17,	0x17,	0x18,	0x18,	0x19,	0x19,	0x1a,	0x1a,
	0x1b,	0x1b,	0x1c,	0x1c,	0x1d,	0x1d,	0x40,	0x40,
};

deflate_decoder::code deflate_decoder::static_len[512];
deflate_decoder::code deflate_decoder::static_dist[32];

int deflate_decoder::inflate_table(const uint8 *lens, uint32 codes, code *table, uint32 &bits, uint16 *work, int max_entries, const uint16 *base, const uint8 *extra_bits, int end) {
	code	c;						// table entry for duplication
	uint16	count[MAX_BITS + 1];	// number of codes of each length
	uint16	offsets[MAX_BITS + 1];	// offsets in table for each length

	// accumulate lengths for codes (assumes lens[] all in 0..MAX_BITS)
	for (int len = 0; len <= MAX_BITS; len++)
		count[len] = 0;
	for (int sym = 0; sym < codes; sym++)
		count[lens[sym]]++;

	// bound code lengths, force root to be within code lengths
	uint32	max = MAX_BITS;
	while (max && count[max] == 0)
		--max;

	if (max == 0) {				// no symbols to code at all
		c.op		= 64;		// invalid code marker
		c.bits		= 1;
		c.val		= 0;
		table[0]	= c;		// make a table to force an error
		table[1]	= c;
		bits		= 1;
		return 2;				// no symbols, but wait for decoding to report error
	}

	uint32	min = 1;
	while (min <= MAX_BITS && count[min] == 0)
		++min;


	// check for an over-subscribed or incomplete set of lengths
	int left = 1;
	for (int len = 1; len <= MAX_BITS; len++) {
		left = (left << 1) - count[len];
		if (left < 0)
			return -1;	// over-subscribed
	}

	// generate offsets into symbol table for each length for sorting
	uint32	total = 0;
	for (int len = 1; len <= MAX_BITS; len++) {
		offsets[len] = total;
		total	+= count[len];
	}
	if (total > max_entries)
		return 0;

	// sort symbols by length, by symbol order within each length
	for (int sym = 0; sym < codes; sym++) {
		if (lens[sym])
			work[offsets[lens[sym]]++] = sym;
	}

#if 0
	// initialize state for loop
	uint32	root	= bits < min ? min : bits > max ? max : bits;
	code	*next	= table;		// current table to fill in
	uint32	curr	= root;			// current table index bits
	uint32	drop	= 0;			// current bits to drop from code for index
	uint32	low		= uint32(-1);	// trigger new sub-table when len > root
	uint32	used	= 1 << root;	// use root table entries
	uint32	mask	= used - 1;		// mask for comparing low

	// check available table space
	if (used > max_entries)
		return 0;

	// process all codes and make table entries
	for (uint32 i = 0; i < offsets[max - 1]; ++i) {
		uint32	sym	= work[i];
		uint32	len = lens[sym];

		// create table entry
		c.bits = uint8(len - drop);
		if (sym < end) {
			c.op	= 0;
			c.val	= sym;
		} else {
			c.op	= extra_bits[sym - end];
			c.val	= base[sym - end];
		}

		uint32	huff	= reverse_bits(i, len);

		// replicate for those indices with low len bits equal to huff
		uint32	incr = 1 << (len - drop);
		uint32	fill = 1 << curr;
		min = fill;	// save offset to next table
		do {
			fill -= incr;
			next[(huff >> drop) + fill] = c;
		} while (fill != 0);

		// go to next symbol, update count, len

		// create new sub-table if needed
		if (len > root && (huff & mask) != low) {
			// if first time, transition to sub-tables
			if (drop == 0)
				drop = root;

			// increment past last table
			next += min;	// here min is 1 << curr

			// determine length of next table
			curr = len - drop;
			left = 1 << curr;
			while (curr + drop < max) {
				left -= count[curr + drop];
				if (left <= 0)
					break;
				curr++;
				left <<= 1;
			}

			// check for enough space
			used += 1 << curr;
			if (used >= max_entries)
				return 0;

			// point entry in root table to sub-table
			low = huff & mask;
			table[low].op	= (uint8)curr;
			table[low].bits	= (uint8)root;
			table[low].val	= (uint16)(next - table);
		}
	}
	uint32	len = max;
	c.op	= 64;	// invalid code marker
	c.bits	= uint8(len - drop);
	c.val	= 0;
	for (uint32 i = total; i < max_entries; i++) {
		uint32	huff	= reverse_bits(i, len);

		// when done with sub-table, drop back to root table
		if (drop && (huff & mask) != low) {
			drop	= 0;
			next	= table;
			c.bits	= len = root;
		}

		// put invalid code marker in table
		next[huff >> drop] = c;
	}
#else
	// initialize state for loop
	uint32	root	= bits < min ? min : bits > max ? max : bits;
	uint32	huff	= 0;			// starting code
	uint32	sym		= 0;			// starting code symbol
	uint32	len		= min;			// starting code length
	code	*next	= table;		// current table to fill in
	uint32	curr	= root;			// current table index bits
	uint32	drop	= 0;			// current bits to drop from code for index
	uint32	low		= uint32(-1);	// trigger new sub-table when len > root
	uint32	used	= 1 << root;	// use root table entries
	uint32	mask	= used - 1;		// mask for comparing low

									// check available table space
	if (used > max_entries)
		return 0;

	// process all codes and make table entries
	for (;;) {
		// create table entry
		c.bits = uint8(len - drop);
		if (work[sym] < end) {
			c.op	= 0;
			c.val	= work[sym];
		} else {
			c.op	= extra_bits[work[sym] - end];
			c.val	= base[work[sym] - end];
		}

		// replicate for those indices with low len bits equal to huff
		uint32	incr = 1 << (len - drop);
		uint32	fill = 1 << curr;
		do {
			fill -= incr;
			next[(huff >> drop) + fill] = c;
		} while (fill != 0);

		// backwards increment the len-bit code huff
		incr = 1 << (len - 1);
		while (huff & incr)
			incr >>= 1;
		huff = incr ? (huff & (incr - 1)) + incr : 0;

		// go to next symbol, update count, len
		sym++;

		if (--count[len] == 0) {
			if (len == max)
				break;
			len = lens[work[sym]];
		}

		// create new sub-table if needed
		if (len > root && (huff & mask) != low) {
			// if first time, transition to sub-tables
			if (drop == 0)
				drop = root;

			// increment past last table
			next += 1 << curr;

			// determine length of next table
			curr = len - drop;
			left = 1 << curr;
			while (curr + drop < max) {
				left -= count[curr + drop];
				if (left <= 0)
					break;
				curr++;
				left <<= 1;
			}

			// check for enough space
			used += 1 << curr;
			if (used >= max_entries)
				return 0;

			// point entry in root table to sub-table
			low = huff & mask;
			table[low].op	= (uint8)curr;
			table[low].bits	= (uint8)root;
			table[low].val	= (uint16)(next - table);
		}
	}
	c.op	= 64;	// invalid code marker
	c.bits	= uint8(len - drop);
	c.val	= 0;
	while (huff) {
		// when done with sub-table, drop back to root table
		if (drop && (huff & mask) != low) {
			drop	= 0;
			len		= root;
			next	= table;
			c.bits	= (uint8)len;
		}

		// put invalid code marker in table
		next[huff >> drop] = c;

		// backwards increment the len-bit code huff
		uint32	incr = 1 << (len - 1);
		while (huff & incr)
			incr >>= 1;
		huff = incr ? (huff & (incr - 1)) + incr : 0;
	}
#endif

	// set return parameters
	bits	= root;
	return used;
}

bool deflate_decoder::make_static_tables() {
	uint16	work[288];		// work area for code table building

	clear(static_len);
	uint32	lenbits		= 9;
	inflate_table(static_len_bits, num_elements(static_len_bits), static_len, lenbits, work, num_elements(static_len), len_base, len_ops, 256);

	uint32	distbits	= 5;
	inflate_table(static_dist_bits, num_elements(static_dist_bits), static_dist, distbits, work, num_elements(static_dist), dist_base, dist_ops, 0);

	return true;
}

template<typename WIN> uint8 *deflate_decoder::process1(VLC &vlc, uint8 *dst, uint8 *dst_end, WIN win) {

	for (;;) switch (mode) {
		case LIT:
			if (dst == dst_end)
				return dst;
			*dst++	= uint8(L);
			mode	= LEN;

		//fall through
		case LEN: {
			code	c = lencode[vlc.peek(lenbits)];
			if (c.op && !(c.op & 0xf0)) {
				vlc.discard(c.bits);
				c = lencode[c.val + vlc.peek(c.op)];
			}
			vlc.discard(c.bits);
			L = c.val;
			if (c.op == 0) {
				mode = LIT;
				break;
			}
			if (c.op & 32) {
				mode =	BLOCK;
				return dst;
			}
			if (c.op & 64) {
				mode = BAD;
				break;
			}
			if (uint32 extra = c.op & 15)
				L += vlc.get(extra);

			c = distcode[vlc.peek(distbits)];
			if ((c.op & 0xf0) == 0) {
				vlc.discard(c.bits);
				c = distcode[c.val + vlc.peek(c.op)];
			}
			vlc.discard(c.bits);
			if (c.op & 64) {
				mode = BAD;
				break;
			}
			D	= c.val;
			if (uint32 extra = c.op & 15)
				D += vlc.get(extra);

			if (win.check_low_limit(dst - D)) {
				mode = BAD;
				break;
			}
			mode = MATCH;
		}
		//fall through
		case MATCH: {
			if (dst == dst_end)
				return dst;

			auto	end = dst + L;
			L	= win.copy(dst, dst - D, end, dst_end);
			dst = end - L;

			if (L == 0)
				mode = LEN;
			break;
		}

		default:
			ISO_ASSERT(0);
			return dst;
	}
}

uint8 *deflate_decoder::process(uint8* dst, uint8 *dst_end, istream_ref file, TRANSCODE_FLAGS flags) {
	auto	vlc		= make_vlc_in(file, vlc0);
	uint8*	dst0	= dst;

	if (mode > BLOCK) {
		if (mode == COPY) {
			uint32	n = min(L, dst_end - dst);
			file.readbuff(dst, n);
			dst		+= n;
			if (L -= n)
				return dst;

			mode = BLOCK;
		} else {
			dst = process1(vlc, dst, dst_end, external_window(dst0 - offset, window));
		}
	}

	while (mode == BLOCK && !last) {
		last = vlc.get_bit();
		switch (vlc.get(2)) {
			case STORED_BLOCK: {
				vlc.restore_unused();
				uint32	v	= file.get<uint32>();
				uint32	n	= v & 0xffff;
				if (n != ((v >> 16) ^ 0xffff)) {
					mode	= BAD;
					return dst;
				}
				uint32	n1	= min(n, dst_end - dst);
				file.readbuff(dst, n1);
				dst	+= n1;

				if (L = n - n1) {
					mode	= COPY;
					return dst;
				}
				continue;
			}
			case STATIC_TREES: {
				lencode		= static_len;
				lenbits		= 9;
				distcode	= static_dist;
				distbits	= 5;
				mode		= LEN;
				break;
			}
			case DYN_TREES: {
				uint8	lens[320];		// temporary storage for code lengths
				uint16	work[288];		// work area for code table building
				uint32	nlen	= vlc.get(5) + 257;
				uint32	ndist	= vlc.get(5) + 1;
				uint32	ncode	= vlc.get(4) + 4;

				for (int i = 0; i < ncode; i++)
					lens[order[i]] = vlc.get(3);
				for (int i = ncode; i < BL_CODES; i++)
					lens[order[i]] = 0;

				// build CODES table
				uint32	codebits	= 7;
				if (inflate_table(lens, BL_CODES, codes, codebits, work, 2048, 0, 0, BL_CODES) > 0) {
					uint16	len;
					for (int i = 0; i < nlen + ndist;) {
						code	c = codes[vlc.peek(codebits)];
						vlc.discard(c.bits);

						if (c.val < 16) {
							lens[i++] = len = c.val;

						} else {
							if (c.val == REP_3_6) {
								if (i == 0)
									return nullptr;
							} else {
								len = 0;
							}

							uint32	copy = c.val == REP_3_6 ? 3 + vlc.get(2) : c.val == REPZ_3_10 ? 3 + vlc.get(3) : 11 + vlc.get(7);
							if (i + copy > nlen + ndist)
								return nullptr;

							while (copy--)
								lens[i++] = len;
						}
					}

					// build LENS table
					lencode		= codes;
					lenbits		= 9;
					int		ret	= inflate_table(lens, nlen, codes, lenbits, work, 2048 - MAXD, len_base, len_ops, 256);

					if (ret > 0) {
						// build DISTS table
						distcode	= codes + ret;
						distbits	= 6;
						if (inflate_table(lens + nlen, ndist, unconst(distcode), distbits, work, MAXD, dist_base, dist_ops, 0) > 0) {
							mode = LEN;
							break;
						}
					}
				}
			}
			// fall through
			case 3:
				mode = BAD;
				return dst;
		}

		dst = process1(vlc, dst, dst_end, external_window(dst0 - offset, window));
	}

	vlc0 = vlc.get_state();

	size_t	written = dst - dst0;
	offset	+= written;

	// copy wsize or less output bytes into the circular window
	if (written && (mode > BLOCK || !last))
		extend_window(window, written, 1 << wbits);

	return dst;
}


//-----------------------------------------------------------------------------
//  deflate_encoder
//-----------------------------------------------------------------------------
/*
const uint8 _dist_code[512] = {
	0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,
	8,  8,  8,  8,	8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,
	10, 10, 10, 10, 10, 10, 10, 10,	10, 10, 10, 10, 10, 10, 10, 10,
	11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,	11, 11, 11, 11,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
	13, 13, 13, 13,	13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
	13, 13, 13, 13, 13, 13, 13, 13,	13, 13, 13, 13, 13, 13, 13, 13,
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,	14, 14, 14, 14,
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	14, 14, 14, 14,	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	15, 15, 15, 15, 15, 15, 15, 15,	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,	15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
	
	0,  0,  16, 17,	18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21,
	22, 22, 22, 22, 22, 22, 22, 22,	23, 23, 23, 23, 23, 23, 23, 23,
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,	24, 24, 24, 24,
	25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
	26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
	26, 26, 26, 26,	26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
	27, 27, 27, 27, 27, 27, 27, 27,	27, 27, 27, 27, 27, 27, 27, 27,
	27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,	27, 27, 27, 27,
	28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
	28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
	28, 28, 28, 28,	28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
	28, 28, 28, 28, 28, 28, 28, 28,	28, 28, 28, 28, 28, 28, 28, 28,
	29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,	29, 29, 29, 29,
	29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
	29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
	29, 29, 29, 29,	29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29
};

const uint8 deflate_encoder::_length_code[256] = {
	0,  1,  2,  3,  4,  5,  6,  7,  8,  8,  9,  9,  10, 10, 11, 11,
	12, 12, 12, 12,	13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15,
	16, 16, 16, 16, 16, 16, 16, 16,	17, 17, 17, 17, 17, 17, 17, 17,
	18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,	19, 19, 19, 19,
	20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
	22, 22, 22, 22,	22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
	23, 23, 23, 23, 23, 23, 23, 23,	23, 23, 23, 23, 23, 23, 23, 23,
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,	24, 24, 24, 24,
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
	25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
	25, 25, 25, 25,	25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
	26, 26, 26, 26, 26, 26, 26, 26,	26, 26, 26, 26, 26, 26, 26, 26,
	26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,	26, 26, 26, 26,
	27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
	27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28
};
*/
constexpr uint8 deflate_encoder::dist_code(uint32 dist) {
	if (dist < 3)
		return dist - 1;
	auto	i	= highest_set_index(dist - 1);
	return i * 2 - 2 + ((dist - 1) >> (i - 1));
}

constexpr uint8 deflate_encoder::length_code(uint32 len) {
	if (len < 8)
		return len + 1;
	auto	i	= highest_set_index(len);
	return i * 4 - 7 + (len >> (i - 2));
}
/*
static struct test_dist_code {
	test_dist_code() {
		for (int dist = 0; dist < 0x8000; dist++) {
			auto	c0 = dist <= 256 ? _dist_code[dist - 1] : _dist_code[256 + ((dist - 1) >> 7)];
			auto	c1	= deflate_encoder::dist_code(dist);
			ISO_ASSERT(c0 == c1);
		}
		for (int dist = 0; dist < 0xff; dist++) {
			auto	c0 = deflate_encoder::_length_code[dist];
			auto	c1	= length_code2(dist);
			ISO_ASSERT(c0 == c1);
		}
	}
} _test;
*/

const deflate_encoder::TreeDesc  deflate_encoder::len_desc	= {len_extra,	deflate_encoder::LITERALS,	deflate_encoder::L_CODES,	deflate_encoder::MAX_BITS};
const deflate_encoder::TreeDesc  deflate_encoder::dist_desc	= {dist_extra,	0,							deflate_encoder::D_CODES,	deflate_encoder::MAX_BITS};
const deflate_encoder::TreeDesc  deflate_encoder::bl_desc	= {bl_extra,	0,							deflate_encoder::BL_CODES,	deflate_encoder::MAX_BL_BITS};

deflate_encoder::ct_data deflate_encoder::static_len_tree[deflate_encoder::L_CODES_STATIC];
deflate_encoder::ct_data deflate_encoder::static_dist_tree[deflate_encoder::D_CODES];

const deflate_encoder::SearchParams deflate_encoder::configuration_table[] = {
	// store only
	/* 0 */ {deflate_encoder::STORED,	false, 0,    0,  0,     0},
	// max speed, no lazy matches
	/* 1 */ {deflate_encoder::FAST,		false, 4,    4,  8,     4},
	/* 2 */ {deflate_encoder::FAST,		false, 4,    5, 16,     8},
	/* 3 */ {deflate_encoder::FAST,		false, 4,    6, 32,    32},
	// lazy matches
	/* 4 */ {deflate_encoder::SLOW,		false, 4,    4, 16,    16},
	/* 5 */ {deflate_encoder::SLOW,		false, 8,   16, 32,    32},
	/* 6 */ {deflate_encoder::SLOW,		false, 8,   16, 128,  128},
	/* 7 */ {deflate_encoder::SLOW,		false, 8,   32, 128,  256},
	/* 8 */ {deflate_encoder::SLOW,		false, 32, 128, 258, 1024},
	/* 9 */ {deflate_encoder::SLOW,		false, 32, 258, 258, 4096}		// max compression
};

//-----------------------------------------------------------------------------
//	Tree functions
//-----------------------------------------------------------------------------

void deflate_encoder::TreeDesc::gen_codes(ct_data *tree, int max_code, const uint16 *counts) {
	uint16 next_code[MAX_BITS + 1];

	// The distribution counts are first used to generate the code values without bit reversal
	for (int bits = 1, code = 0; bits <= MAX_BITS; bits++)
		next_code[bits] = code = (code + counts[bits - 1]) << 1;

	// Check that the bit counts in counts are consistent. The last code must be all ones
	for (int n = 0;  n <= max_code; n++) {
		if (int len = tree[n].len)
			tree[n].code = reverse_bits(next_code[len]++, len);
	}
}

uint32 deflate_encoder::TreeDesc::total_len(const ct_data* tree) const {
	uint32	total = 0;
	for (int n = 0; n < num_elems; n++) {
		int	xbits	= n >= extra_base ? extra_bits[n - extra_base] : 0;
		total += tree[n].freq * (tree[n].len + xbits);
	}
	return total;
}

// Construct one Huffman tree and assign the code bit strings and lengths
// Update the total bit length for the current block
// IN assertion: the field freq is set for all tree elements
// OUT assertions: the fields len and code are set to the optimal bit length and corresponding code
// returns max_code

int deflate_encoder::TreeDesc::build_tree(ct_data* tree, uint32 &len) const {
	static const int	HEAP_SIZE	= L_CODES * 2 + 1;	// maximum heap size

	int		heap[HEAP_SIZE];	// heap used to build the Huffman trees
	int		heap_len	= 0;
	int		heap_max	= HEAP_SIZE;
	int		max_code	= -1;	// largest code with non zero frequency

	for (int n = 0; n < num_elems; n++) {
		if (tree[n].freq) {
			heap[heap_len++] = max_code = n;
			tree[n].depth = 0;
		} else {
			tree[n].len = 0;
		}
	}

	// The pkzip format requires that at least one distance code exists, and that at least one bit should be sent even if there is only one possible code
	// To avoid special checks later on, force at least two codes of non zero frequency
	while (heap_len < 2) {
		int	n = heap[heap_len++] = max_code < 2 ? ++max_code : 0;
		tree[n].freq	= 1;
		tree[n].depth	= 0;
	}

	// The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree, establish sub-heaps of increasing lengths:
	auto	heap_begin = make_indexed_iterator(tree, begin(heap));
	heap_make(heap_begin, heap_begin + heap_len);

	// Construct the Huffman tree by repeatedly combining the least two frequent nodes
	uint16	node	= num_elems;	// next internal node of the tree
	do {
		int	n = heap[0];			// n = node of next least frequency

		heap[0] = heap[--heap_len];
		heap_siftdown(heap_begin, heap_begin + heap_len, heap_begin);

		int	m = heap[0];			// m = node of next least frequency

		heap[--heap_max] = n;
		heap[--heap_max] = m;

		// Create a new node parent of n and m
		tree[node].freq		= tree[n].freq + tree[m].freq;
		tree[node].depth	= max(tree[m].depth, tree[n].depth) + 1;
		tree[n].parent		= tree[m].parent = node;

		// and insert the new node in the heap
		heap[0]				= node++;
		heap_siftdown(heap_begin, heap_begin + heap_len, heap_begin);

	} while (heap_len >= 2);

	heap[--heap_max] = heap[0];

	// Compute the optimal bit lengths for a tree and update the total bit length for the current block

	uint16		counts[MAX_BITS + 1] = {0};		// number of codes at each bit length for an optimal tree

	// In a first pass, compute the optimal bit lengths (which may overflow in the case of the bit length tree)
	tree[heap[heap_max]].len = 0; // root of the heap

	int overflow = 0;				// number of elements with bit length too large
	for (int h = heap_max + 1; h < HEAP_SIZE; h++) {
		int		n		= heap[h];
		uint16	bits	= tree[tree[n].parent].len + 1;
		if (bits > max_length) {
			bits = max_length;
			++overflow;
		}
		tree[n].len = bits;			// overwrite tree[n].parent which is no longer needed

		if (n <= max_code)
			++counts[bits];
	}

	if (overflow) {
		// Find the first bit length which could increase
		while (overflow > 0) {
			int	bits = max_length - 1;
			while (counts[bits] == 0)
				--bits;
			--counts[bits];			// move one leaf down the tree
			--counts[max_length];
			counts[bits + 1] += 2;	// move one overflow item as its brother; the brother of the overflow item also moves one step up, but this does not affect counts[max_length]
			overflow -= 2;
		}

		// recompute all bit lengths, scanning in increasing frequency
		for (int bits = max_length, h = HEAP_SIZE; bits != 0; bits--) {
			for (int n = counts[bits]; n != 0;) {
				int	m = heap[--h];
				if (m <= max_code) {
					tree[m].len = bits;
					n--;
				}
			}
		}
	}

	len += total_len(tree);
	gen_codes(tree, max_code, counts);

	return max_code;
}

// Scan a literal or distance tree to determine the frequencies of the codes in the bit length tree
void deflate_encoder::TreeDesc::scan_tree(ct_data *tree, int max_code, ct_data *bl_tree) {
	int prevlen		= -1;			// last emitted length
	int nextlen		= tree[0].len;	// length of next code
	int count		= 0;			// repeat count of the current code
	int max_count	= 7;			// max repeat count
	int min_count	= 4;			// min repeat count

	if (nextlen == 0) {
		max_count = 138;
		min_count = 3;
	}
	tree[max_code + 1].len = (uint16)0xffff; // guard

	for (int n = 0; n <= max_code; n++) {
		int	curlen = nextlen;
		nextlen = tree[n + 1].len;

		if (++count < max_count && curlen == nextlen) {
			continue;

		} else if (count < min_count) {
			bl_tree[curlen].freq += count;

		} else if (curlen != 0) {
			if (curlen != prevlen)
				++bl_tree[curlen].freq;
			++bl_tree[REP_3_6].freq;

		} else if (count <= 10) {
			++bl_tree[REPZ_3_10].freq;

		} else {
			++bl_tree[REPZ_11_138].freq;
		}

		count = 0;
		prevlen = curlen;
		if (nextlen == 0) {
			max_count = 138;
			min_count = 3;
		} else if (curlen == nextlen) {
			max_count = 6;
			min_count = 3;
		} else {
			max_count = 7;
			min_count = 4;
		}
	}
}

// Send a literal or distance tree in compressed form, using the codes in bl_tree
void deflate_encoder::TreeDesc::send_tree(VLC &vlc, const ct_data *tree, int max_code, const ct_data *bl_tree) {
	int prevlen		= -1;			// last emitted length
	int nextlen		= tree[0].len;	// length of next code
	int count		= 0;			// repeat count of the current code
	int max_count	= 7;			// max repeat count
	int min_count	= 4;			// min repeat count

	if (nextlen == 0) {
		max_count = 138;
		min_count = 3;
	}

	for (int n = 0; n <= max_code; n++) {
		int	curlen = nextlen;
		nextlen = tree[n+1].len;

		if (++count < max_count && curlen == nextlen) {
			continue;

		} else if (count < min_count) {
			while (count--)
				bl_tree[curlen].send(vlc);

		} else if (curlen != 0) {
			if (curlen != prevlen) {
				bl_tree[curlen].send(vlc);
				count--;
			}
			bl_tree[REP_3_6].send(vlc);
			vlc.put(count - 3, 2);

		} else if (count <= 10) {
			bl_tree[REPZ_3_10].send(vlc);
			vlc.put(count - 3, 3);

		} else {
			bl_tree[REPZ_11_138].send(vlc);
			vlc.put(count - 11, 7);
		}

		count = 0; prevlen = curlen;
		if (nextlen == 0) {
			max_count = 138;
			min_count = 3;
		} else if (curlen == nextlen) {
			max_count = 6;
			min_count = 3;
		} else {
			max_count = 7;
			min_count = 4;
		}
	}
}

//-----------------------------------------------------------------------------
//	deflate_encoder
//-----------------------------------------------------------------------------

bool deflate_encoder::make_static_tables() {
	uint16	counts[MAX_BITS + 1];

	clear(counts);
	const uint8	*p = static_len_bits;
	for (auto &t : static_len_tree)
		++counts[t.len = *p++];

	TreeDesc::gen_codes(static_len_tree, num_elements(static_len_tree) - 1, counts);

	clear(counts);
	p = static_dist_bits;
	for (auto &t : static_dist_tree)
		++counts[t.len = *p++];
	
	TreeDesc::gen_codes(static_dist_tree, num_elements(static_dist_tree) - 1, counts);

	return true;
}

deflate_encoder::deflate_encoder(int level, int wbits, int mem_level)
	: wsize(1 << wbits), hash_size(1 << (mem_level + 7)), hash_shift(div_round_up(mem_level + 7, MIN_MATCH)), head(hash_size), prev(wsize)
	, block_start(0), match_length(MIN_MATCH - 1), match_available(false)
	, buf(1 << (mem_level + 6)), num_lits(0)
	, offset(0)
{
	static bool unused = make_static_tables();
	clear_hash();
	params	= configuration_table[level];
}


// Send the block data compressed using the given Huffman trees
void deflate_encoder::compress_block(VLC &vlc, const ct_data *ltree, const ct_data *dtree, entry *buf, uint32 num_lits) {
	for (uint32 i = 0; i < num_lits; ++i) {
		uint16	lc	= buf[i].lc;

		if (uint16 dist = buf[i].dist) {
			uint32	code = length_code(lc);
			ltree[code + LITERALS].send(vlc);						// send the length code
			if (uint8 extra = len_extra[code])
				vlc.put(lc + MIN_MATCH - len_base[code], extra);	// send the extra length bits

			code = dist_code(dist);
			dtree[code].send(vlc);									// send the distance code
			if (uint8 extra = dist_extra[code])
				vlc.put(dist - dist_base[code], extra);				// send the extra distance bits

		} else {
			ltree[lc].send(vlc);									// send a literal byte
		}
	}

	ltree[END_BLOCK].send(vlc);
}


void deflate_encoder::flush_block(VLC &vlc, const uint8* src, const uint8 *base, bool last) {

	const uint8 *start		= base + block_start;
	uint32		store		= src - start;
	uint32		num_lits	= exchange(this->num_lits, 0);
	
	block_start				= src - base;

	// Determine the best encoding for the current block: dynamic trees, static trees or store, and output the encoded block

	if (params.strategy != STORED) {
		ct_data		dyn_ltree[2 * L_CODES + 1];	// literal and length tree
		ct_data		dyn_dtree[2 * D_CODES + 1];	// distance tree

		// calculate frequencies
		for (int n = 0; n < L_CODES; n++) {
			dyn_ltree[n].freq	= 0;
			dyn_ltree[n].len	= static_len_bits[n];
		}
		for (int n = 0; n < D_CODES; n++) {
			dyn_dtree[n].freq	= 0;
			dyn_dtree[n].len	= static_dist_bits[n];
		}

		dyn_ltree[END_BLOCK].freq = 1;

		for (uint32 i = 0; i < num_lits; ++i) {
			uint16	lc	= buf[i].lc;
			if (uint16 dist = buf[i].dist) {
				++dyn_dtree[dist_code(dist)].freq;
				lc = length_code(lc) + LITERALS;
			}
			++dyn_ltree[lc].freq;
		}

		uint32	static_lenb = (len_desc.total_len(dyn_ltree) + dist_desc.total_len(dyn_dtree) + 3 + 7) >> 3;

		if (!params.fixed) {
			// Construct the literal and distance trees
			uint32	opt_len		= 0;
			int		l_max_code = len_desc.build_tree(dyn_ltree, opt_len);
			int		d_max_code = dist_desc.build_tree(dyn_dtree, opt_len);

			// Huffman tree for bit lengths
			ct_data		bl_tree[2 * BL_CODES + 1];
			for (int n = 0; n < BL_CODES; n++)
				bl_tree[n].freq = 0;

			TreeDesc::scan_tree(dyn_ltree, l_max_code, bl_tree);
			TreeDesc::scan_tree(dyn_dtree, d_max_code, bl_tree);

			bl_desc.build_tree(bl_tree, opt_len);
			int		bl_max_index = BL_CODES - 1;
			while (bl_max_index >= 3 && bl_tree[order[bl_max_index]].len == 0)
				--bl_max_index;

			uint32	opt_lenb	= (opt_len + (bl_max_index + 1) * 3 + 5 + 5 + 4 + 3 + 7) >> 3;

			if (opt_lenb < static_lenb && opt_lenb < store + 4) {
				//dynamic trees
				vlc.put((DYN_TREES << 1) + last, 3);
				vlc.put(l_max_code - 256, 5);
				vlc.put(d_max_code, 5);
				vlc.put(bl_max_index - 3, 4);

				// send_bl_tree
				for (int i = 0; i <= bl_max_index; i++)
					vlc.put(bl_tree[order[i]].len, 3);

				TreeDesc::send_tree(vlc, dyn_ltree, l_max_code, bl_tree);
				TreeDesc::send_tree(vlc, dyn_dtree, d_max_code, bl_tree);

				compress_block(vlc, dyn_ltree, dyn_dtree, buf, num_lits);
				return;
			}
		}
		if (static_lenb < store + 4) {
			//static trees
			vlc.put((STATIC_TREES << 1) + last, 3);
			compress_block(vlc, static_len_tree, static_dist_tree, buf, num_lits);
			return;
		}
	}

	//store
	vlc.put((STORED_BLOCK << 1) + last, 3);
	vlc.flush();

	auto	file = vlc.get_stream();
	file.write<uint16le>(store);
	file.write<uint16le>(~store);
	file.writebuff(start, store);
}

const uint8* deflate_encoder::process(ostream_ref file, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	/*
	if (offset > 0x80000000) {
		uint32	delta = offset - wsize;
		offset = wsize;
	//	memcpy(window, window + wsize, wsize);
		match_start -= delta;
		block_start -= delta;

		// Slide the hash table (could be avoided with 32 bit values at the expense of memory usage)
		for (int i = 0; i < hash_size; i++)
			head[i] = head[i] >= delta ? head[i] - delta : 0;

		for (int i = 0; i < wsize; i++)
			prev[i] = prev[i] >= delta ? prev[i] - delta : 0;
	}
	*/
	if (!(flags & TRANSCODE_PARTIAL))
		flags |= TRANSCODE_FLUSH;

	const uint8* src0	= src;
	external_window win(src - offset, window);

	auto	vlc		= make_vlc_out(file, vlc0);

	switch (params.strategy) {
		default:
		case STORED:		src =  deflate_stored	(vlc, src, src_end, flags, win);		break;
		case FAST:			src =  deflate_fast		(vlc, src, src_end, flags, win);		break;
		case SLOW:			src =  deflate_slow		(vlc, src, src_end, flags, win, false);	break;
		case FILTERED:		src =  deflate_slow		(vlc, src, src_end, flags, win, true);	break;
		case HUFFMAN_ONLY:	src =  deflate_huff		(vlc, src, src_end, flags, win);		break;
		case RLE:			src =  deflate_rle		(vlc, src, src_end, flags, win);		break;
	}

	if (src == src_end) {
		if (flags & TRANSCODE_PARTIAL) {
			if (flags & (TRANSCODE_FLUSH | TRANSCODE_ALIGN)) {
				if (num_lits)
					flush_block(vlc, src, win.base, false);

				if (flags & TRANSCODE_FLUSH) {
					vlc.put(STORED_BLOCK<<1, 3);
					vlc.align(8);

					if (flags & TRANSCODE_RESET)
						clear_hash();

				} else {
					vlc.put(STATIC_TREES<<1, 3);
					static_len_tree[END_BLOCK].send(vlc);
					vlc.align(8);
				}
			}
		} else {
			flush_block(vlc, src, win.base, true);
			vlc.flush();
			window = none;
			return src;
		}
	}
	vlc0	= vlc.get_state();

	if (window)
		add_to_window(window, src0, src - src0, wsize);
	else if (flags & TRANSCODE_SRC_VOLATILE)
		window = const_memory_block(src0, src).end(wsize);

	offset = src - win.base;
	return src;
}


// Set match_start to the longest match starting at the given string and return its length
// Matches shorter or equal to prev_length are discarded, in which case the result is equal to prev_length and match_start is garbage
// IN assertions: match_pos is the head of the hash chain for the current string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
// OUT assertion: the match length is not greater than lookahead

void deflate_encoder::longest_match(const uint8 *src, uint32 lookahead, uint32 match_pos, external_window win, uint32 best_len, uint32 nice_len, uint32 max_chain) {
	lookahead	= min(lookahead, MAX_MATCH);
	nice_len	= min(nice_len, lookahead);	// stop if match long enough

	uint32	limit		= src - win.base > MAX_DIST() ? src - win.base - MAX_DIST() : 0;
	uint16	scan_start	= load_packed<uint16>(src);
	uint16	scan_end	= load_packed<uint16>(src + best_len - 1);

	do {
		const uint8	*match = win.base + match_pos;
		if (load_packed<uint16>(win.adjust_ref(match + best_len - 1)) == scan_end && load_packed<uint16>(win.adjust_ref(match)) == scan_start) {
			int	len	= win.match_len(src, match, src + lookahead);
			if (len > best_len) {
				match_start		= match_pos;
				best_len		= len;
				if (len >= nice_len)
					break;

				scan_end = load_packed<uint16>(src + best_len - 1);
			}
		}
		match_pos = prev[match_pos & (wsize - 1)];
	} while (match_pos > limit && --max_chain);

	match_length	= best_len;
}


const uint8* deflate_encoder::deflate_stored(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win) {
	// Copy as much as possible from input to output:
	while (src != src_end) {
		size_t	lookahead = src_end - src;

		// Emit a stored block if pending_buf will be full:
		uint64	max_start = block_start + MAX_BLOCK_SIZE;
		if (lookahead >= max_start) {
			src = win.base + max_start;
			flush_block(vlc, src, win.base, false);
		} else {
			src += lookahead;
		}
	}

	return src;
}

// No lazy evaluation of matches and inserts new strings in the window only for unmatched strings or short matches
const uint8* deflate_encoder::deflate_fast(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win) {
	uint32	h = src_end - src >= MIN_LOOKAHEAD ? update_hash(src[0], src[1]) : 0;

	while (src != src_end) {
		// need MAX_MATCH bytes for the next match, plus MIN_MATCH bytes to insert the string following the next match
		size_t	lookahead = src_end - src;
		if (lookahead < MIN_LOOKAHEAD && !(flags & TRANSCODE_FLUSH))
			break;

		// Insert the string window[strstart .. strstart+2] in the window, and set hash_head to the head of the hash chain
		uint32	hash_head = 0;
		if (lookahead >= MIN_MATCH) {
			h = update_hash(h, src[MIN_MATCH - 1]);
			hash_head = insert_string(h, src - win.base);
		}

		// Find the longest match, discarding those <= prev_length. At this point always match_length < MIN_MATCH
		if (hash_head && (src - win.base) - hash_head <= MAX_DIST()) {
			// To simplify the code, prevent matches with the string of window index 0 (in particular avoid a match of the string with itself at the start of the input file)
			longest_match(src, lookahead, hash_head, win, match_length, params.nice_length, params.calc_max_chain(match_length));
		}

		bool	bflush;
		if (match_length >= MIN_MATCH) {
			bflush = tally_dist((src - win.base) - match_start, match_length);
			lookahead -= match_length;

			// Insert new strings in the hash table only if the match length is not too large (saves time but degrades compression)
			if (match_length <= params.max_lazy && lookahead >= MIN_MATCH) {
				while (--match_length) {
					++src;
					h = update_hash(h, src[MIN_MATCH - 1]);
					hash_head = insert_string(h, src - win.base);
				}
				++src;
			} else {
				src	+= match_length;
				h	= update_hash(src[0], src[1]);
				match_length	= 0;
			}
		} else {
			// No match, output a literal byte
			bflush = tally_lit(*src++);
		}
		if (bflush)
			flush_block(vlc, src, win.base, false);

	}

	return src;
}

// Uses a lazy evaluation for matches: a match is finally adopted only if there is no better match at the next window position
const uint8* deflate_encoder::deflate_slow(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win, bool filtered) {
	static const uint32	TOO_FAR		= 4096;		// Matches of length 3 are discarded if their distance exceeds TOO_FAR

	if (src == src_end)
		return src;

	uint32	h = src_end - src >= MIN_LOOKAHEAD ? update_hash(src[0], src[1]) : 0;

	while (src != src_end) {
		// need MAX_MATCH bytes for the next match, plus MIN_MATCH bytes to insert the string following the next match
		size_t	lookahead = src_end - src;
		if (lookahead < MIN_LOOKAHEAD && !(flags & TRANSCODE_FLUSH))
			return src;

		// Insert the string window[strstart .. strstart+2] in the window, and set hash_head to the head of the hash chain
		uint32	hash_head = 0;
		if (lookahead >= MIN_MATCH) {
			h = update_hash(h, src[MIN_MATCH - 1]);
			hash_head = insert_string(h, src - win.base);
		}

		// Find the longest match, discarding those <= prev_length
		uint32	prev_length		= match_length;
		uint32	prev_start		= match_start;
		
		match_length	= MIN_MATCH-1;

		if (hash_head && prev_length < params.max_lazy && (src - win.base) - hash_head <= MAX_DIST()) {
			// To simplify the code, prevent matches with the string of window index 0 (in particular avoid a match of the string with itself at the start of the input file)
			longest_match(src, lookahead, hash_head, win, prev_length, params.nice_length, params.calc_max_chain(prev_length));

			// If prev_start is also MIN_MATCH, match_start is garbage but we will ignore the current match anyway.
			if (match_length <= 5 && (filtered || (match_length == MIN_MATCH && (src - win.base) - match_start > TOO_FAR)))
				match_length = MIN_MATCH-1;
		}

		// If there was a match at the previous step and the current match is not better, output the previous match
		if (prev_length >= MIN_MATCH && match_length <= prev_length) {
			uint32	max_insert	= (src - win.base) + lookahead - MIN_MATCH;	// Do not insert strings in hash table beyond this
			bool	bflush		= tally_dist((src - win.base) - 1 - prev_start, prev_length);

			// Insert in hash table all strings up to the end of the match
			// strstart-1 and strstart are already inserted. If there is not enough lookahead, the last two strings are not inserted in the hash table
			prev_length -= 2;
			while (prev_length--) {
				if (++src - win.base <= max_insert) {
					h = update_hash(h, src[MIN_MATCH - 1]);
					hash_head = insert_string(h, src - win.base);
				}
			}

			match_available = false;
			match_length	= MIN_MATCH-1;
			++src;

			if (bflush)
				flush_block(vlc, src, win.base, false);

		} else if (match_available) {
			// If there was no match at the previous position, output a single literal. If there was a match but the current match is longer, truncate the previous match to a single literal
			if (tally_lit(src[-1]))
				flush_block(vlc, src, win.base, false);

			++src;

		} else {
			// There is no previous match to compare with, wait for the next step to decide
			match_available = true;
			++src;
		}
	}

	if (match_available) {
		tally_lit(src[-1]);
		match_available = false;
	}

	return src;
}

// simply look for runs of bytes, generate matches only of distance one. Do not maintain a hash table (it will be regenerated if this run of deflate switches away from RLE)
const uint8* deflate_encoder::deflate_rle(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win) {

	while (src != src_end) {
		// need MAX_MATCH bytes for the longest run, plus one for the unrolled loop
		size_t lookahead = src_end - src;
		if (lookahead <= MAX_MATCH && !(flags & TRANSCODE_FLUSH))
			break;

		// See how many times the previous byte repeats
		match_length = 0;
		if (lookahead >= MIN_MATCH && src > win.base) {
			const uint8	*scan = src;
			uint8		prev = *scan;
			if (prev == *++scan && prev == *++scan) {
				const uint8	*end = src + MAX_MATCH;
				do {
				} while (prev == *++scan && prev == *++scan
					&&	prev == *++scan && prev == *++scan
					&&	prev == *++scan && prev == *++scan
					&&	prev == *++scan && prev == *++scan
					&&	scan < end
				);
				match_length = min(MAX_MATCH - (int)(end - scan), lookahead);
			}
		}

		// Emit match if have run of MIN_MATCH or longer, else emit literal
		bool	bflush;
		if (match_length >= MIN_MATCH) {
			bflush	= tally_dist(1, match_length);
			src		+= match_length;
			match_length = 0;
		} else {
			// No match, output a literal byte
			bflush = tally_lit(*src++);
		}
		if (bflush)
			flush_block(vlc, src, win.base, false);
	}

	return src;
}

// do not look for matches or maintain a hash table (it will be regenerated if this run of deflate switches away from Huffman)
const uint8* deflate_encoder::deflate_huff(VLC &vlc, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags, external_window win) {
	while (src != src_end) {
		// Output a literal byte
		match_length = 0;
		if (tally_lit(*src++))
			flush_block(vlc, src, win.base, false);
	}

	return src;
}

//-----------------------------------------------------------------------------
//  zlib
//-----------------------------------------------------------------------------

uint8 *zlib_decoder::process(uint8* dst, uint8 *dst_end, reader_intf file, TRANSCODE_FLAGS flags) {
	if (mode == HEAD) {
		auto	h	= file.get<header>();
		if (!h.verify() || h.method != METHOD_DEFLATED) {
			mode = BAD;
			return 0;
		}
		wbits = h.info + 8;

		if (h.dict)
			adler = file.get<uint32be>();
		mode = BLOCK;
	}

	uint8	*dst0 = dst;
	dst = deflate_decoder::process(dst, dst_end, file, flags);

	adler = adler32(adler, dst0, dst - dst0);

	if (mode == BLOCK && last) {
		auto	vlc = make_vlc_in(copy(file), vlc0);
		vlc.align(8);

		uint32	adler1 = file.get<uint32be>();//vlc.get<uint32be>();
		mode	= adler1 == adler ? DONE : BAD;
	}

	return dst;
}

const uint8* zlib_encoder::process(ostream_ref file, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	if (h.method) {
		file.write(h);
		h.method = 0;
	}
	const uint8	*src0 = src;
	src = deflate_encoder::process(file, src, src_end, flags);
	
	adler = adler32(adler, src0, src - src0);

	if (src == src_end && !(flags & TRANSCODE_PARTIAL))
		file.write<uint32be>(adler);
	return src;
}


static bool _test_deflate = test_stream_codec(deflate_encoder(), deflate_decoder());
