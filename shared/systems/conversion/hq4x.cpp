#include "defs.h"
#include "iso_convert.h"
#include "..\filetypes\bitmap.h"

using namespace iso;

const  int   Ymask = 0x00FF0000;
const  int   Umask = 0x0000FF00;
const  int   Vmask = 0x000000FF;
const  int   trY   = 0x00300000;
const  int   trU   = 0x00000700;
const  int   trV   = 0x00000006;

inline uint32 Interp1(uint32 c1, uint32 c2) {
	return (c1 * 3 + c2) >> 2;
}
inline uint32 Interp2(uint32 c1, uint32 c2, uint32 c3) {
	return (c1 * 2 + c2 + c3) >> 2;
}
inline uint32 Interp3(uint32 c1, uint32 c2) {
	//*pc = (c1*7+c2)/8;
	return ((((c1 & 0x00FF00)*7 + (c2 & 0x00FF00) ) & 0x0007F800)
		+ (((c1 & 0xFF00FF)*7 + (c2 & 0xFF00FF) ) & 0x07F807F8)) >> 3;
}
inline uint32 Interp5(uint32 c1, uint32 c2) {
	return (c1 + c2) >> 1;
}
inline uint32 Interp6(uint32 c1, uint32 c2, uint32 c3) {
	//*pc = (c1*5+c2*2+c3)/8;
	return ((((c1 & 0x00FF00)*5 + (c2 & 0x00FF00)*2 + (c3 & 0x00FF00)) & 0x0007F800)
		+ (((c1 & 0xFF00FF)*5 + (c2 & 0xFF00FF)*2 + (c3 & 0xFF00FF)) & 0x07F807F8)) >> 3;
}
inline uint32 Interp7(uint32 c1, uint32 c2, uint32 c3) {
	//*pc = (c1*6+c2+c3)/8;
	return ((((c1 & 0x00FF00)*6 + (c2 & 0x00FF00) + (c3 & 0x00FF00)) & 0x0007F800)
		+ (((c1 & 0xFF00FF)*6 + (c2 & 0xFF00FF) + (c3 & 0xFF00FF)) & 0x07F807F8)) >> 3;
}
inline uint32 Interp8(uint32 c1, uint32 c2) {
	//*pc = (c1*5+c2*3)/8;
	return ((((c1 & 0x00FF00)*5 + (c2 & 0x00FF00)*3) & 0x0007F800)
		+ (((c1 & 0xFF00FF)*5 + (c2 & 0xFF00FF)*3) & 0x07F807F8)) >> 3;
}

