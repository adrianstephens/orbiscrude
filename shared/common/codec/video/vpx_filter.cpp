#include "vpx_decode.h"

using namespace vp9;

force_inline uint8	clip_pixel(int val)							{ return clamp(val, 0, 255); }
//inline void			clip_pixel_add(uint8 &dest, int trans)	{ dest = clip_pixel(trans); }//clamp(dest + trans, 0, 255); }
inline void			clip_pixel_add(uint8 &dest, int trans)		{ dest = clamp(dest + trans, 0, 255); }
force_inline int8	signed_char_clamp(int t)					{ return (int8)clamp(t, -128, 127); }


//-------------------------------------
//	filters
//-------------------------------------

// Lagrangian interpolation filter
DECLARE_ALIGNED(256, static const ScaleFactors::kernel, sub_pel_filters_8[]) = {
	{ 0,   0,   0, 128,   0,   0,   0,  0},
	{ 0,   1,  -5, 126,   8,  -3,   1,  0},
	{-1,   3, -10, 122,  18,  -6,   2,  0},
	{-1,   4, -13, 118,  27,  -9,   3, -1},
	{-1,   4, -16, 112,  37, -11,   4, -1},
	{-1,   5, -18, 105,  48, -14,   4, -1},
	{-1,   5, -19,  97,  58, -16,   5, -1},
	{-1,   6, -19,  88,  68, -18,   5, -1},
	{-1,   6, -19,  78,  78, -19,   6, -1},
	{-1,   5, -18,  68,  88, -19,   6, -1},
	{-1,   5, -16,  58,  97, -19,   5, -1},
	{-1,   4, -14,  48, 105, -18,   5, -1},
	{-1,   4, -11,  37, 112, -16,   4, -1},
	{-1,   3,  -9,  27, 118, -13,   4, -1},
	{ 0,   2,  -6,  18, 122, -10,   3, -1},
	{ 0,   1,  -3,   8, 126,  -5,   1,  0}
};

// freqmultiplier = 0.5
DECLARE_ALIGNED(256, static const ScaleFactors::kernel, sub_pel_filters_8lp[]) = {
	{ 0,   0,   0, 128,   0,   0,   0,  0},
	{-3,  -1,  32,  64,  38,   1,  -3,  0},
	{-2,  -2,  29,  63,  41,   2,  -3,  0},
	{-2,  -2,  26,  63,  43,   4,  -4,  0},
	{-2,  -3,  24,  62,  46,   5,  -4,  0},
	{-2,  -3,  21,  60,  49,   7,  -4,  0},
	{-1,  -4,  18,  59,  51,   9,  -4,  0},
	{-1,  -4,  16,  57,  53,  12,  -4, -1},
	{-1,  -4,  14,  55,  55,  14,  -4, -1},
	{-1,  -4,  12,  53,  57,  16,  -4, -1},
	{ 0,  -4,   9,  51,  59,  18,  -4, -1},
	{ 0,  -4,   7,  49,  60,  21,  -3, -2},
	{ 0,  -4,   5,  46,  62,  24,  -3, -2},
	{ 0,  -4,   4,  43,  63,  26,  -2, -2},
	{ 0,  -3,   2,  41,  63,  29,  -2, -2},
	{ 0,  -3,   1,  38,  64,  32,  -1, -3}
};

// DCT based filter
DECLARE_ALIGNED(256, static const ScaleFactors::kernel, sub_pel_filters_8s[]) = {
	{ 0,   0,   0, 128,   0,   0,   0,  0},
	{-1,   3,  -7, 127,   8,  -3,   1,  0},
	{-2,   5, -13, 125,  17,  -6,   3, -1},
	{-3,   7, -17, 121,  27, -10,   5, -2},
	{-4,   9, -20, 115,  37, -13,   6, -2},
	{-4,  10, -23, 108,  48, -16,   8, -3},
	{-4,  10, -24, 100,  59, -19,   9, -3},
	{-4,  11, -24,  90,  70, -21,  10, -4},
	{-4,  11, -23,  80,  80, -23,  11, -4},
	{-4,  10, -21,  70,  90, -24,  11, -4},
	{-3,   9, -19,  59, 100, -24,  10, -4},
	{-3,   8, -16,  48, 108, -23,  10, -4},
	{-2,   6, -13,  37, 115, -20,   9, -4},
	{-2,   5, -10,  27, 121, -17,   7, -3},
	{-1,   3,  -6,  17, 125, -13,   5, -2},
	{0,   1,  -3,    8, 127,  -7,   3, -1}
};

DECLARE_ALIGNED(256, static const ScaleFactors::kernel, bilinear_filters[]) = {
	{ 0,   0,   0, 128,   0,   0,   0,  0},
	{ 0,   0,   0, 120,   8,   0,   0,  0},
	{ 0,   0,   0, 112,  16,   0,   0,  0},
	{ 0,   0,   0, 104,  24,   0,   0,  0},
	{ 0,   0,   0,  96,  32,   0,   0,  0},
	{ 0,   0,   0,  88,  40,   0,   0,  0},
	{ 0,   0,   0,  80,  48,   0,   0,  0},
	{ 0,   0,   0,  72,  56,   0,   0,  0},
	{ 0,   0,   0,  64,  64,   0,   0,  0},
	{ 0,   0,   0,  56,  72,   0,   0,  0},
	{ 0,   0,   0,  48,  80,   0,   0,  0},
	{ 0,   0,   0,  40,  88,   0,   0,  0},
	{ 0,   0,   0,  32,  96,   0,   0,  0},
	{ 0,   0,   0,  24, 104,   0,   0,  0},
	{ 0,   0,   0,  16, 112,   0,   0,  0},
	{ 0,   0,   0,   8, 120,   0,   0,  0}
};

const ScaleFactors::kernel *filter_kernels[4] = {
	sub_pel_filters_8,		// INTERP_8TAP
	sub_pel_filters_8lp,	// INTERP_8TAP_SMOOTH
	sub_pel_filters_8s,		// INTERP_8TAP_SHARP
	bilinear_filters		// INTERP_BILINEAR
};

//-------------------------------------
//	predictor
//-------------------------------------
#define DST(x, y) dst[(x) + (y) * stride]
template<typename T> force_inline T avg2(T a, T b)			{ return (a + b + 1) >> 1; }
template<typename T> force_inline T avg3(T a, T b, T c)		{ return (a + 2 * b + c + 2) >> 2; }
/*template<typename T> force_inline int sum(const T *p, int n)	{
	int	t = 0;
	for (const T *e = p + n; p != e; ++p)
		t += *p;
	return t;
}
*/
template<PREDICTION_MODE M, TX_SIZE S> struct predictor;

template<TX_SIZE S> struct predictor<D207_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		// first column
		for (int r = 0; r < bs - 1; ++r)
			dst[r * stride] = avg2(left[r], left[r + 1]);
		dst[(bs - 1) * stride] = left[bs - 1];

		// second column
		for (int r = 0; r < bs - 2; ++r)
			dst[r * stride + 1] = avg3(left[r], left[r + 1], left[r + 2]);
		dst[(bs - 2) * stride + 1] = avg3(left[bs - 2], left[bs - 1], left[bs - 1]);
		dst[(bs - 1) * stride + 1] = left[bs - 1];

		// rest of last row
		for (int c = 2; c < bs; ++c)
			dst[(bs - 1) * stride + c] = left[bs - 1];

		for (int r = bs - 2; r >= 0; --r)
			for (int c = 2; c < bs; ++c)
				dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
	}
};

template<TX_SIZE S> struct predictor<D63_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		for (int c = 0; c < bs; ++c) {
			dst[c]			= avg2(above[c], above[c + 1]);
			dst[stride + c] = avg3(above[c], above[c + 1], above[c + 2]);
		}
		for (int r = 2, size = bs - 2; r < bs; r += 2, --size) {
			memcpy(dst + (r + 0) * stride, dst + (r >> 1), size);
			memset(dst + (r + 0) * stride + size, above[bs - 1], bs - size);
			memcpy(dst + (r + 1) * stride, dst + stride + (r >> 1), size);
			memset(dst + (r + 1) * stride + size, above[bs - 1], bs - size);
		}
	}
};

template<TX_SIZE S> struct predictor<D45_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const uint8 above_right = above[bs - 1];
		const uint8 *const dst_row0 = dst;
		for (int x = 0; x < bs - 1; ++x)
			dst[x] = avg3(above[x], above[x + 1], above[x + 2]);
		dst[bs - 1] = above_right;
		for (int x = 1, size = bs - 2; x < bs; ++x, --size) {
			dst += stride;
			memcpy(dst, dst_row0 + x, size);
			memset(dst + size, above_right, x + 1);
		}
	}
};

template<TX_SIZE S> struct predictor<D117_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		// first row
		for (int c = 0; c < bs; c++)
			dst[c] = avg2(above[c - 1], above[c]);
		dst += stride;

		// second row
		dst[0] = avg3(left[0], above[-1], above[0]);
		for (int c = 1; c < bs; c++)
			dst[c] = avg3(above[c - 2], above[c - 1], above[c]);
		dst += stride;

		// the rest of first col
		dst[0] = avg3(above[-1], left[0], left[1]);
		for (int r = 3; r < bs; ++r)
			dst[(r - 2) * stride] = avg3(left[r - 3], left[r - 2], left[r - 1]);

		// the rest of the block
		for (int r = 2; r < bs; ++r) {
			for (int c = 1; c < bs; c++)
				dst[c] = dst[-2 * stride + c - 1];
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<D135_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		dst[0] = avg3(left[0], above[-1], above[0]);
		for (int c = 1; c < bs; c++)
			dst[c] = avg3(above[c - 2], above[c - 1], above[c]);

		dst[stride] = avg3(above[-1], left[0], left[1]);
		for (int r = 2; r < bs; ++r)
			dst[r * stride] = avg3(left[r - 2], left[r - 1], left[r]);

		for (int r = 1; r < bs; ++r) {
			dst += stride;
			for (int c = 1; c < bs; c++)
				dst[c] = dst[-stride + c - 1];
		}
	}
};

template<TX_SIZE S> struct predictor<D153_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		//1st col
		dst[0] = avg2(above[-1], left[0]);
		for (int r = 1; r < bs; r++)
			dst[r * stride] = avg2(left[r - 1], left[r]);

		//2nd col
		dst[1] = avg3(left[0], above[-1], above[0]);
		dst[stride + 1] = avg3(above[-1], left[0], left[1]);
		for (int r = 2; r < bs; r++)
			dst[r * stride + 1] = avg3(left[r - 2], left[r - 1], left[r]);

		//1st row
		for (int c = 2; c < bs; c++)
			dst[c] = avg3(above[c - 3], above[c - 2], above[c - 1]);

		for (int r = 1; r < bs; ++r) {
			dst += stride;
			for (int c = 2; c < bs; c++)
				dst[c] = dst[-stride + c - 2];
		}
	}
};

