#include "lha.h"

using namespace iso;

#define	MAXMATCH	256
#define THRESHOLD	3
#define MAXDICT		16

static int method_dicbits[] = {
	0,	// METHOD_LZHUFF0	no compress
	12,	// METHOD_LZHUFF1	2^12 =	4KB sliding window
	13,	// METHOD_LZHUFF2	2^13 =	8KB sliding window
	13,	// METHOD_LZHUFF3	2^13 =	8KB sliding window
	12,	// METHOD_LZHUFF4	2^12 =	4KB sliding window
	13,	// METHOD_LZHUFF5	2^13 =	8KB sliding window
	15,	// METHOD_LZHUFF6	2^15 = 32KB sliding window
	16,	// METHOD_LZHUFF7	2^16 = 64KB sliding window
	11,	// METHOD_LARC		2^11 =	2KB sliding window
	12,	// METHOD_LARC5		2^12 =	4KB sliding window
	0,	// METHOD_LARC4		no compress
};

//-----------------------------------------------------------------------------
//	encoders/decoders
//-----------------------------------------------------------------------------

struct LHAdecoder {
	virtual	uint16	decode_c()					= 0;
	virtual	uint16	decode_p(uint32 loc)		= 0;
	virtual	void	decode_start(uint32 dicbit, uint8 *text)	= 0;
	virtual	void	decode_end()				= 0;
};

struct LHAencoder {
	virtual void	output(uint32 c, uint32 p)	= 0;
	virtual void	encode_start(uint32 dicbit)	= 0;
	virtual void	encode_end()				= 0;
};

struct VLCin : public vlc_in<uint32, true> {
	VLCin(istream_ref _file) : iso::vlc_in<uint32, true>(_file)	{}
};

struct VLCout : public vlc_out<uint32, true> {
	VLCout(ostream_ref _file) : vlc_out<uint32, true>(_file)		{}
};

//-----------------------------------------------------------------------------
// lh1
//-----------------------------------------------------------------------------

struct LHA1 {
	static const uint8 fixed[];
	enum {
		DICBIT		= 12,
		NC			= 314,
		NP			= 1 << (DICBIT - 6),
		N1			= 512
	};
	DYNHUFF<NC>		chuff;
};

const uint8 LHA1::fixed[] = {0, 0, 0x01, 0x04, 0x0c, 0x18, 0x30, 0};

struct LHAdecoder1 : LHAdecoder, LHA1 {
	VLCin		v;

	THUFF_decoder<NP,8>	phuff;

	uint16 decode_c() {
		int		c	= chuff.decode(v);
		chuff.update(c);
		if (c == N1)
			c += v.get(8);
		return c;
	}
	uint16	decode_p(uint32 loc) {
		uint16 p = phuff.decode(v);
		return (p << 6) + v.get(6);
	}
	void	decode_start(uint32 dicbit, uint8 *text) {
		ISO_ASSERT(dicbit == DICBIT);
		chuff.start();
		phuff.make_fixed_decoder(fixed);
	}
	void	decode_end() {
		v.restore_unused();
	}
	LHAdecoder1(istream_ref file) : v(file) {}
};

struct LHAencoder1 : LHAencoder, LHA1 {
	VLCout	v;

	THUFF_encoder<NP,8>	phuff;
	
	void	output(uint32 c, uint32 p) {
		if (c >= N1) {
			chuff.encode(v, N1);
			chuff.update(N1);
			v.put(c - N1, 8);
		} else {
			chuff.encode(v, c);
			chuff.update(c);
		}
		if (c >= 0x100) {
			phuff.encode(v, p >> 6);
			v.put(p, 6);
		}
	}
	void	encode_start(uint32 dicbit) {
		ISO_ASSERT(dicbit == DICBIT);
		chuff.start();
		phuff.make_fixed_encoder(fixed);
	}
	void	encode_end() {
		v.put(0, 7);
	}
	LHAencoder1(ostream_ref file) : v(file) {}
};

