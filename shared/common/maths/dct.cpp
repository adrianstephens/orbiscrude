#include "dct.h"
#include "base/strings.h"

namespace iso {

struct GenerateConsts {
	GenerateConsts() {
		for (int i = 1; i < 32; ++i) {
			double	d = cos(i * pi / 64);
			ISO_OUTPUTF("static const int cospi_%d_64 = %.0f;\t// %.10f\n", i, round(16384 * d), d);
		}
		for (int i = 1; i < 5; ++i) {
			double	d = sqrt(2) * sin(i * pi / 9) * 2 / 3;
			ISO_OUTPUTF("static const int sinpi_%d_9 = %.0f;\t// %.10f\n", i, round(16384 * d), d);
		}
	}
};// generateconsts;


#ifdef USE_VEC

vec<int16,16> swizzle_0_8_4_12_2_10_6_14_1_9_5_13_3_11_7_15(const vec<int16,16> &x) {
	return swizzle<0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15>(x);
};
vec<int, 8> swizzle_7_6_5_4_3_2_1_0(const vec<int, 8>& x) {
	return swizzle<7,6,5,4,3,2,1,0>(x);
}

vec<int16, 8> swizzle_0_1_2_3_9_10_13_14(const vec<int16, 16>& x) {
	return swizzle<0,1,2,3,9,10,13,14>(x);
}

vec<int, 8> swizzle_1_0_3_2_7_6_5_4(const vec<int, 8>& x) {
	return swizzle<1,0,3,2,7,6,5,4>(x);
}
#endif

//-----------------------------------------------------------------------------

#if 0
void fwht4x4(const int16 *input, tran_low_t *output, int stride) {
	transformCR<tran_low_t, 4>(
		fwht4<tran_low_t>,
		fwht4<tran_low_t>,
		[input, stride](int i, int j)	{ return input[j * stride + i]; },
		[](int a)						{ return a; },
		[output](int i, int j, int a)	{ output[j + i * 4] = a << 2; }
	);
}

//-----------------------------------------------------------------------------

void fdct4x4(const int16 *input, tran_low_t *output, int stride) {
	transformCR<tran_low_t, 4>(
		dct<tran_low_t, DCT_CONST_BITS, 4>::fdct,
		dct<tran_low_t, DCT_CONST_BITS, 4>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 16; },
		[](int a)						{ return a; },
		[output](int i, int j, int a)	{ output[j + i * 4] = a >> 2; }
	);
}

void fdct4x4_1(const int16 *input, tran_low_t *output, int stride) {
	output[ 0] = sum<4>(input, stride) * 2;
	output[ 1] = 0;
}

void fdct8x8(const int16 *input, tran_low_t *output, int stride) {
	transformCR<tran_low_t, 8>(
		dct<tran_low_t, DCT_CONST_BITS, 8>::fdct,
		dct<tran_low_t, DCT_CONST_BITS, 8>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 4; },
		[](int a)						{ return a; },
		[output](int i, int j, int a)	{ output[j + i * 8] = a / 2; }
	);
}

void fdct8x8_1(const int16 *input, tran_low_t *output, int stride) {
	output[ 0] = sum<8>(input, stride);
	output[ 1] = 0;
}

void fdct16x16(const int16 *input, tran_low_t *output, int stride) {
	transformCR<tran_low_t, 16>(
		dct<tran_low_t, DCT_CONST_BITS, 16>::fdct,
		dct<tran_low_t, DCT_CONST_BITS, 16>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 4; },
		[](int a)						{ return a >> 2; },
		[output](int i, int j, int a)	{ output[j + i * 16] = a / 2; }
	);
}

void fdct16x16_1(const int16 *input, tran_low_t *output, int stride) {
	output[ 0] = sum<16>(input, stride) >> 1;
	output[ 1] = 0;
}