template<TX_SIZE S> struct predictor<V_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		for (int r = 0; r < bs; r++) {
			memcpy(dst, above, bs);
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<H_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		for (int r = 0; r < bs; r++) {
			memset(dst, left[r], bs);
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<TM_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		int ytop_left = above[-1];
		for (int r = 0; r < bs; r++) {
			for (int c = 0; c < bs; c++)
				dst[c] = clip_pixel(left[r] + above[c] - ytop_left);
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<DC_128, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		for (int r = 0; r < bs; r++) {
			memset(dst, 128, bs);
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<DC_NO_A, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		int	dc = (sum<bs>(left) + (bs >> 1)) / bs;
		for (int r = 0; r < bs; r++) {
			memset(dst, dc, bs);
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<DC_NO_L, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		int	dc = (sum<bs>(above) + (bs >> 1)) / bs;
		for (int r = 0; r < bs; r++) {
			memset(dst, dc, bs);
			dst += stride;
		}
	}
};

template<TX_SIZE S> struct predictor<DC_PRED, S> {
	enum {bs = 4<<S};
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		int	dc = (sum<bs>(above) + sum<bs>(left) + bs) / (bs << 1);
		for (int r = 0; r < bs; r++) {
			memset(dst, dc, bs);
			dst += stride;
		}
	}
};

template<> struct predictor<D207_PRED,TX_4X4> {
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const int I = left[0];
		const int J = left[1];
		const int K = left[2];
		const int L = left[3];
		DST(0, 0) = avg2(I, J);
		DST(2, 0) = DST(0, 1) = avg2(J, K);
		DST(2, 1) = DST(0, 2) = avg2(K, L);
		DST(1, 0) = avg3(I, J, K);
		DST(3, 0) = DST(1, 1) = avg3(J, K, L);
		DST(3, 1) = DST(1, 2) = avg3(K, L, L);
		DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
	}
};

template<> struct predictor<D63_PRED,TX_4X4> {
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const int A = above[0];
		const int B = above[1];
		const int C = above[2];
		const int D = above[3];
		const int E = above[4];
		const int F = above[5];
		const int G = above[6];
		DST(0, 0) = avg2(A, B);
		DST(1, 0) = DST(0, 2) = avg2(B, C);
		DST(2, 0) = DST(1, 2) = avg2(C, D);
		DST(3, 0) = DST(2, 2) = avg2(D, E);
		DST(3, 2) = avg2(E, F);		// differs from vp8
		DST(0, 1) = avg3(A, B, C);
		DST(1, 1) = DST(0, 3) = avg3(B, C, D);
		DST(2, 1) = DST(1, 3) = avg3(C, D, E);
		DST(3, 1) = DST(2, 3) = avg3(D, E, F);
		DST(3, 3) = avg3(E, F, G);	// differs from vp8
	}
};

template<> struct predictor<D45_PRED,TX_4X4> {
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const int A = above[0];
		const int B = above[1];
		const int C = above[2];
		const int D = above[3];
		const int E = above[4];
		const int F = above[5];
		const int G = above[6];
		const int H = above[7];
		DST(0, 0) = avg3(A, B, C);
		DST(1, 0) = DST(0, 1) = avg3(B, C, D);
		DST(2, 0) = DST(1, 1) = DST(0, 2) = avg3(C, D, E);
		DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = avg3(D, E, F);
		DST(3, 1) = DST(2, 2) = DST(1, 3) = avg3(E, F, G);
		DST(3, 2) = DST(2, 3) = avg3(F, G, H);
		DST(3, 3) = H;				// differs from vp8
	}
};

template<> struct predictor<D117_PRED,TX_4X4> {
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const int I = left[0];
		const int J = left[1];
		const int K = left[2];
		const int X = above[-1];
		const int A = above[0];
		const int B = above[1];
		const int C = above[2];
		const int D = above[3];
		DST(0, 0) = DST(1, 2) = avg2(X, A);
		DST(1, 0) = DST(2, 2) = avg2(A, B);
		DST(2, 0) = DST(3, 2) = avg2(B, C);
		DST(3, 0) = avg2(C, D);
		DST(0, 3) = avg3(K, J, I);
		DST(0, 2) = avg3(J, I, X);
		DST(0, 1) = DST(1, 3) = avg3(I, X, A);
		DST(1, 1) = DST(2, 3) = avg3(X, A, B);
		DST(2, 1) = DST(3, 3) = avg3(A, B, C);
		DST(3, 1) = avg3(B, C, D);
	}
};

template<> struct predictor<D135_PRED,TX_4X4> {
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const int I = left[0];
		const int J = left[1];
		const int K = left[2];
		const int L = left[3];
		const int X = above[-1];
		const int A = above[0];
		const int B = above[1];
		const int C = above[2];
		const int D = above[3];
		DST(0, 3) = avg3(J, K, L);
		DST(1, 3) = DST(0, 2) = avg3(I, J, K);
		DST(2, 3) = DST(1, 2) = DST(0, 1) = avg3(X, I, J);
		DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = avg3(A, X, I);
		DST(3, 2) = DST(2, 1) = DST(1, 0) = avg3(B, A, X);
		DST(3, 1) = DST(2, 0) = avg3(C, B, A);
		DST(3, 0) = avg3(D, C, B);
	}
};

template<> struct predictor<D153_PRED,TX_4X4> {
	template<typename T> static void f(T *dst, ptrdiff_t stride, const T *above, const T *left) {
		const int I = left[0];
		const int J = left[1];
		const int K = left[2];
		const int L = left[3];
		const int X = above[-1];
		const int A = above[0];
		const int B = above[1];
		const int C = above[2];
		DST(0, 0) = DST(2, 1) = avg2(I, X);
		DST(0, 1) = DST(2, 2) = avg2(J, I);
		DST(0, 2) = DST(2, 3) = avg2(K, J);
		DST(0, 3) = avg2(L, K);
		DST(3, 0) = avg3(A, B, C);
		DST(2, 0) = avg3(X, A, B);
		DST(1, 0) = DST(3, 1) = avg3(I, X, A);
		DST(1, 1) = DST(3, 2) = avg3(J, I, X);
		DST(1, 2) = DST(3, 3) = avg3(K, J, I);
		DST(1, 3) = avg3(L, K, J);
	}
};

template<typename T> void pred(PREDICTION_MODE M, TX_SIZE S, T *dst, ptrdiff_t stride, const T *above, const T *left) {
	typedef void(*intra_pred_fn)(T *dst, ptrdiff_t stride, const T *above, const T *left);
	static intra_pred_fn table[][TX_SIZES]{
		{	predictor<DC_PRED,TX_4X4>::f,		predictor<DC_PRED,TX_8X8>::f,		predictor<DC_PRED,TX_16X16>::f,		predictor<DC_PRED,TX_32X32>::f		},
		{	predictor<V_PRED,TX_4X4>::f,		predictor<V_PRED,TX_8X8>::f,		predictor<V_PRED,TX_16X16>::f,		predictor<V_PRED,TX_32X32>::f		},
		{	predictor<H_PRED,TX_4X4>::f,		predictor<H_PRED,TX_8X8>::f,		predictor<H_PRED,TX_16X16>::f,		predictor<H_PRED,TX_32X32>::f		},
		{	predictor<D45_PRED,TX_4X4>::f,		predictor<D45_PRED,TX_8X8>::f,		predictor<D45_PRED,TX_16X16>::f,	predictor<D45_PRED,TX_32X32>::f		},
		{	predictor<D135_PRED,TX_4X4>::f,		predictor<D135_PRED,TX_8X8>::f,		predictor<D135_PRED,TX_16X16>::f,	predictor<D135_PRED,TX_32X32>::f	},
		{	predictor<D117_PRED,TX_4X4>::f,		predictor<D117_PRED,TX_8X8>::f,		predictor<D117_PRED,TX_16X16>::f,	predictor<D117_PRED,TX_32X32>::f	},
		{	predictor<D153_PRED,TX_4X4>::f,		predictor<D153_PRED,TX_8X8>::f,		predictor<D153_PRED,TX_16X16>::f,	predictor<D153_PRED,TX_32X32>::f	},
		{	predictor<D207_PRED,TX_4X4>::f,		predictor<D207_PRED,TX_8X8>::f,		predictor<D207_PRED,TX_16X16>::f,	predictor<D207_PRED,TX_32X32>::f	},
		{	predictor<D63_PRED,TX_4X4>::f,		predictor<D63_PRED,TX_8X8>::f,		predictor<D63_PRED,TX_16X16>::f,	predictor<D63_PRED,TX_32X32>::f		},
		{	predictor<TM_PRED,TX_4X4>::f,		predictor<TM_PRED,TX_8X8>::f,		predictor<TM_PRED,TX_16X16>::f,		predictor<TM_PRED,TX_32X32>::f		},

		{	predictor<DC_NO_L,TX_4X4>::f,		predictor<DC_NO_L,TX_8X8>::f,		predictor<DC_NO_L,TX_16X16>::f,		predictor<DC_NO_L,TX_32X32>::f		},
		{	predictor<DC_NO_A,TX_4X4>::f,		predictor<DC_NO_A,TX_8X8>::f,		predictor<DC_NO_A,TX_16X16>::f,		predictor<DC_NO_A,TX_32X32>::f		},
		{	predictor<DC_128,TX_4X4>::f,		predictor<DC_128,TX_8X8>::f,		predictor<DC_128,TX_16X16>::f,		predictor<DC_128,TX_32X32>::f		},
	};
	table[M][S](dst, stride, above, left);
}

template void pred(PREDICTION_MODE M, TX_SIZE S, uint8 *dst, ptrdiff_t stride, const uint8 *above, const uint8 *left);

//-------------------------------------
//	LoopFilterMasks
//-------------------------------------
#if 0
uint32	total_filter;
uint32	level_total;
uint32	level_count;
#define FILTER_LEVEL(n)		level_total += n; ++level_count;
#define FILTERED(n)			total_filter += n
#else
#define FILTER_LEVEL(n)
#define FILTERED(n)
#endif

