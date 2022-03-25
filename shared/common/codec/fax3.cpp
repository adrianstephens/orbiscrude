#include "fax3.h"
#include "vlc.h"
#include "base/bits.h"

using namespace iso;

struct HUFF_code {
	uint8	bitlen;
	uint8	code;
};

enum {
	G3_V		= 3,
	G3_PASS		= 7,
	G3_HORIZ	= 8,
	G3_EXT1D	= 9,
	G3_EXT2D	= 10,
	G3_EOL		= 11,
};

const HUFF_code FaxMainCodes[] = {	//		action
	{ 7,  0x40 },	//02 0000 010			V - 3	0100000
	{ 6,  0x40 },	//02 0000 10			V - 2	-010000
	{ 3,  0x40 },	//02 010				V - 1	----010
	{ 1,  0x80 },	//01 1					V		------1
	{ 3,  0xc0 },	//03 011				V + 1	----110
	{ 6,  0xc0 },	//03 0000 11			V + 2	-110000
	{ 7,  0xc0 },	//03 0000 011			V + 3	1100000
	{ 4,  0x80 },	//01 0001				PASS	---1000
	{ 3,  0x80 },	//01 001				HORIZ	----100
//	{ 10, 0x80 },	//01 0000 0000 01		EXT1D
	{ 7,  0x80 },	//01 0000 001			EXT2D	1000000	
//	{ 12, 0x80 },	//01 0000 0000 0001		EOL
	{ 7,  0x00 },	//00 0000 0000 0001		EOL (or EXT1D)	0000000
};

