//set to 0 for floats
#define DCT_CONST_BITS	14

#include "dct.fxh"
#include "vpx.fxh"

//-----------------------------------------------------------------------------
//	DCT consts
//-----------------------------------------------------------------------------

#ifdef DCT_FLOAT
float4	dct_texels(float4 o, int n)	{ return o + 0.5;}
#else
int4	dct_texels(int4 o, int n)	{ return clamp(round_pow2(o, n) + 128, 0, 255); }
#endif

#if 0
void put4v(inout T array[], int stride, int x, int y, T4 v) {
	array[stride * (y * 4 + 0) + x] = v.x;
	array[stride * (y * 4 + 1) + x] = v.y;
	array[stride * (y * 4 + 2) + x] = v.z;
	array[stride * (y * 4 + 3) + x] = v.w;
}

T4 get4h(T array[], int stride, int x, int y) {
	return T4(array[stride * y + x * 4 + 0], array[stride * y + x * 4 + 1], array[stride * y + x * 4 + 2], array[stride * y + x * 4 + 3]);
}
#else
#define put4v(ARRAY, STRIDE, X, Y, O)\
	ARRAY[STRIDE * (Y * 4 + 0) + X] = O.x;\
	ARRAY[STRIDE * (Y * 4 + 1) + X] = O.y;\
	ARRAY[STRIDE * (Y * 4 + 2) + X] = O.z;\
	ARRAY[STRIDE * (Y * 4 + 3) + X] = O.w;

#define get4h(ARRAY, STRIDE, X, Y)	T4(ARRAY[STRIDE * Y + X * 4 + 0], ARRAY[STRIDE * Y + X * 4 + 1], ARRAY[STRIDE * Y + X * 4 + 2], ARRAY[STRIDE * Y + X * 4 + 3])
#endif

//-----------------------------------------------------------------------------
//	SHADERS
//-----------------------------------------------------------------------------

struct DCTinfo {
	uint	q;		//uint32	eob:10, q0:11, q1:11;
	uint	pos;	//uint16	x, y;
	uint	coeffs;
};

int get_eob(DCTinfo info) {
	return (info.q & ((1 << 10) - 1)) + 1;
}
int4 get_quant(DCTinfo info, uint threadid) {
	uint2	q = uint2(info.q << (32 - 21), info.q) >> (32 - 11);
	return threadid == 0 ? int4(q.x, q.yyy) : q.yyyy;
}
int get_quant_ac(DCTinfo info) {
	return info.q >> (32 - 11);
}
uint2 get_pos(DCTinfo info) {
	return uint2(info.pos << 16, info.pos) >> 16;
}

#ifdef PLAT_PS4

#define	CoeffBuffer		Buffer<T>
#define OutputTex		RWTexture2D<T4>

struct DCTbuffers {
	RegularBuffer<DCTinfo>	info;
	BufferDescriptor		coeffs;
	RWTexture2D<T4>			output;
};


Buffer<T> get_coeff_buffer(BufferDescriptor desc, DCTinfo info) {
	desc.m_regs[0]	+= info.coeffs * 2;
	desc.m_regs[2]	=  get_eob(info);
	return Buffer<T>(desc);
}

int4 unpack_get4n(Buffer<int> input, uint eob, uint x) {
	return int4(
		input[(x >>  0) & 15],
		input[(x >>  4) & 15],
		input[(x >>  8) & 15],
		input[(x >> 12) & 15]
	);
}

int4 unpack_get4(Buffer<int> input, uint eob, uint x) {
	return int4(
		input[(x >>  0) & 255],
		input[(x >>  8) & 255],
		input[(x >> 16) & 255],
		input[(x >> 24) & 255]
	);
}

int4 unpack_get4(Buffer<int> input, uint eob, uint2 x) {
	return int4(
		input[x.x & 0xffff],
		input[x.x >> 16],
		input[x.y & 0xffff],
		input[x.y >> 16]
	);
}

float4 unpack_get4n(Buffer<float> input, uint eob, uint x) {
	return float4(
		input[(x >>  0) & 15],
		input[(x >>  4) & 15],
		input[(x >>  8) & 15],
		input[(x >> 12) & 15]
	);
}

float4 unpack_get4(Buffer<float> input, uint eob, uint x) {
	return float4(
		input[(x >>  0) & 255],
		input[(x >>  8) & 255],
		input[(x >> 16) & 255],
		input[(x >> 24) & 255]
	);
}

float4 unpack_get4(Buffer<float> input, uint eob, uint2 x) {
	return float4(
		input[x.x & 0xffff],
		input[x.x >> 16],
		input[x.y & 0xffff],
		input[x.y >> 16]
	);
}

uint get_scan4x4(nibble16 s, int i) {
	return s >> (i << 4);
}

void output4(OutputTex output, int2 uv, T4 x) {
	output[uv] = x;
}

#else

#define	CoeffBuffer			Buffer<T>
#define OutputTex			RWTexture2D<uint>
#define BufferDescriptor	CoeffBuffer

BufferDescriptor		buffers_coeffs	: register(t0);
RegularBuffer<DCTinfo>	buffers_info	: register(t1);
OutputTex				buffers_output	: register(u0);

