#include "common.fxh"

//#define DCT_FLOAT

//-----------------------------------------------------------------------------
//	helpers
//-----------------------------------------------------------------------------
//typedef uint2	nibble16;
typedef ulong	nibble16;

#define PACK4n(a,b,c,d)								((a) & 0xf) | (((b) & 0xf)<<4) | (((c) & 0xf)<<8) | (((d) & 0xf)<<12)
#define PACK8n(a,b,c,d,e,f,g,h)						PACK4n(a,b,c,d) | ((PACK4n(e,f,g,h))<<16)
#define PACK16n(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	uint(PACK8n(a,b,c,d,e,f,g,h)) | (ulong(PACK8n(i,j,k,l,m,n,o,p))<<32)
//#define PACK16n(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	uint2(PACK8n(a,b,c,d,e,f,g,h), PACK8n(i,j,k,l,m,n,o,p))

#define PACK4b(a,b,c,d)								((a) & 0xff) | (((b) & 0xff)<<8) | (((c) & 0xff)<<16) | (((d) & 0xff)<<24)
#define PACK8b(a,b,c,d,e,f,g,h)						PACK4b(a,b,c,d), PACK4b(e,f,g,h)
#define PACK16b(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	PACK8b(a,b,c,d,e,f,g,h), PACK8b(i,j,k,l,m,n,o,p)

#define PACK4bo(O, a,b,c,d)							PACK4b(a + O, b + O, c + O, d + O)
#define PACK8bo(O, a,b,c,d,e,f,g,h)					PACK4bo(O, a,b,c,d), PACK4bo(O, e,f,g,h)
#define PACK16bo(O, a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) PACK8bo(O, a,b,c,d,e,f,g,h), PACK8bo(O, i,j,k,l,m,n,o,p)

#define PACK2w(a,b)									a|(b<<16)
#define PACK4w(a,b,c,d)								PACK2w(a, b), PACK2w(c, d)
#define PACK8w(a,b,c,d,e,f,g,h)						{PACK4w(a,b,c,d), PACK4w(e,f,g,h)}
#define PACK16w(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	PACK8w(a,b,c,d,e,f,g,h), PACK8w(i,j,k,l,m,n,o,p)

int2 unpack2i(uint x) {
	return int2(x << 16, x) >> 16;
}
int4 unpack4i(uint v) {
	return int4(v << 24, v << 16, v << 8, v) >> 24;
}
uint2 unpack2u(uint x) {
	return uint2(x << 16, x) >> 16;
}
uint4 unpack4u(uint v) {
	return uint4(v << 24, v << 16, v << 8, v) >> 24;
}

int4 unpack_get4n(Buffer<int> input, uint start, uint x) {
	return int4(
		input[start + ((x >>  0) & 15)],
		input[start + ((x >>  4) & 15)],
		input[start + ((x >>  8) & 15)],
		input[start + ((x >> 12) & 15)]
	);
}

int4 unpack_get4(Buffer<int> input, uint start, uint x) {
	return int4(
		input[start + ((x >>  0) & 255)],
		input[start + ((x >>  8) & 255)],
		input[start + ((x >> 16) & 255)],
		input[start + ((x >> 24) & 255)]
	);
}

int4 unpack_get4(Buffer<int> input, uint start, uint2 x) {
	return int4(
		input[start + (x.x & 0xffff)],
		input[start + (x.x >> 16)],
		input[start + (x.y & 0xffff)],
		input[start + (x.y >> 16)]
	);
}

float4 unpack_get4n(Buffer<float> input, uint start, uint x) {
	return float4(
		input[start + ((x >>  0) & 15)],
		input[start + ((x >>  4) & 15)],
		input[start + ((x >>  8) & 15)],
		input[start + ((x >> 12) & 15)]
	);
}

float4 unpack_get4(Buffer<float> input, uint start, uint x) {
	return float4(
		input[start + ((x >>  0) & 255)],
		input[start + ((x >>  8) & 255)],
		input[start + ((x >> 16) & 255)],
		input[start + ((x >> 24) & 255)]
	);
}

float4 unpack_get4(Buffer<float> input, uint start, uint2 x) {
	return float4(
		input[start + (x.x & 0xffff)],
		input[start + (x.x >> 16)],
		input[start + (x.y & 0xffff)],
		input[start + (x.y >> 16)]
	);
}

void add_clamp_pixel(RWTexture2D<int> dest, int2 uv, uint x) {
	dest[uv] = clamp(int(dest[uv] + x) - 128, 0, 255);
}

//-----------------------------------------------------------------------------
//	DCT consts
//-----------------------------------------------------------------------------

#ifdef DCT_FLOAT

typedef float	T;
typedef float4	T4;
#define DCT_FIXED(x)	x
float	dct_round(float x)			{ return x; }
float4	dct_round(float4 x)			{ return x; }
float4	dct_texels(float4 o, int n)	{ return o + 0.5;}
#else
typedef int		T;
typedef int4	T4;
#define DCT_CONST_BITS	14
#define DCT_FIXED(x)	int((x) * (1<<DCT_CONST_BITS) + 0.5)
int		dct_round(int x)			{ return round_pow2(x, DCT_CONST_BITS); }
int4	dct_round(int4 x)			{ return round_pow2(x, DCT_CONST_BITS); }
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

static const T cospi_1_64		= DCT_FIXED(0.9987954139);
static const T cospi_2_64		= DCT_FIXED(0.9951847076);
static const T cospi_3_64		= DCT_FIXED(0.9891765594);
static const T cospi_4_64		= DCT_FIXED(0.9807852745);
static const T cospi_5_64		= DCT_FIXED(0.9700312614);
static const T cospi_6_64		= DCT_FIXED(0.9569403648);
static const T cospi_7_64		= DCT_FIXED(0.9415440559);
static const T cospi_8_64		= DCT_FIXED(0.9238795280);
static const T cospi_9_64		= DCT_FIXED(0.9039893150);
static const T cospi_10_64		= DCT_FIXED(0.8819211959);
static const T cospi_11_64		= DCT_FIXED(0.8577285766);
static const T cospi_12_64		= DCT_FIXED(0.8314696311);
static const T cospi_13_64		= DCT_FIXED(0.8032074928);
static const T cospi_14_64		= DCT_FIXED(0.7730104446);
static const T cospi_15_64		= DCT_FIXED(0.7409511089);
static const T cospi_16_64		= DCT_FIXED(0.7071067810);
static const T cospi_17_64		= DCT_FIXED(0.6715589046);
static const T cospi_18_64		= DCT_FIXED(0.6343932628);
static const T cospi_19_64		= DCT_FIXED(0.5956993103);
static const T cospi_20_64		= DCT_FIXED(0.5555701732);
static const T cospi_21_64		= DCT_FIXED(0.5141026973);
static const T cospi_22_64		= DCT_FIXED(0.4713966369);
static const T cospi_23_64		= DCT_FIXED(0.4275551319);
static const T cospi_24_64		= DCT_FIXED(0.3826834201);
static const T cospi_25_64		= DCT_FIXED(0.3368898391);
static const T cospi_26_64		= DCT_FIXED(0.2902846336);
static const T cospi_27_64		= DCT_FIXED(0.2429801225);
static const T cospi_28_64		= DCT_FIXED(0.1950902366);
static const T cospi_29_64		= DCT_FIXED(0.1467304944);
static const T cospi_30_64		= DCT_FIXED(0.0980171298);
static const T cospi_31_64		= DCT_FIXED(0.0490676498);
static const T sinpi_1_9_sqrt2	= DCT_FIXED(0.3224596977);
static const T sinpi_2_9_sqrt2	= DCT_FIXED(0.6060259819);
static const T sinpi_3_9_sqrt2	= DCT_FIXED(0.8164966583);
static const T sinpi_4_9_sqrt2	= DCT_FIXED(0.9284856796);

//-----------------------------------------------------------------------------
// (lossless) Walsh–Hadamard transform
//-----------------------------------------------------------------------------

void iwht4(T4 i0, out T4 o0) {
	T4	s0 = i0 / 4;

	s0.x += s0.z;
	s0.w -= s0.y;
	T e = (s0.x - s0.w) / 2;
	s0.y = e - s0.y;
	s0.z = e - s0.z;

	o0 = T4(
		s0.x - s0.y,
		s0.y,
		s0.z,
		s0.w + s0.z
	);
}

//-----------------------------------------------------------------------------
//	IDCT
//-----------------------------------------------------------------------------

void idct4(T4 i0, out T4 o0) {
	// stage 1
	T4	s = dct_round(T4(
		(i0.x + i0.z) * cospi_16_64,
		(i0.x - i0.z) * cospi_16_64,
		i0.y * cospi_24_64 - i0.w * cospi_8_64,
		i0.y * cospi_8_64  + i0.w * cospi_24_64
	));

	// stage 2
	o0 = T4(
		s.x + s.w,
		s.y + s.z,
		s.y - s.z,
		s.x - s.w
	);
}