const HUFF_code FaxWhiteCodes[] = {	//	run_length
	{ 8,  0xac },	//35 0011 0101			0	
	{ 6,  0xe0 },	//07 0001 11			1	
	{ 4,  0xe0 },	//07 0111				2	
	{ 4,  0x10 },	//08 1000				3	
	{ 4,  0xd0 },	//0B 1011				4	
	{ 4,  0x30 },	//0C 1100				5	
	{ 4,  0x70 },	//0E 1110				6	
	{ 4,  0xf0 },	//0F 1111				7	
	{ 5,  0xc8 },	//13 1001 1				8	
	{ 5,  0x28 },	//14 1010 0				9	
	{ 5,  0xe0 },	//07 0011 1				10
	{ 5,  0x10 },	//08 0100 0				11
	{ 6,  0x10 },	//08 0010 00			12
	{ 6,  0xc0 },	//03 0000 11			13
	{ 6,  0x2c },	//34 1101 00			14
	{ 6,  0xac },	//35 1101 01			15
	{ 6,  0x54 },	//2A 1010 10			16
	{ 6,  0xd4 },	//2B 1010 11			17
	{ 7,  0xe4 },	//27 0100 111			18
	{ 7,  0x30 },	//0C 0001 100			19
	{ 7,  0x10 },	//08 0001 000			20
	{ 7,  0xe8 },	//17 0010 111			21
	{ 7,  0xc0 },	//03 0000 011			22
	{ 7,  0x20 },	//04 0000 100			23
	{ 7,  0x14 },	//28 0101 000			24
	{ 7,  0xd4 },	//2B 0101 011			25
	{ 7,  0xc8 },	//13 0010 011			26
	{ 7,  0x24 },	//24 0100 100			27
	{ 7,  0x18 },	//18 0011 000			28
	{ 8,  0x40 },	//02 0000 0010			29
	{ 8,  0xc0 },	//03 0000 0011			30
	{ 8,  0x58 },	//1A 0001 1010			31
	{ 8,  0xd8 },	//1B 0001 1011			32
	{ 8,  0x48 },	//12 0001 0010			33
	{ 8,  0xc8 },	//13 0001 0011			34
	{ 8,  0x28 },	//14 0001 0100			35
	{ 8,  0xa8 },	//15 0001 0101			36
	{ 8,  0x68 },	//16 0001 0110			37
	{ 8,  0xe8 },	//17 0001 0111			38
	{ 8,  0x14 },	//28 0010 1000			39
	{ 8,  0x94 },	//29 0010 1001			40
	{ 8,  0x54 },	//2A 0010 1010			41
	{ 8,  0xd4 },	//2B 0010 1011			42
	{ 8,  0x34 },	//2C 0010 1100			43
	{ 8,  0xb4 },	//2D 0010 1101			44
	{ 8,  0x20 },	//04 0000 0100			45
	{ 8,  0xa0 },	//05 0000 0101			46
	{ 8,  0x50 },	//0A 0000 1010			47
	{ 8,  0xd0 },	//0B 0000 1011			48
	{ 8,  0x4a },	//52 0101 0010			49
	{ 8,  0xca },	//53 0101 0011			50
	{ 8,  0x2a },	//54 0101 0100			51
	{ 8,  0xaa },	//55 0101 0101			52
	{ 8,  0x24 },	//24 0010 0100			53
	{ 8,  0xa4 },	//25 0010 0101			54
	{ 8,  0x1a },	//58 0101 1000			55
	{ 8,  0x9a },	//59 0101 1001			56
	{ 8,  0x5a },	//5A 0101 1010			57
	{ 8,  0xda },	//5B 0101 1011			58
	{ 8,  0x52 },	//4A 0100 1010			59
	{ 8,  0xd2 },	//4B 0100 1011			60
	{ 8,  0x4c },	//32 0011 0010			61
	{ 8,  0xcc },	//33 0011 0011			62
	{ 8,  0x2c },	//34 0011 0100			63

	{ 5,  0xd8 },	//1B 1101 1				64
	{ 5,  0x48 },	//12 1001 0				128	
	{ 6,  0xe8 },	//17 0101 11			192	
	{ 7,  0xec },	//37 0110 111			256	
	{ 8,  0x6c },	//36 0011 0110			320	
	{ 8,  0xec },	//37 0011 0111			384	
	{ 8,  0x26 },	//64 0110 0100			448	
	{ 8,  0xa6 },	//65 0110 0101			512	
	{ 8,  0x16 },	//68 0110 1000			576	
	{ 8,  0xe6 },	//67 0110 0111			640	
	{ 9,  0x33 },	//CC 0110 0110 0		704	
	{ 9,  0xb3 },	//CD 0110 0110 1		768	
	{ 9,  0x4b },	//D2 0110 1001 0		832	
	{ 9,  0xcb },	//D3 0110 1001 1		896	
	{ 9,  0x2b },	//D4 0110 1010 0		960	
	{ 9,  0xab },	//D5 0110 1010 1		1024	
	{ 9,  0x6b },	//D6 0110 1011 0		1088	
	{ 9,  0xeb },	//D7 0110 1011 1		1152	
	{ 9,  0x1b },	//D8 0110 1100 0		1216	
	{ 9,  0x9b },	//D9 0110 1100 1		1280	
	{ 9,  0x5b },	//DA 0110 1101 0		1344	
	{ 9,  0xdb },	//DB 0110 1101 1		1408	
	{ 9,  0x19 },	//98 0100 1100 0		1472	
	{ 9,  0x99 },	//99 0100 1100 1		1536	
	{ 9,  0x59 },	//9A 0100 1101 0		1600	
	{ 6,  0x18 },	//18 0110 00			1664	
	{ 9,  0xd9 },	//9B 0100 1101 1		1728	
	{ 11, 0x10 },	//08 0000 0001 000		1792
	{ 11, 0x30 },	//0C 0000 0001 100		1856
	{ 11, 0xb0 },	//0D 0000 0001 101		1920
	{ 12, 0x48 },	//12 0000 0001 0010		1984
	{ 12, 0xc8 },	//13 0000 0001 0011		2048
	{ 12, 0x28 },	//14 0000 0001 0100		2112
	{ 12, 0xa8 },	//15 0000 0001 0101		2176
	{ 12, 0x68 },	//16 0000 0001 0110		2240
	{ 12, 0xe8 },	//17 0000 0001 0111		2304
	{ 12, 0x38 },	//1C 0000 0001 1100		2368
	{ 12, 0xb8 },	//1D 0000 0001 1101		2432
	{ 12, 0x78 },	//1E 0000 0001 1110		2496
	{ 12, 0xf8 },	//1F 0000 0001 1111		2560

	{ 12, 0x80 },	//01 0000 0000 0001		EOL 
	{  9, 0x80 },	//01 0000 0000 1		INV 
	{ 10, 0x80 },	//01 0000 0000 01		INV 
	{ 11, 0x80 },	//01 0000 0000 001		INV 
	{ 12, 0x00 },	//00 0000 0000 0000		INV 
};

