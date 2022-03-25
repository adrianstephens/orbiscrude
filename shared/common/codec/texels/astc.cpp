#include "base/vector.h"
#include "maths/geometry.h"
#include "astc.h"
#include "utilities.h"

namespace iso {

//-----------------------------------------------------------------------------
//	ASTC: Adaptive Scalable Texture Compression
//	http://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/nystad12_astc.pdf
//-----------------------------------------------------------------------------

template<int N> struct raw_line			{ vec<float,N> a, b; };
template<int N> struct processed_line	{ vec<float,N> amod, bs, bis; };

enum Format {
	FMT_LUMINANCE					= 0,	//1
	FMT_LUMINANCE_DELTA				= 1,	//1
	FMT_HDR_LUMINANCE_LARGE_RANGE	= 2,	//1
	FMT_HDR_LUMINANCE_SMALL_RANGE	= 3,	//1
	FMT_LUMINANCE_ALPHA				= 4,	//2
	FMT_LUMINANCE_ALPHA_DELTA		= 5,	//2
	FMT_RGB_SCALE					= 6,	//2
	FMT_HDR_RGB_SCALE				= 7,	//2
	FMT_RGB							= 8,	//3
	FMT_RGB_DELTA					= 9,	//3
	FMT_RGB_SCALE_ALPHA				= 10,	//3
	FMT_HDR_RGB						= 11,	//3
	FMT_RGBA						= 12,	//4
	FMT_RGBA_DELTA					= 13,	//4
	FMT_HDR_RGB_LDR_ALPHA			= 14,	//4
	FMT_HDR_RGBA					= 15,	//4
};

int NumComps(Format f) {
	return (f >> 2) + 1;
}

//-----------------------------------------------------------------------------
//	Quantisaton
//-----------------------------------------------------------------------------

enum Quant {
	QUANT_2							= 0,
	QUANT_3							= 1,
	QUANT_4							= 2,
	QUANT_5							= 3,
	QUANT_6							= 4,
	QUANT_8							= 5,
	QUANT_10						= 6,
	QUANT_12						= 7,
	QUANT_16						= 8,
	QUANT_20						= 9,
	QUANT_24						= 10,
	QUANT_32						= 11,
	QUANT_40						= 12,
	QUANT_48						= 13,
	QUANT_64						= 14,
	QUANT_80						= 15,
	QUANT_96						= 16,
	QUANT_128						= 17,
	QUANT_160						= 18,
	QUANT_192						= 19,
	QUANT_256						= 20,
	QUANT_ILLEGAL					= 255,
};

enum ISE {ISE_NONE, ISE_TRITS, ISE_QUINTS};

struct QuantInfo {
	uint8	bits, mode, max;
} quant_info[] = {
	{1, ISE_NONE,	2	- 1},	//QUANT_2
	{0, ISE_TRITS,	3	- 1},	//QUANT_3
	{2, ISE_NONE,	4	- 1},	//QUANT_4
	{0, ISE_QUINTS,	5	- 1},	//QUANT_5
	{1, ISE_TRITS,	6	- 1},	//QUANT_6
	{3, ISE_NONE,	8	- 1},	//QUANT_8
	{1, ISE_QUINTS,	10	- 1},	//QUANT_10
	{2, ISE_TRITS,	12	- 1},	//QUANT_12
	{4, ISE_NONE,	16	- 1},	//QUANT_16
	{2, ISE_QUINTS,	20	- 1},	//QUANT_20
	{3, ISE_TRITS,	24	- 1},	//QUANT_24
	{5, ISE_NONE,	32	- 1},	//QUANT_32
	{3, ISE_QUINTS,	40	- 1},	//QUANT_40
	{4, ISE_TRITS,	48	- 1},	//QUANT_48
	{6, ISE_NONE,	64	- 1},	//QUANT_64
	{4, ISE_QUINTS,	80	- 1},	//QUANT_80
	{5, ISE_TRITS,	96	- 1},	//QUANT_96
	{7, ISE_NONE,	128	- 1},	//QUANT_128
	{5, ISE_QUINTS,	160	- 1},	//QUANT_160
	{6, ISE_TRITS,	192	- 1},	//QUANT_192
	{8, ISE_NONE,	256	- 1},	//QUANT_256
};

int ISE_bits(Quant quant, int items) {
	uint32	t = items * quant_info[quant].bits;
	switch (quant_info[quant].mode) {
		default:			return t;
		case ISE_TRITS:		return t + (8 * items + 4) / 5;
		case ISE_QUINTS:	return t + (7 * items + 2) / 3;
	}
}

inline int UnQuantWgt(Quant qmode, uint8 i) {
	uint32	m	= quant_info[qmode].max;
	return i < (m + 1) / 2 ? i * 66 / (m + 1) : 64 - (m - i) * 66 / (m + 1);
}
inline int UnQuantCol(Quant qmode, uint8 i) {
	uint32	m	= quant_info[qmode].max;
	return i < (m + 1) / 2 ? i * 256 / (m + 1) : 255 - (m - i) * 256 / (m + 1);
}
inline float UnQuantFloat(Quant qmode, int i) {
	return UnQuantCol(qmode, i) / 255.f;
}

// from 0..1023
int ClosestQuant1024(Quant qmode, int i) {
	uint32	m	= quant_info[qmode].max;
	return i < 512 ? i * (m + 1) / 1024 : m - (1023 - i) * (m + 1) / 1024;
}

// from 0..255
int ClosestQuant(Quant qmode, int i) {
	uint32	m	= quant_info[qmode].max;
	return i < 128 ? i * (m + 1) / 256 : m - (255 - i) * (m + 1) / 256;
}

struct trit {
	static const trit trits_of_integer[256];
	static const uint8 integer_of_trits[3][3][3][3][3];

	uint16	a : 2, b : 2, c : 2, d : 2, e : 2;
	constexpr trit(uint8 a, uint8 b, uint8 c, uint8 d, uint8 e) : a(a), b(b), c(c), d(d), e(e) {}
	constexpr trit(uint8 x)					: trit(trits_of_integer[x]) {}
	constexpr operator uint8() const		{ return integer_of_trits[e][d][c][b][a]; }
};

const trit trit::trits_of_integer[256] = {
	{0, 0, 0, 0, 0},	{1, 0, 0, 0, 0},	{2, 0, 0, 0, 0},	{0, 0, 2, 0, 0},	{0, 1, 0, 0, 0},	{1, 1, 0, 0, 0},	{2, 1, 0, 0, 0},	{1, 0, 2, 0, 0},
	{0, 2, 0, 0, 0},	{1, 2, 0, 0, 0},	{2, 2, 0, 0, 0},	{2, 0, 2, 0, 0},	{0, 2, 2, 0, 0},	{1, 2, 2, 0, 0},	{2, 2, 2, 0, 0},	{2, 0, 2, 0, 0},
	{0, 0, 1, 0, 0},	{1, 0, 1, 0, 0},	{2, 0, 1, 0, 0},	{0, 1, 2, 0, 0},	{0, 1, 1, 0, 0},	{1, 1, 1, 0, 0},	{2, 1, 1, 0, 0},	{1, 1, 2, 0, 0},
	{0, 2, 1, 0, 0},	{1, 2, 1, 0, 0},	{2, 2, 1, 0, 0},	{2, 1, 2, 0, 0},	{0, 0, 0, 2, 2},	{1, 0, 0, 2, 2},	{2, 0, 0, 2, 2},	{0, 0, 2, 2, 2},
	{0, 0, 0, 1, 0},	{1, 0, 0, 1, 0},	{2, 0, 0, 1, 0},	{0, 0, 2, 1, 0},	{0, 1, 0, 1, 0},	{1, 1, 0, 1, 0},	{2, 1, 0, 1, 0},	{1, 0, 2, 1, 0},
	{0, 2, 0, 1, 0},	{1, 2, 0, 1, 0},	{2, 2, 0, 1, 0},	{2, 0, 2, 1, 0},	{0, 2, 2, 1, 0},	{1, 2, 2, 1, 0},	{2, 2, 2, 1, 0},	{2, 0, 2, 1, 0},
	{0, 0, 1, 1, 0},	{1, 0, 1, 1, 0},	{2, 0, 1, 1, 0},	{0, 1, 2, 1, 0},	{0, 1, 1, 1, 0},	{1, 1, 1, 1, 0},	{2, 1, 1, 1, 0},	{1, 1, 2, 1, 0},
	{0, 2, 1, 1, 0},	{1, 2, 1, 1, 0},	{2, 2, 1, 1, 0},	{2, 1, 2, 1, 0},	{0, 1, 0, 2, 2},	{1, 1, 0, 2, 2},	{2, 1, 0, 2, 2},	{1, 0, 2, 2, 2},
	{0, 0, 0, 2, 0},	{1, 0, 0, 2, 0},	{2, 0, 0, 2, 0},	{0, 0, 2, 2, 0},	{0, 1, 0, 2, 0},	{1, 1, 0, 2, 0},	{2, 1, 0, 2, 0},	{1, 0, 2, 2, 0},
	{0, 2, 0, 2, 0},	{1, 2, 0, 2, 0},	{2, 2, 0, 2, 0},	{2, 0, 2, 2, 0},	{0, 2, 2, 2, 0},	{1, 2, 2, 2, 0},	{2, 2, 2, 2, 0},	{2, 0, 2, 2, 0},
	{0, 0, 1, 2, 0},	{1, 0, 1, 2, 0},	{2, 0, 1, 2, 0},	{0, 1, 2, 2, 0},	{0, 1, 1, 2, 0},	{1, 1, 1, 2, 0},	{2, 1, 1, 2, 0},	{1, 1, 2, 2, 0},
	{0, 2, 1, 2, 0},	{1, 2, 1, 2, 0},	{2, 2, 1, 2, 0},	{2, 1, 2, 2, 0},	{0, 2, 0, 2, 2},	{1, 2, 0, 2, 2},	{2, 2, 0, 2, 2},	{2, 0, 2, 2, 2},
	{0, 0, 0, 0, 2},	{1, 0, 0, 0, 2},	{2, 0, 0, 0, 2},	{0, 0, 2, 0, 2},	{0, 1, 0, 0, 2},	{1, 1, 0, 0, 2},	{2, 1, 0, 0, 2},	{1, 0, 2, 0, 2},
	{0, 2, 0, 0, 2},	{1, 2, 0, 0, 2},	{2, 2, 0, 0, 2},	{2, 0, 2, 0, 2},	{0, 2, 2, 0, 2},	{1, 2, 2, 0, 2},	{2, 2, 2, 0, 2},	{2, 0, 2, 0, 2},
	{0, 0, 1, 0, 2},	{1, 0, 1, 0, 2},	{2, 0, 1, 0, 2},	{0, 1, 2, 0, 2},	{0, 1, 1, 0, 2},	{1, 1, 1, 0, 2},	{2, 1, 1, 0, 2},	{1, 1, 2, 0, 2},
	{0, 2, 1, 0, 2},	{1, 2, 1, 0, 2},	{2, 2, 1, 0, 2},	{2, 1, 2, 0, 2},	{0, 2, 2, 2, 2},	{1, 2, 2, 2, 2},	{2, 2, 2, 2, 2},	{2, 0, 2, 2, 2},
	{0, 0, 0, 0, 1},	{1, 0, 0, 0, 1},	{2, 0, 0, 0, 1},	{0, 0, 2, 0, 1},	{0, 1, 0, 0, 1},	{1, 1, 0, 0, 1},	{2, 1, 0, 0, 1},	{1, 0, 2, 0, 1},
	{0, 2, 0, 0, 1},	{1, 2, 0, 0, 1},	{2, 2, 0, 0, 1},	{2, 0, 2, 0, 1},	{0, 2, 2, 0, 1},	{1, 2, 2, 0, 1},	{2, 2, 2, 0, 1},	{2, 0, 2, 0, 1},
	{0, 0, 1, 0, 1},	{1, 0, 1, 0, 1},	{2, 0, 1, 0, 1},	{0, 1, 2, 0, 1},	{0, 1, 1, 0, 1},	{1, 1, 1, 0, 1},	{2, 1, 1, 0, 1},	{1, 1, 2, 0, 1},
	{0, 2, 1, 0, 1},	{1, 2, 1, 0, 1},	{2, 2, 1, 0, 1},	{2, 1, 2, 0, 1},	{0, 0, 1, 2, 2},	{1, 0, 1, 2, 2},	{2, 0, 1, 2, 2},	{0, 1, 2, 2, 2},
	{0, 0, 0, 1, 1},	{1, 0, 0, 1, 1},	{2, 0, 0, 1, 1},	{0, 0, 2, 1, 1},	{0, 1, 0, 1, 1},	{1, 1, 0, 1, 1},	{2, 1, 0, 1, 1},	{1, 0, 2, 1, 1},
	{0, 2, 0, 1, 1},	{1, 2, 0, 1, 1},	{2, 2, 0, 1, 1},	{2, 0, 2, 1, 1},	{0, 2, 2, 1, 1},	{1, 2, 2, 1, 1},	{2, 2, 2, 1, 1},	{2, 0, 2, 1, 1},
	{0, 0, 1, 1, 1},	{1, 0, 1, 1, 1},	{2, 0, 1, 1, 1},	{0, 1, 2, 1, 1},	{0, 1, 1, 1, 1},	{1, 1, 1, 1, 1},	{2, 1, 1, 1, 1},	{1, 1, 2, 1, 1},
	{0, 2, 1, 1, 1},	{1, 2, 1, 1, 1},	{2, 2, 1, 1, 1},	{2, 1, 2, 1, 1},	{0, 1, 1, 2, 2},	{1, 1, 1, 2, 2},	{2, 1, 1, 2, 2},	{1, 1, 2, 2, 2},
	{0, 0, 0, 2, 1},	{1, 0, 0, 2, 1},	{2, 0, 0, 2, 1},	{0, 0, 2, 2, 1},	{0, 1, 0, 2, 1},	{1, 1, 0, 2, 1},	{2, 1, 0, 2, 1},	{1, 0, 2, 2, 1},
	{0, 2, 0, 2, 1},	{1, 2, 0, 2, 1},	{2, 2, 0, 2, 1},	{2, 0, 2, 2, 1},	{0, 2, 2, 2, 1},	{1, 2, 2, 2, 1},	{2, 2, 2, 2, 1},	{2, 0, 2, 2, 1},
	{0, 0, 1, 2, 1},	{1, 0, 1, 2, 1},	{2, 0, 1, 2, 1},	{0, 1, 2, 2, 1},	{0, 1, 1, 2, 1},	{1, 1, 1, 2, 1},	{2, 1, 1, 2, 1},	{1, 1, 2, 2, 1},
	{0, 2, 1, 2, 1},	{1, 2, 1, 2, 1},	{2, 2, 1, 2, 1},	{2, 1, 2, 2, 1},	{0, 2, 1, 2, 2},	{1, 2, 1, 2, 2},	{2, 2, 1, 2, 2},	{2, 1, 2, 2, 2},
	{0, 0, 0, 1, 2},	{1, 0, 0, 1, 2},	{2, 0, 0, 1, 2},	{0, 0, 2, 1, 2},	{0, 1, 0, 1, 2},	{1, 1, 0, 1, 2},	{2, 1, 0, 1, 2},	{1, 0, 2, 1, 2},
	{0, 2, 0, 1, 2},	{1, 2, 0, 1, 2},	{2, 2, 0, 1, 2},	{2, 0, 2, 1, 2},	{0, 2, 2, 1, 2},	{1, 2, 2, 1, 2},	{2, 2, 2, 1, 2},	{2, 0, 2, 1, 2},
	{0, 0, 1, 1, 2},	{1, 0, 1, 1, 2},	{2, 0, 1, 1, 2},	{0, 1, 2, 1, 2},	{0, 1, 1, 1, 2},	{1, 1, 1, 1, 2},	{2, 1, 1, 1, 2},	{1, 1, 2, 1, 2},
	{0, 2, 1, 1, 2},	{1, 2, 1, 1, 2},	{2, 2, 1, 1, 2},	{2, 1, 2, 1, 2},	{0, 2, 2, 2, 2},	{1, 2, 2, 2, 2},	{2, 2, 2, 2, 2},	{2, 1, 2, 2, 2},
};

const uint8 trit::integer_of_trits[3][3][3][3][3] = {
	//				b=0					b=1					b=2
	//			a=0,1,2				a=0,1,2				a=0,1,2
	{	//e=0
		{		//d=0
			{	{0, 1, 2},			{4, 5, 6},			{8, 9, 10},			},	//c=0
			{	{16, 17, 18},		{20, 21, 22},		{24, 25, 26},		},	//c=1
			{	{3, 7, 15},			{19, 23, 27},		{12, 13, 14},		},	//c=2
		}, {	//d=1
			{	{32, 33, 34},		{36, 37, 38},		{40, 41, 42},		},	//c=0
			{	{48, 49, 50},		{52, 53, 54},		{56, 57, 58},		},	//c=1
			{	{35, 39, 47},		{51, 55, 59},		{44, 45, 46},		},	//c=2
		}, {	//d=2
			{	{64, 65, 66},		{68, 69, 70},		{72, 73, 74},		},	//c=0
			{	{80, 81, 82},		{84, 85, 86},		{88, 89, 90},		},	//c=1
			{	{67, 71, 79},		{83, 87, 91},		{76, 77, 78},		},	//c=2
		},
	}, {//e=1
		{		//d=0
			{	{128, 129, 130},	{132, 133, 134},	{136, 137, 138},	},	//c=0
			{	{144, 145, 146},	{148, 149, 150},	{152, 153, 154},	},	//c=1
			{	{131, 135, 143},	{147, 151, 155},	{140, 141, 142},	},	//c=2
		}, {	//d=1
			{	{160, 161, 162},	{164, 165, 166},	{168, 169, 170},	},	//c=0
			{	{176, 177, 178},	{180, 181, 182},	{184, 185, 186},	},	//c=1
			{	{163, 167, 175},	{179, 183, 187},	{172, 173, 174},	},	//c=2
		}, {	//d=2
			{	{192, 193, 194},	{196, 197, 198},	{200, 201, 202},	},	//c=0
			{	{208, 209, 210},	{212, 213, 214},	{216, 217, 218},	},	//c=1
			{	{195, 199, 207},	{211, 215, 219},	{204, 205, 206},	},	//c=2
		},
	}, {//e=2
		{		//d=0
			{	{96, 97, 98},		{100, 101, 102},	{104, 105, 106},	},	//c=0
			{	{112, 113, 114},	{116, 117, 118},	{120, 121, 122},	},	//c=1
			{	{99, 103, 111},		{115, 119, 123},	{108, 109, 110},	},	//c=2
		}, {	//d=1
			{	{224, 225, 226},	{228, 229, 230},	{232, 233, 234},	},	//c=0
			{	{240, 241, 242},	{244, 245, 246},	{248, 249, 250},	},	//c=1
			{	{227, 231, 239},	{243, 247, 251},	{236, 237, 238},	},	//c=2
		}, {	//d=2
			{	{28, 29, 30},		{60, 61, 62},		{92, 93, 94},		},	//c=0
			{	{156, 157, 158},	{188, 189, 190},	{220, 221, 222},	},	//c=1
			{	{31, 63, 127},		{159, 191, 255},	{252, 253, 254},	},	//c=2
		},
	},
};


struct quint	{
	static const quint quints_of_integer[128];
	static const uint8 integer_of_quints[5][5][5];

	uint16	a : 3, b : 3, c : 3;
	constexpr quint(uint8 a, uint8 b, uint8 c) : a(a), b(b), c(c) {}
	constexpr quint(uint8 x)				: quint(quints_of_integer[x]) {}
	constexpr operator uint8() const		{ return integer_of_quints[c][b][a]; }
};

const quint quint::quints_of_integer[128] = {
	{0, 0, 0},	{1, 0, 0},	{2, 0, 0},	{3, 0, 0},	{4, 0, 0},	{0, 4, 0},	{4, 4, 0},	{4, 4, 4},
	{0, 1, 0},	{1, 1, 0},	{2, 1, 0},	{3, 1, 0},	{4, 1, 0},	{1, 4, 0},	{4, 4, 1},	{4, 4, 4},
	{0, 2, 0},	{1, 2, 0},	{2, 2, 0},	{3, 2, 0},	{4, 2, 0},	{2, 4, 0},	{4, 4, 2},	{4, 4, 4},
	{0, 3, 0},	{1, 3, 0},	{2, 3, 0},	{3, 3, 0},	{4, 3, 0},	{3, 4, 0},	{4, 4, 3},	{4, 4, 4},
	{0, 0, 1},	{1, 0, 1},	{2, 0, 1},	{3, 0, 1},	{4, 0, 1},	{0, 4, 1},	{4, 0, 4},	{0, 4, 4},
	{0, 1, 1},	{1, 1, 1},	{2, 1, 1},	{3, 1, 1},	{4, 1, 1},	{1, 4, 1},	{4, 1, 4},	{1, 4, 4},
	{0, 2, 1},	{1, 2, 1},	{2, 2, 1},	{3, 2, 1},	{4, 2, 1},	{2, 4, 1},	{4, 2, 4},	{2, 4, 4},
	{0, 3, 1},	{1, 3, 1},	{2, 3, 1},	{3, 3, 1},	{4, 3, 1},	{3, 4, 1},	{4, 3, 4},	{3, 4, 4},
	{0, 0, 2},	{1, 0, 2},	{2, 0, 2},	{3, 0, 2},	{4, 0, 2},	{0, 4, 2},	{2, 0, 4},	{3, 0, 4},
	{0, 1, 2},	{1, 1, 2},	{2, 1, 2},	{3, 1, 2},	{4, 1, 2},	{1, 4, 2},	{2, 1, 4},	{3, 1, 4},
	{0, 2, 2},	{1, 2, 2},	{2, 2, 2},	{3, 2, 2},	{4, 2, 2},	{2, 4, 2},	{2, 2, 4},	{3, 2, 4},
	{0, 3, 2},	{1, 3, 2},	{2, 3, 2},	{3, 3, 2},	{4, 3, 2},	{3, 4, 2},	{2, 3, 4},	{3, 3, 4},
	{0, 0, 3},	{1, 0, 3},	{2, 0, 3},	{3, 0, 3},	{4, 0, 3},	{0, 4, 3},	{0, 0, 4},	{1, 0, 4},
	{0, 1, 3},	{1, 1, 3},	{2, 1, 3},	{3, 1, 3},	{4, 1, 3},	{1, 4, 3},	{0, 1, 4},	{1, 1, 4},
	{0, 2, 3},	{1, 2, 3},	{2, 2, 3},	{3, 2, 3},	{4, 2, 3},	{2, 4, 3},	{0, 2, 4},	{1, 2, 4},
	{0, 3, 3},	{1, 3, 3},	{2, 3, 3},	{3, 3, 3},	{4, 3, 3},	{3, 4, 3},	{0, 3, 4},	{1, 3, 4},
};

const uint8 quint::integer_of_quints[5][5][5] = {
	//		b=0							b=1							b=2							b=3							b=4
	//	a=0,1,2,3,4					a=0,1,2,3,4					a=0,1,2,3,4					a=0,1,2,3,4					a=0,1,2,3,4
	{	{0, 1, 2, 3, 4},			{8, 9, 10, 11, 12},			{16, 17, 18, 19, 20},		{24, 25, 26, 27, 28},		{5, 13, 21, 29, 6},			},	//c=0
	{	{32, 33, 34, 35, 36},		{40, 41, 42, 43, 44},		{48, 49, 50, 51, 52},		{56, 57, 58, 59, 60},		{37, 45, 53, 61, 14},		},	//c=1
	{	{64, 65, 66, 67, 68},		{72, 73, 74, 75, 76},		{80, 81, 82, 83, 84},		{88, 89, 90, 91, 92},		{69, 77, 85, 93, 22},		},	//c=2
	{	{96, 97, 98, 99, 100},		{104, 105, 106, 107, 108},	{112, 113, 114, 115, 116},	{120, 121, 122, 123, 124},	{101, 109, 117, 125, 30},	},	//c=3
	{	{102, 103, 70, 71, 38},		{110, 111, 78, 79, 46},		{118, 119, 86, 87, 54},		{126, 127, 94, 95, 62},		{39, 47, 55, 63, 31},		},	//c=4
};


uint8 add_trit(uint8 x, uint8 t, uint8 bits) {
	int	b	= ((x & 1) << bits) >> 1;
	if (b) {
		t = 2 - t;
		x = ~x;
	}
	b |= ((x >> 1) & ((1 << bits) - 1) >> 1);
	return b * 3 + t;
}

void combine_trit(uint8 &r, uint8 t, uint8 bits) {
	r = add_trit(r, t, bits);
}

uint8 add_quint(uint8 x, uint8 t, uint8 bits) {
	int	b	= ((x & 1) << bits) >> 1;
	if (b) {
		t = 4 - t;
		x = ~x;
	}
	b |= ((x >> 1) & ((1 << bits) - 1) >> 1);
	return b * 5 + t;
}

void combine_quint(uint8 &r, uint8 t, uint8 bits) {
	r = add_quint(r, t, bits);
}

void ISE_decode(Quant quant, int elements, const uint8 *input_data, uint8 *output_data, int bit_offset) {
	if (elements > 64) {
		for (int i = 0; i < elements; i++)
			output_data[i] = 0;
		return;
	}

	uint8	results[68];
	uint8	tq_blocks[22];

	int		bits	= quant_info[quant].bits;
	ISE		ise		= ISE(quant_info[quant].mode);
	uint8	topbit	= (1 << bits) >> 1;
	uint8	botbits	= ((1 << bits) - 1) >> 1;
	uint8	allbits	= (1 << bits) - 1;

	clear(tq_blocks);

	// collect bits for each element, as well as bits for any trit-blocks and quint-blocks.
	for (int i = 0, j = 0, b = 0, s = 0; i < elements; i++) {
		results[i] = read_bits(input_data, bit_offset, bits);
		bit_offset += bits;
		switch (ise) {
			case ISE_TRITS: {
				static const uint8 bits_to_read[5]	= { 2, 2, 1, 2, 1 };
				int	trit_bits = bits_to_read[j];
				tq_blocks[b] |= read_bits(input_data, bit_offset, trit_bits) << s;
				bit_offset	+= trit_bits;
				s	+= trit_bits;
				if (++j == 5) {
					++b;
					j = s = 0;
				}
				break;
			}
			case ISE_QUINTS: {
				static const uint8 bits_to_read[3]	= { 3, 2, 2 };
				int	quint_bits = bits_to_read[j];
				tq_blocks[b] |= read_bits(input_data, bit_offset, quint_bits) << s;
				bit_offset	+= quint_bits;
				s	+= quint_bits;
				if (++j == 3) {
					++b;
					j = s = 0;
				}
				break;
			}
		}
	}

	// unpack trit-blocks or quint-blocks as needed
	switch (ise) {
		case ISE_TRITS:
			for (int i = 0, n = (elements + 4) / 5; i < n; i++) {
				trit	t(tq_blocks[i]);
				combine_trit(results[5 * i + 0], t.a, bits);
				combine_trit(results[5 * i + 1], t.b, bits);
				combine_trit(results[5 * i + 2], t.c, bits);
				combine_trit(results[5 * i + 3], t.d, bits);
				combine_trit(results[5 * i + 4], t.e, bits);
			}
			break;
		case ISE_QUINTS:
			for (int i = 0, n = (elements + 2) / 3; i < n; i++) {
				quint	t(tq_blocks[i]);
				combine_quint(results[3 * i + 0], t.a, bits);
				combine_quint(results[3 * i + 1], t.b, bits);
				combine_quint(results[3 * i + 2], t.c, bits);
			}
			break;
	}
	for (int i = 0; i < elements; i++)
		output_data[i] = results[i];
}

void ISE_encode(Quant quant, int elements, const uint8 *input_data, uint8 *output_data, int bit_offset) {
}

struct QuantizationTable {
	struct row {
		int8	table[128];
		int8&	operator[](int i)		{ return table[i]; }
		Quant	operator[](int i) const { return Quant(table[i]); }
	};
	row table[17];