void idct8(T4 i0, T4 i1, out T4 o0, out T4 o1) {
	T4	a, b;

	idct4(T4(i0.xz, i1.xz), a);

	// stage 1
	b = dct_round(T4(
		i0.y * cospi_28_64 - i1.w * cospi_4_64,
		i1.y * cospi_12_64 - i0.w * cospi_20_64,
		i1.y * cospi_20_64 + i0.w * cospi_12_64,
		i0.y * cospi_4_64  + i1.w * cospi_28_64
	));

	// stage 2
	b = T4(
		b.y + b.x,
		b.x - b.y,
		b.w - b.z,
		b.z + b.w
	);

	// stage 3
	b = T4(
		b.x,
		dct_round((b.z - b.y) * cospi_16_64),
		dct_round((b.y + b.z) * cospi_16_64),
		b.w
	);

	// stage 4
	o0 = a.xyzw + b.wzyx;
	o1 = a.wzyx - b.xyzw;
}

void idct16(T4 i0, T4 i1, T4 i2, T4 i3, out T4 o0, out T4 o1, out T4 o2, out T4 o3) {
	T4	step1a, step1b, step2a, step2b;

	T4	t0, t1;
	idct8(T4(i0.xz, i1.xz), T4(i2.xz, i3.xz), t0, t1);

	// stage 1
	step1a	= T4(i0.y, i2.y, i1.y, i3.y);
	step1b	= T4(i0.w, i2.w, i1.w, i3.w);

	// stage 2
	step2a	= dct_round(T4(
		step1a.x * cospi_30_64 - step1b.w * cospi_2_64,		// 8
		step1a.y * cospi_14_64 - step1b.z * cospi_18_64,	// 9
		step1a.z * cospi_22_64 - step1b.y * cospi_10_64,	//10
		step1a.w * cospi_6_64  - step1b.x * cospi_26_64		//11
	));
	step2b	= dct_round(T4(
		step1a.w * cospi_26_64 + step1b.x * cospi_6_64,		//12
		step1a.z * cospi_10_64 + step1b.y * cospi_22_64,	//13
		step1a.y * cospi_18_64 + step1b.z * cospi_14_64,	//14
		step1a.x * cospi_2_64  + step1b.w * cospi_30_64		//15
	));

	// stage 3
	step1a	= T4(
		step2a.x + step2a.y,
		step2a.x - step2a.y,
		step2a.w - step2a.z,
		step2a.z + step2a.w
	);
	step1b	= T4(
		step2b.x + step2b.y,
		step2b.x - step2b.y,
		step2b.w - step2b.z,
		step2b.z + step2b.w
	);

	// stage 4
	step2a	= T4(
		step1a.x,
		dct_round( step1b.z * cospi_24_64 - step1a.y * cospi_8_64),
		dct_round(-step1a.z * cospi_24_64 - step1b.y * cospi_8_64),
		step1a.w
	);
	step2b	= T4(
		step1b.x,
		dct_round( step1b.y * cospi_24_64 - step1a.z * cospi_8_64),
		dct_round( step1a.y * cospi_24_64 + step1b.z * cospi_8_64),
		step1b.w
	);

	// stage 5
	step1a	= T4(
		step2a.x + step2a.w,
		step2a.y + step2a.z,
		step2a.y - step2a.z,
		step2a.x - step2a.w	
	);
	step1b	= T4(
		step2b.w - step2b.x,
		step2b.z - step2b.y,
		step2b.y + step2b.z,
		step2b.x + step2b.w	
	);

	// stage 6
	step2a	= T4(
		step1a.x,
		step1a.y,
		dct_round((step1b.y - step1a.z) * cospi_16_64),
		dct_round((step1b.x - step1a.w) * cospi_16_64)
	);
	step2b	= T4(
		dct_round((step1a.w + step1b.x) * cospi_16_64),
		dct_round((step1a.z + step1b.y) * cospi_16_64),
		step1b.z,
		step1b.w
	);

	// stage 7
	o0	= t0 + step2b.wzyx;
	o1	= t1 + step2a.wzyx;
	o2	= t1.wzyx - step2a;
	o3	= t0.wzyx - step2b;
}

void idct32(
	T4 i0, T4 i1, T4 i2, T4 i3, T4 i4, T4 i5, T4 i6, T4 i7,
	out T4 o0, out T4 o1, out T4 o2, out T4 o3, out T4 o4, out T4 o5, out T4 o6, out T4 o7
) {
	T4	step1[8], step2[8];

	T4	t0, t1, t2, t3;
	idct16(T4(i0.xz, i1.xz), T4(i2.xz, i3.xz), T4(i4.xz, i5.xz), T4(i6.xz, i7.xz), t0, t1, t2, t3);

	// stage 1
	step1[4]	= dct_round(T4(
		i0.y * cospi_31_64 - i7.w * cospi_1_64,
		i4.y * cospi_15_64 - i3.w * cospi_17_64,
		i2.y * cospi_23_64 - i5.w * cospi_9_64,
		i6.y * cospi_7_64  - i1.w * cospi_25_64
	));
	step1[5]	= dct_round(T4(
		i1.y * cospi_27_64 - i6.w * cospi_5_64,
		i5.y * cospi_11_64 - i2.w * cospi_21_64,
		i3.y * cospi_19_64 - i4.w * cospi_13_64,
		i7.y * cospi_3_64  - i0.w * cospi_29_64
	));
	step1[6]	= dct_round(T4(
		i7.y * cospi_29_64 + i0.w * cospi_3_64,
		i3.y * cospi_13_64 + i4.w * cospi_19_64,
		i5.y * cospi_21_64 + i2.w * cospi_11_64,
		i1.y * cospi_5_64  + i6.w * cospi_27_64
	));
	step1[7]	= dct_round(T4(
		i6.y * cospi_25_64 + i1.w * cospi_7_64,
		i2.y * cospi_9_64  + i5.w * cospi_23_64,
		i4.y * cospi_17_64 + i3.w * cospi_15_64,
		i0.y * cospi_1_64  + i7.w * cospi_31_64
	));

	// stage 2
	step2[4]	= T4(
		step1[4].x + step1[4].y,
		step1[4].x - step1[4].y,
		step1[4].w - step1[4].z,
		step1[4].z + step1[4].w
	);
	step2[5]	= T4(
		step1[5].x + step1[5].y,
		step1[5].x - step1[5].y,
		step1[5].w - step1[5].z,
		step1[5].z + step1[5].w
	);
	step2[6]	= T4(
		step1[6].x + step1[6].y,
		step1[6].x - step1[6].y,
		step1[6].w - step1[6].z,
		step1[6].z + step1[6].w
	);
	step2[7]	= T4(
		step1[7].x + step1[7].y,
		step1[7].x - step1[7].y,
		step1[7].w - step1[7].z,
		step1[7].z + step1[7].w
	);

	// stage 3
	step1[4]	= T4(
		step2[4].x,
		dct_round(-step2[4].y * cospi_4_64  + step2[7].z * cospi_28_64),
		dct_round(-step2[4].z * cospi_28_64 - step2[7].y * cospi_4_64),
		step2[4].w
	);
	step1[5]	= T4(
		step2[5].x,
		dct_round(-step2[5].y * cospi_20_64 + step2[6].z * cospi_12_64),
		dct_round(-step2[5].z * cospi_12_64 - step2[6].y * cospi_20_64),
		step2[5].w
	);
	step1[6]	= T4(
		step2[6].x,
		dct_round(-step2[5].z * cospi_20_64 + step2[6].y * cospi_12_64),
		dct_round( step2[5].y * cospi_12_64 + step2[6].z * cospi_20_64),
		step2[6].w
	);
	step1[7]	= T4(
		step2[7].x,
		dct_round(-step2[4].z * cospi_4_64  + step2[7].y * cospi_28_64),
		dct_round( step2[4].y * cospi_28_64 + step2[7].z * cospi_4_64),
		step2[7].w
	);

	// stage 4
	step2[4]	= T4(
		step1[4].x + step1[4].w,
		step1[4].y + step1[4].z,
		step1[4].y - step1[4].z,
		step1[4].x - step1[4].w
	);
	step2[5]	= T4(
		step1[5].w - step1[5].x,
		step1[5].z - step1[5].y,
		step1[5].y + step1[5].z,
		step1[5].x + step1[5].w
	);
	step2[6]	= T4(
		step1[6].x + step1[6].w,
		step1[6].y + step1[6].z,
		step1[6].y - step1[6].z,
		step1[6].x - step1[6].w
	);
	step2[7]	= T4(
		step1[7].w - step1[7].x,
		step1[7].z - step1[7].y,
		step1[7].y + step1[7].z,
		step1[7].x + step1[7].w
	);

	// stage 5
	step1[4]	= T4(
		step2[4].x,
		step2[4].y,
		dct_round(-step2[4].z * cospi_8_64  + step2[7].y * cospi_24_64),
		dct_round(-step2[4].w * cospi_8_64  + step2[7].x * cospi_24_64)
	);
	step1[5]	= T4(
		dct_round(-step2[5].x * cospi_24_64 - step2[6].w * cospi_8_64),
		dct_round(-step2[5].y * cospi_24_64 - step2[6].z * cospi_8_64),
		step2[5].z,
		step2[5].w
	);
	step1[6]	= T4(
		step2[6].x,
		step2[6].y,
		dct_round(-step2[5].y * cospi_8_64  + step2[6].z * cospi_24_64),
		dct_round(-step2[5].x * cospi_8_64  + step2[6].w * cospi_24_64)
	);
	step1[7]	= T4(
		dct_round( step2[4].w * cospi_24_64 + step2[7].x * cospi_8_64),
		dct_round( step2[4].z * cospi_24_64 + step2[7].y * cospi_8_64),
		step2[7].z,
		step2[7].w
	);

	// stage 6
	step2[4]	= step1[4] + step1[5].wzyx;
	step2[5]	= step1[4].wzyx - step1[5];
	step2[6]	= step1[7].wzyx - step1[6];
	step2[7]	= step1[7] + step1[6].wzyx;

	// stage 7
	step1[4]	= step2[4];
	step1[5]	= dct_round((step2[6].wzyx - step2[5]) * cospi_16_64);
	step1[6]	= dct_round((step2[5].wzyx + step2[6]) * cospi_16_64);
	step1[7]	= step2[7];

	// final stage
	o0	= t0 + step1[7].wzyx;
	o1	= t1 + step1[6].wzyx;
	o2	= t2 + step1[5].wzyx;
	o3	= t3 + step1[4].wzyx;
	o4	= t3.wzyx - step1[4];
	o5	= t2.wzyx - step1[5];
	o6	= t1.wzyx - step1[6];
	o7	= t0.wzyx - step1[7];
}