//-----------------------------------------------------------------------------
// lh2
//-----------------------------------------------------------------------------

struct LHAdecoder2 : LHAdecoder {
	enum {
		DICBIT		= 13,
		NC			= 286,
		NP			= 1 << (DICBIT - 6),
	};

	VLCin	v;

	DYNHUFF<NC>		chuff;
	DYNHUFF<NP>		phuff;
	int				np;
	uint32			nextloc;

	uint16 decode_c() {
		int	c	= chuff.decode(v);
		chuff.update(c);
		if (c == NC - 1)
			c += v.get(8);
		return c;
	}
	uint16 decode_p(uint32 loc) {
		while (loc > nextloc) {
			phuff.add_leaf(nextloc / 64, np++);
			if ((nextloc += 64) >= (1 << DICBIT))
				nextloc = 0xffffffff;
		}

		int	p	= phuff.decode(v);
		phuff.update(p);
		return (p << 6) + v.get(6);
	}
	void decode_start(uint32 dicbit, uint8 *text) {
		ISO_ASSERT(dicbit == DICBIT);
		np = 0;

		chuff.start();
		phuff.start(0);

		nextloc	= 0;
	}
	void	decode_end() {
		v.restore_unused();
	}
	LHAdecoder2(istream_ref file) : v(file) {}
};


//-----------------------------------------------------------------------------
// lh3
//-----------------------------------------------------------------------------

struct LHAdecoder3 : LHAdecoder {
	enum {
		DICBIT		= 13,
		NC			= 286,
		NP			= (1 << DICBIT) / 64,	//=128
		LENFIELD	= 4,	// bit size of length field for tree output
		EXTRABITS	= 8,	// >= log2(F-THRESHOLD + 258 - N1)
	};

	VLCin	v;

	uint16					blocksize;
	THUFF_decoder<NC,12>	chuff;
	THUFF_decoder<NP,8>		phuff;

	bool	read_block();

	uint16	decode_c() {
		if (blocksize == 0)
			read_block();
		blocksize--;

		uint16 c = chuff.decode(v);
		if (c == NC - 1)
			c += v.get(EXTRABITS);
		return c;
	}
	uint16	decode_p(uint32 loc) {
		uint16 p = phuff.decode(v);
		return (p << 6) + v.get(6);
	}
	void	decode_start(uint32 dicbit, uint8 *text) {
		ISO_ASSERT(dicbit == DICBIT);
		blocksize	= 0;
	}
	void	decode_end() {
		v.restore_unused();
	}

	LHAdecoder3(istream_ref file) : v(file) {}
};

bool LHAdecoder3::read_block() {
	// read block blocksize
	blocksize = v.get(16);

	// read c
	for (int i = 0; i < NC; ++i) {
		chuff.bitlen[i] = v.get(1) ? v.get(LENFIELD) + 1 : 0;
		if (i == 2 && chuff.bitlen[0] == 1 && chuff.bitlen[1] == 1 && chuff.bitlen[2] == 1) {
			chuff.clear_from(0);
			chuff.fill_table(chuff.get_raw(v));
			break;
		}
	}
	if (!chuff.make_table())
		return false;

	// read p
	if (!v.get(1)) {
		static uint8 fixed[] = {0, 0x01, 0x01, 0x03, 0x06, 0x0D, 0x1F, 0x4E, 0};    // 8K buf
		return phuff.make_fixed_decoder(fixed);

	} else {
		for (int i = 0; i < NP; ++i) {
			phuff.bitlen[i] = v.get(LENFIELD);
			if (i == 2 && phuff.bitlen[0] == 1 && phuff.bitlen[1] == 1 && phuff.bitlen[2] == 1) {
				phuff.clear_from(0);
				phuff.fill_table(phuff.get_raw(v));
				break;
			}
		}
		return phuff.make_table();
	}
}