// This function sets up the bit masks for a block represented by mi_row, mi_col in a 64x64 region.
// TODO(SJL): This function only works for yv12.
void LoopFilterMasks::build_mask(const int filter_level, const ModeInfo *mi, int row_in_sb, int col_in_sb, int bw, int bh) {
	static const uint64 prediction_mask[BLOCK_SIZES] = {
		0x0000000000000001ULL,  // BLOCK_4X4
		0x0000000000000001ULL,  // BLOCK_4X8
		0x0000000000000001ULL,  // BLOCK_8X4
		0x0000000000000001ULL,  // BLOCK_8X8
		0x0000000000000101ULL,  // BLOCK_8X16,
		0x0000000000000003ULL,  // BLOCK_16X8
		0x0000000000000303ULL,  // BLOCK_16X16
		0x0000000003030303ULL,  // BLOCK_16X32,
		0x0000000000000f0fULL,  // BLOCK_32X16,
		0x000000000f0f0f0fULL,  // BLOCK_32X32,
		0x0f0f0f0f0f0f0f0fULL,  // BLOCK_32X64,
		0x00000000ffffffffULL,  // BLOCK_64X32,
		0xffffffffffffffffULL,  // BLOCK_64X64
	};
	static const uint16 prediction_mask_uv[BLOCK_SIZES] = {
		0x0001,					// BLOCK_4X4
		0x0001,					// BLOCK_4X8
		0x0001,					// BLOCK_8X4
		0x0001,					// BLOCK_8X8
		0x0001,					// BLOCK_8X16,
		0x0001,					// BLOCK_16X8
		0x0001,					// BLOCK_16X16
		0x0011,					// BLOCK_16X32,
		0x0003,					// BLOCK_32X16,
		0x0033,					// BLOCK_32X32,
		0x3333,					// BLOCK_32X64,
		0x00ff,					// BLOCK_64X32,
		0xffff,					// BLOCK_64X64
	};

	FILTER_LEVEL(filter_level);

	// A LoopFilter should be applied to every other 8x8 horizontally.
	static const struct txform_mask {
		uint64 left_64x64,		above_64x64;	uint16		left_64x64_uv, above_64x64_uv;
	} txform_masks[TX_SIZES] = {
		{0xffffffffffffffffULL,  0xffffffffffffffffULL,		0xffff,			0xffff},	// TX_4X4
		{0xffffffffffffffffULL,  0xffffffffffffffffULL,		0xffff,			0xffff},	// TX_8x8
		{0x5555555555555555ULL,  0x00ff00ff00ff00ffULL,		0x5555,			0x0f0f},	// TX_16x16
		{0x1111111111111111ULL,  0x000000ff000000ffULL,		0x1111,			0x000f},	// TX_32x32
	};

	if (!filter_level)
		return;

	const BLOCK_SIZE	block_size	= mi->sb_type;
	const TX_SIZE		tx_size_y	= mi->tx_size;
	const TX_SIZE		tx_size_uv	= mi->get_uv_tx_size(1, 1);

	const int			shift_y		= col_in_sb + (row_in_sb << 3);
	const int			shift_uv	= (col_in_sb >> 1) + ((row_in_sb >> 1) << 2);
	const bool			build_uv	= ((row_in_sb | col_in_sb) & 1) == 0;

	for (int i = shift_y; bh--; i += 8)
		memset(&level_y[i], filter_level, bw);

	// These set 1 in the current block size for the block size edges.
	// For instance if the block size is 32x16, we'll set:
	//    above =   1111
	//              0000
	//    and
	//    left  =   1000
	//          =   1000
	// NOTE : In this example the low bit is left most ( 1000 ) is stored as 1,  not 8...
	// U and V set things on a 16 bit scale.

	uint64	mask	= prediction_mask[block_size];
	uint16	mask_uv	= prediction_mask_uv[block_size];

	above_y[tx_size_y]	|= (mask & 0x00000000000000ffULL) << shift_y;
	left_y[tx_size_y]	|= (mask & 0x0101010101010101ULL) << shift_y;

	if (build_uv) {
		above_uv[tx_size_uv]	|= (mask_uv & 0x000f) << shift_uv;
		left_uv[tx_size_uv]		|= (mask_uv & 0x1111) << shift_uv;
	}

	// If the block has no coefficients and is not intra we skip applying the loop filter on block edges.
	if (!mi->skip || !mi->is_inter_block()) {
		// Add a mask for the transform size
		// The transform size mask is set to be correct for a 64x64 prediction block size. Mask to match the size of the block we are working on and then shift it into place.
		above_y[tx_size_y]	|= (mask & txform_masks[tx_size_y].above_64x64) << shift_y;
		left_y[tx_size_y]	|= (mask & txform_masks[tx_size_y].left_64x64) << shift_y;

		if (build_uv) {
			above_uv[tx_size_uv]	|= (mask_uv & txform_masks[tx_size_uv].above_64x64_uv) << shift_uv;
			left_uv[tx_size_uv]		|= (mask_uv & txform_masks[tx_size_uv].left_64x64_uv) << shift_uv;
		}

		// Try to determine what to do with the internal 4x4 block boundaries
		// These differ from the 4x4 boundaries on the outside edge of an 8x8 in that the internal ones can be skipped and don't depend on the prediction block size.
		if (tx_size_y == TX_4X4)
			int_4x4_y	|= mask << shift_y;

		if (build_uv && tx_size_uv == TX_4X4)
			int_4x4_uv	|= mask_uv << shift_uv;
	}
}

void LoopFilterMasks::adjust_mask(const int mi_row, const int mi_col, const int mi_rows, const int mi_cols) {
	// The largest LoopFilter we have is 16x16 so we use the 16x16 mask for 32x32 transforms also
	left_y[TX_16X16]	|= left_y[TX_32X32];
	above_y[TX_16X16]	|= above_y[TX_32X32];
	left_uv[TX_16X16]	|= left_uv[TX_32X32];
	above_uv[TX_16X16]	|= above_uv[TX_32X32];

	// We do at least 8 tap filter on every 32x32 even if the transform size is 4x4; so if the 4x4 is set on a border pixel add it to the 8x8 and remove it from the 4x4
	static const uint64 left_border		= 0x1111111111111111ULL;
	static const uint64 above_border	= 0x000000ff000000ffULL;
	static const uint16 left_border_uv	= 0x1111;
	static const uint16 above_border_uv = 0x000f;

	left_y[TX_8X8]		|= left_y[TX_4X4] & left_border;
	left_y[TX_4X4]		&= ~left_border;
	above_y[TX_8X8]		|= above_y[TX_4X4] & above_border;
	above_y[TX_4X4]		&= ~above_border;
	left_uv[TX_8X8]		|= left_uv[TX_4X4] & left_border_uv;
	left_uv[TX_4X4]		&= ~left_border_uv;
	above_uv[TX_8X8]	|= above_uv[TX_4X4] & above_border_uv;
	above_uv[TX_4X4]	&= ~above_border_uv;

	// We do some special edge handling.
	if (mi_row + MI_BLOCK_SIZE > mi_rows) {
		const uint64 rows		= mi_rows - mi_row;

		// Each pixel inside the border gets a 1,
		const uint64 mask_y		= (((uint64)1 << (rows << 3)) - 1);
		const uint16 mask_uv	= (((uint16)1 << (((rows + 1) >> 1) << 2)) - 1);

		// Remove values completely outside our border.
		for (int i = 0; i < TX_32X32; i++) {
			left_y[i]	&= mask_y;
			above_y[i]	&= mask_y;
			left_uv[i]	&= mask_uv;
			above_uv[i] &= mask_uv;
		}
		int_4x4_y	&= mask_y;
		int_4x4_uv	&= mask_uv;

		// We don't apply a wide loop filter on the last uv block row; if set apply the shorter one instead
		if (rows == 1) {
			above_uv[TX_8X8]	|= above_uv[TX_16X16];
			above_uv[TX_16X16]	= 0;
		} else if (rows == 5) {
			above_uv[TX_8X8]	|= above_uv[TX_16X16] & 0xff00;
			above_uv[TX_16X16]	&= ~(above_uv[TX_16X16] & 0xff00);
		}
	}

	if (mi_col + MI_BLOCK_SIZE > mi_cols) {
		const uint64 columns	= mi_cols - mi_col;

		// Each pixel inside the border gets a 1, the multiply copies the border to where we need it
		const uint64 mask_y		= (((1 << columns) - 1)) * 0x0101010101010101ULL;
		const uint16 mask_uv	= ((1 << ((columns + 1) >> 1)) - 1) * 0x1111;

		// Internal edges are not applied on the last column of the image so we mask 1 more for the internal edges
		const uint16 mask_uv_int = ((1 << (columns >> 1)) - 1) * 0x1111;

		// Remove the bits outside the image edge.
		for (int i = 0; i < TX_32X32; i++) {
			left_y[i]	&= mask_y;
			above_y[i]	&= mask_y;
			left_uv[i]	&= mask_uv;
			above_uv[i] &= mask_uv;
		}
		int_4x4_y	&= mask_y;
		int_4x4_uv	&= mask_uv_int;

		// We don't apply a wide loop filter on the last uv column; if set apply the shorter one instead
		if (columns == 1) {
			left_uv[TX_8X8]		|= left_uv[TX_16X16];
			left_uv[TX_16X16]	= 0;
		} else if (columns == 5) {
			left_uv[TX_8X8]		|= (left_uv[TX_16X16] & 0xcccc);
			left_uv[TX_16X16]	&= ~(left_uv[TX_16X16] & 0xcccc);
		}
	}
	// We don't apply a loop filter on the first column in the image, mask that out.
	if (mi_col == 0) {
		for (int i = 0; i < TX_32X32; i++) {
			left_y[i]	&= 0xfefefefefefefefeULL;
			left_uv[i]	&= 0xeeee;
		}
	}
}

// should we apply any filter at all: 11111111 yes, 00000000 no
force_inline int8 filter_mask(uint8 limit, uint8 blimit,
	uint8 p3, uint8 p2, uint8 p1, uint8 p0,
	uint8 q0, uint8 q1, uint8 q2, uint8 q3
) {
	int8 mask = 0;
	mask |= (abs(p3 - p2) > limit) * -1;
	mask |= (abs(p2 - p1) > limit) * -1;
	mask |= (abs(p1 - p0) > limit) * -1;
	mask |= (abs(q1 - q0) > limit) * -1;
	mask |= (abs(q2 - q1) > limit) * -1;
	mask |= (abs(q3 - q2) > limit) * -1;
	mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit) * -1;
	return ~mask;
}

force_inline int8 flat_mask4(uint8 thresh,
	uint8 p3, uint8 p2, uint8 p1, uint8 p0,
	uint8 q0, uint8 q1, uint8 q2, uint8 q3
) {
	int8 mask = 0;
	mask |= (abs(p1 - p0) > thresh) * -1;
	mask |= (abs(q1 - q0) > thresh) * -1;
	mask |= (abs(p2 - p0) > thresh) * -1;
	mask |= (abs(q2 - q0) > thresh) * -1;
	mask |= (abs(p3 - p0) > thresh) * -1;
	mask |= (abs(q3 - q0) > thresh) * -1;
	return ~mask;
}

force_inline int8 flat_mask5(uint8 thresh,
	uint8 p4, uint8 p3,
	uint8 p2, uint8 p1,
	uint8 p0, uint8 q0,
	uint8 q1, uint8 q2,
	uint8 q3, uint8 q4
) {
	int8 mask = ~flat_mask4(thresh, p3, p2, p1, p0, q0, q1, q2, q3);
	mask |= (abs(p4 - p0) > thresh) * -1;
	mask |= (abs(q4 - q0) > thresh) * -1;
	return ~mask;
}

force_inline void filter4(int8 mask, uint8 thresh, uint8 *op1, uint8 *op0, uint8 *oq0, uint8 *oq1) {
	const int8	ps1 = (int8)*op1 ^ 0x80;
	const int8	ps0 = (int8)*op0 ^ 0x80;
	const int8	qs0 = (int8)*oq0 ^ 0x80;
	const int8	qs1 = (int8)*oq1 ^ 0x80;
	const uint8 hev = abs(*op1 - *op0) > thresh || abs(*oq1 - *oq0) > thresh ? 0xff : 0;

	// add outer taps if we have high edge variance
	int8 filter = signed_char_clamp(ps1 - qs1) & hev;

	// inner taps
	filter = signed_char_clamp(filter + 3 * (qs0 - ps0)) & mask;

	// save bottom 3 bits so that we round one side +4 and the other +3
	// if it equals 4 we'll set to adjust by -1 to account for the fact we'd round 3 the other way
	int8	filter1 = signed_char_clamp(filter + 4) >> 3;
	int8	filter2 = signed_char_clamp(filter + 3) >> 3;

	*oq0 = signed_char_clamp(qs0 - filter1) ^ 0x80;
	*op0 = signed_char_clamp(ps0 + filter2) ^ 0x80;

	// outer tap adjustments
	filter = round_pow2(filter1, 1) & ~hev;

	*oq1 = signed_char_clamp(qs1 - filter) ^ 0x80;
	*op1 = signed_char_clamp(ps1 + filter) ^ 0x80;

	FILTERED(4);
}