//-----------------------------------------------------------------------------
//	IADST
//-----------------------------------------------------------------------------

void iadst4(T4 i0, out T4 o0) {
	T4	s0 = i0.xxyz * T4(
		sinpi_1_9_sqrt2,
		sinpi_2_9_sqrt2,
		sinpi_3_9_sqrt2,
		sinpi_4_9_sqrt2
	);
	T4	s1 = T4(
		sinpi_1_9_sqrt2 * i0.z,
		sinpi_2_9_sqrt2 * i0.w,
		sinpi_4_9_sqrt2 * i0.w,
		sinpi_3_9_sqrt2 * (i0.x - i0.z + i0.w)
	);
	T	s2x = s0.x + s0.w + s1.y;
	T	s2y = s0.y - s1.x - s1.z;

	// 1-D transform scaling factor is sqrt(2)
	o0 = dct_round(T4(
		s2x + s0.z,
		s2y + s0.z,
		s1.w,
		s2x + s2y - s0.z
	));
}

void iadst8(T4 i0, T4 i1, out T4 o0, out T4 o1) {
	T4	x0	= T4(i1.w, i0.x, i1.y, i0.z);
	T4	x1	= T4(i0.w, i1.x, i0.y, i1.z);

	// stage 1
	T4	s0	= T4(
		cospi_2_64  * x0.x + cospi_30_64 * x0.y,
		cospi_30_64 * x0.x - cospi_2_64  * x0.y,
		cospi_10_64 * x0.z + cospi_22_64 * x0.w,
		cospi_22_64 * x0.z - cospi_10_64 * x0.w
	);
	T4	s1	= T4(
		cospi_18_64 * x1.x + cospi_14_64 * x1.y,
		cospi_14_64 * x1.x - cospi_18_64 * x1.y,
		cospi_26_64 * x1.z + cospi_6_64  * x1.w,
		cospi_6_64  * x1.z - cospi_26_64 * x1.w
	);

	x0	= dct_round(s0 + s1);
	x1	= dct_round(s0 - s1);

	// stage 2
	s1	= T4(
		 cospi_8_64  * x1.x + cospi_24_64 * x1.y,
		-cospi_8_64  * x1.y + cospi_24_64 * x1.x,
		 cospi_8_64  * x1.w - cospi_24_64 * x1.z,
		 cospi_8_64  * x1.z + cospi_24_64 * x1.w
	);

	x0	= x0.xyxy + T4(x0.zw, -x0.zw);
	x1	= dct_round(s1.xyxy + T4(s1.zw, -s1.zw));

	// stage 3
	s0	= dct_round(cospi_16_64 * T4(
		x0.z + x0.w,
		x0.z - x0.w,
		x1.z + x1.w,
		x1.z - x1.w
	));

	o0 = T4(x0.x, -x1.x, s0.z, -s0.x);
	o1 = T4(s0.y, -s0.w, x1.y, -x0.y);
}

void iadst16(T4 i0, T4 i1, T4 i2, T4 i3, out T4 o0, out T4 o1, out T4 o2, out T4 o3) {
	T4	x0	= T4(i3.w, i0.x, i3.y, i0.z);
	T4	x1	= T4(i2.w, i1.x, i2.y, i1.z);
	T4	x2	= T4(i1.w, i2.x, i1.y, i2.z);
	T4	x3	= T4(i0.w, i3.x, i0.y, i3.z);

	// stage 1
	T4	s0	= T4(
		x0.x  * cospi_1_64  + x0.y * cospi_31_64,
		x0.x  * cospi_31_64 - x0.y * cospi_1_64,
		x0.z  * cospi_5_64  + x0.w * cospi_27_64,
		x0.z  * cospi_27_64 - x0.w * cospi_5_64
	);
	T4	s1	= T4(
		x1.x  * cospi_9_64  + x1.y * cospi_23_64,
		x1.x  * cospi_23_64 - x1.y * cospi_9_64,
		x1.z  * cospi_13_64 + x1.w * cospi_19_64,
		x1.z  * cospi_19_64 - x1.w * cospi_13_64
	);
	T4	s2	= T4(
		x2.x * cospi_17_64 + x2.y * cospi_15_64,
		x2.x * cospi_15_64 - x2.y * cospi_17_64,
		x2.z * cospi_21_64 + x2.w * cospi_11_64,
		x2.z * cospi_11_64 - x2.w * cospi_21_64
	);
	T4	s3	= T4(
		x3.x * cospi_25_64 + x3.y * cospi_7_64,
		x3.x * cospi_7_64  - x3.y * cospi_25_64,
		x3.z * cospi_29_64 + x3.w * cospi_3_64,
		x3.z * cospi_3_64  - x3.w * cospi_29_64
	);

	x0	= dct_round(s0 + s2);
	x1	= dct_round(s1 + s3);
	x2	= dct_round(s0 - s2);
	x3	= dct_round(s1 - s3);

	// stage 2
	s0	= x0;
	s1	= x1;
	s2	= T4(
		x2.x * cospi_4_64  + x2.y * cospi_28_64,
		x2.x * cospi_28_64 - x2.y * cospi_4_64,
		x2.z * cospi_20_64 + x2.w * cospi_12_64,
		x2.z * cospi_12_64 - x2.w * cospi_20_64
	);
	s3	= T4(
		x3.y * cospi_4_64  - x3.x * cospi_28_64,
		x3.x * cospi_4_64  + x3.y * cospi_28_64,
		x3.w * cospi_20_64 - x3.z * cospi_12_64,
		x3.z * cospi_20_64 + x3.w * cospi_12_64
	);

	x0	= s0 + s1;
	x1	= s0 - s1;
	x2	= dct_round(s2 + s3);
	x3	= dct_round(s2 - s3);

	// stage 3
	s0	= x0;
	s1	= T4(
		 x1.x * cospi_8_64 + x1.y * cospi_24_64,
		-x1.y * cospi_8_64 + x1.x * cospi_24_64,
		 x1.w * cospi_8_64 - x1.z * cospi_24_64,
		 x1.z * cospi_8_64 + x1.w * cospi_24_64
	);
	s2	= x2;
	s3	= T4(
		 x3.x * cospi_8_64 + x3.y * cospi_24_64,
		-x3.y * cospi_8_64 + x3.x * cospi_24_64 ,
		 x3.w * cospi_8_64 - x3.z * cospi_24_64,
		 x3.z * cospi_8_64 + x3.w * cospi_24_64
	);

	x0	= s0.xyxy + T4(s0.zw, -s0.zw);
	x1	= dct_round(s1.xyxy + T4(s1.zw, -s1.zw));
	x2	= s2.xyxy + T4(s2.zw, -s2.zw);
	x3	= dct_round(s3.xyxy + T4(s3.zw, -s3.zw));

	// stage 4
	s0	= dct_round(cospi_16_64 * T4(
		-x0.z - x0.w,
		 x0.z - x0.w,
		 x1.z + x1.w,
		-x1.z + x1.w
	));

	s1	= dct_round(cospi_16_64 * T4(
		 x2.z + x2.w,
		-x2.z + x2.w,
		-x3.z - x3.w,
		 x3.z - x3.w
	));

	o0	= T4(x0.x, -x2.x, x3.x, -x1.x);
	o1	= T4(s0.z,  s1.z, s1.x,  s0.x);
	o2	= T4(s0.y,  s1.y, s1.w,  s0.w);
	o3	= T4(x1.y, -x3.y, x2.y, -x0.y);
}