//-----------------------------------------------------------------------------
// lh4, 5, 6, 7
//-----------------------------------------------------------------------------

struct LHA4567 {
	enum {
		NC	= 256 + MAXMATCH + 1 - THRESHOLD,
		NP	= MAXDICT + 1,
		NT	= 16 + 3,
	};

	uint32	np, pbit;

	void init(uint32 dicbit) {
		ISO_ASSERT(dicbit <= MAXDICT);
		np		= max(dicbit + 1, 14);
		pbit	= log2_ceil(np + 1);
	}
};

struct LHAdecoder4567 : LHAdecoder, LHA4567 {
	VLCin	v;

	THUFF_decoder<NC,12>	chuff;
	THUFF_decoder<NP,8>		phuff;
	uint16					blocksize;

	void	read_block();

	int		get_pt() {
		int c = v.peek(3);
		if (c != 7) {
			v.discard(3);
		} else {
			int		bits = v.peek(16);
			uint16	mask = 1 << (16 - 4);
			while (mask & bits) {
				mask >>= 1;
				c++;
			}
			v.discard(c - 3);
		}
		return c;
	}
	
	uint16	decode_c() {
		if (blocksize == 0)
			read_block();
		--blocksize;
		return chuff.decode(v);
	}

	uint16 decode_p(uint32 loc) {
		return phuff.decode(v, np);
	}

	void	decode_start(uint32 dicbit, uint8 *text) {
		init(dicbit);
		blocksize = 0;
	}
	void	decode_end() {
		v.restore_unused();
	}

	LHAdecoder4567(istream_ref file) : v(file) {}
};

void LHAdecoder4567::read_block() {
	blocksize = v.get(16);

	THUFF_decoder<NT,8>	thuff;

	// read t len
	if (int n = thuff.get_raw(v)) {
		int	i = 0;
		while (i < min(n, NT)) {
			thuff.bitlen[i++] = get_pt();
			if (i == 3) {
				for (int c = v.get(2); c-- && i < NT;)
					thuff.bitlen[i++] = 0;
			}
		}
		thuff.clear_from(i);
		thuff.make_table();
	} else {
		thuff.clear_from(0);
		thuff.fill_table(thuff.get_raw(v));
	}

	// read c len
	if (int n = chuff.get_raw(v)) {
		int	i = 0;
		while (i < min(n, NC)) {
			uint16 c = thuff.decode(v, NT);
			if (c <= 2) {
				c	= c == 0 ? 1
					: c == 1 ? v.get(4) + 3
					: chuff.get_raw(v) + 20;
				while (c--)
					chuff.bitlen[i++] = 0;
			} else {
				chuff.bitlen[i++] = c - 2;
			}
		}
		chuff.clear_from(i);
		chuff.make_table();
	} else {
		chuff.clear_from(0);
		chuff.fill_table(chuff.get_raw(v));
	}

	// read p len
	if (int	n = v.get(pbit)) {
		int	i = 0;
		while (i < min(n, NP))
			phuff.bitlen[i++] = get_pt();
		phuff.clear_from(i);
		phuff.make_table();
	} else {
		phuff.clear_from(0);
		phuff.fill_table(v.get(pbit));
	}
}

struct LHAencoder4567 : LHAencoder, LHA4567 {
	VLCout	v;
	
	THUFF_encoder<NC,12>	chuff;
	THUFF_encoder<NP,8>		phuff;
	
	uint16	cpos, output_pos, output_mask;
	uint8	buf[65536];

	void	put_pt(int k) {
		if (k < 7)
			v.put(k, 3);
		else // k=7 -> 1110	k=8 -> 11110	k=9 -> 111110 ...
			v.put(0xffff << 1, k - 3);
	}

	void	send_block();