const HUFF_code FaxBlackCodes[] = {	//	run_length
	{ 10, 0xec },	//37 0000 1101 11		0 
	{ 3,  0x40 },	//02 010				1 
	{ 2,  0xc0 },	//03 11					2 
	{ 2,  0x40 },	//02 10					3 
	{ 3,  0xc0 },	//03 011				4 
	{ 4,  0xc0 },	//03 0011				5 
	{ 4,  0x40 },	//02 0010				6 
	{ 5,  0xc0 },	//03 0001 1				7 
	{ 6,  0xa0 },	//05 0001 01			8 
	{ 6,  0x20 },	//04 0001 00			9 
	{ 7,  0x20 },	//04 0000 100			10
	{ 7,  0xa0 },	//05 0000 101			11
	{ 7,  0xe0 },	//07 0000 111			12
	{ 8,  0x20 },	//04 0000 0100			13
	{ 8,  0xe0 },	//07 0000 0111			14
	{ 9,  0x18 },	//18 0000 1100 0		15
	{ 10, 0xe8 },	//17 0000 0101 11		16
	{ 10, 0x18 },	//18 0000 0110 00		17
	{ 10, 0x10 },	//08 0000 0010 00		18
	{ 11, 0xe6 },	//67 0000 1100 111		19
	{ 11, 0x16 },	//68 0000 1101 000		20
	{ 11, 0x36 },	//6C 0000 1101 100		21
	{ 11, 0xec },	//37 0000 0110 111		22
	{ 11, 0x14 },	//28 0000 0101 000		23
	{ 11, 0xe8 },	//17 0000 0010 111		24
	{ 11, 0x18 },	//18 0000 0011 000		25
	{ 12, 0x53 },	//CA 0000 1100 1010		26
	{ 12, 0xd3 },	//CB 0000 1100 1011		27
	{ 12, 0x33 },	//CC 0000 1100 1100		28
	{ 12, 0xb3 },	//CD 0000 1100 1101		29
	{ 12, 0x16 },	//68 0000 0110 1000		30
	{ 12, 0x96 },	//69 0000 0110 1001		31
	{ 12, 0x56 },	//6A 0000 0110 1010		32
	{ 12, 0xd6 },	//6B 0000 0110 1011		33
	{ 12, 0x4b },	//D2 0000 1101 0010		34
	{ 12, 0xcb },	//D3 0000 1101 0011		35
	{ 12, 0x2b },	//D4 0000 1101 0100		36
	{ 12, 0xab },	//D5 0000 1101 0101		37
	{ 12, 0x6b },	//D6 0000 1101 0110		38
	{ 12, 0xeb },	//D7 0000 1101 0111		39
	{ 12, 0x36 },	//6C 0000 0110 1100		40
	{ 12, 0xb6 },	//6D 0000 0110 1101		41
	{ 12, 0x5b },	//DA 0000 1101 1010		42
	{ 12, 0xdb },	//DB 0000 1101 1011		43
	{ 12, 0x2a },	//54 0000 0101 0100		44
	{ 12, 0xaa },	//55 0000 0101 0101		45
	{ 12, 0x6a },	//56 0000 0101 0110		46
	{ 12, 0xea },	//57 0000 0101 0111		47
	{ 12, 0x26 },	//64 0000 0110 0100		48
	{ 12, 0xa6 },	//65 0000 0110 0101		49
	{ 12, 0x4a },	//52 0000 0101 0010		50
	{ 12, 0xca },	//53 0000 0101 0011		51
	{ 12, 0x24 },	//24 0000 0010 0100		52
	{ 12, 0xec },	//37 0000 0011 0111		53
	{ 12, 0x1c },	//38 0000 0011 1000		54
	{ 12, 0xe4 },	//27 0000 0010 0111		55
	{ 12, 0x14 },	//28 0000 0010 1000		56
	{ 12, 0x1a },	//58 0000 0101 1000		57
	{ 12, 0x9a },	//59 0000 0101 1001		58
	{ 12, 0xd4 },	//2B 0000 0010 1011		59
	{ 12, 0x34 },	//2C 0000 0010 1100		60
	{ 12, 0x5a },	//5A 0000 0101 1010		61
	{ 12, 0x66 },	//66 0000 0110 0110		62
	{ 12, 0xe6 },	//67 0000 0110 0111		63

	{ 10, 0xf0 },	//0F 0000 0011 11		64
	{ 12, 0x13 },	//C8 0000 1100 1000		128 
	{ 12, 0x93 },	//C9 0000 1100 1001		192 
	{ 12, 0xda },	//5B 0000 0101 1011		256 
	{ 12, 0xcc },	//33 0000 0011 0011		320 
	{ 12, 0x2c },	//34 0000 0011 0100		384 
	{ 12, 0xac },	//35 0000 0011 0101		448 
	{ 13, 0x36 },	//6C 0000 0011 0110 0	512 
	{ 13, 0xb6 },	//6D 0000 0011 0110 1	576 
	{ 13, 0x52 },	//4A 0000 0010 0101 0	640 
	{ 13, 0xd2 },	//4B 0000 0010 0101 1	704 
	{ 13, 0x32 },	//4C 0000 0010 0110 0	768 
	{ 13, 0xb2 },	//4D 0000 0010 0110 1	832 
	{ 13, 0x4e },	//72 0000 0011 1001 0	896 
	{ 13, 0xce },	//73 0000 0011 1001 1	960 
	{ 13, 0x2e },	//74 0000 0011 1010 0	1024
	{ 13, 0xae },	//75 0000 0011 1010 1	1088
	{ 13, 0x6e },	//76 0000 0011 1011 0	1152
	{ 13, 0xee },	//77 0000 0011 1011 1	1216
	{ 13, 0x4a },	//52 0000 0010 1001 0	1280
	{ 13, 0xca },	//53 0000 0010 1001 1	1344
	{ 13, 0x2a },	//54 0000 0010 1010 0	1408
	{ 13, 0xaa },	//55 0000 0010 1010 1	1472
	{ 13, 0x5a },	//5A 0000 0010 1101 0	1536
	{ 13, 0xda },	//5B 0000 0010 1101 1	1600
	{ 13, 0x26 },	//64 0000 0011 0010 0	1664
	{ 13, 0xa6 },	//65 0000 0011 0010 1	1728
	{ 11, 0x10 },	//08 0000 0001 000		1792
	{ 11, 0x30 },	//0C 0000 0001 100		1856
	{ 11, 0xb0 },	//0D 0000 0001 101		1920
	{ 12, 0x48 },	//12 0000 0001 0010		1984
	{ 12, 0xc8 },	//13 0000 0001 0011		2048
	{ 12, 0x28 },	//14 0000 0001 0100		2112
	{ 12, 0xa8 },	//15 0000 0001 0101		2176
	{ 12, 0x68 },	//16 0000 0001 0110		2240
	{ 12, 0xe8 },	//17 0000 0001 0111		2304
	{ 12, 0x38 },	//1C 0000 0001 1100		2368
	{ 12, 0xb8 },	//1D 0000 0001 1101		2432
	{ 12, 0x78 },	//1E 0000 0001 1110		2496
	{ 12, 0xf8 },	//1F 0000 0001 1111		2560

	{ 12, 0x80 },	//01 0000 0000 0001		EOL
	{  9, 0x80 },	//01 0000 0000 1		INV
	{ 10, 0x80 },	//01 0000 0000 01		INV
	{ 11, 0x80 },	//01 0000 0000 001		INV
	{ 12, 0x00 },	//00 0000 0000 0000		INV
};