//-----------------------------------------------------------------------------
//	SHADERS
//-----------------------------------------------------------------------------

struct DCTinfo {
	uint	q;		//uint32	eob:10, q0:11, q1:11;
	uint	pos;	//uint16	x, y;
	uint	coeffs;
};

struct DCTbuffers {
	Buffer<T4>		input;
	RWBuffer<T4>	output;
};

struct DCTbuffersScan {
	Buffer<T>		input;
	RWBuffer<T4>	output;
};
struct DCTbuffersDQScan {
	RegularBuffer<DCTinfo>	info;
	BufferDescriptor		coeffs;
	RWBuffer<T4>			output;
};
struct DCTbuffersDQScan2 {
	RegularBuffer<DCTinfo>	info;
	BufferDescriptor		coeffs;
	RWTexture2D<T4>			output;
};

Buffer<T> get_coeff_buffer(BufferDescriptor desc, DCTinfo info) {
	desc.m_regs[0]	+= info.coeffs * 2;
	desc.m_regs[2]	=  (info.q & ((1 << 10) - 1)) + 1;
	return Buffer<T>(desc);
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

//-----------------------------------------------------------------------------
//	4x4 shaders
//-----------------------------------------------------------------------------

thread_group_memory	T	temp4x4[4 * 4];

#if 0
COMPUTE_SHADER(4,1,1)
void iwht4hv(uint dispatchid : DISPATCH_THREAD_ID, uint threadid : GROUP_THREAD_ID, DCTbuffers info : S_SRT_DATA) {
	T4	o0;
	iwht4(info.input[dispatchid], o0);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iwht4(get4h(temp4x4, 4, 0, threadid), info.output[dispatchid]);
}

technique iwht4x4 {
	pass p0 { OrbisComputeShader	= iwht4hv; }
};

COMPUTE_SHADER(4,1,1)
void idct4hv(uint dispatchid : DISPATCH_THREAD_ID, uint threadid : GROUP_THREAD_ID, DCTbuffers info : S_SRT_DATA) {
	T4	o0;
	idct4(info.input[dispatchid], o0);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	idct4(get4h(temp4x4, 4, 0, threadid), info.output[dispatchid]);
}

COMPUTE_SHADER(4,1,1)
void idct4h_iadst4v(uint dispatchid : DISPATCH_THREAD_ID, uint threadid : GROUP_THREAD_ID, DCTbuffers info : S_SRT_DATA) {
	T4	o0;
	idct4(info.input[dispatchid], o0);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iadst4(get4h(temp4x4, 4, 0, threadid), info.output[dispatchid]);
}

COMPUTE_SHADER(4,1,1)
void iadst4h_idct4v(uint dispatchid : DISPATCH_THREAD_ID, uint threadid : GROUP_THREAD_ID, DCTbuffers info : S_SRT_DATA) {
	T4	o0;
	iadst4(info.input[dispatchid], o0);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	idct4(get4h(temp4x4, 4, 0, threadid), info.output[dispatchid]);
}

COMPUTE_SHADER(4,1,1)
void iadst4h_iadst4v(uint dispatchid : DISPATCH_THREAD_ID, uint threadid : GROUP_THREAD_ID, DCTbuffers info : S_SRT_DATA) {
	T4	o0;
	iadst4(info.input[dispatchid], o0);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iadst4(get4h(temp4x4, 4, 0, threadid), info.output[dispatchid]);
}

technique iht4x4 {
	pass p0 { OrbisComputeShader	= idct4hv; }
	pass p1 { OrbisComputeShader	= idct4h_iadst4v; }
	pass p2 { OrbisComputeShader	= iadst4h_idct4v; }
	pass p3 { OrbisComputeShader	= iadst4h_iadst4v; }
};
#endif

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

ulong get_scan4x4(ulong s) {
	return s;
}
ulong get_scan4x4(uint2 s) {
	return s.x | (ulong(s.y) << 32);
}

COMPUTE_SHADER(4,1,1)
void all_iwht4(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint		scan	= get_scan4x4(default_iscan_4x4_trans) >> (threadid << 4);
	T4	o0;
	iwht4(
		unpack_get4n(coeffs, 0, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iwht4(get4h(temp4x4, 4, 0, threadid), o0);
	buffers.output[get_pos(info) + int2(0, threadid)] = dct_texels(o0, 4);
}

COMPUTE_SHADER(4,1,1)
void all_idct4h_idct4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint		scan	= get_scan4x4(default_iscan_4x4_trans) >> (threadid << 4);
	T4	o0;
	idct4(
		unpack_get4n(coeffs, 0, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	idct4(get4h(temp4x4, 4, 0, threadid), o0);
	buffers.output[get_pos(info) + int2(0, threadid)] = dct_texels(o0, 4);
}

COMPUTE_SHADER(4,1,1)
void all_idct4h_iadst4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint		scan	= get_scan4x4(row_iscan_4x4_trans) >> (threadid << 4);
	T4	o0;
	iadst4(
		unpack_get4n(coeffs, 0, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	idct4(get4h(temp4x4, 4, 0, threadid), o0);
	buffers.output[get_pos(info) + int2(0, threadid)] = dct_texels(o0, 4);
}

COMPUTE_SHADER(4,1,1)
void all_iadst4h_idct4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint		scan	= get_scan4x4(col_iscan_4x4_trans) >> (threadid << 4);
	T4	o0;
	idct4(
		unpack_get4n(coeffs, 0, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iadst4(get4h(temp4x4, 4, 0, threadid), o0);
	buffers.output[get_pos(info) + int2(0, threadid)] = dct_texels(o0, 4);
}

COMPUTE_SHADER(4,1,1)
void all_iadst4h_iadst4v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint		scan	= get_scan4x4(default_iscan_4x4_trans) >> (threadid << 4);
	T4	o0;
	iadst4(
		unpack_get4n(coeffs, 0, scan) * get_quant(info, threadid),
		o0
	);
	put4v(temp4x4, 4, threadid, 0, o0);
	ThreadGroupMemoryBarrierSync();
	iadst4(get4h(temp4x4, 4, 0, threadid), o0);
	buffers.output[get_pos(info) + int2(0, threadid)] = dct_texels(o0, 4);
}

//-----------------------------------------------------------------------------
//	8x8 shaders
//-----------------------------------------------------------------------------

thread_group_memory	T	temp8x8[8 * 8];

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

COMPUTE_SHADER(8,1,1)
void all_idct8h_idct8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint2		scan	= default_iscan_8x8_trans[threadid];

	T4	o0, o1;
	idct8(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 5);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 5);
}

COMPUTE_SHADER(8,1,1)
void all_idct8h_iadst8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint2		scan	= row_iscan_8x8_trans[threadid];

	T4	o0, o1;
	iadst8(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 5);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 5);
}

COMPUTE_SHADER(8,1,1)
void all_iadst8h_idct8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint2		scan	= col_iscan_8x8_trans[threadid];

	T4	o0, o1;
	idct8(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 5);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 5);
}

COMPUTE_SHADER(8,1,1)
void all_iadst8h_iadst8v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint2		scan	= default_iscan_8x8_trans[threadid];

	T4	o0, o1;
	iadst8(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 5);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 5);
}

//-----------------------------------------------------------------------------
//	16x16 shaders
//-----------------------------------------------------------------------------

thread_group_memory	T	temp16x16[16 * 16];

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

COMPUTE_SHADER(16,1,1)
void all_idct16h_idct16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint4		scan	= default_iscan_16x16_trans[threadid];

	T4	o0, o1, o2, o3;
	idct16(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.w) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 6);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 6);
	buffers.output[pos + int2(2, threadid)] = dct_texels(o2, 6);
	buffers.output[pos + int2(3, threadid)] = dct_texels(o3, 6);
}

COMPUTE_SHADER(16,1,1)
void all_idct16h_iadst16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint4		scan	= row_iscan_16x16_trans[threadid];

	T4	o0, o1, o2, o3;
	iadst16(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.w) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 6);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 6);
	buffers.output[pos + int2(2, threadid)] = dct_texels(o2, 6);
	buffers.output[pos + int2(3, threadid)] = dct_texels(o3, 6);
}

COMPUTE_SHADER(16,1,1)
void all_iadst16h_idct16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint4		scan	= col_iscan_16x16_trans[threadid];

	T4	o0, o1, o2, o3;
	idct16(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.w) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 6);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 6);
	buffers.output[pos + int2(2, threadid)] = dct_texels(o2, 6);
	buffers.output[pos + int2(3, threadid)] = dct_texels(o3, 6);
}