	QuantizationTable() {
		for (int i = 0; i <= 16; i++) {
			for (int j = 0; j < 128; j++)
				table[i][j] = -1;
		}

		for (int i = 0; i < 21; i++) {
			for (int j = 1; j <= 16; j++) {
				int p = ISE_bits((Quant)i, 2 * j);
				if (p < 128)
					table[j][p] = i;
			}
		}
		for (int i = 0; i <= 16; i++) {
			int max = -1;
			for (int j = 0; j < 128; j++) {
				if (table[i][j] > max)
					max = table[i][j];
				else
					table[i][j] = max;
			}
		}
	}
	const row	&operator[](int i) const { return table[i]; }

} quantization_table;

//-----------------------------------------------------------------------------
//	Format
//-----------------------------------------------------------------------------

bool rgb_delta_unpack(const uint8 input[6], Quant qmode, uint16x4 output[2]) {
	// unquantize the color endpoints
	int r0 = input[0];
	int g0 = input[2];
	int b0 = input[4];

	int r1 = input[1];
	int g1 = input[3];
	int b1 = input[5];

	// perform the bit-transfer procedure
	r0 |= (r1 & 0x80) << 1;
	g0 |= (g1 & 0x80) << 1;
	b0 |= (b1 & 0x80) << 1;
	r1 &= 0x7F;
	g1 &= 0x7F;
	b1 &= 0x7F;
	if (r1 & 0x40)
		r1 -= 0x80;
	if (g1 & 0x40)
		g1 -= 0x80;
	if (b1 & 0x40)
		b1 -= 0x80;

	r0 >>= 1;
	g0 >>= 1;
	b0 >>= 1;
	r1 >>= 1;
	g1 >>= 1;
	b1 >>= 1;

	bool	retval = r1 + g1 + b1 < 0;

	r1 += r0;
	g1 += g0;
	b1 += b0;

	if (retval) {
		swap(r0, r1);
		swap(g0, g1);
		swap(b0, b1);
		r0 = (r0 + b0) >> 1;
		g0 = (g0 + b0) >> 1;
		r1 = (r1 + b1) >> 1;
		g1 = (g1 + b1) >> 1;
	}

	output[0].x = clamp(r0, 0, 255);
	output[0].y = clamp(g0, 0, 255);
	output[0].z = clamp(b0, 0, 255);
	output[0].w = 0xFF;

	output[1].x = clamp(r1, 0, 255);
	output[1].y = clamp(g1, 0, 255);
	output[1].z = clamp(b1, 0, 255);
	output[1].w = 0xFF;

	return retval;
}

bool rgb_unpack(const uint8 input[6], Quant qmode, uint16x4 output[2]) {
	int ri0b = input[0];
	int ri1b = input[1];
	int gi0b = input[2];
	int gi1b = input[3];
	int bi0b = input[4];
	int bi1b = input[5];

	if (ri0b + gi0b + bi0b > ri1b + gi1b + bi1b) {
		// blue-contraction
		output[0].x = (ri1b + bi1b) >> 1;
		output[0].y = (gi1b + bi1b) >> 1;
		output[0].z = bi1b;
		output[0].w = 255;

		output[1].x = (ri0b + bi0b) >> 1;
		output[1].y = (gi0b + bi0b) >> 1;
		output[1].z = bi0b;
		output[1].w = 255;
		return true;
	} else {
		output[0].x = ri0b;
		output[0].y = gi0b;
		output[0].z = bi0b;
		output[0].w = 255;

		output[1].x = ri1b;
		output[1].y = gi1b;
		output[1].z = bi1b;
		output[1].w = 255;
		return false;
	}
}

void rgb_scale_unpack(const uint8 input[4], Quant qmode, uint16x4 output[2]) {
	uint16 ir = input[0];
	uint16 ig = input[1];
	uint16 ib = input[2];

	int iscale = input[3];

	output[1] = uint16x4{ir, ig, ib, 255};
	output[0] = uint16x4{uint16((ir * iscale) >> 8), uint16((ig * iscale) >> 8), uint16((ib * iscale) >> 8), 255};
}

void hdr_rgb_unpack3(const uint8 input[6], Quant qmode, uint16x4 output[2]) {
	int v0 = input[0];
	int v1 = input[1];
	int v2 = input[2];
	int v3 = input[3];
	int v4 = input[4];
	int v5 = input[5];

	// extract all the fixed-placement bitfields
	int modeval = ((v1 & 0x80) >> 7) | (((v2 & 0x80) >> 7) << 1) | (((v3 & 0x80) >> 7) << 2);
	int majcomp = ((v4 & 0x80) >> 7) | (((v5 & 0x80) >> 7) << 1);

	if (majcomp == 3) {
		output[0] = uint16x4{uint16(v0 << 8), uint16(v2 << 8), uint16((v4 & 0x7F) << 9), 0x7800};
		output[1] = uint16x4{uint16(v1 << 8), uint16(v3 << 8), uint16((v5 & 0x7F) << 9), 0x7800};
		return;
	}

	int a	= v0 | ((v1 & 0x40) << 2);
	int b0	= v2 & 0x3f;
	int b1	= v3 & 0x3f;
	int c	= v1 & 0x3f;
	int d0	= v4 & 0x7f;
	int d1	= v5 & 0x7f;

	// get hold of the number of bits in 'd0' and 'd1'
	static const int dbits_tab[8] = { 7, 6, 7, 6, 5, 6, 5, 6 };
	int dbits = dbits_tab[modeval];

	// extract six variable-placement bits
	int bit0 = (v2 >> 6) & 1;
	int bit1 = (v3 >> 6) & 1;
	int bit2 = (v4 >> 6) & 1;
	int bit3 = (v5 >> 6) & 1;
	int bit4 = (v4 >> 5) & 1;
	int bit5 = (v5 >> 5) & 1;

	// and prepend the variable-placement bits depending on mode.
	int ohmod = 1 << modeval;	// one-hot-mode
	if (ohmod & 0xA4)
		a |= bit0 << 9;
	if (ohmod & 0x8)
		a |= bit2 << 9;
	if (ohmod & 0x50)
		a |= bit4 << 9;

	if (ohmod & 0x50)
		a |= bit5 << 10;
	if (ohmod & 0xA0)
		a |= bit1 << 10;

	if (ohmod & 0xC0)
		a |= bit2 << 11;

	if (ohmod & 0x4)
		c |= bit1 << 6;
	if (ohmod & 0xE8)
		c |= bit3 << 6;

	if (ohmod & 0x20)
		c |= bit2 << 7;


	if (ohmod & 0x5B)
		b0 |= bit0 << 6;
	if (ohmod & 0x5B)
		b1 |= bit1 << 6;

	if (ohmod & 0x12)
		b0 |= bit2 << 7;
	if (ohmod & 0x12)
		b1 |= bit3 << 7;

	if (ohmod & 0xAF)
		d0 |= bit4 << 5;
	if (ohmod & 0xAF)
		d1 |= bit5 << 5;
	if (ohmod & 0x5)
		d0 |= bit2 << 6;
	if (ohmod & 0x5)
		d1 |= bit3 << 6;

	// sign-extend 'd0' and 'd1'
	// note: this code assumes that signed right-shift actually sign-fills, not zero-fills.
	int d0x = d0;
	int d1x = d1;
	int sx_shamt = 32 - dbits;
	d0x <<= sx_shamt;
	d0x >>= sx_shamt;
	d1x <<= sx_shamt;
	d1x >>= sx_shamt;
	d0	= d0x;
	d1	= d1x;

	// expand all values to 12 bits, with left-shift as needed.
	int val_shamt = (modeval >> 1) ^ 3;
	a	<<= val_shamt;
	b0	<<= val_shamt;
	b1	<<= val_shamt;
	c	<<= val_shamt;
	d0	<<= val_shamt;
	d1	<<= val_shamt;

	// then compute the actual color values.
	int red1	= a;
	int green1	= a - b0;
	int blue1	= a - b1;
	int red0	= a - c;
	int green0	= a - b0 - c - d0;
	int blue0	= a - b1 - c - d1;

	// switch around the color components
	switch (majcomp) {
		case 1:					// switch around red and green
			swap(red0, green0);
			swap(red1, green1);
			break;
		case 2:					// switch around red and blue
			swap(red0, blue0);
			swap(red1, blue1);
			break;
		case 0:					// no switch
			break;
	}
	output[0] = uint16x4{uint16(clamp(red0, 0, 0xfff) << 4), uint16(clamp(green0, 0, 0xfff) << 4), uint16(clamp(blue0, 0, 0xfff) << 4), 0x7800};
	output[1] = uint16x4{uint16(clamp(red1, 0, 0xfff) << 4), uint16(clamp(green1, 0, 0xfff) << 4), uint16(clamp(blue1, 0, 0xfff) << 4), 0x7800};
}

int unpack_color_endpoints(Format format, Quant qmode, const uint8 *input, uint16x4 output[2]) {
	switch (format) {
		case FMT_LUMINANCE: {
			uint16 lum0 = input[0];
			uint16 lum1 = input[1];
			output[0] = concat(uint16x3(lum0), 255);
			output[1] = concat(uint16x3(lum1), 255);
			return 0;
		}

		case FMT_LUMINANCE_DELTA: {
			uint16 v0 = input[0];
			uint16 v1 = input[1];
			uint16 l0 = (v0 >> 2) | (v1 & 0xC0);
			uint16 l1 = min(l0 + (v1 & 0x3F), 255);
			output[0] = concat(uint16x3(l0), 255);
			output[1] = concat(uint16x3(l1), 255);
			return 0;
		}

		case FMT_HDR_LUMINANCE_SMALL_RANGE: {
			int v0 = input[0];
			int v1 = input[1];

			int y0, y1;
			if (v0 & 0x80) {
				y0 = ((v1 & 0xE0) << 4) | ((v0 & 0x7F) << 2);
				y1 = (v1 & 0x1F) << 2;
			} else {
				y0 = ((v1 & 0xF0) << 4) | ((v0 & 0x7F) << 1);
				y1 = (v1 & 0xF) << 1;
			}
			y1 = min(y1 + y0, 0xFFF);

			output[0] = concat(uint16x3(y0 << 4), 0x7800);
			output[1] = concat(uint16x3(y1 << 4), 0x7800);
			return 3;
		}

		case FMT_HDR_LUMINANCE_LARGE_RANGE: {
			int v0 = input[0];
			int v1 = input[1];

			int y0, y1;
			if (v1 >= v0) {
				y0 = v0 << 4;
				y1 = v1 << 4;
			} else {
				y0 = (v1 << 4) + 8;
				y1 = (v0 << 4) - 8;
			}
			output[0] = concat(uint16x3(y0 << 4), 0x7800);
			output[1] = concat(uint16x3(y1 << 4), 0x7800);
			return 3;
		}

		case FMT_LUMINANCE_ALPHA: {
			output[0] = concat(uint16x3(input[0]), input[2]);
			output[1] = concat(uint16x3(input[1]), input[3]);
			return 0;
		}

		case FMT_LUMINANCE_ALPHA_DELTA: {
			int lum0	= input[0];
			int lum1	= input[1];
			int alpha0	= input[2];
			int alpha1	= input[3];

			lum0 |= (lum1 & 0x80) << 1;
			alpha0 |= (alpha1 & 0x80) << 1;
			lum1 &= 0x7F;
			alpha1 &= 0x7F;
			if (lum1 & 0x40)
				lum1 -= 0x80;
			if (alpha1 & 0x40)
				alpha1 -= 0x80;

			lum0	>>= 1;
			alpha0	>>= 1;
			lum1	= clamp((lum1 >> 1) + lum0, 0, 255);
			alpha1	= clamp((alpha1 >> 1) + alpha0, 0, 255);

			output[0] = concat(uint16x3(lum0), alpha0);
			output[1] = concat(uint16x3(lum1), alpha1);
			return 0;
		}

		case FMT_RGB_SCALE:
			rgb_scale_unpack(input, qmode, output);
			return 0;

		case FMT_RGB_SCALE_ALPHA:
			rgb_scale_unpack(input, qmode, output);
			output[0].w = input[4];
			output[1].w = input[5];
			return 0;

		case FMT_HDR_RGB_SCALE: {
			int v0 = input[0];
			int v1 = input[1];
			int v2 = input[2];
			int v3 = input[3];

			int modeval = ((v0 & 0xC0) >> 6) | (((v1 & 0x80) >> 7) << 2) | (((v2 & 0x80) >> 7) << 3);
			int majcomp;
			int mode;
			if ((modeval & 0xC) != 0xC) {
				majcomp = modeval >> 2;
				mode	= modeval & 3;
			} else if (modeval != 0xF) {
				majcomp = modeval & 3;
				mode	= 4;
			} else {
				majcomp = 0;
				mode	= 5;
			}

			int red		= v0 & 0x3F;
			int green	= v1 & 0x1F;
			int blue	= v2 & 0x1F;
			int scale	= v3 & 0x1F;

			int bit0	= (v1 >> 6) & 1;
			int bit1	= (v1 >> 5) & 1;
			int bit2	= (v2 >> 6) & 1;
			int bit3	= (v2 >> 5) & 1;
			int bit4	= (v3 >> 7) & 1;
			int bit5	= (v3 >> 6) & 1;
			int bit6	= (v3 >> 5) & 1;

			int ohcomp = 1 << mode;

			if (ohcomp & 0x30)
				green |= bit0 << 6;
			if (ohcomp & 0x3A)
				green |= bit1 << 5;
			if (ohcomp & 0x30)
				blue |= bit2 << 6;
			if (ohcomp & 0x3A)
				blue |= bit3 << 5;

			if (ohcomp & 0x3D)
				scale |= bit6 << 5;
			if (ohcomp & 0x2D)
				scale |= bit5 << 6;
			if (ohcomp & 0x04)
				scale |= bit4 << 7;

			if (ohcomp & 0x3B)
				red |= bit4 << 6;
			if (ohcomp & 0x04)
				red |= bit3 << 6;

			if (ohcomp & 0x10)
				red |= bit5 << 7;
			if (ohcomp & 0x0F)
				red |= bit2 << 7;

			if (ohcomp & 0x05)
				red |= bit1 << 8;
			if (ohcomp & 0x0A)
				red |= bit0 << 8;

			if (ohcomp & 0x05)
				red |= bit0 << 9;
			if (ohcomp & 0x02)
				red |= bit6 << 9;

			if (ohcomp & 0x01)
				red |= bit3 << 10;
			if (ohcomp & 0x02)
				red |= bit5 << 10;

			// expand to 12 bits.
			static const int shamts[6] = { 1, 1, 2, 3, 4, 5 };
			int shamt = shamts[mode];
			red		<<= shamt;
			green	<<= shamt;
			blue	<<= shamt;
			scale	<<= shamt;

			// on modes 0 to 4, the values stored for "green" and "blue" are differentials, not absolute values.
			if (mode != 5) {
				green	= red - green;
				blue	= red - blue;
			}

			// switch around components.
			switch (majcomp) {
				case 1:		swap(red, green);	break;
				case 2:		swap(red, blue);	break;
				default:	break;
			}

			output[0]	= concat(to<uint16>(max(int32x3{red, green, blue} - scale, zero)), 0x7800);
			output[1]	= concat(to<uint16>(max(int32x3{red, green, blue}, zero) << 4), 0x7800);
			return 3;
		}

		case FMT_RGB:
			rgb_unpack(input, qmode, output);
			return 0;

		case FMT_RGB_DELTA:
			rgb_delta_unpack(input, qmode, output);
			return 0;

		case FMT_HDR_RGB:
			hdr_rgb_unpack3(input, qmode, output);
			return 3;

		case FMT_RGBA: {
			int order = rgb_unpack(input, qmode, output);
			int a0 = input[6];
			int a1 = input[7];
			output[0].w = order == 0 ? a0 : a1;
			output[1].w = order == 0 ? a1 : a0;
			return 0;
		}

		case FMT_RGBA_DELTA: {
			int order	= rgb_delta_unpack(input, qmode, output);
			int a0		= input[6];
			int a1		= input[7];
			a0 |= (a1 & 0x80) << 1;
			a1 &= 0x7F;
			if (a1 & 0x40)
				a1 -= 0x80;
			a0 >>= 1;
			a1 = clamp(a0 + (a1 >> 1), 0, 255);

			output[0].w = order == 0 ? a0 : a1;
			output[1].w = order == 0 ? a1 : a0;
			return 0;
		}

		case FMT_HDR_RGB_LDR_ALPHA: {
			hdr_rgb_unpack3(input, qmode, output);
			output[0].w = input[6];
			output[1].w = input[7];
			return 1;
		}

		case FMT_HDR_RGBA: {
			hdr_rgb_unpack3(input, qmode, output);
			int v6 = input[6];
			int v7 = input[7];

			int selector = ((v6 >> 7) & 1) | ((v7 >> 6) & 2);
			v6 &= 0x7F;
			v7 &= 0x7F;
			if (selector == 3) {
				v6 <<= 5;
				v7 <<= 5;
			} else {
				v6 |= (v7 << (selector + 1)) & 0x780;
				v7 &= (0x3f >> selector);
				v7 = (v7 ^ (32 >> selector)) - (32 >> selector);
				v6 <<= (4 - selector);
				v7 <<= (4 - selector);
				v7 = clamp(v6 + v7, 0, 0xfff);
			}

			output[0].w = v6 << 4;
			output[1].w = v7 << 4;
			return 3;
		}
		default:
			return -1;
	}
}

static inline int cqt_lookup(Quant qmode, int value) {
	return ClosestQuant(qmode, clamp(value, 0, 255));
}
static inline float clamp255(float val) {
	return clamp(val, 0, 255);
}

static inline float clamp01(float val) {
	return clamp(val, 0, 1);
}

void quantize_rgb(float4 color0, float4 color1, uint8 output[6], Quant qmode) {
	color0.xyz = color0.xyz * (1.0f / 257.0f);
	color1.xyz = color1.xyz * (1.0f / 257.0f);

	float r0 = clamp255(color0.x);
	float g0 = clamp255(color0.y);
	float b0 = clamp255(color0.z);

	float r1 = clamp255(color1.x);
	float g1 = clamp255(color1.y);
	float b1 = clamp255(color1.z);

	int ri0, gi0, bi0, ri1, gi1, bi1;
	int ri0b, gi0b, bi0b, ri1b, gi1b, bi1b;
	float rgb0_addon = 0.5f;
	float rgb1_addon = 0.5f;
	int iters = 0;
	do {
		ri0 = cqt_lookup(qmode, (int)floor(r0 + rgb0_addon));
		gi0 = cqt_lookup(qmode, (int)floor(g0 + rgb0_addon));
		bi0 = cqt_lookup(qmode, (int)floor(b0 + rgb0_addon));
		ri1 = cqt_lookup(qmode, (int)floor(r1 + rgb1_addon));
		gi1 = cqt_lookup(qmode, (int)floor(g1 + rgb1_addon));
		bi1 = cqt_lookup(qmode, (int)floor(b1 + rgb1_addon));

		ri0b = UnQuantCol(qmode, ri0);
		gi0b = UnQuantCol(qmode, gi0);
		bi0b = UnQuantCol(qmode, bi0);
		ri1b = UnQuantCol(qmode, ri1);
		gi1b = UnQuantCol(qmode, gi1);
		bi1b = UnQuantCol(qmode, bi1);

		rgb0_addon -= 0.2f;
		rgb1_addon += 0.2f;
		iters++;
	} while (ri0b + gi0b + bi0b > ri1b + gi1b + bi1b);

	output[0] = ri0;
	output[1] = ri1;
	output[2] = gi0;
	output[3] = gi1;
	output[4] = bi0;
	output[5] = bi1;
}

void quantize_rgba(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	color0.w = color0.w * (1.0f / 257.0f);
	color1.w = color1.w * (1.0f / 257.0f);

	float a0 = clamp255(color0.w);
	float a1 = clamp255(color1.w);
	int ai0 = ClosestQuant(qmode, (int)floor(a0 + 0.5f));
	int ai1 = ClosestQuant(qmode, (int)floor(a1 + 0.5f));

	output[6] = ai0;
	output[7] = ai1;

	quantize_rgb(color0, color1, output, qmode);
}


bool try_quantize_rgb_blue_contract(float4 color0,	// assumed to be the smaller color
	float4 color1,	// assumed to be the larger color
	uint8 output[6], Quant qmode) {
	color0.xyz = color0.xyz * (1.0f / 257.0f);
	color1.xyz = color1.xyz * (1.0f / 257.0f);

	float r0 = color0.x;
	float g0 = color0.y;
	float b0 = color0.z;

	float r1 = color1.x;
	float g1 = color1.y;
	float b1 = color1.z;

	// inverse blue-contraction. This can produce an overflow;
	// just bail out immediately if this is the case.
	r0 += (r0 - b0);
	g0 += (g0 - b0);
	r1 += (r1 - b1);
	g1 += (g1 - b1);

	if (r0 < 0.0f || r0 > 255.0f || g0 < 0.0f || g0 > 255.0f || b0 < 0.0f || b0 > 255.0f ||
		r1 < 0.0f || r1 > 255.0f || g1 < 0.0f || g1 > 255.0f || b1 < 0.0f || b1 > 255.0f) {
		return false;
	}

	// quantize the inverse-blue-contracted color
	int ri0 = ClosestQuant(qmode, (int)floor(r0 + 0.5f));
	int gi0 = ClosestQuant(qmode, (int)floor(g0 + 0.5f));
	int bi0 = ClosestQuant(qmode, (int)floor(b0 + 0.5f));
	int ri1 = ClosestQuant(qmode, (int)floor(r1 + 0.5f));
	int gi1 = ClosestQuant(qmode, (int)floor(g1 + 0.5f));
	int bi1 = ClosestQuant(qmode, (int)floor(b1 + 0.5f));

	// then unquantize again
	int ru0 = UnQuantCol(qmode, ri0);
	int gu0 = UnQuantCol(qmode, gi0);
	int bu0 = UnQuantCol(qmode, bi0);
	int ru1 = UnQuantCol(qmode, ri1);
	int gu1 = UnQuantCol(qmode, gi1);
	int bu1 = UnQuantCol(qmode, bi1);

	// if color #1 is not larger than color #0, then blue-contraction is not a valid approach.
	// note that blue-contraction and quantization may itself change this order, which is why
	// we must only test AFTER blue-contraction.
	if (ru1 + gu1 + bu1 <= ru0 + gu0 + bu0)
		return 0;

	output[0] = ri1;
	output[1] = ri0;
	output[2] = gi1;
	output[3] = gi0;
	output[4] = bi1;
	output[5] = bi0;

	return true;
}

bool try_quantize_rgba_blue_contract(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	color0.w = color0.w * (1.0f / 257.0f);
	color1.w = color1.w * (1.0f / 257.0f);

	float a0 = clamp255(color0.w);
	float a1 = clamp255(color1.w);

	output[7] = ClosestQuant(qmode, (int)floor(a0 + 0.5f));
	output[6] = ClosestQuant(qmode, (int)floor(a1 + 0.5f));

	return try_quantize_rgb_blue_contract(color0, color1, output, qmode);
}

bool try_quantize_rgb_delta(float4 color0, float4 color1, uint8 output[6], Quant qmode) {
	color0.xyz = color0.xyz * (1.0f / 257.0f);
	color1.xyz = color1.xyz * (1.0f / 257.0f);

	float r0 = clamp255(color0.x);
	float g0 = clamp255(color0.y);
	float b0 = clamp255(color0.z);

	float r1 = clamp255(color1.x);
	float g1 = clamp255(color1.y);
	float b1 = clamp255(color1.z);

	// transform r0 to unorm9
	int r0a = (int)floor(r0 + 0.5f);
	int g0a = (int)floor(g0 + 0.5f);
	int b0a = (int)floor(b0 + 0.5f);
	r0a <<= 1;
	g0a <<= 1;
	b0a <<= 1;

	// mask off the top bit
	int r0b = r0a & 0xFF;
	int g0b = g0a & 0xFF;
	int b0b = b0a & 0xFF;

	// quantize, then unquantize in order to get a value that we take
	// differences against.
	int r0be = ClosestQuant(qmode, r0b);
	int g0be = ClosestQuant(qmode, g0b);
	int b0be = ClosestQuant(qmode, b0b);

	r0b = UnQuantCol(qmode, r0be);
	g0b = UnQuantCol(qmode, g0be);
	b0b = UnQuantCol(qmode, b0be);
	r0b |= r0a & 0x100;			// final unquantized-values for endpoint 0.
	g0b |= g0a & 0x100;
	b0b |= b0a & 0x100;

	// then, get hold of the second value
	int r1d = (int)floor(r1 + 0.5f);
	int g1d = (int)floor(g1 + 0.5f);
	int b1d = (int)floor(b1 + 0.5f);

	r1d <<= 1;
	g1d <<= 1;
	b1d <<= 1;
	// and take differences!
	r1d -= r0b;
	g1d -= g0b;
	b1d -= b0b;

	// check if the difference is too large to be encodable.
	if (r1d > 63 || g1d > 63 || b1d > 63 || r1d < -64 || g1d < -64 || b1d < -64)
		return false;

	// insert top bit of the base into the offset
	r1d &= 0x7F;
	g1d &= 0x7F;
	b1d &= 0x7F;

	r1d |= (r0b & 0x100) >> 1;
	g1d |= (g0b & 0x100) >> 1;
	b1d |= (b0b & 0x100) >> 1;

	// then quantize & unquantize; if this causes any of the top two bits to flip,
	// then encoding fails, since we have then corrupted either the top bit of the base
	// or the sign bit of the offset.
	int r1de = ClosestQuant(qmode, r1d);
	int g1de = ClosestQuant(qmode, g1d);
	int b1de = ClosestQuant(qmode, b1d);

	int r1du = UnQuantCol(qmode, r1de);
	int g1du = UnQuantCol(qmode, g1de);
	int b1du = UnQuantCol(qmode, b1de);

	if (((r1d ^ r1du) | (g1d ^ g1du) | (b1d ^ b1du)) & 0xC0)
		return false;

	// check that the sum of the encoded offsets is nonnegative, else encoding fails
	r1du &= 0x7f;
	g1du &= 0x7f;
	b1du &= 0x7f;
	if (r1du & 0x40)
		r1du -= 0x80;
	if (g1du & 0x40)
		g1du -= 0x80;
	if (b1du & 0x40)
		b1du -= 0x80;
	if (r1du + g1du + b1du < 0)
		return 0;

	// check that the offsets produce legitimate sums as well.
	r1du += r0b;
	g1du += g0b;
	b1du += b0b;
	if (r1du < 0 || r1du > 0x1FF || g1du < 0 || g1du > 0x1FF || b1du < 0 || b1du > 0x1FF)
		return 0;

	// OK, we've come this far; we can now encode legitimate values.
	output[0] = r0be;
	output[1] = r1de;
	output[2] = g0be;
	output[3] = g1de;
	output[4] = b0be;
	output[5] = b1de;

	return true;
}


bool try_quantize_rgb_delta_blue_contract(float4 color0, float4 color1, uint8 output[6], Quant qmode) {
	color0.xyz = color0.xyz * (1.0f / 257.0f);
	color1.xyz = color1.xyz * (1.0f / 257.0f);

	// switch around endpoint colors already at start
	float r0 = color1.x;
	float g0 = color1.y;
	float b0 = color1.z;

	float r1 = color0.x;
	float g1 = color0.y;
	float b1 = color0.z;

	// inverse blue-contraction. This step can perform an overflow, in which case we will bail out immediately
	r0 += (r0 - b0);
	g0 += (g0 - b0);
	r1 += (r1 - b1);
	g1 += (g1 - b1);

	if (r0 < 0.0f || r0 > 255.0f || g0 < 0.0f || g0 > 255.0f || b0 < 0.0f || b0 > 255.0f || r1 < 0.0f || r1 > 255.0f || g1 < 0.0f || g1 > 255.0f || b1 < 0.0f || b1 > 255.0f)
		return false;

	// transform r0 to unorm9
	int r0a = (int)floor(r0 + 0.5f);
	int g0a = (int)floor(g0 + 0.5f);
	int b0a = (int)floor(b0 + 0.5f);
	r0a <<= 1;
	g0a <<= 1;
	b0a <<= 1;

	// mask off the top bit
	int r0b = r0a & 0xFF;
	int g0b = g0a & 0xFF;
	int b0b = b0a & 0xFF;

	// quantize, then unquantize in order to get a value that we take differences against
	int r0be = ClosestQuant(qmode, r0b);
	int g0be = ClosestQuant(qmode, g0b);
	int b0be = ClosestQuant(qmode, b0b);

	r0b = UnQuantCol(qmode, r0be);
	g0b = UnQuantCol(qmode, g0be);
	b0b = UnQuantCol(qmode, b0be);
	r0b |= r0a & 0x100;			// final unquantized-values for endpoint 0
	g0b |= g0a & 0x100;
	b0b |= b0a & 0x100;

	// then, get hold of the second value
	int r1d = (int)floor(r1 + 0.5f);
	int g1d = (int)floor(g1 + 0.5f);
	int b1d = (int)floor(b1 + 0.5f);

	r1d <<= 1;
	g1d <<= 1;
	b1d <<= 1;
	// and take differences!
	r1d -= r0b;
	g1d -= g0b;
	b1d -= b0b;

	// check if the difference is too large to be encodable
	if (r1d > 63 || g1d > 63 || b1d > 63 || r1d < -64 || g1d < -64 || b1d < -64)
		return false;

	// insert top bit of the base into the offset
	r1d &= 0x7F;
	g1d &= 0x7F;
	b1d &= 0x7F;

	r1d |= (r0b & 0x100) >> 1;
	g1d |= (g0b & 0x100) >> 1;
	b1d |= (b0b & 0x100) >> 1;

	// then quantize & unquantize
	// if this causes any of the top two bits to flip, then encoding fails, since we have then corrupted either the top bit of the base or the sign bit of the offset
	int r1de = ClosestQuant(qmode, r1d);
	int g1de = ClosestQuant(qmode, g1d);
	int b1de = ClosestQuant(qmode, b1d);

	int r1du = UnQuantCol(qmode, r1de);
	int g1du = UnQuantCol(qmode, g1de);
	int b1du = UnQuantCol(qmode, b1de);

	if (((r1d ^ r1du) | (g1d ^ g1du) | (b1d ^ b1du)) & 0xC0)
		return false;

	// check that the sum of the encoded offsets is negative, else encoding fails note that this is inverse of the test for non-blue-contracted RGB.
	r1du &= 0x7f;
	g1du &= 0x7f;
	b1du &= 0x7f;
	if (r1du & 0x40)
		r1du -= 0x80;
	if (g1du & 0x40)
		g1du -= 0x80;
	if (b1du & 0x40)
		b1du -= 0x80;
	if (r1du + g1du + b1du >= 0)
		return false;

	// check that the offsets produce legitimate sums as well
	r1du += r0b;
	g1du += g0b;
	b1du += b0b;
	if (r1du < 0 || r1du > 0x1FF || g1du < 0 || g1du > 0x1FF || b1du < 0 || b1du > 0x1FF)
		return false;

	// OK, we've come this far; we can now encode legitimate values
	output[0] = r0be;
	output[1] = r1de;
	output[2] = g0be;
	output[3] = g1de;
	output[4] = b0be;
	output[5] = b1de;

	return true;
}

bool try_quantize_alpha_delta(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	color0.w = color0.w * (1.0f / 257.0f);
	color1.w = color1.w * (1.0f / 257.0f);

	// the calculation for alpha-delta is exactly the same as for RGB-delta; see the RGB-delta function for comments
	float a0 = clamp255(color0.w);
	float a1 = clamp255(color1.w);

	int a0a = (int)floor(a0 + 0.5f);
	a0a <<= 1;
	int a0b = a0a & 0xFF;
	int a0be = ClosestQuant(qmode, a0b);
	a0b = UnQuantCol(qmode, a0be);
	a0b |= a0a & 0x100;
	int a1d = (int)floor(a1 + 0.5f);
	a1d <<= 1;
	a1d -= a0b;
	if (a1d > 63 || a1d < -64)
		return false;
	a1d &= 0x7F;
	a1d |= (a0b & 0x100) >> 1;
	int a1de = ClosestQuant(qmode, a1d);
	int a1du = UnQuantCol(qmode, a1de);
	if ((a1d ^ a1du) & 0xC0)
		return false;
	a1du &= 0x7F;
	if (a1du & 0x40)
		a1du -= 0x80;
	a1du += a0b;
	if (a1du < 0 || a1du > 0x1FF)
		return false;
	output[6] = a0be;
	output[7] = a1de;
	return true;
}

bool try_quantize_luminance_alpha_delta(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	float l0 = clamp255((color0.x + color0.y + color0.z) * ((1.0f / 3.0f) * (1.0f / 257.0f)));
	float l1 = clamp255((color1.x + color1.y + color1.z) * ((1.0f / 3.0f) * (1.0f / 257.0f)));
	float a0 = clamp255(color0.w * (1.0f / 257.0f));
	float a1 = clamp255(color1.w * (1.0f / 257.0f));

	int l0a = (int)floor(l0 + 0.5f);
	int a0a = (int)floor(a0 + 0.5f);
	l0a <<= 1;
	a0a <<= 1;
	int l0b = l0a & 0xFF;
	int a0b = a0a & 0xFF;
	int l0be = ClosestQuant(qmode, l0b);
	int a0be = ClosestQuant(qmode, a0b);
	l0b = UnQuantCol(qmode, l0be);
	a0b = UnQuantCol(qmode, a0be);
	l0b |= l0a & 0x100;
	a0b |= a0a & 0x100;
	int l1d = (int)floor(l1 + 0.5f);
	int a1d = (int)floor(a1 + 0.5f);
	l1d <<= 1;
	a1d <<= 1;
	l1d -= l0b;
	a1d -= a0b;
	if (l1d > 63 || l1d < -64)
		return false;
	if (a1d > 63 || a1d < -64)
		return false;
	l1d &= 0x7F;
	a1d &= 0x7F;
	l1d |= (l0b & 0x100) >> 1;
	a1d |= (a0b & 0x100) >> 1;

	int l1de = ClosestQuant(qmode, l1d);
	int a1de = ClosestQuant(qmode, a1d);
	int l1du = UnQuantCol(qmode, l1de);
	int a1du = UnQuantCol(qmode, a1de);
	if ((l1d ^ l1du) & 0xC0)
		return false;
	if ((a1d ^ a1du) & 0xC0)
		return false;
	l1du &= 0x7F;
	a1du &= 0x7F;
	if (l1du & 0x40)
		l1du -= 0x80;
	if (a1du & 0x40)
		a1du -= 0x80;
	l1du += l0b;
	a1du += a0b;
	if (l1du < 0 || l1du > 0x1FF)
		return false;
	if (a1du < 0 || a1du > 0x1FF)
		return false;
	output[0] = l0be;
	output[1] = l1de;
	output[2] = a0be;
	output[3] = a1de;

	return true;
}

int try_quantize_rgba_delta(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	return	try_quantize_alpha_delta(color0, color1, output, qmode)
		&&	try_quantize_rgb_delta(color0, color1, output, qmode);
}

int try_quantize_rgba_delta_blue_contract(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	// notice that for the alpha encoding, we are swapping around color0 and color1;
	// this is because blue-contraction involves swapping around the two colors.
	return	try_quantize_alpha_delta(color1, color0, output, qmode)
		&&	try_quantize_rgb_delta_blue_contract(color0, color1, output, qmode);
}

void quantize_rgbs_new(float4 rgbs_color, uint8 output[4], Quant qmode) {
	rgbs_color.xyz = rgbs_color.xyz / 257.0f;

	float r = clamp255(rgbs_color.x);
	float g = clamp255(rgbs_color.y);
	float b = clamp255(rgbs_color.z);

	int ri = ClosestQuant(qmode, (int)floor(r + 0.5f));
	int gi = ClosestQuant(qmode, (int)floor(g + 0.5f));
	int bi = ClosestQuant(qmode, (int)floor(b + 0.5f));

	int ru = UnQuantCol(qmode, ri);
	int gu = UnQuantCol(qmode, gi);
	int bu = UnQuantCol(qmode, bi);

	float oldcolorsum = rgbs_color.x + rgbs_color.y + rgbs_color.z;
	float newcolorsum = (float)(ru + gu + bu);

	float scale = clamp01(rgbs_color.w * (oldcolorsum + 1e-10f) / (newcolorsum + 1e-10f));

	int scale_idx = (int)floor(scale * 256.0f + 0.5f);

	if (scale_idx < 0)
		scale_idx = 0;
	else if (scale_idx > 255)
		scale_idx = 255;

	output[0] = ri;
	output[1] = gi;
	output[2] = bi;
	output[3] = ClosestQuant(qmode, scale_idx);
}

void quantize_rgbs_alpha_new(float4 color0, float4 color1, float4 rgbs_color, uint8 output[6], Quant qmode) {
	color0.w = color0.w * (1.0f / 257.0f);
	color1.w = color1.w * (1.0f / 257.0f);

	float a0 = clamp255(color0.w);
	float a1 = clamp255(color1.w);

	int ai0 = ClosestQuant(qmode, (int)floor(a0 + 0.5f));
	int ai1 = ClosestQuant(qmode, (int)floor(a1 + 0.5f));

	output[4] = ai0;
	output[5] = ai1;

	quantize_rgbs_new(rgbs_color, output, qmode);
}

void quantize_luminance(float4 color0, float4 color1, uint8 output[2], Quant qmode) {
	color0.xyz = color0.xyz * (1.0f / 257.0f);
	color1.xyz = color1.xyz * (1.0f / 257.0f);

	float lum0 = clamp255((color0.x + color0.y + color0.z) * (1.0f / 3.0f));
	float lum1 = clamp255((color1.x + color1.y + color1.z) * (1.0f / 3.0f));

	if (lum0 > lum1) {
		float avg = (lum0 + lum1) * 0.5f;
		lum0 = avg;
		lum1 = avg;
	}

	output[0] = ClosestQuant(qmode, (int)floor(lum0 + 0.5f));
	output[1] = ClosestQuant(qmode, (int)floor(lum1 + 0.5f));
}

void quantize_luminance_alpha(float4 color0, float4 color1, uint8 output[4], Quant qmode) {
	color0 = color0 / 257.0f;
	color1 = color1 / 257.0f;

	float	lum0	= clamp255((color0.x + color0.y + color0.z) / 3.f);
	float	lum1	= clamp255((color1.x + color1.y + color1.z) / 3.f);
	float	a0		= clamp255(color0.w);
	float	a1		= clamp255(color1.w);

	// if the endpoints are *really* close, then pull them apart slightly;
	// tisa affords for >8 bits precision for normal maps.
	if (qmode > 18 && abs(lum0 - lum1) < 3.0f) {
		if (lum0 < lum1) {
			lum0 -= 0.5f;
			lum1 += 0.5f;
		} else {
			lum0 += 0.5f;
			lum1 -= 0.5f;
		}
		lum0 = clamp255(lum0);
		lum1 = clamp255(lum1);
	}
	if (qmode > 18 && abs(a0 - a1) < 3.0f) {
		if (a0 < a1) {
			a0 -= 0.5f;
			a1 += 0.5f;
		} else {
			a0 += 0.5f;
			a1 -= 0.5f;
		}
		a0 = clamp255(a0);
		a1 = clamp255(a1);
	}

	output[0] = ClosestQuant(qmode, (int)floor(lum0 + 0.5f));
	output[1] = ClosestQuant(qmode, (int)floor(lum1 + 0.5f));
	output[2] = ClosestQuant(qmode, (int)floor(a0 + 0.5f));
	output[3] = ClosestQuant(qmode, (int)floor(a1 + 0.5f));
}


void quantize0(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	for (int i = 0; i < 8; i++)
		output[i] = 0;
}

// quantize and unquantize a number, wile making sure to retain the top two bits.
static inline void quantize_and_unquantize_retain_top_two_bits(Quant qmode, int value_to_quantize, int *quantized_value, int *unquantized_value) {
	bool perform_loop;
	int quantval;
	int uquantval;

	do {
		quantval	= ClosestQuant(qmode, value_to_quantize);
		uquantval	= UnQuantCol(qmode, quantval);

		// perform looping if the top two bits were modified by quant/unquant
		perform_loop = (value_to_quantize & 0xC0) != (uquantval & 0xC0);

		if ((uquantval & 0xC0) > (value_to_quantize & 0xC0)) {
			// quant/unquant rounded UP so that the top two bits changed;
			// decrement the input value in hopes that this will avoid rounding up
			value_to_quantize--;
		} else if ((uquantval & 0xC0) < (value_to_quantize & 0xC0)) {
			// quant/unquant rounded DOWN so that the top two bits changed;
			// decrement the input value in hopes that this will avoid rounding down
			value_to_quantize--;
		}
	} while (perform_loop);

	*quantized_value = quantval;
	*unquantized_value = uquantval;
}


// quantize and unquantize a number, wile making sure to retain the top four bits.
static inline void quantize_and_unquantize_retain_top_four_bits(Quant qmode, int value_to_quantize, int *quantized_value, int *unquantized_value) {
	bool perform_loop;
	int quantval;
	int uquantval;

	do {
		quantval = ClosestQuant(qmode, value_to_quantize);
		uquantval = UnQuantCol(qmode, quantval);

		// perform looping if the top two bits were modified by quant/unquant
		perform_loop = (value_to_quantize & 0xF0) != (uquantval & 0xF0);

		if ((uquantval & 0xF0) > (value_to_quantize & 0xF0)) {
			// quant/unquant rounded UP so that the top two bits changed;
			// decrement the input value in hopes that this will avoid rounding up
			value_to_quantize--;
		} else if ((uquantval & 0xF0) < (value_to_quantize & 0xF0)) {
			// quant/unquant rounded DOWN so that the top two bits changed;
			// decrement the input value in hopes that this will avoid rounding down
			value_to_quantize--;
		}
	} while (perform_loop);

	*quantized_value = quantval;
	*unquantized_value = uquantval;
}

// quantize and unquantize a number, wile making sure to retain the top two bits
static inline void quantize_and_unquantize_retain_top_bit(Quant qmode, int value_to_quantize, int *quantized_value, int *unquantized_value) {
	bool perform_loop;
	int quantval;
	int uquantval;

	do {
		quantval	= ClosestQuant(qmode, value_to_quantize);
		uquantval	= UnQuantCol(qmode, quantval);

		// perform looping if the top two bits were modified by quant/unquant
		perform_loop = (value_to_quantize & 0x80) != (uquantval & 0x80);

		if ((uquantval & 0x80) > (value_to_quantize & 0x80)) {
			// quant/unquant rounded UP so that the top two bits changed;
			// decrement the input value in hopes that this will avoid rounding up
			value_to_quantize--;
		} else if ((uquantval & 0x80) < (value_to_quantize & 0x80)) {
			// quant/unquant rounded DOWN so that the top two bits changed;
			// decrement the input value in hopes that this will avoid rounding down
			value_to_quantize--;
		}
	} while (perform_loop);

	*quantized_value = quantval;
	*unquantized_value = uquantval;
}

void quantize_hdr_rgbo3(float4 color, uint8 output[4], Quant qmode) {
	color.xyz = color.xyz + color.www;

	if (!(color.x > 0.0f))
		color.x = 0.0f;
	else if (color.x > 65535.0f)
		color.x = 65535.0f;

	if (!(color.y > 0.0f))
		color.y = 0.0f;
	else if (color.y > 65535.0f)
		color.y = 65535.0f;

	if (!(color.z > 0.0f))
		color.z = 0.0f;
	else if (color.z > 65535.0f)
		color.z = 65535.0f;

	if (!(color.w > 0.0f))
		color.w = 0.0f;
	else if (color.w > 65535.0f)
		color.w = 65535.0f;

	float4 color_bak = color;
	int majcomp;
	if (color.x > color.y && color.x > color.z)
		majcomp = 0;			// red is largest component
	else if (color.y > color.z)
		majcomp = 1;			// green is largest component
	else
		majcomp = 2;			// blue is largest component

	// swap around the red component and the largest component.
	switch (majcomp) {
		case 1:
			color = color.yxzw;
			break;
		case 2:
			color = color.zyxw;
			break;
		default:
			break;
	}

	static const int mode_bits[5][3] ={
		{ 11, 5, 7 },
		{ 11, 6, 5 },
		{ 10, 5, 8 },
		{ 9, 6, 7 },
		{ 8, 7, 6 }
	};


	static const float mode_cutoffs[5][2] ={
		{ 1024, 4096 },
		{ 2048, 1024 },
		{ 2048, 16384 },
		{ 8192, 16384 },
		{ 32768, 16384 }
	};

	static const float mode_rscales[5] ={
		32.0f,
		32.0f,
		64.0f,
		128.0f,
		256.0f,
	};

	static const float mode_scales[5] ={
		1.0f / 32.0f,
		1.0f / 32.0f,
		1.0f / 64.0f,
		1.0f / 128.0f,
		1.0f / 256.0f,
	};

	float r_base = color.x;
	float g_base = color.x - color.y;
	float b_base = color.x - color.z;
	float s_base = color.w;

	int mode;
	for (mode = 0; mode < 5; mode++) {
		if (g_base > mode_cutoffs[mode][0] || b_base > mode_cutoffs[mode][0] || s_base > mode_cutoffs[mode][1])
			continue;

		// encode the mode into a 4-bit vector.
		int mode_enc = mode < 4 ? (mode | (majcomp << 2)) : (majcomp | 0xC);

		float mode_scale = mode_scales[mode];
		float mode_rscale = mode_rscales[mode];

		int gb_intcutoff = 1 << mode_bits[mode][1];
		int s_intcutoff = 1 << mode_bits[mode][2];

		// first, quantize and unquantize R.
		int r_intval = (int)floor(r_base * mode_scale + 0.5f);

		int r_lowbits = r_intval & 0x3f;

		r_lowbits |= (mode_enc & 3) << 6;

		int r_quantval;
		int r_uquantval;
		quantize_and_unquantize_retain_top_two_bits(qmode, r_lowbits, &r_quantval, &r_uquantval);

		r_intval = (r_intval & ~0x3f) | (r_uquantval & 0x3f);
		float r_fval = r_intval * mode_rscale;


		// next, recompute G and B, then quantize and unquantize them.
		float g_fval = r_fval - color.y;
		float b_fval = r_fval - color.z;
		if (g_fval < 0.0f)
			g_fval = 0.0f;
		else if (g_fval > 65535.0f)
			g_fval = 65535.0f;
		if (b_fval < 0.0f)
			b_fval = 0.0f;
		else if (b_fval > 65535.0f)
			b_fval = 65535.0f;

		int g_intval = (int)floor(g_fval * mode_scale + 0.5f);
		int b_intval = (int)floor(b_fval * mode_scale + 0.5f);

		if (g_intval >= gb_intcutoff || b_intval >= gb_intcutoff)
			continue;

		int g_lowbits = g_intval & 0x1f;
		int b_lowbits = b_intval & 0x1f;

		int bit0 = 0;
		int bit1 = 0;
		int bit2 = 0;
		int bit3 = 0;

		switch (mode) {
			case 0:
			case 2:
				bit0 = (r_intval >> 9) & 1;
				break;
			case 1:
			case 3:
				bit0 = (r_intval >> 8) & 1;
				break;
			case 4:
			case 5:
				bit0 = (g_intval >> 6) & 1;
				break;
		}

		switch (mode) {
			case 0:
			case 1:
			case 2:
			case 3:
				bit2 = (r_intval >> 7) & 1;
				break;
			case 4:
			case 5:
				bit2 = (b_intval >> 6) & 1;
				break;
		}

		switch (mode) {
			case 0:
			case 2:
				bit1 = (r_intval >> 8) & 1;
				break;
			case 1:
			case 3:
			case 4:
			case 5:
				bit1 = (g_intval >> 5) & 1;
				break;
		}

		switch (mode) {
			case 0:
				bit3 = (r_intval >> 10) & 1;
				break;
			case 2:
				bit3 = (r_intval >> 6) & 1;
				break;
			case 1:
			case 3:
			case 4:
			case 5:
				bit3 = (b_intval >> 5) & 1;
				break;
		}

		g_lowbits |= (mode_enc & 0x4) << 5;
		b_lowbits |= (mode_enc & 0x8) << 4;

		g_lowbits |= bit0 << 6;
		g_lowbits |= bit1 << 5;
		b_lowbits |= bit2 << 6;
		b_lowbits |= bit3 << 5;

		int g_quantval;
		int b_quantval;
		int g_uquantval;
		int b_uquantval;

		quantize_and_unquantize_retain_top_four_bits(qmode, g_lowbits, &g_quantval, &g_uquantval);
		quantize_and_unquantize_retain_top_four_bits(qmode, b_lowbits, &b_quantval, &b_uquantval);

		g_intval = (g_intval & ~0x1f) | (g_uquantval & 0x1f);
		b_intval = (b_intval & ~0x1f) | (b_uquantval & 0x1f);

		g_fval = g_intval * mode_rscale;
		b_fval = b_intval * mode_rscale;


		// finally, recompute the scale value, based on the errors introduced to red, green and blue
		// If the error is positive, then the R,G,B errors combined have raised the color value overall; as such, the scale value needs to be increased
		float rgb_errorsum = (r_fval - color.x) + (r_fval - g_fval - color.y) + (r_fval - b_fval - color.z);

		float s_fval = s_base + rgb_errorsum * (1.0f / 3.0f);
		if (s_fval < 0.0f)
			s_fval = 0.0f;
		else if (s_fval > 1e9)
			s_fval = 1e9;

		int s_intval = (int)floor(s_fval * mode_scale + 0.5f);

		if (s_intval >= s_intcutoff)
			continue;

		int s_lowbits = s_intval & 0x1f;

		int bit4;
		int bit5;
		int bit6;
		switch (mode) {
			case 1:
				bit6 = (r_intval >> 9) & 1;
				break;
			default:
				bit6 = (s_intval >> 5) & 1;
				break;
		}

		switch (mode) {
			case 4:
				bit5 = (r_intval >> 7) & 1;
				break;
			case 1:
				bit5 = (r_intval >> 10) & 1;
				break;
			default:
				bit5 = (s_intval >> 6) & 1;
				break;
		}

		switch (mode) {
			case 2:
				bit4 = (s_intval >> 7) & 1;
				break;
			default:
				bit4 = (r_intval >> 6) & 1;
				break;
		}


		s_lowbits |= bit6 << 5;
		s_lowbits |= bit5 << 6;
		s_lowbits |= bit4 << 7;

		int s_quantval;
		int s_uquantval;

		quantize_and_unquantize_retain_top_four_bits(qmode, s_lowbits, &s_quantval, &s_uquantval);

		s_intval = (s_intval & ~0x1f) | (s_uquantval & 0x1f);
		s_fval = s_intval * mode_rscale;
		output[0] = r_quantval;
		output[1] = g_quantval;
		output[2] = b_quantval;
		output[3] = s_quantval;
		return;
	}

	// failed to encode any of the modes above? In that case, encode using mode #5.
	float vals[4];
	int ivals[4];
	vals[0] = color_bak.x;
	vals[1] = color_bak.y;
	vals[2] = color_bak.z;
	vals[3] = color_bak.w;

	float cvals[3];

	for (int i = 0; i < 3; i++) {
		if (vals[i] < 0.0f)
			vals[i] = 0.0f;
		else if (vals[i] > 65020.0f)
			vals[i] = 65020.0f;

		ivals[i] = (int)floor(vals[i] * (1.0f / 512.0f) + 0.5f);
		cvals[i] = ivals[i] * 512.0f;
	}

	float rgb_errorsum = (cvals[0] - vals[0]) + (cvals[1] - vals[1]) + (cvals[2] - vals[2]);
	vals[3] += rgb_errorsum * (1.0f / 3.0f);

	if (vals[3] < 0.0f)
		vals[3] = 0.0f;
	else if (vals[3] > 65020.0f)
		vals[3] = 65020.0f;

	ivals[3] = (int)floor(vals[3] * (1.0f / 512.0f) + 0.5f);

	int encvals[4];

	encvals[0] = (ivals[0] & 0x3f) | 0xC0;
	encvals[1] = (ivals[1] & 0x7f) | 0x80;
	encvals[2] = (ivals[2] & 0x7f) | 0x80;
	encvals[3] = (ivals[3] & 0x7f) | ((ivals[0] & 0x40) << 1);

	for (int i = 0; i < 4; i++) {
		int dummy, val = output[i];
		quantize_and_unquantize_retain_top_four_bits(qmode, encvals[i], &val, &dummy);
		output[i] = val;
	}
}

void quantize_hdr_rgb3(float4 color0, float4 color1, uint8 output[6], Quant qmode) {
	color0.xyz = clamp(color0.xyz, 0.f, 65535.f);
	color1.xyz = clamp(color1.xyz, 0.f, 65535.f);

	float4 color0_bak = color0;
	float4 color1_bak = color1;

	int majcomp = max_component_index(color1.xyz);

	// swizzle the components
	switch (majcomp) {
		case 1:					// red-green swap
			color0 = color0.yxzw;
			color1 = color1.yxzw;
			break;
		case 2:					// red-blue swap
			color0 = color0.zyxw;
			color1 = color1.zyxw;
			break;
		default:
			break;
	}

	float a_base = color1.x;

	float b0_base = a_base - color1.y;
	float b1_base = a_base - color1.z;
	float c_base = a_base - color0.x;
	float d0_base = a_base - b0_base - c_base - color0.y;
	float d1_base = a_base - b1_base - c_base - color0.z;

	// number of bits in the various fields in the various modes
	static const int mode_bits[8][4] ={
		{ 9, 7, 6, 7 },
		{ 9, 8, 6, 6 },
		{ 10, 6, 7, 7 },
		{ 10, 7, 7, 6 },
		{ 11, 8, 6, 5 },
		{ 11, 6, 8, 6 },
		{ 12, 7, 7, 5 },
		{ 12, 6, 7, 6 }
	};

	// cutoffs to use for the computed values of a,b,c,d, assuming the range 0..65535 are LNS values corresponding to fp16
	static const float mode_cutoffs[8][4] ={
		{ 16384, 8192, 8192, 8 },	// mode 0: 9,7,6,7
		{ 32768, 8192, 4096, 8 },	// mode 1: 9,8,6,6
		{ 4096, 8192, 4096, 4 },	// mode 2: 10,6,7,7
		{ 8192, 8192, 2048, 4 },	// mode 3: 10,7,7,6
		{ 8192, 2048, 512, 2 },		// mode 4: 11,8,6,5
		{ 2048, 8192, 1024, 2 },	// mode 5: 11,6,8,6
		{ 2048, 2048, 256, 1 },		// mode 6: 12,7,7,5
		{ 1024, 2048, 512, 1 },		// mode 7: 12,6,7,6
	};

	static const float mode_scales[8] ={
		1.0f / 128.0f,
		1.0f / 128.0f,
		1.0f / 64.0f,
		1.0f / 64.0f,
		1.0f / 32.0f,
		1.0f / 32.0f,
		1.0f / 16.0f,
		1.0f / 16.0f,
	};

	// scaling factors when going from what was encoded in the mode to 16 bits
	static const float mode_rscales[8] ={
		128.0f,
		128.0f,
		64.0f,
		64.0f,
		32.0f,
		32.0f,
		16.0f,
		16.0f
	};


	// try modes one by one, with the highest-precision mode first
	for (int mode = 7; mode >= 0; mode--) {
		// for each mode, test if we can in fact accommodate the computed b,c,d values. If we clearly can't, then we skip to the next mode

		float b_cutoff = mode_cutoffs[mode][0];
		float c_cutoff = mode_cutoffs[mode][1];
		float d_cutoff = mode_cutoffs[mode][2];

		if (b0_base > b_cutoff || b1_base > b_cutoff || c_base > c_cutoff || abs(d0_base) > d_cutoff || abs(d1_base) > d_cutoff)
			continue;

		float mode_scale = mode_scales[mode];
		float mode_rscale = mode_rscales[mode];

		int b_intcutoff = 1 << mode_bits[mode][1];
		int c_intcutoff = 1 << mode_bits[mode][2];
		int d_intcutoff = 1 << (mode_bits[mode][3] - 1);

		// first, quantize and unquantize A, with the assumption that its high bits can be handled safely.
		int a_intval = (int)floor(a_base * mode_scale + 0.5f);
		int a_lowbits = a_intval & 0xFF;

		int a_quantval = ClosestQuant(qmode, a_lowbits);
		int a_uquantval = UnQuantCol(qmode, a_quantval);
		a_intval = (a_intval & ~0xFF) | a_uquantval;
		float a_fval = a_intval * mode_rscale;

		// next, recompute C, then quantize and unquantize it
		float c_fval = a_fval - color0.x;

		int c_intval = (int)floor(c_fval * mode_scale + 0.5f);

		if (c_intval >= c_intcutoff) {
			continue;
		}

		int c_lowbits = c_intval & 0x3f;

		c_lowbits |= (mode & 1) << 7;
		c_lowbits |= (a_intval & 0x100) >> 2;

		int c_quantval;
		int c_uquantval;
		quantize_and_unquantize_retain_top_two_bits(qmode, c_lowbits, &c_quantval, &c_uquantval);
		c_intval = (c_intval & ~0x3F) | (c_uquantval & 0x3F);
		c_fval = c_intval * mode_rscale;


		// next, recompute B0 and B1, then quantize and unquantize them
		float b0_fval = a_fval - color1.y;
		float b1_fval = a_fval - color1.z;

		int b0_intval = (int)floor(b0_fval * mode_scale + 0.5f);
		int b1_intval = (int)floor(b1_fval * mode_scale + 0.5f);

		if (b0_intval >= b_intcutoff || b1_intval >= b_intcutoff)
			continue;

		int b0_lowbits = b0_intval & 0x3f;
		int b1_lowbits = b1_intval & 0x3f;

		int bit0 = 0;
		int bit1 = 0;
		switch (mode) {
			case 0:
			case 1:
			case 3:
			case 4:
			case 6:
				bit0 = (b0_intval >> 6) & 1;
				break;
			case 2:
			case 5:
			case 7:
				bit0 = (a_intval >> 9) & 1;
				break;
		}

		switch (mode) {
			case 0:
			case 1:
			case 3:
			case 4:
			case 6:
				bit1 = (b1_intval >> 6) & 1;
				break;
			case 2:
				bit1 = (c_intval >> 6) & 1;
				break;
			case 5:
			case 7:
				bit1 = (a_intval >> 10) & 1;
				break;
		}

		b0_lowbits |= bit0 << 6;
		b1_lowbits |= bit1 << 6;

		b0_lowbits |= ((mode >> 1) & 1) << 7;
		b1_lowbits |= ((mode >> 2) & 1) << 7;

		int b0_quantval;
		int b1_quantval;
		int b0_uquantval;
		int b1_uquantval;

		quantize_and_unquantize_retain_top_two_bits(qmode, b0_lowbits, &b0_quantval, &b0_uquantval);

		quantize_and_unquantize_retain_top_two_bits(qmode, b1_lowbits, &b1_quantval, &b1_uquantval);

		b0_intval = (b0_intval & ~0x3f) | (b0_uquantval & 0x3f);
		b1_intval = (b1_intval & ~0x3f) | (b1_uquantval & 0x3f);
		b0_fval = b0_intval * mode_rscale;
		b1_fval = b1_intval * mode_rscale;


		// finally, recompute D0 and D1, then quantize and unquantize them
		float d0_fval = a_fval - b0_fval - c_fval - color0.y;
		float d1_fval = a_fval - b1_fval - c_fval - color0.z;

		if (d0_fval < -65535.0f)
			d0_fval = -65535.0f;
		else if (d0_fval > 65535.0f)
			d0_fval = 65535.0f;

		if (d1_fval < -65535.0f)
			d1_fval = -65535.0f;
		else if (d1_fval > 65535.0f)
			d1_fval = 65535.0f;

		int d0_intval = (int)floor(d0_fval * mode_scale + 0.5f);
		int d1_intval = (int)floor(d1_fval * mode_scale + 0.5f);

		if (abs(d0_intval) >= d_intcutoff || abs(d1_intval) >= d_intcutoff)
			continue;

		// d0_intval += mode_dbiases[mode];
		// d1_intval += mode_dbiases[mode];

		int d0_lowbits = d0_intval & 0x1f;
		int d1_lowbits = d1_intval & 0x1f;

		int bit2 = 0;
		int bit3 = 0;
		int bit4;
		int bit5;
		switch (mode) {
			case 0:
			case 2:
				bit2 = (d0_intval >> 6) & 1;
				break;
			case 1:
			case 4:
				bit2 = (b0_intval >> 7) & 1;
				break;
			case 3:
				bit2 = (a_intval >> 9) & 1;
				break;
			case 5:
				bit2 = (c_intval >> 7) & 1;
				break;
			case 6:
			case 7:
				bit2 = (a_intval >> 11) & 1;
				break;
		}
		switch (mode) {
			case 0:
			case 2:
				bit3 = (d1_intval >> 6) & 1;
				break;
			case 1:
			case 4:
				bit3 = (b1_intval >> 7) & 1;
				break;
			case 3:
			case 5:
			case 6:
			case 7:
				bit3 = (c_intval >> 6) & 1;
				break;
		}

		switch (mode) {
			case 4:
			case 6:
				bit4 = (a_intval >> 9) & 1;
				bit5 = (a_intval >> 10) & 1;
				break;
			default:
				bit4 = (d0_intval >> 5) & 1;
				bit5 = (d1_intval >> 5) & 1;
				break;
		}

		d0_lowbits |= bit2 << 6;
		d1_lowbits |= bit3 << 6;
		d0_lowbits |= bit4 << 5;
		d1_lowbits |= bit5 << 5;

		d0_lowbits |= (majcomp & 1) << 7;
		d1_lowbits |= ((majcomp >> 1) & 1) << 7;

		int d0_quantval;
		int d1_quantval;
		int d0_uquantval;
		int d1_uquantval;

		quantize_and_unquantize_retain_top_four_bits(qmode, d0_lowbits, &d0_quantval, &d0_uquantval);
		quantize_and_unquantize_retain_top_four_bits(qmode, d1_lowbits, &d1_quantval, &d1_uquantval);

		output[0] = a_quantval;
		output[1] = c_quantval;
		output[2] = b0_quantval;
		output[3] = b1_quantval;
		output[4] = d0_quantval;
		output[5] = d1_quantval;
		return;
	}

	// neither of the modes fit? In this case, we will use a flat representation for storing data, using 8 bits for red and green, and 7 bits for blue.
	// This gives color accuracy roughly similar to LDR 4:4:3 which is not at all great but usable.
	// This representation is used if the light color is more than 4x the color value of the dark color.
	float vals[6];
	vals[0] = color0_bak.x;
	vals[1] = color1_bak.x;
	vals[2] = color0_bak.y;
	vals[3] = color1_bak.y;
	vals[4] = color0_bak.z;
	vals[5] = color1_bak.z;


	for (int i = 0; i < 6; i++) {
		if (vals[i] > 65020.0f)
			vals[i] = 65020.0f;
	}
	for (int i = 0; i < 4; i++) {
		int idx = (int)floor(vals[i] * 1.0f / 256.0f + 0.5f);
		output[i] = ClosestQuant(qmode, idx);
	}
	for (int i = 4; i < 6; i++) {
		int dummy, val = output[i];
		int idx = (int)floor(vals[i] * 1.0f / 512.0f + 0.5f) + 128;
		quantize_and_unquantize_retain_top_two_bits(qmode, idx, &val, &dummy);
		output[i] = val;
	}

	return;
}

void quantize_hdr_rgb_ldr_alpha3(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	color0.w *= (1.0f / 257.0f);
	color1.w *= (1.0f / 257.0f);

	quantize_hdr_rgb3(color0, color1, output, qmode);

	float a0 = clamp255(color0.w);
	float a1 = clamp255(color1.w);
	int ai0 = ClosestQuant(qmode, (int)floor(a0 + 0.5f));
	int ai1 = ClosestQuant(qmode, (int)floor(a1 + 0.5f));

	output[6] = ai0;
	output[7] = ai1;
}

void quantize_hdr_luminance_large_range3(float4 color0, float4 color1, uint8 output[2], Quant qmode) {
	float lum1 = (color1.x + color1.y + color1.z) * (1.0f / 3.0f);
	float lum0 = (color0.x + color0.y + color0.z) * (1.0f / 3.0f);

	if (lum1 < lum0) {
		float avg = (lum0 + lum1) * 0.5f;
		lum0 = avg;
		lum1 = avg;
	}

	int ilum1 = static_cast <int>(floor(lum1 + 0.5f));
	int ilum0 = static_cast <int>(floor(lum0 + 0.5f));

	// find the closest encodable point in the upper half of the code-point space
	int upper_v0 = (ilum0 + 128) >> 8;
	int upper_v1 = (ilum1 + 128) >> 8;

	if (upper_v0 < 0)
		upper_v0 = 0;
	else if (upper_v0 > 255)
		upper_v0 = 255;

	if (upper_v1 < 0)
		upper_v1 = 0;
	else if (upper_v1 > 255)
		upper_v1 = 255;

	// find the closest encodable point in the lower half of the code-point space
	int lower_v0 = (ilum1 + 256) >> 8;
	int lower_v1 = ilum0 >> 8;

	if (lower_v0 < 0)
		lower_v0 = 0;
	else if (lower_v0 > 255)
		lower_v0 = 255;

	if (lower_v1 < 0)
		lower_v1 = 0;
	else if (lower_v1 > 255)
		lower_v1 = 255;

	// determine the distance between the point in code-point space and the input value
	int upper0_dec = upper_v0 << 8;
	int upper1_dec = upper_v1 << 8;
	int lower0_dec = (lower_v1 << 8) + 128;
	int lower1_dec = (lower_v0 << 8) - 128;

	int upper0_diff = upper0_dec - ilum0;
	int upper1_diff = upper1_dec - ilum1;
	int lower0_diff = lower0_dec - ilum0;
	int lower1_diff = lower1_dec - ilum1;

	int upper_error = (upper0_diff * upper0_diff) + (upper1_diff * upper1_diff);
	int lower_error = (lower0_diff * lower0_diff) + (lower1_diff * lower1_diff);

	int v0, v1;
	if (upper_error < lower_error) {
		v0 = upper_v0;
		v1 = upper_v1;
	} else {
		v0 = lower_v0;
		v1 = lower_v1;
	}

	output[0] = ClosestQuant(qmode, v0);
	output[1] = ClosestQuant(qmode, v1);
}

bool try_quantize_hdr_luminance_small_range3(float4 color0, float4 color1, uint8 output[2], Quant qmode) {
	float lum1 = (color1.x + color1.y + color1.z) * (1.0f / 3.0f);
	float lum0 = (color0.x + color0.y + color0.z) * (1.0f / 3.0f);

	if (lum1 < lum0) {
		float avg = (lum0 + lum1) * 0.5f;
		lum0 = avg;
		lum1 = avg;
	}

	int ilum1 = static_cast<int>(floor(lum1 + 0.5f));
	int ilum0 = static_cast<int>(floor(lum0 + 0.5f));

	// difference of more than a factor-of-2 results in immediate failure
	if (ilum1 - ilum0 > 2048)
		return 0;

	int lowval, highval, diffval;
	int v0, v1;
	int v0e, v1e;
	int v0d, v1d;

	// first, try to encode the high-precision submode
	lowval = (ilum0 + 16) >> 5;
	highval = (ilum1 + 16) >> 5;

	if (lowval < 0)
		lowval = 0;
	else if (lowval > 2047)
		lowval = 2047;

	if (highval < 0)
		highval = 0;
	else if (highval > 2047)
		highval = 2047;

	v0 = lowval & 0x7F;
	v0e = ClosestQuant(qmode, v0);
	v0d = UnQuantCol(qmode, v0e);
	if ((v0d & 0x80) == 0x80)
		goto LOW_PRECISION_SUBMODE;

	lowval = (lowval & ~0x7F) | (v0d & 0x7F);
	diffval = highval - lowval;
	if (diffval < 0 || diffval > 15)
		goto LOW_PRECISION_SUBMODE;

	v1 = ((lowval >> 3) & 0xF0) | diffval;
	v1e = ClosestQuant(qmode, v1);
	v1d = UnQuantCol(qmode, v1e);
	if ((v1d & 0xF0) != (v1 & 0xF0))
		goto LOW_PRECISION_SUBMODE;

	output[0] = v0e;
	output[1] = v1e;
	return true;


	// failed to encode the high-precision submode; well, then try to encode the
	// low-precision submode.
LOW_PRECISION_SUBMODE:

	lowval = (ilum0 + 32) >> 6;
	highval = (ilum1 + 32) >> 6;
	if (lowval < 0)
		lowval = 0;
	else if (lowval > 1023)
		lowval = 1023;
	if (highval < 0)
		highval = 0;
	else if (highval > 1023)
		highval = 1023;

	v0 = (lowval & 0x7F) | 0x80;
	v0e = ClosestQuant(qmode, v0);
	v0d = UnQuantCol(qmode, v0e);
	if ((v0d & 0x80) == 0)
		return false;

	lowval = (lowval & ~0x7F) | (v0d & 0x7F);
	diffval = highval - lowval;
	if (diffval < 0 || diffval > 31)
		return false;

	v1 = ((lowval >> 2) & 0xE0) | diffval;
	v1e = ClosestQuant(qmode, v1);
	v1d = UnQuantCol(qmode, v1e);
	if ((v1d & 0xE0) != (v1 & 0xE0))
		return false;

	output[0] = v0e;
	output[1] = v1e;
	return true;
}

void quantize_hdr_alpha3(float alpha0, float alpha1, uint8 output[2], Quant qmode) {
	if (alpha0 < 0)
		alpha0 = 0;
	else if (alpha0 > 65280)
		alpha0 = 65280;

	if (alpha1 < 0)
		alpha1 = 0;
	else if (alpha1 > 65280)
		alpha1 = 65280;

	int ialpha0 = static_cast<int>(floor(alpha0 + 0.5f));
	int ialpha1 = static_cast<int>(floor(alpha1 + 0.5f));

	int val0, val1, diffval;
	int v6, v7;
	int v6e, v7e;
	int v6d, v7d;

	// try to encode one of the delta submodes, in decreasing-precision order
	for (int i = 2; i >= 0; i--) {
		val0 = (ialpha0 + (128 >> i)) >> (8 - i);
		val1 = (ialpha1 + (128 >> i)) >> (8 - i);

		v6 = (val0 & 0x7F) | ((i & 1) << 7);
		v6e = ClosestQuant(qmode, v6);
		v6d = UnQuantCol(qmode, v6e);

		if ((v6 ^ v6d) & 0x80)
			continue;

		val0 = (val0 & ~0x7f) | (v6d & 0x7f);
		diffval = val1 - val0;
		int cutoff = 32 >> i;
		int mask = 2 * cutoff - 1;

		if (diffval < -cutoff || diffval >= cutoff)
			continue;

		v7 = ((i & 2) << 6) | ((val0 >> 7) << (6 - i)) | (diffval & mask);
		v7e = ClosestQuant(qmode, v7);
		v7d = UnQuantCol(qmode, v7e);

		static const int testbits[3] ={ 0xE0, 0xF0, 0xF8 };

		if ((v7 ^ v7d) & testbits[i])
			continue;

		output[0] = v6e;
		output[1] = v7e;
		return;
	}

	// could not encode any of the delta modes; instead encode a flat value
	val0 = (ialpha0 + 256) >> 9;
	val1 = (ialpha1 + 256) >> 9;
	v6 = val0 | 0x80;
	v7 = val1 | 0x80;

	v6e = ClosestQuant(qmode, v6);
	v7e = ClosestQuant(qmode, v7);
	output[0] = v6e;
	output[1] = v7e;
}

void quantize_hdr_rgb_alpha3(float4 color0, float4 color1, uint8 output[8], Quant qmode) {
	quantize_hdr_rgb3(color0, color1, output, qmode);
	quantize_hdr_alpha3(color0.w, color1.w, output + 6, qmode);
}

Format pack_color_endpoints(float4 color0, float4 color1, float4 rgbs_color, float4 rgbo_color, Format format, uint8 *output, Quant qmode) {
	// we do not support negative colors.
	color0 = max(color0, zero);
	color1 = max(color1, zero);

	switch (format) {
		case FMT_RGB:
			if (qmode <= 18) {
				if (try_quantize_rgb_delta_blue_contract(color0, color1, output, qmode))
					return FMT_RGB_DELTA;
				if (try_quantize_rgb_delta(color0, color1, output, qmode))
					return FMT_RGB_DELTA;
			}
			if (try_quantize_rgb_blue_contract(color0, color1, output, qmode))
				return FMT_RGB;
			quantize_rgb(color0, color1, output, qmode);
			return FMT_RGB;

		case FMT_RGBA:
			if (qmode <= 18) {
				if (try_quantize_rgba_delta_blue_contract(color0, color1, output, qmode))
					return FMT_RGBA_DELTA;
				if (try_quantize_rgba_delta(color0, color1, output, qmode))
					return FMT_RGBA_DELTA;
			}
			if (try_quantize_rgba_blue_contract(color0, color1, output, qmode))
				return FMT_RGBA;
			quantize_rgba(color0, color1, output, qmode);
			return FMT_RGBA;

		case FMT_RGB_SCALE:
			quantize_rgbs_new(rgbs_color, output, qmode);
			// quantize_rgbs( color0, color1, output, qmode );
			return FMT_RGB_SCALE;

		case FMT_HDR_RGB_SCALE:
			quantize_hdr_rgbo3(rgbo_color, output, qmode);
			// quantize_hdr_rgb_scale( rgbo_color, output, qmode );
			return FMT_HDR_RGB_SCALE;

		case FMT_HDR_RGB:
			quantize_hdr_rgb3(color0, color1, output, qmode);
			// quantize_hdr_rgb_rgba( color0, color1, 0, output, qmode );
			return FMT_HDR_RGB;

		case FMT_RGB_SCALE_ALPHA:
			quantize_rgbs_alpha_new(color0, color1, rgbs_color, output, qmode);
			// quantize_rgbs_alpha( color0, color1, output, qmode );
			return FMT_RGB_SCALE_ALPHA;

		case FMT_HDR_LUMINANCE_SMALL_RANGE:
		case FMT_HDR_LUMINANCE_LARGE_RANGE:
			if (try_quantize_hdr_luminance_small_range3(color0, color1, output, qmode))
				return FMT_HDR_LUMINANCE_SMALL_RANGE;
			quantize_hdr_luminance_large_range3(color0, color1, output, qmode);
			return FMT_HDR_LUMINANCE_LARGE_RANGE;

		case FMT_LUMINANCE:
			quantize_luminance(color0, color1, output, qmode);
			return FMT_LUMINANCE;

		case FMT_LUMINANCE_ALPHA:
			if (qmode <= 18 && try_quantize_luminance_alpha_delta(color0, color1, output, qmode))
				return FMT_LUMINANCE_ALPHA_DELTA;
			quantize_luminance_alpha(color0, color1, output, qmode);
			return FMT_LUMINANCE_ALPHA;

		case FMT_HDR_RGB_LDR_ALPHA:
			quantize_hdr_rgb_ldr_alpha3(color0, color1, output, qmode);
			return FMT_HDR_RGB_LDR_ALPHA;

		case FMT_HDR_RGBA:
			quantize_hdr_rgb_alpha3(color0, color1, output, qmode);
			return FMT_HDR_RGBA;

		default:
			quantize0(color0, color1, output, qmode);
			return FMT_LUMINANCE;
	}
}

//-----------------------------------------------------------------------------
//	PartitionTable
//-----------------------------------------------------------------------------

struct PartitionTable {
	int		xdim, ydim, zdim;
	int		num_partitions;
	uint8	partition[ASTC::MAX_TEXELS_PER_BLOCK];
	uint8	texels[4][ASTC::MAX_TEXELS_PER_BLOCK];
	int		counts[4];