T clamped_load(CoeffBuffer input, uint eob, uint x) {
	return x < eob ? input.Load(x) : 0;
}

T4 unpack_get4n(CoeffBuffer input, uint eob, uint x) {
	return int4(
		clamped_load(input, eob, (x >>  0) & 15),
		clamped_load(input, eob, (x >>  4) & 15),
		clamped_load(input, eob, (x >>  8) & 15),
		clamped_load(input, eob, (x >> 12) & 15)
	);
}

T4 unpack_get4(CoeffBuffer input, uint eob, uint x) {
	return int4(
		clamped_load(input, eob, (x >>  0) & 255),
		clamped_load(input, eob, (x >>  8) & 255),
		clamped_load(input, eob, (x >> 16) & 255),
		clamped_load(input, eob, (x >> 24) & 255)
	);
}

T4 unpack_get4(CoeffBuffer input, uint eob, uint2 x) {
	return int4(
		clamped_load(input, eob, x.x & 0xffff),
		clamped_load(input, eob, x.x >> 16),
		clamped_load(input, eob, x.y & 0xffff),
		clamped_load(input, eob, x.y >> 16)
	);
}

uint get_scan4x4(nibble16 s, int i) {
	return s[i >> 1] >> ((i & 1) << 4);
}

void output4(OutputTex output, int2 uv, T4 x) {
	output[uv] = dot(x, uint4(1, 1<<8, 1<<16, 1<<24));
}

#endif

//-----------------------------------------------------------------------------
//	4x4 shaders
//-----------------------------------------------------------------------------

thread_local	T	temp4x4[4 * 4];

static const nibble16 default_iscan_4x4_trans = PACK16n(
	 0,  1,  4,  6,
	 2,  3,  7, 10,
	 5,  9, 11, 13,
	 8, 12, 14, 15
);
static const nibble16 row_iscan_4x4_trans = PACK16n(
	 0,  2,  7, 10,
	 1,  4,  8, 12,
	 3,  6, 11, 14,
	 5,  9, 13, 15
);
static const nibble16 col_iscan_4x4_trans = PACK16n(
	 0,  1,  2,  4,
	 3,  5,  6,  8,
	 7,  9, 10, 13,
	11, 12, 14, 15
);