COMPUTE_SHADER(16,1,1)
void all_iadst16h_iadst16v(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint4		scan	= default_iscan_16x16_trans[threadid];

	T4	o0, o1, o2, o3;
	iadst16(
		unpack_get4(coeffs, 0, scan.x) * get_quant(info, threadid),
		unpack_get4(coeffs, 0, scan.y) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.z) * get_quant_ac(info),
		unpack_get4(coeffs, 0, scan.w) * get_quant_ac(info),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 6);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 6);
	buffers.output[pos + int2(2, threadid)] = dct_texels(o2, 6);
	buffers.output[pos + int2(3, threadid)] = dct_texels(o3, 6);
}

//-----------------------------------------------------------------------------
//	32x32 shaders
//-----------------------------------------------------------------------------

thread_group_memory	T	temp32x32[32 * 32];

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


COMPUTE_SHADER(32,1,1)
void all_idct32(uint threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, DCTbuffersDQScan2 buffers : S_SRT_DATA) {
	DCTinfo		info	= buffers.info[groupid];
	Buffer<T>	coeffs	= get_coeff_buffer(buffers.coeffs, info);
	uint4		scan0	= default_iscan_32x32_trans[threadid * 4 + 0];
	uint4		scan1	= default_iscan_32x32_trans[threadid * 4 + 1];
	uint4		scan2	= default_iscan_32x32_trans[threadid * 4 + 2];
	uint4		scan3	= default_iscan_32x32_trans[threadid * 4 + 3];

	T4	o0, o1, o2, o3, o4, o5, o6, o7;
	idct32(
		quant32_fudge(unpack_get4(coeffs, 0, scan0.xy) * get_quant(info, threadid)),
		quant32_fudge(unpack_get4(coeffs, 0, scan0.zw) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, 0, scan1.xy) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, 0, scan1.zw) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, 0, scan2.xy) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, 0, scan2.zw) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, 0, scan3.xy) * get_quant_ac(info)),
		quant32_fudge(unpack_get4(coeffs, 0, scan3.zw) * get_quant_ac(info)),
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
	buffers.output[pos + int2(0, threadid)] = dct_texels(o0, 6);
	buffers.output[pos + int2(1, threadid)] = dct_texels(o1, 6);
	buffers.output[pos + int2(2, threadid)] = dct_texels(o2, 6);
	buffers.output[pos + int2(3, threadid)] = dct_texels(o3, 6);
	buffers.output[pos + int2(4, threadid)] = dct_texels(o4, 6);
	buffers.output[pos + int2(5, threadid)] = dct_texels(o5, 6);
	buffers.output[pos + int2(6, threadid)] = dct_texels(o6, 6);
	buffers.output[pos + int2(7, threadid)] = dct_texels(o7, 6);
}

//-----------------------------------------------------------------------------

technique transforms {
	pass p0 { OrbisComputeShader	= all_iwht4; }
	pass p1 { OrbisComputeShader	= all_idct4h_idct4v; }
	pass p2 { OrbisComputeShader	= all_idct4h_iadst4v; }
	pass p3 { OrbisComputeShader	= all_iadst4h_idct4v; }
	pass p4 { OrbisComputeShader	= all_iadst4h_iadst4v; }

	pass p5 { OrbisComputeShader	= all_idct8h_idct8v; }
	pass p6 { OrbisComputeShader	= all_idct8h_iadst8v; }
	pass p7 { OrbisComputeShader	= all_iadst8h_idct8v; }
	pass p8 { OrbisComputeShader	= all_iadst8h_iadst8v; }

	pass p9 { OrbisComputeShader	= all_idct16h_idct16v; }
	pass p10 { OrbisComputeShader	= all_idct16h_iadst16v; }
	pass p11 { OrbisComputeShader	= all_iadst16h_idct16v; }
	pass p12 { OrbisComputeShader	= all_iadst16h_iadst16v; }

	pass p13 { OrbisComputeShader	= all_idct32; }
};


//-----------------------------------------------------------------------------
//	INTRA-PREDICTION
//-----------------------------------------------------------------------------

int avg2(int a, int b)			{ return (a + b + 1) >> 1; }
int avg3(int a, int b, int c)	{ return (a + 2 * b + c + 2) >> 2; }

enum PREDICTION_MODE {
	PRED_DC			= 0,	// Average of above and left pixels
	PRED_V			= 1,	// Vertical
	PRED_H			= 2,	// Horizontal
	PRED_D45		= 3,	// Directional 45  deg = round(arctan(1/1) * 180/pi)
	PRED_D135		= 4,	// Directional 135 deg = 180 - 45
	PRED_D117		= 5,	// Directional 117 deg = 180 - 63
	PRED_D153		= 6,	// Directional 153 deg = 180 - 27
	PRED_D207		= 7,	// Directional 207 deg = 180 + 27
	PRED_D63		= 8,	// Directional 63  deg = round(arctan(2/1) * 180/pi)
	PRED_TM			= 9,	// True-motion

	DC_NO_L			= 10,
	DC_NO_A			= 11,
	DC_127			= 12,
	DC_129			= 13,
	D45_NO_R		= 14,
	D135_NO_L		= 15,
	D135_NO_A		= 16,
	D117_NO_L		= 17,
	D117_NO_A		= 18,
	D153_NO_L		= 19,
	D153_NO_A		= 20,
	D63_NO_R		= 21,
};

int intraDC(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	a	= dest[uv + int2(duv.x, -1)];
	int	b	= dest[uv + int2(-1, duv.x)];
	a += QuadSwizzle(a, 1u, 0u, 3u, 2u);
	b += QuadSwizzle(b, 1u, 0u, 3u, 2u);

	a += QuadSwizzle(a, 2u, 3u, 0u, 1u);
	b += QuadSwizzle(b, 2u, 3u, 0u, 1u);

	if (size > 4) {
		a += LaneSwizzle(a, 31u, 0u, 4u);
		b += LaneSwizzle(b, 31u, 0u, 4u);
		if (size > 8) {
			a += LaneSwizzle(a, 31u, 0u, 8u);
			b += LaneSwizzle(b, 31u, 0u, 8u);
			if (size > 16) {
				a += LaneSwizzle(a, 31u, 0u, 16u);
				b += LaneSwizzle(b, 31u, 0u, 16u);
			}
		}
	}
	return (a + b + size) / (size * 2);
}

int intraDC_NO_L(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	a	= dest[uv + int2(duv.x, -1)];
	a += QuadSwizzle(a, 1u, 0u, 3u, 2u);
	a += QuadSwizzle(a, 2u, 3u, 0u, 1u);

	if (size > 4) {
		a += LaneSwizzle(a, 31u, 0u, 4u);
		if (size > 8) {
			a += LaneSwizzle(a, 31u, 0u, 8u);
			if (size > 16)
				a += LaneSwizzle(a, 31u, 0u, 16u);
		}
	}
	return (a + size / 2) / size;
}

int intraDC_NO_A(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	a	= dest[uv + int2(-1, duv.x)];
	a += QuadSwizzle(a, 1u, 0u, 3u, 2u);
	a += QuadSwizzle(a, 2u, 3u, 0u, 1u);

	if (size > 4) {
		a += LaneSwizzle(a, 31u, 0u, 4u);
		if (size > 8) {
			a += LaneSwizzle(a, 31u, 0u, 8u);
			if (size > 16)
				a += LaneSwizzle(a, 31u, 0u, 16u);
		}
	}
	return (a + size / 2) / size;
}

int intraDC_127(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	return 127;
}

int intraDC_129(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	return 129;
}

int intraV(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	return dest[uv + int2(duv.x, -1)];
}

int intraH(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	return dest[uv + int2(-1, duv.y)];
}

int intraD45(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	u	= duv.x + duv.y;
	return u == (size - 1) + (size - 1)
		? dest[uv + int2(u + 1, -1)]
		: avg3(dest[uv + int2(u, -1)], dest[uv + int2(u + 1, -1)], dest[uv + int2(u + 2, -1)]);
}
int intraD45_NO_R(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	u	= duv.x + duv.y;
	int	m	= size - 1;
	return avg3(dest[uv + int2(min(u, m), -1)], dest[uv + int2(min(u + 1, m), -1)], dest[uv + int2(min(u + 2, m), -1)]);
}