template<int TB, typename T> struct HUFF_table {
	T		table[1 << TB];
	const HUFF_code	*codes;

	template<int NC> HUFF_table(const HUFF_code (&codes)[NC]) : codes(codes) {
		memset(table, 0xff, sizeof(table));
		
		for (int i = 0; i < NC; i++) {
			int	len		= codes[i].bitlen;
			int	code	= shift_bits(codes[i].code, len - 8);
			while (code < (1 << TB)) {
				table[code] = i;
				code += 1 << len;
			}
		}
	}

	template<typename V> T	decode(V& v) const {
		T	i = table[v.peek(TB)];
		v.discard(codes[i].bitlen);
		return i;
	}
};

const HUFF_table<7,  uint8>	FaxMainTable(FaxMainCodes);
const HUFF_table<12, uint8>	FaxWhiteTable(FaxWhiteCodes);
const HUFF_table<14, uint8>	FaxBlackTable(FaxBlackCodes); 


template<typename V, typename T> int getrun(V &v, T &t) {
	int	run = 0;
	int	code;

	while ((code = t.decode(v)) >= 64 && code < 104)
		run += (code - 63) * 64;

	if (code == 104)
		return -1;	//EOL

	if (code < 64)
		run += code;
	else
		return -2;	//error!

	return run;
}