void iwht4(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint		scan	= get_scan4x4(default_iscan_4x4_trans, threadid);
	uint		eob		= get_eob(info);
	T4	o0;
	iwht4(
		unpack_get4n(coeffs, eob, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iwht4(get4h(temp4x4, 4, 0, threadid), o0);
	output4(output, get_pos(info) + int2(0, threadid), dct_texels(o0, 4));
}

void idct4h_idct4v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint		scan	= get_scan4x4(default_iscan_4x4_trans, threadid);
	uint		eob		= get_eob(info);
	T4	o0;
	idct4(
		unpack_get4n(coeffs, eob, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	idct4(get4h(temp4x4, 4, 0, threadid), o0);
	output4(output, get_pos(info) + int2(0, threadid), dct_texels(o0, 4));
}

void idct4h_iadst4v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint		scan	= get_scan4x4(row_iscan_4x4_trans, threadid);
	uint		eob		= get_eob(info);
	T4	o0;
	iadst4(
		unpack_get4n(coeffs, eob, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	idct4(get4h(temp4x4, 4, 0, threadid), o0);
	output4(output, get_pos(info) + int2(0, threadid), dct_texels(o0, 4));
}

void iadst4h_idct4v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint		scan	= get_scan4x4(col_iscan_4x4_trans, threadid);
	uint		eob		= get_eob(info);
	T4	o0;
	idct4(
		unpack_get4n(coeffs, eob, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iadst4(get4h(temp4x4, 4, 0, threadid), o0);
	output4(output, get_pos(info) + int2(0, threadid), dct_texels(o0, 4));
}

void iadst4h_iadst4v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint		scan	= get_scan4x4(default_iscan_4x4_trans, threadid);
	uint		eob		= get_eob(info);
	T4	o0;
	iadst4(
		unpack_get4n(coeffs, eob, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iadst4(get4h(temp4x4, 4, 0, threadid), o0);
	output4(output, get_pos(info) + int2(0, threadid), dct_texels(o0, 4));
}


#ifdef PLAT_PS4

COMPUTE_SHADER(4,1,1)
void all_iwht4(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iwht4(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(4,1,1)
void all_idct4h_idct4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct4h_idct4v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(4,1,1)
void all_idct4h_iadst4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct4h_iadst4v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(4,1,1)
void all_iadst4h_idct4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iadst4h_idct4v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(4,1,1)
void all_iadst4h_iadst4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iadst4h_iadst4v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

#else

COMPUTE_SHADER(4,1,1)
void all_iwht4(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iwht4(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(4,1,1)
void all_idct4h_idct4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct4h_idct4v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(4,1,1)
void all_idct4h_iadst4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct4h_iadst4v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(4,1,1)
void all_iadst4h_idct4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iadst4h_idct4v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(4,1,1)
void all_iadst4h_iadst4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iadst4h_iadst4v(threadid, info, buffers_coeffs, buffers_output);
}

#endif
//-----------------------------------------------------------------------------
//	8x8 shaders
//-----------------------------------------------------------------------------
thread_local	T	temp8x8[8 * 8];

static const uint2 default_iscan_8x8_trans[8] = {
	{PACK8b(   0,   1,   3,   7,  12,  18,  25,  33)},
	{PACK8b(   2,   4,   6,  11,  16,  23,  32,  40)},
	{PACK8b(   5,   8,  10,  15,  20,  28,  39,  46)},
	{PACK8b(   9,  13,  17,  21,  27,  35,  45,  51)},
	{PACK8b(  14,  19,  24,  29,  34,  41,  50,  54)},
	{PACK8b(  22,  26,  30,  36,  43,  48,  55,  58)},
	{PACK8b(  31,  38,  42,  47,  52,  56,  59,  61)},
	{PACK8b(  37,  44,  49,  53,  57,  60,  62,  63)},
};
static const uint2 row_iscan_8x8_trans[8] = {
	{PACK8b(   0,   3,   6,  11,  18,  26,  32,  40)},
	{PACK8b(   1,   4,   9,  14,  22,  29,  36,  45)},
	{PACK8b(   2,   7,  13,  17,  25,  33,  42,  48)},
	{PACK8b(   5,  10,  16,  23,  31,  38,  47,  53)},
	{PACK8b(   8,  15,  21,  28,  35,  43,  51,  56)},
	{PACK8b(  12,  20,  27,  34,  41,  49,  54,  58)},
	{PACK8b(  19,  30,  37,  44,  50,  55,  60,  62)},
	{PACK8b(  24,  39,  46,  52,  57,  59,  61,  63)},
};
static const uint2 col_iscan_8x8_trans[8] = {
	{PACK8b(   0,   1,   2,   4,   6,   9,  14,  19)},
	{PACK8b(   3,   5,   7,  10,  12,  17,  23,  29)},
	{PACK8b(   8,  11,  13,  16,  21,  25,  30,  36)},
	{PACK8b(  15,  18,  20,  24,  27,  33,  37,  42)},
	{PACK8b(  22,  26,  28,  31,  35,  39,  45,  49)},
	{PACK8b(  32,  34,  38,  41,  43,  48,  53,  57)},
	{PACK8b(  40,  44,  46,  50,  52,  55,  59,  61)},
	{PACK8b(  47,  51,  54,  56,  58,  60,  62,  63)},
};

void idct8h_idct8v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint2		scan	= default_iscan_8x8_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1;
	idct8(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		o0, o1
	);

	put4v(temp8x8, 8, threadid, 0, o0);
	put4v(temp8x8, 8, threadid, 1, o1);

	ThreadGroupMemoryBarrierSync();
	idct8(
		get4h(temp8x8, 8, 0, threadid),
		get4h(temp8x8, 8, 1, threadid),
		o0, o1
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 5));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 5));
}

void idct8h_iadst8v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint2		scan	= row_iscan_8x8_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1;
	iadst8(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		o0, o1
	);

	put4v(temp8x8, 8, threadid, 0, o0);
	put4v(temp8x8, 8, threadid, 1, o1);

	ThreadGroupMemoryBarrierSync();
	idct8(
		get4h(temp8x8, 8, 0, threadid),
		get4h(temp8x8, 8, 1, threadid),
		o0, o1
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 5));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 5));
}

void iadst8h_idct8v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint2		scan	= col_iscan_8x8_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1;
	idct8(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		o0, o1
	);

	put4v(temp8x8, 8, threadid, 0, o0);
	put4v(temp8x8, 8, threadid, 1, o1);

	ThreadGroupMemoryBarrierSync();
	iadst8(
		get4h(temp8x8, 8, 0, threadid),
		get4h(temp8x8, 8, 1, threadid),
		o0, o1
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 5));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 5));
}

void iadst8h_iadst8v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint2		scan	= default_iscan_8x8_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1;
	iadst8(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		o0, o1
	);

	put4v(temp8x8, 8, threadid, 0, o0);
	put4v(temp8x8, 8, threadid, 1, o1);

	ThreadGroupMemoryBarrierSync();
	iadst8(
		get4h(temp8x8, 8, 0, threadid),
		get4h(temp8x8, 8, 1, threadid),
		o0, o1
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 5));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 5));
}

#ifdef PLAT_PS4

COMPUTE_SHADER(8,1,1)
void all_idct8h_idct8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct8h_idct8v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(8,1,1)
void all_idct8h_iadst8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct8h_iadst8v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(8,1,1)
void all_iadst8h_idct8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iadst8h_idct8v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(8,1,1)
void all_iadst8h_iadst8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iadst8h_iadst8v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

#else

COMPUTE_SHADER(8,1,1)
void all_idct8h_idct8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct8h_idct8v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(8,1,1)
void all_idct8h_iadst8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct8h_iadst8v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(8,1,1)
void all_iadst8h_idct8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iadst8h_idct8v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(8,1,1)
void all_iadst8h_iadst8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iadst8h_iadst8v(threadid, info, buffers_coeffs, buffers_output);
}

#endif