#define PIXEL00_0     pOut[0] = c[5]
#define PIXEL00_11    pOut[0] = Interp1(c[5], c[4])
#define PIXEL00_12    pOut[0] = Interp1(c[5], c[2])
#define PIXEL00_20    pOut[0] = Interp2(c[5], c[2], c[4])
#define PIXEL00_50    pOut[0] = Interp5(c[2], c[4])
#define PIXEL00_80    pOut[0] = Interp8(c[5], c[1])
#define PIXEL00_81    pOut[0] = Interp8(c[5], c[4])
#define PIXEL00_82    pOut[0] = Interp8(c[5], c[2])
#define PIXEL01_0     pOut[1] = c[5]
#define PIXEL01_10    pOut[1] = Interp1(c[5], c[1])
#define PIXEL01_12    pOut[1] = Interp1(c[5], c[2])
#define PIXEL01_14    pOut[1] = Interp1(c[2], c[5])
#define PIXEL01_21    pOut[1] = Interp2(c[2], c[5], c[4])
#define PIXEL01_31    pOut[1] = Interp3(c[5], c[4])
#define PIXEL01_50    pOut[1] = Interp5(c[2], c[5])
#define PIXEL01_60    pOut[1] = Interp6(c[5], c[2], c[4])
#define PIXEL01_61    pOut[1] = Interp6(c[5], c[2], c[1])
#define PIXEL01_82    pOut[1] = Interp8(c[5], c[2])
#define PIXEL01_83    pOut[1] = Interp8(c[2], c[4])
#define PIXEL02_0     pOut[2] = c[5]
#define PIXEL02_10    pOut[2] = Interp1(c[5], c[3])
#define PIXEL02_11    pOut[2] = Interp1(c[5], c[2])
#define PIXEL02_13    pOut[2] = Interp1(c[2], c[5])
#define PIXEL02_21    pOut[2] = Interp2(c[2], c[5], c[6])
#define PIXEL02_32    pOut[2] = Interp3(c[5], c[6])
#define PIXEL02_50    pOut[2] = Interp5(c[2], c[5])
#define PIXEL02_60    pOut[2] = Interp6(c[5], c[2], c[6])
#define PIXEL02_61    pOut[2] = Interp6(c[5], c[2], c[3])
#define PIXEL02_81    pOut[2] = Interp8(c[5], c[2])
#define PIXEL02_83    pOut[2] = Interp8(c[2], c[6])
#define PIXEL03_0     pOut[3] = c[5]
#define PIXEL03_11    pOut[3] = Interp1(c[5], c[2])
#define PIXEL03_12    pOut[3] = Interp1(c[5], c[6])
#define PIXEL03_20    pOut[3] = Interp2(c[5], c[2], c[6])
#define PIXEL03_50    pOut[3] = Interp5(c[2], c[6])
#define PIXEL03_80    pOut[3] = Interp8(c[5], c[3])
#define PIXEL03_81    pOut[3] = Interp8(c[5], c[2])
#define PIXEL03_82    pOut[3] = Interp8(c[5], c[6])
#define PIXEL10_0     pOut[BpL] = c[5]
#define PIXEL10_10    pOut[BpL] = Interp1(c[5], c[1])
#define PIXEL10_11    pOut[BpL] = Interp1(c[5], c[4])
#define PIXEL10_13    pOut[BpL] = Interp1(c[4], c[5])
#define PIXEL10_21    pOut[BpL] = Interp2(c[4], c[5], c[2])
#define PIXEL10_32    pOut[BpL] = Interp3(c[5], c[2])
#define PIXEL10_50    pOut[BpL] = Interp5(c[4], c[5])
#define PIXEL10_60    pOut[BpL] = Interp6(c[5], c[4], c[2])
#define PIXEL10_61    pOut[BpL] = Interp6(c[5], c[4], c[1])
#define PIXEL10_81    pOut[BpL] = Interp8(c[5], c[4])
#define PIXEL10_83    pOut[BpL] = Interp8(c[4], c[2])
#define PIXEL11_0     pOut[BpL+1] = c[5]
#define PIXEL11_30    pOut[BpL+1] = Interp3(c[5], c[1])
#define PIXEL11_31    pOut[BpL+1] = Interp3(c[5], c[4])
#define PIXEL11_32    pOut[BpL+1] = Interp3(c[5], c[2])
#define PIXEL11_70    pOut[BpL+1] = Interp7(c[5], c[4], c[2])
#define PIXEL12_0     pOut[BpL+2] = c[5]
#define PIXEL12_30    pOut[BpL+2] = Interp3(c[5], c[3])
#define PIXEL12_31    pOut[BpL+2] = Interp3(c[5], c[2])
#define PIXEL12_32    pOut[BpL+2] = Interp3(c[5], c[6])
#define PIXEL12_70    pOut[BpL+2] = Interp7(c[5], c[6], c[2])
#define PIXEL13_0     pOut[BpL+3] = c[5]
#define PIXEL13_10    pOut[BpL+3] = Interp1(c[5], c[3])
#define PIXEL13_12    pOut[BpL+3] = Interp1(c[5], c[6])
#define PIXEL13_14    pOut[BpL+3] = Interp1(c[6], c[5])
#define PIXEL13_21    pOut[BpL+3] = Interp2(c[6], c[5], c[2])
#define PIXEL13_31    pOut[BpL+3] = Interp3(c[5], c[2])
#define PIXEL13_50    pOut[BpL+3] = Interp5(c[6], c[5])
#define PIXEL13_60    pOut[BpL+3] = Interp6(c[5], c[6], c[2])
#define PIXEL13_61    pOut[BpL+3] = Interp6(c[5], c[6], c[3])
#define PIXEL13_82    pOut[BpL+3] = Interp8(c[5], c[6])
#define PIXEL13_83    pOut[BpL+3] = Interp8(c[6], c[2])
#define PIXEL20_0     pOut[BpL+BpL] = c[5]
#define PIXEL20_10    pOut[BpL+BpL] = Interp1(c[5], c[7])
#define PIXEL20_12    pOut[BpL+BpL] = Interp1(c[5], c[4])
#define PIXEL20_14    pOut[BpL+BpL] = Interp1(c[4], c[5])
#define PIXEL20_21    pOut[BpL+BpL] = Interp2(c[4], c[5], c[8])
#define PIXEL20_31    pOut[BpL+BpL] = Interp3(c[5], c[8])
#define PIXEL20_50    pOut[BpL+BpL] = Interp5(c[4], c[5])
#define PIXEL20_60    pOut[BpL+BpL] = Interp6(c[5], c[4], c[8])
#define PIXEL20_61    pOut[BpL+BpL] = Interp6(c[5], c[4], c[7])
#define PIXEL20_82    pOut[BpL+BpL] = Interp8(c[5], c[4])
#define PIXEL20_83    pOut[BpL+BpL] = Interp8(c[4], c[8])
#define PIXEL21_0     pOut[BpL+BpL+1]  = c[5]
#define PIXEL21_30    pOut[BpL+BpL+1] = Interp3(c[5], c[7])
#define PIXEL21_31    pOut[BpL+BpL+1] = Interp3(c[5], c[8])
#define PIXEL21_32    pOut[BpL+BpL+1] = Interp3(c[5], c[4])
#define PIXEL21_70    pOut[BpL+BpL+1] = Interp7(c[5], c[4], c[8])
#define PIXEL22_0     pOut[BpL+BpL+2]  = c[5]
#define PIXEL22_30    pOut[BpL+BpL+2] = Interp3(c[5], c[9])
#define PIXEL22_31    pOut[BpL+BpL+2] = Interp3(c[5], c[6])
#define PIXEL22_32    pOut[BpL+BpL+2] = Interp3(c[5], c[8])
#define PIXEL22_70    pOut[BpL+BpL+2] = Interp7(c[5], c[6], c[8])
#define PIXEL23_0     pOut[BpL+BpL+3] = c[5]
#define PIXEL23_10    pOut[BpL+BpL+3] = Interp1(c[5], c[9])
#define PIXEL23_11    pOut[BpL+BpL+3] = Interp1(c[5], c[6])
#define PIXEL23_13    pOut[BpL+BpL+3] = Interp1(c[6], c[5])
#define PIXEL23_21    pOut[BpL+BpL+3] = Interp2(c[6], c[5], c[8])
#define PIXEL23_32    pOut[BpL+BpL+3] = Interp3(c[5], c[8])
#define PIXEL23_50    pOut[BpL+BpL+3] = Interp5(c[6], c[5])
#define PIXEL23_60    pOut[BpL+BpL+3] = Interp6(c[5], c[6], c[8])
#define PIXEL23_61    pOut[BpL+BpL+3] = Interp6(c[5], c[6], c[9])
#define PIXEL23_81    pOut[BpL+BpL+3] = Interp8(c[5], c[6])
#define PIXEL23_83    pOut[BpL+BpL+3] = Interp8(c[6], c[8])
#define PIXEL30_0     pOut[BpL+BpL+BpL] = c[5]
#define PIXEL30_11    pOut[BpL+BpL+BpL] = Interp1(c[5], c[8])
#define PIXEL30_12    pOut[BpL+BpL+BpL] = Interp1(c[5], c[4])
#define PIXEL30_20    pOut[BpL+BpL+BpL] = Interp2(c[5], c[8], c[4]);
#define PIXEL30_50    pOut[BpL+BpL+BpL] = Interp5(c[8], c[4])
#define PIXEL30_80    pOut[BpL+BpL+BpL] = Interp8(c[5], c[7])
#define PIXEL30_81    pOut[BpL+BpL+BpL] = Interp8(c[5], c[8])
#define PIXEL30_82    pOut[BpL+BpL+BpL] = Interp8(c[5], c[4])
#define PIXEL31_0     pOut[BpL+BpL+BpL+1] = c[5]
#define PIXEL31_10    pOut[BpL+BpL+BpL+1] = Interp1(c[5], c[7])
#define PIXEL31_11    pOut[BpL+BpL+BpL+1] = Interp1(c[5], c[8])
#define PIXEL31_13    pOut[BpL+BpL+BpL+1] = Interp1(c[8], c[5])
#define PIXEL31_21    pOut[BpL+BpL+BpL+1] = Interp2(c[8], c[5], c[4]);
#define PIXEL31_32    pOut[BpL+BpL+BpL+1] = Interp3(c[5], c[4])
#define PIXEL31_50    pOut[BpL+BpL+BpL+1] = Interp5(c[8], c[5])
#define PIXEL31_60    pOut[BpL+BpL+BpL+1] = Interp6(c[5], c[8], c[4]);
#define PIXEL31_61    pOut[BpL+BpL+BpL+1] = Interp6(c[5], c[8], c[7]);
#define PIXEL31_81    pOut[BpL+BpL+BpL+1] = Interp8(c[5], c[8])
#define PIXEL31_83    pOut[BpL+BpL+BpL+1] = Interp8(c[8], c[4])
#define PIXEL32_0     pOut[BpL+BpL+BpL+2] = c[5]
#define PIXEL32_10    pOut[BpL+BpL+BpL+2] = Interp1(c[5], c[9])
#define PIXEL32_12    pOut[BpL+BpL+BpL+2] = Interp1(c[5], c[8])
#define PIXEL32_14    pOut[BpL+BpL+BpL+2] = Interp1(c[8], c[5])
#define PIXEL32_21    pOut[BpL+BpL+BpL+2] = Interp2(c[8], c[5], c[6]);
#define PIXEL32_31    pOut[BpL+BpL+BpL+2] = Interp3(c[5], c[6])
#define PIXEL32_50    pOut[BpL+BpL+BpL+2] = Interp5(c[8], c[5])
#define PIXEL32_60    pOut[BpL+BpL+BpL+2] = Interp6(c[5], c[8], c[6]);
#define PIXEL32_61    pOut[BpL+BpL+BpL+2] = Interp6(c[5], c[8], c[9]);
#define PIXEL32_82    pOut[BpL+BpL+BpL+2] = Interp8(c[5], c[8])
#define PIXEL32_83    pOut[BpL+BpL+BpL+2] = Interp8(c[8], c[6])
#define PIXEL33_0     pOut[BpL+BpL+BpL+3] = c[5]
#define PIXEL33_11    pOut[BpL+BpL+BpL+3] = Interp1(c[5], c[6])
#define PIXEL33_12    pOut[BpL+BpL+BpL+3] = Interp1(c[5], c[8])
#define PIXEL33_20    pOut[BpL+BpL+BpL+3] = Interp2(c[5], c[8], c[6]);
#define PIXEL33_50    pOut[BpL+BpL+BpL+3] = Interp5(c[8], c[6])
#define PIXEL33_80    pOut[BpL+BpL+BpL+3] = Interp8(c[5], c[9])
#define PIXEL33_81    pOut[BpL+BpL+BpL+3] = Interp8(c[5], c[6])
#define PIXEL33_82    pOut[BpL+BpL+BpL+3] = Interp8(c[5], c[8])

