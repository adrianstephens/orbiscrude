#include "common.fxh"
#include "vpx.fxh"

//-----------------------------------------------------------------------------
//	INTER-PREDICTION
//-----------------------------------------------------------------------------

#ifdef PLAT_PS4

struct INTERtextures {
	RWTexture2D<uint>	dest;
	Texture2D<uint>		src[3];
};

struct INTERbuffers {
	Buffer<uint>		info;
	Buffer<uint>		mv;
	INTERtextures		*textures;
};

#else

Buffer<uint>			buffers_mv		: register(t0);
Buffer<uint>			buffers_info	: register(t1);
Texture2D<uint>			buffers_src[3]	: register(t2);
RWTexture2D<uint>		buffers_dest	: register(u0);

#endif

#define SUBPEL_TAPS		8
#define SUBPEL_BITS		4
#define SUBPEL_FACTOR	(1 << SUBPEL_BITS)
#define SUBPEL_MASK		(SUBPEL_FACTOR - 1)
#define FILTER_BITS	7

static const uint2 filters[] = {
// INTERP_8TAP
	{PACK8bo(-1,	 0,   0,   0, 128,   0,   0,   0,  0)},
	{PACK8bo(-1,	 0,   1,  -5, 126,   8,  -3,   1,  0)},
	{PACK8bo(-1,	-1,   3, -10, 122,  18,  -6,   2,  0)},
	{PACK8bo(-1,	-1,   4, -13, 118,  27,  -9,   3, -1)},
	{PACK8bo(-1,	-1,   4, -16, 112,  37, -11,   4, -1)},
	{PACK8bo(-1,	-1,   5, -18, 105,  48, -14,   4, -1)},
	{PACK8bo(-1,	-1,   5, -19,  97,  58, -16,   5, -1)},
	{PACK8bo(-1,	-1,   6, -19,  88,  68, -18,   5, -1)},
	{PACK8bo(-1,	-1,   6, -19,  78,  78, -19,   6, -1)},
	{PACK8bo(-1,	-1,   5, -18,  68,  88, -19,   6, -1)},
	{PACK8bo(-1,	-1,   5, -16,  58,  97, -19,   5, -1)},
	{PACK8bo(-1,	-1,   4, -14,  48, 105, -18,   5, -1)},
	{PACK8bo(-1,	-1,   4, -11,  37, 112, -16,   4, -1)},
	{PACK8bo(-1,	-1,   3,  -9,  27, 118, -13,   4, -1)},
	{PACK8bo(-1,	 0,   2,  -6,  18, 122, -10,   3, -1)},
	{PACK8bo(-1,	 0,   1,  -3,   8, 126,  -5,   1,  0)},

// INTERP_8TAP_SMOOTH
	{PACK8bo(-1,	 0,   0,   0, 128,   0,   0,   0,  0)},
	{PACK8bo(-1,	-3,  -1,  32,  64,  38,   1,  -3,  0)},
	{PACK8bo(-1,	-2,  -2,  29,  63,  41,   2,  -3,  0)},
	{PACK8bo(-1,	-2,  -2,  26,  63,  43,   4,  -4,  0)},
	{PACK8bo(-1,	-2,  -3,  24,  62,  46,   5,  -4,  0)},
	{PACK8bo(-1,	-2,  -3,  21,  60,  49,   7,  -4,  0)},
	{PACK8bo(-1,	-1,  -4,  18,  59,  51,   9,  -4,  0)},
	{PACK8bo(-1,	-1,  -4,  16,  57,  53,  12,  -4, -1)},
	{PACK8bo(-1,	-1,  -4,  14,  55,  55,  14,  -4, -1)},
	{PACK8bo(-1,	-1,  -4,  12,  53,  57,  16,  -4, -1)},
	{PACK8bo(-1,	 0,  -4,   9,  51,  59,  18,  -4, -1)},
	{PACK8bo(-1,	 0,  -4,   7,  49,  60,  21,  -3, -2)},
	{PACK8bo(-1,	 0,  -4,   5,  46,  62,  24,  -3, -2)},
	{PACK8bo(-1,	 0,  -4,   4,  43,  63,  26,  -2, -2)},
	{PACK8bo(-1,	 0,  -3,   2,  41,  63,  29,  -2, -2)},
	{PACK8bo(-1,	 0,  -3,   1,  38,  64,  32,  -1, -3)},

// INTERP_8TAP_SHARP
	{PACK8bo(-1,	 0,   0,   0, 128,   0,   0,   0,  0)},
	{PACK8bo(-1,	-1,   3,  -7, 127,   8,  -3,   1,  0)},
	{PACK8bo(-1,	-2,   5, -13, 125,  17,  -6,   3, -1)},
	{PACK8bo(-1,	-3,   7, -17, 121,  27, -10,   5, -2)},
	{PACK8bo(-1,	-4,   9, -20, 115,  37, -13,   6, -2)},
	{PACK8bo(-1,	-4,  10, -23, 108,  48, -16,   8, -3)},
	{PACK8bo(-1,	-4,  10, -24, 100,  59, -19,   9, -3)},
	{PACK8bo(-1,	-4,  11, -24,  90,  70, -21,  10, -4)},
	{PACK8bo(-1,	-4,  11, -23,  80,  80, -23,  11, -4)},
	{PACK8bo(-1,	-4,  10, -21,  70,  90, -24,  11, -4)},
	{PACK8bo(-1,	-3,   9, -19,  59, 100, -24,  10, -4)},
	{PACK8bo(-1,	-3,   8, -16,  48, 108, -23,  10, -4)},
	{PACK8bo(-1,	-2,   6, -13,  37, 115, -20,   9, -4)},
	{PACK8bo(-1,	-2,   5, -10,  27, 121, -17,   7, -3)},
	{PACK8bo(-1,	-1,   3,  -6,  17, 125, -13,   5, -2)},
	{PACK8bo(-1,	 0,   1,  -3,   8, 127,  -7,   3, -1)},

//INTERP_BILINEAR
	{PACK8bo(-1,	 0,   0,   0, 128,   0,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0, 120,   8,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0, 112,  16,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0, 104,  24,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  96,  32,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  88,  40,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  80,  48,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  72,  56,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  64,  64,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  56,  72,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  48,  80,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  40,  88,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  32,  96,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  24, 104,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,  16, 112,   0,   0,  0)},
	{PACK8bo(-1,	 0,   0,   0,   8, 120,   0,   0,  0)}
};