//-----------------------------------------------------------------------------
//	16x16 shaders
//-----------------------------------------------------------------------------

thread_local	T	temp16x16[16 * 16];

static const uint4 default_iscan_16x16_trans[16] = {
	{PACK16b(   0,   1,   3,   6,  10,  15,  22,  29,  38,  50,  62,  75,  89, 103, 121, 137)},
	{PACK16b(   2,   4,   7,  12,  14,  21,  27,  35,  45,  56,  70,  82, 100, 115, 135, 152)},
	{PACK16b(   5,   8,  11,  16,  19,  26,  32,  42,  51,  63,  76,  90, 111, 126, 144, 160)},
	{PACK16b(   9,  13,  18,  23,  28,  34,  41,  49,  61,  74,  87, 102, 123, 136, 158, 174)},
	{PACK16b(  17,  20,  25,  31,  37,  43,  48,  59,  68,  83,  97, 112, 132, 149, 170, 184)},
	{PACK16b(  24,  30,  33,  39,  47,  52,  60,  69,  80,  94, 107, 124, 142, 162, 181, 195)},
	{PACK16b(  36,  40,  46,  53,  58,  65,  73,  81,  93, 109, 122, 138, 156, 171, 192, 206)},
	{PACK16b(  44,  54,  57,  64,  67,  77,  85,  95, 105, 117, 131, 146, 167, 183, 200, 212)},
	{PACK16b(  55,  66,  71,  78,  84,  91,  99, 108, 118, 129, 145, 157, 180, 194, 209, 220)},
	{PACK16b(  72,  79,  86,  92,  98, 106, 116, 125, 134, 147, 159, 173, 189, 204, 218, 226)},
	{PACK16b(  88,  96, 101, 110, 114, 120, 130, 139, 150, 163, 172, 187, 203, 215, 227, 232)},
	{PACK16b( 104, 113, 119, 127, 133, 140, 151, 155, 168, 177, 188, 202, 216, 224, 233, 239)},
	{PACK16b( 128, 141, 148, 153, 161, 165, 175, 182, 191, 199, 210, 219, 231, 236, 243, 247)},
	{PACK16b( 143, 154, 164, 169, 176, 185, 190, 197, 207, 213, 222, 230, 237, 241, 244, 249)},
	{PACK16b( 166, 178, 186, 193, 198, 205, 211, 217, 223, 228, 235, 240, 246, 248, 251, 253)},
	{PACK16b( 179, 196, 201, 208, 214, 221, 225, 229, 234, 238, 242, 245, 250, 252, 254, 255)},
};
static const uint4 row_iscan_16x16_trans[16] = {
	{PACK16b(   0,   3,   8,  14,  21,  28,  41,  52,  66,  80,  95, 111, 126, 141, 152, 158)},
	{PACK16b(   1,   5,  10,  16,  24,  34,  46,  57,  71,  89, 104, 118, 132, 149, 163, 173)},
	{PACK16b(   2,   7,  13,  20,  30,  39,  49,  62,  78,  92, 109, 124, 137, 156, 171, 187)},
	{PACK16b(   4,  11,  18,  26,  35,  45,  56,  69,  82, 101, 117, 129, 145, 166, 183, 194)},
	{PACK16b(   6,  15,  23,  31,  40,  50,  63,  75,  91, 105, 123, 140, 153, 172, 186, 198)},
	{PACK16b(   9,  19,  27,  37,  47,  58,  70,  83,  97, 114, 128, 147, 160, 180, 193, 209)},
	{PACK16b(  12,  25,  33,  44,  55,  67,  79,  93, 108, 125, 143, 157, 174, 189, 201, 213)},
	{PACK16b(  17,  32,  42,  53,  65,  77,  90, 102, 113, 131, 144, 164, 178, 199, 211, 217)},
	{PACK16b(  22,  38,  51,  61,  74,  87,  98, 110, 127, 139, 155, 170, 184, 200, 214, 225)},
	{PACK16b(  29,  48,  60,  73,  81,  96, 107, 120, 136, 151, 165, 181, 197, 210, 218, 229)},
	{PACK16b(  36,  59,  72,  85,  94, 106, 122, 134, 148, 162, 175, 191, 204, 220, 227, 235)},
	{PACK16b(  43,  68,  88, 100, 112, 121, 138, 150, 168, 177, 190, 203, 216, 228, 236, 241)},
	{PACK16b(  54,  84, 103, 116, 133, 146, 159, 176, 188, 192, 206, 224, 231, 238, 245, 248)},
	{PACK16b(  64,  99, 119, 135, 154, 169, 182, 195, 202, 208, 219, 230, 237, 242, 247, 250)},
	{PACK16b(  76, 115, 142, 161, 179, 196, 207, 215, 221, 223, 233, 240, 244, 249, 252, 254)},
	{PACK16b(  86, 130, 167, 185, 205, 212, 222, 226, 232, 234, 239, 243, 246, 251, 253, 255)},
};
static const uint4 col_iscan_16x16_trans[16] = {
	{PACK16b(   0,   1,   2,   3,   5,   7,   9,  13,  17,  22,  27,  33,  42,  50,  57,  65)},
	{PACK16b(   4,   6,   8,  10,  12,  15,  19,  24,  30,  36,  44,  51,  61,  72,  80,  88)},
	{PACK16b(  11,  14,  16,  18,  21,  26,  28,  35,  40,  48,  56,  68,  77,  87,  97, 107)},
	{PACK16b(  20,  23,  25,  29,  32,  37,  39,  46,  53,  62,  70,  79,  90, 100, 111, 124)},
	{PACK16b(  31,  34,  38,  41,  45,  49,  54,  60,  66,  76,  84,  94, 106, 118, 131, 139)},
	{PACK16b(  43,  47,  52,  55,  58,  63,  69,  73,  82,  92,  99, 110, 121, 128, 143, 152)},
	{PACK16b(  59,  64,  67,  71,  74,  78,  86,  91,  98, 105, 113, 125, 134, 145, 155, 163)},
	{PACK16b(  75,  81,  83,  89,  93,  96, 102, 108, 115, 120, 127, 138, 148, 158, 169, 177)},
	{PACK16b(  85,  95, 101, 103, 104, 112, 117, 122, 126, 133, 140, 149, 160, 168, 178, 185)},
	{PACK16b( 109, 114, 116, 119, 123, 129, 132, 137, 142, 147, 156, 162, 173, 183, 192, 199)},
	{PACK16b( 130, 135, 136, 141, 144, 146, 151, 154, 161, 167, 175, 184, 191, 204, 214, 221)},
	{PACK16b( 150, 153, 157, 159, 164, 166, 170, 174, 180, 186, 193, 202, 211, 222, 231, 234)},
	{PACK16b( 165, 171, 172, 176, 179, 182, 187, 189, 197, 203, 209, 217, 225, 233, 239, 243)},
	{PACK16b( 181, 188, 190, 194, 196, 200, 206, 207, 213, 219, 226, 229, 238, 242, 246, 248)},
	{PACK16b( 195, 201, 205, 208, 210, 215, 220, 224, 227, 232, 236, 241, 245, 249, 250, 252)},
	{PACK16b( 198, 212, 216, 218, 223, 228, 230, 235, 237, 240, 244, 247, 251, 253, 254, 255)},
};