	PartitionTable(int _xdim, int _ydim, int _zdim, int _num_partitions, int index);
};

int calc_partition(int seed, int x, int y, int z, int partitioncount, bool small_block) {
	if (small_block) {
		x <<= 1;
		y <<= 1;
		z <<= 1;
	}

	seed += (partitioncount - 1) * 1024;

	uint32 rnum	= (seed ^ (seed >> 15)) * 0xEEDE0891;	// (2^4+1)*(2^7+1)*(2^17-1)
	rnum ^= rnum >> 5;
	rnum += rnum << 16;
	rnum ^= rnum >> 7;
	rnum ^= rnum >> 3;
	rnum ^= rnum << 6;
	rnum ^= rnum >> 17;

	// squaring all the seeds in order to bias their distribution towards lower values.
	uint8	seed1	= square(rnum & 0xF);
	uint8	seed2	= square((rnum >>  4) & 0xF);
	uint8	seed3	= square((rnum >>  8) & 0xF);
	uint8	seed4	= square((rnum >> 12) & 0xF);
	uint8	seed5	= square((rnum >> 16) & 0xF);
	uint8	seed6	= square((rnum >> 20) & 0xF);
	uint8	seed7	= square((rnum >> 24) & 0xF);
	uint8	seed8	= square((rnum >> 28) & 0xF);
	uint8	seed9	= square((rnum >> 18) & 0xF);
	uint8	seed10	= square((rnum >> 22) & 0xF);
	uint8	seed11	= square((rnum >> 26) & 0xF);
	uint8	seed12	= square(((rnum >> 30) | (rnum << 2)) & 0xF);

	int		sx		= seed & 1		? (seed & 2 ? 4 : 5) : partitioncount == 3 ? 6 : 5;
	int		sy		= !(seed & 1)	? (seed & 2 ? 4 : 5) : partitioncount == 3 ? 6 : 5;
	int		sz		= seed & 0x10	? sx : sy;

	int		a		= partitioncount > 0 ? ((seed1 >> sx) * x + (seed2 >> sy) * y + (seed11 >> sz) * z + (rnum >> 14)) & 0x3f : 0;
	int		b		= partitioncount > 1 ? ((seed3 >> sx) * x + (seed4 >> sy) * y + (seed12 >> sz) * z + (rnum >> 10)) & 0x3f : 0;
	int		c		= partitioncount > 2 ? ((seed5 >> sx) * x + (seed6 >> sy) * y + (seed9  >> sz) * z + (rnum >> 6) ) & 0x3f : 0;
	int		d		= partitioncount > 3 ? ((seed7 >> sx) * x + (seed8 >> sy) * y + (seed10 >> sz) * z + (rnum >> 2) ) & 0x3f : 0;

	return	a >= b && a >= c && a >= d ? 0
		:	b >= c && b >= d ? 1
		:	c >= d ? 2
		:	3;
}

PartitionTable::PartitionTable(int _xdim, int _ydim, int _zdim, int _num_partitions, int index) : xdim(_xdim), ydim(_ydim), zdim(_zdim), num_partitions(_num_partitions) {
	clear(counts);

	uint8	*p		= partition;
	bool	small	= xdim * ydim * zdim < 32;
	for (int z = 0; z < zdim; z++) {
		for (int y = 0; y < ydim; y++) {
			for (int x = 0; x < xdim; x++)
				*p++ = calc_partition(index, x, y, z, num_partitions, small);
		}
	}

	for (int i = 0, n = xdim * ydim * zdim; i < n; i++) {
		int j = partition[i];
		texels[j][counts[j]++] = i;
	}

	for (int i = 0; i < 4; i++) {
		if (counts[i] == 0) {
			num_partitions = i;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//	BlockMode
//-----------------------------------------------------------------------------

struct BlockMode {
	int		X, Y, Z, dual;
	Quant	qmode;
	bool	decode(int mode, bool z);
};

bool BlockMode::decode(int mode, bool z) {
	X = Y = Z = 1;
	dual = (mode >> 10) & 1;

	if ((mode & 15) == 0)
		return false;

	int base_quant_mode = (mode >> 4) & 1;
	int H = (mode >> 9) & 1;
	int A = (mode >> 5) & 3;
	int B = (mode >> 7) & 3;
	int M = (mode >> 2) & 3;

	if (mode & 3) {
		base_quant_mode |= (mode & 3) << 1;
		if (z) {
			X = A + 2;
			Y = B + 2;
			Z = M + 2;
		} else switch (M) {
			case 0:	X = B + 4;	Y = A + 2;	break;
			case 1:	X = B + 8;	Y = A + 2;	break;
			case 2:	X = A + 2;	Y = B + 8;	break;
			default:
				B &= 1;
				if (mode & 0x100) {
					X = B + 2;
					Y = A + 2;
				} else {
					X = A + 2;
					Y = B + 6;
				}
				break;
		}
	} else {
		base_quant_mode |= M << 1;

		int C = (mode >> 9) & 3;
		if (z) {
			switch (B) {
				case 0:	X = 6;		Y = C + 2;	Z = A + 2;	dual = H = 0;	break;
				case 1:	X = A + 2;	Y = 6;		Z = C + 2;	dual = H = 0;	break;
				case 2:	X = A + 2;	Y = C + 2;	Z = 6;		dual = H = 0;	break;
				default:
					switch (A) {
						case 0:	X = 6;	Y = 2;	Z = 2;	break;
						case 1:	X = 2;	Y = 6;	Z = 2;	break;
						case 2:	X = 2;	Y = 2;	Z = 6;	break;
						default: return false;
					}
					break;
			}
		} else {
			switch (B) {
				case 0:	X = 12;		Y = A + 2;	break;
				case 1:	X = A + 2;	Y = 12;		break;
				case 2:	X = A + 6;	Y = C + 6;	dual = H = 0;	break;
				default:
					switch (A) {
						case 0:	X = 6;	Y = 10;	break;
						case 1:	X = 10;	Y = 6;	break;
						default:	return false;
					}
					break;
			}
		}
	}

	qmode	= Quant((base_quant_mode - 2) + 6 * H);

	int weight_count	= (X * Y * Z) << dual;
	return weight_count <= ASTC::MAX_WEIGHTS_PER_BLOCK && between(ISE_bits(qmode, weight_count), ASTC::MIN_WEIGHT_BITS_PER_BLOCK, ASTC::MAX_WEIGHT_BITS_PER_BLOCK);
}

//-----------------------------------------------------------------------------
//	DecimationTable
//-----------------------------------------------------------------------------

struct DecimationTable {
	int		num_texels;
	int		num_weights;

	struct texel {
		uint8	num_weights;					// number of indices that go into the calculation for a texel
		uint8	w [4];							// the weights that go into a texel calculation
		uint8	wi[4];							// the weight to assign to each weight
		float	wf[4];							// the weight to assign to each weight

		int compute(const int *weights) const {
			int summed_value = 8;
			for (int i = 0; i < num_weights; i++)
				summed_value += weights[w[i]] * wi[i];
			return summed_value >> 4;
		}
		float compute(const float *weights) const {
			return weights[w[0]] * wf[0] + weights[w[1]] * wf[1] + weights[w[2]] * wf[2] + weights[w[3]] * wf[3];
		}

	} texels[ASTC::MAX_TEXELS_PER_BLOCK];

	struct weighting {
		uint8	num_texels;							// the number of texels that a given weight contributes to
		uint8	texel[ASTC::MAX_TEXELS_PER_BLOCK];	// the texels that the weight contributes to
		uint8	wi[ASTC::MAX_TEXELS_PER_BLOCK];		// the weights that the weight contributes to a texel.
		float	wf[ASTC::MAX_TEXELS_PER_BLOCK];		// the weights that the weight contributes to a texel.
	} weights[ASTC::MAX_WEIGHTS_PER_BLOCK];

	DecimationTable(int xdim, int ydim, int zdim, int X, int Y, int Z);
};


void calc_weights(int w[8], int X, int Y, int xw, int yw) {
	int		xf	= xw & 0xF;
	int		yf	= yw & 0xF;

	int		xi	= xw >> 4;
	int		yi	= yw >> 4;

	// truncated-precision bilinear interpolation.
	int		w3	= (xf * yf + 8) >> 4;
	w[0] = 16 - xf - yf + w3;
	w[1] = xf - w3;
	w[2] = yf - w3;
	w[3] = w3;

	int		s0	= xi + yi * X;
	w[4] = s0;
	w[5] = s0 + 1;
	w[6] = s0 + X;
	w[7] = s0 + X + 1;
}

void calc_weights(int w[8], int X, int Y, int Z, int xw, int yw, int zw) {
	int		xf	= xw & 0xF;
	int		yf	= yw & 0xF;
	int		zf	= zw & 0xF;

	int		xi	= xw >> 4;
	int		yi	= yw >> 4;
	int		zi	= zw >> 4;

	// simplex interpolation
	int		s0	= (zi * Y + yi) * X + xi, s1, s2;
	switch (((xf > yf) << 2) + ((yf > zf) << 1) + ((xf > zf))) {
		case 7:		s1 = 1;		s2 = X;		w[0] = 16 - xf;	w[1] = xf - yf;	w[2] = yf - zf;	w[3] = zf;	break;
		case 3:		s1 = X;		s2 = 1;		w[0] = 16 - yf;	w[1] = yf - xf;	w[2] = xf - zf;	w[3] = zf;	break;
		case 5:		s1 = 1;		s2 = X*Y;	w[0] = 16 - xf;	w[1] = xf - zf;	w[2] = zf - yf;	w[3] = yf;	break;
		case 4:		s1 = X*Y;	s2 = 1;		w[0] = 16 - zf;	w[1] = zf - xf;	w[2] = xf - yf;	w[3] = yf;	break;
		case 2:		s1 = X;		s2 = X*Y;	w[0] = 16 - yf;	w[1] = yf - zf;	w[2] = zf - xf;	w[3] = xf;	break;
		case 0:		s1 = X*Y;	s2 = X;		w[0] = 16 - zf;	w[1] = zf - yf;	w[2] = yf - xf;	w[3] = xf;	break;
		default:	s1 = X*Y;	s2 = X;		w[0] = 16 - zf;	w[1] = zf - yf;	w[2] = yf - xf;	w[3] = xf;	break;
	}

	w[4] = s0;
	w[5] = s0 + s1;
	w[6] = s0 + s1 + s2;
	w[7] = ((zi + 1) * Y + (yi + 1)) * X + (xi + 1);
}

struct collect_weights {
	int weightcount_of_texel[ASTC::MAX_TEXELS_PER_BLOCK];
	int grid_weights_of_texel[ASTC::MAX_TEXELS_PER_BLOCK][4];
	int weights_of_texel[ASTC::MAX_TEXELS_PER_BLOCK][4];

	int texelcount_of_weight[ASTC::MAX_WEIGHTS_PER_BLOCK];
	int texels_of_weight[ASTC::MAX_WEIGHTS_PER_BLOCK][ASTC::MAX_TEXELS_PER_BLOCK];
	int texelweights_of_weight[ASTC::MAX_WEIGHTS_PER_BLOCK][ASTC::MAX_TEXELS_PER_BLOCK];

	collect_weights() {
		clear(texelcount_of_weight);
		clear(weightcount_of_texel);
	}

	void	add(int i, int weight[8]) {
		for (int j = 0; j < 4; j++) {
			if (int w = weight[j]) {
				int		qw	= weight[j + 4];
				int		&wc = weightcount_of_texel[i];
				int		&tc = texelcount_of_weight[qw];
				grid_weights_of_texel[i][wc]	= qw;
				weights_of_texel[i][wc]			= w;
				wc++;
				texels_of_weight[qw][tc]		= i;
				texelweights_of_weight[qw][tc]	= w;
				tc++;
			}
		}
	}

	void	put(int i, DecimationTable::texel &t) {
		clear(t);
		t.num_weights = weightcount_of_texel[i];

		for (int j = 0; j < weightcount_of_texel[i]; j++) {
			t.wi[j]	= weights_of_texel[i][j];
			t.wf[j]	= weights_of_texel[i][j] / 16.f;
			t.w[j]	= grid_weights_of_texel[i][j];
		}
	}
	void	put(int i, DecimationTable::weighting &t) {
		t.num_texels	= texelcount_of_weight[i];
		for (int j = 0; j < texelcount_of_weight[i]; j++) {
			t.texel[j]	= texels_of_weight[i][j];
			t.wi[j]		= texelweights_of_weight[i][j];
			t.wf[j]		= texelweights_of_weight[i][j];
		}
	}
};

DecimationTable::DecimationTable(int xdim, int ydim, int zdim, int X, int Y, int Z) {
	num_texels	= xdim * ydim * zdim;
	num_weights	= X * Y * Z;

	collect_weights	cw;

	if (zdim > 1) {
		for (int z = 0; z < zdim; z++) {
			for (int y = 0; y < ydim; y++) {
				for (int x = 0; x < xdim; x++) {
					int		weight[8];
					calc_weights(weight, X, Y, Z,
						(((1024 + xdim / 2) / (xdim - 1)) * x * (X - 1) + 32) >> 6,
						(((1024 + ydim / 2) / (ydim - 1)) * y * (Y - 1) + 32) >> 6,
						(((1024 + zdim / 2) / (zdim - 1)) * z * (Z - 1) + 32) >> 6
					);
					cw.add((z * ydim + y) * xdim + x, weight);
				}
			}
		}
	} else {
		for (int y = 0; y < ydim; y++) {
			for (int x = 0; x < xdim; x++) {
				int		weight[8];
				calc_weights(weight, X, Y,
					(((1024 + xdim / 2) / (xdim - 1)) * x * (X - 1) + 32) >> 6,
					(((1024 + ydim / 2) / (ydim - 1)) * y * (Y - 1) + 32) >> 6
				);
				cw.add(y * xdim + x, weight);
			}
		}
	}

	for (int i = 0; i < num_texels; i++)
		cw.put(i, texels[i]);
	for (int i = 0; i < num_weights; i++)
		cw.put(i, weights[i]);
}

//-----------------------------------------------------------------------------
//	BlockSizeDescriptor
//-----------------------------------------------------------------------------

struct BlockSizeDescriptor {
	struct Mode {
		int8		decimation;
		Quant		qmode;
		bool		dual, permit_encode, permit_decode;
		float		percentile;

		Mode() : decimation(-1), qmode(QUANT_ILLEGAL), dual(false), permit_encode(false), permit_decode(false), percentile(1) {}
	};

	struct Decimation {
		int			samples;
		Quant		maxprec[2];
		float		percentile;
		bool		permit_encode;
		const DecimationTable *tables;

		Decimation(int xdim, int ydim, int zdim, int x, int y, int z) : samples(x * y * z), percentile(1), permit_encode(x <= xdim && y <= ydim && z <= zdim) {
			int max1	= -1;
			int max2	= -1;

			for (int i = 0; i < 12; i++) {
				if (between(ISE_bits((Quant)i, samples), ASTC::MIN_WEIGHT_BITS_PER_BLOCK, ASTC::MAX_WEIGHT_BITS_PER_BLOCK))
					max1 = i;
				if (between(ISE_bits((Quant)i, 2 * samples), ASTC::MIN_WEIGHT_BITS_PER_BLOCK, ASTC::MAX_WEIGHT_BITS_PER_BLOCK))
					max2 = i;
			}

			maxprec[0]	= (Quant)max1;
			maxprec[1]	= (Quant)max2;
			tables		= new DecimationTable(xdim, ydim, zdim, x, y, z);
		}
	};

	dynamic_array<Decimation>	dec;
	Mode						modes[2048];
	BlockSizeDescriptor(int xdim, int ydim, int zdim);

	void ComputeAngularEndpoints1(float mode_cutoff, const float *decimated_quantized_weights, const float *decimated_weights, float *low_value, float *high_value) const;
	void ComputeAngularEndpoints2(float mode_cutoff, const float *decimated_quantized_weights, const float *decimated_weights, float *low_value1, float *high_value1, float *low_value2, float *high_value2) const;

	static BlockSizeDescriptor *pointers[];
	static const BlockSizeDescriptor *get(int xdim, int ydim, int zdim) {
		int i = xdim + (ydim << 4) + (zdim << 8);
		if (!pointers[i])
			pointers[i] = new BlockSizeDescriptor(xdim, ydim, zdim);
		return pointers[i];
	}
};

BlockSizeDescriptor *BlockSizeDescriptor::pointers[4096];

BlockSizeDescriptor::BlockSizeDescriptor(int xdim, int ydim, int zdim) {
	int dec_index[1024];
	for (int i = 0; i < 1024; i++)
		dec_index[i] = -1;

	// gather all the infill-modes that can be used with the current block size
	if (zdim > 1) {
		for (int x = 2; x <= 6; x++) {
			for (int y = 2; y <= 6; y++) {
				for (int z = 2; z <= 6; z++) {
					if (x * y * z <= ASTC::MAX_WEIGHTS_PER_BLOCK) {
						dec_index[(z - 1) * 128 + y * 16 + x] = int(dec.size());
						dec.emplace_back(xdim, ydim, zdim, x, y, z);
					}
				}
			}
		}
	} else {
		for (int x = 2; x <= 12; x++) {
			for (int y = 2; y <= 12; y++) {
				if (x * y <= ASTC::MAX_WEIGHTS_PER_BLOCK) {
					dec_index[y * 16 + x] = int(dec.size());
					dec.emplace_back(xdim, ydim, 1, x, y, 1);
				}
			}
		}
	}

	//const float *percentiles = 0;//get_3d_percentile_table(xdim, ydim, zdim);

	// then construct the list of block formats
	for (int i = 0; i < 2048; i++) {
		BlockMode	bm;
		if (bm.decode(i, zdim > 1)) {
			int d = dec_index[(bm.Z - 1) * 128 + bm.Y * 16 + bm.X];
			modes[i].decimation		= d;
			modes[i].qmode			= bm.qmode;
			modes[i].dual			= bm.dual;
			modes[i].permit_encode	= modes[i].permit_decode = bm.X <= xdim && bm.Y <= ydim && bm.Z <= zdim;
			//modes[i].percentile		= percentiles[i];
			//if (dec[d].percentile > percentiles[i])
			//	dec[d].percentile = percentiles[i];
		}
	}
}

//-----------------------------------------------------------------------------
//	CompressionParams
//-----------------------------------------------------------------------------

void CompressionParams::ExpandBlockArtifactSuppression(int xdim, int ydim, int zdim) {
	float centerpos_x	= (xdim - 1) * 0.5f;
	float centerpos_y	= (ydim - 1) * 0.5f;
	float centerpos_z	= (zdim - 1) * 0.5f;
	float *bef			= block_artifact_suppression_expanded;

	for (int z = 0; z < zdim; z++) {
		for (int y = 0; y < ydim; y++) {
			for (int x = 0; x < xdim; x++)
				*bef++ = pow(square((x - centerpos_x) / xdim) + square((y - centerpos_y) / ydim) + square((z - centerpos_z) / zdim) + square(0.36f), block_artifact_suppression / 2);
		}
	}
}

void CompressionParams::ComputeAverages(const block<HDRpixel, 3> &b) {
	int xsize			= b.size<1>();
	int ysize			= b.size<2>();
	int zsize			= b.size<3>();

	input_averages		= make_auto_block<float4>(xsize, ysize, zsize);
	input_variances		= make_auto_block<float4>(xsize, ysize, zsize);
	input_alpha_averages= make_auto_block<float>(xsize, ysize, zsize);

	bool use_z_axis		= zsize > 1;

	int	rc				= int(stdev_radius.rgb);
	int	ra				= int(stdev_radius.a);

	int kernel_radius	= max(rc, ra);
	int kernel_radius_z	= use_z_axis ? kernel_radius : 0;
	int kernel_dim		= 2 * kernel_radius + 1;
	int kernel_dim_z	= 2 * kernel_radius_z + 1;

	// allocate memory
	int xpadsize		= xsize + kernel_dim;
	int ypadsize		= ysize + kernel_dim;
	int zpadsize		= zsize + kernel_dim_z;

	auto_block<double4p, 3>	varbuf1	= make_auto_block<double4p>(xpadsize, ypadsize, zpadsize);
	auto_block<double4p, 3>	varbuf2	= make_auto_block<double4p>(xpadsize, ypadsize, zpadsize);

	bool powers_are_1 = power.rgb == 1 && power.a == 1;
	for (int z = 0; z < zpadsize - 1; z++) {
		for (int y = 0; y < ypadsize - 1; y++) {
			for (int x = 0; x < xpadsize - 1; x++) {
				HDRpixel	c = b[z][y][x];
				if (flags.test(PERFORM_SRGB_TRANSFORM))
					c = srgb_to_linear(c);

				double4p d = {c.r, c.g, c.b, c.a};
				if (!powers_are_1) {
					d.x = pow(max(d.x, 1e-6), (double)power.rgb);
					d.y = pow(max(d.y, 1e-6), (double)power.rgb);
					d.z = pow(max(d.z, 1e-6), (double)power.rgb);
					d.w = pow(max(d.w, 1e-6), (double)power.a);
				}

				varbuf1[z][y][x] = d;
				varbuf2[z][y][x] = square(d);
			}
		}
	}

	// pad out buffers with 0s
	for (int z = 0; z < zpadsize; z++) {
		for (int y = 0; y < ypadsize; y++) {
			clear(varbuf1[z][y][xpadsize - 1]);
			clear(varbuf2[z][y][xpadsize - 1]);
		}
		for (int x = 0; x < xpadsize; x++) {
			clear(varbuf1[z][ypadsize - 1][x]);
			clear(varbuf2[z][ypadsize - 1][x]);
		}
	}

	if (use_z_axis) {
		for (int y = 0; y < ypadsize; y++) {
			for (int x = 0; x < xpadsize; x++) {
				clear(varbuf1[zpadsize - 1][y][x]);
				clear(varbuf2[zpadsize - 1][y][x]);
			}
		}
	}


	// generate summed-area tables for x and x2; this is done in-place
	for (int z = 0; z < zpadsize; z++) {
		for (int y = 0; y < ypadsize; y++) {
			double4p summa1 = double4p{0.0, 0.0, 0.0, 0.0};
			double4p summa2 = double4p{0.0, 0.0, 0.0, 0.0};
			for (int x = 0; x < xpadsize; x++) {
				double4p val1 = varbuf1[z][y][x];
				double4p val2 = varbuf2[z][y][x];
				varbuf1[z][y][x] = summa1;
				varbuf2[z][y][x] = summa2;
				summa1 = summa1 + val1;
				summa2 = summa2 + val2;
			}
		}
	}

	for (int z = 0; z < zpadsize; z++) {
		for (int x = 0; x < xpadsize; x++) {
			double4p summa1 = double4p{0.0, 0.0, 0.0, 0.0};
			double4p summa2 = double4p{0.0, 0.0, 0.0, 0.0};
			for (int y = 0; y < ypadsize; y++) {
				double4p val1 = varbuf1[z][y][x];
				double4p val2 = varbuf2[z][y][x];
				varbuf1[z][y][x] = summa1;
				varbuf2[z][y][x] = summa2;
				summa1 = summa1 + val1;
				summa2 = summa2 + val2;
			}
		}
	}

	if (use_z_axis) {
		for (int y = 0; y < ypadsize; y++) {
			for (int x = 0; x < xpadsize; x++) {
				double4p summa1 = double4p{0.0, 0.0, 0.0, 0.0};
				double4p summa2 = double4p{0.0, 0.0, 0.0, 0.0};
				for (int z = 0; z < zpadsize; z++) {
					double4p val1 = varbuf1[z][y][x];
					double4p val2 = varbuf2[z][y][x];
					varbuf1[z][y][x] = summa1;
					varbuf2[z][y][x] = summa2;
					summa1 = summa1 + val1;
					summa2 = summa2 + val2;
				}
			}
		}
	}

	int		avg_var_kerneldim	= 2 * rc + 1;
	int		alpha_kerneldim		= 2 * ra + 1;

	// compute a few constants used in the variance-calculation.
	double	avg_var_samples		= use_z_axis ? cube(avg_var_kerneldim) : square(avg_var_kerneldim);
	double	avg_var_rsamples	= 1 / avg_var_samples;
	double	alpha_rsamples		= 1 / (use_z_axis ? cube(alpha_kerneldim) : square(alpha_kerneldim));
	double	mul1				= avg_var_samples == 1 ? 1 : 1 / (avg_var_samples * (avg_var_samples - 1));
	double	mul2				= avg_var_samples * mul1;

	// use the summed-area tables to compute variance for each sample-neighborhood
	if (use_z_axis) {
		for (int z = 0; z < b.size<3>(); z++) {
			int z_src = z + kernel_radius_z;
			for (int y = 0; y < b.size<2>(); y++) {
				int y_src = y + kernel_radius;

				for (int x = 0; x < b.size<1>(); x++) {
					int x_src = x + kernel_radius;

					// summed-area table lookups for alpha average
					double vasum = (
							varbuf1[z_src + 1][y_src - ra][x_src - ra].w
						-	varbuf1[z_src + 1][y_src - ra][x_src + ra + 1].w
						-	varbuf1[z_src + 1][y_src + ra + 1][x_src - ra].w
						+	varbuf1[z_src + 1][y_src + ra + 1][x_src + ra + 1].w
						) - (
							varbuf1[z_src][y_src - ra][x_src - ra].w
						-	varbuf1[z_src][y_src - ra][x_src + ra + 1].w
						-	varbuf1[z_src][y_src + ra + 1][x_src - ra].w
						+	varbuf1[z_src][y_src + ra + 1][x_src + ra + 1].w
						);
					input_alpha_averages[z][y][x] = float(vasum * alpha_rsamples);


					// summed-area table lookups for rgba average
					double4p v0sum = (
							varbuf1[z_src + 1][y_src - rc][x_src - rc]
						-	varbuf1[z_src + 1][y_src - rc][x_src + rc + 1]
						-	varbuf1[z_src + 1][y_src + rc + 1][x_src - rc]
						+	varbuf1[z_src + 1][y_src + rc + 1][x_src + rc + 1]
						) - (
							varbuf1[z_src][y_src - rc][x_src - rc]
						-	varbuf1[z_src][y_src - rc][x_src + rc + 1]
						-	varbuf1[z_src][y_src + rc + 1][x_src - rc]
						+	varbuf1[z_src][y_src + rc + 1][x_src + rc + 1]
						);

					double4p avg = v0sum * avg_var_rsamples;
					input_averages[z][y][x] = float4{float(avg.x), float(avg.y), float(avg.z), float(avg.w)};


					// summed-area table lookups for variance
					double4p v1sum = (
							varbuf1[z_src + 1][y_src - rc][x_src - rc]
						-	varbuf1[z_src + 1][y_src - rc][x_src + rc + 1]
						-	varbuf1[z_src + 1][y_src + rc + 1][x_src - rc]
						+	varbuf1[z_src + 1][y_src + rc + 1][x_src + rc + 1]
						) - (
							varbuf1[z_src][y_src - rc][x_src - rc]
						-	varbuf1[z_src][y_src - rc][x_src + rc + 1]
						-	varbuf1[z_src][y_src + rc + 1][x_src - rc]
						+	varbuf1[z_src][y_src + rc + 1][x_src + rc + 1]
						);
					double4p v2sum = (
							varbuf2[z_src + 1][y_src - rc][x_src - rc]
						-	varbuf2[z_src + 1][y_src - rc][x_src + rc + 1]
						-	varbuf2[z_src + 1][y_src + rc + 1][x_src - rc]
						+	varbuf2[z_src + 1][y_src + rc + 1][x_src + rc + 1]
						) - (
							varbuf2[z_src][y_src - rc][x_src - rc]
						-	varbuf2[z_src][y_src - rc][x_src + rc + 1]
						-	varbuf2[z_src][y_src + rc + 1][x_src - rc]
						+	varbuf2[z_src][y_src + rc + 1][x_src + rc + 1]
						);

					// the actual variance
					double4p variance = v2sum * mul2 - (v1sum * v1sum) * mul1;
					input_variances[z][y][x] = float4{float(variance.x), float(variance.y), float(variance.z), float(variance.w)};
				}
			}
		}
	} else {
		for (int z = 0; z < b.size<3>(); z++) {
			int z_src = z;
			for (int y = 0; y < b.size<2>(); y++) {
				int y_src = y + kernel_radius;

				for (int x = 0; x < b.size<1>(); x++) {
					int x_src = x + kernel_radius;

					// summed-area table lookups for alpha average
					double vasum =	varbuf1[z_src][y_src - ra][x_src - ra].w
								-	varbuf1[z_src][y_src - ra][x_src + ra + 1].w
								-	varbuf1[z_src][y_src + ra + 1][x_src - ra].w
								+	varbuf1[z_src][y_src + ra + 1][x_src + ra + 1].w;
					input_alpha_averages[z][y][x] = float(vasum * alpha_rsamples);

					// summed-area table lookups for rgba average
					double4p v0sum =	varbuf1[z_src][y_src - rc][x_src - rc]
								-	varbuf1[z_src][y_src - rc][x_src + rc + 1]
								-	varbuf1[z_src][y_src + rc + 1][x_src - rc]
								+	varbuf1[z_src][y_src + rc + 1][x_src + rc + 1];

					double4p avg = v0sum * avg_var_rsamples;
					input_averages[z][y][x] = float4{float(avg.x), float(avg.y), float(avg.z), float(avg.w)};

					// summed-area table lookups for variance
					double4p v1sum =	varbuf1[z_src][y_src - rc][x_src - rc]
								-	varbuf1[z_src][y_src - rc][x_src + rc + 1]
								-	varbuf1[z_src][y_src + rc + 1][x_src - rc]
								+	varbuf1[z_src][y_src + rc + 1][x_src + rc + 1];
					double4p v2sum =	varbuf2[z_src][y_src - rc][x_src - rc]
								-	varbuf2[z_src][y_src - rc][x_src + rc + 1]
								-	varbuf2[z_src][y_src + rc + 1][x_src - rc]
								+	varbuf2[z_src][y_src + rc + 1][x_src + rc + 1];

					// the actual variance
					double4p variance = v2sum * mul2 - mul1 * (v1sum * v1sum);
					input_variances[z][y][x] = float4{float(variance.x), float(variance.y), float(variance.z), float(variance.w)};
				}
			}
		}
	}
	delete[]varbuf2[0][0];
	delete[]varbuf1[0][0];
	delete[]varbuf2[0];
	delete[]varbuf1[0];
	delete[]varbuf2;
	delete[]varbuf1;
}

//-----------------------------------------------------------------------------
//	ErrorWeightBlock
//-----------------------------------------------------------------------------

struct ImageStats {
	float4		a, b;
	bool		grayscale;
	bool		hdr_rgb, hdr_alpha;
	uint32		num_texels;
	float4		texel[ASTC::MAX_TEXELS_PER_BLOCK];

	template<typename T> ImageStats(const block<T, 3> &block, int xdim, int ydim, int zdim) : a(1e38f), b(-1e38f), grayscale(true), num_texels(xdim * ydim * zdim) {
		clear(texel);
		hdr_rgb		= true;
		hdr_alpha	= true;
		for (int z = 0, i = 0; z < zdim; z++) {
			for (int y = 0; y < ydim; y++) {
				for (int x = 0; x < xdim; x++, i++) {
					if (z < block.template size<3>() && y < block.template size<2>() && x < block.template size<1>()) {
						HDRpixel	c = block[z][y][x];
						texel[i]	= float4{c.r, c.g, c.b, c.a};
						grayscale	= grayscale && (texel[i].x == texel[i].y && texel[i].x == texel[i].z);
						a = min(a, texel[i]);
						b = max(b, texel[i]);
					}
				}
			}
		}
	}
	bool	HasAlpha() const { return a.w != b.w; }
};

struct ErrorWeightBlock {//10,12,
	typedef	float	weights[ASTC::MAX_TEXELS_PER_BLOCK];
	union {
		struct {
			weights	r;		//1
			weights	g;		//2
			weights	rg;		//3
			weights	b;		//4
			weights	rb;		//5
			weights	gb;		//6
			weights	rgb;	//7
			weights	a;		//8
			weights	ra;		//9
			weights	ga;		//10
			weights	rga;	//11
			weights	ba;		//12
			weights	rba;	//13
			weights	gba;	//14
			weights	rgba;	//15
		};
		weights	all[15];
	};
	float4	v[ASTC::MAX_TEXELS_PER_BLOCK];

	void set(int i, param(float4) e) {
		v[i]	= e;

		r[i]	= e.x;
		g[i]	= e.y;
		b[i]	= e.z;
		a[i]	= e.w;

		rg[i]	= (e.x + e.y) * half;
		rb[i]	= (e.x + e.z) * half;
		gb[i]	= (e.y + e.z) * half;
		ra[i]	= (e.x + e.w) * half;
		ga[i]	= (e.y + e.w) * half;
		ba[i]	= (e.z + e.w) * half;

		gba[i]	= (e.y + e.z + e.w) * third;
		rba[i]	= (e.x + e.z + e.w) * third;
		rga[i]	= (e.x + e.y + e.w) * third;
		rgb[i]	= (e.x + e.y + e.z) * third;

		rgba[i]	= (e.x + e.y + e.z + e.w) * quarter;
	}

	bool	contains_zeroweight_texels;
	float	total;

	ErrorWeightBlock(const block<ISO_rgba, 3> &block, int xdim, int ydim, int zdim, int xoffset, int yoffset, int zoffset, const CompressionParams *params);

	const float	*GetWeights(int mask) const { return all[mask - 1]; }

	void	PrepareStatistics(const ImageStats &block, bool *is_normal_map, float *lowest_correl) const;
	void	PartitionWeightings(const PartitionTable &pt, float4 error_weightings[4], float4 color_scalefactors[4]) const;
	void	Directions(const ImageStats &block, const PartitionTable &pt, const float4 *color_scalefactors, float4 *averages, float4 *directions) const;
	float	CalcErrorSquared(const ImageStats &block, const PartitionTable &pt, const processed_line<4> *lines, float *length_of_lines) const;
	float	CalcErrorSquared(const ImageStats &block, const PartitionTable &pt, int partition_to_test, const processed_line<3> &line) const;
};

inline float deriv_linear_to_srgb(float r) {
	return r <= 0.0031308f	? 12.92f : 0.4396f * pow(r, -0.58333f);
}

ErrorWeightBlock::ErrorWeightBlock(const block<ISO_rgba, 3> &block, int xdim, int ydim, int zdim, int xoffset, int yoffset, int zoffset, const CompressionParams *params) : contains_zeroweight_texels(false), total(0) {
	bool	any_mean_stdev_weight	= params->AnyMeanStdevWeight();
	float4	color_weights			= load<float4>(params->rgba_weights);
	float4	error_weight_sum(zero);

	for (int z = 0, idx = 0; z < zdim; z++) {
		int	zpos = z + zoffset;
		for (int y = 0; y < ydim; y++) {
			int	ypos = y + yoffset;
			for (int x = 0; x < xdim; x++, idx++) {
				int	xpos = x + zoffset;

				if (x >= block.size<1>() || y >= block.size<2>() || z >= block.size<3>()) {
					set(idx, float4(1e-11f));
					contains_zeroweight_texels	= true;

				} else {
					HDRpixel	c		= block[z][y][x];
					float4 error_weight = params->base_weight;

					if (any_mean_stdev_weight) {
						float4 avg		= square(max(params->input_averages[zpos][ypos][xpos], 6e-5f));
						float4 variance = square(params->input_variances[zpos][ypos][xpos]);

						avg.xyz			= lerp(avg.xyz,			float3((avg.x + avg.y + avg.z) * third),				params->rgb_mean_and_stdev_mixing) * params->mean_weight.rgb;
						variance.xyz	= lerp(variance.xyz,	float3((variance.x + variance.y + variance.z) * third),	params->rgb_mean_and_stdev_mixing);
						float4 stdev	= sqrt(max(variance, zero)) * params->stdev_weight.operator iso::float4();

						error_weight = reciprocal(error_weight + avg + stdev);
					}

					if (params->flags.test(CompressionParams::RA_NORMAL_ANGULAR_SCALE)) {
						float x		= (c.r - 0.5f) * 2;
						float y		= (c.a - 0.5f) * 2;
						float denom = 1 / max(1 - x * x - y * y, .1f);
						error_weight.x *= 1 + x * x * denom;
						error_weight.w *= 1 + y * y * denom;
					}

					if (params->flags.test(CompressionParams::ENABLE_RGB_SCALE_WITH_ALPHA))
						error_weight.xyz = error_weight.xyz * square(max(params->stdev_radius.a != 0 ? params->input_alpha_averages[zpos][ypos][xpos] : float3(c.a), 0.0001f));

					error_weight = error_weight * color_weights;
					error_weight = error_weight * params->block_artifact_suppression_expanded[idx];

					// if we perform a conversion from linear to sRGB, then we multiply the weight with the derivative of the linear->sRGB transform function.
					if (params->flags.test(CompressionParams::PERFORM_SRGB_TRANSFORM)) {
						error_weight.x *= deriv_linear_to_srgb(c.r);
						error_weight.y *= deriv_linear_to_srgb(c.g);
						error_weight.z *= deriv_linear_to_srgb(c.b);
					}

#if 0
					// when we loaded the block to begin with, we applied a transfer function
					// and computed the derivative of the transfer function. However, the
					// error-weight computation so far is based on the original color values,
					// not the transfer-function values. As such, we must multiply the
					// error weights by the derivative of the inverse of the transfer function,
					// which is equivalent to dividing by the derivative of the transfer
					// function.
					ewbo->error_weights[idx] = error_weight;
					error_weight.x /= (blk->deriv_data[4 * idx] * blk->deriv_data[4 * idx] * 1e-10f);
					error_weight.y /= (blk->deriv_data[4 * idx + 1] * blk->deriv_data[4 * idx + 1] * 1e-10f);
					error_weight.z /= (blk->deriv_data[4 * idx + 2] * blk->deriv_data[4 * idx + 2] * 1e-10f);
					error_weight.w /= (blk->deriv_data[4 * idx + 3] * blk->deriv_data[4 * idx + 3] * 1e-10f);
#endif
					error_weight_sum += error_weight;
					set(idx, error_weight);
					contains_zeroweight_texels = contains_zeroweight_texels || reduce_add(error_weight) < 1e-10f;
				}
			}
		}
	}
	total = reduce_add(error_weight_sum);
}

void ErrorWeightBlock::PrepareStatistics(const ImageStats &block, bool *is_normal_map, float *lowest_correl) const {
	float4	sum(zero);
	float4	r_sum(zero);
	float4	g_sum(zero);
	float4	b_sum(zero);
	float	aa_sum = 0.0f;

	float	weight_sum = 0.0f;

	for (int i = 0; i < block.num_texels; i++) {
		float weight = rgba[i];
		weight_sum	+= weight;
		float4 c(block.texel[i]);
		sum		+= c * weight;
		r_sum	+= c * (c.x * weight);
		g_sum	+= c * (c.y * weight);
		b_sum	+= c * (c.z * weight);
		aa_sum	+= c.w * (c.w * weight);
	}

	float4x4	cov_matrix = float4x4(
		float4{r_sum.x - sum.x * sum.x, g_sum.x - sum.x * sum.y, b_sum.x - sum.x * sum.z, r_sum.w - sum.x * sum.w},
		float4{r_sum.y - sum.x * sum.y, g_sum.y - sum.y * sum.y, b_sum.y - sum.y * sum.z, g_sum.w - sum.y * sum.w},
		float4{r_sum.z - sum.x * sum.z, g_sum.z - sum.y * sum.z, b_sum.z - sum.z * sum.z, b_sum.w - sum.z * sum.w},
		float4{r_sum.w - sum.x * sum.w, g_sum.w - sum.y * sum.w, b_sum.w - sum.z * sum.w, aa_sum  - sum.w * sum.w}
	) * reciprocal(max(weight_sum, 1e-7f));

	// use the covariance matrix to compute correllation coefficients
	float rr_var = cov_matrix.x.x;
	float gg_var = cov_matrix.y.y;
	float bb_var = cov_matrix.z.z;
	float aa_var = cov_matrix.w.w;

	float rg_correlation = cov_matrix.x.y / sqrt(max(rr_var * gg_var, 1e-30f));
	float rb_correlation = cov_matrix.x.z / sqrt(max(rr_var * bb_var, 1e-30f));
	float ra_correlation = cov_matrix.x.w / sqrt(max(rr_var * aa_var, 1e-30f));
	float gb_correlation = cov_matrix.y.z / sqrt(max(gg_var * bb_var, 1e-30f));
	float ga_correlation = cov_matrix.y.w / sqrt(max(gg_var * aa_var, 1e-30f));
	float ba_correlation = cov_matrix.z.w / sqrt(max(bb_var * aa_var, 1e-30f));

	if (is_nan(rg_correlation))
		rg_correlation = 1.0f;
	if (is_nan(rb_correlation))
		rb_correlation = 1.0f;
	if (is_nan(ra_correlation))
		ra_correlation = 1.0f;
	if (is_nan(gb_correlation))
		gb_correlation = 1.0f;
	if (is_nan(ga_correlation))
		ga_correlation = 1.0f;
	if (is_nan(ba_correlation))
		ba_correlation = 1.0f;

	*lowest_correl	= min(min(min(min(min(abs(rg_correlation), abs(rb_correlation)), abs(ra_correlation)), abs(gb_correlation)), abs(ga_correlation)), abs(ba_correlation));

	// compute a "normal-map" factor
	float nf_sum = 0.0f;
	for (int i = 0; i < block.num_texels; i++)
		nf_sum += abs(len2((block.texel[i].xyz - half) * 2) - 1);

	*is_normal_map = nf_sum / block.num_texels < 0.2;
}

float ErrorWeightBlock::CalcErrorSquared(const ImageStats &block, const PartitionTable &pt, int partition_to_test, const processed_line<3> &line) const {
	float error = 0;
	for (int i = 0; i < block.num_texels; i++) {
		if (pt.partition[i] == partition_to_test && rgb[i] > 1e-20) {
			float3	point	= block.texel[i].xyz;
			error		+= dot(v[i].xyz, square(line.amod + line.bis * dot(point, line.bs) - point));
		}
	}
	return error;
}

float ErrorWeightBlock::CalcErrorSquared(const ImageStats &block, const PartitionTable &pt, const processed_line<4> * plines, float *length_of_lines) const {
	float error = 0;
	for (int partition = 0; partition < pt.num_partitions; partition++) {
		const uint8 *weights	= pt.texels[partition];
		int			texelcount	= pt.counts[partition];
		float		lowparam	= 1e10;
		float		highparam	= -1e10;

		processed_line<4> l = plines[partition];

		for (int i = 0; i < texelcount; i++) {
			int iwt = weights[i];
			if (!contains_zeroweight_texels || rgba[iwt] > 1e-20) {
				float4	point = float4(block.texel[iwt]);
				float	param = dot(point, l.bs);
				error += dot(v[iwt], square(l.amod + param * l.bis - point));
				if (param < lowparam)
					lowparam = param;
				if (param > highparam)
					highparam = param;
			}
		}

		length_of_lines[partition] = max(highparam - lowparam, 1e-7f);
	}

	return error;
}

void ErrorWeightBlock::PartitionWeightings(const PartitionTable &pt, float4 error_weightings[4], float4 color_scalefactors[4]) const {
	int	num_texels			= pt.xdim * pt.ydim * pt.zdim;
	int num_partitions		= pt.num_partitions;

	for (int i = 0; i < num_partitions; i++)
		error_weightings[i] = float4(1e-12f);

	for (int i = 0; i < num_texels; i++) {
		int part = pt.partition[i];
		error_weightings[part] = error_weightings[part] + rgba[i];
	}

	for (int i = 0; i < num_partitions; i++) {
		error_weightings[i]		= error_weightings[i] / pt.counts[i];
		color_scalefactors[i]	= sqrt(error_weightings[i]);
	}
}

void ErrorWeightBlock::Directions(const ImageStats &block, const PartitionTable &pt, const float4 *color_scalefactors, float4 *averages, float4 *directions) const {
	int num_partitions = pt.num_partitions;

	for (int partition = 0; partition < num_partitions; partition++) {
		float4			scales		= color_scalefactors[partition];
		const float		*weights	= GetWeights(bit_mask(scales != 0));
		const uint8		*texels		= pt.texels[partition];
		int				texelcount	= pt.counts[partition];
		float4			base_sum(zero);
		float			partition_weight = 0;


		for (int i = 0; i < texelcount; i++) {
			int			iwt			= texels[i];
			float4		texel_datum = float4(block.texel[iwt]) * weights[iwt];
			partition_weight += weights[iwt];
			base_sum		= base_sum + texel_datum;
		}

		float4 average		= base_sum / max(partition_weight, 1e-7f);
		averages[partition] = average * scales;

		float4 sum_xp(zero);
		float4 sum_yp(zero);
		float4 sum_zp(zero);
		float4 sum_wp(zero);

		for (int i = 0; i < texelcount; i++) {
			int		iwt		= texels[i];
			float4	texel_datum = (float4(block.texel[iwt]) - average) * weights[iwt];

			if (texel_datum.x > 0.0f)
				sum_xp = sum_xp + texel_datum;
			if (texel_datum.y > 0.0f)
				sum_yp = sum_yp + texel_datum;
			if (texel_datum.z > 0.0f)
				sum_zp = sum_zp + texel_datum;
			if (texel_datum.w > 0.0f)
				sum_wp = sum_wp + texel_datum;
		}

		float prod_xp = dot(sum_xp, sum_xp);
		float prod_yp = dot(sum_yp, sum_yp);
		float prod_zp = dot(sum_zp, sum_zp);
		float prod_wp = dot(sum_wp, sum_wp);

		float4 best_vector	= sum_xp;
		float	best_sum	= prod_xp;
		if (prod_yp > best_sum) {
			best_vector = sum_yp;
			best_sum = prod_yp;
		}
		if (prod_zp > best_sum) {
			best_vector = sum_zp;
			best_sum = prod_zp;
		}
		if (prod_wp > best_sum) {
			best_vector = sum_wp;
			best_sum = prod_wp;
		}

		directions[partition]	= best_vector;
	}
}

//-----------------------------------------------------------------------------
//	Endpoints
//-----------------------------------------------------------------------------

struct Endpoints {
	int		num_partitions;
	float4	endpt0[4];
	float4	endpt1[4];

	Endpoints	Merge(const Endpoints &b, int comp) const {
		Endpoints	r = *this;
		for (int i = 0; i < num_partitions; i++) {
			r.endpt0[i][comp] = b.endpt0[i][comp];
			r.endpt1[i][comp] = b.endpt1[i][comp];
		}
		return r;
	}

	void RecomputeIdeal(const ImageStats &block, const PartitionTable &pt, const DecimationTable &dt, Quant weight_qmode,
		float4		*rgbs_vectors,			// used to return RGBS-vectors. (endpoint mode #6)
		float4		*rgbo_vectors,			// used to return RGBO-vectors. (endpoint mode #7)
		float2		*lum_vectors,			// used to return luminance-vectors.
		const uint8 *weight_set8,			// the current set of weight values
		const uint8 *plane2_weight_set8,	// NULL if plane 2 is not actually used.
		int plane2_color_component,			// color component for 2nd plane of weights; -1 if the 2nd plane of weights is not present
		const ErrorWeightBlock *ewb
	);
};

struct EndpointsWeights : Endpoints {
	float weights[ASTC::MAX_TEXELS_PER_BLOCK];
	float weight_error_scale[ASTC::MAX_TEXELS_PER_BLOCK];

	void ComputeWithMask(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, param(float4) mask);
	void Compute1Component(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, int component);

	void Compute2Component(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, int component1, int component2) {
		float4	mask(zero);
		mask[component1] = one;
		mask[component2] = one;
		ComputeWithMask(block, pt, ewb, mask);
	}
	void Compute3Component(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, int component1, int component2, int component3) {
		float4	mask(zero);
		mask[component1] = one;
		mask[component2] = one;
		mask[component3] = one;
		ComputeWithMask(block, pt, ewb, mask);
	}
	void Compute4Component(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb) {
		ComputeWithMask(block, pt, ewb, float4(one));
	}

	void ComputeIdealWeights(const DecimationTable &d, float *weight_set, float *weights);
	void ComputeIdealWeights(const DecimationTable &d, float low_bound, float high_bound, const float *weight_set_in, float *weight_set_out, uint8 *quantized_weight_set, Quant qmode);

	float CalcError(const DecimationTable &d, const float *weights, int i) const {
		return square(d.texels[i].compute(weights) - weights[i]) * weight_error_scale[i];
	}

	float CalcError(const DecimationTable &d, const float *weights) const {
		float error = 0;
		for (int i = 0; i < d.num_texels; i++)
			error += CalcError(d, weights, i);
		return error;
	}

	void CalcTwoErrorChanges(const DecimationTable &d, float *infilled_weights, int w, float perturbation1, float perturbation2, float *res1, float *res2);

	EndpointsWeights() {}
	EndpointsWeights(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb) {
		if (block.HasAlpha())
			Compute4Component(block, pt, ewb);
		else
			Compute3Component(block, pt, ewb, 0, 1, 2);
	}
};

void EndpointsWeights::Compute1Component(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, int component) {
	num_partitions		= pt.num_partitions;

	float lowvalues[4], highvalues[4];
	float partition_error_scale[4];
	float linelengths_rcp[4];

	for (int i = 0; i < num_partitions; i++) {
		lowvalues[i]	= 1e10;
		highvalues[i]	= -1e10;
	}

	const float *error_weights = ewb->GetWeights(1<<component);

	for (int i = 0; i < block.num_texels; i++) {
		if (error_weights[i] > 1e-10) {
			float	value			= block.texel[i][component];
			int		partition		= pt.partition[i];
			lowvalues[partition]	= min(lowvalues[partition], value);
			highvalues[partition]	= max(highvalues[partition], value);
		}
	}

	for (int i = 0; i < num_partitions; i++) {
		float diff = highvalues[i] - lowvalues[i];
		if (diff < 0) {
			lowvalues[i]	= 0;
			highvalues[i]	= 0;
		}
		if (diff < 1e-7f)
			diff = 1e-7f;
		partition_error_scale[i] = diff * diff;
		linelengths_rcp[i] = 1.0f / diff;
	}

	for (int i = 0; i < block.num_texels; i++) {
		int		partition		= pt.partition[i];
		weights[i]				= clamp((block.texel[i][component] - lowvalues[partition]) * linelengths_rcp[partition], 0, 1);
		weight_error_scale[i]	= partition_error_scale[partition] * error_weights[i];
	}

	for (int i = 0; i < num_partitions; i++) {
		endpt0[i]				= float4(block.a);
		endpt1[i]				= float4(block.b);
		endpt0[i][component]	= lowvalues[i];
		endpt1[i][component]	= highvalues[i];
	}
}

void EndpointsWeights::ComputeWithMask(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, param(float4) mask) {
	num_partitions = pt.num_partitions;

	float4 error_weightings[4];
	float4 color_scalefactors[4];

	ewb->PartitionWeightings(pt, error_weightings, color_scalefactors);

	float	masklen	= len(mask);
	for (int i = 0; i < num_partitions; i++)
		color_scalefactors[i] = normalise(color_scalefactors[i] * mask) * masklen;


	float4	averages[4], directions[4];
	ewb->Directions(block, pt, color_scalefactors, averages, directions);

	raw_line<4> lines[4];
	float	lowparam[4], highparam[4];

	for (int i = 0; i < num_partitions; i++) {
		if (reduce_add(directions[i]) < 0)
			directions[i] = -directions[i];

		lines[i].a = averages[i];
		if (dot(directions[i], directions[i]) == 0)
			lines[i].b = normalise(mask);
		else
			lines[i].b = normalise(directions[i]);

		lowparam[i]		= 1e10;
		highparam[i]	= -1e10;
	}

	const float *error_weights = ewb->GetWeights(bit_mask(mask != 0));
	for (int i = 0; i < block.num_texels; i++) {
		if (error_weights[i] > 1e-10) {
			int		partition	= pt.partition[i];
			float4	point		= float4(block.texel[i]) * color_scalefactors[partition];
			raw_line<4>	l			= lines[partition];
			float param			= dot(point - l.a, l.b);
			weights[i]			= param;
			if (param < lowparam[partition])
				lowparam[partition] = param;
			if (param > highparam[partition])
				highparam[partition] = param;
		} else {
			weights[i] = -1e38f;
		}
	}

	float4	lowvalues[4], highvalues[4];
	float	scale[4], length_squared[4];

	for (int i = 0; i < num_partitions; i++) {
		float length = highparam[i] - lowparam[i];
		if (length < 0)	{		// case for when none of the texels had any weight
			lowparam[i]		= 0;
			highparam[i]	= 1e-7f;
		}

		// it is possible for a uniform-color partition to produce length=0; this causes NaN-production and NaN-propagation later on. Set length to a small value to avoid this problem.
		if (length < 1e-7f)
			length = 1e-7f;

		length_squared[i]	= length * length;
		scale[i]			= 1.0f / length;
		lowvalues[i]		= (lines[i].a + lines[i].b * lowparam[i])  / color_scalefactors[i];
		highvalues[i]		= (lines[i].a + lines[i].b * highparam[i]) / color_scalefactors[i];
	}

	for (int i = 0; i < num_partitions; i++) {
		endpt0[i] = lerp(float4(block.a), lowvalues[i], mask);
		endpt1[i] = lerp(float4(block.a), highvalues[i], mask);
	}

	for (int i = 0; i < block.num_texels; i++) {
		int partition			= pt.partition[i];
		weights[i]				= clamp((weights[i] - lowparam[partition]) * scale[partition], 0, 1);
		weight_error_scale[i]	= length_squared[partition] * error_weights[i];
	}
}

void Compute2PlanesEndpoints(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb, int separate_component,
	EndpointsWeights *ei1,	// for the three components of the primary plane of weights
	EndpointsWeights *ei2	// for the remaining component.
) {
	float4	mask(one);
	if (!block.HasAlpha())
		mask.w = 0;
	mask[separate_component] = 0;
	ei1->ComputeWithMask(block, pt, ewb, mask);
	ei2->Compute1Component(block, pt, ewb, 0);
}

// this routine is rather heavily optimized since it consumes a lot of cpu time.
void EndpointsWeights::CalcTwoErrorChanges(const DecimationTable &d, float *infilled_weights, int w, float perturbation1, float perturbation2, float *res1, float *res2) {
	float	error_change0	= 0;
	float	error_change1	= 0;
	const uint8 *texel		= d.weights[w].texel;
	const float *weights	= d.weights[w].wf;
	for (int i = d.weights[w].num_texels - 1; i >= 0; i--) {
		uint8 t				= texel[i];
		float weight		= weights[i];
		float scale			= weight_error_scale[t] * weight;
		float old_weight	= infilled_weights[t];
		float ideal_weight	= weights[t];

		error_change0	+= weight * scale;
		error_change1	+= (old_weight - ideal_weight) * scale;
	}
	*res1 = error_change0 * square(perturbation1 / 16) + error_change1 * perturbation1 * (2.0f / 16);
	*res2 = error_change0 * square(perturbation2 / 16) + error_change1 * perturbation2 * (2.0f / 16);
}

//Given a complete weight set and a decimation table, try to compute the optimal weight set (assuming infinite precision)
void EndpointsWeights::ComputeIdealWeights(const DecimationTable &d, float *weight_set, float *weights) {
	int num_texels		= d.num_texels;
	int weight_count	= d.num_weights;

	// perform a shortcut in the case of a complete decimation table
	if (num_texels == weight_count) {
		for (int i = 0; i < d.num_texels; i++) {
			int texel		= d.texels[i].w[0];
			weight_set[i]	= weights[texel];
			weights[i]		= weight_error_scale[texel];
		}
		return;
	}

	// if the shortcut is not available, we will instead compute a simple estimate and perform three rounds of refinement on that estimate.
	float infilled_weights[ASTC::MAX_TEXELS_PER_BLOCK];

	// compute an initial average for each weight.
	for (int i = 0; i < weight_count; i++) {
		int		texel_count		= d.weights[i].num_texels;
		float	weight_weight	= 1e-10f;	// to avoid 0/0 later on
		float	initial_weight	= 0;
		for (int j = 0; j < texel_count; j++) {
			int		texel	= d.weights[i].texel[j];
			float	weight	= d.weights[i].wf[j];
			float	contrib	= weight * weight_error_scale[texel];
			weight_weight	+= contrib;
			initial_weight	+= weights[texel] * contrib;
		}

		weights[i]		= weight_weight;
		weight_set[i]	= initial_weight / weight_weight;
	}

	for (int i = 0; i < num_texels; i++)
		infilled_weights[i] = d.texels[i].compute(weight_set);

	const float stepsizes[3] ={ 0.25f, 0.125f, 0.0625f };

	for (int j = 0; j < 2; j++) {
		float stepsize = stepsizes[j];

		for (int i = 0; i < weight_count; i++) {
			float weight_val	= weight_set[i];
			float error_change_up, error_change_down;
			CalcTwoErrorChanges(d, infilled_weights, i, stepsize, -stepsize, &error_change_up, &error_change_down);

			/*
				assume that the error-change function behaves like a quadratic function in the interval examined,
				with "error_change_up" and "error_change_down" defining the function at the endpoints
				of the interval. Then, find the position where the function's derivative is zero.

				The "abs(b) >= a" check tests several conditions in one:
				if a is negative, then the 2nd derivative fo the function is negative;
				in this case, f'(x)=0 will maximize error.
				If abs(b) > abs(a), then f'(x)=0 will lie outside the interval altogether.
				If a and b are both 0, then set step to 0;
				otherwise, we end up computing 0/0, which produces a lethal NaN.
				We can get an a=b=0 situation if an error weight is 0 in the wrong place.
				*/

			float step	= 0;
			float a		= (error_change_up + error_change_down) * 2.0f;
			float b		= error_change_down - error_change_up;
			if (abs(b) >= a) {
				if (a <= 0.0f) {
					if (error_change_up < error_change_down)
						step = 1;
					else if (error_change_up > error_change_down)
						step = -1;

				} else {
					if (a < 1e-10f)
						a = 1e-10f;
					step = b / a;
					if (step < -1.0f)
						step = -1.0f;
					else if (step > 1.0f)
						step = 1.0f;
				}
			} else {
				step = b / a;
			}

			step *= stepsize;
			float new_weight_val = weight_val + step;

			// update the weight
			weight_set[i] = new_weight_val;
			// update the infilled-weights
			int			num_weights		= d.weights[i].num_texels;
			float		perturbation	= (new_weight_val - weight_val) / 16;
			const uint8 *texel			= d.weights[i].texel;
			const float *weights		= d.weights[i].wf;
			for (int k = num_weights - 1; k >= 0; k--)
				infilled_weights[texel[k]] += perturbation *  weights[k];
		}
	}
}

/*
	For a decimation table, try to compute an optimal weight set, assuming
	that the weights are quantized and subject to a transfer function.

	We do this as follows:
	First, we take the initial weights and quantize them. This is our initial estimate.
	Then, go through the weights one by one; try to perturb then up and down one weight at a
	time; apply any perturbations that improve overall error
	Repeat until we have made a complete processing pass over all weights without
	triggering any perturbations *OR* we have run 4 full passes.
	*/

void EndpointsWeights::ComputeIdealWeights(const DecimationTable &d, float low_bound, float high_bound, const float *weight_set_in, float *weight_set_out, uint8 *quantized_weight_set, Quant qmode) {
	int weight_count	= d.num_weights;
	int num_texels		= d.num_texels;

	// quantize the weight set using both the specified low/high bounds and the standard 0..1 weight bounds.

	if (high_bound - low_bound < 0.5f) {
		low_bound	= 0.0f;
		high_bound	= 1.0f;
	}

	float rscale	= high_bound - low_bound;
	float scale		= 1.0f / rscale;

	// rescale the weights so that
	// low_bound -> 0
	// high_bound -> 1

	for (int i = 0; i < weight_count; i++)
		weight_set_out[i] = (weight_set_in[i] - low_bound) * scale;

	static const float quantization_step_table[12] ={
		1.0f / 1.0f,
		1.0f / 2.0f,
		1.0f / 3.0f,
		1.0f / 4.0f,
		1.0f / 5.0f,
		1.0f / 7.0f,
		1.0f / 9.0f,
		1.0f / 11.0f,
		1.0f / 15.0f,
		1.0f / 19.0f,
		1.0f / 23.0f,
		1.0f / 31.0f,
	};

	float quantization_cutoff = quantization_step_table[qmode] * 0.333f;

	bool	is_perturbable[ASTC::MAX_WEIGHTS_PER_BLOCK];
	int		perturbable_count = 0;

	// quantize the weight set
	for (int i = 0; i < weight_count; i++) {
		float	ix0		= clamp(weight_set_out[i], 0, 1);
		float	ix		= ix0 * 1024;
		int		ix2		= (int)floor(ix + 0.5f);
		int		weight	= ClosestQuant1024(qmode, ix2);

		ix						= UnQuantFloat(qmode, weight);
		weight_set_out[i]		= ix;
		quantized_weight_set[i] = weight;

		// test whether the error of the weight is greater than 1/3 of the weight spacing;
		// if it is not, then it is flagged as "not perturbable". This causes a quality loss of about 0.002 dB, which is totally worth the speedup we're getting.
		if (is_perturbable[i] = abs(ix - ix0) > quantization_cutoff)
			perturbable_count++;
	}


	// if the decimation table is complete, the quantization above was all we needed to do, so we can early-out.
	if (d.num_weights == d.num_texels) {
		for (int i = 0; i < weight_count; i++)
			weight_set_out[i] = (weight_set_out[i] * rscale) + low_bound;
		return;
	}

	int weights_tested = 0;

	// if no weights are flagged as perturbable, don't try to perturb them.
	// if only one weight is flagged as perturbable, perturbation is also pointless.
	if (perturbable_count > 1) {
		EndpointsWeights eaix;
		for (int i = 0; i < num_texels; i++) {
			eaix.weights[i]				= (weights[i] - low_bound) * scale;
			eaix.weight_error_scale[i]	= weight_error_scale[i];
		}

		float infilled_weights[ASTC::MAX_TEXELS_PER_BLOCK];
		for (int i = 0; i < num_texels; i++)
			infilled_weights[i] = d.texels[i].compute(weight_set_out);

		int		weight_to_perturb	= 0;
		int		weights_since_last_perturbation = 0;
		int		num_weights			= d.num_weights;

		while (weights_since_last_perturbation < num_weights && weights_tested < num_weights * 4) {
			bool do_quant_mod = false;
			if (is_perturbable[weight_to_perturb]) {
				int		weight_val				= quantized_weight_set[weight_to_perturb];
				int		weight_next_up			= weight_val + 1;
				int		weight_next_down		= weight_val - 1;
				float	flt_weight_val			= UnQuantFloat(qmode, weight_val);
				float	flt_weight_next_up		= UnQuantFloat(qmode, weight_next_up);
				float	flt_weight_next_down	= UnQuantFloat(qmode, weight_next_down);

				float error_change_up, error_change_down;

				// compute the error change from perturbing the weight either up or down.
				eaix.CalcTwoErrorChanges(
					d,
					infilled_weights,
					weight_to_perturb,
					(flt_weight_next_up - flt_weight_val), (flt_weight_next_down - flt_weight_val), &error_change_up, &error_change_down
				);

				int		new_weight_val;
				float	flt_new_weight_val;
				if (weight_val != weight_next_up && error_change_up < 0.0f) {
					do_quant_mod		= true;
					new_weight_val		= weight_next_up;
					flt_new_weight_val	= flt_weight_next_up;
				} else if (weight_val != weight_next_down && error_change_down < 0.0f) {
					do_quant_mod		= true;
					new_weight_val		= weight_next_down;
					flt_new_weight_val	= flt_weight_next_down;
				}

				if (do_quant_mod) {
					// update the weight.
					weight_set_out[weight_to_perturb]		= flt_new_weight_val;
					quantized_weight_set[weight_to_perturb] = new_weight_val;

					// update the infilled-weights
					int		num_weights		= d.weights[weight_to_perturb].num_texels;
					float	perturbation	= (flt_new_weight_val - flt_weight_val) / 16;
					const uint8 *texel		= d.weights[weight_to_perturb].texel;
					const float *weights	= d.weights[weight_to_perturb].wf;
					for (int i = num_weights - 1; i >= 0; i--)
						infilled_weights[texel[i]] += perturbation * weights[i];
				}
			}

			if (do_quant_mod)
				weights_since_last_perturbation = 0;
			else
				weights_since_last_perturbation++;

			weight_to_perturb++;
			if (weight_to_perturb >= num_weights)
				weight_to_perturb -= num_weights;

			weights_tested++;
		}
	}

	for (int i = 0; i < weight_count; i++)
		weight_set_out[i] = (weight_set_out[i] * rscale) + low_bound;
}

//  for a given weight set, we wish to recompute the colors so that they are optimal for a particular weight set
void Endpoints::RecomputeIdeal(const ImageStats &block, const PartitionTable &pt, const DecimationTable &dt, Quant weight_qmode,
	float4		*rgbs_vectors,			// used to return RGBS-vectors. (endpoint mode #6)
	float4		*rgbo_vectors,			// used to return RGBO-vectors. (endpoint mode #7)
	float2		*lum_vectors,			// used to return luminance-vectors.
	const uint8 *weight_set8,			// the current set of weight values
	const uint8 *plane2_weight_set8,	// NULL if plane 2 is not actually used.
	int plane2_color_component,			// color component for 2nd plane of weights; -1 if the 2nd plane of weights is not present
	const ErrorWeightBlock *ewb
) {
	int		num_texels = block.num_texels;
	float	weight_set[ASTC::MAX_WEIGHTS_PER_BLOCK];
	float	plane2_weight_set[ASTC::MAX_WEIGHTS_PER_BLOCK];

	for (int i = 0; i < dt.num_weights; i++)
		weight_set[i] = UnQuantFloat(weight_qmode, weight_set8[i]);

	if (plane2_weight_set8) {
		for (int i = 0; i < dt.num_weights; i++)
			plane2_weight_set[i] = UnQuantFloat(weight_qmode, plane2_weight_set8[i]);
	}

	int num_partitions = pt.num_partitions;

	float4		pmat1_red[4], pmat1_green[4], pmat1_blue[4], pmat1_alpha[4], pmat1_lum[4], pmat1_scale[4];	// matrices for plane of weights 1
	float4		pmat2_red[4], pmat2_green[4], pmat2_blue[4], pmat2_alpha[4];	// matrices for plane of weights 2
	float2		red_vec[4];
	float2		green_vec[4];
	float2		blue_vec[4];
	float2		alpha_vec[4];
	float2		lum_vec[4];
	float2		scale_vec[4];
	float		wmin1[4], wmax1[4];
	float		wmin2[4], wmax2[4];
	float4		weight_sum[4];
	float		lum_weight_sum[4];
	float		scale_weight_sum[4];
	float		red_weight_weight_sum[4];
	float		green_weight_weight_sum[4];
	float		blue_weight_weight_sum[4];
	float		psum[4];				// sum of (weight * qweight^2) across (red,green,blue)
	float		qsum[4];				// sum of (weight * qweight * texelval) across (red,green,blue)
	float3		rgb_sum[4];
	float3		rgb_weight_sum[4];
	float3		scale_directions[4];
	float		scale_min[4];
	float		scale_max[4];
	float		lum_min[4];
	float		lum_max[4];

	clear(pmat1_red);
	clear(pmat2_red);
	clear(pmat1_green);
	clear(pmat2_green);
	clear(pmat1_blue);
	clear(pmat2_blue);
	clear(pmat1_alpha);
	clear(pmat2_alpha);
	clear(pmat1_lum);
	clear(pmat1_scale);
	clear(red_vec);
	clear(green_vec);
	clear(blue_vec);
	clear(alpha_vec);
	clear(lum_vec);
	clear(scale_vec);
	clear(rgb_sum);
	clear(rgb_weight_sum);

	for (int i = 0; i < num_texels; i++) {
		float3	rgb			= block.texel[i].xyz;
		float3	rgb_weight	= ewb->v[i].xyz;
		int		part		= pt.partition[i];
		rgb_sum[part]		+= rgb * rgb_weight;
		rgb_weight_sum[part]+= rgb_weight;
	}

	for (int i = 0; i < num_partitions; i++) {
		wmin1[i]			= 1.0f;
		wmax1[i]			= 0.0f;
		wmin2[i]			= 1.0f;
		wmax2[i]			= 0.0f;

		weight_sum[i]		= float4(1e-17f);
		lum_weight_sum[i]	= 1e-17f;
		scale_weight_sum[i] = 1e-17f;

		red_weight_weight_sum[i] = 1e-17f;
		green_weight_weight_sum[i] = 1e-17f;
		blue_weight_weight_sum[i] = 1e-17f;

		psum[i]				= 1e-17f;
		qsum[i]				= 1e-17f;

		scale_directions[i] = normalise((rgb_sum[i] + 1e-17f) / (rgb_weight_sum[i] + 1e-17f));
		scale_max[i]		= 0.0f;
		scale_min[i]		= 1e10f;
		lum_max[i]			= 0.0f;
		lum_min[i]			= 1e10f;
	}

	for (int i = 0; i < num_texels; i++) {
		float4	c		= block.texel[i];
		int		part	= pt.partition[i];
		float	idx0	= dt.texels[i].compute(weight_set);
		float	om_idx0 = 1.0f - idx0;

		if (idx0 > wmax1[part])
			wmax1[part] = idx0;
		if (idx0 < wmin1[part])
			wmin1[part] = idx0;

		float4	weight		= ewb->v[i];

		float	lum_weight		= (weight.x + weight.y + weight.z);
		float	scale_weight	= lum_weight;
		float	lum				= dot(c.xyz, weight.xyz) / lum_weight;
		float3	scale_direction = scale_directions[part];
		float	scale			= dot(scale_direction, c.xyz);
		if (lum < lum_min[part])
			lum_min[part] = scale;
		if (lum > lum_max[part])
			lum_max[part] = scale;
		if (scale < scale_min[part])
			scale_min[part] = scale;
		if (scale > scale_max[part])
			scale_max[part] = scale;


		weight_sum[part]		+= weight;
		lum_weight_sum[part]	+= lum_weight;
		scale_weight_sum[part]	+= scale_weight;

		float4	m0{om_idx0 * om_idx0, idx0 * om_idx0, idx0 * om_idx0, idx0 * idx0};
		pmat1_red[part]		+= m0 * weight.x;
		pmat1_green[part]	+= m0 * weight.y;
		pmat1_blue[part]	+= m0 * weight.z;
		pmat1_alpha[part]	+= m0 * weight.w;
		pmat1_lum[part]		+= m0 * lum_weight;
		pmat1_scale[part]	+= m0 * scale_weight;

		float idx1 = 0.0f, om_idx1 = 0.0f;
		if (plane2_weight_set8) {
			idx1 = dt.texels[i].compute(plane2_weight_set);
			om_idx1 = 1.0f - idx1;
			if (idx1 > wmax2[part])
				wmax2[part] = idx1;
			if (idx1 < wmin2[part])
				wmin2[part] = idx1;

			float4	m1{om_idx1 * om_idx1, idx1 * om_idx1, idx1 * om_idx1, idx1 * idx1};
			pmat1_red[part]		+= m1 * weight.x;
			pmat1_green[part]	+= m1 * weight.y;
			pmat1_blue[part]	+= m1 * weight.z;
			pmat1_alpha[part]	+= m1 * weight.w;
		}

		float red_idx	= plane2_color_component == 0 ? idx1 : idx0;
		float green_idx = plane2_color_component == 1 ? idx1 : idx0;
		float blue_idx	= plane2_color_component == 2 ? idx1 : idx0;
		float alpha_idx = plane2_color_component == 3 ? idx1 : idx0;


		red_vec[part].x		+= (weight.x * c.x) * (1 - red_idx);
		green_vec[part].x	+= (weight.y * c.y) * (1 - green_idx);
		blue_vec[part].x	+= (weight.z * c.z) * (1 - blue_idx);
		alpha_vec[part].x	+= (weight.w * c.w) * (1 - alpha_idx);
		lum_vec[part].x		+= (lum_weight * lum) * om_idx0;
		scale_vec[part].x	+= (scale_weight * scale) * om_idx0;

		red_vec[part].y		+= (weight.x * c.x) * red_idx;
		green_vec[part].y	+= (weight.y * c.y) * green_idx;
		blue_vec[part].y	+= (weight.z * c.z) * blue_idx;
		alpha_vec[part].y	+= (weight.w * c.w) * alpha_idx;
		lum_vec[part].y		+= (lum_weight * lum) * idx0;
		scale_vec[part].y	+= (scale_weight * scale) * idx0;

		red_weight_weight_sum[part]		+= weight.x * red_idx;
		green_weight_weight_sum[part]	+= weight.y * green_idx;
		blue_weight_weight_sum[part]	+= weight.z * blue_idx;

		psum[part] += dot(weight.xyz, square(float3{red_idx, green_idx, blue_idx}));

	}

	// calculations specific to mode #7, the HDR RGB-scale mode.
	float red_sum[4];
	float green_sum[4];
	float blue_sum[4];
	for (int i = 0; i < num_partitions; i++) {
		red_sum[i]		= red_vec[i].x + red_vec[i].y;
		green_sum[i]	= green_vec[i].x + green_vec[i].y;
		blue_sum[i]		= blue_vec[i].x + blue_vec[i].y;
		qsum[i]			= red_vec[i].y + green_vec[i].y + blue_vec[i].y;
	}

	// rgb+offset for HDR endpoint mode #7
	int rgbo_fail[4];
	for (int i = 0; i < num_partitions; i++) {
		float4x4 mod7_mat(
			float4{weight_sum[i].x, 0.0f, 0.0f, red_weight_weight_sum[i]},
			float4{0.0f, weight_sum[i].y, 0.0f, green_weight_weight_sum[i]},
			float4{0.0f, 0.0f, weight_sum[i].z, blue_weight_weight_sum[i]},
			float4{red_weight_weight_sum[i], green_weight_weight_sum[i], blue_weight_weight_sum[i], psum[i]}
		);

		rgbo_vectors[i] = float4{red_sum[i], green_sum[i], blue_sum[i], qsum[i]} / mod7_mat;

		// we will occasionally get a failure due to a singluar matrix. Record whether such a failure has taken place; if it did, compute rgbo_vectors[] with a different method later on.
		float chkval = len2(rgbo_vectors[i]);
		rgbo_fail[i] = chkval != chkval;
	}

	// initialize the luminance and scale vectors with a reasonable default, just in case the subsequent calculation blows up.
	for (int i = 0; i < num_partitions; i++) {
		float scalediv = clamp(scale_min[i] / scale_max[i], 0, 1);
		rgbs_vectors[i] = concat(scale_directions[i] * scale_max[i], scalediv);
		lum_vectors[i]	= float2{lum_min[i], lum_max[i]};
	}

	for (int i = 0; i < num_partitions; i++) {
		if (wmin1[i] >= wmax1[i] * 0.999) {
			// if all weights in the partition were equal, then just take average of all colors in the partition and use that as both endpoint colors.
			float4 avg = float4{
				red_vec[i].x	+ red_vec[i].y,
				green_vec[i].x	+ green_vec[i].y,
				blue_vec[i].x	+ blue_vec[i].y,
				alpha_vec[i].x	+ alpha_vec[i].y
			} / weight_sum[i];

			if (plane2_color_component != 0 && avg.x == avg.x)
				endpt0[i].x = endpt1[i].x = avg.x;
			if (plane2_color_component != 1 && avg.y == avg.y)
				endpt0[i].y = endpt1[i].y = avg.y;
			if (plane2_color_component != 2 && avg.z == avg.z)
				endpt0[i].z = endpt1[i].z = avg.z;
			if (plane2_color_component != 3 && avg.w == avg.w)
				endpt0[i].w = endpt1[i].w = avg.w;

			rgbs_vectors[i] = concat(scale_directions[i] * scale_max[i], 1.0f);
			float lumval = (red_vec[i].x + red_vec[i].y + green_vec[i].x + green_vec[i].y + blue_vec[i].x + blue_vec[i].y) / (weight_sum[i].x + weight_sum[i].y + weight_sum[i].z);
			lum_vectors[i] = float2(lumval);
		} else {
			// otherwise, complete the analytic calculation of ideal-endpoint-values for the given set of texel weigths and pixel colors.
			float red_det1		= float2x2(pmat1_red[i]).det();
			float green_det1	= float2x2(pmat1_green[i]).det();
			float blue_det1		= float2x2(pmat1_blue[i]).det();
			float alpha_det1	= float2x2(pmat1_alpha[i]).det();
			float lum_det1		= float2x2(pmat1_lum[i]).det();
			float scale_det1	= float2x2(pmat1_scale[i]).det();

			float red_mss1		= len2(pmat1_red[i]);
			float green_mss1	= len2(pmat1_green[i]);
			float blue_mss1		= len2(pmat1_blue[i]);
			float alpha_mss1	= len2(pmat1_alpha[i]);
			float lum_mss1		= len2(pmat1_lum[i]);
			float scale_mss1	= len2(pmat1_scale[i]);

			float2x2	ipmat1_red		= inverse(float2x2(pmat1_red[i]));
			float2x2	ipmat1_green	= inverse(float2x2(pmat1_green[i]));
			float2x2	ipmat1_blue		= inverse(float2x2(pmat1_blue[i]));
			float2x2	ipmat1_alpha	= inverse(float2x2(pmat1_alpha[i]));
			float2x2	ipmat1_lum		= inverse(float2x2(pmat1_lum[i]));
			float2x2	ipmat1_scale	= inverse(float2x2(pmat1_scale[i]));

			float4 ep0 = {
				dot(ipmat1_red.x,	red_vec[i]),
				dot(ipmat1_green.x, green_vec[i]),
				dot(ipmat1_blue.x,	blue_vec[i]),
				dot(ipmat1_alpha.x, alpha_vec[i])
			};

			float4 ep1 = {
				dot(ipmat1_red.y,	red_vec[i]),
				dot(ipmat1_green.y, green_vec[i]),
				dot(ipmat1_blue.y,	blue_vec[i]),
				dot(ipmat1_alpha.y, alpha_vec[i])
			};

			float lum_ep0	= dot(ipmat1_lum.x, lum_vec[i]);
			float lum_ep1	= dot(ipmat1_lum.y, lum_vec[i]);
			float scale_ep0 = dot(ipmat1_scale.x, scale_vec[i]);
			float scale_ep1 = dot(ipmat1_scale.y, scale_vec[i]);

			if (plane2_color_component != 0 && abs(red_det1) > (red_mss1 * 1e-4f) && ep0.x == ep0.x && ep1.x == ep1.x) {
				endpt0[i].x = ep0.x;
				endpt1[i].x = ep1.x;
			}
			if (plane2_color_component != 1 && abs(green_det1) > (green_mss1 * 1e-4f) && ep0.y == ep0.y && ep1.y == ep1.y) {
				endpt0[i].y = ep0.y;
				endpt1[i].y = ep1.y;
			}
			if (plane2_color_component != 2 && abs(blue_det1) > (blue_mss1 * 1e-4f) && ep0.z == ep0.z && ep1.z == ep1.z) {
				endpt0[i].z = ep0.z;
				endpt1[i].z = ep1.z;
			}
			if (plane2_color_component != 3 && abs(alpha_det1) > (alpha_mss1 * 1e-4f) && ep0.w == ep0.w && ep1.w == ep1.w) {
				endpt0[i].w = ep0.w;
				endpt1[i].w = ep1.w;
			}

			if (abs(lum_det1) > (lum_mss1 * 1e-4f) && lum_ep0 == lum_ep0 && lum_ep1 == lum_ep1 && lum_ep0 < lum_ep1) {
				lum_vectors[i].x = lum_ep0;
				lum_vectors[i].y = lum_ep1;
			}
			if (abs(scale_det1) >(scale_mss1 * 1e-4f) && scale_ep0 == scale_ep0 && scale_ep1 == scale_ep1 && scale_ep0 < scale_ep1) {
				float scalediv = scale_ep0 / scale_ep1;
				rgbs_vectors[i] = concat(scale_directions[i] * scale_ep1, scalediv);
			}
		}

		if (plane2_weight_set8) {
			if (wmin2[i] >= wmax2[i] * 0.999) {
				// if all weights in the partition were equal, then just take average
				// of all colors in the partition and use that as both endpoint colors.
				float4 avg = float4{
					red_vec[i].x	+ red_vec[i].y,
					green_vec[i].x	+ green_vec[i].y,
					blue_vec[i].x	+ blue_vec[i].y,
					alpha_vec[i].x	+ alpha_vec[i].y
				} / weight_sum[i];

				if (plane2_color_component == 0 && avg.x == avg.x)
					endpt0[i].x = endpt1[i].x = avg.x;
				if (plane2_color_component == 1 && avg.y == avg.y)
					endpt0[i].y = endpt1[i].y = avg.y;
				if (plane2_color_component == 2 && avg.z == avg.z)
					endpt0[i].z = endpt1[i].z = avg.z;
				if (plane2_color_component == 3 && avg.w == avg.w)
					endpt0[i].w = endpt1[i].w = avg.w;
			} else {

				// otherwise, complete the analytic calculation of ideal-endpoint-values for the given set of texel weigths and pixel colors.
				float red_det2		= float2x2(pmat2_red[i]).det();
				float green_det2	= float2x2(pmat2_green[i]).det();
				float blue_det2		= float2x2(pmat2_blue[i]).det();
				float alpha_det2	= float2x2(pmat2_alpha[i]).det();

				float red_mss2		= len2(pmat2_red[i]);
				float green_mss2	= len2(pmat2_green[i]);
				float blue_mss2		= len2(pmat2_blue[i]);
				float alpha_mss2	= len2(pmat2_alpha[i]);

				float2x2	ipmat2_red		= inverse(float2x2(pmat2_red[i]));
				float2x2	ipmat2_green	= inverse(float2x2(pmat2_green[i]));
				float2x2	ipmat2_blue		= inverse(float2x2(pmat2_blue[i]));
				float2x2	ipmat2_alpha	= inverse(float2x2(pmat2_alpha[i]));
				float4 ep0 = {
					dot(ipmat2_red.x, red_vec[i]),
					dot(ipmat2_green.x, green_vec[i]),
					dot(ipmat2_blue.x, blue_vec[i]),
					dot(ipmat2_alpha.x, alpha_vec[i])
				};
				float4 ep1 = {
					dot(ipmat2_red.y, red_vec[i]),
					dot(ipmat2_green.y, green_vec[i]),
					dot(ipmat2_blue.y, blue_vec[i]),
					dot(ipmat2_alpha.y, alpha_vec[i])
				};

				if (plane2_color_component == 0 && abs(red_det2) > (red_mss2 * 1e-4f) && ep0.x == ep0.x && ep1.x == ep1.x) {
					endpt0[i].x = ep0.x;
					endpt1[i].x = ep1.x;
				}
				if (plane2_color_component == 1 && abs(green_det2) > (green_mss2 * 1e-4f) && ep0.y == ep0.y && ep1.y == ep1.y) {
					endpt0[i].y = ep0.y;
					endpt1[i].y = ep1.y;
				}
				if (plane2_color_component == 2 && abs(blue_det2) > (blue_mss2 * 1e-4f) && ep0.z == ep0.z && ep1.z == ep1.z) {
					endpt0[i].z = ep0.z;
					endpt1[i].z = ep1.z;
				}
				if (plane2_color_component == 3 && abs(alpha_det2) > (alpha_mss2 * 1e-4f) && ep0.w == ep0.w && ep1.w == ep1.w) {
					endpt0[i].w = ep0.w;
					endpt1[i].w = ep1.w;
				}
			}
		}
	}

	// if the calculation of an RGB-offset vector failed, try to compute a somewhat-sensible value anyway
	for (int i = 0; i < num_partitions; i++) {
		if (rgbo_fail[i]) {
			float4	v0		= endpt0[i];
			float4	v1		= endpt1[i];
			float	avgdif	= max(dot(v1.xyz - v0.xyz, float3(one)) * (1.0f / 3.0f), 0);
			float4	avg		= (v0 + v1) * 0.5f;
			float4	ep0		= avg - float4(avgdif) * 0.5f;

			rgbo_vectors[i] = concat(ep0.xyz, avgdif);
		}
	}
}

//-----------------------------------------------------------------------------
//	Angle Endpoints
//-----------------------------------------------------------------------------

static const float angular_steppings[] ={
	1.0, 1.125,
	1.25, 1.375,
	1.5, 1.625,
	1.75, 1.875,

	2.0, 2.25, 2.5, 2.75,
	3.0, 3.25, 3.5, 3.75,
	4.0, 4.25, 4.5, 4.75,
	5.0, 5.25, 5.5, 5.75,
	6.0, 6.25, 6.5, 6.75,
	7.0, 7.25, 7.5, 7.75,

	8.0, 8.5,
	9.0, 9.5,
	10.0, 10.5,
	11.0, 11.5,
	12.0, 12.5,
	13.0, 13.5,
	14.0, 14.5,
	15.0, 15.5,
	16.0, 16.5,
	17.0, 17.5,
	18.0, 18.5,
	19.0, 19.5,
	20.0, 20.5,
	21.0, 21.5,
	22.0, 22.5,
	23.0, 23.5,
	24.0, 24.5,
	25.0, 25.5,
	26.0, 26.5,
	27.0, 27.5,
	28.0, 28.5,
	29.0, 29.5,
	30.0, 30.5,
	31.0, 31.5,
	32.0, 32.5,
	33.0, 33.5,
	34.0, 34.5,
	35.0, 35.5,
};

#define ANGULAR_STEPS ((int)(sizeof(angular_steppings)/sizeof(angular_steppings[0])))
#define SINCOS_STEPS 64

static float	stepsizes[ANGULAR_STEPS];
static float	stepsizes_sqr[ANGULAR_STEPS];
static int		max_angular_steps_needed_for_quant_level[13];
static float	sin_table[SINCOS_STEPS][ANGULAR_STEPS];
static float	cos_table[SINCOS_STEPS][ANGULAR_STEPS];

void prepare_angular_tables(void) {
	int max_angular_steps_needed_for_quant_steps[40];
	for (int i = 0; i < ANGULAR_STEPS; i++) {
		stepsizes[i]		= 1.0f / angular_steppings[i];
		stepsizes_sqr[i]	= stepsizes[i] * stepsizes[i];

		for (int j = 0; j < SINCOS_STEPS; j++) {
			sin_table[j][i] = static_cast <float>(sin((2.0f * pi / (SINCOS_STEPS - 1)) * angular_steppings[i] * j));
			cos_table[j][i] = static_cast <float>(cos((2.0f * pi / (SINCOS_STEPS - 1)) * angular_steppings[i] * j));
		}
		max_angular_steps_needed_for_quant_steps[int(floor(angular_steppings[i])) + 1] = min(i + 1, ANGULAR_STEPS - 1);
	}


	// yes, the next-to-last entry is supposed to have the value 33. This because under
	// ASTC, the the 32-weight mode leaves a double-sized hole in the middle of the
	// weight space, so we are better off matching 33 weights than 32.
	static const int steps_of_level[] ={ 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 33, 36 };

	for (int i = 0; i < 13; i++)
		max_angular_steps_needed_for_quant_level[i] = max_angular_steps_needed_for_quant_steps[steps_of_level[i]];

}

// function to compute angular sums; then, from the angular sums, compute alignment factor and offset.
void compute_angular_offsets(int samplecount, const float *samples, const float *sample_weights, int max_angular_steps, float *offsets) {
	float anglesum_x[ANGULAR_STEPS];
	float anglesum_y[ANGULAR_STEPS];

	for (int i = 0; i < max_angular_steps; i++) {
		anglesum_x[i] = 0;
		anglesum_y[i] = 0;
	}

	// compute the angle-sums.
	for (int i = 0; i < samplecount; i++) {
		float	sample_weight	= sample_weights[i];
		uint32	isample			= iorf((samples[i] * (SINCOS_STEPS - 1.0f)) + 12582912.0f).i() & 0x3F;

		const float *sinptr = sin_table[isample];
		const float *cosptr = cos_table[isample];
		for (int j = 0; j < max_angular_steps; j++) {
			anglesum_x[j] += cosptr[j] * sample_weight;
			anglesum_y[j] += sinptr[j] * sample_weight;
		}
	}

	// postprocess the angle-sums
	for (int i = 0; i < max_angular_steps; i++)
		offsets[i] = atan2(anglesum_y[i], anglesum_x[i]) * stepsizes[i] / (pi * 2);
}

// for a given step-size and a given offset, compute the lowest and highest weight that results from quantizing using the stepsize & offset.
void compute_lowest_and_highest_weight(int samplecount, const float *samples, const float *sample_weights,
	int max_angular_steps, const float *offsets,
	int8 *lowest_weight, int8 *highest_weight,
	float *error, float *cut_low_weight_error, float *cut_high_weight_error
) {
	float error_from_forcing_weight_down[60];
	float error_from_forcing_weight_either_way[60];

	clear(error_from_forcing_weight_down);
	clear(error_from_forcing_weight_either_way);

	// weight + 12
	static const unsigned int idxtab[256] ={
		12, 13, 14, 15, 16, 17, 18, 19,
		20, 21, 22, 23, 24, 25, 26, 27,
		28, 29, 30, 31, 32, 33, 34, 35,
		36, 37, 38, 39, 40, 41, 42, 43,
		44, 45, 46, 47, 48, 49, 50, 51,
		52, 53, 54, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 55, 55, 55, 55,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 1, 2, 3,
		4, 5, 6, 7, 8, 9, 10, 11,

		12, 13, 14, 15, 16, 17, 18, 19,
		20, 21, 22, 23, 24, 25, 26, 27,
		28, 29, 30, 31, 32, 33, 34, 35,
		36, 37, 38, 39, 40, 41, 42, 43,
		44, 45, 46, 47, 48, 49, 50, 51,
		52, 53, 54, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 55, 55, 55, 55,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 1, 2, 3,
		4, 5, 6, 7, 8, 9, 10, 11
	};

	for (int sp = 0; sp < max_angular_steps; sp++) {
		unsigned int minidx_bias12 = 55;
		unsigned int maxidx_bias12 = 0;

		float errval = 0.0f;

		float rcp_stepsize = angular_steppings[sp];
		float offset = offsets[sp];

		float scaled_offset = rcp_stepsize * offset;


		for (int i = 0; i < samplecount - 1; i += 2) {
			float	wt1		= sample_weights[i];
			float	wt2		= sample_weights[i + 1];
			float	sval1	= (samples[i] * rcp_stepsize) - scaled_offset;
			float	sval2	= (samples[i + 1] * rcp_stepsize) - scaled_offset;
			iorf	p1		= sval1 + 12582912.0f;
			iorf	p2		= sval2 + 12582912.0f;
			float	isval1	= p1.f() - 12582912.0f;
			float	isval2	= p2.f() - 12582912.0f;
			float	dif1	= sval1 - isval1;
			float	dif2	= sval2 - isval2;

			errval += square(dif1) * wt1;
			errval += square(dif2) * wt2;

			// table lookups that really perform a minmax function.
			uint32 idx1_bias12 = idxtab[p1.i() & 0xFF];
			uint32 idx2_bias12 = idxtab[p2.i() & 0xFF];

			if (idx1_bias12 < minidx_bias12)
				minidx_bias12 = idx1_bias12;
			if (idx1_bias12 > maxidx_bias12)
				maxidx_bias12 = idx1_bias12;
			if (idx2_bias12 < minidx_bias12)
				minidx_bias12 = idx2_bias12;
			if (idx2_bias12 > maxidx_bias12)
				maxidx_bias12 = idx2_bias12;

			error_from_forcing_weight_either_way[idx1_bias12]	+= wt1;
			error_from_forcing_weight_down[idx1_bias12]			+= dif1 * wt1;

			error_from_forcing_weight_either_way[idx2_bias12]	+= wt2;
			error_from_forcing_weight_down[idx2_bias12]			+= dif2 * wt2;
		}

		if (samplecount & 1) {
			int		i		= samplecount - 1;
			float	wt		= sample_weights[i];
			float	sval	= (samples[i] * rcp_stepsize) - scaled_offset;
			iorf	p		= sval + 12582912.0f;
			float	isval	= p.f() - 12582912.0f;
			float	dif		= sval - isval;

			errval += square(dif) * wt;

			uint32 idx_bias12 = idxtab[p.i() & 0xFF];

			if (idx_bias12 < minidx_bias12)
				minidx_bias12 = idx_bias12;
			if (idx_bias12 > maxidx_bias12)
				maxidx_bias12 = idx_bias12;

			error_from_forcing_weight_either_way[idx_bias12]	+= wt;
			error_from_forcing_weight_down[idx_bias12]			+= dif * wt;
		}


		lowest_weight[sp] = (int)minidx_bias12 - 12;
		highest_weight[sp] = (int)maxidx_bias12 - 12;
		error[sp] = errval;

		// the cut_(lowest/highest)_weight_error indicate the error that results from forcing samples that should have had the (lowest/highest) weight value one step (up/down).
		cut_low_weight_error[sp]	= error_from_forcing_weight_either_way[minidx_bias12] - 2 * error_from_forcing_weight_down[minidx_bias12];
		cut_high_weight_error[sp]	= error_from_forcing_weight_either_way[maxidx_bias12] + 2 * error_from_forcing_weight_down[maxidx_bias12];

		// clear out the error-from-forcing values we actually used in this pass
		// so that these are clean for the next pass.
		for (uint32 ui = minidx_bias12 & ~0x3; ui <= maxidx_bias12; ui += 4) {
			error_from_forcing_weight_either_way[ui]		= 0;
			error_from_forcing_weight_down[ui]				= 0;
			error_from_forcing_weight_either_way[ui + 1]	= 0;
			error_from_forcing_weight_down[ui + 1]			= 0;
			error_from_forcing_weight_either_way[ui + 2]	= 0;
			error_from_forcing_weight_down[ui + 2]			= 0;
			error_from_forcing_weight_either_way[ui + 3]	= 0;
			error_from_forcing_weight_down[ui + 3]			= 0;
		}
	}

	for (int sp = 0; sp < max_angular_steps; sp++) {
		float errscale = stepsizes_sqr[sp];
		error[sp]					*= errscale;
		cut_low_weight_error[sp]	*= errscale;
		cut_high_weight_error[sp]	*= errscale;
	}
}

void compute_angular_endpoints_for_qmodes(int samplecount, const float *samples, const float *sample_weights, Quant max_qmode, float low_value[12], float high_value[12]) {
	static const int quantization_steps_for_level[13] = { 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 33, 36 };
	int max_quantization_steps = quantization_steps_for_level[max_qmode + 1];

	float	offsets[ANGULAR_STEPS];
	int		max_angular_steps = max_angular_steps_needed_for_quant_level[max_qmode + 1];
	compute_angular_offsets(samplecount, samples, sample_weights, max_angular_steps, offsets);


	// the +4 offsets are to allow for vectorization within compute_lowest_and_highest_weight().
	int8	lowest_weight[ANGULAR_STEPS + 4];
	int8	highest_weight[ANGULAR_STEPS + 4];
	float	error[ANGULAR_STEPS + 4];

	float cut_low_weight_error[ANGULAR_STEPS + 4];
	float cut_high_weight_error[ANGULAR_STEPS + 4];

	compute_lowest_and_highest_weight(samplecount, samples, sample_weights, max_angular_steps, offsets, lowest_weight, highest_weight, error, cut_low_weight_error, cut_high_weight_error);


	// for each quantization level, find the best error terms.
	float	best_errors[40];
	int		best_scale[40];
	uint8	cut_low_weight[40];

	for (int i = 0; i < (max_quantization_steps + 4); i++) {
		best_errors[i]		= 1e30f;
		best_scale[i]		= -1;	// Indicates no solution found
		cut_low_weight[i]	= 0;
	}

	for (int i = 0; i < max_angular_steps; i++) {
		int samplecount = highest_weight[i] - lowest_weight[i] + 1;
		if (samplecount >= (max_quantization_steps + 4))
			continue;

		if (samplecount < 2)
			samplecount = 2;

		if (best_errors[samplecount] > error[i]) {
			best_errors[samplecount] = error[i];
			best_scale[samplecount]		= i;
			cut_low_weight[samplecount]	= 0;
		}

		float error_cut_low			= error[i] + cut_low_weight_error[i];
		float error_cut_high		= error[i] + cut_high_weight_error[i];
		float error_cut_low_high	= error[i] + cut_low_weight_error[i] + cut_high_weight_error[i];

		if (best_errors[samplecount - 1] > error_cut_low) {
			best_errors[samplecount - 1] = error_cut_low;
			best_scale[samplecount - 1]		= i;
			cut_low_weight[samplecount - 1] = 1;
		}

		if (best_errors[samplecount - 1] > error_cut_high) {
			best_errors[samplecount - 1] = error_cut_high;
			best_scale[samplecount - 1]		= i;
			cut_low_weight[samplecount - 1] = 0;
		}

		if (best_errors[samplecount - 2] > error_cut_low_high) {
			best_errors[samplecount - 2] = error_cut_low_high;
			best_scale[samplecount - 2]		= i;
			cut_low_weight[samplecount - 2] = 1;
		}

	}

	// if we got a better error-value for a low samplecount than for a high one, use the low-samplecount error value for the higher samplecount as well.
	for (int i = 3; i <= max_quantization_steps; i++) {
		if (best_errors[i] > best_errors[i - 1]) {
			best_errors[i]		= best_errors[i - 1];
			best_scale[i]		= best_scale[i - 1];
			cut_low_weight[i]	= cut_low_weight[i - 1];
		}
	}

	static const int ql_weights[12] = { 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 33 };
	for (int i = 0; i <= max_qmode; i++) {
		int		q			= ql_weights[i];
		int		bsi			= best_scale[q];
		float	stepsize	= stepsizes[bsi];
		int		lwi			= lowest_weight[bsi] + cut_low_weight[q];
		int		hwi			= lwi + q - 1;
		float	offset		= offsets[bsi];

		low_value[i]	= offset + lwi * stepsize;
		high_value[i]	= offset + hwi * stepsize;
	}
}

//-----------------------------------------------------------------------------
//	Functions to pick the best ASTC endpoint format for a given block
//-----------------------------------------------------------------------------

struct EncodingChoiceErrors {
	float	rgb_scale_error;	// error of using LDR RGB-scale instead of complete endpoints.
	float	rgb_luma_error;		// error of using HDR RGB-scale instead of complete endpoints.
	float	luminance_error;	// error of using luminance instead of RGB
	float	alpha_drop_error;	// error of discarding alpha
	float	rgb_drop_error;		// error of discarding rgb
	bool	can_offset_encode;
	bool	can_blue_contract;
};

void CalcEncodingChoiceErrors(EncodingChoiceErrors *eci, const ImageStats &block, const PartitionTable &pt, int separate_component, const ErrorWeightBlock *ewb) {
	int		num_partitions		= pt.num_partitions;
	int		num_texels	= block.num_texels;

	float4 averages[4], directions[4];

	float4 error_weightings[4];
	float4 color_scalefactors[4];
	float4 inverse_color_scalefactors[4];

	ewb->PartitionWeightings(pt, error_weightings, color_scalefactors);
	ewb->Directions(block, pt, color_scalefactors, averages, directions);

	raw_line<3>	uncorr_rgb_lines[4];
	raw_line<3>	samechroma_rgb_lines[4];	// for LDR-RGB-scale
	raw_line<3>	rgb_luma_lines[4];			// for HDR-RGB-scale
	raw_line<3>	luminance_lines[4];

	processed_line<3> proc_uncorr_rgb_lines[4];
	processed_line<3> proc_samechroma_rgb_lines[4];	// for LDR-RGB-scale
	processed_line<3> proc_rgb_luma_lines[4];	// for HDR-RGB-scale
	processed_line<3> proc_luminance_lines[4];


	for (int i = 0; i < num_partitions; i++) {
		inverse_color_scalefactors[i].x = 1.0f / max(color_scalefactors[i].x, 1e-7f);
		inverse_color_scalefactors[i].y = 1.0f / max(color_scalefactors[i].y, 1e-7f);
		inverse_color_scalefactors[i].z = 1.0f / max(color_scalefactors[i].z, 1e-7f);
		inverse_color_scalefactors[i].w = 1.0f / max(color_scalefactors[i].w, 1e-7f);


		uncorr_rgb_lines[i].a = averages[i].xyz;
		uncorr_rgb_lines[i].b = dot(directions[i].xyz, directions[i].xyz) == 0 ? normalise(color_scalefactors[i].xyz) : normalise(directions[i].xyz);

		samechroma_rgb_lines[i].a = float3(zero);
		if (dot(averages[i], averages[i]) < 1e-20f)
			samechroma_rgb_lines[i].b = normalise(color_scalefactors[i].xyz);
		else
			samechroma_rgb_lines[i].b = normalise(averages[i].xyz);

		rgb_luma_lines[i].a = averages[i].xyz;
		rgb_luma_lines[i].b = normalise(color_scalefactors[i].xyz);

		luminance_lines[i].a = float3(zero);
		luminance_lines[i].b = normalise(color_scalefactors[i].xyz);

		proc_uncorr_rgb_lines[i].amod		= inverse_color_scalefactors[i].xyz * (uncorr_rgb_lines[i].a - uncorr_rgb_lines[i].b * dot(uncorr_rgb_lines[i].a, uncorr_rgb_lines[i].b));
		proc_uncorr_rgb_lines[i].bs			= color_scalefactors[i].xyz * uncorr_rgb_lines[i].b;
		proc_uncorr_rgb_lines[i].bis		= inverse_color_scalefactors[i].xyz * uncorr_rgb_lines[i].b;

		proc_samechroma_rgb_lines[i].amod	= inverse_color_scalefactors[i].xyz * (samechroma_rgb_lines[i].a - samechroma_rgb_lines[i].b * dot(samechroma_rgb_lines[i].a, samechroma_rgb_lines[i].b));
		proc_samechroma_rgb_lines[i].bs		= color_scalefactors[i].xyz * samechroma_rgb_lines[i].b;
		proc_samechroma_rgb_lines[i].bis	= inverse_color_scalefactors[i].xyz * samechroma_rgb_lines[i].b;

		proc_rgb_luma_lines[i].amod			= inverse_color_scalefactors[i].xyz * (rgb_luma_lines[i].a - rgb_luma_lines[i].b * dot(rgb_luma_lines[i].a, rgb_luma_lines[i].b));
		proc_rgb_luma_lines[i].bs			= color_scalefactors[i].xyz * rgb_luma_lines[i].b;
		proc_rgb_luma_lines[i].bis			= inverse_color_scalefactors[i].xyz * rgb_luma_lines[i].b;

		proc_luminance_lines[i].amod		= inverse_color_scalefactors[i].xyz * (luminance_lines[i].a - luminance_lines[i].b * dot(luminance_lines[i].a, luminance_lines[i].b));
		proc_luminance_lines[i].bs			= color_scalefactors[i].xyz * luminance_lines[i].b;
		proc_luminance_lines[i].bis			= inverse_color_scalefactors[i].xyz * luminance_lines[i].b;
	}

	float uncorr_rgb_error[4], samechroma_rgb_error[4], rgb_luma_error[4], luminance_rgb_error[4];
	for (int i = 0; i < num_partitions; i++) {
		uncorr_rgb_error[i]		= ewb->CalcErrorSquared(block, pt, i, proc_uncorr_rgb_lines[i]);
		samechroma_rgb_error[i] = ewb->CalcErrorSquared(block, pt, i, proc_samechroma_rgb_lines[i]);
		rgb_luma_error[i]		= ewb->CalcErrorSquared(block, pt, i, proc_rgb_luma_lines[i]);
		luminance_rgb_error[i]	= ewb->CalcErrorSquared(block, pt, i, proc_luminance_lines[i]);
	}

	// compute the error that arises from just ditching alpha and RGB
	float alpha_drop_error[4], rgb_drop_error[4];
	for (int i = 0; i < num_partitions; i++) {
		alpha_drop_error[i] = 0;
		rgb_drop_error[i]	= 0;
	}
	for (int i = 0; i < num_texels; i++) {
		int			partition	= pt.partition[i];
		float4		c			= block.texel[i];
		float default_alpha		= block.hdr_alpha ? (float)0x7800 : (float)0xFFFF;

		alpha_drop_error[partition] += square(c.w - default_alpha) * ewb->v[i].w;
		rgb_drop_error[partition]	+= dot(c.xyz * c.xyz, ewb->v[i].xyz);
	}

	// check if we are eligible for blue-contraction and offset-encoding

	Endpoints ep;
	if (separate_component == -1) {
		ep = EndpointsWeights(block, pt, ewb);
	} else {
		EndpointsWeights ei1, ei2;
		Compute2PlanesEndpoints(block, pt, ewb, separate_component, &ei1, &ei2);
		ep	= ei1.Merge(ei2, separate_component);
	}

	bool	eligible_for_offset_encode[4];
	bool	eligible_for_blue_contraction[4];

	for (int i = 0; i < num_partitions; i++) {
		float4 endpt0		= ep.endpt0[i];
		float4 endpt1		= ep.endpt1[i];
		float4 endpt_dif	= endpt1 - endpt0;

		eligible_for_offset_encode[i]		= abs(endpt_dif.x) < (0.12f * 65535.0f) && abs(endpt_dif.y) < (0.12f * 65535.0f) && abs(endpt_dif.z) < (0.12f * 65535.0f);
		eligible_for_blue_contraction[i]	=
			between(endpt0.x - endpt0.z, 0.01f * 65535.0f, 0.99f * 65535.0f) && between(endpt1.x - endpt0.z, 0.01f * 65535.0f, 0.99f * 65535.0f)
		&&	between(endpt0.y - endpt0.z, 0.01f * 65535.0f, 0.99f * 65535.0f) && between(endpt1.y - endpt0.z, 0.01f * 65535.0f, 0.99f * 65535.0f);
	}


	// finally, gather up our results
	for (int i = 0; i < num_partitions; i++) {
		eci[i].rgb_scale_error   = (samechroma_rgb_error[i] - uncorr_rgb_error[i]) * 0.7f;	// empirical
		eci[i].rgb_luma_error    = (rgb_luma_error[i] - uncorr_rgb_error[i]) * 1.5f;	// wild guess
		eci[i].luminance_error   = (luminance_rgb_error[i] - uncorr_rgb_error[i]) * 3.0f;	// empirical
		eci[i].alpha_drop_error  = alpha_drop_error[i] * 3.0f;
		eci[i].rgb_drop_error    = rgb_drop_error[i] * 3.0f;
		eci[i].can_offset_encode = eligible_for_offset_encode[i];
		eci[i].can_blue_contract = eligible_for_blue_contraction[i];
	}
}


static void compute_color_error_for_every_integer_count_and_qmode(bool encode_hdr_rgb, bool encode_hdr_alpha,
	int partition_index, const PartitionTable &pt,
	const EncodingChoiceErrors *eci,
	const Endpoints *ep, float4 error_weightings[4],
	float best_error[21][4], Format format_of_choice[21][4]
) {
	int partition_size = pt.counts[partition_index];

	static const float baseline_quant_error[21] = {
		(65536.0f * 65536.0f / 18.0f),				// 2 values, 1 step
		(65536.0f * 65536.0f / 18.0f) / (2 * 2),	// 3 values, 2 steps
		(65536.0f * 65536.0f / 18.0f) / (3 * 3),	// 4 values, 3 steps
		(65536.0f * 65536.0f / 18.0f) / (4 * 4),	// 5 values
		(65536.0f * 65536.0f / 18.0f) / (5 * 5),
		(65536.0f * 65536.0f / 18.0f) / (7 * 7),
		(65536.0f * 65536.0f / 18.0f) / (9 * 9),
		(65536.0f * 65536.0f / 18.0f) / (11 * 11),
		(65536.0f * 65536.0f / 18.0f) / (15 * 15),
		(65536.0f * 65536.0f / 18.0f) / (19 * 19),
		(65536.0f * 65536.0f / 18.0f) / (23 * 23),
		(65536.0f * 65536.0f / 18.0f) / (31 * 31),
		(65536.0f * 65536.0f / 18.0f) / (39 * 39),
		(65536.0f * 65536.0f / 18.0f) / (47 * 47),
		(65536.0f * 65536.0f / 18.0f) / (63 * 63),
		(65536.0f * 65536.0f / 18.0f) / (79 * 79),
		(65536.0f * 65536.0f / 18.0f) / (95 * 95),
		(65536.0f * 65536.0f / 18.0f) / (127 * 127),
		(65536.0f * 65536.0f / 18.0f) / (159 * 159),
		(65536.0f * 65536.0f / 18.0f) / (191 * 191),
		(65536.0f * 65536.0f / 18.0f) / (255 * 255)
	};

	// float4 eps = ep->endpt_scale[partition_index];
	float4	ep0						= ep->endpt0[partition_index];	// / eps;
	float4	ep1						= ep->endpt1[partition_index];	// / eps;

	float	ep0_max					= max(max(max(ep0.x, ep0.y), ep0.z), 1e-10f);
	float	ep0_min					= max(min(min(ep0.x, ep0.y), ep0.z), 0.0f);
	float	ep1_max					= max(max(max(ep1.x, ep1.y), ep1.z), 1e-10f);
	float	ep1_min					= max(min(min(ep1.x, ep1.y), ep1.z), 0.0f);

	float4	error_weight			= error_weightings[partition_index];
	float	error_weight_rgbsum		= error_weight.x + error_weight.y + error_weight.z;
	float4	range_upper_limit		= concat(float3(encode_hdr_rgb ? 61440.0f : 65535.0f), encode_hdr_alpha ? 61440.0f : 65535.0f);

	// it is possible to get endpoint colors significantly outside [0,upper-limit] even if the input data are safely contained in [0,upper-limit];
	float4	ep0_range_error_high	= max(ep0 - range_upper_limit, zero);
	float4	ep1_range_error_high	= max(ep1 - range_upper_limit, zero);
	float4	ep0_range_error_low		= min(ep0, zero);
	float4	ep1_range_error_low		= min(ep1, zero);

	float4	sum_range_error			= (ep0_range_error_low * ep0_range_error_low) + (ep1_range_error_low * ep1_range_error_low) + (ep0_range_error_high * ep0_range_error_high) + (ep1_range_error_high * ep1_range_error_high);
	float	rgb_range_error			= dot(sum_range_error.xyz, error_weight.xyz) * 0.5f * partition_size;
	float	alpha_range_error		= sum_range_error.w * error_weight.w * 0.5f * partition_size;

	if (encode_hdr_rgb) {
		// collect some statistics
		float af, cf;
		if (ep1.x > ep1.y && ep1.x > ep1.z) {
			af = ep1.x;
			cf = ep1.x - ep0.x;
		} else if (ep1.y > ep1.z) {
			af = ep1.y;
			cf = ep1.y - ep0.y;
		} else {
			af = ep1.z;
			cf = ep1.z - ep0.z;
		}

		float	bf		= af - ep1_min;	// estimate of color-component spread in high endpoint color
		float3	prd		= ep1.xyz - float3(cf);
		float3	pdif	= prd - ep0.xyz;
		// estimate of color-component spread in low endpoint color
		float	df		= max(max(abs(pdif.x), abs(pdif.y)), abs(pdif.z));

		int		b		= (int)bf;
		int		c		= (int)cf;
		int		d		= (int)df;

		// determine which one of the 6 submodes is likely to be used in case of an RGBO-mode
		int rgbo_mode = 5;		// 7 bits per component
		// mode 4: 8 7 6
		if (b < 32768 && c < 16384)
			rgbo_mode = 4;
		// mode 3: 9 6 7
		if (b < 8192 && c < 16384)
			rgbo_mode = 3;
		// mode 2: 10 5 8
		if (b < 2048 && c < 16384)
			rgbo_mode = 2;
		// mode 1: 11 6 5
		if (b < 2048 && c < 1024)
			rgbo_mode = 1;
		// mode 0: 11 5 7
		if (b < 1024 && c < 4096)
			rgbo_mode = 0;

		// determine which one of the 9 submodes is likely to be used in case of an RGB-mode.
		int rgb_mode = 8;		// 8 bits per component, except 7 bits for blue
		// mode 0: 9 7 6 7
		if (b < 16384 && c < 8192 && d < 8192)
			rgb_mode = 0;
		// mode 1: 9 8 6 6
		if (b < 32768 && c < 8192 && d < 4096)
			rgb_mode = 1;
		// mode 2: 10 6 7 7
		if (b < 4096 && c < 8192 && d < 4096)
			rgb_mode = 2;
		// mode 3: 10 7 7 6
		if (b < 8192 && c < 8192 && d < 2048)
			rgb_mode = 3;
		// mode 4: 11 8 6 5
		if (b < 8192 && c < 2048 && d < 512)
			rgb_mode = 4;
		// mode 5: 11 6 8 6
		if (b < 2048 && c < 8192 && d < 1024)
			rgb_mode = 5;
		// mode 6: 12 7 7 5
		if (b < 2048 && c < 2048 && d < 256)
			rgb_mode = 6;
		// mode 7: 12 6 7 6
		if (b < 1024 && c < 2048 && d < 512)
			rgb_mode = 7;

		static const float rgbo_error_scales[6] = { 4.0f, 4.0f, 16.0f, 64.0f, 256.0f, 1024.0f };
		static const float rgb_error_scales[9]	= { 64.0f, 64.0f, 16.0f, 16.0f, 4.0f, 4.0f, 1.0f, 1.0f, 384.0f };

		float mode7mult		= rgbo_error_scales[rgbo_mode] * 0.0015f;	// empirically determined ....
		float mode11mult	= rgb_error_scales[rgb_mode] * 0.010f;		// empirically determined ....

		float lum_high		= (ep1.x + ep1.y + ep1.z) * (1.0f / 3.0f);
		float lum_low		= (ep0.x + ep0.y + ep0.z) * (1.0f / 3.0f);
		float lumdif		= lum_high - lum_low;
		float mode23mult	= lumdif < 960 ? 4.0f : lumdif < 3968 ? 16.0f : 128.0f;

		mode23mult *= 0.0005f;	// empirically determined ....

		// pick among the available HDR endpoint modes
		for (int i = 0; i < 8; i++) {
			best_error[i][3] = 1e30f;
			format_of_choice[i][3] = encode_hdr_alpha ? FMT_HDR_RGBA : FMT_HDR_RGB_LDR_ALPHA;
			best_error[i][2] = 1e30f;
			format_of_choice[i][2] = FMT_HDR_RGB;
			best_error[i][1] = 1e30f;
			format_of_choice[i][1] = FMT_HDR_RGB_SCALE;
			best_error[i][0] = 1e30f;
			format_of_choice[i][0] = FMT_HDR_LUMINANCE_LARGE_RANGE;
		}

		for (int i = 8; i < 21; i++) {
			// base_quant_error should depend on the scale-factor that would be used
			// during actual encode of the color value.

			float base_quant_error			= baseline_quant_error[i] * partition_size * 1.0f;
			float rgb_quantization_error	= error_weight_rgbsum * base_quant_error * 2.0f;
			float alpha_quantization_error	= error_weight.w * base_quant_error * 2.0f;
			float rgba_quantization_error	= rgb_quantization_error + alpha_quantization_error;

			// for 8 integers, we have two encodings: one with HDR alpha and another one
			// with LDR alpha.

			best_error[i][3] = rgba_quantization_error + rgb_range_error + alpha_range_error;;
			format_of_choice[i][3] = encode_hdr_alpha ? FMT_HDR_RGBA : FMT_HDR_RGB_LDR_ALPHA;

			// for 6 integers, we have one HDR-RGB encoding
			best_error[i][2] = (rgb_quantization_error * mode11mult) + rgb_range_error + eci->alpha_drop_error;;
			format_of_choice[i][2] = FMT_HDR_RGB;

			// for 4 integers, we have one HDR-RGB-Scale encoding
			best_error[i][1] = (rgb_quantization_error * mode7mult) + rgb_range_error + eci->alpha_drop_error + eci->rgb_luma_error;
			format_of_choice[i][1] = FMT_HDR_RGB_SCALE;

			// for 2 integers, we assume luminance-with-large-range
			best_error[i][0] = (rgb_quantization_error * mode23mult) + rgb_range_error + eci->alpha_drop_error + eci->luminance_error;
			format_of_choice[i][0] = FMT_HDR_LUMINANCE_LARGE_RANGE;
		}
	} else {
		for (int i = 0; i < 4; i++) {
			best_error[i][3] = 1e30f;
			best_error[i][2] = 1e30f;
			best_error[i][1] = 1e30f;
			best_error[i][0] = 1e30f;

			format_of_choice[i][3] = FMT_RGBA;
			format_of_choice[i][2] = FMT_RGB;
			format_of_choice[i][1] = FMT_RGB_SCALE;
			format_of_choice[i][0] = FMT_LUMINANCE;
		}

		// pick among the available LDR endpoint modes
		for (int i = 4; i < 21; i++) {
			float base_quant_error			= baseline_quant_error[i] * partition_size * 1.0f;
			float rgb_quantization_error	= error_weight_rgbsum * base_quant_error;
			float alpha_quantization_error	= error_weight.w * base_quant_error;
			float rgba_quantization_error	= rgb_quantization_error + alpha_quantization_error;

			// for 8 integers, the available encodings are:
			// full LDR RGB-Alpha
			float full_ldr_rgba_error = rgba_quantization_error;
			if (eci->can_blue_contract)
				full_ldr_rgba_error *= 0.625f;
			if (eci->can_offset_encode && i <= 18)
				full_ldr_rgba_error *= 0.5f;
			full_ldr_rgba_error += rgb_range_error + alpha_range_error;

			best_error[i][3] = full_ldr_rgba_error;
			format_of_choice[i][3] = FMT_RGBA;

			// for 6 integers, we have:
			// - an LDR-RGB encoding
			// - an RGBS + Alpha encoding (LDR)

			float full_ldr_rgb_error = rgb_quantization_error;
			if (eci->can_blue_contract)
				full_ldr_rgb_error *= 0.5f;
			if (eci->can_offset_encode && i <= 18)
				full_ldr_rgb_error *= 0.25f;
			full_ldr_rgb_error += eci->alpha_drop_error + rgb_range_error;

			float rgbs_alpha_error = rgba_quantization_error + eci->rgb_scale_error + rgb_range_error + alpha_range_error;
			if (rgbs_alpha_error < full_ldr_rgb_error) {
				best_error[i][2] = rgbs_alpha_error;
				format_of_choice[i][2] = FMT_RGB_SCALE_ALPHA;
			} else {
				best_error[i][2] = full_ldr_rgb_error;
				format_of_choice[i][2] = FMT_RGB;
			}

			// for 4 integers, we have a Luminance-Alpha encoding and the RGBS encoding
			float ldr_rgbs_error	= rgb_quantization_error + eci->alpha_drop_error + eci->rgb_scale_error + rgb_range_error;
			float lum_alpha_error	= rgba_quantization_error + eci->luminance_error + rgb_range_error + alpha_range_error;
			if (ldr_rgbs_error < lum_alpha_error) {
				best_error[i][1] = ldr_rgbs_error;
				format_of_choice[i][1] = FMT_RGB_SCALE;
			} else {
				best_error[i][1] = lum_alpha_error;
				format_of_choice[i][1] = FMT_LUMINANCE_ALPHA;
			}

			// for 2 integers, we have a Luminance-encoding and an Alpha-encoding.
			best_error[i][0] = rgb_quantization_error + eci->alpha_drop_error + eci->luminance_error + rgb_range_error;;
			format_of_choice[i][0] = FMT_LUMINANCE;
		}
	}
}

void determine_optimal_set_of_endpoint_formats_to_use(const ImageStats &block, const PartitionTable &pt, const ErrorWeightBlock *ewb,
	const Endpoints *ep,
	int separate_component,
	const int *qwt_bitcounts, const float *qwt_errors,
	Format partition_format_specifiers[4][4], int quantized_weight[4],
	Quant qmode[4], Quant qmode_mod[4]
) {
	int		num_partitions		= pt.num_partitions;
	bool	encode_hdr_rgb		= block.hdr_rgb;
	bool	encode_hdr_alpha	= block.hdr_alpha;

	// call a helper function to compute the errors that result from various
	// encoding choices (such as using luminance instead of RGB, discarding Alpha,
	// using RGB-scale in place of two separate RGB endpoints and so on)
	EncodingChoiceErrors eci[4];
	CalcEncodingChoiceErrors(eci, block, pt, separate_component, ewb);

	// for each partition, compute the error weights to apply for that partition.
	float4 error_weightings[4];
	float4 dummied_color_scalefactors[4];	// only used to receive data
	ewb->PartitionWeightings(pt, error_weightings, dummied_color_scalefactors);

	float	best_error[4][21][4];
	Format	format_of_choice[4][21][4];
	for (int i = 0; i < num_partitions; i++)
		compute_color_error_for_every_integer_count_and_qmode(encode_hdr_rgb, encode_hdr_alpha, i, pt, &(eci[i]), ep, error_weightings, best_error[i], format_of_choice[i]);

	float	errors_of_best_combination[ASTC::MAX_WEIGHT_MODES];
	Quant	best_qmodes[ASTC::MAX_WEIGHT_MODES];
	Quant	best_qmodes_mod[ASTC::MAX_WEIGHT_MODES];
	Format	best_ep_formats[ASTC::MAX_WEIGHT_MODES][4];

	switch (num_partitions) {
		case 1: {	// code for the case where the block contains 1 partition
			for (int i = 0; i < ASTC::MAX_WEIGHT_MODES; i++) {
				if (qwt_errors[i] >= 1e29f) {
					errors_of_best_combination[i] = 1e30f;
					continue;
				}

				int		bits_available				= qwt_bitcounts[i];
				int		best_integer_count			= -1;
				float	best_integer_count_error	= 1e20f;
				for (int j = 0; j < 4; j++) {
					Quant qmode = quantization_table[j + 1][bits_available];
					if (qmode != QUANT_ILLEGAL && best_error[0][qmode][j] < best_integer_count_error) {
						best_integer_count_error	= best_error[0][qmode][j];
						best_integer_count			= j;
					}
				}

				Quant	best_qmode		= quantization_table[best_integer_count + 1][bits_available];

				errors_of_best_combination[i] = best_integer_count_error + qwt_errors[i];
				best_qmodes[i]			= best_qmode;
				best_qmodes_mod[i]		= best_qmode;
				best_ep_formats[i][0]	= best_qmode == QUANT_ILLEGAL ? FMT_LUMINANCE : format_of_choice[0][best_qmode][best_integer_count];
			}
			break;
		}

		case 2: {	// code for the case where the block contains 2 partitions
			Format	best_formats[2];
			float	combined_best_error[21][7];
			Format	formats_of_choice[21][7][2];

			for (int i = 0; i < 21; i++)
				for (int j = 0; j < 7; j++)
					combined_best_error[i][j] = 1e30f;

			for (int quant = 5; quant < 21; quant++) {
				for (int i = 0; i < 4; i++) {	// integer-count for first endpoint-pair
					for (int j = 0; j < 4; j++)	{ // integer-count for second endpoint-pair
						int low2 = min(i, j);
						int high2 = max(i, j);
						if (high2 - low2 > 1)
							continue;

						int intcnt = i + j;
						float errorterm = min(best_error[0][quant][i] + best_error[1][quant][j], 1e10f);
						if (errorterm <= combined_best_error[quant][intcnt]) {
							combined_best_error[quant][intcnt] = errorterm;
							formats_of_choice[quant][intcnt][0] = format_of_choice[0][quant][i];
							formats_of_choice[quant][intcnt][1] = format_of_choice[1][quant][j];
						}
					}
				}
			}

			for (int i = 0; i < ASTC::MAX_WEIGHT_MODES; i++) {
				if (qwt_errors[i] >= 1e29f) {
					errors_of_best_combination[i] = 1e30f;
					continue;
				}
				int		bits_available				= qwt_bitcounts[i];
				int		best_integer_count			= 0;
				float	best_integer_count_error	= 1e20f;

				for (int j = 2; j <= 8; j++) {
					Quant qmode = quantization_table[j][bits_available];
					if (qmode != QUANT_ILLEGAL && combined_best_error[qmode][j - 2] < best_integer_count_error) {
						best_integer_count_error = combined_best_error[qmode][j - 2];
						best_integer_count = j;
					}
				}

				Quant	best_qmode		= quantization_table[best_integer_count][bits_available];
				Quant	best_qmode_mod	= quantization_table[best_integer_count][bits_available + 2];
				float	error_of_best_combination = best_integer_count_error;
				if (best_qmode != QUANT_ILLEGAL) {
					for (int j = 0; j < 2; j++)
						best_formats[j] = formats_of_choice[best_qmode][best_integer_count - 2][j];
				} else {
					for (int j = 0; j < 2; j++)
						best_formats[j] = FMT_LUMINANCE;
				}
				error_of_best_combination += qwt_errors[i];

				errors_of_best_combination[i] = error_of_best_combination;
				best_qmodes[i] = best_qmode;
				best_qmodes_mod[i] = best_qmode_mod;
				best_ep_formats[i][0] = best_formats[0];
				best_ep_formats[i][1] = best_formats[1];
			}
			break;
		}

		case 3: {	// code for the case where the block contains 3 partitions
			Format	best_formats[3];
			float	combined_best_error[21][10];
			Format	formats_of_choice[21][10][3];

			for (int i = 0; i < 21; i++)
				for (int j = 0; j < 10; j++)
					combined_best_error[i][j] = 1e30f;

			for (int quant = 5; quant < 21; quant++) {
				for (int i = 0; i < 4; i++)	{ // integer-count for first endpoint-pair
					for (int j = 0; j < 4; j++) {	// integer-count for second endpoint-pair
						int low2 = min(i, j);
						int high2 = max(i, j);
						if ((high2 - low2) > 1)
							continue;
						for (int k = 0; k < 4; k++)	{ // integer-count for third endpoint-pair
							int low3 = min(k, low2);
							int high3 = max(k, high2);
							if ((high3 - low3) > 1)
								continue;

							int intcnt = i + j + k;
							float errorterm = min(best_error[0][quant][i] + best_error[1][quant][j] + best_error[2][quant][k], 1e10f);
							if (errorterm <= combined_best_error[quant][intcnt]) {
								combined_best_error[quant][intcnt] = errorterm;
								formats_of_choice[quant][intcnt][0] = format_of_choice[0][quant][i];
								formats_of_choice[quant][intcnt][1] = format_of_choice[1][quant][j];
								formats_of_choice[quant][intcnt][2] = format_of_choice[2][quant][k];
							}
						}
					}
				}
			}

			for (int i = 0; i < ASTC::MAX_WEIGHT_MODES; i++) {
				if (qwt_errors[i] >= 1e29f) {
					errors_of_best_combination[i] = 1e30f;
					continue;
				}
				int		bits_available				= qwt_bitcounts[i];
				int		best_integer_count		 = 0;
				float	best_integer_count_error = 1e20f;

				for (int j = 3; j <= 9; j++) {
					Quant qmode = quantization_table[j][bits_available];
					if (qmode != QUANT_ILLEGAL && combined_best_error[qmode][j - 3] < best_integer_count_error) {
						best_integer_count_error = combined_best_error[qmode][j - 3];
						best_integer_count	= j;
					}
				}

				Quant	best_qmode		= quantization_table[best_integer_count][bits_available];
				Quant	best_qmode_mod	= quantization_table[best_integer_count][bits_available + 5];
				float	error_of_best_combination = best_integer_count_error;
				if (best_qmode != QUANT_ILLEGAL) {
					for (int j = 0; j < 3; j++)
						best_formats[j] = formats_of_choice[best_qmode][best_integer_count - 3][j];
				} else {
					for (int j = 0; j < 3; j++)
						best_formats[j] = FMT_LUMINANCE;
				}

				error_of_best_combination += qwt_errors[i];

				errors_of_best_combination[i] = error_of_best_combination;
				best_qmodes[i] = best_qmode;
				best_qmodes_mod[i] = best_qmode_mod;
				best_ep_formats[i][0] = best_formats[0];
				best_ep_formats[i][1] = best_formats[1];
				best_ep_formats[i][2] = best_formats[2];
			}
			break;
		}
		case 4: {	// code for the case where the block contains 4 partitions
			Format	best_formats[4];
			float	combined_best_error[21][13];
			Format	formats_of_choice[21][13][4];

			for (int i = 0; i < 21; i++)
				for (int j = 0; j < 13; j++)
					combined_best_error[i][j] = 1e30f;

			for (int quant = 5; quant < 21; quant++) {
				for (int i = 0; i < 4; i++)	{ // integer-count for first endpoint-pair
					for (int j = 0; j < 4; j++) {	// integer-count for second endpoint-pair
						int low2 = min(i, j);
						int high2 = max(i, j);
						if ((high2 - low2) > 1)
							continue;
						for (int k = 0; k < 4; k++)	{ // integer-count for third endpoint-pair
							int low3 = min(k, low2);
							int high3 = max(k, high2);
							if ((high3 - low3) > 1)
								continue;
							for (int l = 0; l < 4; l++)	{ // integer-count for fourth endpoint-pair
								int low4 = min(l, low3);
								int high4 = max(l, high3);
								if ((high4 - low4) > 1)
									continue;

								int intcnt = i + j + k + l;
								float errorterm = min(best_error[0][quant][i] + best_error[1][quant][j] + best_error[2][quant][k] + best_error[3][quant][l], 1e10f);
								if (errorterm <= combined_best_error[quant][intcnt]) {
									combined_best_error[quant][intcnt] = errorterm;
									formats_of_choice[quant][intcnt][0] = format_of_choice[0][quant][i];
									formats_of_choice[quant][intcnt][1] = format_of_choice[1][quant][j];
									formats_of_choice[quant][intcnt][2] = format_of_choice[2][quant][k];
									formats_of_choice[quant][intcnt][3] = format_of_choice[3][quant][l];
								}
							}
						}
					}
				}
			}

			for (int i = 0; i < ASTC::MAX_WEIGHT_MODES; i++) {
				if (qwt_errors[i] >= 1e29f) {
					errors_of_best_combination[i] = 1e30f;
					continue;
				}
				int		bits_available				= qwt_bitcounts[i];
				int		best_integer_count = 0;
				float	best_integer_count_error = 1e20f;

				for (int j = 4; j <= 9; j++) {
					Quant qmode = quantization_table[j][bits_available];
					if (qmode == QUANT_ILLEGAL && combined_best_error[qmode][j - 4] < best_integer_count_error) {
						best_integer_count_error	= combined_best_error[qmode][j - 4];
						best_integer_count			= j;
					}
				}

				Quant	best_qmode		= quantization_table[best_integer_count][bits_available];
				Quant	best_qmode_mod	= quantization_table[best_integer_count][bits_available + 8];
				float	error_of_best_combination = best_integer_count_error;
				if (best_qmode != QUANT_ILLEGAL) {
					for (int j = 0; j < 4; j++)
						best_formats[j] = formats_of_choice[best_qmode][best_integer_count - 4][j];
				} else {
					for (int j = 0; j < 4; j++)
						best_formats[j] = FMT_LUMINANCE;
				}
				error_of_best_combination += qwt_errors[i];

				errors_of_best_combination[i] = error_of_best_combination;
				best_qmodes[i]		= best_qmode;
				best_qmodes_mod[i]	= best_qmode_mod;
				best_ep_formats[i][0] = best_formats[0];
				best_ep_formats[i][1] = best_formats[1];
				best_ep_formats[i][2] = best_formats[2];
				best_ep_formats[i][3] = best_formats[3];
			}
			break;
		}
	}

	// finally, go through the results and pick the 4 best-looking modes.

	int best_error_weights[4];
	for (int i = 0; i < 4; i++) {
		float best_ep_error = 1e30f;
		int best_error_index = -1;
		for (int j = 0; j < ASTC::MAX_WEIGHT_MODES; j++) {
			if (errors_of_best_combination[j] < best_ep_error && best_qmodes[j] >= 5) {
				best_ep_error = errors_of_best_combination[j];
				best_error_index = j;
			}
		}
		best_error_weights[i] = best_error_index;

		if (best_error_index >= 0)
			errors_of_best_combination[best_error_index] = 1e30f;
	}

	for (int i = 0; i < 4; i++) {
		quantized_weight[i] = best_error_weights[i];
		if (quantized_weight[i] >= 0) {
			qmode[i] = best_qmodes[best_error_weights[i]];
			qmode_mod[i] = best_qmodes_mod[best_error_weights[i]];
			for (int j = 0; j < num_partitions; j++)
				partition_format_specifiers[i][j] = best_ep_formats[best_error_weights[i]][j];
		}
	}
}
//aaa
void BlockSizeDescriptor::ComputeAngularEndpoints1(float mode_cutoff, const float *decimated_quantized_weights, const float *decimated_weights, float *low_value, float *high_value) const {
	float low_values[ASTC::MAX_DECIMATION_MODES][12];
	float high_values[ASTC::MAX_DECIMATION_MODES][12];

	for (int i = 0; i < dec.size(); i++) {
		const Decimation &d = dec[i];
		if (!d.permit_encode || d.samples < 1 || d.maxprec[0] == QUANT_ILLEGAL || d.percentile > mode_cutoff)
			continue;

		compute_angular_endpoints_for_qmodes(d.samples,
			decimated_quantized_weights + i * ASTC::MAX_WEIGHTS_PER_BLOCK,
			decimated_weights + i * ASTC::MAX_WEIGHTS_PER_BLOCK, d.maxprec[0], low_values[i], high_values[i]
		);
	}

	for (int i = 0; i < ASTC::MAX_WEIGHT_MODES; i++) {
		if (!modes[i].dual && modes[i].percentile <= mode_cutoff) {
			Quant	quant_mode	= modes[i].qmode;
			int		decim_mode	= modes[i].decimation;
			low_value[i]		= low_values[decim_mode][quant_mode];
			high_value[i]		= high_values[decim_mode][quant_mode];
		}
	}
}

void BlockSizeDescriptor::ComputeAngularEndpoints2(float mode_cutoff,
	const float *decimated_quantized_weights,
	const float *decimated_weights,
	float *low_value1, float *high_value1, float *low_value2, float *high_value2
) const {
	float low_values1[ASTC::MAX_DECIMATION_MODES][12];
	float high_values1[ASTC::MAX_DECIMATION_MODES][12];
	float low_values2[ASTC::MAX_DECIMATION_MODES][12];
	float high_values2[ASTC::MAX_DECIMATION_MODES][12];

	for (int i = 0; i < dec.size(); i++) {
		const Decimation &d = dec[i];
		if (!d.permit_encode || d.samples < 1 || d.maxprec[2] == QUANT_ILLEGAL < 0 || d.percentile > mode_cutoff)
			continue;

		compute_angular_endpoints_for_qmodes(
			d.samples,
			decimated_quantized_weights + (2 * i + 0) * ASTC::MAX_WEIGHTS_PER_BLOCK,
			decimated_weights + (2 * i + 0) * ASTC::MAX_WEIGHTS_PER_BLOCK,
			d.maxprec[1], low_values1[i], high_values1[i]
		);

		compute_angular_endpoints_for_qmodes(
			d.samples,
			decimated_quantized_weights + (2 * i + 1) * ASTC::MAX_WEIGHTS_PER_BLOCK,
			decimated_weights + (2 * i + 1) * ASTC::MAX_WEIGHTS_PER_BLOCK,
			d.maxprec[1], low_values2[i], high_values2[i]
		);
	}

	for (int i = 0; i < ASTC::MAX_WEIGHT_MODES; i++) {
		if (modes[i].dual && modes[i].percentile <= mode_cutoff) {
			Quant	quant_mode = modes[i].qmode;
			int			decim_mode = modes[i].decimation;

			low_value1[i]	= low_values1[decim_mode][quant_mode];
			high_value1[i]	= high_values1[decim_mode][quant_mode];
			low_value2[i]	= low_values2[decim_mode][quant_mode];
			high_value2[i]	= high_values2[decim_mode][quant_mode];
		}
	}
}
//-----------------------------------------------------------------------------
//	BlockInfo
//-----------------------------------------------------------------------------

struct BlockInfo {
	uint16		mode;								// 0 to 2047. Negative value marks constant-color block (-1: FP16, -2:UINT16)
	uint16		partition_id;						// 0 to 1023
	uint8		num_partitions;						// 1 to 4; Zero marks a constant-color block.
	uint8		plane2_color_component;				// color component for the secondary plane of weights
	bool		error_block				= false;	// 1 marks error block, 0 marks non-error-block.
	bool		color_formats_matched	= false;	// color format for all endpoint pairs are matched.
	Format		color_formats[4];					// color format for each endpoint color pair.
	uint8		color_values[4][12];				// unquantized endpoint color pairs.
	Quant		color_qmode;
	uint8		plane_weights[2][ASTC::MAX_WEIGHTS_PER_BLOCK];	// quantized and decimated weights
	uint16x4	const_color;				// constant-color, as FP16 or UINT16. Used for constant-color blocks only.

	BlockInfo() {}
	BlockInfo(const ASTC &pb, int xdim, int ydim, int zdim);
	BlockInfo(const block<ISO_rgba, 3> &block, int xdim, int ydim, int zdim, CompressionParams *params);
	BlockInfo(const block<HDRpixel, 3> &block, int xdim, int ydim, int zdim, CompressionParams *params);
};

BlockInfo::BlockInfo(const ASTC &pb, int xdim, int ydim, int zdim) : mode(pb.mode) {
	if ((mode & 0x1FF) == 0x1FC) {
		// void-extent block
		num_partitions = 0;

		for (int i = 0; i < 4; i++)
			const_color[i] = pb.const_col[i];

		// check the void-extent
		if (zdim == 1) {
			// 2D void-extent
			error_block = pb.vx2d.rsvd != 3 || (
				!(pb.vx2d.x0 == 0x1FFF && pb.vx2d.x1 == 0x1FFF && pb.vx2d.y0 == 0x1FFF && pb.vx2d.y1 == 0x1FFF)
				&&	(pb.vx2d.x0 >= pb.vx2d.x1 || pb.vx2d.y0 >= pb.vx2d.y1)
			);
		} else {
			// 3D void-extent
			error_block = !(pb.vx3d.x0 == 0x1FF && pb.vx3d.x1 == 0x1FF && pb.vx3d.y0 == 0x1FF && pb.vx3d.y1 == 0x1FF && pb.vx3d.z0 == 0x1FF && pb.vx3d.z1 == 0x1FF)
				&&	(pb.vx3d.x0 >= pb.vx3d.x1 || pb.vx3d.y0 >= pb.vx3d.y1 || pb.vx3d.z0 >= pb.vx3d.z1);
		}
		return;
	}

	// get hold of the block-size descriptor and the Decimation tables.
	const BlockSizeDescriptor	*bsd = BlockSizeDescriptor::get(xdim, ydim, zdim);
	if (!bsd->modes[mode].permit_decode) {
		error_block = true;
		return;
	}

	int			weight_count		= bsd->dec[bsd->modes[mode].decimation].tables->num_weights;
	Quant		weight_qmode		= bsd->modes[mode].qmode;
	bool		dual				= bsd->modes[mode].dual;
	int			real_weight_count	= weight_count << int(dual);

	num_partitions					= pb.num_partitions + 1;

	uint8 bswapped[16];
	for (int i = 0; i < 16; i++)
		bswapped[i] = reverse_bits(pb.data[15 - i]);

	int bits_for_weights	= ISE_bits(weight_qmode, real_weight_count);
	int below_weights_pos	= 128 - bits_for_weights;

	if (dual) {
		if (num_partitions == 4)
			error_block = true;

		uint8 indices[64];
		ISE_decode(weight_qmode, real_weight_count, bswapped, indices, 0);
		for (int i = 0; i < weight_count; i++) {
			plane_weights[0][i] = indices[2 * i + 0];
			plane_weights[1][i] = indices[2 * i + 1];
		}
	} else {
		ISE_decode(weight_qmode, weight_count, bswapped, plane_weights[0], 0);
	}

	// then, determine the format of each endpoint pair
	int encoded_type_highpart_size = 0;
	if (num_partitions == 1) {
		color_formats[0]	= Format(pb.col_fmt);
		partition_id		= 0;
	} else {
		encoded_type_highpart_size = 3 * num_partitions - 4;
		below_weights_pos	-= encoded_type_highpart_size;
		int encoded_type	= pb.mode_bits | (read_bits(pb.data, below_weights_pos, encoded_type_highpart_size) << 6);
		int baseclass		= encoded_type & 0x3;
		if (baseclass == 0) {
			for (int i = 0; i < num_partitions; i++)
				color_formats[i] = Format((encoded_type >> 2) & 0xF);
			below_weights_pos			+= encoded_type_highpart_size;
			color_formats_matched		= true;
			encoded_type_highpart_size	= 0;
		} else {
			baseclass--;
			for (int i = 0; i < num_partitions; i++)
				color_formats[i] = Format(((((encoded_type >> (i + 2)) & 1) + baseclass) << 2) | (encoded_type >> (2 + num_partitions + i * 2) & 3));
		}
		partition_id	= pb.partition_id;
	}

	// then, determine the number of integers we need to unpack for the endpoint pairs
	int comp_count = 0;
	for (int i = 0; i < num_partitions; i++)
		comp_count += NumComps(color_formats[i]);

	if (comp_count > 9)
		error_block = true;

	// then, determine the color endpoint format to use for these integers
	static const int color_bits_arr[5] = { -1, 115 - 4, 113 - 4 - 10, 113 - 4 - 10, 113 - 4 - 10 };
	int color_bits = max(color_bits_arr[num_partitions] - bits_for_weights - encoded_type_highpart_size - (dual ? 2 : 0), 0);
	color_qmode = quantization_table[comp_count][color_bits];
	if (color_qmode < QUANT_6)
		error_block = true;

	// then unpack the integer-bits
	uint8 values_to_decode[32];
	ISE_decode(color_qmode, comp_count * 2, pb.data, values_to_decode, (num_partitions == 1 ? 17 : 19 + 10));

	// and distribute them over the endpoint types
	uint8	*vals = values_to_decode;
	for (int i = 0; i < num_partitions; i++) {
		for (int j = 0, n = NumComps(color_formats[i]) * 2; j < n; j++)
			color_values[i][j] = UnQuantCol(color_qmode, *vals++);
	}

	// get hold of color component for second-plane in the case of dual plane of weights
	if (dual)
		plane2_color_component = read_bits(pb.data, below_weights_pos - 2, 2);
}

BlockInfo::BlockInfo(const block<HDRpixel, 3> &block, int xdim, int ydim, int zdim, CompressionParams *params) : mode(0), error_block(false) {
	ImageStats	stats(block, xdim, ydim, zdim);
	if (all(stats.a == stats.b)) {
		// constant-color block
		mode			= 0x200;
		num_partitions = 0;
		const_color		= to<uint16>(stats.a);
	}
}

int realign_weights(const ImageStats &block, const ErrorWeightBlock * ewb, BlockInfo *scb, uint8 *weight_set8, uint8 *plane2_weight_set8) {
	int adjustments = 0;
#if 0
	// get the appropriate partition descriptor.
	int num_partitions = scb->num_partitions;
	const partition_info *pt = get_partition_table(xdim, ydim, zdim, num_partitions);
	pt += scb->partition_id;

	// get the appropriate block descriptor
	const block_size_descriptor *bsd = get_block_size_descriptor(xdim, ydim, zdim);
	const decimation_table *const *ixtab2 = bsd->decimation_tables;

	const decimation_table *it = ixtab2[bsd->modes[scb->mode].decimation];

	int dual = bsd->modes[scb->mode].dual;

	// get quantization-parameters
	int weight_qmode = bsd->modes[scb->mode].qmode;


	// decode the color endpoints
	uint16x4 color_endpoint0[4];
	uint16x4 color_endpoint1[4];
	int rgb_hdr[4];
	int alpha_hdr[4];
	int nan_endpoint[4];


	for (i = 0; i < num_partitions; i++)
		unpack_color_endpoints(decode_mode,
		scb->color_formats[i], scb->color_qmode, scb->color_values[i], &rgb_hdr[i], &alpha_hdr[i], &nan_endpoint[i], &(color_endpoint0[i]), &(color_endpoint1[i]));


	float uq_plane1_weights[MAX_WEIGHTS_PER_BLOCK];
	float uq_plane2_weights[MAX_WEIGHTS_PER_BLOCK];
	int weight_count = d.num_weights;

	// read and unquantize the weights.

	const quantization_and_transfer_table *qat = &(quant_and_xfer_tables[weight_qmode]);

	for (i = 0; i < weight_count; i++) {
		uq_plane1_weights[i] = qat->unquantized_value_flt[weight_set8[i]];
	}
	if (dual) {
		for (i = 0; i < weight_count; i++)
			uq_plane2_weights[i] = qat->unquantized_value_flt[plane2_weight_set8[i]];
	}


	int plane2_color_component = dual ? scb->plane2_color_component : -1;

	// for each weight, unquantize the weight, use it to compute a color and a color error.
	// then, increment the weight until the color error stops decreasing
	// then, decrement the weight until the color error stops increasing

#define COMPUTE_ERROR( errorvar ) \
		errorvar = 0.0f; \
		for(j=0;j<texels_to_evaluate;j++) \
					{ \
			int texel = d.weight_texel[i][j]; \
			int partition = pt->partition_of_texel[texel]; \
			float plane1_weight = compute_value_of_texel_flt( texel, it, uq_plane1_weights ); \
			float plane2_weight = 0.0f; \
			if( dual ) \
				plane2_weight = compute_value_of_texel_flt( texel, it, uq_plane2_weights ); \
			int int_plane1_weight = static_cast<int>(floor( plane1_weight*64.0f + 0.5f ) ); \
			int int_plane2_weight = static_cast<int>(floor( plane2_weight*64.0f + 0.5f ) ); \
			uint16x4 lrp_color = lerp_color_int( \
				decode_mode, \
				color_endpoint0[partition], \
				color_endpoint1[partition], \
				int_plane1_weight, \
				int_plane2_weight, \
				plane2_color_component ); \
			float4 color = float4( lrp_color.x, lrp_color.y, lrp_color.z, lrp_color.w ); \
			float4 origcolor = float4( \
				blk->work_data[4*texel], \
				blk->work_data[4*texel+1], \
				blk->work_data[4*texel+2], \
				blk->work_data[4*texel+3] ); \
			float4 error_weight = ewb->error_weights[texel]; \
			float4 colordiff = color - origcolor; \
			errorvar += dot( colordiff*colordiff, error_weight ); \
					}


	for (i = 0; i < weight_count; i++) {
		int current_wt = weight_set8[i];
		int texels_to_evaluate = d.weight_num_texels[i];

		float current_error;

		COMPUTE_ERROR(current_error);

		// increment until error starts increasing.
		while (1) {
			int next_wt = qat->next_quantized_value[current_wt];
			if (next_wt == current_wt)
				break;
			uq_plane1_weights[i] = qat->unquantized_value_flt[next_wt];
			float next_error;
			COMPUTE_ERROR(next_error);
			if (next_error < current_error) {
				// succeeded, increment the weight
				current_wt = next_wt;
				current_error = next_error;
				adjustments++;
			} else {
				// failed, back out the attempted increment
				uq_plane1_weights[i] = qat->unquantized_value_flt[current_wt];
				break;
			}
		}
		// decrement until error starts increasing
		while (1) {
			int prev_wt = qat->prev_quantized_value[current_wt];
			if (prev_wt == current_wt)
				break;
			uq_plane1_weights[i] = qat->unquantized_value_flt[prev_wt];
			float prev_error;
			COMPUTE_ERROR(prev_error);
			if (prev_error < current_error) {
				// succeeded, decrement the weight
				current_wt = prev_wt;
				current_error = prev_error;
				adjustments++;
			} else {
				// failed, back out the attempted decrement
				uq_plane1_weights[i] = qat->unquantized_value_flt[current_wt];
				break;
			}
		}

		weight_set8[i] = current_wt;
	}

	if (!dual)
		return adjustments;

	// processing of the second plane of weights
	for (i = 0; i < weight_count; i++) {
		int current_wt = plane2_weight_set8[i];
		int texels_to_evaluate = d.weight_num_texels[i];

		float current_error;

		COMPUTE_ERROR(current_error);

		// increment until error starts increasing.
		while (1) {
			int next_wt = qat->next_quantized_value[current_wt];
			if (next_wt == current_wt)
				break;
			uq_plane2_weights[i] = qat->unquantized_value_flt[next_wt];
			float next_error;
			COMPUTE_ERROR(next_error);
			if (next_error < current_error) {
				// succeeded, increment the weight
				current_wt = next_wt;
				current_error = next_error;
				adjustments++;
			} else {
				// failed, back out the attempted increment
				uq_plane2_weights[i] = qat->unquantized_value_flt[current_wt];
				break;
			}
		}
		// decrement until error starts increasing
		while (1) {
			int prev_wt = qat->prev_quantized_value[current_wt];
			if (prev_wt == current_wt)
				break;
			uq_plane1_weights[i] = qat->unquantized_value_flt[prev_wt];
			float prev_error;
			COMPUTE_ERROR(prev_error);
			if (prev_error < current_error) {
				// succeeded, decrement the weight
				current_wt = prev_wt;
				current_error = prev_error;
				adjustments++;
			} else {
				// failed, back out the attempted decrement
				uq_plane2_weights[i] = qat->unquantized_value_flt[current_wt];
				break;
			}
		}

		plane2_weight_set8[i] = current_wt;
	}
#endif
	return adjustments;
}

void CalcPartition1(BlockInfo scb[4], const ImageStats &block, int xdim, int ydim, int zdim,
	float mode_cutoff,
	int max_refinement_iters,
	int num_partitions, int partition_id,
	const ErrorWeightBlock *ewb
) {
	static const int free_bits_for_partition_count[5] = { 0, 115 - 4, 111 - 4 - 10, 108 - 4 - 10, 105 - 4 - 10 };
	PartitionTable		pt(xdim, ydim, zdim, num_partitions, partition_id);

	// first, compute ideal weights and endpoint colors, under thre assumption that there is no quantization or decimation going on.
	dynamic_array<EndpointsWeights> eix;
	EndpointsWeights ei(block, pt, ewb);

	// next, compute ideal weights and endpoint colors for every decimation.
	const BlockSizeDescriptor	*bsd	= BlockSizeDescriptor::get(xdim, ydim, zdim);
	temp_array<float>	decimated_quantized_weights(ASTC::MAX_DECIMATION_MODES * ASTC::MAX_WEIGHTS_PER_BLOCK);
	temp_array<float>	decimated_weights(ASTC::MAX_DECIMATION_MODES * ASTC::MAX_WEIGHTS_PER_BLOCK);
	temp_array<float>	flt_quantized_decimated_quantized_weights(2048 * ASTC::MAX_WEIGHTS_PER_BLOCK);
	temp_array<uint8>	u8_quantized_decimated_quantized_weights(2048 * ASTC::MAX_WEIGHTS_PER_BLOCK);

	// for each decimation mode, compute an ideal set of weights (that is, weights computed with the assumption that they are not quantized)
	for (int i = 0; i < bsd->dec.size(); i++) {
		const BlockSizeDescriptor::Decimation	&d = bsd->dec[i];
		if (!d.permit_encode || d.maxprec[0] < 0 || d.percentile > mode_cutoff)
			continue;
		eix[i] = ei;
		eix[i].ComputeIdealWeights(*d.tables, decimated_quantized_weights + i * ASTC::MAX_WEIGHTS_PER_BLOCK, decimated_weights + i * ASTC::MAX_WEIGHTS_PER_BLOCK);
	}

	// compute maximum colors for the endpoints and ideal weights
	// for each endpoint-and-ideal-weight pair, compute the smallest weight value that will result in a color value greater than 1
	float4 min_ep = float4(10);
	for (int i = 0; i < num_partitions; i++) {
		float4 ep = (float4(one) - ei.endpt0[i]) / (ei.endpt1[i] - ei.endpt0[i]);
		if (ep.x > 0.5f && ep.x < min_ep.x)
			min_ep.x = ep.x;
		if (ep.y > 0.5f && ep.y < min_ep.y)
			min_ep.y = ep.y;
		if (ep.z > 0.5f && ep.z < min_ep.z)
			min_ep.z = ep.z;
		if (ep.w > 0.5f && ep.w < min_ep.w)
			min_ep.w = ep.w;
	}

	float min_wt_cutoff = min(min(min_ep.x, min_ep.y), min(min_ep.z, min_ep.w));

	// for each mode, use the angular method to compute a shift
	float weight_low_value[2048];
	float weight_high_value[2048];

	bsd->ComputeAngularEndpoints1(mode_cutoff, decimated_quantized_weights, decimated_weights, weight_low_value, weight_high_value);

	// for each mode (which specifies a decimation and a quantization):
	// * compute number of bits needed for the quantized weights.
	// * generate an optimized set of quantized weights.
	// * compute quantization errors for the mode.

	int		qwt_bitcounts[2048];
	float	qwt_errors[2048];

	for (int i = 0; i < 2048; i++) {
		const BlockSizeDescriptor::Mode	&b = bsd->modes[i];
		if (!bsd->modes[i].permit_encode || bsd->modes[i].dual != 0 || bsd->modes[i].percentile > mode_cutoff) {
			qwt_errors[i] = 1e38f;
			continue;
		}
		if (weight_high_value[i] > 1.02f * min_wt_cutoff)
			weight_high_value[i] = 1.0f;

		int decimation = bsd->modes[i].decimation;

		// compute weight bitcount for the mode
		int bits_used_by_weights	= ISE_bits(bsd->modes[i].qmode, bsd->dec[decimation].tables->num_weights);
		int bitcount				= free_bits_for_partition_count[num_partitions] - bits_used_by_weights;
		if (bitcount <= 0 || bits_used_by_weights < 24 || bits_used_by_weights > 96) {
			qwt_errors[i] = 1e38f;
			continue;
		}
		qwt_bitcounts[i] = bitcount;

		// then, generate the optimized set of weights for the weight mode
		eix[decimation].ComputeIdealWeights(
			*bsd->dec[decimation].tables,
			weight_low_value[i], weight_high_value[i],
			decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * decimation,
			flt_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * i,
			u8_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * i,
			bsd->modes[i].qmode
		);

		// then, compute weight-errors for the weight mode.
		qwt_errors[i] = eix[decimation].CalcError(*bsd->dec[decimation].tables, flt_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * i);
	}

	// for each weighting mode, determine the optimal combination of color endpoint encodings and weight encodings; return results for the 4 best-looking modes
	Format	partition_format_specifiers[4][4];
	int		quantized_weight[4];
	Quant	color_qmode[4];
	Quant	color_qmode_mod[4];
	determine_optimal_set_of_endpoint_formats_to_use(block, pt, ewb, &ei, -1,	// used to flag that we are in single-weight mode
		qwt_bitcounts, qwt_errors, partition_format_specifiers, quantized_weight, color_qmode, color_qmode_mod
	);

	// then iterate over the 4 believed-to-be-best modes to find out which one is actually best
	for (int i = 0; i < 4; i++) {
		uint8 *u8_weight_src;
		int weights_to_copy;

		if (quantized_weight[i] < 0) {
			scb->error_block = 1;
			scb++;
			continue;
		}

		int		decimation			= bsd->modes[quantized_weight[i]].decimation;
		Quant	weight_qmode		= bsd->modes[quantized_weight[i]].qmode;
		const DecimationTable &dt	= *bsd->dec[decimation].tables;

		u8_weight_src	= u8_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * quantized_weight[i];
		weights_to_copy = dt.num_weights;

		// recompute the ideal color endpoints before storing them.
		float4 rgbs_colors[4];
		float4 rgbo_colors[4];
		float2 lum_intervals[4];

		for (int l = 0; l < max_refinement_iters; l++) {
			eix[decimation].RecomputeIdeal(block, pt, dt, weight_qmode, rgbs_colors, rgbo_colors, lum_intervals, u8_weight_src, NULL, -1, ewb);

			// quantize the chosen color

			// store the colors for the block
			for (int j = 0; j < num_partitions; j++) {
				scb->color_formats[j] = pack_color_endpoints(
					eix[decimation].endpt0[j],
					eix[decimation].endpt1[j],
					rgbs_colors[j], rgbo_colors[j], partition_format_specifiers[i][j], scb->color_values[j], color_qmode[i]
				);
			}


			// if all the color endpoint modes are the same, we get a few more bits to store colors; let's see if we can take advantage of this:
			// requantize all the colors and see if the endpoint modes remain the same; if they do, then exploit it
			scb->color_formats_matched = false;

			if ((num_partitions >= 2 && scb->color_formats[0] == scb->color_formats[1] && color_qmode != color_qmode_mod)
			&& (num_partitions == 2 || (scb->color_formats[0] == scb->color_formats[2] && (num_partitions == 3 || (scb->color_formats[0] == scb->color_formats[3]))))
			) {
				uint8	colorvals[4][12];
				Format	color_formats_mod[4];
				for (int j = 0; j < num_partitions; j++) {
					color_formats_mod[j] = pack_color_endpoints(
						eix[decimation].endpt0[j],
						eix[decimation].endpt1[j],
						rgbs_colors[j], rgbo_colors[j], partition_format_specifiers[i][j], colorvals[j], color_qmode_mod[i]
					);
				}
				if (color_formats_mod[0] == color_formats_mod[1]
					&& (num_partitions == 2 || (color_formats_mod[0] == color_formats_mod[2] && (num_partitions == 3 || (color_formats_mod[0] == color_formats_mod[3]))))
				) {
					scb->color_formats_matched = 1;
					for (int j = 0; j < 4; j++)
						for (int k = 0; k < 12; k++)
							scb->color_values[j][k] = colorvals[j][k];
					for (int j = 0; j < 4; j++)
						scb->color_formats[j] = color_formats_mod[j];
				}
			}


			// store header fields
			scb->num_partitions		= num_partitions;
			scb->partition_id		= partition_id;
			scb->color_qmode		= scb->color_formats_matched ? color_qmode_mod[i] : color_qmode[i];
			scb->mode				= quantized_weight[i];
			scb->error_block		= 0;

			if (scb->color_qmode < 4)
				scb->error_block = true;	// should never happen, but cannot prove it impossible.

			// perform a final pass over the weights to try to improve them.
			int adjustments = realign_weights(block,
				ewb, scb,
				u8_weight_src,
				NULL
			);

			if (adjustments == 0)
				break;
		}

		for (int j = 0; j < weights_to_copy; j++)
			scb->plane_weights[0][j] = u8_weight_src[j];

		scb++;
	}
}

void CalcFixedPartition2(BlockInfo scb[4], const ImageStats &block, int xdim, int ydim, int zdim,
	float mode_cutoff,
	int max_refinement_iters,
	int num_partitions, int partition_id,
	int separate_component,
	const ErrorWeightBlock *ewb
) {
	static const int free_bits_for_partition_count[5] = { 0, 113 - 4, 109 - 4 - 10, 106 - 4 - 10, 103 - 4 - 10 };

	PartitionTable		pt(xdim, ydim, zdim, num_partitions, partition_id);

	// first, compute ideal weights and endpoint colors
	EndpointsWeights	ei1;
	EndpointsWeights	ei2;
	dynamic_array<EndpointsWeights>	eix1;
	dynamic_array<EndpointsWeights> eix2;
	Compute2PlanesEndpoints(block, pt, ewb, separate_component, &ei1, &ei2);

	// next, compute ideal weights and endpoint colors for every decimation.
	const BlockSizeDescriptor	*bsd					= BlockSizeDescriptor::get(xdim, ydim, zdim);
	temp_array<float> decimated_quantized_weights(2 * ASTC::MAX_DECIMATION_MODES * ASTC::MAX_WEIGHTS_PER_BLOCK);
	temp_array<float> decimated_weights(2 * ASTC::MAX_DECIMATION_MODES * ASTC::MAX_WEIGHTS_PER_BLOCK);
	temp_array<float> flt_quantized_decimated_quantized_weights(2 * 2048 * ASTC::MAX_WEIGHTS_PER_BLOCK);
	temp_array<uint8> u8_quantized_decimated_quantized_weights(2 * 2048 * ASTC::MAX_WEIGHTS_PER_BLOCK);

	// for each decimation mode, compute an ideal set of weights
	for (int i = 0; i < bsd->dec.size(); i++) {
		const BlockSizeDescriptor::Decimation	&d = bsd->dec[i];
		if (!d.permit_encode || d.maxprec[1] < 0 || d.percentile > mode_cutoff)
			continue;

		eix1[i] = ei1;
		eix2[i] = ei2;
		eix1[i].ComputeIdealWeights(*d.tables, decimated_quantized_weights + (i * 2 + 0) * ASTC::MAX_WEIGHTS_PER_BLOCK, decimated_weights + (i * 2 + 0) * ASTC::MAX_WEIGHTS_PER_BLOCK);
		eix2[i].ComputeIdealWeights(*d.tables, decimated_quantized_weights + (i * 2 + 1) * ASTC::MAX_WEIGHTS_PER_BLOCK, decimated_weights + (i * 2 + 1) * ASTC::MAX_WEIGHTS_PER_BLOCK);
	}

	// compute maximum colors for the endpoints and ideal weights.
	// for each endpoint-and-ideal-weight pair, compute the smallest weight value
	// that will result in a color value greater than 1.

	float4 min_ep1 = float4(10);
	float4 min_ep2 = float4(10);
	for (int i = 0; i < num_partitions; i++) {
		float4 ep1 = (float4(one) - ei1.endpt0[i]) / (ei1.endpt1[i] - ei1.endpt0[i]);
		if (ep1.x > 0.5f && ep1.x < min_ep1.x)
			min_ep1.x = ep1.x;
		if (ep1.y > 0.5f && ep1.y < min_ep1.y)
			min_ep1.y = ep1.y;
		if (ep1.z > 0.5f && ep1.z < min_ep1.z)
			min_ep1.z = ep1.z;
		if (ep1.w > 0.5f && ep1.w < min_ep1.w)
			min_ep1.w = ep1.w;
		float4 ep2 = (float4(one) - ei2.endpt0[i]) / (ei2.endpt1[i] - ei2.endpt0[i]);
		if (ep2.x > 0.5f && ep2.x < min_ep2.x)
			min_ep2.x = ep2.x;
		if (ep2.y > 0.5f && ep2.y < min_ep2.y)
			min_ep2.y = ep2.y;
		if (ep2.z > 0.5f && ep2.z < min_ep2.z)
			min_ep2.z = ep2.z;
		if (ep2.w > 0.5f && ep2.w < min_ep2.w)
			min_ep2.w = ep2.w;
	}

	float min_wt_cutoff1, min_wt_cutoff2;
	switch (separate_component) {
		case 0:		min_wt_cutoff2 = min_ep2.x;	min_ep1.x = 1e30f; break;
		case 1:		min_wt_cutoff2 = min_ep2.y;	min_ep1.y = 1e30f; break;
		case 2:		min_wt_cutoff2 = min_ep2.z;	min_ep1.z = 1e30f; break;
		case 3:		min_wt_cutoff2 = min_ep2.w;	min_ep1.w = 1e30f; break;
		default:	min_wt_cutoff2 = 1e30f;
	}

	min_wt_cutoff1 = min(min(min_ep1.x, min_ep1.y), min(min_ep1.z, min_ep1.w));

	float weight_low_value1[2048];
	float weight_high_value1[2048];
	float weight_low_value2[2048];
	float weight_high_value2[2048];

//	compute_angular_endpoints_2planes(mode_cutoff, bsd, decimated_quantized_weights, decimated_weights, weight_low_value1, weight_high_value1, weight_low_value2, weight_high_value2);

	// for each mode (which specifies a decimation and a quantization):
	// * generate an optimized set of quantized weights.
	// * compute quantization errors for each mode
	// * compute number of bits needed for the quantized weights.

	int		qwt_bitcounts[2048];
	float	qwt_errors[2048];
	for (int i = 0; i < 2048; i++) {
		if (bsd->modes[i].permit_encode == 0 || bsd->modes[i].dual != 1 || bsd->modes[i].percentile > mode_cutoff) {
			qwt_errors[i] = 1e38f;
			continue;
		}
		int decimation				= bsd->modes[i].decimation;
		const DecimationTable	&dt	= *bsd->dec[decimation].tables;

		if (weight_high_value1[i] > 1.02f * min_wt_cutoff1)
			weight_high_value1[i] = 1.0f;
		if (weight_high_value2[i] > 1.02f * min_wt_cutoff2)
			weight_high_value2[i] = 1.0f;

		// compute weight bitcount for the mode
		int bits_used_by_weights	= ISE_bits(bsd->modes[i].qmode, 2 * dt.num_weights);
		int bitcount				= free_bits_for_partition_count[num_partitions] - bits_used_by_weights;
		if (bitcount <= 0 || bits_used_by_weights < 24 || bits_used_by_weights > 96) {
			qwt_errors[i] = 1e38f;
			continue;
		}
		qwt_bitcounts[i] = bitcount;

		// then, generate the optimized set of weights for the mode
		eix1[decimation].ComputeIdealWeights(
			dt,
			weight_low_value1[i],
			weight_high_value1[i],
			decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * decimation),
			flt_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * i),
			u8_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * i), bsd->modes[i].qmode
		);

		eix2[decimation].ComputeIdealWeights(
			dt,
			weight_low_value2[i],
			weight_high_value2[i],
			decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * decimation + 1),
			flt_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * i + 1),
			u8_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * i + 1), bsd->modes[i].qmode
		);

		// then, compute quantization errors for the block mode
		qwt_errors[i] = eix1[decimation].CalcError(
			dt,
			flt_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * i))
			+ eix2[decimation].CalcError(dt, flt_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * i + 1)
		);
	}


	// decide the optimal combination of color endpoint encodings and weight encoodings.
	Format	partition_format_specifiers[4][4];
	int		quantized_weight[4];
	Quant	color_qmode[4];
	Quant	color_qmode_mod[4];

	Endpoints epm = ei1.Merge(ei1, separate_component);

	determine_optimal_set_of_endpoint_formats_to_use(block, pt, ewb,
		&epm, separate_component, qwt_bitcounts, qwt_errors, partition_format_specifiers, quantized_weight, color_qmode, color_qmode_mod
	);

	for (int i = 0; i < 4; i++) {
		if (quantized_weight[i] < 0) {
			scb->error_block = 1;
			scb++;
			continue;
		}

		int		decimation			= bsd->modes[quantized_weight[i]].decimation;
		Quant	weight_qmode		= bsd->modes[quantized_weight[i]].qmode;
		const DecimationTable &dt	= *bsd->dec[decimation].tables;
		uint8	*u8_weight1_src		= u8_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * quantized_weight[i]);
		uint8	*u8_weight2_src		= u8_quantized_decimated_quantized_weights + ASTC::MAX_WEIGHTS_PER_BLOCK * (2 * quantized_weight[i] + 1);

		int		weights_to_copy = dt.num_weights;

		// recompute the ideal color endpoints before storing them.
		epm = eix1[decimation].Merge(eix2[decimation], separate_component);

		float4 rgbs_colors[4];
		float4 rgbo_colors[4];
		float2 lum_intervals[4];

		for (int l = 0; l < max_refinement_iters; l++) {
			epm.RecomputeIdeal(block, pt, dt, weight_qmode, rgbs_colors, rgbo_colors, lum_intervals, u8_weight1_src, u8_weight2_src, separate_component, ewb);

			// store the colors for the block
			for (int j = 0; j < num_partitions; j++) {
				scb->color_formats[j] = pack_color_endpoints(
					epm.endpt0[j],
					epm.endpt1[j],
					rgbs_colors[j], rgbo_colors[j], partition_format_specifiers[i][j], scb->color_values[j], color_qmode[i]
				);
			}
			scb->color_formats_matched = false;

			if ((num_partitions >= 2 && scb->color_formats[0] == scb->color_formats[1]
				&& color_qmode != color_qmode_mod)
				&& (num_partitions == 2 || (scb->color_formats[0] == scb->color_formats[2] && (num_partitions == 3 || (scb->color_formats[0] == scb->color_formats[3]))))
			) {
				uint8	colorvals[4][12];
				Format	color_formats_mod[4];
				for (int j = 0; j < num_partitions; j++) {
					color_formats_mod[j] = pack_color_endpoints(
						epm.endpt0[j],
						epm.endpt1[j],
						rgbs_colors[j], rgbo_colors[j], partition_format_specifiers[i][j], colorvals[j], color_qmode_mod[i]
					);
				}
				if (color_formats_mod[0] == color_formats_mod[1]
					&& (num_partitions == 2 || (color_formats_mod[0] == color_formats_mod[2] && (num_partitions == 3 || (color_formats_mod[0] == color_formats_mod[3]))))) {
					scb->color_formats_matched = 1;
					for (int j = 0; j < 4; j++)
						for (int k = 0; k < 12; k++)
							scb->color_values[j][k] = colorvals[j][k];
					for (int j = 0; j < 4; j++)
						scb->color_formats[j] = color_formats_mod[j];
				}
			}


			// store header fields
			scb->num_partitions = num_partitions;
			scb->partition_id	= partition_id;
			scb->color_qmode	= scb->color_formats_matched ? color_qmode_mod[i] : color_qmode[i];
			scb->mode			= quantized_weight[i];
			scb->plane2_color_component = separate_component;
			scb->error_block	= false;

			if (scb->color_qmode < 4) {
				scb->error_block = 1;	// should never happen, but cannot prove it impossible
			}

			int adjustments = realign_weights(
				block, ewb, scb,
				u8_weight1_src,
				u8_weight2_src
			);

			if (adjustments == 0)
				break;
		}

		for (int j = 0; j < weights_to_copy; j++) {
			scb->plane_weights[0][j] = u8_weight1_src[j];
			scb->plane_weights[1][j] = u8_weight2_src[j];
		}

		scb++;
	}
}