force_inline void lpf_horizontal_4(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh, int count) {
	// loop filter designed to work using chars so that we can make maximum use of 8 bit simd instructions.
	for (int i = 0; i < 8 * count; ++i) {
		const uint8 p3	= s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
		const uint8 q0	= s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p];
		const int8	mask = filter_mask(thresh.limit[0], thresh.blimit[0], p3, p2, p1, p0, q0, q1, q2, q3);
		filter4(mask, thresh.hev_thr[0], s - 2 * p, s - 1 * p, s, s + 1 * p);
		++s;
	}
}

force_inline void lpf_horizontal_4_dual(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh0, const LoopFilterInfo::Thresh &thresh1) {
	lpf_horizontal_4(s, p, thresh0, 1);
	lpf_horizontal_4(s + 8, p, thresh1, 1);
}

force_inline void lpf_vertical_4(uint8 *s, int stride, const LoopFilterInfo::Thresh &thresh, int count) {
	// loop filter designed to work using chars so that we can make maximum use of 8 bit simd instructions.
	for (int i = 0; i < 8 * count; ++i) {
		const uint8 p3	= s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
		const uint8 q0	= s[0], q1 = s[1], q2 = s[2], q3 = s[3];
		const int8	mask = filter_mask(thresh.limit[0], thresh.blimit[0], p3, p2, p1, p0, q0, q1, q2, q3);
		filter4(mask, thresh.hev_thr[0], s - 2, s - 1, s, s + 1);
		s += stride;
	}
}

force_inline void lpf_vertical_4_dual(uint8 *s, int stride, const LoopFilterInfo::Thresh &thresh0, const LoopFilterInfo::Thresh &thresh1) {
	lpf_vertical_4(s, stride, thresh0, 1);
	lpf_vertical_4(s + 8 * stride, stride, thresh1, 1);
}

force_inline void filter8(int8 mask, uint8 thresh, uint8 flat,
	uint8 *op3, uint8 *op2,
	uint8 *op1, uint8 *op0,
	uint8 *oq0, uint8 *oq1,
	uint8 *oq2, uint8 *oq3
) {
	if (flat && mask) {
		const uint8 p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
		const uint8 q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3;

		// 7-tap filter [1, 1, 1, 2, 1, 1, 1]
		*op2 = round_pow2(p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0, 3);
		*op1 = round_pow2(p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1, 3);
		*op0 = round_pow2(p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2, 3);
		*oq0 = round_pow2(p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3, 3);
		*oq1 = round_pow2(p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3, 3);
		*oq2 = round_pow2(p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3, 3);
		FILTERED(6);
	} else {
		filter4(mask, thresh, op1, op0, oq0, oq1);
	}
}

force_inline void lpf_horizontal_8(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh, int count) {
	// loop filter designed to work using chars so that we can make maximum use of 8 bit simd instructions.
	for (int i = 0; i < 8 * count; ++i) {
		const uint8 p3	= s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
		const uint8 q0	= s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p];
		const int8 mask	= filter_mask(thresh.limit[0], thresh.blimit[0], p3, p2, p1, p0, q0, q1, q2, q3);
		const int8 flat	= flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
		filter8(mask, thresh.hev_thr[0], flat, s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p, s + 2 * p, s + 3 * p);
		++s;
	}
}

force_inline void lpf_horizontal_8_dual(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh0, const LoopFilterInfo::Thresh &thresh1) {
	lpf_horizontal_8(s, p, thresh0, 1);
	lpf_horizontal_8(s + 8, p, thresh1, 1);
}

force_inline void lpf_vertical_8(uint8 *s, int stride, const LoopFilterInfo::Thresh &thresh, int count) {
	for (int i = 0; i < 8 * count; ++i) {
		const uint8 p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
		const uint8 q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
		const int8 mask = filter_mask(thresh.limit[0], thresh.blimit[0], p3, p2, p1, p0, q0, q1, q2, q3);
		const int8 flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
		filter8(mask, thresh.hev_thr[0], flat, s - 4, s - 3, s - 2, s - 1, s, s + 1, s + 2, s + 3);
		s += stride;
	}
}

force_inline void lpf_vertical_8_dual(uint8 *s, int stride, const LoopFilterInfo::Thresh &thresh0, const LoopFilterInfo::Thresh &thresh1) {
	lpf_vertical_8(s, stride, thresh0, 1);
	lpf_vertical_8(s + 8 * stride, stride, thresh1, 1);
}

force_inline void filter16(int8 mask, uint8 thresh,
	uint8 flat, uint8 flat2,
	uint8 *op7, uint8 *op6, uint8 *op5, uint8 *op4, uint8 *op3, uint8 *op2, uint8 *op1, uint8 *op0,
	uint8 *oq0, uint8 *oq1, uint8 *oq2, uint8 *oq3, uint8 *oq4, uint8 *oq5, uint8 *oq6, uint8 *oq7
) {
	if (flat2 && flat && mask) {
		const uint8 p7 = *op7, p6 = *op6, p5 = *op5, p4 = *op4, p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
		const uint8 q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3, q4 = *oq4, q5 = *oq5, q6 = *oq6, q7 = *oq7;
		// 15-tap filter [1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1]
		*op6 = round_pow2(p7 * 7 + p6 * 2 + p5 + p4 + p3 + p2 + p1 + p0 + q0, 4);
		*op5 = round_pow2(p7 * 6 + p6 + p5 * 2 + p4 + p3 + p2 + p1 + p0 + q0 + q1, 4);
		*op4 = round_pow2(p7 * 5 + p6 + p5 + p4 * 2 + p3 + p2 + p1 + p0 + q0 + q1 + q2, 4);
		*op3 = round_pow2(p7 * 4 + p6 + p5 + p4 + p3 * 2 + p2 + p1 + p0 + q0 + q1 + q2 + q3, 4);
		*op2 = round_pow2(p7 * 3 + p6 + p5 + p4 + p3 + p2 * 2 + p1 + p0 + q0 + q1 + q2 + q3 + q4, 4);
		*op1 = round_pow2(p7 * 2 + p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 + q0 + q1 + q2 + q3 + q4 + q5, 4);
		*op0 = round_pow2(p7 + p6 + p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 + q1 + q2 + q3 + q4 + q5 + q6, 4);
		*oq0 = round_pow2(p6 + p5 + p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 + q2 + q3 + q4 + q5 + q6 + q7, 4);
		*oq1 = round_pow2(p5 + p4 + p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 + q3 + q4 + q5 + q6 + q7 * 2, 4);
		*oq2 = round_pow2(p4 + p3 + p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 + q4 + q5 + q6 + q7 * 3, 4);
		*oq3 = round_pow2(p3 + p2 + p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 + q5 + q6 + q7 * 4, 4);
		*oq4 = round_pow2(p2 + p1 + p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 + q6 + q7 * 5, 4);
		*oq5 = round_pow2(p1 + p0 + q0 + q1 + q2 + q3 + q4 + q5 * 2 + q6 + q7 * 6, 4);
		*oq6 = round_pow2(p0 + q0 + q1 + q2 + q3 + q4 + q5 + q6 * 2 + q7 * 7, 4);
		FILTERED(14);
	} else {
		filter8(mask, thresh, flat, op3, op2, op1, op0, oq0, oq1, oq2, oq3);
	}
}

force_inline void lpf_horizontal_16(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh, int count) {
	// loop filter designed to work using chars so that we can make maximum use of 8 bit simd instructions.
	for (int i = 0; i < 8 * count; ++i) {
		const uint8 p3 = s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
		const uint8 q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p];
		const int8 mask = filter_mask(thresh.limit[0], thresh.blimit[0], p3, p2, p1, p0, q0, q1, q2, q3);
		const int8 flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
		const int8 flat2 = flat_mask5(1, s[-8 * p], s[-7 * p], s[-6 * p], s[-5 * p], p0, q0, s[4 * p], s[5 * p], s[6 * p], s[7 * p]);

		filter16(mask, thresh.hev_thr[0], flat, flat2,
			s - 8 * p, s - 7 * p, s - 6 * p, s - 5 * p,
			s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p,
			s, s + 1 * p, s + 2 * p, s + 3 * p,
			s + 4 * p, s + 5 * p, s + 6 * p, s + 7 * p
		);
		++s;
	}
}

force_inline void mb_lpf_vertical_edge_w(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh, int count) {
	for (int i = 0; i < count; ++i) {
		const uint8 p3		= s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
		const uint8 q0		= s[0], q1 = s[1], q2 = s[2], q3 = s[3];
		const int8	mask	= filter_mask(thresh.limit[0], thresh.blimit[0], p3, p2, p1, p0, q0, q1, q2, q3);
		const int8	flat	= flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
		const int8	flat2	= flat_mask5(1, s[-8], s[-7], s[-6], s[-5], p0, q0, s[4], s[5], s[6], s[7]);

		filter16(mask, thresh.hev_thr[0], flat, flat2,
			s - 8, s - 7, s - 6, s - 5,
			s - 4, s - 3, s - 2, s - 1,
			s, s + 1, s + 2, s + 3, s + 4,
			s + 5, s + 6, s + 7
		);
		s += p;
	}
}

force_inline void lpf_vertical_16(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh) {
	mb_lpf_vertical_edge_w(s, p, thresh, 8);
}

force_inline void lpf_vertical_16_dual(uint8 *s, int p, const LoopFilterInfo::Thresh &thresh) {
	mb_lpf_vertical_edge_w(s, p, thresh, 16);
}

void LoopFilterInfo::filter_selectively_vert_row2(uint8 *s, int stride, int lfl_forward, uint32 mask_16x16, uint32 mask_8x8, uint32 mask_4x4, uint32 mask_4x4_int, const uint8 *level) const {
	for (uint32 mask = (mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int) & 0xff; mask; mask >>= 1) {
		const Thresh &lfi0 = thresh[level[0]];
		const Thresh &lfi1 = thresh[level[lfl_forward]];

		if (mask & 1) {
			if (mask_16x16 & 0x0101) {
				if (!(mask_16x16 & 0x0100))
					lpf_vertical_16(s, stride, lfi0);
				else if (!(mask_16x16 & 0x0001))
					lpf_vertical_16(s + 8 * stride, stride, lfi1);
				else
					lpf_vertical_16_dual(s, stride, lfi0);
			}

			if (mask_8x8 & 0x0101) {
				if (!(mask_8x8 & 0x0100))
					lpf_vertical_8(s, stride, lfi0, 1);
				else if (!(mask_8x8 & 0x0001))
					lpf_vertical_8(s + 8 * stride, stride, lfi1, 1);
				else
					lpf_vertical_8_dual(s, stride, lfi0, lfi1);
			}

			if (mask_4x4 & 0x0101) {
				if (!(mask_4x4 & 0x0100))
					lpf_vertical_4(s, stride, lfi0, 1);
				else if (!(mask_4x4 & 0x0001))
					lpf_vertical_4(s + 8 * stride, stride, lfi1, 1);
				else
					lpf_vertical_4_dual(s, stride, lfi0, lfi1);
			}

			if (mask_4x4_int & 0x0101) {
				if (!(mask_4x4_int & 0x0100))
					lpf_vertical_4(s + 4, stride, lfi0, 1);
				else if (!(mask_4x4_int & 0x0001))
					lpf_vertical_4(s + 8 * stride + 4, stride, lfi1, 1);
				else
					lpf_vertical_4_dual(s + 4, stride, lfi0, lfi1);
			}
		}

		s				+= 8;
		level			+= 1;
		mask_16x16		>>= 1;
		mask_8x8		>>= 1;
		mask_4x4		>>= 1;
		mask_4x4_int	>>= 1;
	}
}