void idct16h_idct16v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint4		scan	= default_iscan_16x16_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1, o2, o3;
	idct16(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.w) * get_quant_ac(info),
		o0, o1, o2, o3
	);

	put4v(temp16x16, 16, threadid, 0, o0);
	put4v(temp16x16, 16, threadid, 1, o1);
	put4v(temp16x16, 16, threadid, 2, o2);
	put4v(temp16x16, 16, threadid, 3, o3);

	ThreadGroupMemoryBarrierSync();

	idct16(
		get4h(temp16x16, 16, 0, threadid),
		get4h(temp16x16, 16, 1, threadid),
		get4h(temp16x16, 16, 2, threadid),
		get4h(temp16x16, 16, 3, threadid),
		o0, o1, o2, o3
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 6));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 6));
	output4(output, pos + int2(2, threadid), dct_texels(o2, 6));
	output4(output, pos + int2(3, threadid), dct_texels(o3, 6));
}

void idct16h_iadst16v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint4		scan	= row_iscan_16x16_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1, o2, o3;
	iadst16(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.w) * get_quant_ac(info),
		o0, o1, o2, o3
	);

	put4v(temp16x16, 16, threadid, 0, o0);
	put4v(temp16x16, 16, threadid, 1, o1);
	put4v(temp16x16, 16, threadid, 2, o2);
	put4v(temp16x16, 16, threadid, 3, o3);

	ThreadGroupMemoryBarrierSync();

	idct16(
		get4h(temp16x16, 16, 0, threadid),
		get4h(temp16x16, 16, 1, threadid),
		get4h(temp16x16, 16, 2, threadid),
		get4h(temp16x16, 16, 3, threadid),
		o0, o1, o2, o3
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 6));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 6));
	output4(output, pos + int2(2, threadid), dct_texels(o2, 6));
	output4(output, pos + int2(3, threadid), dct_texels(o3, 6));
}

void iadst16h_idct16v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint4		scan	= col_iscan_16x16_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1, o2, o3;
	idct16(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.w) * get_quant_ac(info),
		o0, o1, o2, o3
	);

	put4v(temp16x16, 16, threadid, 0, o0);
	put4v(temp16x16, 16, threadid, 1, o1);
	put4v(temp16x16, 16, threadid, 2, o2);
	put4v(temp16x16, 16, threadid, 3, o3);

	ThreadGroupMemoryBarrierSync();

	iadst16(
		get4h(temp16x16, 16, 0, threadid),
		get4h(temp16x16, 16, 1, threadid),
		get4h(temp16x16, 16, 2, threadid),
		get4h(temp16x16, 16, 3, threadid),
		o0, o1, o2, o3
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 6));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 6));
	output4(output, pos + int2(2, threadid), dct_texels(o2, 6));
	output4(output, pos + int2(3, threadid), dct_texels(o3, 6));
}