float CalcDifference(const block<ISO_rgba, 3> &block, BlockInfo &bi, int xdim, int ydim, int zdim) {
	return 0;
}

void find_best_partitionings(
	int partition_search_limit, int xdim, int ydim, int zdim, int partition_count,
	const ImageStats &block, const ErrorWeightBlock *ewb, int candidates_to_return,
	int *best_partitions_uncorrellated,
	int *best_partitions_samechroma,
	int *best_partitions_dual_weight_planes
) {
}

//float compress_symbolic_block(const astc_codec_image * input_image, astc_decode_mode decode_mode, int xdim, int ydim, int zdim, const error_weighting_params * params, const imageblock * blk, symbolic_compressed_block * scb)
BlockInfo::BlockInfo(const block<ISO_rgba, 3> &block, int xdim, int ydim, int zdim, CompressionParams *params) : mode(0), error_block(false) {
	ImageStats	stats(block, xdim, ydim, zdim);

	if (all(stats.a == stats.b)) {
		// constant-color block
		num_partitions	= 0;
		const_color		= to<uint16>(stats.a * 65535 + 0.5f);
		return;// 0.0f;
	}

	ErrorWeightBlock		*ewb	= new ErrorWeightBlock(block, xdim, ydim, zdim, 0, 0, 0, params);

	float best_errorvals_in_modes[17];
	for (int i = 0; i < 17; i++)
		best_errorvals_in_modes[i] = 1e30f;

	bool	uses_alpha			= stats.HasAlpha();
	float	mode_cutoff			= params->block_mode_cutoff;
	float	error_of_best_block = 1e20f;
	float	error_weight_sum	= ewb->total;

	BlockInfo tempblocks[4];

	// Test mode #0. This mode uses 1 plane of weights and 1 partition.
	float modecutoffs[2]	= { 0, mode_cutoff };
	float errorval_mult[2]	= { 2.5, 1 };

	float best_errorval_in_mode;
	for (int i = 0; i < 2; i++) {
		CalcPartition1(tempblocks, stats, xdim, ydim, zdim, modecutoffs[i], params->max_refinement_iters, 1, 0, ewb);

		best_errorval_in_mode = 1e30f;
		for (int j = 0; j < 4; j++) {
			if (tempblocks[j].error_block)
				continue;

			float	errorval = CalcDifference(block, tempblocks[j], xdim, ydim, zdim) * errorval_mult[i];

			if (errorval < best_errorval_in_mode)
				best_errorval_in_mode = errorval;

			if (errorval < error_of_best_block) {
				error_of_best_block = errorval;
				*this = tempblocks[j];
			}
		}

		best_errorvals_in_modes[0] = best_errorval_in_mode;
		if ((error_of_best_block / error_weight_sum) < params->texel_avg_error_limit)
			goto END_OF_TESTS;
	}

	bool	is_normal_map;
	float	lowest_correl;
	ewb->PrepareStatistics(stats, &is_normal_map, &lowest_correl);

	if (is_normal_map && lowest_correl < 0.99f)
		lowest_correl = 0.99f;

	// next, test the four possible 1-partition, 2-planes modes
	for (int i = 0; i < 4; i++) {
		if (lowest_correl > params->lowest_correlation_cutoff)
			continue;

		if (stats.grayscale && i != 3)
			continue;

		if (!uses_alpha && i == 3)
			continue;

		CalcFixedPartition2(tempblocks, stats, xdim, ydim, zdim, mode_cutoff, params->max_refinement_iters, 1, 0, i, ewb);

		for (int j = 0; j < 4; j++) {
			if (tempblocks[j].error_block)
				continue;

			float	errorval = CalcDifference(block, tempblocks[j], xdim, ydim, zdim);

			if (errorval < best_errorval_in_mode)
				best_errorval_in_mode = errorval;

			if (errorval < error_of_best_block) {
				error_of_best_block = errorval;
				*this = tempblocks[j];
			}

			best_errorvals_in_modes[i + 1] = best_errorval_in_mode;
		}

		if ((error_of_best_block / error_weight_sum) < params->texel_avg_error_limit)
			goto END_OF_TESTS;
	}

	// find best blocks for 2, 3 and 4 partitions
	int num_partitions;
	for (num_partitions = 2; num_partitions <= 4; num_partitions++) {
		int partition_indices_1plane[2];
		int partition_indices_2planes[2];

		find_best_partitionings(params->partition_search_limit,
			xdim, ydim, zdim, num_partitions, stats, ewb, 1,
			&(partition_indices_1plane[0]), &(partition_indices_1plane[1]), &(partition_indices_2planes[0]));

		for (int i = 0; i < 2; i++) {
			CalcPartition1(tempblocks, stats, xdim, ydim, zdim, mode_cutoff, params->max_refinement_iters, num_partitions, partition_indices_1plane[i], ewb);

			best_errorval_in_mode = 1e30f;
			for (int j = 0; j < 4; j++) {
				if (tempblocks[j].error_block)
					continue;

				float	errorval = CalcDifference(block, tempblocks[j], xdim, ydim, zdim);

				if (errorval < best_errorval_in_mode)
					best_errorval_in_mode = errorval;

				if (errorval < error_of_best_block) {
					error_of_best_block = errorval;
					*this = tempblocks[j];
				}
			}

			best_errorvals_in_modes[4 * (num_partitions - 2) + 5] = best_errorval_in_mode;

			if ((error_of_best_block / error_weight_sum) < params->texel_avg_error_limit)
				goto END_OF_TESTS;
		}


		if (num_partitions == 2 && !is_normal_map && min(best_errorvals_in_modes[5], best_errorvals_in_modes[6]) > (best_errorvals_in_modes[0] * params->partition_1_to_2_limit))
			goto END_OF_TESTS;

		// don't bother to check 4 partitions for dual plane of weights, ever
		if (num_partitions == 4)
			break;

		for (int i = 0; i < 2; i++) {
			if (lowest_correl > params->lowest_correlation_cutoff)
				continue;
			CalcPartition1(tempblocks, stats,
				xdim, ydim, zdim,
				mode_cutoff,
				params->max_refinement_iters,
				partition_indices_2planes[i] & 1023, partition_indices_2planes[i] >> 10,
				ewb
			);

			best_errorval_in_mode = 1e30f;
			for (int j = 0; j < 4; j++) {
				if (tempblocks[j].error_block)
					continue;
				float	errorval = CalcDifference(block, tempblocks[j], xdim, ydim, zdim);

				if (errorval < best_errorval_in_mode)
					best_errorval_in_mode = errorval;

				if (errorval < error_of_best_block) {
					error_of_best_block = errorval;
					*this = tempblocks[j];
				}
			}

			best_errorvals_in_modes[4 * (num_partitions - 2) + 5 + 2] = best_errorval_in_mode;

			if ((error_of_best_block / error_weight_sum) < params->texel_avg_error_limit)
				goto END_OF_TESTS;
		}
	}

END_OF_TESTS:
#if 0
	// compress/decompress to a physical block
	physical_compressed_block psb = symbolic_to_physical(xdim, ydim, zdim, scb);
	physical_to_symbolic(xdim, ydim, zdim, psb, scb);

	// mean squared error per color component.
	//return error_of_best_block / ((float)xdim * ydim * zdim);
#endif
	;
}