	void	output(uint32 c, uint32 p) {
		output_mask >>= 1;

		if (output_mask == 0) {
			output_mask = 255;
			if (output_pos >= sizeof(buf) - 3 * 8) {
				send_block();
				output_pos = 0;
			}
			cpos = output_pos++;
			buf[cpos] = 0;
		}

		buf[output_pos++] = (uint8)c;
		++chuff.freq[c];

		if (c >= 0x100) {
			buf[cpos]			|= output_mask;
			buf[output_pos++]	= (uint8)(p >> 8);
			buf[output_pos++]	= (uint8)p;
			c = 0;
			while (p) {
				p >>= 1;
				c++;
			}
			++phuff.freq[c];
		}
	}
	void	encode_start(uint32 dicbit) {
		init(dicbit);
		clear(chuff.freq);
		clear(phuff.freq);
		output_pos = output_mask = 0;
		buf[0] = 0;
	}
	void	encode_end() {
		send_block();
		v.flush(0);

	}

	LHAencoder4567(ostream_ref file) : v(file) {}
};

void LHAencoder4567::send_block() {
	int		root	= chuff.make_tree();
	uint16	size	= chuff.freq[root];
	v.put(size, 16);

	THUFF_encoder<NT,0>	thuff;

	if (root >= NC) {
		// count t freq
		clear(thuff.freq);

		for (int i = 0, n = chuff.last(); i < n;) {
			if (int	k = chuff.bitlen[i++]) {
				thuff.freq[k + 2]++;

			} else {
				int	count = 1;
				while (i < n && chuff.bitlen[i] == 0) {
					i++;
					count++;
				}
				if (count <= 2) {
					thuff.freq[0] += count;
				} else if (count <= 18) {
					thuff.freq[1]++;
				} else if (count == 19) {
					thuff.freq[0]++;
					thuff.freq[1]++;
				} else {
					thuff.freq[2]++;
				}
			}
		}

		//make + write t
		root	= thuff.make_tree();
		if (root >= NT) {
			int	n = thuff.last();
			thuff.put_raw(v, n);
			for (int i = 0; i < n;) {
				put_pt(thuff.bitlen[i++]);
				if (i == 3) {
					while (i < 6 && thuff.bitlen[i] == 0)
						i++;
					v.put(i - 3, 2);
				}
			}
		} else {
			thuff.put_raw(v, 0);
			thuff.put_raw(v, root);
		}

		//write c len
		int	n = chuff.last();
		chuff.put_raw(v, n);

		for (int i = 0; i < n;) {
			int k = chuff.bitlen[i++];
			if (k == 0) {
				int	count = 1;
				while (i < n && chuff.bitlen[i] == 0) {
					i++;
					count++;
				}
				if (count <= 2) {
					for (k = 0; k < count; k++)
						thuff.encode(v, 0);
				} else if (count <= 18) {
					thuff.encode(v, 1);
					v.put(count - 3, 4);
				} else if (count == 19) {
					thuff.encode(v, 0);
					thuff.encode(v, 1);
					v.put(15, 4);
				} else {
					thuff.encode(v, 2);
					chuff.put_raw(v, count - 20);
				}
			} else {
				thuff.encode(v, k + 2);
			}
		}

	} else {
		thuff.put_raw(v, 0);
		thuff.put_raw(v, 0);
		chuff.put_raw(v, 0);
		chuff.put_raw(v, root);
	}

	//make + write p
	root = phuff.make_tree(np);
	if (root >= np) {
		int	n = phuff.last(np);
		v.put(n, pbit);
		for (int i = 0; i < n; i++)
			put_pt(phuff.bitlen[i]);

	} else {
		v.put(0, pbit);
		v.put(root, pbit);
	}

	//write buff
	uint16	pos = 0;
	uint8	flags;
	for (int i = 0; i < size; i++) {
		if ((i & 7) == 0)
			flags = buf[pos++];
		else
			flags <<= 1;

		int	c = buf[pos++];
		if (flags & 0x80) {
			chuff.encode(v, c + 0x100);
			int	p = (buf[pos + 0] << 8) + buf[pos + 1];
			pos += 2;

			//encode_p
			c = 0;
			for (uint16 q = p; q; q >>= 1)
				c++;
			chuff.encode(v, c);
			if (c > 1)
				v.put(p, c - 1);

		} else {
			chuff.encode(v, c);
		}
	}

	clear(chuff.freq);
	clear(phuff.freq);
}

