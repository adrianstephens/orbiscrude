#include "common.fxh"

//-----------------------------------------------------------------------------
//	DCT consts
//-----------------------------------------------------------------------------

#if DCT_CONST_BITS==0

typedef float	T;
typedef float4	T4;
#define DCT_FIXED(x)		x
float	dct_round(float x)	{ return x; }
float4	dct_round(float4 x)	{ return x; }

#else

//typedef int	T;
//typedef int4	T4;
#define T	int
#define T4	int4

#define DCT_FIXED(x)		int((x) * (1<<DCT_CONST_BITS) + 0.5)
int		dct_round(int x)	{ return round_pow2(x, DCT_CONST_BITS); }
int4	dct_round(int4 x)	{ return round_pow2(x, DCT_CONST_BITS); }

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
