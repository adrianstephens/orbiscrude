#include "movie.h"
#include "iso/iso_files.h"
#include "codec/vlc.h"
#include "maths/dct.h"
#include "comms/http.h"

using namespace iso;

typedef vlc_in<uint32, true>	vlc_reader;
typedef vlc_out<uint32, true>	vlc_writer;

struct VLCtab {	int8 val, len; };
struct DCTtab { int8 run, level, len; };

class MPEG {
	static const uint8	default_intra_matrix[];
	static const uint8	ZAG[];
	static const uint8	ALT[];
	static VLCtab		DClumtab0[], DClumtab1[];
	static VLCtab		DCchromtab0[], DCchromtab1[];
	static DCTtab		DCTtabnext[], DCTtab0[], DCTtab0a[], DCTtab1[], DCTtab1a[], DCTtab2[], DCTtab3[], DCTtab4[], DCTtab5[], DCTtab6[];

	int					DCy, DCcr, DCcb, qscale;
	const uint8			*scan;

public:
	MPEG() : DCy(0), DCcr(0), DCcb(0), qscale(0), scan(ZAG)	{}

	void		SetScan(bool altscan) {	scan = altscan ? ALT : ZAG;	}

	void		MB2Bitmap(bitmap &bm, int x, int y, short *blocks);
	void		Bitmap2MB(bitmap &bm, int x, int y, short *blocks);

	static int	FindVLC(VLCtab *tab, size_t n, int val);
	static int	FindDCT(DCTtab *tab, size_t n, int run, int level);
	static void PutCode(vlc_writer &vlc, int val, int len, int nbits) {	vlc.put(val >> (len - nbits), nbits); }

	static int	GetLuminanceDC(vlc_reader &vlc);
	static int	GetChrominanceDC(vlc_reader &vlc);
	bool		GetAC1(vlc_reader &vlc, short *block, const uint8 *qmatrix, int qscale);
	bool		GetAC2(vlc_reader &vlc, short *block, const uint8 *qmatrix, int qscale, bool intravlc);

	static void PutLuminanceDC(vlc_writer &vlc, int val);
	static void PutChrominanceDC(vlc_writer &vlc, int val);
	void		PutAC1(vlc_writer &vlc, short *block, const uint8 *qmatrix, int qscale);
	void		PutAC2(vlc_writer &vlc, short *block, const uint8 *qmatrix, int qscale, bool intravlc);

	void		MBRead(vlc_reader &vlc, short blocks[6][8*8], bool dtd, bool intravlc);
	void		MBWrite(vlc_writer &vlc, short blocks[6][8*8], int qscl, bool dtd, bool intravlc);
};

const uint8 MPEG::default_intra_matrix[] = {
	8,  16, 19, 22, 26, 27, 29, 34,
	16, 16, 22, 24, 27, 29, 34, 37,
	19, 22, 26, 27, 29, 34, 34, 38,
	22, 22, 26, 27, 29, 34, 37, 40,
	22, 26, 27, 29, 32, 35, 40, 48,
	26, 27, 29, 32, 35, 40, 48, 58,
	26, 27, 29, 34, 38, 46, 56, 69,
	27, 29, 35, 38, 46, 56, 69, 83
};

const uint8 MPEG::ZAG[8*8] = {
	  0,  1,  8, 16,  9,  2,  3, 10,
	 17, 24, 32, 25, 18, 11,  4,  5,
	 12, 19, 26, 33, 40, 48, 41, 34,
	 27, 20, 13,  6,  7, 14, 21, 28,
	 35, 42, 49, 56, 57, 50, 43, 36,
	 29, 22, 15, 23, 30, 37, 44, 51,
	 58, 59, 52, 45, 38, 31, 39, 46,
	 53, 60, 61, 54, 47, 55, 62, 63,
};

const uint8 MPEG::ALT[8*8] = {
	 0,  8, 16, 24,  1,  9,  2, 10,
	17, 25, 32, 40, 48, 56, 57, 49,
	41, 33, 26, 18,  3, 11,  4, 12,
	19, 27, 34, 42, 50, 58, 35, 43,
	51, 59, 20, 28,  5, 13,  6, 14,
	21, 29, 36, 44, 52, 60, 37, 45,
	53, 61, 22, 30,  7, 15, 23, 31,
	38, 46, 54, 62, 39, 47, 55, 63
};

// Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110
VLCtab MPEG::DClumtab0[32] = {
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
	{0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
	{4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {-1, 0}
};

// Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111
VLCtab MPEG::DClumtab1[16] = {
	{7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
	{8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9}
};

// Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110
VLCtab MPEG::DCchromtab0[32] = {
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
	{3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {-1, 0}
};

// Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111
VLCtab MPEG::DCchromtab1[32] = {
	{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
	{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
	{7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
	{8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10}
};

// Table B-14, DCT coefficients table zero, codes 0100 ... 1xxx (used for first (DC) coefficient)
DCTtab DCTtabfirst[12] = {
	{0,2,4}, {2,1,4}, {1,1,3}, {1,1,3},
	{0,1,1}, {0,1,1}, {0,1,1}, {0,1,1},
	{0,1,1}, {0,1,1}, {0,1,1}, {0,1,1}
};

// Table B-14, DCT coefficients table zero, codes 0100 ... 1xxx (used for all other coefficients)
DCTtab MPEG::DCTtabnext[12] = {
	{0,2,4},  {2,1,4},  {1,1,3},  {1,1,3},
	{64,0,2}, {64,0,2}, {64,0,2}, {64,0,2}, // EOB
	{0,1,2},  {0,1,2},  {0,1,2},  {0,1,2}
};

// Table B-14, DCT coefficients table zero, codes 000001xx ... 00111xxx
DCTtab MPEG::DCTtab0[60] = {
	{65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, // Escape
	{2,2,7}, {2,2,7}, {9,1,7}, {9,1,7},
	{0,4,7}, {0,4,7}, {8,1,7}, {8,1,7},
	{7,1,6}, {7,1,6}, {7,1,6}, {7,1,6},
	{6,1,6}, {6,1,6}, {6,1,6}, {6,1,6},
	{1,2,6}, {1,2,6}, {1,2,6}, {1,2,6},
	{5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
	{13,1,8}, {0,6,8}, {12,1,8}, {11,1,8},
	{3,2,8}, {1,3,8}, {0,5,8}, {10,1,8},
	{0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
	{0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
	{4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
	{4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
	{3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
	{3,1,5}, {3,1,5}, {3,1,5}, {3,1,5}
};

// Table B-15, DCT coefficients table one, codes 000001xx ... 11111111
DCTtab MPEG::DCTtab0a[252] = {
	{65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, // Escape
	{7,1,7}, {7,1,7}, {8,1,7}, {8,1,7},
	{6,1,7}, {6,1,7}, {2,2,7}, {2,2,7},
	{0,7,6}, {0,7,6}, {0,7,6}, {0,7,6},
	{0,6,6}, {0,6,6}, {0,6,6}, {0,6,6},
	{4,1,6}, {4,1,6}, {4,1,6}, {4,1,6},
	{5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
	{1,5,8}, {11,1,8}, {0,11,8}, {0,10,8},
	{13,1,8}, {12,1,8}, {3,2,8}, {1,4,8},
	{2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
	{2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
	{1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
	{1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
	{3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
	{3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	{64,0,4}, {64,0,4}, {64,0,4}, {64,0,4}, // EOB
	{64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
	{64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
	{64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
	{0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	{0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	{0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	{0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	{0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
	{0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
	{0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
	{0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
	{9,1,7}, {9,1,7}, {1,3,7}, {1,3,7},
	{10,1,7}, {10,1,7}, {0,8,7}, {0,8,7},
	{0,9,7}, {0,9,7}, {0,12,8}, {0,13,8},
	{2,3,8}, {4,2,8}, {0,14,8}, {0,15,8}
};

// Table B-14, DCT coefficients table zero, codes 0000001000 ... 0000001111
DCTtab MPEG::DCTtab1[8] = {
	{16,1,10}, {5,2,10}, {0,7,10}, {2,3,10},
	{1,4,10}, {15,1,10}, {14,1,10}, {4,2,10}
};

// Table B-15, DCT coefficients table one, codes 000000100x ... 000000111x
DCTtab MPEG::DCTtab1a[8] = {
	{5,2,9}, {5,2,9}, {14,1,9}, {14,1,9},
	{2,4,10}, {16,1,10}, {15,1,9}, {15,1,9}
};

// Table B-14/15, DCT coefficients table zero / one, codes 000000010000 ... 000000011111
DCTtab MPEG::DCTtab2[16] = {
	{0,11,12}, {8,2,12}, {4,3,12}, {0,10,12},
	{2,4,12}, {7,2,12}, {21,1,12}, {20,1,12},
	{0,9,12}, {19,1,12}, {18,1,12}, {1,5,12},
	{3,3,12}, {0,8,12}, {6,2,12}, {17,1,12}
};

// Table B-14/15, DCT coefficients table zero / one, codes 0000000010000 ... 0000000011111
DCTtab MPEG::DCTtab3[16] = {
	{10,2,13}, {9,2,13}, {5,3,13}, {3,4,13},
	{2,5,13}, {1,7,13}, {1,6,13}, {0,15,13},
	{0,14,13}, {0,13,13}, {0,12,13}, {26,1,13},
	{25,1,13}, {24,1,13}, {23,1,13}, {22,1,13}
};

// Table B-14/15, DCT coefficients table zero / one, codes 00000000010000 ... 00000000011111
DCTtab MPEG::DCTtab4[16] = {
	{0,31,14}, {0,30,14}, {0,29,14}, {0,28,14},
	{0,27,14}, {0,26,14}, {0,25,14}, {0,24,14},
	{0,23,14}, {0,22,14}, {0,21,14}, {0,20,14},
	{0,19,14}, {0,18,14}, {0,17,14}, {0,16,14}
};

// Table B-14/15, DCT coefficients table zero / one, codes 000000000010000 ... 000000000011111
DCTtab MPEG::DCTtab5[16] = {
	{0,40,15}, {0,39,15}, {0,38,15}, {0,37,15},
	{0,36,15}, {0,35,15}, {0,34,15}, {0,33,15},
	{0,32,15}, {1,14,15}, {1,13,15}, {1,12,15},
	{1,11,15}, {1,10,15}, {1,9,15}, {1,8,15}
};

// Table B-14/15, DCT coefficients table zero / one, codes 0000000000010000 ... 0000000000011111
DCTtab MPEG::DCTtab6[16] = {
	{1,18,16}, {1,17,16}, {1,16,16}, {1,15,16},
	{6,3,16}, {16,2,16}, {15,2,16}, {14,2,16},
	{13,2,16}, {12,2,16}, {11,2,16}, {31,1,16},
	{30,1,16}, {29,1,16}, {28,1,16}, {27,1,16}
};

//-----------------------------------------------------------------------------
//	COLOURSPACE CONVERSION
//-----------------------------------------------------------------------------

void MPEG::MB2Bitmap(bitmap &bm, int x, int y, short *blocks) {
	for (int j = 0; j < 16; j++ ) {
		ISO_rgba	*dest	= bm.ScanLine(y + j) + x;
		short		*ys		= blocks + (j + (j & 8)) * 8;
		short		*uvs	= blocks + 4 * 8*8 + j/2 * 8;
		for (int i = 0; i < 4; i++, dest+=2, uvs++, ys+=2 ) {
			dest[0]		= ISO_rgba::YCbCr(ys[0]		+ 128, uvs[0], uvs[8*8]);
			dest[1]		= ISO_rgba::YCbCr(ys[1]		+ 128, uvs[0], uvs[8*8]);
			dest[8+0]	= ISO_rgba::YCbCr(ys[8*8]	+ 128, uvs[4], uvs[8*8+4]);
			dest[8+1]	= ISO_rgba::YCbCr(ys[8*8+1]	+ 128, uvs[4], uvs[8*8+4]);
		}
	}
}

void MPEG::Bitmap2MB(bitmap &bm, int x, int y, short *blocks) {
	int8	compdata[3][16*16];

	for (int j = 0; j < 16; j++ ) {
		ISO_rgba	*srce	= bm.ScanLine(y + j) + x;
		int			d		= j * 16;
		for (int i = 0; i < 16; i++, d++ ) {
			ISO_rgba	&c = *srce++;
			compdata[2][d] = (int)( 0.50000 * c.r - 0.41869 * c.g - 0.08131 * c.b);		// Cr
			compdata[1][d] = (int)(-0.16874 * c.r - 0.33126 * c.g + 0.50000 * c.b);		// Cb
			compdata[0][d] = (int)( 0.29900 * c.r + 0.58700 * c.g + 0.11400 * c.b)-128;	// Y-128
		}
	}

	for (int i = 0; i < 4; i++) {
		int8	*srce = compdata[0] + (i&1) * 8 + (i&2) * 64;
		for (y = 0; y < 8; y++, srce += 16)
			for (x = 0; x < 8; x++)
				*blocks++ = srce[x];

	}

	int8	*srce = compdata[1];
	for (int i = 0; i < 2; i++)
		for (y = 0; y < 8; y++, srce += 16 * 2)
			for (x = 0; x < 8; x++)
				*blocks++ = (srce[x*2] + srce[x*2+1] + srce[x*2+16] + srce[x*2+16+1] + 2) / 4;

}

//-----------------------------------------------------------------------------
//	HUFFMAN BLOCK DECODE
//-----------------------------------------------------------------------------

void MPEG::MBRead(vlc_reader &vlc, short blocks[6][8*8], bool dtd, bool intravlc) {
	int	mbt = vlc.peek(2);
	if (!mbt)
		mbt = 1;

	vlc.discard(mbt > 1 ? 1 : 2);

	if (dtd) {
		int	dt = vlc.get(1);
	}

	if (mbt == 1)
		qscale = vlc.get(5);

	for (int i = 0; i < 4; i++) {
		blocks[i][0] = (DCy += GetLuminanceDC(vlc)) << 3;
		GetAC2(vlc, blocks[i], default_intra_matrix, qscale, intravlc);
	};
	blocks[4][0] = (DCcr += GetChrominanceDC(vlc)) << 3;
	GetAC2(vlc, blocks[4], default_intra_matrix, qscale, intravlc);
	blocks[5][0] = (DCcb += GetChrominanceDC(vlc)) << 3;
	GetAC2(vlc, blocks[5], default_intra_matrix, qscale, intravlc);

	for (int i = 0; i < 6; i++)
		jpeg_idct(blocks[i]);

}

void MPEG::MBWrite(vlc_writer &vlc, short blocks[6][8*8], int qscl, bool dtd, bool intravlc) {
	if (qscl == qscale)
		vlc.put(1,1);
	else
		vlc.put(1,2);

	if (dtd)
		vlc.put(0, 1);

	if (qscl != qscale)
		vlc.put(qscale = qscl, 5);

	for (int i = 0; i < 6; i++)
		jpeg_idct(blocks[i]);

	for (int i = 0; i < 4; i++) {
		PutLuminanceDC(vlc, (blocks[i][0]>>3) - DCy);
		DCy = blocks[i][0]>>3;
		PutAC2(vlc, blocks[i], default_intra_matrix, qscale, intravlc);
	};

	PutChrominanceDC(vlc, (blocks[4][0]>>3) - DCcr);
	DCcr = blocks[4][0]>>3;
	PutAC2(vlc, blocks[4], default_intra_matrix, qscale, intravlc);

	PutChrominanceDC(vlc, (blocks[5][0]>>3) - DCcb);
	DCcb = blocks[5][0]>>3;
	PutAC2(vlc, blocks[5], default_intra_matrix, qscale, intravlc);
}

//-----------------------------------------------------------------------------
//	HUFFMAN BLOCK DECODE
//-----------------------------------------------------------------------------

int MPEG::FindVLC(VLCtab *tab, size_t n, int val) {
	for (size_t i = 0; i < n; i++)
		if (tab[i].val == val)
			return int(i);
	return -1;
}

int MPEG::FindDCT(DCTtab *tab, size_t n, int run, int level) {
	for (size_t i = 0; i < n; i++)
		if (tab[i].run == run && tab[i].level == level)
			return int(i);
	return -1;
}

int MPEG::GetLuminanceDC(vlc_reader &vlc) {
	int code, size, val;

	// decode length
	code = vlc.peek(5);

	if (code < 31) {
		size = DClumtab0[code].val;
		vlc.discard(DClumtab0[code].len);
	} else {
		code = vlc.peek(9) - 0x1f0;
		size = DClumtab1[code].val;
		vlc.discard(DClumtab1[code].len);
	}

	if (size==0)
		val = 0;
	else {
		val = vlc.get(size);
		if ((val & (1<<(size-1)))==0)
			val -= (1<<size) - 1;
	}

	return val;
}

void MPEG::PutLuminanceDC(vlc_writer &vlc, int val) {
	int code, size, t;

	for (size = 0, t = abs(val); t; size++, t>>=1);		// Find the number of bits needed for the magnitude of the coefficient

	// encode length
	if ((code = FindVLC(DClumtab0, num_elements(DClumtab0), size)) >= 0) {
		PutCode(vlc, code, 5, DClumtab0[code].len);
	} else {
		code = FindVLC(DClumtab1, num_elements(DClumtab1), size);
		PutCode(vlc, code + 0x1f0, 9, DClumtab1[code].len);
	}

	if (size)
		vlc.put(val - (val < 0), size);
}

int MPEG::GetChrominanceDC(vlc_reader &vlc) {
	int code, size, val;

	// decode length
	code = vlc.peek(5);

	if (code<31) {
		size = DCchromtab0[code].val;
		vlc.discard(DCchromtab0[code].len);
	} else {
		code = vlc.peek(10) - 0x3e0;
		size = DCchromtab1[code].val;
		vlc.discard(DCchromtab1[code].len);
	}

	if (size==0)
		val = 0;
	else {
		val = vlc.get(size);
		if ((val & (1<<(size-1)))==0)
			val-= (1<<size) - 1;
	}

	return val;
}

void MPEG::PutChrominanceDC(vlc_writer &vlc, int val) {
	int code, size, t;

	for (size = 0, t = abs(val); t; size++, t>>=1);		// Find the number of bits needed for the magnitude of the coefficient

	// encode length
	if ((code = FindVLC(DCchromtab0, num_elements(DCchromtab0), size)) >= 0) {
		PutCode(vlc, code, 5, DCchromtab0[code].len);
	} else {
		code = FindVLC(DCchromtab1, num_elements(DCchromtab1), size);
		PutCode(vlc, code + 0x3e0, 10, DCchromtab1[code].len);
	}

	if (size)
		vlc.put(val - (val < 0), size);
}


bool MPEG::GetAC1(vlc_reader &vlc, short *block, const uint8 *qmatrix, int qscale) {
	int				val, tval, j, sign;
	unsigned int	code;
	DCTtab			*tab;

	for (int i = 1; ; i++) {
		code = vlc.peek(16);
		if (code >= 16384)
			tab = &DCTtabnext[(code>>12)-4];
		else if (code >= 1024)
			tab = &DCTtab0[(code>>8)-4];
		else if (code >= 512)
			tab = &DCTtab1[(code>>6)-8];
		else if (code >= 256)
			tab = &DCTtab2[(code>>4)-16];
		else if (code >= 128)
			tab = &DCTtab3[(code>>3)-16];
		else if (code >= 64)
			tab = &DCTtab4[(code>>2)-16];
		else if (code >= 32)
			tab = &DCTtab5[(code>>1)-16];
		else if (code >= 16)
			tab = &DCTtab6[code-16];
		else
			return false;

		vlc.discard(tab->len);

		if (tab->run == 64) // end_of_block
			return true;

		if (tab->run == 65) {// escape
			i += vlc.get(6);

			if ((tval = vlc.get(8)) == 0)
				val = vlc.get(8);
			else if (tval == 128)
				val = vlc.get(8) - 256;
			else
				val = (signed char)tval;

			if (sign = (val < 0))
				val = -val;
		} else {
			i	+= tab->run;
			val	 = tab->level;
			sign = vlc.get(1);
		}

		if (i >= 64)
			return false;

		j = scan[i];
		val = (val * qscale * qmatrix[j]) >> 3;
//		if (val!=0) // should always be true
			val = (val-1) | 1;
		block[j] = sign ? -val : val;
	}
}

bool MPEG::GetAC2(vlc_reader &vlc, short *block, const uint8 *qmatrix, int qscale, bool intravlc) {
	int				val, j, sign, nc;
	unsigned int	code;
	DCTtab			*tab;
/*
	struct layer_data *ld1;

	// with data partitioning, data always goes to base layer
	ld1 = (ld->scalable_mode==SC_DP) ? &base : ld;
	bp = ld1->block[comp];

	if (base.scalable_mode==SC_DP)
		if (base.pri_brk<64)
			ld = &enhan;
		else
			ld = &base;
*/

	nc=0;

	// decode AC coefficients
	for (int i = 1; ; i++) {
		code = vlc.peek(16);
		if (code >= 16384 && !intravlc)
			tab = &DCTtabnext[(code>>12)-4];
		else if (code>=1024)
			tab = &(intravlc ? DCTtab0a : DCTtab0)[(code>>8)-4];
		else if (code>=512)
			tab = &(intravlc ? DCTtab1a : DCTtab1)[(code>>6)-8];
		else if (code>=256)
			tab = &DCTtab2[(code>>4)-16];
		else if (code>=128)
			tab = &DCTtab3[(code>>3)-16];
		else if (code>=64)
			tab = &DCTtab4[(code>>2)-16];
		else if (code>=32)
			tab = &DCTtab5[(code>>1)-16];
		else if (code>=16)
			tab = &DCTtab6[code-16];
		else
			return false;

		vlc.discard(tab->len);

		if (tab->run == 64) // end_of_block
			return true;

		if (tab->run == 65) { // escape
			i	+= vlc.get(6);
			val = vlc.get(12);
			if ((val & 2047) == 0)
				return false;
			if (sign = (val >= 2048))
				val = 4096 - val;
		} else {
			i	+= tab->run;
			val  = tab->level;
			sign = vlc.get(1);
		}

		if (i >= 64)
			return false;

		j = scan[i];
		val = (val * qscale * 2 * qmatrix[j]) >> 4;
		block[j] = sign ? -val : val;
		nc++;

//		if (base.scalable_mode==SC_DP && nc==base.pri_brk-63)
//			ld = &enhan;
	}
}

void MPEG::PutAC1(vlc_writer &vlc, short *block, const uint8 *qmatrix, int qscale) {
	short		zblock[8*8];
	int			i, run, level;

	// Quantise and zag
	for (int j = 0; j < 8*8; j++) {
		int	i		= scan[j];
		int	q		= qmatrix[i] * qscale;
		zblock[j]	= ((block[i]<<3) + q/2) / q;
	}

	for (i = 1, run = 0; i < 64; i++) {
		if (level = zblock[i]) {
			bool	putsign	= true;
			int		val		= abs(level);
			int		code;
			if ((code = FindDCT(DCTtabnext, num_elements(DCTtabnext), run, val)) >= 0) {
				PutCode(vlc, code + 4, 4, DCTtabnext[code].len);

			} else if ((code = FindDCT(DCTtab0, num_elements(DCTtab0), run, val)) >= 0) {
				PutCode(vlc, code + 4, 8, DCTtab0[code].len);

			} else if ((code = FindDCT(DCTtab1, num_elements(DCTtab1), run, val)) >= 0) {
				PutCode(vlc, code + 8, 10, DCTtab1[code].len);

			} else if ((code = FindDCT(DCTtab2, num_elements(DCTtab2), run, val)) >= 0) {
				PutCode(vlc, code + 16, 12, DCTtab2[code].len);

			} else if ((code = FindDCT(DCTtab3, num_elements(DCTtab3), run, val)) >= 0) {
				PutCode(vlc, code + 16, 13, DCTtab3[code].len);

			} else if ((code = FindDCT(DCTtab4, num_elements(DCTtab4), run, val)) >= 0) {
				PutCode(vlc, code + 16, 14, DCTtab4[code].len);

			} else if ((code = FindDCT(DCTtab5, num_elements(DCTtab5), run, val)) >= 0) {
				PutCode(vlc, code + 16, 15, DCTtab5[code].len);

			} else if ((code = FindDCT(DCTtab6, num_elements(DCTtab6), run, val)) >= 0) {
				PutCode(vlc, code + 16, 16, DCTtab6[code].len);

			} else {	//escape
				vlc.put(1, 6);
				vlc.put(run, 6);
				if (level && level <= 127 && level >= -127)
					vlc.put(level, 8);
				else {
					vlc.put(level < 0 ? 0x80 : 0, 8);
					vlc.put(level, 8);
				}
				putsign = false;
			}
			if (putsign)
				vlc.put(level < 0, 1);
			run = 0;
		} else
			run++;
	}
	vlc.put(2, 2);	//EOB
}

void MPEG::PutAC2(vlc_writer &vlc, short *block, const uint8 *qmatrix, int qscale, bool intravlc) {
	short		zblock[8*8];
	int			i, run, level;

	// Quantise and zag
	for (int j = 0; j < 8*8; j++) {
		int	i		= scan[j];
		int	q		= qmatrix[i] * qscale;
		zblock[j]	= ((block[i]<<3) + q/2) / q;
	}

	for (i = 1, run = 0; i < 64; i++) {
		if (level = zblock[i]) {
			bool	putsign	= true;
			int		val		= abs(level);
			int		code;
			if (!intravlc && (code = FindDCT(DCTtabnext, num_elements(DCTtabnext), run, val)) >= 0) {
				PutCode(vlc, code + 4, 4, DCTtabnext[code].len);

			} else if ((code = intravlc ? FindDCT(DCTtab0a, num_elements(DCTtab0a), run, val) : FindDCT(DCTtab0, num_elements(DCTtab0), run, val)) >= 0) {
				PutCode(vlc, code + 4, 8, DCTtab0[code].len);

			} else if ((code = intravlc ? FindDCT(DCTtab1a, num_elements(DCTtab1a), run, val) : FindDCT(DCTtab1, num_elements(DCTtab1), run, val)) >= 0) {
				PutCode(vlc, code + 8, 10, DCTtab1[code].len);

			} else if ((code = FindDCT(DCTtab2, num_elements(DCTtab2), run, val)) >= 0) {
				PutCode(vlc, code + 16, 12, DCTtab2[code].len);

			} else if ((code = FindDCT(DCTtab3, num_elements(DCTtab3), run, val)) >= 0) {
				PutCode(vlc, code + 16, 13, DCTtab3[code].len);

			} else if ((code = FindDCT(DCTtab4, num_elements(DCTtab4), run, val)) >= 0) {
				PutCode(vlc, code + 16, 14, DCTtab4[code].len);

			} else if ((code = FindDCT(DCTtab5, num_elements(DCTtab5), run, val)) >= 0) {
				PutCode(vlc, code + 16, 15, DCTtab5[code].len);

			} else if ((code = FindDCT(DCTtab6, num_elements(DCTtab6), run, val)) >= 0) {
				PutCode(vlc, code + 16, 16, DCTtab6[code].len);

			} else {	//escape
				vlc.put(1, 6);
				vlc.put(run, 6);
				vlc.put(level, 12);
				putsign = false;
			}
			if (putsign)
				vlc.put(level < 0, 1);
			run = 0;
		} else
			run++;
	}
	if (intravlc)
		vlc.put(6, 4);	//EOB
	else
		vlc.put(2, 2);	//EOB
}

//-----------------------------------------------------------------------------
//	FileHandler
//-----------------------------------------------------------------------------

struct MPEG_tsheader {
	uint32	sync:8,		//0x47
			tei:1,		//Transport Error Indicator
			pus:1,		//Payload Unit Start Indicator: 1 = start of PES data or PSI
			pri:1,		//Transport Priority: 1 = higher priority than other packets with the same PID
			pid:13,		//Packet ID
			scr:2,		//Scrambling control: 0 = Not scrambled, 1 = Reserved, 2 = Scrambled with even key, 3 = Scrambled with odd key
			afe:2,		//Adaptation field exist: 1 = payload only; 2 = adaptation field only; 3 = adaptation field and payload
			cont:4;		//Continuity counter: Incremented only when a payload is present (i.e., adaptation field exist is 01 or 11)
};
struct MPEG_adaption {
	uint16	length:8,	//Adaptation Field Length: Number of bytes in the adaptation field immediately following this byte
			dis:1,		//Discontinuity indicator: 1 = current TS packet is in a discontinuity state with respect to either the continuity counter or the program clock reference
			rand:1,		//Random Access indicator: 1 = PES packet in this TS packet starts a video/audio sequence
			pri:1,		//Elementary stream priority indicator: 1 = higher priority
			pcr:1,		//PCR flag: 1 = adaptation field does contain a PCR field
			opcr:1,		//OPCR flag: 1 = adaptation field does contain an OPCR field
			splice:1,	//Splicing point flag: 1 = presence of splice countdown field in adaptation field
			priv:1,		//Transport private data flag: 1 = presence of private data bytes in adaptation field
			ext:1;		//Adaptation field extension flag: 1 = presence of adaptation field extension
};
struct MPEG_pcr {
	//PCR 33+6+9 Program clock reference, stored in 6 octets in big-endian as 33 bits base, 6 bits padding, 9 bits extension.
};
struct MPEG_splice {
	//Splice countdown 8 Indicates how many TS packets from this one a splicing point occurs (may be negative)
};

struct MPEG_PAT {// (program association table)
	enum {PID = 0x0000, TID = 0x00};//The transport stream contains at least one or more TS packets with PID 0x0000. Some of these consecutive packets form the PAT. At the decoder side the PSI section filter listens to the incoming TS packets. After the filter identifies the PAT table they assemble the packet and decode it. A PAT has information about all the programs contained in the TS. The PAT contains information showing the association of Program Map Table PID and Program Number. The PAT should end with a 32-bit CRC
/*
(If TS payload)
 unit start Pointer field	8		Present if payload_unit_start_indicator bit is set in the TS header bytes. Gives the number of bytes from the end of this field to the start of payload data.
 Table ID					8		0x00
 Section syntax indicator	1		Always 1 for PAT
 0							1		Always 0 for PAT
 Reserved					2		Always set to binary '11'
 Section length				2+10	Informs how many programs are listed below by specifying the number of bytes of this section, starting immediately following this field and including the CRC. First two bits must be zero.
 transport stream ID		16		User defined data. Value not important to demuxers or players.
 Reserved					2		Always set to binary '11'
 Version number				5		Table version number. Incremented by 1 when data in table changes. Wraps around from 31 to 0.
 Current/next indicator		1		If 0, table data isn't applicable yet (becomes applicable when set to 1)
 Section number				8		Index of this section in the sequence of all PAT table sections. First section is numbered 0
 Last section number		8		Index of last section of PAT table

Repeated N times depending on section length
 Program num				16
 Reserved					3		Always set to binary '111'
 Program PID				13		packets with this PID are assumed to be PMT tables (see below)
*/
	crc32	crc;
};

struct MPEG_CAT {//(conditional access table)
	enum {TID = 0x01};//This table is used for conditional access to the streams. This table provides association with EMM stream. When the TS is scrambled then this section contains the EMM PID
};
struct MPEG_PMT {//(program map table)
	enum {TID = 0x02};
/*
(If TS payload)
 unit start Pointer field	8		Generally 0x00 for PMT
 Table ID					8		Always 0x02 for PMT
 Section syntax indicator	1
 0							1
 Reserved					2		Always set to binary '11'
 Section length				2+10	Informs how many programs are listed below by specifying the number of bytes of this section, starting immediately following this field and including the CRC. First two bits must be zero.
 Program num				16
 Reserved					2
 Version number				5		Incremented by 1 mod 32 each time the table data changes
 Current Next indicator		1		If 1, this table is currently valid. If 0, this table will become valid next.
 Section number				8		Always 0x00
 Last section number		8		Always 0x00
 Reserved					3
 PCR PID					13		PID of general timecode stream, or 0x1FFF
 Reserved					4
 Program info length		2+10	Sum size of following program descriptors. First two bits must be zero.
 Program descriptor			N*8

Repeated N times depending on section length
 stream type				8
 Reserved					3		Always set to binary '111'
 Elementary PID				13
 Reserved					4
 ES Info length				2+10	First two bits must be zero. Entire value may be zero
 ES Descriptor				N*8		If ES Info length is zero, this is omitted.
*/
	crc32	crc;
};

class MPEG_frames : public ISO::VirtualDefaults {
	istream_ptr			file;
	int					width, height;
	int					last_frame;
	ISO_ptr<bitmap>		bm;
	ISO_ptr<bitmap>		GetFrame(int i);
public:
	MPEG_frames(istream_ptr &&file);
	bool				Valid()			{ return width > 0;		}
	int					Count()			{ return 100;			}
	ISO::Browser2		Index(int i)	{ return GetFrame(i);	}

	int					Width()			{ return width;			}
	int					Height()		{ return height;		}
	float				FrameRate()		{ return 30.f;			}
};

MPEG_frames::MPEG_frames(istream_ptr &&_file) : file(move(_file)), width(0) {
	MPEG_tsheader	ts = file.get();
	if (ts.sync != 0x47)
		return;

	width		= 1280;
	height		= 720;
	last_frame	= -1;
	file.seek(0);
}

ISO_ptr<bitmap> MPEG_frames::GetFrame(int i) {
	if (i == last_frame)
		return bm;

	if (i < last_frame) {
		file.seek(0);
		last_frame = -1;
	}

	while (last_frame < i) {
		MPEG_tsheader	ts		= file.get();
		bool		dtd			= false;
		bool		altblockscan= false;
		bool		altscan		= false;
		bool		intravlc	= false;
		bool		qst			= false;
		bool		mpeg1		= false;

		MPEG		mpg;
		int16		blocks[6][8*8];
		vlc_reader	vlc(file);

		mpg.SetScan(altscan);
		bm.Create()->Create(width, height);
		if (altblockscan) {
			for (int x = 0; x < width; x += 16) {
				for (int y = 0; y < height; y += 16) {
					memset(blocks, 0, sizeof(blocks));
					if (x || y)
						vlc.get(1);
					mpg.MBRead(vlc, blocks, dtd, intravlc);
					mpg.MB2Bitmap(*bm, x, y, blocks[0]);
				}
			}
		} else {
			for (int y = 0; y < height; y += 16) {
				for (int x = 0; x < width; x += 16) {
					memset(blocks, 0, sizeof(blocks));
					if (x || y) vlc.get(1);
					mpg.MBRead(vlc, blocks, dtd, intravlc);
					mpg.MB2Bitmap(*bm, x, y, blocks[0]);
				}
			}
		}
		++last_frame;
	}

	return bm;
}

ISO_DEFVIRT(MPEG_frames);

class MPEGFileHandler : public FileHandler {
	const char*		GetExt() override { return "mpg";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		MPEG_tsheader	ts = file.get();
		return ts.sync == 0x47 ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<MPEG_frames>	frames(
			0,
			fn.find(':') > fn.begin() + 1 ? istream_ptr(new HTTPistream(HTTP("IsoEditor", fn).Get(fn.dir()))) : istream_ptr(new FileInput(fn))
		);
		if (frames->Valid())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} mpeg;

//-----------------------------------------------------------------------------
//	BSF
//-----------------------------------------------------------------------------

class BSF_frames : public ISO::VirtualDefaults {
	istream_ptr			file;
	int					width, height;
	int					last_frame;
	ISO_ptr<bitmap>		bm;
	ISO_ptr<bitmap>		GetFrame(int i);
public:
	BSF_frames(istream_ptr &&_file) : file(move(_file)), width(0) {
		width		= 1280;
		height		= 720;
		last_frame	= -1;
		file.seek(0);
	}
	bool				Valid()			{ return width > 0;		}
	int					Count()			{ return 100;			}
	ISO::Browser2		Index(int i)	{ return GetFrame(i);	}

	int					Width()			{ return width;			}
	int					Height()		{ return height;		}
	float				FrameRate()		{ return 30.f;			}
};

ISO_ptr<bitmap> BSF_frames::GetFrame(int i) {
	if (i == last_frame)
		return bm;

	if (i < last_frame) {
		file.seek(0);
		last_frame = -1;
	}

	while (last_frame < i) {
		MPEG		mpg;
		int16		blocks[6][8*8];
		vlc_reader	vlc(file);

		bm.Create()->Create(width, height);
		for (int y = 0; y < height; y += 16) {
			for (int x = 0; x < width; x += 16) {
				memset(blocks, 0, sizeof(blocks));
				if (x || y)
					vlc.get(1);
				mpg.MBRead(vlc, blocks, false, false);
				mpg.MB2Bitmap(*bm, x, y, blocks[0]);
			}
		}
		++last_frame;
	}

	return bm;
}

ISO_DEFVIRT(BSF_frames);

class BSFFileHandler : public FileHandler {
	const char*		GetExt() override { return "bsf";	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<BSF_frames>	frames(0, new FileInput(fn));
		if (frames->Valid())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} bsf;

#if 0
//-----------------------------------------------------------------------------
//
//	LuxBitmapIPU
//
//-----------------------------------------------------------------------------

class LuxBitmapIPU : LuxBitmapFormat {

	struct IPUHEADER {
		char	id[4];
		DWORD	size;
		WORD	width;
		WORD	height;
		DWORD	nframes;
	};

	char	*Ext()			{ return "ipu";				}
	char	*Description()	{ return "PS2 IPU Bitmap";	}

	bool Read(LuxBitmap &bm, Stream &file, int flags)
	{
		IPUHEADER	header;
		short		blocks[6][8*8];

		file.Read(&header, sizeof(header));
		if (strncmp(header.id, "ipum", 4)) return false;

//		LOG_IPU_VLC	vlc(file, "vlc_reader.txt");
		IPU_VLC	vlc(file);

		LuxBitmapFrame	*pbm = &bm;
		for (DWORD i = 0; i < header.nframes; i++) {
			if (i) {
				pbm->SetNext(new LuxBitmapFrame);
				pbm = pbm->Next();
				vlc.ReadAlign(8);
				vlc.get(16);	//skip start code
				vlc.get(16);	//skip start code
			}
			pbm->SetDelay(1000/60);
			pbm->Create((header.width + 15) & ~15, (header.height + 15) & ~15);


			int			IPUflags	= vlc.get(8);
			int			idp			= IPUflags & 3;
			bool		dtd			= (IPUflags & (1<<2)) != 0;
			bool		altblockscan= (IPUflags & (1<<3)) != 0;
			bool		altscan		= (IPUflags & (1<<4)) != 0;
			bool		intravlc	= (IPUflags & (1<<5)) != 0;
			bool		qst			= (IPUflags & (1<<6)) != 0;
			bool		mpeg1		= (IPUflags & (1<<7)) != 0;

			MPEG		mpg;
			mpg.SetScan(altscan);

			if (altblockscan) {
				for (int x = 0; x < header.width; x += 16) {
					for (int y = 0; y < header.height; y += 16) {
						memset(blocks, 0, sizeof(blocks));
						if (x || y) vlc.get(1);
						mpg.MBRead(vlc, blocks, dtd, intravlc);
						mpg.MB2Bitmap(*pbm, x, y, blocks[0]);
					}
				}
			} else {
				for (int y = 0; y < header.height; y += 16) {
					for (int x = 0; x < header.width; x += 16) {
						memset(blocks, 0, sizeof(blocks));
						if (x || y) vlc.get(1);
						mpg.MBRead(vlc, blocks, dtd, intravlc);
						mpg.MB2Bitmap(*pbm, x, y, blocks[0]);
					}
				}
			}
			pbm->Crop(0, 0, header.width, header.height);
			if (flags & BMRF_THUMBNAIL)
				break;
		}

		return true;
	}

	bool Write(LuxBitmap &bm, Stream &file, int flags)
	{
		short		blocks[6][8*8];
		long		ftell, fend;

		int			width	= bm.Width(),
					height	= bm.Height();

		file.PutBEL('ipum');
		ftell	= file.Tell();
		file.PutL(0);
		file.PutLEW(width);
		file.PutLEW(height);
		file.PutLEL(bm.NumFrames());

		int		qscl			= BMWF_GETQUALITY(flags) ? (100 - BMWF_GETQUALITY(flags)) * 30 / 99 + 1 : 8;
		bool	altscan			= false;//		= (flags & BMWF_TWIDDLE) != 0;
		bool	altblockscan	= (flags & BMWF_TWIDDLE) != 0;
		bool	mpeg1			= (flags & BMWF_USERFLAG) != 0;
		bool	dtd				= false;
		bool	intravlc		= false;

		IPU_VLC	vlc(file);

		for (LuxBitmapFrame *pbm = &bm; pbm; pbm = pbm->Next()) {
			MPEG		mpg;
			mpg.SetScan(altscan);

			vlc.put((altblockscan << 0) | (dtd << 2) | (altscan << 4) | (intravlc << 5) | (mpeg1 << 7), 8);

			pbm->Resize(width, height, true);

			if (altblockscan) {
				for (int x = 0; x < width; x += 16) {
					for (int y = 0; y < height; y += 16) {
						if (x || y) vlc.put(1, 1);
						mpg.Bitmap2MB(*pbm, x, y, blocks[0]);
						mpg.MBWrite(vlc, blocks, qscl, dtd, intravlc);
					}
				}
			} else {
				for (int y = 0; y < height; y += 16) {
					for (int x = 0; x < width; x += 16) {
						if (x || y) vlc.put(1, 1);
						mpg.Bitmap2MB(*pbm, x, y, blocks[0]);
						mpg.MBWrite(vlc, blocks, qscl, dtd, intravlc);
		#if 0
						for (int i = 0; i < 6; i++) {
							for (int j = 0; j < 64; j++) {
								blocks[i][j] = ((blocks[i][j] << 3) / (default_intra_matrix[j]*qscl) * (default_intra_matrix[j]*qscl))>>3;
							}
							mpg.DCTdecode(blocks[i]);
						}
						mpg.MB2Bitmap(*pbm, x, y, blocks[0]);
		#endif
					}
				}
			}
			vlc.Flush();
			file.PutBEL(0x000001b0);
		}
		file.PutBEL(0x000001b1);

		fend = file.Tell();
		file.Seek(ftell);
		file.PutLEL(fend - ftell - 4);
		file.Seek(fend);

		return true;
	}
} ipu;
#endif