void fdct32x32(const int16 *input, tran_low_t *output, int stride) {
	transformCR<tran_low_t, 32>(
		dct<tran_low_t, DCT_CONST_BITS, 32>::fdct,
		dct<tran_low_t, DCT_CONST_BITS, 32>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 4; },
		[](int a)						{ return half_round(a); },
		[output](int i, int j, int a)	{ output[j + i * 32] = half_round(a); }
	);
}

// Note that although we use K::round in dct32 computation flow, this 2d fdct32x32 for rate-distortion optimization loop is operating within 16 bits precision.
void fdct32x32_rd(const int16 *input, tran_low_t *output, int stride) {
	transformCR<tran_low_t, 32>(
		dct<tran_low_t, DCT_CONST_BITS, 32>::fdct,
		dct<tran_low_t, DCT_CONST_BITS, 32>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 4; },
		[](int a)						{ return half_round(a >> 2); },
		[output](int i, int j, int a)	{ output[j + i * 32] = a; }
	);
}

void fdct32x32_1(const int16 *input, tran_low_t *output, int stride) {
	output[ 0] = sum<32>(input, stride) >> 3;
	output[ 1] = 0;
}

void fht4x4(const int16 *input, tran_low_t *output, int stride, int tx_type) {
	transformCR<tran_low_t, 4>(
		tx_type & 2 ? dct<tran_low_t, DCT_CONST_BITS, 4>::fadst : dct<tran_low_t, DCT_CONST_BITS, 4>::fdct,
		tx_type & 1 ? dct<tran_low_t, DCT_CONST_BITS, 4>::fadst : dct<tran_low_t, DCT_CONST_BITS, 4>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 16; },
		[](int a)						{ return a; },
		[output](int i, int j, int a)	{ output[j + i * 4] = a >> 2; }
	);
}

void fht8x8(const int16 *input, tran_low_t *output, int stride, int tx_type) {
	transformCR<tran_low_t, 8>(
		tx_type & 2 ? dct<tran_low_t, DCT_CONST_BITS, 8>::fadst : dct<tran_low_t, DCT_CONST_BITS, 8>::fdct,
		tx_type & 1 ? dct<tran_low_t, DCT_CONST_BITS, 8>::fadst : dct<tran_low_t, DCT_CONST_BITS, 8>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 4; },
		[](int a)						{ return a; },
		[output](int i, int j, int a)	{ output[j + i * 8] = a / 2; }
	);
}

void fht16x16(const int16 *input, tran_low_t *output, int stride, int tx_type) {
	transformCR<tran_low_t, 16>(
		tx_type & 2 ? dct<tran_low_t, DCT_CONST_BITS, 16>::fadst : dct<tran_low_t, DCT_CONST_BITS, 16>::fdct,
		tx_type & 1 ? dct<tran_low_t, DCT_CONST_BITS, 16>::fadst : dct<tran_low_t, DCT_CONST_BITS, 16>::fdct,
		[input, stride](int i, int j)	{ return input[j * stride + i] * 4; },
		[](int a)						{ return half_round(a); },
		[output](int i, int j, int a)	{ output[j + i * 16] = a / 2; }
	);
}
#endif

//-----------------------------------------------------------------------------

void jpeg_fdct(int16 *block8x8) {
	transformRC<8>(block8x8,
		dct<int16,14,8>::fdct,
		dct<int16,14,8>::fdct,
		[block8x8](int i, int j, int a) { block8x8[i + j * 8] = round_pow2(a, 2); }
	);
}

void jpeg_idct(int16 *block8x8) {
	transformRC<8>(block8x8,
		dct<int16,14,8>::idct,
		dct<int16,14,8>::idct,
		[block8x8](int i, int j, int a) { block8x8[i + j * 8] = round_pow2(a, 2); }
	);
}

void jpeg_fdctf(float *block8x8) {
	transformRC<8>(block8x8,
		dct<float,0,8>::fdct,
		dct<float,0,8>::fdct,
		[block8x8](int i, int j, float a) { block8x8[i + j * 8] = a; }
	);
}

} // namespace iso