inline uint32 RGBtoYUV(uint32 c) {
	int	r = c & 0xff, g = (c >> 8) & 0xff, b = (c >> 16) & 0xff;
	int	Y = (r + g + b) >> 2;
	int	u = 128 + ((r - b) >> 2);
	int	v = 128 + ((-r + 2 * g -b) >> 3);
	return (Y << 16) + (u << 8) + v;
}

inline bool Diff(uint32 c1, uint32 c2)
{
	uint32	YUV1 = RGBtoYUV(c1);
	uint32	YUV2 = RGBtoYUV(c2);
	return	abs(int(YUV1 & Ymask) - int(YUV2 & Ymask)) > trY ||
			abs(int(YUV1 & Umask) - int(YUV2 & Umask)) > trU ||
			abs(int(YUV1 & Vmask) - int(YUV2 & Vmask)) > trV;
}

void hq4x(uint32 *pIn, uint32 *pOut, int Xres, int Yres)
{
	int		BpL = Xres * 4;
	uint32  c[10];

	//   +----+----+----+
	//   |    |    |    |
	//   | w1 | w2 | w3 |
	//   +----+----+----+
	//   |    |    |    |
	//   | w4 | w5 | w6 |
	//   +----+----+----+
	//   |    |    |    |
	//   | w7 | w8 | w9 |
	//   +----+----+----+

	for (int j = 0; j < Yres; j++) {
		int	prevline = j > 0		? -Xres : 0;
		int	nextline = j < Yres-1	?  Xres : 0;

		for (int i = 0; i < Xres; i++, pIn++) {
			c[2] = pIn[prevline];
			c[5] = pIn[0];
			c[8] = pIn[nextline];

			if (i > 0) {
				c[1] = pIn[prevline - 1];
				c[4] = pIn[-1];
				c[7] = pIn[nextline - 1];
			} else {
				c[1] = c[2];
				c[4] = c[5];
				c[7] = c[8];
			}

			if (i < Xres-1) {
				c[3] = pIn[prevline + 1];
				c[6] = pIn[1];
				c[9] = pIn[nextline + 1];
			} else {
				c[3] = c[2];
				c[6] = c[5];
				c[9] = c[8];
			}

			int pattern	= 0;
			int flag	= 1;

			uint32	YUV1 = RGBtoYUV(c[5]);
			for (int k = 1; k <= 9; k++) {
				if (k == 5)
					continue;
				if (c[k] != c[5]) {
					uint32	YUV2 = RGBtoYUV(c[k]);
					if (abs(int(YUV1 & Ymask) - int(YUV2 & Ymask)) > trY
					||	abs(int(YUV1 & Umask) - int(YUV2 & Umask)) > trU
					||	abs(int(YUV1 & Vmask) - int(YUV2 & Vmask)) > trV
					)
						pattern |= flag;
				}
				flag <<= 1;
			}

			switch (pattern) {
				case 0:
				case 1:
				case 4:
				case 32:
				case 128:
				case 5:
				case 132:
				case 160:
				case 33:
				case 129:
				case 36:
				case 133:
				case 164:
				case 161:
				case 37:
				case 165: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 2:
				case 34:
				case 130:
				case 162: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 16:
				case 17:
				case 48:
				case 49: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 64:
				case 65:
				case 68:
				case 69: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 8:
				case 12:
				case 136:
				case 140: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 3:
				case 35:
				case 131:
				case 163: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 6:
				case 38:
				case 134:
				case 166: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 20:
				case 21:
				case 52:
				case 53: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 144:
				case 145:
				case 176:
				case 177: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 192:
				case 193:
				case 196:
				case 197: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 96:
				case 97:
				case 100:
				case 101: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 40:
				case 44:
				case 168:
				case 172: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 9:
				case 13:
				case 137:
				case 141: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 18:
				case 50: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL12_0;
						PIXEL13_50;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 80:
				case 81: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_61;
					PIXEL21_30;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 72:
				case 76: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_70;
					PIXEL13_60;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_50;
						PIXEL21_0;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 10:
				case 138: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
						PIXEL11_0;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 66: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 24: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 7:
				case 39:
				case 135: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 148:
				case 149:
				case 180: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 224:
				case 228:
				case 225: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 41:
				case 169:
				case 45: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 22:
				case 54: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 208:
				case 209: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 104:
				case 108: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_70;
					PIXEL13_60;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 11:
				case 139: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else{
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 19:
				case 51: {
					if (Diff(c[2], c[6])) {
						PIXEL00_81;
						PIXEL01_31;
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL00_12;
						PIXEL01_14;
						PIXEL02_83;
						PIXEL03_50;
						PIXEL12_70;
						PIXEL13_21;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 146:
				case 178: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
						PIXEL23_32;
						PIXEL33_82;
					} else {
						PIXEL02_21;
						PIXEL03_50;
						PIXEL12_70;
						PIXEL13_83;
						PIXEL23_13;
						PIXEL33_11;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					break;
				}
				case 84:
				case 85: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					if (Diff(c[6], c[8])) {
						PIXEL03_81;
						PIXEL13_31;
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL03_12;
						PIXEL13_14;
						PIXEL22_70;
						PIXEL23_83;
						PIXEL32_21;
						PIXEL33_50;
					}
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 112:
				case 113: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_82;
					PIXEL21_32;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL30_82;
						PIXEL31_32;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_70;
						PIXEL23_21;
						PIXEL30_11;
						PIXEL31_13;
						PIXEL32_83;
						PIXEL33_50;
					}
					break;
				}
				case 200:
				case 204: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_70;
					PIXEL13_60;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
						PIXEL32_31;
						PIXEL33_81;
					} else {
						PIXEL20_21;
						PIXEL21_70;
						PIXEL30_50;
						PIXEL31_83;
						PIXEL32_14;
						PIXEL33_12;
					}
					PIXEL22_31;
					PIXEL23_81;
					break;
				}
				case 73:
				case 77: {
					if (Diff(c[8], c[4])) {
						PIXEL00_82;
						PIXEL10_32;
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL00_11;
						PIXEL10_13;
						PIXEL20_83;
						PIXEL21_70;
						PIXEL30_50;
						PIXEL31_21;
					}
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 42:
				case 170: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
						PIXEL20_31;
						PIXEL30_81;
					} else {
						PIXEL00_50;
						PIXEL01_21;
						PIXEL10_83;
						PIXEL11_70;
						PIXEL20_14;
						PIXEL30_12;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 14:
				case 142: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL02_32;
						PIXEL03_82;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_50;
						PIXEL01_83;
						PIXEL02_13;
						PIXEL03_11;
						PIXEL10_21;
						PIXEL11_70;
					}
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 67: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 70: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 28: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 152: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 194: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 98: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 56: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 25: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 26:
				case 31: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL11_0;
					PIXEL12_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 82:
				case 214: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 88:
				case 248: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					break;
				}
				case 74:
				case 107: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_61;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 27: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 86: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 216: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 106: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_61;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 30: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 210: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 120: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 75: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 29: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 198: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 184: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 99: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 57: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 71: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 156: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 226: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 60: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 195: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 102: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 153: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 58: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 83: {
					PIXEL00_81;
					PIXEL01_31;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL20_61;
					PIXEL21_30;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 92: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 202: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL12_30;
					PIXEL13_61;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_31;
					PIXEL23_81;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 78: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					PIXEL02_32;
					PIXEL03_82;
					PIXEL12_32;
					PIXEL13_82;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 154: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 114: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL20_82;
					PIXEL21_32;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					PIXEL30_82;
					PIXEL31_32;
					break;
				}
				case 89: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 90: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 55:
				case 23: {
					if (Diff(c[2], c[6])) {
						PIXEL00_81;
						PIXEL01_31;
						PIXEL02_0;
						PIXEL03_0;
						PIXEL12_0;
						PIXEL13_0;
					} else {
						PIXEL00_12;
						PIXEL01_14;
						PIXEL02_83;
						PIXEL03_50;
						PIXEL12_70;
						PIXEL13_21;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 182:
				case 150: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL12_0;
						PIXEL13_0;
						PIXEL23_32;
						PIXEL33_82;
					} else {
						PIXEL02_21;
						PIXEL03_50;
						PIXEL12_70;
						PIXEL13_83;
						PIXEL23_13;
						PIXEL33_11;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					break;
				}
				case 213:
				case 212: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					if (Diff(c[6], c[8])) {
						PIXEL03_81;
						PIXEL13_31;
						PIXEL22_0;
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL03_12;
						PIXEL13_14;
						PIXEL22_70;
						PIXEL23_83;
						PIXEL32_21;
						PIXEL33_50;
					}
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 241:
				case 240: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_82;
					PIXEL21_32;
					if (Diff(c[6], c[8])) {
						PIXEL22_0;
						PIXEL23_0;
						PIXEL30_82;
						PIXEL31_32;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL22_70;
						PIXEL23_21;
						PIXEL30_11;
						PIXEL31_13;
						PIXEL32_83;
						PIXEL33_50;
					}
					break;
				}
				case 236:
				case 232: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_70;
					PIXEL13_60;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL21_0;
						PIXEL30_0;
						PIXEL31_0;
						PIXEL32_31;
						PIXEL33_81;
					} else {
						PIXEL20_21;
						PIXEL21_70;
						PIXEL30_50;
						PIXEL31_83;
						PIXEL32_14;
						PIXEL33_12;
					}
					PIXEL22_31;
					PIXEL23_81;
					break;
				}
				case 109:
				case 105: {
					if (Diff(c[8], c[4])) {
						PIXEL00_82;
						PIXEL10_32;
						PIXEL20_0;
						PIXEL21_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL00_11;
						PIXEL10_13;
						PIXEL20_83;
						PIXEL21_70;
						PIXEL30_50;
						PIXEL31_21;
					}
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 171:
				case 43: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
						PIXEL11_0;
						PIXEL20_31;
						PIXEL30_81;
					} else {
						PIXEL00_50;
						PIXEL01_21;
						PIXEL10_83;
						PIXEL11_70;
						PIXEL20_14;
						PIXEL30_12;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 143:
				case 15: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL02_32;
						PIXEL03_82;
						PIXEL10_0;
						PIXEL11_0;
					} else {
						PIXEL00_50;
						PIXEL01_83;
						PIXEL02_13;
						PIXEL03_11;
						PIXEL10_21;
						PIXEL11_70;
					}
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 124: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 203: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 62: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 211: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 118: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 217: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 110: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 155: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 188: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 185: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 61: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 157: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 103: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 227: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 230: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 199: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 220: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					break;
				}
				case 158: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL12_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 234: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL12_30;
					PIXEL13_61;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 242: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_82;
					PIXEL31_32;
					break;
				}
				case 59: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL11_0;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 121: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 87: {
					PIXEL00_81;
					PIXEL01_31;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_0;
					PIXEL20_61;
					PIXEL21_30;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 79: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_32;
					PIXEL03_82;
					PIXEL11_0;
					PIXEL12_32;
					PIXEL13_82;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 122: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 94: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL12_0;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 218: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					break;
				}
				case 91: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL11_0;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 229: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 167: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 173: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 181: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 186: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 115: {
					PIXEL00_81;
					PIXEL01_31;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL20_82;
					PIXEL21_32;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					PIXEL30_82;
					PIXEL31_32;
					break;
				}
				case 93: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					break;
				}
				case 206: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					PIXEL02_32;
					PIXEL03_82;
					PIXEL12_32;
					PIXEL13_82;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_31;
					PIXEL23_81;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 205:
				case 201: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					if (Diff(c[8], c[4])) {
						PIXEL20_10;
						PIXEL21_30;
						PIXEL30_80;
						PIXEL31_10;
					} else {
						PIXEL20_12;
						PIXEL21_0;
						PIXEL30_20;
						PIXEL31_11;
					}
					PIXEL22_31;
					PIXEL23_81;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 174:
				case 46: {
					if (Diff(c[4], c[2])) {
						PIXEL00_80;
						PIXEL01_10;
						PIXEL10_10;
						PIXEL11_30;
					} else {
						PIXEL00_20;
						PIXEL01_12;
						PIXEL10_11;
						PIXEL11_0;
					}
					PIXEL02_32;
					PIXEL03_82;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 179:
				case 147: {
					PIXEL00_81;
					PIXEL01_31;
					if (Diff(c[2], c[6])) {
						PIXEL02_10;
						PIXEL03_80;
						PIXEL12_30;
						PIXEL13_10;
					} else {
						PIXEL02_11;
						PIXEL03_20;
						PIXEL12_0;
						PIXEL13_12;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 117:
				case 116: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_82;
					PIXEL21_32;
					if (Diff(c[6], c[8])) {
						PIXEL22_30;
						PIXEL23_10;
						PIXEL32_10;
						PIXEL33_80;
					} else {
						PIXEL22_0;
						PIXEL23_11;
						PIXEL32_12;
						PIXEL33_20;
					}
					PIXEL30_82;
					PIXEL31_32;
					break;
				}
				case 189: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 231: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 126: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_0;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 219: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 125: {
					if (Diff(c[8], c[4])) {
						PIXEL00_82;
						PIXEL10_32;
						PIXEL20_0;
						PIXEL21_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL00_11;
						PIXEL10_13;
						PIXEL20_83;
						PIXEL21_70;
						PIXEL30_50;
						PIXEL31_21;
					}
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 221: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					if (Diff(c[6], c[8])) {
						PIXEL03_81;
						PIXEL13_31;
						PIXEL22_0;
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL03_12;
						PIXEL13_14;
						PIXEL22_70;
						PIXEL23_83;
						PIXEL32_21;
						PIXEL33_50;
					}
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 207: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL02_32;
						PIXEL03_82;
						PIXEL10_0;
						PIXEL11_0;
					} else {
						PIXEL00_50;
						PIXEL01_83;
						PIXEL02_13;
						PIXEL03_11;
						PIXEL10_21;
						PIXEL11_70;
					}
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_31;
					PIXEL23_81;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 238: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_32;
					PIXEL13_82;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL21_0;
						PIXEL30_0;
						PIXEL31_0;
						PIXEL32_31;
						PIXEL33_81;
					} else {
						PIXEL20_21;
						PIXEL21_70;
						PIXEL30_50;
						PIXEL31_83;
						PIXEL32_14;
						PIXEL33_12;
					}
					PIXEL22_31;
					PIXEL23_81;
					break;
				}
				case 190: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL12_0;
						PIXEL13_0;
						PIXEL23_32;
						PIXEL33_82;
					} else {
						PIXEL02_21;
						PIXEL03_50;
						PIXEL12_70;
						PIXEL13_83;
						PIXEL23_13;
						PIXEL33_11;
					}
					PIXEL10_10;
					PIXEL11_30;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					break;
				}
				case 187: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
						PIXEL11_0;
						PIXEL20_31;
						PIXEL30_81;
					} else {
						PIXEL00_50;
						PIXEL01_21;
						PIXEL10_83;
						PIXEL11_70;
						PIXEL20_14;
						PIXEL30_12;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 243: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_82;
					PIXEL21_32;
					if (Diff(c[6], c[8])) {
						PIXEL22_0;
						PIXEL23_0;
						PIXEL30_82;
						PIXEL31_32;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL22_70;
						PIXEL23_21;
						PIXEL30_11;
						PIXEL31_13;
						PIXEL32_83;
						PIXEL33_50;
					}
					break;
				}
				case 119: {
					if (Diff(c[2], c[6])) {
						PIXEL00_81;
						PIXEL01_31;
						PIXEL02_0;
						PIXEL03_0;
						PIXEL12_0;
						PIXEL13_0;
					} else {
						PIXEL00_12;
						PIXEL01_14;
						PIXEL02_83;
						PIXEL03_50;
						PIXEL12_70;
						PIXEL13_21;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 237:
				case 233: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_60;
					PIXEL03_20;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_70;
					PIXEL13_60;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_31;
					PIXEL23_81;
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 175:
				case 47: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_70;
					PIXEL23_60;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_60;
					PIXEL33_20;
					break;
				}
				case 183:
				case 151: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_60;
					PIXEL21_70;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_20;
					PIXEL31_60;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 245:
				case 244: {
					PIXEL00_20;
					PIXEL01_60;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_60;
					PIXEL11_70;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_0;
					PIXEL23_0;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
				case 250: {
					PIXEL00_80;
					PIXEL01_10;
					PIXEL02_10;
					PIXEL03_80;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_30;
					PIXEL13_10;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					break;
				}
				case 123: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_10;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 95: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL11_0;
					PIXEL12_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_80;
					PIXEL31_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 222: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 252: {
					PIXEL00_80;
					PIXEL01_61;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_31;
					PIXEL13_31;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_0;
					PIXEL23_0;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
				case 249: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_61;
					PIXEL03_80;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					break;
				}
				case 235: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_61;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_31;
					PIXEL23_81;
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 111: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_32;
					PIXEL13_82;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_61;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 63: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_0;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_61;
					PIXEL33_80;
					break;
				}
				case 159: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL11_0;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_80;
					PIXEL31_61;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 215: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_61;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 246: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_61;
					PIXEL11_30;
					PIXEL12_0;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_0;
					PIXEL23_0;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
				case 254: {
					PIXEL00_80;
					PIXEL01_10;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_10;
					PIXEL11_30;
					PIXEL12_0;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_0;
					PIXEL23_0;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
				case 253: {
					PIXEL00_82;
					PIXEL01_82;
					PIXEL02_81;
					PIXEL03_81;
					PIXEL10_32;
					PIXEL11_32;
					PIXEL12_31;
					PIXEL13_31;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_0;
					PIXEL23_0;
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
				case 251: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_10;
					PIXEL03_80;
					PIXEL11_0;
					PIXEL12_30;
					PIXEL13_10;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					break;
				}
				case 239: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					PIXEL02_32;
					PIXEL03_82;
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_32;
					PIXEL13_82;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_31;
					PIXEL23_81;
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					PIXEL32_31;
					PIXEL33_81;
					break;
				}
				case 127: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					if (Diff(c[2], c[6])) {
						PIXEL02_0;
						PIXEL03_0;
						PIXEL13_0;
					} else {
						PIXEL02_50;
						PIXEL03_50;
						PIXEL13_50;
					}
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_0;
					if (Diff(c[8], c[4])) {
						PIXEL20_0;
						PIXEL30_0;
						PIXEL31_0;
					} else {
						PIXEL20_50;
						PIXEL30_50;
						PIXEL31_50;
					}
					PIXEL21_0;
					PIXEL22_30;
					PIXEL23_10;
					PIXEL32_10;
					PIXEL33_80;
					break;
				}
				case 191: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_31;
					PIXEL21_31;
					PIXEL22_32;
					PIXEL23_32;
					PIXEL30_81;
					PIXEL31_81;
					PIXEL32_82;
					PIXEL33_82;
					break;
				}
				case 223: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
						PIXEL01_0;
						PIXEL10_0;
					} else {
						PIXEL00_50;
						PIXEL01_50;
						PIXEL10_50;
					}
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL11_0;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_10;
					PIXEL21_30;
					PIXEL22_0;
					if (Diff(c[6], c[8])) {
						PIXEL23_0;
						PIXEL32_0;
						PIXEL33_0;
					} else {
						PIXEL23_50;
						PIXEL32_50;
						PIXEL33_50;
					}
					PIXEL30_80;
					PIXEL31_10;
					break;
				}
				case 247: {
					PIXEL00_81;
					PIXEL01_31;
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL10_81;
					PIXEL11_31;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_82;
					PIXEL21_32;
					PIXEL22_0;
					PIXEL23_0;
					PIXEL30_82;
					PIXEL31_32;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
				case 255: {
					if (Diff(c[4], c[2])) {
						PIXEL00_0;
					} else {
						PIXEL00_20;
					}
					PIXEL01_0;
					PIXEL02_0;
					if (Diff(c[2], c[6])) {
						PIXEL03_0;
					} else {
						PIXEL03_20;
					}
					PIXEL10_0;
					PIXEL11_0;
					PIXEL12_0;
					PIXEL13_0;
					PIXEL20_0;
					PIXEL21_0;
					PIXEL22_0;
					PIXEL23_0;
					if (Diff(c[8], c[4])) {
						PIXEL30_0;
					} else {
						PIXEL30_20;
					}
					PIXEL31_0;
					PIXEL32_0;
					if (Diff(c[6], c[8])) {
						PIXEL33_0;
					} else {
						PIXEL33_20;
					}
					break;
				}
			}
			pOut += 4;
		}
		pOut += BpL * 3;
	}
}

bitmap HQ4X(WRAP<ISO_ptr<bitmap> > bm) {
	bitmap	bm2;
	if (bm.t) {
		int	w = bm->Width(), h = bm->Height();
		bm2.Create(w * 4, h * 4, bm->Flags(), bm->Depth());
		hq4x((uint32*)bm->ScanLine(0), (uint32*)bm2.ScanLine(0), w, h);
		return bm2;
	}
	return bm2;
}

static initialise init(
	ISO_getconversion("HQ4X",		HQ4X)
);