int intraD135(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x) {
		int	u = duv.x - duv.y;
		return avg3(dest[uv + (u == 0 ? int2(-1, 0) : int2(u - 2, -1))], dest[uv + int2(u - 1, -1)], dest[uv + int2(u, -1)]);
	} else {
		int	v = duv.y - duv.x;
		return avg3(dest[uv + int2(-1, v - 2)], dest[uv + int2(-1, v - 1)], dest[uv + int2(-1, v)]);
	}
}
int intraD135_NO_L(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x) {
		int	u = duv.x - duv.y;
		return avg3(u < 2 ? 129 : dest[uv + int2(u - 2, -1)], u < 1 ? 129 : dest[uv + int2(u - 1, -1)], dest[uv + int2(u, -1)]);
	} else {
		return 129;
	}
}
int intraD135_NO_A(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x) {
		int	u = duv.x - duv.y;
		return avg3(u == 0 ? dest[uv + int2(-1, 0)] : 127, 127, 127);
	} else {
		int	v = duv.y - duv.x;
		return avg3(v < 2 ? 127 : dest[uv + int2(-1, v - 2)], v < 1 ? 127 : dest[uv + int2(-1, v - 1)], dest[uv + int2(-1, v)]);
	}
}
int intraD117(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x * 2) {
		int	u = duv.x - duv.y / 2;
		int	a = dest[uv + int2(u - 1, -1)], b = dest[uv + int2(u, -1)];
		return (duv.y & 1) == 0
			? avg2(a, b)
			: avg3(dest[uv + int2(u - 2, -1)], a, b);
	} else {
		int	v = duv.y - duv.x * 2;
		return avg3(dest[uv + (v == 1 ? int2(0, -1) : int2(-1, v - 3))], dest[uv + int2(-1, v - 2)], dest[uv + int2(-1, v - 1)]);
	}
}
int intraD117_NO_L(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x * 2) {
		int	u = duv.x - duv.y / 2;
		int	a = u < 1 ? 129 : dest[uv + int2(u - 1, -1)], b = dest[uv + int2(u, -1)];
		return (duv.y & 1) == 0
			? avg2(a, b)
			: avg3(u < 2 ? 129 : dest[uv + int2(u - 2, -1)], a, b);
	} else {
		int	v = duv.y - duv.x * 2;
		return avg3(v == 1 ? dest[uv + int2(0, -1)] : 129, 129, 129);
	}
}
int intraD117_NO_A(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x * 2) {
		return 127;
	} else {
		int	v = duv.y - duv.x * 2;
		return avg3(v < 3 ? 127 : dest[uv + int2(-1, v - 3)], v < 2 ? 127 : dest[uv + int2(-1, v - 2)], v < 1 ? 127 : dest[uv + int2(-1, v - 1)]);
	}
}
int intraD153(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y < duv.x / 2) {
		int	u = duv.x - duv.y * 2;
		return avg3(dest[uv + int2(u - 3, -1)], dest[uv + int2(u - 2, -1)], dest[uv + int2(u - 1, -1)]);
	} else {
		int	v = duv.y - duv.x / 2;
		int	a = dest[uv + int2(-1, v - 1)], b = dest[uv + int2(-1, v)];
		return (duv.x & 1) == 0
			? avg2(a, b)
			: avg3(dest[uv + (v == 0 ? int2(0, -1) : int2(-1, v - 2))], a, b);
	}
}
int intraD153_NO_L(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y <= duv.x / 2) {
		int	u = duv.x - duv.y * 2;
		return avg3(u < 3 ? 129 : dest[uv + int2(u - 3, -1)], u < 2 ? 129 : dest[uv + int2(u - 2, -1)], u < 1 ? 129 : dest[uv + int2(u - 1, -1)]);
	} else {
		return 129;
	}
}
int intraD153_NO_A(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	if (duv.y < duv.x / 2) {
		return 127;
	} else {
		int	v = duv.y - duv.x / 2;
		int	a = v < 1 ? 127 : dest[uv + int2(-1, v - 1)], b = dest[uv + int2(-1, v)];
		return (duv.x & 1) == 0
			? avg2(a, b)
			: avg3(v < 2 ? 127 : dest[uv + int2(-1, v - 2)], a, b);
	}
}
int intraD207(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	v	= duv.y + duv.x / 2;
	int	m	= size - 1;
	int	a	= dest[uv + int2(-1, min(v, m))], b = dest[uv + int2(-1, min(v + 1, m))];
	return (duv.x & 1) == 0
		? avg2(a, b)
		: avg3(a, b, dest[uv + int2(-1, min(v + 2, m))]);
}

int intraD63(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	u	= duv.x + duv.y / 2;
	int	m	= size + 3;
	int	a	= dest[uv + int2(min(u, m), -1)], b = dest[uv + int2(min(u + 1, m), -1)];
	return (duv.y & 1) == 0
		? avg2(a, b)
		: avg3(a, b, dest[uv + int2(min(u + 2, m), -1)]);
}
int intraD63_NO_R(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	int	u	= duv.x + duv.y / 2;
	int	m	= size - 1;
	int	a	= dest[uv + int2(min(u, m), -1)], b = dest[uv + int2(min(u + 1, m), -1)];
	return (duv.y & 1) == 0
		? avg2(a, b)
		: avg3(a, b, dest[uv + int2(min(u + 2, m), -1)]);
}

int intraTM(int size, int2 uv, int2 duv, RWTexture2D<int> dest) {
	return clamp(dest[uv + int2(-1, duv.y)] + dest[uv + int2(duv.x, -1)] - dest[uv + int2(-1, -1)], 0, 0xff);
}

int intra(int size, int mode, int2 uv, int2 duv, RWTexture2D<int> dest) {
	switch (mode) {
		case PRED_DC:		return intraDC		(size, uv, duv, dest);
		case PRED_V:		return intraV		(size, uv, duv, dest);
		case PRED_H:		return intraH		(size, uv, duv, dest);
		case PRED_D45:		return intraD45		(size, uv, duv, dest);
		case PRED_D135:		return intraD135	(size, uv, duv, dest);
		case PRED_D117:		return intraD117	(size, uv, duv, dest);
		case PRED_D153:		return intraD153	(size, uv, duv, dest);
		case PRED_D207:		return intraD207	(size, uv, duv, dest);
		case PRED_D63:		return intraD63		(size, uv, duv, dest);
		case PRED_TM:		return intraTM		(size, uv, duv, dest);
		case DC_NO_L:		return intraDC_NO_L	(size, uv, duv, dest);
		case DC_NO_A:		return intraDC_NO_A	(size, uv, duv, dest);
		case DC_127:		return intraDC_127	(size, uv, duv, dest);
		case DC_129:		return intraDC_129	(size, uv, duv, dest);
		case D45_NO_R:		return intraD45_NO_R(size, uv, duv, dest);
		case D135_NO_L:		return intraD135_NO_L(size, uv, duv, dest);
		case D135_NO_A:		return intraD135_NO_A(size, uv, duv, dest);
		case D117_NO_L:		return intraD117_NO_L(size, uv, duv, dest);
		case D117_NO_A:		return intraD117_NO_A(size, uv, duv, dest);
		case D153_NO_L:		return intraD153_NO_L(size, uv, duv, dest);
		case D153_NO_A:		return intraD153_NO_A(size, uv, duv, dest);
		case D63_NO_R:		return intraD63_NO_R(size, uv, duv, dest);
	}
}

int		get_pred_mode(uint info)	{ return info & 63; }
int2	get_pred_uv(uint info)		{ return int2(BitFieldExtract(info, 6u, 13u), BitFieldExtract(info, 19u, 13u)); }

struct PREDbuffers {
	RegularBuffer<uint>		info;
//	RWTexture2D<int>		dest;
	sce::Gnm::Texture		dest;
};
COMPUTE_SHADER(4,4,1)
void intra4(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid];
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(4, get_pred_mode(info), uv, threadid, dest));
}

COMPUTE_SHADER(8,8,1)
void intra8(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid];
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(8, get_pred_mode(info), uv, threadid, dest));
}

COMPUTE_SHADER(16,16,1)
void intra16(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid];
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(16, get_pred_mode(info), uv, threadid, dest));
}

COMPUTE_SHADER(32,32,1)
void intra32(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid];
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(32, get_pred_mode(info), uv, threadid, dest));
}

technique intra_predictions {
	pass p0 { OrbisComputeShader	= intra4; }
	pass p1 { OrbisComputeShader	= intra8; }
	pass p2 { OrbisComputeShader	= intra16; }
	pass p3 { OrbisComputeShader	= intra32; }
};

struct PREDbuffers2 {
	RegularBuffer<uint2>	info;
//	RWTexture2D<int>		dest;
	sce::Gnm::Texture		dest;
};
COMPUTE_SHADER(4,4,1)
void intra4_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid].x;
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(4, get_pred_mode(info), uv, threadid, dest));
}

COMPUTE_SHADER(8,8,1)
void intra8_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid].x;
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(8, get_pred_mode(info), uv, threadid, dest));
}

COMPUTE_SHADER(16,16,1)
void intra16_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid].x;
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(16, get_pred_mode(info), uv, threadid, dest));
}

COMPUTE_SHADER(32,32,1)
void intra32_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint				info	= buffers.info[groupid].x;
	RWTexture2D<int>	dest	= RWTexture2D<int>(buffers.dest);
	int2				uv		= get_pred_uv(info);
	add_clamp_pixel(dest, uv + threadid, intra(32, get_pred_mode(info), uv, threadid, dest));
}

technique intra_predictions2 {
	pass p0 { OrbisComputeShader	= intra4_2; }
	pass p1 { OrbisComputeShader	= intra8_2; }
	pass p2 { OrbisComputeShader	= intra16_2; }
	pass p3 { OrbisComputeShader	= intra32_2; }
};

//-----------------------------------------------------------------------------
//	INTER-PREDICTION
//-----------------------------------------------------------------------------

struct INTERinfo {
	uint	u;
};