thread_local	uint	temp_convolve0[24 * 24];
thread_local	uint	temp_convolve1[24 * 24];

int2 unpack_mv(uint x) {
	return int2(x, x << 16) >> 16;
}

int		round_q4(int value)						{ return (value < 0 ? value - 2 : value + 2) / 4; }
int		round_q2(int value)						{ return (value < 0 ? value - 1 : value + 1) / 2; }
int2	average(int2 a, int2 b)					{ return int2(round_q2(a.x + b.x), round_q2(a.y + b.y)); }
int2	average(int2 a, int2 b, int2 c, int2 d)	{ return int2(round_q4(a.x + b.x + c.x + d.x), round_q4(a.y + b.y + c.y + d.y)); }

void add_clamp_pixel(RWTexture2D<uint> dest, int2 uv, uint x) {
	dest[uv] = clamp(int(dest[uv] + x) - 128, 0, 255);
}

int convolve(const uint table[24 * 24], const uint2 filters, int i, int di) {
	return	dot(int4(table[i + di * 0], table[i + di * 1], table[i + di * 2], table[i + di * 3]), unpack4i(filters.x) + 1)
		+	dot(int4(table[i + di * 4], table[i + di * 5], table[i + di * 6], table[i + di * 7]), unpack4i(filters.y) + 1);
}

#if 0
void read2x2(out uint a[], uint i, Texture2D<uint> src, int2 uv, int2 minuv, int2 maxuv) {
	a[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), minuv, maxuv)];
	a[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), minuv, maxuv)];
	a[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), minuv, maxuv)];
	a[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), minuv, maxuv)];
}
#else
#define read2x2(a, i, src, uv, minuv, maxuv)\
	a[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), minuv, maxuv)];\
	a[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), minuv, maxuv)];\
	a[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), minuv, maxuv)];\
	a[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), minuv, maxuv)]
#define read3x2(a, i, src, uv, minuv, maxuv)\
	read2x2(a, i, src, uv, minuv, maxuv);\
	a[i + 0 * 24 + 4]	= src[clamp(uv + int2(+1, -3), minuv, maxuv)];\
	a[i + 8 * 24 + 4]	= src[clamp(uv + int2(+1, +5), minuv, maxuv)]