//-----------------------------------------------------------------------------
//	LZS
//-----------------------------------------------------------------------------

struct LZS {
	enum {
		DICBIT		= 11,
		MAGIC		= 18,
	};
};

struct LHAdecoderLZS : LHAdecoder, LZS {
	VLCin	v;
	int		matchpos;

	uint16	decode_c() {
		if (v.get_bit())
			return v.get(8);

		matchpos = v.get(11);
		return v.get(4) + 0x101;
	}
	uint16	decode_p(uint32 loc) {
		return (loc - matchpos - MAGIC) & 0x7ff;
	}
	void	decode_start(uint32 dicbit, uint8 *text) {
		ISO_ASSERT(dicbit == DICBIT);
	}

	void	decode_end() {
		v.restore_unused();
	}

	LHAdecoderLZS(istream_ref file) : v(file) {}
};

//-----------------------------------------------------------------------------
//	LZ5
//-----------------------------------------------------------------------------

struct LZ5 {
	enum {
		DICBIT		= 12,
		MAGIC		= 19,
	};

	void	init(uint32 dicbit, uint8 *text) {
		ISO_ASSERT(dicbit == DICBIT);
		for (int i = 0; i < 256; i++)
			memset(text + i * 13 + 18, i, 13);
		for (int i = 0; i < 256; i++) {
			text[256 * 13 + 18 + i] = i;
			text[256 * 14 + 18 + i] = 255 - i;
		}
		memset(text + 256 * 15 + 18, 0, 128);
		memset(text + 256 * 15 + 128 + 18, ' ', 128 - 18);
	}
};

struct LHAdecoderLZ5 : LHAdecoder, LZ5 {
	VLCin	v;
	int		matchpos;
	uint8	flag, flagcnt;

	uint16	decode_c() {
		int	c;
		if (flagcnt == 0) {
			flagcnt = 8;
			flag = v.get_stream().getc();
		}
		c = v.get_stream().getc();
		if ((flag & 1) == 0) {
			matchpos = c;
			c = v.get_stream().getc();
			matchpos += (c & 0xf0) << 4;
			c = (c & 0x0f) + 0x100;
		}
		flag >>= 1;
		--flagcnt;
		return c;
	}

	uint16	decode_p(uint32 loc) {
		return (loc - matchpos - MAGIC) & 0x7ff;
	}

	void	decode_start(uint32 dicbit, uint8 *text) {
		init(dicbit, text);
		flagcnt = 0;
	}

	void	decode_end() {
		v.restore_unused();
	}

	LHAdecoderLZ5(istream_ref file) : v(file) {}
};

//-----------------------------------------------------------------------------
//	PM2
//-----------------------------------------------------------------------------

#if 0
struct LHAdecoderPM2 : LHAdecoder {
	static uint8 historyBits[8];
	static uint8 historyBase[8];
	static uint8 repeatBits[6];
	static uint8 repeatBase[6];

	VLCin	v;

	uint32	dicsiz1;
	size_t	nextcount;
	uint32	lastupdate;

	/* Circular double-linked list. */

	uint8 prev[0x100];
	uint8 next[0x100];
	uint8 lastbyte;
	
	uint8 gettree1;

	void hist_init() {
		for (int i = 0; i < 0x100; i++) {
			prev[(0xFF + i) & 0xFF] = i;
			next[(0x01 + i) & 0xFF] = i;
		}
		prev[0x7F] = 0x00; next[0x00] = 0x7F;
		prev[0xDF] = 0x80; next[0x80] = 0xDF;
		prev[0x9F] = 0xE0; next[0xE0] = 0x9F;
		prev[0x1F] = 0xA0; next[0xA0] = 0x1F;
		prev[0xFF] = 0x20; next[0x20] = 0xFF;
		lastbyte = 0x20;
	}