void LoopFilterInfo::filter_selectively_horiz(uint8 *s, int stride, uint32 mask_16x16, uint32 mask_8x8, uint32 mask_4x4, uint32 mask_4x4_int, const uint8 *level) const {
	for (uint32 mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask; ) {
		const Thresh &lfi = thresh[*level];
		int		count = 1;
		if (mask & 1) {
			if (mask_16x16 & 1) {
				if ((mask_16x16 & 3) == 3) {
					lpf_horizontal_16(s, stride, lfi, 2);
					count = 2;
				} else {
					lpf_horizontal_16(s, stride, lfi, 1);
				}
			} else if (mask_8x8 & 1) {
				if ((mask_8x8 & 3) == 3) {
					// Next block's thresholds.
					const Thresh &lfin = thresh[level[1]];

					lpf_horizontal_8_dual(s, stride, lfi, lfin);

					if ((mask_4x4_int & 3) == 3) {
						lpf_horizontal_4_dual(s + 4 * stride, stride, lfi, lfin);
					} else {
						if (mask_4x4_int & 1)
							lpf_horizontal_4(s + 4 * stride, stride, lfi, 1);
						else if (mask_4x4_int & 2)
							lpf_horizontal_4(s + 8 + 4 * stride, stride, lfin, 1);
					}
					count = 2;
				} else {
					lpf_horizontal_8(s, stride, lfi, 1);

					if (mask_4x4_int & 1)
						lpf_horizontal_4(s + 4 * stride, stride, lfi, 1);
				}
			} else if (mask_4x4 & 1) {
				if ((mask_4x4 & 3) == 3) {
					// Next block's thresholds.
					const Thresh &lfin = thresh[level[1]];

					lpf_horizontal_4_dual(s, stride, lfi, lfin);
					if ((mask_4x4_int & 3) == 3) {
						lpf_horizontal_4_dual(s + 4 * stride, stride, lfi, lfin);
					} else {
						if (mask_4x4_int & 1)
							lpf_horizontal_4(s + 4 * stride, stride, lfi, 1);
						else if (mask_4x4_int & 2)
							lpf_horizontal_4(s + 8 + 4 * stride, stride, lfin, 1);
					}
					count = 2;
				} else {
					lpf_horizontal_4(s, stride, lfi, 1);

					if (mask_4x4_int & 1)
						lpf_horizontal_4(s + 4 * stride, stride, lfi, 1);
				}
			} else if (mask_4x4_int & 1) {
				lpf_horizontal_4(s + 4 * stride, stride, lfi, 1);
			}
		}
		s				+= 8 * count;
		level			+= count;
		mask			>>= count;
		mask_16x16		>>= count;
		mask_8x8		>>= count;
		mask_4x4		>>= count;
		mask_4x4_int	>>= count;
	}
}

void LoopFilterInfo::filter_selectively_vert(uint8 *s, int stride, uint32 mask_16x16, uint32 mask_8x8, uint32 mask_4x4, uint32 mask_4x4_int, const uint8 *level) const {
	for (uint32 mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask; mask >>= 1) {
		const Thresh &lfi = thresh[*level++];

		if (mask & 1) {
			if (mask_16x16 & 1)
				lpf_vertical_16(s, stride, lfi);
			else if (mask_8x8 & 1)
				lpf_vertical_8(s, stride, lfi, 1);
			else if (mask_4x4 & 1)
				lpf_vertical_4(s, stride, lfi, 1);
		}
		if (mask_4x4_int & 1)
			lpf_vertical_4(s + 4, stride, lfi, 1);

		s				+= 8;
		mask_16x16		>>= 1;
		mask_8x8		>>= 1;
		mask_4x4		>>= 1;
		mask_4x4_int	>>= 1;
	}
}

void LoopFilterInfo::filter_block_plane_non420(const Buffer2D dst, int ssx, int ssy, ModeInfo **mi_8x8, int mi_row, int mi_col, const int mi_rows, const int mi_cols, const int mi_stride) const {
	// Log 2 conversion lookup tables for block width and height
	static const uint8 num_4x4_blocks_wide_lookup[BLOCK_SIZES] = { 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16 };
	static const uint8 num_4x4_blocks_high_lookup[BLOCK_SIZES] = { 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16 };
	// Log 2 conversion lookup tables for modeinfo width and height
	static const uint8 num_8x8_blocks_wide_lookup[BLOCK_SIZES] = { 1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8 };
	static const uint8 num_8x8_blocks_high_lookup[BLOCK_SIZES] = { 1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8 };

	const int row_step			= 1 << ssy;
	const int col_step			= 1 << ssx;
	const int row_step_stride	= mi_stride * row_step;

	uint32 mask_16x16[MI_BLOCK_SIZE]	= { 0 };
	uint32 mask_8x8[MI_BLOCK_SIZE]		= { 0 };
	uint32 mask_4x4[MI_BLOCK_SIZE]		= { 0 };
	uint32 mask_4x4_int[MI_BLOCK_SIZE]	= { 0 };
	uint8 level[MI_BLOCK_SIZE * MI_BLOCK_SIZE];

	uint8	*dst1	= dst.buffer;
	int		stride	= dst.stride;
	for (int r = 0; r < MI_BLOCK_SIZE && mi_row + r < mi_rows; r += row_step) {
		uint32 mask_16x16_c = 0;
		uint32 mask_8x8_c	= 0;
		uint32 mask_4x4_c	= 0;

		// Determine the vertical edges that need filtering
		for (int c = 0; c < MI_BLOCK_SIZE && mi_col + c < mi_cols; c += col_step) {
			const ModeInfo		*mi		= mi_8x8[c];
			int					cs		= c >> ssx, cb = 1 << cs;

			// Filter level can vary per MI
			if (!(level[(r << 3) + cs] = get_filter_level(mi)))
				continue;

			const BLOCK_SIZE	sb_type		= mi[0].sb_type;
			const bool			skip_this	= mi[0].skip && mi->is_inter_block();
			// left edge of current unit is block/partition edge -> no skip
			const bool			skip_this_c	= skip_this && (num_4x4_blocks_wide_lookup[sb_type] > 1) && (c & (num_8x8_blocks_wide_lookup[sb_type] - 1));
			// top edge of current unit is block/partition edge -> no skip
			const bool			skip_this_r = skip_this && (num_4x4_blocks_high_lookup[sb_type] > 1) && (r & (num_8x8_blocks_high_lookup[sb_type] - 1));
			const TX_SIZE		tx_size		= mi->get_uv_tx_size(ssx, ssy);
			const bool			skip_border_4x4_c = ssx && mi_col + c == mi_cols - 1;
			const bool			skip_border_4x4_r = ssy && mi_row + r == mi_rows - 1;

			// Build masks based on the transform size of each block
			if (tx_size == TX_32X32) {
				if (!skip_this_c && (cs & 3) == 0) {
					if (!skip_border_4x4_c)
						mask_16x16_c |= cb;
					else
						mask_8x8_c |= cb;
				}
				if (!skip_this_r && ((r >> ssy) & 3) == 0) {
					if (!skip_border_4x4_r)
						mask_16x16[r] |= cb;
					else
						mask_8x8[r] |= cb;
				}
			} else if (tx_size == TX_16X16) {
				if (!skip_this_c && (cs & 1) == 0) {
					if (!skip_border_4x4_c)
						mask_16x16_c |= cb;
					else
						mask_8x8_c |= cb;
				}
				if (!skip_this_r && ((r >> ssy) & 1) == 0) {
					if (!skip_border_4x4_r)
						mask_16x16[r] |= cb;
					else
						mask_8x8[r] |= cb;
				}
			} else {
				// force 8x8 filtering on 32x32 boundaries
				if (!skip_this_c) {
					if (tx_size == TX_8X8 || (cs & 3) == 0)
						mask_8x8_c |= cb;
					else
						mask_4x4_c |= cb;
				}
				if (!skip_this_r) {
					if (tx_size == TX_8X8 || ((r >> ssy) & 3) == 0)
						mask_8x8[r] |= cb;
					else
						mask_4x4[r] |= cb;
				}
				if (!skip_this && tx_size < TX_8X8 && !skip_border_4x4_c)
					mask_4x4_int[r] |= cb;
			}
		}

		// Disable filtering on the leftmost column
		uint32 border_mask = ~uint32(mi_col == 0);
		filter_selectively_vert(dst1, stride, mask_16x16_c & border_mask, mask_8x8_c & border_mask, mask_4x4_c & border_mask, mask_4x4_int[r], &level[r << 3]);
		dst1		+= 8 * stride;
		mi_8x8		+= row_step_stride;
	}

	// Now do horizontal pass
	dst1 = dst.buffer;
	for (int r = 0; r < MI_BLOCK_SIZE && mi_row + r < mi_rows; r += row_step) {
		const uint32 mask_4x4_int_r = ssy && mi_row + r == mi_rows - 1 ? 0 : mask_4x4_int[r];

		if (mi_row + r == 0) {
			filter_selectively_horiz(
				dst1, stride,
				0, 0, 0,
				mask_4x4_int_r,
				&level[r << 3]
			);
		} else {
			filter_selectively_horiz(
				dst1, stride,
				mask_16x16[r], mask_8x8[r], mask_4x4[r],
				mask_4x4_int_r,
				&level[r << 3]
			);
		}

		dst1 += 8 * stride;
	}
}

void LoopFilterInfo::filter_block_plane_ss00(const Buffer2D dst, int mi_row, LoopFilterMasks *lfm, const int mi_rows, const int mi_cols, const int mi_stride) const {
	// Vertical pass: do 2 rows at one time
	uint64 mask_16x16	= lfm->left_y[TX_16X16];
	uint64 mask_8x8		= lfm->left_y[TX_8X8];
	uint64 mask_4x4		= lfm->left_y[TX_4X4];
	uint64 mask_4x4_int = lfm->int_4x4_y;
	uint8	*dst1	= dst.buffer;
	int		stride	= dst.stride;

	for (int r = 0; r < MI_BLOCK_SIZE && mi_row + r < mi_rows; r += 2, dst1 += 16 * stride) {
		filter_selectively_vert_row2(dst1, stride, 8, mask_16x16 & 0xffff, mask_8x8 & 0xffff, mask_4x4 & 0xffff, mask_4x4_int & 0xffff, &lfm->level_y[r << 3]);
		mask_16x16		>>= 16;
		mask_8x8		>>= 16;
		mask_4x4		>>= 16;
		mask_4x4_int	>>= 16;
	}

	// Horizontal pass
	mask_16x16		= lfm->above_y[TX_16X16];
	mask_8x8		= lfm->above_y[TX_8X8];
	mask_4x4		= lfm->above_y[TX_4X4];
	mask_4x4_int	= lfm->int_4x4_y;
	dst1			= dst.buffer;

	for (int r = 0; r < MI_BLOCK_SIZE && mi_row + r < mi_rows; r++, dst1 += 8 * stride) {
		const uint32	m	= mi_row + r == 0 ? 0 : 0xff;	// disable filtering on the leftmost column.
		filter_selectively_horiz(dst1, stride, mask_16x16 & m, mask_8x8 & m, mask_4x4 & m, mask_4x4_int & 0xff, &lfm->level_y[r << 3]);
		mask_16x16		>>= 8;
		mask_8x8		>>= 8;
		mask_4x4		>>= 8;
		mask_4x4_int	>>= 8;
	}
}