typedef vlc_in<uint32, false, memory_reader>	fax_vlc_in;
typedef vlc_out<uint32, false, memory_writer>	fax_vlc_out;

static bool SyncEOL(fax_vlc_in &v) {
	while (v.peek(11))
		v.discard(1);

    while (v.peek(8) == 0)
		v.discard(8);

    while (v.get(1) == 0)
		;
	return true;
}

static int Expand1D(fax_vlc_in &v, int width, uint16 *a) {

	for (int x = 0; x < width;) {

		int	run = getrun(v, FaxWhiteTable);
		if (run < 0) {
			*a++ = width;
			return run;
		}

		*a++ = x += run;

		run = getrun(v, FaxBlackTable);
		if (run < 0) {
			*a++ = width;
			return run;
		}

		*a++ = x += run;
	}
	return 0;
}

static int Expand2D(fax_vlc_in &v, int width, uint16 *a, uint16 *b) {
	uint16 *a_start		= a;

	for (int x = 0; x < width;) {
		int		code;
		switch (code = FaxMainTable.decode(v)) {
			case G3_PASS:
				while (*b <= x)
					b += 2;
				++b;
				x = *b++;
				break;

			case G3_HORIZ:
				if ((a - a_start) & 1) {
					// black first
					int	run = getrun(v, FaxBlackTable);
					if (run < 0) {
						*a++ = width;
						return run;
					}
					*a++ = x += run;

					run = getrun(v, FaxWhiteTable);
					if (run < 0) {
						*a++ = width;
						return run;
					}
					*a++ = x += run;

				} else {
					// white first
					int	run = getrun(v, FaxWhiteTable);
					if (run < 0) {
						*a++ = width;
						return run;
					}
					*a++ = x += run;

					run = getrun(v, FaxBlackTable);
					if (run < 0) {
						*a++ = width;
						return run;
					}
					*a++ = x += run;
				}
				break;

			case G3_V - 3:
			case G3_V - 2:
			case G3_V - 1:
			case G3_V:
			case G3_V + 1:
			case G3_V + 2:
			case G3_V + 3:
				while (*b <= x)
					b += 2;
				*a++ = x = *b++ + (code - G3_V);
				break;

			case G3_EXT2D:
				//extension2d(v.get(3));
				return -2;

			case G3_EOL:
				switch (v.get(3)) {
					case 0:	// EOL?
						*a++ = width;
						return v.get(2) == 2 ? -1 : -2;
					case 4:// G3_EXT1D
						//extension1d(v.get(3));
						break;
					default:
						return -2;
				}

			default:
				return -2;
		}

	}
	return 0;
}


bool iso::FaxDecode(const_memory_block srce, const bitmatrix_aligned<uint32> &dest, FAXMODE mode) {
	fax_vlc_in		v(srce);
	uint16			runs[1024];

	bool	no_eol = mode & FAXMODE_NOEOL;
	if (!no_eol)
		SyncEOL(v);

	int				width = dest.num_cols();
	int				height = dest.num_rows();
	uint16			*a = runs, *b = runs + 512;
	*b = width;

	for (int i = 0; i < height; i++) {
		bool	is2D =	(mode & FAXMODE_FIXED2D)	? true
					:	(mode & FAXMODE_2D)			? !v.get_bit()
					:	false;

		int		ret	= is2D
			? Expand2D(v, width, a, b)
			: Expand1D(v, width, a);

		if (no_eol) {
			if (ret != 0) {
				v.get(13);
				//if (v.get(13) != 0x1001)
				return false;// don't error on badly-terminated strips
			}
		} else if (ret != -1) {
			if (v.get(12) != 1 << 11)
				SyncEOL(v);
		}

		auto	line = dest[i];
		for (auto p = a;;) {
			int	x0 = min(*p++, width);
			if (x0 >= width)
				break;
			int	x1 = min(*p++, width);
			line.slice(x0, x1 - x0).set_all();
			if (x1 >= width)
				break;
		}

		swap(a, b);

		if (mode & (FAXMODE_BYTEALIGN | FAXMODE_WORDALIGN))
			v.align(mode & FAXMODE_WORDALIGN ? 16 : 8);
	}
	return true;
}