	uint8 hist_lookup(int n) {
		auto *direction = prev;
		if (n >= 0x80) {
			// Speedup: If you have to process more than half the ring, it's faster to walk the other way around.
			direction = next;
			n = 0x100 - n;
		}
		int i = lastbyte;
		for (; n != 0; n--)
			i = direction[i];

		return i;
	}

	void hist_update(uint8 data) {
		uint8 oldNext, oldPrev, newNext;

		if (data == lastbyte)
			return;

		/* detach from old position */
		oldNext = next[data];
		oldPrev = prev[data];
		prev[oldNext] = oldPrev;
		next[oldPrev] = oldNext;

		/* attach to new next */
		newNext = next[lastbyte];
		prev[newNext] = data;
		next[data] = newNext;

		/* attach to new prev */
		prev[data] = lastbyte;
		next[lastbyte] = data;

		lastbyte = data;
	}

	uint16 decode_c() {
		while (lastupdate != loc) {
			hist_update(dtext[lastupdate]);
			lastupdate = (lastupdate + 1) & dicsiz1;
		}

		while (decode_count >= nextcount) {
			/* Actually it will never loop, because decode_count doesn't grow that fast.
			However, this is the way LHA does it.
			Probably other encoding methods can have repeats larger than 256 bytes.
			Note: LHA puts this code in decode_p...
			*/

			switch (nextcount) {
				case 0x0000:
					maketree1();
					maketree2(5);
					nextcount = 0x0400;
					break;
				case 0x0400:
					maketree2(6);
					nextcount = 0x0800;
					break;
				case 0x0800:
					maketree2(7);
					nextcount = 0x1000;
					break;
				case 0x1000:
					if (v.get(1))
						maketree1();
					maketree2(8);
					nextcount = 0x2000;
					break;
				default:                /* 0x2000, 0x3000, 0x4000, ... */
					if (v.get(1)) {
						maketree1();
						maketree2(8);
					}
					nextcount += 0x1000;
					break;
			}
		}
		gettree1 = tree1_get();        /* value preserved for decode_p */
		if (gettree1 >= 29) {
			//exit(1);
		}

		/* direct value (ret <= UCHAR_MAX) */
		if (gettree1 < 8)
			return hist_lookup(historyBase[gettree1] + v.get(historyBits[gettree1]));
		/* repeats: (ret > UCHAR_MAX) */
		if (gettree1 < 23)
			return offset + 2 + (gettree1 - 8);

		return offset + repeatBase[gettree1 - 23] + v.get(repeatBits[gettree1 - 23]);
	}

	uint16 decode_p(uint32 loc) {
		/* gettree1 value preserved from decode_c */
		int nbits, delta, gettree2;
		if (gettree1 == 8) {        /* 2-byte repeat with offset 0..63 */
			nbits = 6;
			delta = 0;
		}
		else if (gettree1 < 28) {   /* n-byte repeat with offset 0..8191 */
			gettree2 = tree2_get();
			if (gettree2 == 0) {
				nbits = 6;
				delta = 0;
			}
			else {                  /* 1..7 */
				nbits = 5 + gettree2;
				delta = 1 << nbits;
			}
		}
		else {                      /* 256 bytes repeat with offset 0 */
			nbits = 0;
			delta = 0;
		}

		return delta + v.get(nbits);
	}

	void decode_start(int dicbit) {
		dicsiz1 = (1 << dicbit) - 1;
		hist_init();
		nextcount = 0;
		lastupdate = 0;
		v.get(1);                 // discard bit
	}
//	{decode_c_pm2, decode_p_pm2, decode_start_pm2}
};