void iadst16h_iadst16v(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint4		scan	= default_iscan_16x16_trans[threadid];
	uint		eob		= get_eob(info);

	T4	o0, o1, o2, o3;
	iadst16(
		unpack_get4(coeffs, eob, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, eob, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, eob, scan.w) * get_quant_ac(info),
		o0, o1, o2, o3
	);

	put4v(temp16x16, 16, threadid, 0, o0);
	put4v(temp16x16, 16, threadid, 1, o1);
	put4v(temp16x16, 16, threadid, 2, o2);
	put4v(temp16x16, 16, threadid, 3, o3);

	ThreadGroupMemoryBarrierSync();

	iadst16(
		get4h(temp16x16, 16, 0, threadid),
		get4h(temp16x16, 16, 1, threadid),
		get4h(temp16x16, 16, 2, threadid),
		get4h(temp16x16, 16, 3, threadid),
		o0, o1, o2, o3
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 6));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 6));
	output4(output, pos + int2(2, threadid), dct_texels(o2, 6));
	output4(output, pos + int2(3, threadid), dct_texels(o3, 6));
}

#ifdef PLAT_PS4

COMPUTE_SHADER(16,1,1)
void all_idct16h_idct16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct16h_idct16v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(16,1,1)
void all_idct16h_iadst16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct16h_iadst16v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(16,1,1)
void all_iadst16h_idct16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iadst16h_idct16v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

COMPUTE_SHADER(16,1,1)
void all_iadst16h_iadst16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	iadst16h_iadst16v(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

#else

COMPUTE_SHADER(16,1,1)
void all_idct16h_idct16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct16h_idct16v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(16,1,1)
void all_idct16h_iadst16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct16h_iadst16v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(16,1,1)
void all_iadst16h_idct16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iadst16h_idct16v(threadid, info, buffers_coeffs, buffers_output);
}

COMPUTE_SHADER(16,1,1)
void all_iadst16h_iadst16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	iadst16h_iadst16v(threadid, info, buffers_coeffs, buffers_output);
}

#endif
//-----------------------------------------------------------------------------
//	32x32 shaders
//-----------------------------------------------------------------------------

thread_local	T	temp32x32[32 * 32];