inline uint32 expand_mask(uint16 m) {
	uint32	t = (m & 0x00ff) | ((m & 0xff00) << 8);
	return (t & 0x000f000f) | ((t & 0x00f000f0) << 4);
}
void LoopFilterInfo::filter_block_plane_ss11(const Buffer2D dst, int mi_row, LoopFilterMasks *lfm, const int mi_rows, const int mi_cols, const int mi_stride) const {
	uint8 lfl_uv[16];

	// Vertical pass: do 2 rows at one time
	uint32 mask_16x16	= expand_mask(lfm->left_uv[TX_16X16]);
	uint32 mask_8x8		= expand_mask(lfm->left_uv[TX_8X8]);
	uint32 mask_4x4		= expand_mask(lfm->left_uv[TX_4X4]);
	uint32 mask_4x4_int = expand_mask(lfm->int_4x4_uv);
	uint8	*dst1	= dst.buffer;
	int		stride	= dst.stride;

	for (int r = 0; r < MI_BLOCK_SIZE && mi_row + r < mi_rows; r += 4, dst1 += 16 * stride) {
		for (int c = 0; c < (MI_BLOCK_SIZE >> 1); c++) {
			lfl_uv[(r << 1) + c]		= lfm->level_y[(r << 3) + (c << 1)];
			lfl_uv[((r + 2) << 1) + c]	= lfm->level_y[((r + 2) << 3) + (c << 1)];
		}
		filter_selectively_vert_row2(dst1, stride, 4, mask_16x16 & 0x0f0f, mask_8x8 & 0x0f0f, mask_4x4 & 0x0f0f, mask_4x4_int & 0x0f0f, &lfl_uv[r << 1]);
		mask_16x16		>>= 16;
		mask_8x8		>>= 16;
		mask_4x4		>>= 16;
		mask_4x4_int	>>= 16;
	}

	// Horizontal pass
	mask_16x16		= lfm->above_uv[TX_16X16];
	mask_8x8		= lfm->above_uv[TX_8X8];
	mask_4x4		= lfm->above_uv[TX_4X4];
	mask_4x4_int	= lfm->int_4x4_uv;
	dst1			= dst.buffer;

	for (int r = 0; r < MI_BLOCK_SIZE && mi_row + r < mi_rows; r += 2, dst1 += 8 * stride) {
		const uint32	mi	= mi_row + r == mi_rows - 1 ? 0 : 0xf;
		const uint32	m	= mi_row + r == 0			? 0 : 0xf;		// disable filtering on the leftmost column.
		filter_selectively_horiz(dst1, stride, mask_16x16 & m, mask_8x8 & m, mask_4x4 & m, mask_4x4_int & mi, &lfl_uv[r << 1]);
		mask_16x16		>>= 4;
		mask_8x8		>>= 4;
		mask_4x4		>>= 4;
		mask_4x4_int	>>= 4;
	}
}

//-----------------------------------------------------------------------------
//	ScaleFactors
//-----------------------------------------------------------------------------

force_inline void convolve_horiz(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int w, int h) {
	src -= ScaleFactors::SUBPEL_TAPS / 2 - 1;
	for (int y = 0; y < h; ++y, src += src_stride, dst += dst_stride) {
		for (int x = 0, x_q4 = x0_q4; x < w; ++x, x_q4 += x_step_q4) {
			const uint8 *const src_x = &src[x_q4 >> ScaleFactors::SUBPEL_BITS];
			const int16 *const x_filter = filters[x_q4 & ScaleFactors::SUBPEL_MASK];
			int	sum = 0;
			for (int k = 0; k < ScaleFactors::SUBPEL_TAPS; ++k)
				sum += src_x[k] * x_filter[k];
			dst[x] = clip_pixel(round_pow2(sum, ScaleFactors::FILTER_BITS));
		}
	}
}

force_inline void convolve_avg_horiz(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int w, int h) {
	src -= ScaleFactors::SUBPEL_TAPS / 2 - 1;
	for (int y = 0; y < h; ++y, src += src_stride, dst += dst_stride) {
		for (int x = 0, x_q4 = x0_q4; x < w; ++x, x_q4 += x_step_q4) {
			const uint8 *const src_x = &src[x_q4 >> ScaleFactors::SUBPEL_BITS];
			const int16 *const x_filter = filters[x_q4 & ScaleFactors::SUBPEL_MASK];
			int sum = 0;
			for (int k = 0; k < ScaleFactors::SUBPEL_TAPS; ++k)
				sum += src_x[k] * x_filter[k];
			dst[x] = round_pow2(dst[x] + clip_pixel(round_pow2(sum, ScaleFactors::FILTER_BITS)), 1);
		}
	}
}

force_inline void convolve_vert(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int y0_q4, int y_step_q4, int w, int h) {
	src -= src_stride * (ScaleFactors::SUBPEL_TAPS / 2 - 1);
	for (int x = 0; x < w; ++x, ++src, ++dst) {
		for (int y = 0, y_q4 = y0_q4; y < h; ++y, y_q4 += y_step_q4) {
			const uint8 *src_y = &src[(y_q4 >> ScaleFactors::SUBPEL_BITS) * src_stride];
			const int16 *const y_filter = filters[y_q4 & ScaleFactors::SUBPEL_MASK];
			int sum = 0;
			for (int k = 0; k < ScaleFactors::SUBPEL_TAPS; ++k)
				sum += src_y[k * src_stride] * y_filter[k];
			dst[y * dst_stride] = clip_pixel(round_pow2(sum, ScaleFactors::FILTER_BITS));
		}
	}
}

force_inline void convolve_avg_vert(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int y0_q4, int y_step_q4, int w, int h) {
	src -= src_stride * (ScaleFactors::SUBPEL_TAPS / 2 - 1);
	for (int x = 0; x < w; ++x, ++src, ++dst) {
		for (int y = 0, y_q4 = y0_q4; y < h; ++y, y_q4 += y_step_q4) {
			const uint8 *src_y = &src[(y_q4 >> ScaleFactors::SUBPEL_BITS) * src_stride];
			const int16 *const y_filter = filters[y_q4 & ScaleFactors::SUBPEL_MASK];
			int sum = 0;
			for (int k = 0; k < ScaleFactors::SUBPEL_TAPS; ++k)
				sum += src_y[k * src_stride] * y_filter[k];
			dst[y * dst_stride] = round_pow2(dst[y * dst_stride] + clip_pixel(round_pow2(sum, ScaleFactors::FILTER_BITS)), 1);
		}
	}
}

force_inline void convolve(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride,
	const ScaleFactors::kernel *const filters,
	int x0_q4, int x_step_q4,
	int y0_q4, int y_step_q4,
	int w, int h
) {
	uint8 temp[135 * 64];
	convolve_horiz(src - src_stride * (ScaleFactors::SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, filters, x0_q4, x_step_q4, w, (((h - 1) * y_step_q4 + y0_q4) >> ScaleFactors::SUBPEL_BITS) + ScaleFactors::SUBPEL_TAPS);
	convolve_vert(temp + 64 * (ScaleFactors::SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filters, y0_q4, y_step_q4, w, h);
}

void convolve_avg(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x)
			dst[x] = round_pow2(dst[x] + src[x], 1);
		src += src_stride;
		dst += dst_stride;
	}
}

force_inline const ScaleFactors::kernel *get_filter_base(const int16 *filter) {
	return (const ScaleFactors::kernel*)(((intptr_t)filter) & ~((intptr_t)0xFF));
}
int get_filter_offset(const int16 *f, const ScaleFactors::kernel *base) {
	return (int)((const ScaleFactors::kernel *)(intptr_t)f - base);
}

force_inline void convolve8_horiz(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve_horiz(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, w, h);
}

force_inline void convolve8_avg_horiz(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve_avg_horiz(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, w, h);
}

force_inline void convolve8_vert(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve_vert(src, src_stride, dst, dst_stride, filters, y0_q4, y_step_q4, w, h);
}

force_inline void convolve8_avg_vert(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve_avg_vert(src, src_stride, dst, dst_stride, filters, y0_q4, y_step_q4, w, h);
}

force_inline void convolve8(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}