uint8 LHAdecoderPM2::historyBits[8] = {   3,   3,   4,   5,   5,   5,   6,   6 };
uint8 LHAdecoderPM2::historyBase[8] = {   0,   8,  16,  32,  64,  96, 128, 192 };
uint8 LHAdecoderPM2::repeatBits[6]  = {   3,   3,   5,   6,   7,   0 };
uint8 LHAdecoderPM2::repeatBase[6]  = {  17,  25,  33,  65, 129, 256 };
#endif

//-----------------------------------------------------------------------------
//	Sliding window encoder
//-----------------------------------------------------------------------------

struct SLIDE_encode : LHA::SLIDE_hash<15> {
	enum {
		HASH_BITS	= 15,
		CHAIN_LIMIT	= 0x100,
	};

	static uint32	calc_hash(const uint8 *p)				{ return (((p[0] << 5) ^ p[1]) << 5) ^ p[2]; }
	static uint32	next_hash(uint32 hash, const uint8 *p)	{ return ((hash << 5) ^ p[2]) & ((1 << HASH_BITS) - 1); }

	malloc_block	text;
	uint32			maxmatch;
	uint32			dicbit;
	uint32			remainder;
	size_t			total_read;

	SLIDE_encode(uint32 maxmatch, uint32 dicbit) : SLIDE_hash<HASH_BITS>(1 << dicbit), text(dicsiz * 2 + maxmatch), maxmatch(maxmatch), dicbit(dicbit), total_read(0) {}
	uint32	next_token(istream_ref file, uint32 hash, uint32 &pos);
	void	encode(LHAencoder &encoder, istream_ref file);
};

// slide window
uint32 SLIDE_encode::next_token(istream_ref file, uint32 hash, uint32 &pos) {
	remainder--;
	if (++pos >= dicsiz + maxmatch) {
		pos			-= dicsiz;
		text.shift_down(dicsiz);
		slide_dict(dicsiz);
		remainder	+= file.readbuff(text + dicsiz + maxmatch, dicsiz);
	}
	return next_hash(hash, text + pos);
}

void SLIDE_encode::encode(LHAencoder &encoder, istream_ref file) {
	encoder.encode_start(dicbit);
	text.fill(' ');

	remainder	= file.readbuff(text + dicsiz, text.length() - dicsiz);

	uint32		match_len	= min(THRESHOLD - 1, remainder);
	uint32		match_off	= 0;
	uint32		pos			= dicsiz;
	uint32		hash		= calc_hash(text + pos);
	insert_hash(hash, pos);     // associate hash and pos

	uint8   *p      = text;
	while (remainder) {
		uint32	last_len	= match_len;
		uint32	last_off	= match_off;

		hash		= next_token(file, hash, pos);
		match_len	= max(match_len, THRESHOLD) - 1;
		match_off	= search_dict(p + pos, p + pos + min(maxmatch, remainder), pos, hash, THRESHOLD, match_len, CHAIN_LIMIT, next_hash);
		insert_hash(hash, pos);

		if (match_len > last_len || last_len < THRESHOLD) {
			// output a letter
			encoder.output(p[pos - 1], 0);
		} else {
			// output length and offset
			encoder.output(last_len + 256 - THRESHOLD, (last_off - 1) & (dicsiz - 1));

			while (--last_len > 1) {
				hash = next_token(file, hash, pos);
				insert_hash(hash, pos);
			}
			hash		= next_token(file, hash, pos);
			match_len	= THRESHOLD - 1;
			match_off	= search_dict(p + pos, p + pos + min(maxmatch, remainder), pos, hash, THRESHOLD, match_len, CHAIN_LIMIT, next_hash);
			insert_hash(hash, pos);
		}
	}
	encoder.encode_end();
}
//-----------------------------------------------------------------------------
//	Sliding window decoder
//-----------------------------------------------------------------------------

