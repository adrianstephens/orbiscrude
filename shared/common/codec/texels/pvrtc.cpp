#include "pvrtc.h"
#include "base/vector.h"
#include "utilities.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	PVRTC decompression
//-----------------------------------------------------------------------------

void InterpolateColours(pixel32 P, pixel32 Q, pixel32 R, pixel32 S, pixel32 *dest, uint32 width, uint32 height) {
	pixel32 RminusP = R - P;
	pixel32 SminusQ = S - Q;

	P *= height;
	Q *= height;

	uint32	scale = width * height;

	for (uint32 y = 0; y < height; y++, P += RminusP, Q += SminusQ) {
		pixel32 result	= P * width;
		pixel32 dx		= Q - P;
		for (uint32 x = 0; x < width; x++, result += dx)
			*dest++ = result / scale;
	}
}

void InterpolateColours(pixel32 P, pixel32 Q, pixel32 R, pixel32 S, pixel32 (*dest)[8], bool bpp2) {
	uint32 block_width	= bpp2 ? 8 : 4;
	uint32 block_height	= 4;

	//Get vectors.
	pixel32 QminusP = Q - P;
	pixel32 SminusR = S - R;

	//Multiply colours.
	P	*= block_width;
	R	*= block_width;

	for (uint32 x = 0; x < block_width; x++, P += QminusP, R += SminusR) {
		pixel32 result	= P * 4;
		pixel32 dy		= R - P;
		for (uint32 y = 0; y < block_height; y++, result += dy) {
			dest[y][x] = bpp2 ? pixel32(
				(result.r >> 7) + (result.r >> 2),
				(result.g >> 7) + (result.g >> 2),
				(result.b >> 7) + (result.b >> 2),
				(result.a >> 5) + (result.a >> 1)
			) : pixel32(
				(result.r >> 6) + (result.r >> 1),
				(result.g >> 6) + (result.g >> 1),
				(result.b >> 6) + (result.b >> 1),
				(result.a >> 4) + result.a
			);
		}
	}
}

void UnpackModulations(const PVRTCrec& word, uint8 *mod_values, uint8 *mod_modes, bool bpp2) {
	uint8	mode	= word.col_data & 1;
	uint32	bits	= word.mod_data;

	if (bpp2) {
		static const uint8 mods[4] = {0, 3, 5, 8};
		if (mode) {				// determine which of the three modes are in use:
			if (bits & 1) {		// If this is the either the H-only or V-only interpolation mode...
				mode = bits & (1 << 20) ? 3 : 2;	// look at the "LSB" for the "centre" (V=2,H=4) texel. Its LSB is now actually used to indicate whether it's the H-only mode or the V-only...
				bits = (bits & ~(1 << 20)) | ((bits >> 1) & (1<<20));
			}
			bits = (bits & ~1) | ((bits >> 1) & 1);
		}
		for (int y = 0; y < 4; y++, mod_values += 16, mod_modes += 16) {
			for (int x = 0; x < 8; x++) {
				mod_modes[x]	= mode;
				if (mode == 0) {
					mod_values[x]	= bits & 1 ? 8  : 0;
					bits >>= 1;
				} else if (((x ^ y) & 1) == 0) {
					mod_modes[x]	= 0;
					mod_values[x]	= mods[bits & 3];
					bits >>= 2;
				}
			}
		}
	} else {
		static const uint8	mods[2][4] = {
			{0, 2, 5, 8}, {0, 4, 0x14, 8}
		};
		for (int y = 0; y < 4; y++, mod_values += 16) {
			for (int x = 0; x < 4; x++) {
				mod_values[x] = mods[mode][bits & 3];
				bits >>= 2;
			}
		}
	}
}

uint8 GetModulationValue(uint8 *mod_values, uint8 *mod_modes, uint32 x, uint32 y, bool bpp2) {
	uint8	*p = mod_values + y * 16 + x;
	if (bpp2) {
		switch (mod_modes[y * 16 + x]) {
			case 0:		return p[0];
			case 1:		return (p[ -1] + p[ +1] + p[-16] + p[+16] + 2) / 4;	// H&V interpolation...
			case 2:		return (p[ -1] + p[ +1] + 1) / 2;					// H-Only
			default:	return (p[-16] + p[+16] + 1) / 2;					// V-Only
		};
	}
	return p[0];
}