static const uint4 default_iscan_32x32_trans[4 * 32] = {
	PACK16w(   0,   1,   3,   6,   9,  13,  21,  24,  34,  41,  51,  61,  70,  84,  98, 117), PACK16w( 135, 153, 176, 192, 240, 246, 258, 276, 336, 343, 357, 378, 448, 456, 472, 496),
	PACK16w(   2,   4,   7,  11,  14,  20,  27,  32,  40,  49,  59,  69,  79,  93, 106, 128), PACK16w( 146, 166, 187, 199, 247, 259, 277, 300, 344, 358, 379, 406, 457, 473, 497, 528),
	PACK16w(   5,   8,  12,  16,  19,  26,  33,  39,  46,  55,  66,  75,  86, 103, 115, 136), PACK16w( 152, 174, 195, 206, 260, 278, 301, 318, 359, 380, 407, 427, 474, 498, 529, 552),
	PACK16w(  10,  15,  18,  23,  29,  35,  42,  48,  56,  67,  76,  87,  97, 110, 127, 148), PACK16w( 165, 183, 202, 213, 279, 302, 319, 330, 381, 408, 428, 441, 499, 530, 553, 568),
	PACK16w(  17,  22,  28,  31,  37,  44,  53,  57,  68,  77,  89, 100, 108, 125, 143, 160), PACK16w( 241, 248, 261, 280, 337, 345, 360, 382, 449, 458, 475, 500, 576, 583, 597, 618),
	PACK16w(  25,  30,  36,  43,  50,  54,  63,  71,  81,  91,  99, 114, 122, 141, 156, 175), PACK16w( 249, 262, 281, 303, 346, 361, 383, 409, 459, 476, 501, 531, 584, 598, 619, 646),
	PACK16w(  38,  45,  52,  60,  65,  72,  80,  88,  96, 107, 119, 129, 137, 154, 169, 188), PACK16w( 263, 282, 304, 320, 362, 384, 410, 429, 477, 502, 532, 554, 599, 620, 647, 667),
	PACK16w(  47,  58,  64,  73,  78,  85,  94, 104, 111, 124, 131, 144, 155, 171, 185, 198), PACK16w( 283, 305, 321, 331, 385, 411, 430, 442, 503, 533, 555, 569, 621, 648, 668, 681),
	PACK16w(  62,  74,  82,  90,  95, 105, 113, 120, 130, 138, 149, 162, 242, 250, 264, 284), PACK16w( 338, 347, 363, 386, 450, 460, 478, 504, 577, 585, 600, 622, 688, 694, 706, 724),
	PACK16w(  83,  92, 102, 109, 116, 123, 132, 139, 147, 161, 168, 180, 251, 265, 285, 306), PACK16w( 348, 364, 387, 412, 461, 479, 505, 534, 586, 601, 623, 649, 695, 707, 725, 748),
	PACK16w( 101, 112, 118, 126, 134, 140, 151, 159, 167, 177, 181, 191, 266, 286, 307, 322), PACK16w( 365, 388, 413, 431, 480, 506, 535, 556, 602, 624, 650, 669, 708, 726, 749, 766),
	PACK16w( 121, 133, 142, 150, 157, 163, 172, 178, 186, 194, 200, 207, 287, 308, 323, 332), PACK16w( 389, 414, 432, 443, 507, 536, 557, 570, 625, 651, 670, 682, 727, 750, 767, 778),
	PACK16w( 145, 158, 164, 173, 179, 182, 190, 197, 243, 252, 267, 288, 339, 349, 366, 390), PACK16w( 451, 462, 481, 508, 578, 587, 603, 626, 689, 696, 709, 728, 784, 789, 799, 814),
	PACK16w( 170, 184, 189, 196, 201, 205, 209, 212, 253, 268, 289, 309, 350, 367, 391, 415), PACK16w( 463, 482, 509, 537, 588, 604, 627, 652, 697, 710, 729, 751, 790, 800, 815, 834),
	PACK16w( 193, 203, 208, 211, 214, 216, 218, 221, 269, 290, 310, 324, 368, 392, 416, 433), PACK16w( 483, 510, 538, 558, 605, 628, 653, 671, 711, 730, 752, 768, 801, 816, 835, 849),
	PACK16w( 204, 215, 217, 220, 223, 225, 227, 230, 291, 311, 325, 333, 393, 417, 434, 444), PACK16w( 511, 539, 559, 571, 629, 654, 672, 683, 731, 753, 769, 779, 817, 836, 850, 859),
	PACK16w( 210, 222, 224, 226, 244, 254, 270, 292, 340, 351, 369, 394, 452, 464, 484, 512), PACK16w( 579, 589, 606, 630, 690, 698, 712, 732, 785, 791, 802, 818, 864, 868, 876, 888),
	PACK16w( 219, 228, 231, 232, 255, 271, 293, 312, 352, 370, 395, 418, 465, 485, 513, 540), PACK16w( 590, 607, 631, 655, 699, 713, 733, 754, 792, 803, 819, 837, 869, 877, 889, 904),
	PACK16w( 229, 234, 235, 236, 272, 294, 313, 326, 371, 396, 419, 435, 486, 514, 541, 560), PACK16w( 608, 632, 656, 673, 714, 734, 755, 770, 804, 820, 838, 851, 878, 890, 905, 916),
	PACK16w( 233, 237, 238, 239, 295, 314, 327, 334, 397, 420, 436, 445, 515, 542, 561, 572), PACK16w( 633, 657, 674, 684, 735, 756, 771, 780, 821, 839, 852, 860, 891, 906, 917, 924),
	PACK16w( 245, 256, 273, 296, 341, 353, 372, 398, 453, 466, 487, 516, 580, 591, 609, 634), PACK16w( 691, 700, 715, 736, 786, 793, 805, 822, 865, 870, 879, 892, 928, 931, 937, 946),
	PACK16w( 257, 274, 297, 315, 354, 373, 399, 421, 467, 488, 517, 543, 592, 610, 635, 658), PACK16w( 701, 716, 737, 757, 794, 806, 823, 840, 871, 880, 893, 907, 932, 938, 947, 958),
	PACK16w( 275, 298, 316, 328, 374, 400, 422, 437, 489, 518, 544, 562, 611, 636, 659, 675), PACK16w( 717, 738, 758, 772, 807, 824, 841, 853, 881, 894, 908, 918, 939, 948, 959, 967),
	PACK16w( 299, 317, 329, 335, 401, 423, 438, 446, 519, 545, 563, 573, 637, 660, 676, 685), PACK16w( 739, 759, 773, 781, 825, 842, 854, 861, 895, 909, 919, 925, 949, 960, 968, 973),
	PACK16w( 342, 355, 375, 402, 454, 468, 490, 520, 581, 593, 612, 638, 692, 702, 718, 740), PACK16w( 787, 795, 808, 826, 866, 872, 882, 896, 929, 933, 940, 950, 976, 978, 982, 988),
	PACK16w( 356, 376, 403, 424, 469, 491, 521, 546, 594, 613, 639, 661, 703, 719, 741, 760), PACK16w( 796, 809, 827, 843, 873, 883, 897, 910, 934, 941, 951, 961, 979, 983, 989, 996),
	PACK16w( 377, 404, 425, 439, 492, 522, 547, 564, 614, 640, 662, 677, 720, 742, 761, 774), PACK16w( 810, 828, 844, 855, 884, 898, 911, 920, 942, 952, 962, 969, 984, 990, 997,1002),
	PACK16w( 405, 426, 440, 447, 523, 548, 565, 574, 641, 663, 678, 686, 743, 762, 775, 782), PACK16w( 829, 845, 856, 862, 899, 912, 921, 926, 953, 963, 970, 974, 991, 998,1003,1006),
	PACK16w( 455, 470, 493, 524, 582, 595, 615, 642, 693, 704, 721, 744, 788, 797, 811, 830), PACK16w( 867, 874, 885, 900, 930, 935, 943, 954, 977, 980, 985, 992,1008,1009,1011,1014),
	PACK16w( 471, 494, 525, 549, 596, 616, 643, 664, 705, 722, 745, 763, 798, 812, 831, 846), PACK16w( 875, 886, 901, 913, 936, 944, 955, 964, 981, 986, 993, 999,1010,1012,1015,1018),
	PACK16w( 495, 526, 550, 566, 617, 644, 665, 679, 723, 746, 764, 776, 813, 832, 847, 857), PACK16w( 887, 902, 914, 922, 945, 956, 965, 971, 987, 994,1000,1004,1013,1016,1019,1021),
	PACK16w( 527, 551, 567, 575, 645, 666, 680, 687, 747, 765, 777, 783, 833, 848, 858, 863), PACK16w( 903, 915, 923, 927, 957, 966, 972, 975, 995,1001,1005,1007,1017,1020,1022,1023),
};