void SLIDE_decode(LHAdecoder &decoder, uint32 dicbit, ostream_ref file, size_t size) {
	uint32			dicsiz	= 1L << dicbit;
	
	malloc_block	text(dicsiz);
	
	memset(text, ' ', dicsiz);
	uint8	*p = text;

	decoder.decode_start(dicbit, p);

	uint32	adjust	= 256 - THRESHOLD;
	uint32	loc		= 0;

	for (size_t count = 0; count < size;) {
		int	c = decoder.decode_c();
		if (c < 256) {
			p[loc++] = c;
			if (loc == dicsiz) {
				file.writebuff(p, dicsiz);
				loc = 0;
			}
			count++;

		} else {
			int			matchlen	= c - adjust;
			uint32		matchoff	= decoder.decode_p(loc) + 1;
			uint32		matchpos	= (loc - matchoff) & (dicsiz - 1);
			count += matchlen;
			for (int i = 0; i < matchlen; i++) {
				p[loc++] = p[(matchpos + i) & (dicsiz - 1)];
				if (loc == dicsiz) {
					file.writebuff(p, dicsiz);
					loc = 0;
				}
			}
		}
	}
	if (loc != 0)
		file.writebuff(p, loc);

	decoder.decode_end();
}

//-----------------------------------------------------------------------------
//	interface
//-----------------------------------------------------------------------------

void iso::LHA::decode_lzhuf(istream_ref ifile, ostream_ref ofile, size_t size, METHOD method) {
	if (int dicbit = method_dicbits[method]) {
		switch (method) {
			case METHOD_LZHUFF1:
				SLIDE_decode(lvalue(LHAdecoder1(ifile)), dicbit, ofile, size);
				break;
			case METHOD_LZHUFF2:
				SLIDE_decode(lvalue(LHAdecoder2(ifile)), dicbit, ofile, size);
				break;
			case METHOD_LZHUFF3:
				SLIDE_decode(lvalue(LHAdecoder3(ifile)), dicbit, ofile, size);
				break;
			case METHOD_LZHUFF4:
			case METHOD_LZHUFF5:
			case METHOD_LZHUFF6:
			case METHOD_LZHUFF7:
				SLIDE_decode(lvalue(LHAdecoder4567(ifile)), dicbit, ofile, size);
				break;
			case METHOD_LARC:
				SLIDE_decode(lvalue(LHAdecoderLZS(ifile)), dicbit, ofile, size);
				break;
			case METHOD_LARC5:
				SLIDE_decode(lvalue(LHAdecoderLZ5(ifile)), dicbit, ofile, size);
				break;
//			case METHOD_LARC4:

			default:
				break;
		}

	} else if (size) {
		stream_copy(ofile, ifile, size);
	}
}

void iso::LHA::encode_lzhuf(istream_ref ifile, ostream_ref ofile, size_t size, METHOD method) {
	if (int dicbit = method_dicbits[method]) {
		SLIDE_encode	slide(method == METHOD_LZHUFF1 ? 60 : MAXMATCH, dicbit);

		switch (method) {
			case METHOD_LZHUFF1:
				slide.encode(lvalue(LHAencoder1(ofile)), lvalue(istream_offset(copy(ifile), size)));
				break;
	//		case METHOD_LZHUFF2:
	//			slide.encode(LHAencoder2(ofile), ifile, size);
	//			break;
	//		case METHOD_LZHUFF3:
	//			slide.encode(LHAencoder3(ofile), ifile, size);
	//			break;
			case METHOD_LZHUFF4:
			case METHOD_LZHUFF5:
			case METHOD_LZHUFF6:
			case METHOD_LZHUFF7:
				slide.encode(lvalue(LHAencoder4567(ofile)), lvalue(istream_offset(copy(ifile), size)));
				break;
	//		case METHOD_LARC:
	//		case METHOD_LARC5:
	//		case METHOD_LARC4:

			default:
				break;
		}
	} else if (size) {
		stream_copy(ofile, ifile, size);
	}
}