//-----------------------------------------------------------------------------
// encode
//-----------------------------------------------------------------------------

inline void putcode(fax_vlc_out &v, const HUFF_code &code) {
	v.put(shift_bits(code.code, code.bitlen - 8), code.bitlen);
}

static void putrun(fax_vlc_out &v, const HUFF_code* tab, int32 span) {
	while (span >= 2624) {
		putcode(v, tab[63 + (2560>>6)]);
		span -= 2560;
	}
	if (span >= 64) {
		putcode(v, tab[63 + (span>>6)]);
		span &= 63;
	}
	putcode(v, tab[span]);
}

static void puteol(fax_vlc_out &v, FAXMODE mode, bool is2D) {
	if (mode & FAXMODE_ALIGN)
		v.align(mode & FAXMODE_BYTEALIGN ? 8 : 16, 12);

	if (mode & FAXMODE_2D)
		v.put(is2D ? 2 : 3, 13);
	else
		v.put(1, 12);
}

static void Encode1D(fax_vlc_out &v, int width, const uint16 *a) {
	for (uint32 x = 0; x < width;) {
		// white span
		int	x1 = *a++;
		putrun(v, FaxWhiteCodes, x1 - x);

		if (x >= width)
			break;

		// black span
		x = *a++;
		putrun(v, FaxBlackCodes, x - x1);
	}
}


static void Encode2D(fax_vlc_out &v, int width, const uint16 *a, const uint16 *b) {
	auto	a_start	= a;
	uint32	a1	= *a++;
	uint32	b1	= *b++;

	for (uint32	a0 = 0;;) {
		uint32	b2 = *b++;

		if (b2 >= a1) {
			int		d = (int)b1 - (int)a1;
			if (d < -3 || d > 3) {
				// horizontal mode
				putcode(v, FaxMainCodes[G3_HORIZ]);

				uint32	a2 = *a++;
				if ((a - a_start) & 1) {
					// white first
					putrun(v, FaxWhiteCodes, a1 - a0);
					putrun(v, FaxBlackCodes, a2 - a1);
				} else {
					// black first
					putrun(v, FaxBlackCodes, a1 - a0);
					putrun(v, FaxWhiteCodes, a2 - a1);
				}
				a0 = a2;
			} else {
				// vertical mode
				putcode(v, FaxMainCodes[G3_V + d]);
				a0 = a1;
			}
		} else {
			// pass mode
			putcode(v, FaxMainCodes[G3_PASS]);
			a0 = b2;
		}
		if (a0 >= width)
			break;

		a1 = *a++;
		b1 = *b++;
	}
}

int iso::FaxEncode(memory_block dest, const bitmatrix_aligned<uint32> &srce, FAXMODE mode, int maxk) {
	fax_vlc_out		v(dest);
	uint16			runs[1024];

	int				width = srce.num_cols();
	int				height = srce.num_rows();
	uint16			*a = runs, *b = runs + 512;
	*b = width;

	for (int i = 0, k = 0; i < height; i++) {
		if (!(mode & FAXMODE_NOEOL))
			puteol(v, mode, k != 0);

		auto	line	= srce[i];
		auto	*p		= a;
		for (uint32 x = 0; x < width;) {
			x = line.next(x, false);	// white span
			*p++ = x;
			if (x >= width)
				break;
			x = line.next(x + 1, true);	// black span
			*p++ = x++;
		}

		if (mode & FAXMODE_FIXED2D) {
			Encode2D(v, width, a, b);

		} else if (mode & FAXMODE_2D) {
			if (k == 0) {
				Encode1D(v, width, a);
				k = maxk;
			} else {
				Encode2D(v, width, a, b);
			}
			if (mode & FAXMODE_ALIGN)
				v.align(mode & FAXMODE_BYTEALIGN ? 8 : 16);

		} else {
			Encode1D(v, width, a);
		}

		swap(a, b);
	}

	if (!(mode & FAXMODE_NORTC)) {
		for (int i = 0; i < 6; i++)
			puteol(v, mode, false);
	}

	v.flush();
	return v.get_stream().tell();
}