force_inline void convolve8_avg(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	DECLARE_ALIGNED(16, uint8, temp[64 * 64]);
	convolve8(src, src_stride, temp, 64, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
	convolve_avg(temp, 64, dst, dst_stride, NULL, 0, 0, 0, 0, w, h);
}

force_inline void convolve_copy(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	for (int r = h; r > 0; --r) {
		memcpy(dst, src, w);
		src += src_stride;
		dst += dst_stride;
	}
}

force_inline void scaled_horiz(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve8_horiz(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}

force_inline void scaled_vert(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve8_vert(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}

force_inline void scaled_2d(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve8(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}

force_inline void scaled_avg_horiz(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve8_avg_horiz(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}

force_inline void scaled_avg_vert(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve8_avg_vert(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}

force_inline void scaled_avg_2d(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *filters, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h) {
	convolve8_avg(src, src_stride, dst, dst_stride, filters, x0_q4, x_step_q4, y0_q4, y_step_q4, w, h);
}


void scaled_2d(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *hfilters, int x_step_q4, const ScaleFactors::kernel *vfilters, int y_step_q4, int w, int h) {
	uint8 temp[135 * 64];
	convolve_horiz(src - src_stride * (ScaleFactors::SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, hfilters, 0, x_step_q4, w, (((h - 1) * y_step_q4) >> ScaleFactors::SUBPEL_BITS) + ScaleFactors::SUBPEL_TAPS);
	convolve_vert(temp + 64 * (ScaleFactors::SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, vfilters, 0, y_step_q4, w, h);
}

void ScaleFactors::init(int other_w, int other_h, int this_w, int this_h) {
	if (!valid_ref_frame_size(other_w, other_h, this_w, this_h)) {
		x_scale_fp = INVALID;
		y_scale_fp = INVALID;
		return;
	}

	x_scale_fp	= get_fixed_point_scale_factor(other_w, this_w);
	y_scale_fp	= get_fixed_point_scale_factor(other_h, this_h);

	int	x_step_q4	= x_scale_fp >> (SHIFT - ScaleFactors::SUBPEL_BITS);
	int	y_step_q4	= y_scale_fp >> (SHIFT - ScaleFactors::SUBPEL_BITS);

	if (x_step_q4 == 16) {
		if (y_step_q4 == 16) {
			// No scaling in either direction.
			predict[0][0][0] = convolve_copy;
			predict[0][0][1] = convolve_avg;
			predict[0][1][0] = convolve8_vert;
			predict[0][1][1] = convolve8_avg_vert;
			predict[1][0][0] = convolve8_horiz;
			predict[1][0][1] = convolve8_avg_horiz;
		} else {
			// No scaling in x direction. Must always scale in the y direction.
			predict[0][0][0] = scaled_vert;
			predict[0][0][1] = scaled_avg_vert;
			predict[0][1][0] = scaled_vert;
			predict[0][1][1] = scaled_avg_vert;
			predict[1][0][0] = scaled_2d;
			predict[1][0][1] = scaled_avg_2d;
		}
	} else {
		if (y_step_q4 == 16) {
			// No scaling in the y direction. Must always scale in the x direction.
			predict[0][0][0] = scaled_horiz;
			predict[0][0][1] = scaled_avg_horiz;
			predict[0][1][0] = scaled_2d;
			predict[0][1][1] = scaled_avg_2d;
			predict[1][0][0] = scaled_horiz;
			predict[1][0][1] = scaled_avg_horiz;
		} else {
			// Must always scale in both directions.
			predict[0][0][0] = scaled_2d;
			predict[0][0][1] = scaled_avg_2d;
			predict[0][1][0] = scaled_2d;
			predict[0][1][1] = scaled_avg_2d;
			predict[1][0][0] = scaled_2d;
			predict[1][0][1] = scaled_avg_2d;
		}
	}

	// 2D subpel motion always gets filtered in both directions
	if (x_step_q4 == 16 && y_step_q4 == 16) {
		predict[1][1][0] = convolve8;
		predict[1][1][1] = convolve8_avg;
	} else {
		predict[1][1][0] = scaled_2d;
		predict[1][1][1] = scaled_avg_2d;
	}
}

//-------------------------------------
//	scaling
//-------------------------------------

#define FILTER_BITS				7

#define INTERP_TAPS				8
#define SUBPEL_BITS				5
#define SUBPEL_MASK				((1 << SUBPEL_BITS) - 1)
#define INTERP_PRECISION_BITS	32

typedef int16 interp_kernel[INTERP_TAPS];

// Filters for interpolation (0.5-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters500[(1 << SUBPEL_BITS)] = {
  {-3,  0, 35, 64, 35,  0, -3, 0},
  {-3, -1, 34, 64, 36,  1, -3, 0},
  {-3, -1, 32, 64, 38,  1, -3, 0},
  {-2, -2, 31, 63, 39,  2, -3, 0},
  {-2, -2, 29, 63, 41,  2, -3, 0},
  {-2, -2, 28, 63, 42,  3, -4, 0},
  {-2, -3, 27, 63, 43,  4, -4, 0},
  {-2, -3, 25, 62, 45,  5, -4, 0},
  {-2, -3, 24, 62, 46,  5, -4, 0},
  {-2, -3, 23, 61, 47,  6, -4, 0},
  {-2, -3, 21, 60, 49,  7, -4, 0},
  {-1, -4, 20, 60, 50,  8, -4, -1},
  {-1, -4, 19, 59, 51,  9, -4, -1},
  {-1, -4, 17, 58, 52, 10, -4, 0},
  {-1, -4, 16, 57, 53, 12, -4, -1},
  {-1, -4, 15, 56, 54, 13, -4, -1},
  {-1, -4, 14, 55, 55, 14, -4, -1},
  {-1, -4, 13, 54, 56, 15, -4, -1},
  {-1, -4, 12, 53, 57, 16, -4, -1},
  {0, -4, 10, 52, 58, 17, -4, -1},
  {-1, -4,  9, 51, 59, 19, -4, -1},
  {-1, -4,  8, 50, 60, 20, -4, -1},
  {0, -4,  7, 49, 60, 21, -3, -2},
  {0, -4,  6, 47, 61, 23, -3, -2},
  {0, -4,  5, 46, 62, 24, -3, -2},
  {0, -4,  5, 45, 62, 25, -3, -2},
  {0, -4,  4, 43, 63, 27, -3, -2},
  {0, -4,  3, 42, 63, 28, -2, -2},
  {0, -3,  2, 41, 63, 29, -2, -2},
  {0, -3,  2, 39, 63, 31, -2, -2},
  {0, -3,  1, 38, 64, 32, -1, -3},
  {0, -3,  1, 36, 64, 34, -1, -3}
};

// Filters for interpolation (0.625-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters625[(1 << SUBPEL_BITS)] = {
  {-1, -8, 33, 80, 33, -8, -1, 0},
  {-1, -8, 30, 80, 35, -8, -1, 1},
  {-1, -8, 28, 80, 37, -7, -2, 1},
  {0, -8, 26, 79, 39, -7, -2, 1},
  {0, -8, 24, 79, 41, -7, -2, 1},
  {0, -8, 22, 78, 43, -6, -2, 1},
  {0, -8, 20, 78, 45, -5, -3, 1},
  {0, -8, 18, 77, 48, -5, -3, 1},
  {0, -8, 16, 76, 50, -4, -3, 1},
  {0, -8, 15, 75, 52, -3, -4, 1},
  {0, -7, 13, 74, 54, -3, -4, 1},
  {0, -7, 11, 73, 56, -2, -4, 1},
  {0, -7, 10, 71, 58, -1, -4, 1},
  {1, -7,  8, 70, 60,  0, -5, 1},
  {1, -6,  6, 68, 62,  1, -5, 1},
  {1, -6,  5, 67, 63,  2, -5, 1},
  {1, -6,  4, 65, 65,  4, -6, 1},
  {1, -5,  2, 63, 67,  5, -6, 1},
  {1, -5,  1, 62, 68,  6, -6, 1},
  {1, -5,  0, 60, 70,  8, -7, 1},
  {1, -4, -1, 58, 71, 10, -7, 0},
  {1, -4, -2, 56, 73, 11, -7, 0},
  {1, -4, -3, 54, 74, 13, -7, 0},
  {1, -4, -3, 52, 75, 15, -8, 0},
  {1, -3, -4, 50, 76, 16, -8, 0},
  {1, -3, -5, 48, 77, 18, -8, 0},
  {1, -3, -5, 45, 78, 20, -8, 0},
  {1, -2, -6, 43, 78, 22, -8, 0},
  {1, -2, -7, 41, 79, 24, -8, 0},
  {1, -2, -7, 39, 79, 26, -8, 0},
  {1, -2, -7, 37, 80, 28, -8, -1},
  {1, -1, -8, 35, 80, 30, -8, -1},
};

// Filters for interpolation (0.75-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters750[(1 << SUBPEL_BITS)] = {
  {2, -11,  25,  96,  25, -11,   2, 0},
  {2, -11,  22,  96,  28, -11,   2, 0},
  {2, -10,  19,  95,  31, -11,   2, 0},
  {2, -10,  17,  95,  34, -12,   2, 0},
  {2,  -9,  14,  94,  37, -12,   2, 0},
  {2,  -8,  12,  93,  40, -12,   1, 0},
  {2,  -8,   9,  92,  43, -12,   1, 1},
  {2,  -7,   7,  91,  46, -12,   1, 0},
  {2,  -7,   5,  90,  49, -12,   1, 0},
  {2,  -6,   3,  88,  52, -12,   0, 1},
  {2,  -5,   1,  86,  55, -12,   0, 1},
  {2,  -5,  -1,  84,  58, -11,   0, 1},
  {2,  -4,  -2,  82,  61, -11,  -1, 1},
  {2,  -4,  -4,  80,  64, -10,  -1, 1},
  {1,  -3,  -5,  77,  67,  -9,  -1, 1},
  {1,  -3,  -6,  75,  70,  -8,  -2, 1},
  {1,  -2,  -7,  72,  72,  -7,  -2, 1},
  {1,  -2,  -8,  70,  75,  -6,  -3, 1},
  {1,  -1,  -9,  67,  77,  -5,  -3, 1},
  {1,  -1, -10,  64,  80,  -4,  -4, 2},
  {1,  -1, -11,  61,  82,  -2,  -4, 2},
  {1,   0, -11,  58,  84,  -1,  -5, 2},
  {1,   0, -12,  55,  86,   1,  -5, 2},
  {1,   0, -12,  52,  88,   3,  -6, 2},
  {0,   1, -12,  49,  90,   5,  -7, 2},
  {0,   1, -12,  46,  91,   7,  -7, 2},
  {1,   1, -12,  43,  92,   9,  -8, 2},
  {0,   1, -12,  40,  93,  12,  -8, 2},
  {0,   2, -12,  37,  94,  14,  -9, 2},
  {0,   2, -12,  34,  95,  17, -10, 2},
  {0,   2, -11,  31,  95,  19, -10, 2},
  {0,   2, -11,  28,  96,  22, -11, 2}
};

// Filters for interpolation (0.875-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters875[(1 << SUBPEL_BITS)] = {
  { 3,  -8,  13, 112,  13,  -8,   3, 0},
  { 3,  -7,  10, 112,  17,  -9,   3, -1},
  { 2,  -6,   7, 111,  21,  -9,   3, -1},
  { 2,  -5,   4, 111,  24, -10,   3, -1},
  { 2,  -4,   1, 110,  28, -11,   3, -1},
  { 1,  -3,  -1, 108,  32, -12,   4, -1},
  { 1,  -2,  -3, 106,  36, -13,   4, -1},
  { 1,  -1,  -6, 105,  40, -14,   4, -1},
  { 1,  -1,  -7, 102,  44, -14,   4, -1},
  { 1,   0,  -9, 100,  48, -15,   4, -1},
  { 1,   1, -11,  97,  53, -16,   4, -1},
  { 0,   1, -12,  95,  57, -16,   4, -1},
  { 0,   2, -13,  91,  61, -16,   4, -1},
  { 0,   2, -14,  88,  65, -16,   4, -1},
  { 0,   3, -15,  84,  69, -17,   4,  0},
  { 0,   3, -16,  81,  73, -16,   3,  0},
  { 0,   3, -16,  77,  77, -16,   3,  0},
  { 0,   3, -16,  73,  81, -16,   3,  0},
  { 0,   4, -17,  69,  84, -15,   3,  0},
  {-1,   4, -16,  65,  88, -14,   2,  0},
  {-1,   4, -16,  61,  91, -13,   2,  0},
  {-1,   4, -16,  57,  95, -12,   1,  0},
  {-1,   4, -16,  53,  97, -11,   1,  1},
  {-1,   4, -15,  48, 100,  -9,   0,  1},
  {-1,   4, -14,  44, 102,  -7,  -1,  1},
  {-1,   4, -14,  40, 105,  -6,  -1,  1},
  {-1,   4, -13,  36, 106,  -3,  -2,  1},
  {-1,   4, -12,  32, 108,  -1,  -3,  1},
  {-1,   3, -11,  28, 110,   1,  -4,  2},
  {-1,   3, -10,  24, 111,   4,  -5,  2},
  {-1,   3,  -9,  21, 111,   7,  -6,  2},
  {-1,   3,  -9,  17, 112,  10,  -7,  3}
};

// Filters for interpolation (full-band) - no filtering for integer pixels
static const interp_kernel filteredinterp_filters1000[(1 << SUBPEL_BITS)] = {
  { 0,   0,   0, 128,   0,   0,   0,  0},
  { 0,   1,  -3, 128,   3,  -1,   0,  0},
  {-1,   2,  -6, 127,   7,  -2,   1,  0},
  {-1,   3,  -9, 126,  12,  -4,   1,  0},
  {-1,   4, -12, 125,  16,  -5,   1,  0},
  {-1,   4, -14, 123,  20,  -6,   2,  0},
  {-1,   5, -15, 120,  25,  -8,   2,  0},
  {-1,   5, -17, 118,  30,  -9,   3, -1},
  {-1,   6, -18, 114,  35, -10,   3, -1},
  {-1,   6, -19, 111,  41, -12,   3, -1},
  {-1,   6, -20, 107,  46, -13,   4, -1},
  {-1,   6, -21, 103,  52, -14,   4, -1},
  {-1,   6, -21,  99,  57, -16,   5, -1},
  {-1,   6, -21,  94,  63, -17,   5, -1},
  {-1,   6, -20,  89,  68, -18,   5, -1},
  {-1,   6, -20,  84,  73, -19,   6, -1},
  {-1,   6, -20,  79,  79, -20,   6, -1},
  {-1,   6, -19,  73,  84, -20,   6, -1},
  {-1,   5, -18,  68,  89, -20,   6, -1},
  {-1,   5, -17,  63,  94, -21,   6, -1},
  {-1,   5, -16,  57,  99, -21,   6, -1},
  {-1,   4, -14,  52, 103, -21,   6, -1},
  {-1,   4, -13,  46, 107, -20,   6, -1},
  {-1,   3, -12,  41, 111, -19,   6, -1},
  {-1,   3, -10,  35, 114, -18,   6, -1},
  {-1,   3,  -9,  30, 118, -17,   5, -1},
  { 0,   2,  -8,  25, 120, -15,   5, -1},
  { 0,   2,  -6,  20, 123, -14,   4, -1},
  { 0,   1,  -5,  16, 125, -12,   4, -1},
  { 0,   1,  -4,  12, 126,  -9,   3, -1},
  { 0,   1,  -2,   7, 127,  -6,   2, -1},
  { 0,   0,  -1,   3, 128,  -3,   1,  0}
};

static const interp_kernel *choose_interp_filter(int inlength, int outlength) {
	int outlength16 = outlength * 16;
	return	outlength16 >= inlength * 16 ? filteredinterp_filters1000
		:	outlength16 >= inlength * 13 ? filteredinterp_filters875
		:	outlength16 >= inlength * 11 ? filteredinterp_filters750
		:	outlength16 >= inlength * 9	 ? filteredinterp_filters625
		:	filteredinterp_filters500;
}

static void interpolate(const uint8 *const input, int inlength, uint8 *output, int outlength) {
	const ptrdiff_t delta = (((ptrdiff_t)inlength << 32) + outlength / 2) / outlength;
	const ptrdiff_t offset = inlength > outlength
		? (((ptrdiff_t)(inlength - outlength) << 31) + outlength / 2) / outlength
		: -(((ptrdiff_t)(outlength - inlength) << 31) + outlength / 2) / outlength;
	uint8 *optr = output;

	const interp_kernel *interp_filters = choose_interp_filter(inlength, outlength);

	int	x = 0;
	ptrdiff_t	y = offset;
	while ((y >> INTERP_PRECISION_BITS) < (INTERP_TAPS / 2 - 1)) {
		x++;
		y += delta;
	}

	int	x1 = x;
	x = outlength - 1;
	y = delta * x + offset;
	while ((y >> INTERP_PRECISION_BITS) +
		(ptrdiff_t)(INTERP_TAPS / 2) >= inlength) {
		x--;
		y -= delta;
	}

	int	x2 = x;
	if (x1 > x2) {
		for (x = 0, y = offset; x < outlength; ++x, y += delta) {
			int	int_pel	= y >> INTERP_PRECISION_BITS;
			int	sub_pel = (y >> (INTERP_PRECISION_BITS - SUBPEL_BITS)) & SUBPEL_MASK;
			const int16 *filter	= interp_filters[sub_pel];
			int	sum = 0;
			for (int k = 0; k < INTERP_TAPS; ++k) {
				const int pk = int_pel - INTERP_TAPS / 2 + 1 + k;
				sum += filter[k] * input[(pk < 0 ? 0 : (pk >= inlength ? inlength - 1 : pk))];
			}
			*optr++ = clip_pixel(round_pow2(sum, FILTER_BITS));
		}
	} else {
		// Initial part.
		for (x = 0, y = offset; x < x1; ++x, y += delta) {
			int	int_pel = y >> INTERP_PRECISION_BITS;
			int	sub_pel = (y >> (INTERP_PRECISION_BITS - SUBPEL_BITS)) & SUBPEL_MASK;
			const int16 *filter = interp_filters[sub_pel];
			int	sum = 0;
			for (int k = 0; k < INTERP_TAPS; ++k)
				sum += filter[k] * input[(int_pel - INTERP_TAPS / 2 + 1 + k < 0 ? 0 : int_pel - INTERP_TAPS / 2 + 1 + k)];
			*optr++ = clip_pixel(round_pow2(sum, FILTER_BITS));
		}
		// Middle part.
		for (; x <= x2; ++x, y += delta) {
			int	int_pel = y >> INTERP_PRECISION_BITS;
			int	sub_pel = (y >> (INTERP_PRECISION_BITS - SUBPEL_BITS)) & SUBPEL_MASK;
			const int16 *filter = interp_filters[sub_pel];
			int	sum = 0;
			for (int k = 0; k < INTERP_TAPS; ++k)
				sum += filter[k] * input[int_pel - INTERP_TAPS / 2 + 1 + k];
			*optr++ = clip_pixel(round_pow2(sum, FILTER_BITS));
		}
		// End part.
		for (; x < outlength; ++x, y += delta) {
			int	int_pel = y >> INTERP_PRECISION_BITS;
			int	sub_pel = (y >> (INTERP_PRECISION_BITS - SUBPEL_BITS)) & SUBPEL_MASK;
			const int16 *filter = interp_filters[sub_pel];
			int	sum = 0;
			for (int k = 0; k < INTERP_TAPS; ++k)
				sum += filter[k] * input[(int_pel - INTERP_TAPS / 2 + 1 + k >= inlength ? inlength - 1 : int_pel - INTERP_TAPS / 2 + 1 + k)];
			*optr++ = clip_pixel(round_pow2(sum, FILTER_BITS));
		}
	}
}

static void down2_symeven(const uint8 *const input, int length, uint8 *output) {
	static const int16 down2_symeven_half_filter[]	= { 56, 12, -3, -1 };
	const int16 *filter			= down2_symeven_half_filter;
	const int	filter_len_half = sizeof(down2_symeven_half_filter) / 2;
	uint8		*optr			= output;

	int l1 = filter_len_half;
	int l2 = length - filter_len_half;
	l1 += (l1 & 1);
	l2 += (l2 & 1);

	if (l1 > l2) {
		// Short input length.
		for (int i = 0; i < length; i += 2) {
			int sum = (1 << (FILTER_BITS - 1));
			for (int j = 0; j < filter_len_half; ++j)
				sum += (input[(i - j < 0 ? 0 : i - j)] + input[(i + 1 + j >= length ? length - 1 : i + 1 + j)]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
	} else {
		// Initial part.
		int i = 0;
		for (; i < l1; i += 2) {
			int sum = (1 << (FILTER_BITS - 1));
			for (int j = 0; j < filter_len_half; ++j)
				sum += (input[(i - j < 0 ? 0 : i - j)] + input[i + 1 + j]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
		// Middle part.
		for (; i < l2; i += 2) {
			int sum = (1 << (FILTER_BITS - 1));
			for (int j = 0; j < filter_len_half; ++j)
				sum += (input[i - j] + input[i + 1 + j]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
		// End part.
		for (; i < length; i += 2) {
			int sum = (1 << (FILTER_BITS - 1));
			for (int j = 0; j < filter_len_half; ++j)
				sum += (input[i - j] + input[(i + 1 + j >= length ? length - 1 : i + 1 + j)]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
	}
}

static void down2_symodd(const uint8 *const input, int length, uint8 *output) {
	static const int16 down2_symodd_half_filter[]	= { 64, 35, 0, -3 };
	const int16	*filter			= down2_symodd_half_filter;
	const int	filter_len_half = sizeof(down2_symodd_half_filter) / 2;
	uint8		*optr			= output;

	int l1 = filter_len_half - 1;
	int l2 = length - filter_len_half + 1;
	l1 += (l1 & 1);
	l2 += (l2 & 1);

	if (l1 > l2) {
		// Short input length.
		for (int i = 0; i < length; i += 2) {
			int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
			for (int j = 1; j < filter_len_half; ++j)
				sum += (input[(i - j < 0 ? 0 : i - j)] + input[(i + j >= length ? length - 1 : i + j)]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
	} else {
		int	i = 0;
		// Initial part.
		for (; i < l1; i += 2) {
			int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
			for (int j = 1; j < filter_len_half; ++j)
				sum += (input[(i - j < 0 ? 0 : i - j)] + input[i + j]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
		// Middle part.
		for (; i < l2; i += 2) {
			int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
			for (int j = 1; j < filter_len_half; ++j)
				sum += (input[i - j] + input[i + j]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
		// End part.
		for (; i < length; i += 2) {
			int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
			for (int j = 1; j < filter_len_half; ++j)
				sum += (input[i - j] + input[(i + j >= length ? length - 1 : i + j)]) * filter[j];
			sum >>= FILTER_BITS;
			*optr++ = clip_pixel(sum);
		}
	}
}

static int get_down2_length(int length, int steps) {
	for (int s = 0; s < steps; ++s)
		length = (length + 1) >> 1;
	return length;
}

static int get_down2_steps(int in_length, int out_length) {
	int steps = 0;
	int proj_in_length;
	while ((proj_in_length = get_down2_length(in_length, 1)) >= out_length) {
		++steps;
		in_length = proj_in_length;
	}
	return steps;
}
static void resize_multistep(const uint8 *const input, int length, uint8 *output, int olength, uint8 *buf) {
	if (length == olength) {
		memcpy(output, input, length);
		return;
	}

	int	steps = get_down2_steps(length, olength);
	if (steps > 0) {
		malloc_block	tmp1(length);
		uint8			*tmp2	= tmp1 + get_down2_length(length, 1);
		uint8			*out	= unconst(input);
		int				filteredlength = length;
		for (int s = 0; s < steps; ++s) {
			const int proj_filteredlength = get_down2_length(filteredlength, 1);
			const uint8 *const in = out;
			out = s == steps - 1 && proj_filteredlength == olength ? output : s & 1 ? tmp2 : tmp1;
			if (filteredlength & 1)
				down2_symodd(in, filteredlength, out);
			else
				down2_symeven(in, filteredlength, out);
			filteredlength = proj_filteredlength;
		}
		if (filteredlength != olength)
			interpolate(out, filteredlength, output, olength);
	} else {
		interpolate(input, length, output, olength);
	}
}

static void fill_col_to_arr(uint8 *img, int stride, int len, uint8 *arr) {
	for (int i = 0; i < len; ++i, img += stride)
		*arr++ = *img;
}

static void fill_arr_to_col(uint8 *img, int stride, int len, uint8 *arr) {
	for (int i = 0; i < len; ++i, img += stride)
		*img = *arr++;
}
void resize_plane(const uint8 *const input, int height, int width, int in_stride, uint8 *output, int height2, int width2, int out_stride) {
	malloc_block	intbuf(width2 * height);
	malloc_block	tmpbuf(max(width, height));
	malloc_block	arrbuf(height + height2);

	for (int i = 0; i < height; ++i)
		resize_multistep(input + in_stride * i, width, intbuf + width2 * i, width2, tmpbuf);

	for (int i = 0; i < width2; ++i) {
		fill_col_to_arr(intbuf + i, width2, height, arrbuf);
		resize_multistep(arrbuf, height, arrbuf + height, height2, tmpbuf);
		fill_arr_to_col(output + i, out_stride, height2, arrbuf + height);
	}
}