static void GetDecompressedPixels(const PVRTCrec& P, const PVRTCrec& Q, const PVRTCrec& R, const PVRTCrec& S, pixel8 (*dest)[8], bool bpp2) {
	uint8	mod_values[8][16];
	uint8	mod_modes[8][16];
	pixel32 upscaledColourA[4][8];
	pixel32 upscaledColourB[4][8];

	uint32 block_width	= bpp2 ? 8 : 4;
	uint32 block_height	= 4;

	//Get the modulations from each word.
	UnpackModulations(P, &mod_values[0][0],							&mod_modes[0][0],						bpp2);
	UnpackModulations(Q, &mod_values[0][block_width],				&mod_modes[0][block_width],				bpp2);
	UnpackModulations(R, &mod_values[block_height][0],				&mod_modes[block_height][0],			bpp2);
	UnpackModulations(S, &mod_values[block_height][block_width],	&mod_modes[block_height][block_width],	bpp2);

	// Bilinear upscale image data from 2x2 -> 4x4
	InterpolateColours(P.ColourA(), Q.ColourA(), R.ColourA(), S.ColourA(), upscaledColourA, bpp2);
	InterpolateColours(P.ColourB(), Q.ColourB(), R.ColourB(), S.ColourB(), upscaledColourB, bpp2);

	for (uint32 y = 0; y < block_height; y++) {
		for (uint32 x = 0; x < block_width; x++) {
			int8	val	= GetModulationValue(mod_values[0], mod_modes[0], x + block_width / 2, y + block_height / 2, bpp2);
			int8	mod	= val & 0xf;

			pixel32 result = (upscaledColourA[y][x] * (8 - mod) + upscaledColourB[y][x] * mod) / 8;
			if (val & 0x10)
				result.a = 0;

			dest[y][x] = result;
		}
	}
}

static uint32 twiddle(uint32 width, uint32 height, uint32 x, uint32 y) {
	uint32	m = min(width, height);
	return part_by_1(uint16(y & (m - 1))) | (part_by_1(uint16(x & (m - 1))) << 1) | (((x | y) & (0 - m)) * m);
}

int iso::PVRTCDecompress(const PVRTCrec *srce, pixel8 *dest, uint32 width, uint32 height, uint32 pitch, bool bpp2) {
	uint32 block_width	= bpp2 ? 8 : 4;
	uint32 block_height	= 4;
	int nx = width  / block_width;
	int ny = height / block_height;

	if (dest == 0)
		return nx * ny * sizeof(PVRTCrec);

	pixel8 temp[4][8];
	for (int y1 = 0; y1 < ny; y1++) {
		int	y0 = wrap(y1 - 1, ny);
		for (int x1 = 0; x1 < nx; x1++) {
			int	x0 = wrap(x1 - 1, nx);

			GetDecompressedPixels(
				srce[twiddle(nx, ny, x0, y0)],
				srce[twiddle(nx, ny, x1, y0)],
				srce[twiddle(nx, ny, x0, y1)],
				srce[twiddle(nx, ny, x1, y1)],
				temp,
				bpp2
			);

			pixel8	*destP	= dest + (y0 * block_height + block_height / 2) * pitch + x0 * block_width + block_width / 2;
			pixel8	*destQ	= dest + (y0 * block_height + block_height / 2) * pitch + x1 * block_width;
			pixel8	*destR	= dest + y1 * block_height * pitch + x0 * block_width + block_width / 2;
			pixel8	*destS	= dest + y1 * block_height * pitch + x1 * block_width;
			for (uint32 y = 0; y < block_height / 2; y++) {
				for (uint32 x = 0; x < block_width / 2; x++) {
					destP[y * pitch + x] = temp[y][x];
					destQ[y * pitch + x] = temp[y][x + block_width / 2];
					destR[y * pitch + x] = temp[y + block_height / 2][x];
					destS[y * pitch + x] = temp[y + block_height / 2][x + block_width / 2];
				}
			}
		}
	}

	//Return the data size
	return width * height / (block_width / 2);
}
//-----------------------------------------------------------------------------
//	PVRTC compression
//-----------------------------------------------------------------------------

uint32 MakeColourA(pixel32 c) {
	return c.a < 0xf0
		? ((c.a >> 5) << 12) | ((c.r >> 4) << 8) | ((c.g >> 4) << 4) | ((c.b >> 5) << 1)
		: 0x8000 | ((c.r >> 3) << 10) | ((c.g >> 3) <<  5) | ((c.b >> 4) <<  1);
}
uint32 MakeColourB(pixel32 c) {
	return c.a < 0xf0
    	? ((c.a >> 5) << 28) | ((c.r >> 4) << 24) | ((c.g >> 4) << 20) | ((c.b >> 4) << 16)
		: 0x80000000 | ((c.r >> 3) << 26) | ((c.g >> 3) << 21) | ((c.b >> 3) << 16);
}