uint16x4 lerp_color_int(uint16x4 color0, uint16x4 color1, int weight, int plane2_weight, int plane2_color_component) {
	int32x4 eweight1(weight);
	eweight1[plane2_color_component] = plane2_weight;
	
	return to<uint16>((to<int>(color0) * (64 - eweight1) + to<int>(color1) * eweight1 + 32) / 64);
}

void ASTC::Decode(int X, int Y, int Z, const block<ISO_rgba, 3> &block) const {
	BlockInfo	b(*this, X, Y, Z);

	// if we detected an error-block, blow up immediately.
	if (b.error_block) {
		fill(block, 0);
		return;
	}

	if (b.num_partitions == 0) {
		fill(block, ISO_rgba(b.const_color.x>>8, b.const_color.y>>8, b.const_color.z>>8, b.const_color.w>>8));
		return;
	}

	// get the appropriate partition-table entry
	PartitionTable				pt(X, Y, Z, b.num_partitions, b.partition_id);

	// get the appropriate block descriptor
	const BlockSizeDescriptor	*bsd	= BlockSizeDescriptor::get(X, Y, Z);
	const DecimationTable		&dt		= *bsd->dec[bsd->modes[b.mode].decimation].tables;
	bool						dual	= bsd->modes[b.mode].dual;
	Quant						qmode	= bsd->modes[b.mode].qmode;

	// decode the color endpoints
	uint16x4		color_endpoint[4][2];
	for (int i = 0; i < b.num_partitions; i++)
		unpack_color_endpoints(b.color_formats[i], b.color_qmode, b.color_values[i], color_endpoint[i]);

	// first unquantize the weights
	int		uq_weights[2][MAX_WEIGHTS_PER_BLOCK];
	int		weight_count = dt.num_weights;

	for (int i = 0; i < weight_count; i++)
		uq_weights[0][i] = UnQuantWgt(qmode, b.plane_weights[0][i]);

	if (dual) {
		for (int i = 0; i < weight_count; i++)
			uq_weights[1][i] = UnQuantWgt(qmode, b.plane_weights[1][i]);
	}

	// then un-decimate them.
	int num_texels = X * Y * Z;
	int weights[2][MAX_TEXELS_PER_BLOCK];
	for (int i = 0; i < num_texels; i++)
		weights[0][i] = dt.texels[i].compute(uq_weights[0]);

	if (dual) {
		for (int i = 0; i < num_texels; i++)
			weights[1][i] = dt.texels[i].compute(uq_weights[1]);
	}

	// now that we have endpoint colors and weights, we can unpack actual colors for each texel.
	uint16x4	output[MAX_TEXELS_PER_BLOCK];
	int		plane2_color_component = dual ? b.plane2_color_component : -1;
	for (int i = 0; i < num_texels; i++) {
		int partition	= pt.partition[i];
		output[i] = lerp_color_int(
			color_endpoint[partition][0], color_endpoint[partition][1],
			weights[0][i],				weights[1][i],
			plane2_color_component
		);
	}

	for (int z = 0, i = 0; z < block.size<3>(); z++) {
		for (int y = 0; y < block.size<2>(); y++) {
			for (int x = 0; x < block.size<1>(); x++, i++) {
				block[z][y][x] = ISO_rgba(output[i].x, output[i].y, output[i].z, output[i].w);
			}
		}
	}
}