#define read2x3(a, i, src, uv, minuv, maxuv)\
	read2x2(a, i, src, uv, minuv, maxuv);\
	a[i + 4 * 24 + 0]	= src[clamp(uv + int2(-3, +1), minuv, maxuv)];\
	a[i + 4 * 24 + 8]	= src[clamp(uv + int2(+5, +1), minuv, maxuv)]
#define read3x3(a, i, src, uv, minuv, maxuv)\
	read3x2(a, i, src, uv, minuv, maxuv);\
	a[i + 4 * 24 + 0]	= src[clamp(uv + int2(-3, +1), minuv, maxuv)];\
	a[i + 4 * 24 + 4]	= src[clamp(uv + int2(+1, +1), minuv, maxuv)];\
	a[i + 4 * 24 + 8]	= src[clamp(uv + int2(+5, +1), minuv, maxuv)]
#endif

uint inter_y1(uint2 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int mvoffset, Texture2D<uint> src) {
	uint2	size;
	src.GetDimensionsFast(size.x, size.y);
	int2	maxuv = int2(size.x - 1, size.y * 2 / 3 - 1);

	int2	mv;
	uint	i;
	switch (split) {
		default:
		case 0: {
			mv		= unpack_mv(mvbuffer[mvoffset]) * 2;
			i		= threadid.x + threadid.y * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			read2x2(temp_convolve0, i, src, uv, int2(0, 0), maxuv);
			break;
		}
		case 1: {
			mv		= unpack_mv(mvbuffer[mvoffset + (threadid.x/4)]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + threadid.y * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			read3x2(temp_convolve0, i, src, uv, int2(0, 0), maxuv);
			break;
		}
		case 2: {
			mv		= unpack_mv(mvbuffer[mvoffset + (threadid.y/4)]) * 2;
			i		= threadid.x + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			read2x3(temp_convolve0, i, src, uv, int2(0, 0), maxuv);
			break;
		}
		case 3: {
			mv		= unpack_mv(mvbuffer[mvoffset + (threadid.x/4) + (threadid.y/4) * 2]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			read3x3(temp_convolve0, i, src, uv, int2(0, 0), maxuv);
			break;
		}
	}

	ThreadGroupMemoryBarrierSync();

	// horizontal convolve
	uint	f1		= (mv.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f2		= (mv.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum1	= convolve(temp_convolve0, filters[f1], i + 0 * 24, 1);
	uint	sum2	= convolve(temp_convolve0, filters[f2], i + 8 * 24, 1);

	ThreadGroupMemoryBarrierSync();

	temp_convolve0[i + 0 * 24] = sum1;
	temp_convolve0[i + 8 * 24] = sum2;

	ThreadGroupMemoryBarrierSync();

	// vertical convolve
	uint	f		= (mv.y & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum		= convolve(temp_convolve0, filters[f], i, 24);
	return sum;//round_pow2(sum, FILTER_BITS * 2);
}

uint inter_y2(uint2 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int mvoffset, Texture2D<uint> src0, Texture2D<uint> src1) {
	uint2	size0, size1;
	src0.GetDimensionsFast(size0.x, size0.y);
	src1.GetDimensionsFast(size1.x, size1.y);
	int2	max0	= int2(size0.x - 1, size0.y * 2 / 3 - 1);
	int2	max1	= int2(size1.x - 1, size1.y * 2 / 3 - 1);

	int2	mv0, mv1;
	uint	i;
	switch (split) {
		case 0: {
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + threadid.y * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			read2x2(temp_convolve0, i, src0, uv0, int2(0, 0), max0);
			read2x2(temp_convolve1, i, src1, uv1, int2(0, 0), max1);
			break;
		}
		case 1: {
			mvoffset += (threadid.x/4) * 2;
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + threadid.y * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			read3x2(temp_convolve0, i, src0, uv0, int2(0, 0), max0);
			read3x2(temp_convolve1, i, src1, uv1, int2(0, 0), max1);

			break;
		}
		case 2: {
			mvoffset += (threadid.y/4) * 2;
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			read2x3(temp_convolve0, i, src0, uv0, int2(0, 0), max0);
			read2x3(temp_convolve1, i, src1, uv1, int2(0, 0), max1);
			break;
		}
		case 3: {
			mvoffset += ((threadid.x/4) + (threadid.y/4) * 2) * 2;
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			read3x3(temp_convolve0, i, src0, uv0, int2(0, 0), max0);
			read3x3(temp_convolve1, i, src1, uv1, int2(0, 0), max1);
			break;
		}
	}
	
	ThreadGroupMemoryBarrierSync();

	// horizontal convolve
	uint	f01		= (mv0.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f02		= (mv0.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f11		= (mv1.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f12		= (mv1.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum01	= convolve(temp_convolve0, filters[f01], i + 0 * 24, 1);
	uint	sum02	= convolve(temp_convolve0, filters[f02], i + 8 * 24, 1);
	uint	sum11	= convolve(temp_convolve1, filters[f11], i + 0 * 24, 1);
	uint	sum12	= convolve(temp_convolve1, filters[f12], i + 8 * 24, 1);

	ThreadGroupMemoryBarrierSync();

	temp_convolve0[i + 0 * 24] = sum01;
	temp_convolve0[i + 8 * 24] = sum02;
	temp_convolve1[i + 0 * 24] = sum11;
	temp_convolve1[i + 8 * 24] = sum12;

	ThreadGroupMemoryBarrierSync();

	// vertical convolve
	uint	f0		= (mv0.y & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum0	= convolve(temp_convolve0, filters[f0], i, 24);
	uint	f1		= (mv1.y & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum1	= convolve(temp_convolve1, filters[f1], i, 24);

	return sum0 + sum1;//round_pow2(sum0 + sum1, FILTER_BITS * 2 + 1);
}

#ifdef PLAT_PS4

COMPUTE_SHADER(8,8,1)
void inter_y(uint2 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID, INTERbuffers buffers : S_SRT_DATA) {
	uint	width, height;
	buffers.textures->dest.GetDimensionsFast(width, height);

	uint	u		= buffers.info[groupid.y * (width / 8) + groupid.x];
	uint	ref0	= (u >> 2) & 3;

	if (ref0) {
		uint	filter	= u & 3;
		uint	ref1	= (u >> 4) & 3;
		uint	split	= (u >> 6) & 3;
		uint	mvoffset= u >> 8;

		uint2	pos		= groupid * 8 + threadid;
		uint	texel	= ref1
			? round_pow2(inter_y2(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1], buffers.textures->src[ref1 - 1]), FILTER_BITS * 2 + 1)
			: round_pow2(inter_y1(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1]), FILTER_BITS * 2);

		add_clamp_pixel(buffers.textures->dest, pos, texel);
	}
}

#else

COMPUTE_SHADER(8,8,1)
void inter_y(uint2 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID) {
	uint	width, height;
	buffers_dest.GetDimensionsFast(width, height);

	uint	u		= buffers_info[groupid.y * (width / 8) + groupid.x];
	uint	ref0	= (u >> 2) & 3;

	if (ref0) {
		uint	filter	= u & 3;
		uint	ref1	= (u >> 4) & 3;
		uint	split	= (u >> 6) & 3;
		uint	mvoffset= u >> 8;

		uint2	pos		= groupid * 8 + threadid;
		uint	texel;
		switch (ref0) {
			case 1: texel = inter_y1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[0]); break;
			case 2: texel = inter_y1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[1]); break;
			case 3: texel = inter_y1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[2]); break;
		}
		switch (ref1) {
			case 0: texel += texel; break;
			case 1: texel += inter_y1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[0]); break;
			case 2: texel += inter_y1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[1]); break;
			case 3: texel += inter_y1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[2]); break;
		}
		add_clamp_pixel(buffers_dest, pos, round_pow2(texel, FILTER_BITS * 2 + 1));
	}
}

#endif

uint inter_uv1(uint3 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int mvoffset, Texture2D<uint> src) {
	int2	mv	= unpack_mv(mvbuffer[mvoffset]);
	if (split) {
		mv	= split < 3 ? average(mv, unpack_mv(mvbuffer[mvoffset + 1]))
			: average(mv, unpack_mv(mvbuffer[mvoffset + 1]), unpack_mv(mvbuffer[mvoffset + 2]), unpack_mv(mvbuffer[mvoffset + 3]));
	}

	int2	uv	= pos + mv / SUBPEL_FACTOR;
	uint	i	= threadid.x + threadid.y * 24 + threadid.z * 12;

	uint2	size;
	src.GetDimensionsFast(size.x, size.y);
	int2	minuv = int2(threadid.z * size.x / 2, size.y * 2 / 3);
	int2	maxuv = int2(minuv.x + size.x / 2, size.y) - 1;

	read3x3(temp_convolve0, i, src, uv, minuv, maxuv);

	ThreadGroupMemoryBarrierSync();

	// horizontal convolve
	uint	f1		= (mv.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f2		= (mv.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f3		= (mv.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum1	= convolve(temp_convolve0, filters[f1], i + 0 * 24, 1);
	uint	sum2	= convolve(temp_convolve0, filters[f2], i + 4 * 24, 1);
	uint	sum3	= convolve(temp_convolve0, filters[f2], i + 8 * 24, 1);

	ThreadGroupMemoryBarrierSync();

	temp_convolve0[i + 0 * 24] = sum1;
	temp_convolve0[i + 4 * 24] = sum2;
	temp_convolve0[i + 8 * 24] = sum3;

	ThreadGroupMemoryBarrierSync();

	// vertical convolve
	uint	f		= (mv.y & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum		= convolve(temp_convolve0, filters[f], i, 24);
	return sum;//round_pow2(sum, FILTER_BITS * 2);
}

uint inter_uv2(uint3 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int mvoffset, Texture2D<uint> src0, Texture2D<uint> src1) {
	int2	mv0	= unpack_mv(mvbuffer[mvoffset + 0]);
	int2	mv1	= unpack_mv(mvbuffer[mvoffset + 1]);
	if (split) {
		if (split < 3) {
			mv0	= average(mv0, unpack_mv(mvbuffer[mvoffset + 2]));
			mv1	= average(mv1, unpack_mv(mvbuffer[mvoffset + 3]));
		} else {
			mv0 = average(mv0, unpack_mv(mvbuffer[mvoffset + 2]), unpack_mv(mvbuffer[mvoffset + 4]), unpack_mv(mvbuffer[mvoffset + 6]));
			mv1 = average(mv1, unpack_mv(mvbuffer[mvoffset + 3]), unpack_mv(mvbuffer[mvoffset + 5]), unpack_mv(mvbuffer[mvoffset + 7]));
		}
	}
	
	int2	uv0 = pos + mv0 / SUBPEL_FACTOR;
	int2	uv1 = pos + mv1 / SUBPEL_FACTOR;
	uint	i	= threadid.x + threadid.y * 24 + threadid.z * 12;

	uint2	size0, size1;
	src0.GetDimensionsFast(size0.x, size0.y);
	src1.GetDimensionsFast(size1.x, size1.y);

	int2	min0 = int2(threadid.z * size0.x / 2, size0.y * 2 / 3);
	int2	max0 = int2(min0.x + size0.x / 2, size0.y) - 1;
	int2	min1 = int2(threadid.z * size1.x / 2, size1.y * 2 / 3);
	int2	max1 = int2(min1.x + size0.x / 2, size1.y) - 1;

	read3x3(temp_convolve0, i, src0, uv0, min0, max0);
	read3x3(temp_convolve1, i, src1, uv1, min1, max1);

	ThreadGroupMemoryBarrierSync();

	// horizontal convolve
	uint	f01		= (mv0.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f02		= (mv0.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f03		= (mv0.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f11		= (mv1.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f12		= (mv1.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	f13		= (mv1.x & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum01	= convolve(temp_convolve0, filters[f01], i + 0 * 24, 1);
	uint	sum02	= convolve(temp_convolve0, filters[f02], i + 4 * 24, 1);
	uint	sum03	= convolve(temp_convolve0, filters[f03], i + 8 * 24, 1);
	uint	sum11	= convolve(temp_convolve1, filters[f11], i + 0 * 24, 1);
	uint	sum12	= convolve(temp_convolve1, filters[f12], i + 4 * 24, 1);
	uint	sum13	= convolve(temp_convolve1, filters[f13], i + 8 * 24, 1);

	ThreadGroupMemoryBarrierSync();

	temp_convolve0[i + 0 * 24] = sum01;
	temp_convolve0[i + 4 * 24] = sum02;
	temp_convolve0[i + 8 * 24] = sum03;
	temp_convolve1[i + 0 * 24] = sum11;
	temp_convolve1[i + 4 * 24] = sum12;
	temp_convolve1[i + 8 * 24] = sum13;

	ThreadGroupMemoryBarrierSync();

	// vertical convolve
	uint	f0		= (mv0.y & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum0	= convolve(temp_convolve0, filters[f0], i, 24);
	uint	f1		= (mv1.y & SUBPEL_MASK) + (filter << SUBPEL_BITS);
	uint	sum1	= convolve(temp_convolve1, filters[f1], i, 24);

	return sum0 + sum1;//round_pow2(sum0 + sum1, FILTER_BITS * 2 + 1);
}

#ifdef PLAT_PS4

COMPUTE_SHADER(4,4,2)
void inter_uv(uint3 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID, INTERbuffers buffers : S_SRT_DATA) {
	uint	width, height;
	buffers.textures->dest.GetDimensionsFast(width, height);

	uint	u		= buffers.info[groupid.y * (width / 8) + groupid.x];
	uint	ref0	= (u >> 2) & 3;

	if (ref0) {
		uint	filter	= u & 3;
		uint	ref1	= (u >> 4) & 3;
		uint	split	= (u >> 6) & 3;
		uint	mvoffset= u >> 8;

		RWTexture2D<uint>	dest = buffers.textures->dest;
		uint2				size;
		dest.GetDimensionsFast(size.x, size.y);

		uint2	pos		= groupid * 4 + threadid.xy + int2(threadid.z * size.x / 2, size.y * 2 / 3);
		uint	texel	= ref1
			? round_pow2(inter_uv2(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1], buffers.textures->src[ref1 - 1]), FILTER_BITS * 2 + 1)
			: round_pow2(inter_uv1(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1]), FILTER_BITS * 2);

		add_clamp_pixel(dest, pos, texel);
	}
}

#else

COMPUTE_SHADER(4,4,2)
void inter_uv(uint3 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID) {
	uint	width, height;
	buffers_dest.GetDimensionsFast(width, height);
	
	uint	u		= buffers_info[groupid.y * (width / 8) + groupid.x];
	uint	ref0	= (u >> 2) & 3;

	if (ref0) {
		uint	filter	= u & 3;
		uint	ref1	= (u >> 4) & 3;
		uint	split	= (u >> 6) & 3;
		uint	mvoffset= u >> 8;

		uint2				size;
		buffers_dest.GetDimensionsFast(size.x, size.y);

		uint2	pos		= groupid * 4 + threadid.xy + int2(threadid.z * size.x / 2, size.y * 2 / 3);
		uint	texel;
		switch (ref0) {
			case 1: texel = inter_uv1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[0]); break;
			case 2: texel = inter_uv1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[1]); break;
			case 3: texel = inter_uv1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[2]); break;
		}
		switch (ref1) {
			case 0: texel += texel; break;
			case 1: texel += inter_uv1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[0]); break;
			case 2: texel += inter_uv1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[1]); break;
			case 3: texel += inter_uv1(threadid, pos, split, filter, buffers_mv, mvoffset, buffers_src[2]); break;
		}

		add_clamp_pixel(buffers_dest, pos, round_pow2(texel, FILTER_BITS * 2 + 1));
	}
}

#endif

technique inter_predictions {
	pass y	{ SET_CS(inter_y); }
	pass uv { SET_CS(inter_uv); }
};


//-----------------------------------------------------------------------------
//	YUV CONVERSION
//-----------------------------------------------------------------------------

static const float3x3 yuv_matrix = {
	1.164f,  1.164f, 1.164f,
	  0.0f, -0.392f, 2.017f,
	1.596f, -0.813f,   0.0f,
};

float3 yuv2rgb(float3 yuv) {
	return mul(yuv, yuv_matrix);
}

void yuv_conv0(RWTexture2D<float4> dest, Texture2D<uint> srce, int2 uv) {
	uint2	size;
	dest.GetDimensionsFast(size.x, size.y);
	dest[uv] = float4(yuv2rgb(int3(
		srce[uv] - 16,
		srce[uv / 2 + int2(0, size.y)] - 128,
		srce[uv / 2 + int2(size.x / 2, size.y)] - 128
	) / 255.0), 1);
}


#ifdef PLAT_PS4

struct YUVbuffers {
	RWTexture2D<float4>	dest;
	Texture2D<uint>		srce;
};

COMPUTE_SHADER(8,8,1)
void yuv_conv(uint2 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID, YUVbuffers buffers : S_SRT_DATA) {
	yuv_conv0(buffers.dest, buffers.srce, groupid * 8 + threadid);
}

#else

RWTexture2D<float4>	yuv_dest;
Texture2D<uint>		yuv_srce;

COMPUTE_SHADER(8,8,1)
void yuv_conv(uint2 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID) {
	yuv_conv0(yuv_dest, yuv_srce, groupid * 8 + threadid);
}

#endif

technique yuv_conversion {
	pass p0	{ SET_CS(yuv_conv); }
};