struct INTERtextures {
	RWTexture2D<int>		dest;
	Texture2D<uint>			src[3];
};

struct INTERbuffers {
	Buffer<uint>			info;
	Buffer<uint>			mv;
	INTERtextures			*textures;
	uint					width;
};

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

thread_group_memory	uint	temp_convolve0[24 * 24];
thread_group_memory	uint	temp_convolve1[24 * 24];

int2 unpack_mv(uint x) {
	return int2(x, x << 16) >> 16;
}

int convolve(uint table[], const uint2 filters, int i, int di) {
	return	dot(int4(table[i + di * 0], table[i + di * 1], table[i + di * 2], table[i + di * 3]), unpack4i(filters.x) + 1)
		+	dot(int4(table[i + di * 4], table[i + di * 5], table[i + di * 6], table[i + di * 7]), unpack4i(filters.y) + 1);
}

void read4(out uint	a[], uint i, Texture2D<uint> src, int2 uv, int2 minuv, int2 maxuv) {
	a[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), minuv, maxuv)];
	a[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), minuv, maxuv)];
	a[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), minuv, maxuv)];
	a[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), minuv, maxuv)];
}

uint inter_y1(uint2 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int2 mvoffset, Texture2D<uint> src) {
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
		#if 0
			read4(temp_convolve0, i, src, uv, int2(0, 0), maxuv);
		#else
			temp_convolve0[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), int2(0, 0), maxuv)];
		#endif
			break;
		}
		case 1: {
			mv		= unpack_mv(mvbuffer[mvoffset + (threadid.x/4)]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + threadid.y * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			temp_convolve0[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 0 * 24 + 4]	= src[clamp(uv + int2(+1, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 4]	= src[clamp(uv + int2(+1, +5), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), int2(0, 0), maxuv)];
			break;
		}
		case 2: {
			mv		= unpack_mv(mvbuffer[mvoffset + (threadid.y/4)]) * 2;
			i		= threadid.x + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			temp_convolve0[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 4 * 24 + 0]	= src[clamp(uv + int2(-3, +1), int2(0, 0), maxuv)];
			temp_convolve0[i + 4 * 24 + 8]	= src[clamp(uv + int2(+5, +1), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), int2(0, 0), maxuv)];
			break;
		}
		case 3: {
			mv		= unpack_mv(mvbuffer[mvoffset + (threadid.x/4) + (threadid.y/4) * 2]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv	= pos + mv / SUBPEL_FACTOR;
			temp_convolve0[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 0 * 24 + 4]	= src[clamp(uv + int2(+1, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), int2(0, 0), maxuv)];
			temp_convolve0[i + 4 * 24 + 0]	= src[clamp(uv + int2(-3, +1), int2(0, 0), maxuv)];
			temp_convolve0[i + 4 * 24 + 4]	= src[clamp(uv + int2(+1, +1), int2(0, 0), maxuv)];
			temp_convolve0[i + 4 * 24 + 8]	= src[clamp(uv + int2(+5, +1), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 4]	= src[clamp(uv + int2(+1, +5), int2(0, 0), maxuv)];
			temp_convolve0[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), int2(0, 0), maxuv)];
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
	return round_pow2(sum, FILTER_BITS * 2);
}

uint inter_y2(uint2 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int2 mvoffset, Texture2D<uint> src0, Texture2D<uint> src1) {
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
			temp_convolve0[i + 0 * 24 + 0]	= src0[clamp(uv0 + int2(-3, -3), int2(0, 0), max0)];
			temp_convolve0[i + 0 * 24 + 8]	= src0[clamp(uv0 + int2(+5, -3), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 0]	= src0[clamp(uv0 + int2(-3, +5), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 8]	= src0[clamp(uv0 + int2(+5, +5), int2(0, 0), max0)];

			temp_convolve1[i + 0 * 24 + 0]	= src1[clamp(uv1 + int2(-3, -3), int2(0, 0), max1)];
			temp_convolve1[i + 0 * 24 + 8]	= src1[clamp(uv1 + int2(+5, -3), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 0]	= src1[clamp(uv1 + int2(-3, +5), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 8]	= src1[clamp(uv1 + int2(+5, +5), int2(0, 0), max1)];
			break;
		}
		case 1: {
			mvoffset += (threadid.x/4) * 2;
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + threadid.y * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			temp_convolve0[i + 0 * 24 + 0]	= src0[clamp(uv0 + int2(-3, -3), int2(0, 0), max0)];
			temp_convolve0[i + 0 * 24 + 4]	= src0[clamp(uv0 + int2(+1, -3), int2(0, 0), max0)];
			temp_convolve0[i + 0 * 24 + 8]	= src0[clamp(uv0 + int2(+5, -3), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 0]	= src0[clamp(uv0 + int2(-3, +5), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 4]	= src0[clamp(uv0 + int2(+1, +5), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 8]	= src0[clamp(uv0 + int2(+5, +5), int2(0, 0), max0)];

			temp_convolve1[i + 0 * 24 + 0]	= src1[clamp(uv1 + int2(-3, -3), int2(0, 0), max1)];
			temp_convolve1[i + 0 * 24 + 4]	= src1[clamp(uv1 + int2(+1, -3), int2(0, 0), max1)];
			temp_convolve1[i + 0 * 24 + 8]	= src1[clamp(uv1 + int2(+5, -3), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 0]	= src1[clamp(uv1 + int2(-3, +5), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 4]	= src1[clamp(uv1 + int2(+1, +5), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 8]	= src1[clamp(uv1 + int2(+5, +5), int2(0, 0), max1)];
			break;
		}
		case 2: {
			mvoffset += (threadid.y/4) * 2;
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			temp_convolve0[i + 0 * 24 + 0]	= src0[clamp(uv0 + int2(0, 0), int2(0, 0), max0)];
			temp_convolve0[i + 0 * 24 + 8]	= src0[clamp(uv0 + int2(8, 0), int2(0, 0), max0)];
			temp_convolve0[i + 4 * 24 + 0]	= src0[clamp(uv0 + int2(0, 4), int2(0, 0), max0)];
			temp_convolve0[i + 4 * 24 + 8]	= src0[clamp(uv0 + int2(8, 4), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 0]	= src0[clamp(uv0 + int2(0, 8), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 8]	= src0[clamp(uv0 + int2(8, 8), int2(0, 0), max0)];

			temp_convolve1[i + 0 * 24 + 0]	= src1[clamp(uv1 + int2(0, 0), int2(0, 0), max1)];
			temp_convolve1[i + 0 * 24 + 8]	= src1[clamp(uv1 + int2(8, 0), int2(0, 0), max1)];
			temp_convolve1[i + 4 * 24 + 0]	= src1[clamp(uv1 + int2(0, 4), int2(0, 0), max1)];
			temp_convolve1[i + 4 * 24 + 8]	= src1[clamp(uv1 + int2(8, 4), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 0]	= src1[clamp(uv1 + int2(0, 8), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 8]	= src1[clamp(uv1 + int2(8, 8), int2(0, 0), max1)];
			break;
		}
		case 3: {
			mvoffset += ((threadid.x/4) + (threadid.y/4) * 2) * 2;
			mv0		= unpack_mv(mvbuffer[mvoffset + 0]) * 2;
			mv1		= unpack_mv(mvbuffer[mvoffset + 1]) * 2;
			i		= threadid.x + (threadid.x / 4) * (12 - 4) + (threadid.y + (threadid.y / 4) * (12 - 4)) * 24;
			int2 uv0 = pos + mv0 / SUBPEL_FACTOR;
			int2 uv1 = pos + mv1 / SUBPEL_FACTOR;
			temp_convolve0[i + 0 * 24 + 0]	= src0[clamp(uv0 + int2(-3, -3), int2(0, 0), max0)];
			temp_convolve0[i + 0 * 24 + 4]	= src0[clamp(uv0 + int2(+1, -3), int2(0, 0), max0)];
			temp_convolve0[i + 0 * 24 + 8]	= src0[clamp(uv0 + int2(+5, -3), int2(0, 0), max0)];
			temp_convolve0[i + 4 * 24 + 0]	= src0[clamp(uv0 + int2(-3, +1), int2(0, 0), max0)];
			temp_convolve0[i + 4 * 24 + 4]	= src0[clamp(uv0 + int2(+1, +1), int2(0, 0), max0)];
			temp_convolve0[i + 4 * 24 + 8]	= src0[clamp(uv0 + int2(+5, +1), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 0]	= src0[clamp(uv0 + int2(-3, +5), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 4]	= src0[clamp(uv0 + int2(+1, +5), int2(0, 0), max0)];
			temp_convolve0[i + 8 * 24 + 8]	= src0[clamp(uv0 + int2(+5, +5), int2(0, 0), max0)];

			temp_convolve1[i + 0 * 24 + 0]	= src1[clamp(uv1 + int2(-3, -3), int2(0, 0), max1)];
			temp_convolve1[i + 0 * 24 + 4]	= src1[clamp(uv1 + int2(+1, -3), int2(0, 0), max1)];
			temp_convolve1[i + 0 * 24 + 8]	= src1[clamp(uv1 + int2(+5, -3), int2(0, 0), max1)];
			temp_convolve1[i + 4 * 24 + 0]	= src1[clamp(uv1 + int2(-3, +1), int2(0, 0), max1)];
			temp_convolve1[i + 4 * 24 + 4]	= src1[clamp(uv1 + int2(+1, +1), int2(0, 0), max1)];
			temp_convolve1[i + 4 * 24 + 8]	= src1[clamp(uv1 + int2(+5, +1), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 0]	= src1[clamp(uv1 + int2(-3, +5), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 4]	= src1[clamp(uv1 + int2(+1, +5), int2(0, 0), max1)];
			temp_convolve1[i + 8 * 24 + 8]	= src1[clamp(uv1 + int2(+5, +5), int2(0, 0), max1)];
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

	return round_pow2(sum0 + sum1, FILTER_BITS * 2 + 1);
}


COMPUTE_SHADER(8,8,1)
void inter_y(uint2 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID, INTERbuffers buffers : S_SRT_DATA) {
	uint	u		= buffers.info[groupid.y * buffers.width + groupid.x];
	uint	ref0	= (u >> 2) & 3;

	if (ref0) {
		uint	filter	= u & 3;
		uint	ref1	= (u >> 4) & 3;
		uint	split	= (u >> 6) & 3;
		uint	mvoffset= u >> 8;

		uint2	pos		= groupid * 8 + threadid;
//		buffers.textures->dest[pos] = buffers.textures->src[0][pos];
//		return;

		uint	texel	= ref1
			? inter_y2(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1], buffers.textures->src[ref1 - 1])
			: inter_y1(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1]);

		RWTexture2D<int>	dest = buffers.textures->dest;
		add_clamp_pixel(dest, pos, texel);
	}
}

int		round_q4(int value)						{ return (value < 0 ? value - 2 : value + 2) / 4; }
int		round_q2(int value)						{ return (value < 0 ? value - 1 : value + 1) / 2; }
int2	average(int2 a, int2 b)					{ return int2(round_q2(a.x + b.x), round_q2(a.y + b.y)); }
int2	average(int2 a, int2 b, int2 c, int2 d)	{ return int2(round_q4(a.x + b.x + c.x + d.x), round_q4(a.y + b.y + c.y + d.y)); }

uint inter_uv1(uint3 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int2 mvoffset, Texture2D<uint> src) {
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

	temp_convolve0[i + 0 * 24 + 0]	= src[clamp(uv + int2(-3, -3), minuv, maxuv)];
	temp_convolve0[i + 0 * 24 + 4]	= src[clamp(uv + int2(+1, -3), minuv, maxuv)];
	temp_convolve0[i + 0 * 24 + 8]	= src[clamp(uv + int2(+5, -3), minuv, maxuv)];

	temp_convolve0[i + 4 * 24 + 0]	= src[clamp(uv + int2(-3, +1), minuv, maxuv)];
	temp_convolve0[i + 4 * 24 + 4]	= src[clamp(uv + int2(+1, +1), minuv, maxuv)];
	temp_convolve0[i + 4 * 24 + 8]	= src[clamp(uv + int2(+5, +1), minuv, maxuv)];

	temp_convolve0[i + 8 * 24 + 0]	= src[clamp(uv + int2(-3, +5), minuv, maxuv)];
	temp_convolve0[i + 8 * 24 + 4]	= src[clamp(uv + int2(+1, +5), minuv, maxuv)];
	temp_convolve0[i + 8 * 24 + 8]	= src[clamp(uv + int2(+5, +5), minuv, maxuv)];

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
	return round_pow2(sum, FILTER_BITS * 2);
}

uint inter_uv2(uint3 threadid, uint2 pos, uint split, uint filter, Buffer<uint> mvbuffer, int2 mvoffset, Texture2D<uint> src0, Texture2D<uint> src1) {
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

	temp_convolve0[i + 0 * 24 + 0]	= src0[clamp(uv0 + int2(-3, -3), min0, max0)];
	temp_convolve0[i + 0 * 24 + 4]	= src0[clamp(uv0 + int2(+1, -3), min0, max0)];
	temp_convolve0[i + 0 * 24 + 8]	= src0[clamp(uv0 + int2(+5, -3), min0, max0)];

	temp_convolve0[i + 4 * 24 + 0]	= src0[clamp(uv0 + int2(-3, +1), min0, max0)];
	temp_convolve0[i + 4 * 24 + 4]	= src0[clamp(uv0 + int2(+1, +1), min0, max0)];
	temp_convolve0[i + 4 * 24 + 8]	= src0[clamp(uv0 + int2(+5, +1), min0, max0)];

	temp_convolve0[i + 8 * 24 + 0]	= src0[clamp(uv0 + int2(-3, +5), min0, max0)];
	temp_convolve0[i + 8 * 24 + 4]	= src0[clamp(uv0 + int2(+1, +5), min0, max0)];
	temp_convolve0[i + 8 * 24 + 8]	= src0[clamp(uv0 + int2(+5, +5), min0, max0)];

	temp_convolve1[i + 0 * 24 + 0]	= src1[clamp(uv1 + int2(-3, -3), min1, max1)];
	temp_convolve1[i + 0 * 24 + 4]	= src1[clamp(uv1 + int2(+1, -3), min1, max1)];
	temp_convolve1[i + 0 * 24 + 8]	= src1[clamp(uv1 + int2(+5, -3), min1, max1)];

	temp_convolve1[i + 4 * 24 + 0]	= src1[clamp(uv1 + int2(-3, +1), min1, max1)];
	temp_convolve1[i + 4 * 24 + 4]	= src1[clamp(uv1 + int2(+1, +1), min1, max1)];
	temp_convolve1[i + 4 * 24 + 8]	= src1[clamp(uv1 + int2(+5, +1), min1, max1)];

	temp_convolve1[i + 8 * 24 + 0]	= src1[clamp(uv1 + int2(-3, +5), min1, max1)];
	temp_convolve1[i + 8 * 24 + 4]	= src1[clamp(uv1 + int2(+1, +5), min1, max1)];
	temp_convolve1[i + 8 * 24 + 8]	= src1[clamp(uv1 + int2(+5, +5), min1, max1)];

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

	return round_pow2(sum0 + sum1, FILTER_BITS * 2 + 1);
}


COMPUTE_SHADER(4,4,2)
void inter_uv(uint3 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID, INTERbuffers buffers : S_SRT_DATA) {
	uint	u		= buffers.info[groupid.y * buffers.width + groupid.x];
	uint	ref0	= (u >> 2) & 3;

	if (ref0) {
		uint	filter	= u & 3;
		uint	ref1	= (u >> 4) & 3;
		uint	split	= (u >> 6) & 3;
		uint	mvoffset= u >> 8;

		RWTexture2D<int>	dest = buffers.textures->dest;
		uint2				size;
		dest.GetDimensionsFast(size.x, size.y);

		uint2	pos		= groupid * 4 + threadid.xy + int2(threadid.z * size.x / 2, size.y * 2 / 3);
//		dest[pos] = buffers.textures->src[0][pos];
//		return;

		uint	texel	= ref1
			? inter_uv2(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1], buffers.textures->src[ref1 - 1])
			: inter_uv1(threadid, pos, split, filter, buffers.mv, mvoffset, buffers.textures->src[ref0 - 1]);

		add_clamp_pixel(dest, pos, texel);
	}
}

technique inter_predictions {
	pass y	{ OrbisComputeShader	= inter_y; }
	pass uv { OrbisComputeShader	= inter_uv; }
};


//-----------------------------------------------------------------------------
//	YUV CONVERSION
//-----------------------------------------------------------------------------

struct YUVbuffers {
	RWTexture2D<float4>	dest;
	Texture2D<uint>		srce;
};

static const float3x3 yuv_matrix = {
	1.164f,  1.164f, 1.164f,
	  0.0f, -0.392f, 2.017f,
	1.596f, -0.813f,   0.0f,
};

float3 yuv2rgb(float3 yuv) {
	return mul(yuv, yuv_matrix);
}

COMPUTE_SHADER(8,8,1)
void yuv_conversion(uint2 threadid : GROUP_THREAD_ID, uint2 groupid : GROUP_ID, YUVbuffers buffers : S_SRT_DATA) {
	uint2	size;
	buffers.dest.GetDimensionsFast(size.x, size.y);
	
	buffers.dest[groupid * 8 + threadid] = float4(yuv2rgb(int3(
		buffers.srce[groupid * 8 + threadid] - 16,
		buffers.srce[groupid * 4 + threadid / 2 + int2(0, size.y)] - 128,
		buffers.srce[groupid * 4 + threadid / 2 + int2(size.x / 2, size.y)] - 128
	) / 255.0), 1);
}

technique yuv_conversion {
	pass p0	{ OrbisComputeShader	= yuv_conversion; }
};