#ifdef DCT_FLOAT
float4 quant32_fudge(float4 i) { return  i; }
#else
int4 quant32_fudge(int4 i) { return  i / 2; }
#endif

void idct32(uint threadid, DCTinfo info, CoeffBuffer coeffs, OutputTex output) {
	uint4		scan0	= default_iscan_32x32_trans[threadid * 4 + 0];
	uint4		scan1	= default_iscan_32x32_trans[threadid * 4 + 1];
	uint4		scan2	= default_iscan_32x32_trans[threadid * 4 + 2];
	uint4		scan3	= default_iscan_32x32_trans[threadid * 4 + 3];
	uint		eob		= get_eob(info);

	T4	o0, o1, o2, o3, o4, o5, o6, o7;
	idct32(
		quant32_fudge(unpack_get4(coeffs, eob, scan0.xy) * get_quant(info, threadid)),
		quant32_fudge(unpack_get4(coeffs, eob, scan0.zw) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, eob, scan1.xy) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, eob, scan1.zw) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, eob, scan2.xy) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, eob, scan2.zw) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, eob, scan3.xy) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, eob, scan3.zw) * get_quant_ac(info)),
		o0, o1, o2, o3, o4, o5, o6, o7
	);

	put4v(temp32x32, 32, threadid, 0, o0);
	put4v(temp32x32, 32, threadid, 1, o1);
	put4v(temp32x32, 32, threadid, 2, o2);
	put4v(temp32x32, 32, threadid, 3, o3);
	put4v(temp32x32, 32, threadid, 4, o4);
	put4v(temp32x32, 32, threadid, 5, o5);
	put4v(temp32x32, 32, threadid, 6, o6);
	put4v(temp32x32, 32, threadid, 7, o7);

	ThreadGroupMemoryBarrierSync();
	
	idct32(
		get4h(temp32x32, 32, 0, threadid),
		get4h(temp32x32, 32, 1, threadid),
		get4h(temp32x32, 32, 2, threadid),
		get4h(temp32x32, 32, 3, threadid),
		get4h(temp32x32, 32, 4, threadid),
		get4h(temp32x32, 32, 5, threadid),
		get4h(temp32x32, 32, 6, threadid),
		get4h(temp32x32, 32, 7, threadid),
		o0, o1, o2, o3, o4, o5, o6, o7
	);
	int2	pos = get_pos(info);
	output4(output, pos + int2(0, threadid), dct_texels(o0, 6));
	output4(output, pos + int2(1, threadid), dct_texels(o1, 6));
	output4(output, pos + int2(2, threadid), dct_texels(o2, 6));
	output4(output, pos + int2(3, threadid), dct_texels(o3, 6));
	output4(output, pos + int2(4, threadid), dct_texels(o4, 6));
	output4(output, pos + int2(5, threadid), dct_texels(o5, 6));
	output4(output, pos + int2(6, threadid), dct_texels(o6, 6));
	output4(output, pos + int2(7, threadid), dct_texels(o7, 6));
}

#ifdef PLAT_PS4

COMPUTE_SHADER(32,1,1)
void all_idct32(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffers buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	idct32(threadid, info, get_coeff_buffer(buffers.coeffs, info), buffers.output);
}

#else

COMPUTE_SHADER(32,1,1)
void all_idct32(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	DCTinfo		info	= buffers_info[groupid];
	idct32(threadid, info, buffers_coeffs, buffers_output);
}

#endif

//-----------------------------------------------------------------------------

technique transforms {
	pass p0		{ SET_CS(all_iwht4); }
	pass p1		{ SET_CS(all_idct4h_idct4v); }
	pass p2		{ SET_CS(all_idct4h_iadst4v); }
	pass p3		{ SET_CS(all_iadst4h_idct4v); }
	pass p4		{ SET_CS(all_iadst4h_iadst4v); }
	pass p5		{ SET_CS(all_idct8h_idct8v); }
	pass p6		{ SET_CS(all_idct8h_iadst8v); }
	pass p7		{ SET_CS(all_iadst8h_idct8v); }
	pass p8		{ SET_CS(all_iadst8h_iadst8v); }
	pass p9		{ SET_CS(all_idct16h_idct16v); }
	pass p10	{ SET_CS(all_idct16h_iadst16v); }
	pass p11	{ SET_CS(all_iadst16h_idct16v); }
	pass p12	{ SET_CS(all_iadst16h_iadst16v); }
	pass p13	{ SET_CS(all_idct32); }
};