void ASTC::Encode(int X, int Y, int Z, const block<ISO_rgba, 3> &block, CompressionParams *params) {
	BlockInfo	b(block, X, Y, Z, params);

	if (b.num_partitions == 0) {
		if (b.mode & 0x200) {
			vx2d.mode	= 0x1fc;
			vx2d.rsvd	= 3;
			vx2d.x0		= vx2d.x1 = vx2d.y0 = vx2d.y1 = 0x1fff;
		} else {
			vx3d.mode	= 0x3fc;
			vx3d.x0		= vx3d.x1 = vx3d.z1 = vx3d.y0 = vx3d.y1 = vx3d.z1 = 0x1ff;
		}
		for (int i = 0; i < 4; i++)
			const_col[i] = b.const_color[i];
		return;
	}

	// first, compress the weights. They are encoded as an ordinary integer-sequence, then bit-reversed
	uint8 weightbuf[16];
	clear(weightbuf);

	const BlockSizeDescriptor	*bsd	= BlockSizeDescriptor::get(X, Y, Z);
	const DecimationTable		*it		= bsd->dec[bsd->modes[b.mode].decimation].tables;
	bool						dual	= bsd->modes[b.mode].dual;
	Quant						qmode	= bsd->modes[b.mode].qmode;

	int		weight_count		= it->num_weights;
	int		real_weight_count	= weight_count << int(dual);
	int		bits_for_weights	= ISE_bits(qmode, real_weight_count);
	int		below_weights_pos	= 128 - bits_for_weights;

	if (dual) {
		uint8 weights[64];
		for (int i = 0; i < weight_count; i++) {
			weights[2 * i + 0] = b.plane_weights[0][i];
			weights[2 * i + 1] = b.plane_weights[1][i];
		}
		ISE_encode(qmode, real_weight_count, weights, weightbuf, 0);
	} else {
		ISE_encode(qmode, weight_count, b.plane_weights[0], weightbuf, 0);
	}

	for (int i = 0; i < 16; i++)
		data[i] = reverse_bits(weightbuf[15 - i]);

	mode			= b.mode;
	num_partitions	= b.num_partitions - 1;

	// encode partition index and color endpoint types for blocks with 2 or more partitions.
	if (num_partitions > 1) {
		partition_id = b.partition_id;

		if (b.color_formats_matched) {
			mode_bits = b.color_formats[0] << 2;
		} else {
			// go through the selected endpoint type classes for each partition in order to determine the lowest class present.
			int low_class = 4;
			for (int i = 0; i < num_partitions; i++) {
				int class_of_format = b.color_formats[i] >> 2;
				if (class_of_format < low_class)
					low_class = class_of_format;
			}

			if (low_class == 3)
				low_class = 2;

			int encoded_type	= low_class + 1;
			int bitpos			= 2;
			for (int i = 0; i < num_partitions; i++) {
				int classbit_of_format = (b.color_formats[i] >> 2) - low_class;
				encoded_type |= classbit_of_format << bitpos;
				bitpos++;
			}
			for (int i = 0; i < num_partitions; i++) {
				int lowbits_of_format = b.color_formats[i] & 3;
				encoded_type |= lowbits_of_format << bitpos;
				bitpos += 2;
			}
			int encoded_type_lowpart		= encoded_type & 0x3F;
			int encoded_type_highpart		= encoded_type >> 6;
			int encoded_type_highpart_size	= (3 * num_partitions) - 4;
			int encoded_type_highpart_pos	= 128 - bits_for_weights - encoded_type_highpart_size;
			write_bits(data, encoded_type_lowpart, 13 + 10,  6);
			write_bits(data, encoded_type_highpart, encoded_type_highpart_pos, encoded_type_highpart_size);

			below_weights_pos -= encoded_type_highpart_size;
		}
	} else {
		col_fmt = b.color_formats[0];
	}

	// in dual-plane mode, encode the color component of the second plane of weights
	if (dual)
		write_bits(data, b.plane2_color_component,  below_weights_pos - 2,  2);

	// finally, encode the color bits
	// first, get hold of all the color components to encode
	uint8	values_to_encode[32];
	int		valuecount_to_encode = 0;
	for (int i = 0; i < b.num_partitions; i++) {
		int vals = 2 * (b.color_formats[i] >> 2) + 2;
		for (int j = 0; j < vals; j++)
			values_to_encode[j + valuecount_to_encode] = b.color_values[i][j];
		valuecount_to_encode += vals;
	}

	// then, encode an ISE based on them.
	ISE_encode(b.color_qmode, valuecount_to_encode, values_to_encode, data, (b.num_partitions == 1 ? 17 : 19 + 10));
}


}//namespace iso