int iso::PVRTCompress(const pixel8 *src, PVRTCrec *dest, uint32 width, uint32 height, uint32 pitch, bool bpp2) {
	uint32	block_width		= bpp2 ? 8 : 4;
	uint32	block_height	= 4;
	int		nx				= width  / block_width;
	int		ny				= height / block_height;

	if (dest == 0)
		return nx * ny * sizeof(PVRTCrec);

	temp_array<pixel32>	coloura(nx * ny);
	temp_array<pixel32>	colourb(nx * ny);

	// make blurry map

	temp_array<pixel32>	rows[7] = { nx };

	static const uint8 kernel[] = {1,2,3,4,3,2,1};
	for (int y = 0; y < height + 3; y++) {
		const pixel8	*s = src + wrap(y - 1, height) * pitch;
		pixel32			*d = rows[y % 7];

		for (int x = 0; x < nx; x++) {
			pixel32	t(0,0,0,0);
			for (int i = 0; i < 7; i++)
				t += s[wrap(x * block_width + i - 1, width)] * kernel[i];
			d[x] = t;
		}

		if (y >= 6 && (y % block_height) == 6 - block_height) {
			pixel32	*d2 = coloura + (y - 6) / block_height * nx;
			for (int x = 0; x < nx; x++) {
				pixel32	t(0,0,0,0);
				for (int i = 0; i < 7; i++)
					t += rows[(y + i) % 7][x] * kernel[i];
				d2[x] = t / 256;
			}
		}
	}

	// calculate diffs from blurry map

	temp_array<pixel32>	diffs(width * height);

	for (int y1 = 0; y1 < ny; y1++) {
		int	y0 = wrap(y1 - 1, ny);
		for (int x1 = 0; x1 < nx; x1++) {
			int	x0 = wrap(x1 - 1, nx);

			pixel32 interp[32], *pinterp = interp;
			InterpolateColours(
				coloura[y0 * nx + x0],
				coloura[y0 * nx + x1],
				coloura[y1 * nx + x0],
				coloura[y1 * nx + x1],
				interp, block_width, block_height
			);

			for (uint32 y = 0; y < block_height; y++) {
				uint32			y2	= wrap(y1 * block_height + y - block_height / 2, height);
				const pixel8	*s	= src + y2 * pitch;
				pixel32			*d	= diffs + y2 * width;
				for (uint32 x = 0; x < block_width; x++) {
					uint32	x2	= wrap(x1 * block_width + x - block_width / 2, width);
					d[x2]		= s[x2] - *pinterp++;
				}
			}
		}
	}

	// blur diffs

	temp_array<pixel32>	bdiffs(nx * ny);

	for (int y = 0; y < height + 3; y++) {
		pixel32	*s = diffs + wrap(y - 1, height) * width;
		pixel32	*d = rows[y % 7];

		for (int x = 0; x < nx; x++) {
			pixel32	t(0,0,0,0);
			for (int i = 0; i < 7; i++)
				t += abs(s[wrap(x * block_width + i - 1, width)]) * kernel[i];
			d[x] = t;
		}

		if (y >= 6 && (y % block_height) == 6 - block_height) {
			uint32	off	= (y - 6) / block_height * nx;
			for (int x = 0; x < nx; x++) {
				pixel32	t(0,0,0,0);
				for (int i = 0; i < 7; i++)
					t += rows[(y + i) % 7][x] * kernel[i];
				bdiffs[off + x] = t / 256;
			}
		}
	}

	// calculate modulations

	temp_array<uint8>	mods(width * height);

	for (int y1 = 0; y1 < ny; y1++) {
		int	y0 = wrap(y1 - 1, ny);
		for (int x1 = 0; x1 < nx; x1++) {
			int	x0 = wrap(x1 - 1, nx);

			pixel32 interp[32], *pinterp = interp;
			InterpolateColours(
				bdiffs[y0 * nx + x0],
				bdiffs[y0 * nx + x1],
				bdiffs[y1 * nx + x0],
				bdiffs[y1 * nx + x1],
				interp, block_width, block_height
			);

			for (uint32 y = 0; y < block_height; y++) {
				uint32	y2	= wrap(y1 * block_height + y - block_height / 2, height);
				pixel32	*s	= diffs + y2 * width;
				uint8	*d	= mods + y2 * width;
				for (uint32 x = 0; x < block_width; x++) {
					uint32	x2	= wrap(x1 * block_width + x - block_width / 2, width);
					const pixel32	&a	= s[x2];
					const pixel32	&b	= *pinterp++;
					float	m	= dotp(a, b) / sqrt(float(dotp(a,a) * dotp(b,b)));
					float	m2	= 4 * (m + 1);
					d[x2]	= m2 < 1.5f ? 0
							: m2 < 4	? 1
							: m2 < 6.5f ? 2
							: 3;
				}
			}
		}
	}

	//store results

	for (int y = 0; y < ny; y++) {
		for (int x = 0; x < nx; x++) {
			PVRTCrec	&d	= dest[twiddle(nx, ny, x, y)];
			uint32		mod = 0;

			if (bpp2) {
				//mode 0
				for (uint32 y0 = block_height; y0--; ) {
					uint8	*p	= mods + (y * block_height + y0) * width + x * block_width;
					for (uint32 x0 = block_width; x0--; )
						mod	= (mod << 1) | (p[x0] >> 1);
				}
			} else {
				for (uint32 y0 = block_height; y0--; ) {
					uint8	*p	= mods + (y * block_height + y0) * width + x * block_width;
					mod	= (mod << 8) | (p[0] | (p[1] << 2) | (p[2] << 4) | (p[3] << 6));
				}
			}

			d.mod_data	= mod;

			uint32	off = y * nx + x;
			d.col_data	= MakeColourA(clamp(coloura[off] - bdiffs[off])) | MakeColourB(clamp(coloura[off] + bdiffs[off]));
//			d.mod_data	= y < ny / 2 ? 0 : 0xffffffff;
		}
	}

	//Return the data size
	return nx * ny * sizeof(PVRTCrec);
}
