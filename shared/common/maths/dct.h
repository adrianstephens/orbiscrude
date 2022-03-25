#ifndef	DCT_H
#define	DCT_H

#include "base/defs.h"
#include "base/maths.h"

namespace iso {

template<int N, typename T> T sum(const T *input, int stride) {
	T sum = 0;
	for (int r = 0; r < N; ++r)
		for (int c = 0; c < N; ++c)
			sum += input[r * stride + c];
	return sum;
}

template<int N, typename T, typename FR, typename FC, typename FP> static void transformRC(const T *input, FR row, FC col, FP put, int nonzero_rows = N) {
	T block[N * N], *outptr = block;
	T temp_in[N], temp_out[N];

	// Rows
	for (int r = 0; r < nonzero_rows; ++r) {
		row(input, outptr);
		input	+= N;
		outptr	+= N;
	}
	if (nonzero_rows < N)
		memset(outptr, 0, (N - nonzero_rows) * sizeof(temp_out));

	// Columns
	for (int c = 0; c < N; ++c) {
		for (int r = 0; r < N; ++r)
			temp_in[r] = block[r * N + c];
		col(temp_in, temp_out);
		for (int r = 0; r < N; ++r)
			put(c, r, temp_out[r]);
	}
}

template<typename T, int N, typename FC, typename FR, typename FG, typename FA, typename FP> static void transformCR(FC col, FR row, FG get, FA adjust, FP put) {
	T temp_in[N], temp_out[N];
	T block[N * N];

	// Columns
	for (int c = 0; c < N; ++c) {
		for (int r = 0; r < N; ++r)
			temp_in[r] = get(c, r);
		col(temp_in, temp_out);
		for (int r = 0; r < N; ++r)
			block[r * N + c] = adjust(temp_out[r]);
	}

	// Rows
	for (int r = 0; r < N; ++r) {
		row(block + r * N, temp_out);
		for (int c = 0; c < N; ++c)
			put(c, r, temp_out[c]);
	}
}

template<typename T> inline T half_round(T input) {
	return (input + 1 + (input < 0)) >> 2;
}

// (lossless) Walsh–Hadamard transform
template<typename T> void fwht4(const T *input, T *output) {
	T	a1 = input[0];
	T	b1 = input[1];
	T	c1 = input[2];
	T	d1 = input[3];
	a1	+= b1;
	d1	= d1 - c1;
	T	e1	= (a1 - d1) / 2;
	b1	= e1 - b1;
	c1	= e1 - c1;
	output[0]	= a1 - c1;
	output[1]	= c1;
	output[2]	= d1 + b1;
	output[3]	= b1;
}

template<typename T> void iwht4(const T *input, T *output) {
	T	a1 = input[0];
	T	c1 = input[1];
	T	d1 = input[2];
	T	b1 = input[3];
	a1 += c1;
	d1 -= b1;
	T	e1 = (a1 - d1) / 2;
	b1 = e1 - b1;
	c1 = e1 - c1;
	output[0] = a1 - b1;
	output[1] = b1;
	output[2] = c1;
	output[3] = d1 + c1;
}

template<typename T, int BITS> struct mult_type {
	typedef int_t<(sizeof(T) * 8 + BITS + 7) / 8, num_traits<T>::is_signed>	type;
};

template<typename T> struct mult_type<T, 0> {
	typedef T	type;
};

template<typename T, int BITS> struct dct_consts {
	enum { S = 1 << BITS };
	typedef typename mult_type<T, BITS>::type	T2;
	static constexpr float R			= BITS ? 0.5f : 0;
	static constexpr T cospi_1_64		= T(0.9987954139 * S + R);
	static constexpr T cospi_2_64		= T(0.9951847076 * S + R);
	static constexpr T cospi_3_64		= T(0.9891765594 * S + R);
	static constexpr T cospi_4_64		= T(0.9807852745 * S + R);
	static constexpr T cospi_5_64		= T(0.9700312614 * S + R);
	static constexpr T cospi_6_64		= T(0.9569403648 * S + R);
	static constexpr T cospi_7_64		= T(0.9415440559 * S + R);
	static constexpr T cospi_8_64		= T(0.9238795280 * S + R);
	static constexpr T cospi_9_64		= T(0.9039893150 * S + R);
	static constexpr T cospi_10_64		= T(0.8819211959 * S + R);
	static constexpr T cospi_11_64		= T(0.8577285766 * S + R);
	static constexpr T cospi_12_64		= T(0.8314696311 * S + R);
	static constexpr T cospi_13_64		= T(0.8032074928 * S + R);
	static constexpr T cospi_14_64		= T(0.7730104446 * S + R);
	static constexpr T cospi_15_64		= T(0.7409511089 * S + R);
	static constexpr T cospi_16_64		= T(0.7071067810 * S + R);
	static constexpr T cospi_17_64		= T(0.6715589046 * S + R);
	static constexpr T cospi_18_64		= T(0.6343932628 * S + R);
	static constexpr T cospi_19_64		= T(0.5956993103 * S + R);
	static constexpr T cospi_20_64		= T(0.5555701732 * S + R);
	static constexpr T cospi_21_64		= T(0.5141026973 * S + R);
	static constexpr T cospi_22_64		= T(0.4713966369 * S + R);
	static constexpr T cospi_23_64		= T(0.4275551319 * S + R);
	static constexpr T cospi_24_64		= T(0.3826834201 * S + R);
	static constexpr T cospi_25_64		= T(0.3368898391 * S + R);
	static constexpr T cospi_26_64		= T(0.2902846336 * S + R);
	static constexpr T cospi_27_64		= T(0.2429801225 * S + R);
	static constexpr T cospi_28_64		= T(0.1950902366 * S + R);
	static constexpr T cospi_29_64		= T(0.1467304944 * S + R);
	static constexpr T cospi_30_64		= T(0.0980171298 * S + R);
	static constexpr T cospi_31_64		= T(0.0490676498 * S + R);
	static constexpr T sinpi_1_9_sqrt2	= T(0.3224596977 * S + R);
	static constexpr T sinpi_2_9_sqrt2	= T(0.6060259819 * S + R);
	static constexpr T sinpi_3_9_sqrt2	= T(0.8164966583 * S + R);
	static constexpr T sinpi_4_9_sqrt2	= T(0.9284856796 * S + R);

	static inline T	idc(T dc) {
		return round(round(dc * cospi_16_64) * cospi_16_64);
	}
	static inline T round(T2 x) {
		return round_pow2(x, BITS);
	}
	static inline int result(T x, int bits) {
		return round_pow2(x, BITS - bits);
	}
};

template<> inline float dct_consts<float,0>::round(float x) {
	return x;
}
template<> inline int dct_consts<float,0>::result(float x, int bits) {
	return int(x * (1 << bits) + 0.5f);
}

template<typename T, int N> void dct_copy(const T *input, T *output) {
	for (int i = 0; i < N; i++)
		output[i] = input[i];
}

template<typename T, int BITS, int N> struct dct;

template<typename T, int BITS> struct dct<T, BITS, 4> {
	typedef dct_consts<T, BITS>		K;
	typedef typename K::T2			T2;

	static void fdct(const T *input, T *output) {
		T	s0		= input[ 0] + input[ 3];	// *16 in 1st pass, as-is in 2nd, >> 2 final
		T	s1		= input[ 1] + input[ 2];
		T	s2		= input[ 1] - input[ 2];
		T	s3		= input[ 0] - input[ 3];
		output[ 0]	= K::round((s0 + s1) * K::cospi_16_64);
		output[ 2]	= K::round((s0 - s1) * K::cospi_16_64);
		output[ 1]	= K::round(s2 * K::cospi_24_64 + s3 * K::cospi_8_64);
		output[ 3]	= K::round(s3 * K::cospi_24_64 - s2 * K::cospi_8_64);
	}
	static void idct(const T *input, T *output) {
		T	x0 = input[ 0];
		T	x1 = input[ 1];
		T	x2 = input[ 2];
		T	x3 = input[ 3];

		// stage 1
		T	s0 = K::round((x0 + x2) * K::cospi_16_64);
		T	s1 = K::round((x0 - x2) * K::cospi_16_64);
		T	s2 = K::round(x1 * K::cospi_24_64 - x3 * K::cospi_8_64);
		T	s3 = K::round(x1 * K::cospi_8_64  + x3 * K::cospi_24_64);

		// stage 2
		output[ 0] = s0 + s3;
		output[ 1] = s1 + s2;
		output[ 2] = s1 - s2;
		output[ 3] = s0 - s3;
	}

	static void fadst(const T *input, T *output) {
		T	x0 = input[0];
		T	x1 = input[1];
		T	x2 = input[2];
		T	x3 = input[3];

		if (!(x0 || x1 || x2 || x3)) {
			output[0] = output[1] = output[2] = output[3] = 0;
			return;
		}

		T2	s0 = K::sinpi_1_9_sqrt2 * x0;
		T2	s1 = K::sinpi_4_9_sqrt2 * x0;
		T2	s2 = K::sinpi_2_9_sqrt2 * x1;
		T2	s3 = K::sinpi_1_9_sqrt2 * x1;
		T2	s4 = K::sinpi_3_9_sqrt2 * x2;
		T2	s5 = K::sinpi_4_9_sqrt2 * x3;
		T2	s6 = K::sinpi_2_9_sqrt2 * x3;
		T2	s7 = x0 + x1 - x3;

		s0 = s0 + s2 + s5;
		s2 = s1 - s3 + s6;
		s1 = K::sinpi_3_9_sqrt2 * s7;

		// 1-D transform scaling factor is sqrt(2)
		output[0] = K::round(s0 + s4);
		output[1] = K::round(s1);
		output[2] = K::round(s2 - s4);
		output[3] = K::round(s2 - s0 + s4);
	}
	static void iadst(const T *input, T *output) {
		T	x0 = input[ 0];
		T	x1 = input[ 1];
		T	x2 = input[ 2];
		T	x3 = input[ 3];

		if (!(x0 || x1 || x2 || x3)) {
			output[ 0] = output[ 1] = output[ 2] = output[ 3] = 0;
			return;
		}

		T2	s0 = K::sinpi_1_9_sqrt2 * x0;
		T2	s1 = K::sinpi_2_9_sqrt2 * x0;
		T2	s2 = K::sinpi_3_9_sqrt2 * x1;
		T2	s3 = K::sinpi_4_9_sqrt2 * x2;
		T2	s4 = K::sinpi_1_9_sqrt2 * x2;
		T2	s5 = K::sinpi_2_9_sqrt2 * x3;
		T2	s6 = K::sinpi_4_9_sqrt2 * x3;
		T2	s7 = x0 - x2 + x3;

		s0 = s0 + s3 + s5;
		s1 = s1 - s4 - s6;
		s3 = s2;
		s2 = K::sinpi_3_9_sqrt2 * s7;

		// 1-D transform scaling factor is sqrt(2)
		output[ 0] = K::round(s0 + s3);
		output[ 1] = K::round(s1 + s3);
		output[ 2] = K::round(s2);
		output[ 3] = K::round(s0 + s1 - s3);
	}
};

template<typename T, int BITS> struct dct<T, BITS, 8> {
	typedef dct_consts<T, BITS>		K;
	typedef typename K::T2			T2;

	static void fdct(const T *input, T *output) {
		T	s0 = input[ 0] + input[ 7];
		T	s1 = input[ 1] + input[ 6];
		T	s2 = input[ 2] + input[ 5];
		T	s3 = input[ 3] + input[ 4];
		T	s4 = input[ 3] - input[ 4];
		T	s5 = input[ 2] - input[ 5];
		T	s6 = input[ 1] - input[ 6];
		T	s7 = input[ 0] - input[ 7];

		T	x0 = s0 + s3;
		T	x1 = s1 + s2;
		T	x2 = s1 - s2;
		T	x3 = s0 - s3;
		output[ 0] = K::round((x0 + x1) * K::cospi_16_64);
		output[ 2] = K::round(x2 * K::cospi_24_64 + x3 * K::cospi_8_64);
		output[ 4] = K::round((x0 - x1) * K::cospi_16_64);
		output[ 6] = K::round(x3 * K::cospi_24_64 - x2 * K::cospi_8_64);

		// Stage 2
		T	t2 = K::round((s6 - s5) * K::cospi_16_64);
		T	t3 = K::round((s6 + s5) * K::cospi_16_64);

		// Stage 3
		x0 = s4 + t2;
		x1 = s4 - t2;
		x2 = s7 - t3;
		x3 = s7 + t3;

		// Stage 4
		output[ 1] = K::round(x0 * K::cospi_28_64 + x3 * K::cospi_4_64);
		output[ 3] = K::round(x2 * K::cospi_12_64 - x1 * K::cospi_20_64);
		output[ 5] = K::round(x1 * K::cospi_12_64 + x2 * K::cospi_20_64);
		output[ 7] = K::round(x3 * K::cospi_28_64 - x0 * K::cospi_4_64);
	}
	static void idct(const T *input, T *output) {
		T step1[ 8], step2[ 8];
		// stage 1
		step1[ 0] = input[ 0];
		step1[ 2] = input[ 4];
		step1[ 1] = input[ 2];
		step1[ 3] = input[ 6];
		step1[ 4] = K::round(input[ 1] * K::cospi_28_64 - input[ 7] * K::cospi_4_64);
		step1[ 7] = K::round(input[ 1] * K::cospi_4_64 + input[ 7] * K::cospi_28_64);
		step1[ 5] = K::round(input[ 5] * K::cospi_12_64 - input[ 3] * K::cospi_20_64);
		step1[ 6] = K::round(input[ 5] * K::cospi_20_64 + input[ 3] * K::cospi_12_64);

		// stage 2
		step2[ 0] = K::round((step1[ 0] + step1[ 2]) * K::cospi_16_64);
		step2[ 1] = K::round((step1[ 0] - step1[ 2]) * K::cospi_16_64);
		step2[ 2] = K::round(step1[ 1] * K::cospi_24_64 - step1[ 3] * K::cospi_8_64);
		step2[ 3] = K::round(step1[ 1] * K::cospi_8_64 + step1[ 3] * K::cospi_24_64);
		step2[ 4] = step1[ 4] + step1[ 5];
		step2[ 5] = step1[ 4] - step1[ 5];
		step2[ 6] = step1[ 7] - step1[ 6];
		step2[ 7] = step1[ 6] + step1[ 7];

		// stage 3
		step1[ 0] = step2[ 0] + step2[ 3];
		step1[ 1] = step2[ 1] + step2[ 2];
		step1[ 2] = step2[ 1] - step2[ 2];
		step1[ 3] = step2[ 0] - step2[ 3];
		step1[ 4] = step2[ 4];
		step1[ 5] = K::round((step2[ 6] - step2[ 5]) * K::cospi_16_64);
		step1[ 6] = K::round((step2[ 5] + step2[ 6]) * K::cospi_16_64);
		step1[ 7] = step2[ 7];

		// stage 4
		output[ 0] = step1[ 0] + step1[ 7];
		output[ 1] = step1[ 1] + step1[ 6];
		output[ 2] = step1[ 2] + step1[ 5];
		output[ 3] = step1[ 3] + step1[ 4];
		output[ 4] = step1[ 3] - step1[ 4];
		output[ 5] = step1[ 2] - step1[ 5];
		output[ 6] = step1[ 1] - step1[ 6];
		output[ 7] = step1[ 0] - step1[ 7];
	}

	static void fadst(const T *input, T *output) {
		T	x0 = input[7];
		T	x1 = input[0];
		T	x2 = input[5];
		T	x3 = input[2];
		T	x4 = input[3];
		T	x5 = input[4];
		T	x6 = input[1];
		T	x7 = input[6];

		// stage 1
		T2	s0 = K::cospi_2_64  * x0 + K::cospi_30_64 * x1;
		T2	s1 = K::cospi_30_64 * x0 - K::cospi_2_64  * x1;
		T2	s2 = K::cospi_10_64 * x2 + K::cospi_22_64 * x3;
		T2	s3 = K::cospi_22_64 * x2 - K::cospi_10_64 * x3;
		T2	s4 = K::cospi_18_64 * x4 + K::cospi_14_64 * x5;
		T2	s5 = K::cospi_14_64 * x4 - K::cospi_18_64 * x5;
		T2	s6 = K::cospi_26_64 * x6 + K::cospi_6_64  * x7;
		T2	s7 = K::cospi_6_64  * x6 - K::cospi_26_64 * x7;

		x0 = K::round(s0 + s4);
		x1 = K::round(s1 + s5);
		x2 = K::round(s2 + s6);
		x3 = K::round(s3 + s7);
		x4 = K::round(s0 - s4);
		x5 = K::round(s1 - s5);
		x6 = K::round(s2 - s6);
		x7 = K::round(s3 - s7);

		// stage 2
		s0 = x0;
		s1 = x1;
		s2 = x2;
		s3 = x3;
		s4 = K::cospi_8_64  * x4 + K::cospi_24_64 * x5;
		s5 = K::cospi_24_64 * x4 - K::cospi_8_64  * x5;
		s6 = K::cospi_8_64  * x7 - K::cospi_24_64 * x6;
		s7 = K::cospi_8_64  * x6 + K::cospi_24_64 * x7;

		x0 = s0 + s2;
		x1 = s1 + s3;
		x2 = s0 - s2;
		x3 = s1 - s3;
		x4 = K::round(s4 + s6);
		x5 = K::round(s5 + s7);
		x6 = K::round(s4 - s6);
		x7 = K::round(s5 - s7);

		// stage 3
		s2 = K::cospi_16_64 * (x2 + x3);
		s3 = K::cospi_16_64 * (x2 - x3);
		s6 = K::cospi_16_64 * (x6 + x7);
		s7 = K::cospi_16_64 * (x6 - x7);

		x2 = K::round(s2);
		x3 = K::round(s3);
		x6 = K::round(s6);
		x7 = K::round(s7);

		output[0] = x0;
		output[1] = -x4;
		output[2] = x6;
		output[3] = -x2;
		output[4] = x3;
		output[5] = -x7;
		output[6] = x5;
		output[7] = -x1;
	}
	static void iadst(const T *input, T *output) {
		T2	x0 = input[ 7];
		T2	x1 = input[ 0];
		T2	x2 = input[ 5];
		T2	x3 = input[ 2];
		T2	x4 = input[ 3];
		T2	x5 = input[ 4];
		T2	x6 = input[ 1];
		T2	x7 = input[ 6];

		if (!(x0 || x1 || x2 || x3 || x4 || x5 || x6 || x7)) {
			output[ 0] = output[ 1] = output[ 2] = output[ 3] = output[ 4] = output[ 5] = output[ 6] = output[ 7] = 0;
			return;
		}

		// stage 1
		T2	s0 = K::cospi_2_64  * x0 + K::cospi_30_64 * x1;
		T2	s1 = K::cospi_30_64 * x0 - K::cospi_2_64  * x1;
		T2	s2 = K::cospi_10_64 * x2 + K::cospi_22_64 * x3;
		T2	s3 = K::cospi_22_64 * x2 - K::cospi_10_64 * x3;
		T2	s4 = K::cospi_18_64 * x4 + K::cospi_14_64 * x5;
		T2	s5 = K::cospi_14_64 * x4 - K::cospi_18_64 * x5;
		T2	s6 = K::cospi_26_64 * x6 + K::cospi_6_64  * x7;
		T2	s7 = K::cospi_6_64  * x6 - K::cospi_26_64 * x7;

		x0 = K::round(s0 + s4);
		x1 = K::round(s1 + s5);
		x2 = K::round(s2 + s6);
		x3 = K::round(s3 + s7);
		x4 = K::round(s0 - s4);
		x5 = K::round(s1 - s5);
		x6 = K::round(s2 - s6);
		x7 = K::round(s3 - s7);

		// stage 2
		s0 = x0;
		s1 = x1;
		s2 = x2;
		s3 = x3;
		s4 = K::cospi_8_64  * x4 + K::cospi_24_64 * x5;
		s5 = K::cospi_24_64 * x4 - K::cospi_8_64  * x5;
		s6 = K::cospi_8_64  * x7 - K::cospi_24_64 * x6;
		s7 = K::cospi_8_64  * x6 + K::cospi_24_64 * x7;

		x0 = s0 + s2;
		x1 = s1 + s3;
		x2 = s0 - s2;
		x3 = s1 - s3;
		x4 = K::round(s4 + s6);
		x5 = K::round(s5 + s7);
		x6 = K::round(s4 - s6);
		x7 = K::round(s5 - s7);

		// stage 3
		s2 = K::cospi_16_64 * (x2 + x3);
		s3 = K::cospi_16_64 * (x2 - x3);
		s6 = K::cospi_16_64 * (x6 + x7);
		s7 = K::cospi_16_64 * (x6 - x7);

		x2 = K::round(s2);
		x3 = K::round(s3);
		x6 = K::round(s6);
		x7 = K::round(s7);

		output[ 0] = x0;
		output[ 1] = -x4;
		output[ 2] = x6;
		output[ 3] = -x2;
		output[ 4] = x3;
		output[ 5] = -x7;
		output[ 6] = x5;
		output[ 7] = -x1;
	}
};

template<typename T, int BITS> struct dct<T, BITS, 16> {
	typedef dct_consts<T, BITS>		K;
	typedef typename K::T2			T2;

	static void fdct(const T *input, T *output) {
		T	step0[ 8];
		T	step1[ 8];
		T	step2[ 8];
		T	step3[ 8];

		step0[ 0] = input[ 0] + input[15];
		step0[ 1] = input[ 1] + input[14];
		step0[ 2] = input[ 2] + input[13];
		step0[ 3] = input[ 3] + input[12];
		step0[ 4] = input[ 4] + input[11];
		step0[ 5] = input[ 5] + input[10];
		step0[ 6] = input[ 6] + input[ 9];
		step0[ 7] = input[ 7] + input[ 8];

		step1[ 0] = input[ 7] - input[ 8];
		step1[ 1] = input[ 6] - input[ 9];
		step1[ 2] = input[ 5] - input[10];
		step1[ 3] = input[ 4] - input[11];
		step1[ 4] = input[ 3] - input[12];
		step1[ 5] = input[ 2] - input[13];
		step1[ 6] = input[ 1] - input[14];
		step1[ 7] = input[ 0] - input[15];

		// stage 1
		T	s0 = step0[ 0] + step0[ 7];
		T	s1 = step0[ 1] + step0[ 6];
		T	s2 = step0[ 2] + step0[ 5];
		T	s3 = step0[ 3] + step0[ 4];
		T	s4 = step0[ 3] - step0[ 4];
		T	s5 = step0[ 2] - step0[ 5];
		T	s6 = step0[ 1] - step0[ 6];
		T	s7 = step0[ 0] - step0[ 7];

		// fdct4(step, step);
		T	x0 = s0 + s3;
		T	x1 = s1 + s2;
		T	x2 = s1 - s2;
		T	x3 = s0 - s3;

		output[ 0]	= K::round((x0 + x1) * K::cospi_16_64);
		output[ 4]	= K::round(x3 * K::cospi_8_64 + x2 * K::cospi_24_64);
		output[ 8]	= K::round((x0 - x1) * K::cospi_16_64);
		output[12]	= K::round(x3 * K::cospi_24_64 - x2 * K::cospi_8_64);

		// Stage 2
		T	t2 = K::round((s6 - s5) * K::cospi_16_64);
		T	t3 = K::round((s6 + s5) * K::cospi_16_64);

		// Stage 3
		x0 = s4 + t2;
		x1 = s4 - t2;
		x2 = s7 - t3;
		x3 = s7 + t3;

		// Stage 4
		output[ 2]	= K::round(x0 * K::cospi_28_64 + x3 * K::cospi_4_64);
		output[ 6]	= K::round(x2 * K::cospi_12_64 - x1 * K::cospi_20_64);
		output[10]	= K::round(x1 * K::cospi_12_64 + x2 * K::cospi_20_64);
		output[14]	= K::round(x3 * K::cospi_28_64 - x0 * K::cospi_4_64);

		// Work on the next eight values; step1 -> odd_results
		// step 2
		step2[ 2] = K::round((step1[ 5] - step1[ 2]) * K::cospi_16_64);
		step2[ 3] = K::round((step1[ 4] - step1[ 3]) * K::cospi_16_64);
		step2[ 4] = K::round((step1[ 4] + step1[ 3]) * K::cospi_16_64);
		step2[ 5] = K::round((step1[ 5] + step1[ 2]) * K::cospi_16_64);
		// step 3
		step3[ 0] = step1[ 0] + step2[ 3];
		step3[ 1] = step1[ 1] + step2[ 2];
		step3[ 2] = step1[ 1] - step2[ 2];
		step3[ 3] = step1[ 0] - step2[ 3];
		step3[ 4] = step1[ 7] - step2[ 4];
		step3[ 5] = step1[ 6] - step2[ 5];
		step3[ 6] = step1[ 6] + step2[ 5];
		step3[ 7] = step1[ 7] + step2[ 4];
		// step 4
		step2[ 1] = K::round(step3[ 1] * -K::cospi_8_64 + step3[ 6] * K::cospi_24_64);
		step2[ 2] = K::round(step3[ 2] * K::cospi_24_64 + step3[ 5] * K::cospi_8_64);
		step2[ 5] = K::round(step3[ 2] * K::cospi_8_64 - step3[ 5] * K::cospi_24_64);
		step2[ 6] = K::round(step3[ 1] * K::cospi_24_64 + step3[ 6] * K::cospi_8_64);
		// step 5
		step1[ 0] = step3[ 0] + step2[ 1];
		step1[ 1] = step3[ 0] - step2[ 1];
		step1[ 2] = step3[ 3] + step2[ 2];
		step1[ 3] = step3[ 3] - step2[ 2];
		step1[ 4] = step3[ 4] - step2[ 5];
		step1[ 5] = step3[ 4] + step2[ 5];
		step1[ 6] = step3[ 7] - step2[ 6];
		step1[ 7] = step3[ 7] + step2[ 6];
		// step 6
		output[ 1] = K::round(step1[ 0] * K::cospi_30_64 + step1[ 7] * K::cospi_2_64);
		output[ 9] = K::round(step1[ 1] * K::cospi_14_64 + step1[ 6] * K::cospi_18_64);
		output[ 5] = K::round(step1[ 2] * K::cospi_22_64 + step1[ 5] * K::cospi_10_64);
		output[13] = K::round(step1[ 3] * K::cospi_6_64 + step1[ 4] * K::cospi_26_64);
		output[ 3] = K::round(step1[ 3] * -K::cospi_26_64 + step1[ 4] * K::cospi_6_64);
		output[11] = K::round(step1[ 2] * -K::cospi_10_64 + step1[ 5] * K::cospi_22_64);
		output[ 7] = K::round(step1[ 1] * -K::cospi_18_64 + step1[ 6] * K::cospi_14_64);
		output[15] = K::round(step1[ 0] * -K::cospi_2_64 + step1[ 7] * K::cospi_30_64);
	}
	static void idct(const T *input, T *output) {
		T	step1[16], step2[16];

		// stage 1
		step1[ 0]	= input[0 / 2];
		step1[ 1]	= input[16 / 2];
		step1[ 2]	= input[8 / 2];
		step1[ 3]	= input[24 / 2];
		step1[ 4]	= input[4 / 2];
		step1[ 5]	= input[20 / 2];
		step1[ 6]	= input[12 / 2];
		step1[ 7]	= input[28 / 2];
		step1[ 8]	= input[2 / 2];
		step1[ 9]	= input[18 / 2];
		step1[10]	= input[10 / 2];
		step1[11]	= input[26 / 2];
		step1[12]	= input[6 / 2];
		step1[13]	= input[22 / 2];
		step1[14]	= input[14 / 2];
		step1[15]	= input[30 / 2];

		// stage 2
		step2[ 0]	= step1[ 0];
		step2[ 1]	= step1[ 1];
		step2[ 2]	= step1[ 2];
		step2[ 3]	= step1[ 3];
		step2[ 4]	= step1[ 4];
		step2[ 5]	= step1[ 5];
		step2[ 6]	= step1[ 6];
		step2[ 7]	= step1[ 7];

		step2[ 8]	= K::round(step1[ 8] * K::cospi_30_64 - step1[15] * K::cospi_2_64);
		step2[ 9]	= K::round(step1[ 9] * K::cospi_14_64 - step1[14] * K::cospi_18_64);
		step2[10]	= K::round(step1[10] * K::cospi_22_64 - step1[13] * K::cospi_10_64);
		step2[11]	= K::round(step1[11] * K::cospi_6_64  - step1[12] * K::cospi_26_64);
		step2[12]	= K::round(step1[11] * K::cospi_26_64 + step1[12] * K::cospi_6_64);
		step2[13]	= K::round(step1[10] * K::cospi_10_64 + step1[13] * K::cospi_22_64);
		step2[14]	= K::round(step1[ 9] * K::cospi_18_64 + step1[14] * K::cospi_14_64);
		step2[15]	= K::round(step1[ 8] * K::cospi_2_64  + step1[15] * K::cospi_30_64);

		// stage 3
		step1[ 0]	= step2[ 0];
		step1[ 1]	= step2[ 1];
		step1[ 2]	= step2[ 2];
		step1[ 3]	= step2[ 3];
		step1[ 4]	= K::round(step2[ 4] * K::cospi_28_64 - step2[ 7] * K::cospi_4_64);
		step1[ 5]	= K::round(step2[ 5] * K::cospi_12_64 - step2[ 6] * K::cospi_20_64);
		step1[ 6]	= K::round(step2[ 5] * K::cospi_20_64 + step2[ 6] * K::cospi_12_64);
		step1[ 7]	= K::round(step2[ 4] * K::cospi_4_64  + step2[ 7] * K::cospi_28_64);

		step1[ 8]	= step2[ 8] + step2[ 9];
		step1[ 9]	= step2[ 8] - step2[ 9];
		step1[10]	= step2[11] - step2[10];
		step1[11]	= step2[10] + step2[11];
		step1[12]	= step2[12] + step2[13];
		step1[13]	= step2[12] - step2[13];
		step1[14]	= step2[15] - step2[14];
		step1[15]	= step2[14] + step2[15];

		// stage 4
		step2[ 0]	= K::round((step1[ 0] + step1[ 1]) * K::cospi_16_64);
		step2[ 1]	= K::round((step1[ 0] - step1[ 1]) * K::cospi_16_64);
		step2[ 2]	= K::round(step1[ 2] * K::cospi_24_64 - step1[ 3] * K::cospi_8_64);
		step2[ 3]	= K::round(step1[ 2] * K::cospi_8_64  + step1[ 3] * K::cospi_24_64);
		step2[ 4]	= step1[ 4] + step1[ 5];
		step2[ 5]	= step1[ 4] - step1[ 5];
		step2[ 6]	= step1[ 7] - step1[ 6];
		step2[ 7]	= step1[ 6] + step1[ 7];

		step2[ 8]	= step1[ 8];
		step2[ 9]	= K::round( step1[14] * K::cospi_24_64 - step1[ 9] * K::cospi_8_64);
		step2[10]	= K::round(-step1[10] * K::cospi_24_64 - step1[13] * K::cospi_8_64);
		step2[11]	= step1[11];
		step2[12]	= step1[12];
		step2[13]	= K::round( step1[13] * K::cospi_24_64 - step1[10] * K::cospi_8_64);
		step2[14]	= K::round( step1[ 9] * K::cospi_24_64 + step1[14] * K::cospi_8_64);
		step2[15]	= step1[15];

		// stage 5
		step1[ 0]	= step2[ 0] + step2[ 3];
		step1[ 1]	= step2[ 1] + step2[ 2];
		step1[ 2]	= step2[ 1] - step2[ 2];
		step1[ 3]	= step2[ 0] - step2[ 3];
		step1[ 4]	= step2[ 4];
		step1[ 5]	= K::round((step2[ 6] - step2[ 5]) * K::cospi_16_64);
		step1[ 6]	= K::round((step2[ 5] + step2[ 6]) * K::cospi_16_64);
		step1[ 7]	= step2[ 7];

		step1[ 8]	= step2[ 8] + step2[11];
		step1[ 9]	= step2[ 9] + step2[10];
		step1[10]	= step2[ 9] - step2[10];
		step1[11]	= step2[ 8] - step2[11];
		step1[12]	= step2[15] - step2[12];
		step1[13]	= step2[14] - step2[13];
		step1[14]	= step2[13] + step2[14];
		step1[15]	= step2[12] + step2[15];

		// stage 6
		step2[ 0]	= step1[ 0] + step1[ 7];
		step2[ 1]	= step1[ 1] + step1[ 6];
		step2[ 2]	= step1[ 2] + step1[ 5];
		step2[ 3]	= step1[ 3] + step1[ 4];
		step2[ 4]	= step1[ 3] - step1[ 4];
		step2[ 5]	= step1[ 2] - step1[ 5];
		step2[ 6]	= step1[ 1] - step1[ 6];
		step2[ 7]	= step1[ 0] - step1[ 7];
		step2[ 8]	= step1[ 8];
		step2[ 9]	= step1[ 9];
		step2[10]	= K::round((-step1[10] + step1[13]) * K::cospi_16_64);
		step2[13]	= K::round(( step1[10] + step1[13]) * K::cospi_16_64);
		step2[11]	= K::round((-step1[11] + step1[12]) * K::cospi_16_64);
		step2[12]	= K::round(( step1[11] + step1[12]) * K::cospi_16_64);
		step2[14]	= step1[14];
		step2[15]	= step1[15];

		// stage 7
		output[ 0]	= step2[ 0] + step2[15];
		output[ 1]	= step2[ 1] + step2[14];
		output[ 2]	= step2[ 2] + step2[13];
		output[ 3]	= step2[ 3] + step2[12];
		output[ 4]	= step2[ 4] + step2[11];
		output[ 5]	= step2[ 5] + step2[10];
		output[ 6]	= step2[ 6] + step2[ 9];
		output[ 7]	= step2[ 7] + step2[ 8];
		output[ 8]	= step2[ 7] - step2[ 8];
		output[ 9]	= step2[ 6] - step2[ 9];
		output[10]	= step2[ 5] - step2[10];
		output[11]	= step2[ 4] - step2[11];
		output[12]	= step2[ 3] - step2[12];
		output[13]	= step2[ 2] - step2[13];
		output[14]	= step2[ 1] - step2[14];
		output[15]	= step2[ 0] - step2[15];
	}
	static void fadst(const T *input, T *output) {
		T	x0	= input[15];
		T	x1	= input[0];
		T	x2	= input[13];
		T	x3	= input[2];
		T	x4	= input[11];
		T	x5	= input[4];
		T	x6	= input[9];
		T	x7	= input[6];
		T	x8	= input[7];
		T	x9	= input[8];
		T	x10	= input[5];
		T	x11	= input[10];
		T	x12	= input[3];
		T	x13	= input[12];
		T	x14	= input[1];
		T	x15	= input[14];

		// stage 1
		T2	s0	= x0  * K::cospi_1_64  + x1  * K::cospi_31_64;
		T2	s1	= x0  * K::cospi_31_64 - x1  * K::cospi_1_64;
		T2	s2	= x2  * K::cospi_5_64  + x3  * K::cospi_27_64;
		T2	s3	= x2  * K::cospi_27_64 - x3  * K::cospi_5_64;
		T2	s4	= x4  * K::cospi_9_64  + x5  * K::cospi_23_64;
		T2	s5	= x4  * K::cospi_23_64 - x5  * K::cospi_9_64;
		T2	s6	= x6  * K::cospi_13_64 + x7  * K::cospi_19_64;
		T2	s7	= x6  * K::cospi_19_64 - x7  * K::cospi_13_64;
		T2	s8	= x8  * K::cospi_17_64 + x9  * K::cospi_15_64;
		T2	s9	= x8  * K::cospi_15_64 - x9  * K::cospi_17_64;
		T2	s10	= x10 * K::cospi_21_64 + x11 * K::cospi_11_64;
		T2	s11	= x10 * K::cospi_11_64 - x11 * K::cospi_21_64;
		T2	s12	= x12 * K::cospi_25_64 + x13 * K::cospi_7_64;
		T2	s13	= x12 * K::cospi_7_64  - x13 * K::cospi_25_64;
		T2	s14	= x14 * K::cospi_29_64 + x15 * K::cospi_3_64;
		T2	s15	= x14 * K::cospi_3_64  - x15 * K::cospi_29_64;

		x0	= K::round(s0 + s8);
		x1	= K::round(s1 + s9);
		x2	= K::round(s2 + s10);
		x3	= K::round(s3 + s11);
		x4	= K::round(s4 + s12);
		x5	= K::round(s5 + s13);
		x6	= K::round(s6 + s14);
		x7	= K::round(s7 + s15);
		x8	= K::round(s0 - s8);
		x9	= K::round(s1 - s9);
		x10 = K::round(s2 - s10);
		x11 = K::round(s3 - s11);
		x12 = K::round(s4 - s12);
		x13 = K::round(s5 - s13);
		x14 = K::round(s6 - s14);
		x15 = K::round(s7 - s15);

		// stage 2
		s0	= x0;
		s1	= x1;
		s2	= x2;
		s3	= x3;
		s4	= x4;
		s5	= x5;
		s6	= x6;
		s7	= x7;
		s8	= x8  * K::cospi_4_64  + x9  * K::cospi_28_64;
		s9	= x8  * K::cospi_28_64 - x9  * K::cospi_4_64;
		s10	= x10 * K::cospi_20_64 + x11 * K::cospi_12_64;
		s11	= x10 * K::cospi_12_64 - x11 * K::cospi_20_64;
		s12	= x13 * K::cospi_4_64  - x12 * K::cospi_28_64;
		s13	= x12 * K::cospi_4_64  + x13 * K::cospi_28_64;
		s14	= x15 * K::cospi_20_64 - x14 * K::cospi_12_64;
		s15	= x14 * K::cospi_20_64 + x15 * K::cospi_12_64;

		x0	= s0 + s4;
		x1	= s1 + s5;
		x2	= s2 + s6;
		x3	= s3 + s7;
		x4	= s0 - s4;
		x5	= s1 - s5;
		x6	= s2 - s6;
		x7	= s3 - s7;
		x8	= K::round(s8  + s12);
		x9	= K::round(s9  + s13);
		x10	= K::round(s10 + s14);
		x11	= K::round(s11 + s15);
		x12	= K::round(s8  - s12);
		x13	= K::round(s9  - s13);
		x14	= K::round(s10 - s14);
		x15	= K::round(s11 - s15);

		// stage 3
		s0	= x0;
		s1	= x1;
		s2	= x2;
		s3	= x3;
		s4	= x4 * K::cospi_8_64 + x5 * K::cospi_24_64;
		s5	= x4 * K::cospi_24_64 - x5 * K::cospi_8_64;
		s6	= x7 * K::cospi_8_64 - x6 * K::cospi_24_64;
		s7	= x6 * K::cospi_8_64 + x7 * K::cospi_24_64;
		s8	= x8;
		s9	= x9;
		s10	= x10;
		s11	= x11;
		s12	= x12 * K::cospi_8_64 + x13 * K::cospi_24_64;
		s13	= x12 * K::cospi_24_64 - x13 * K::cospi_8_64;
		s14	= x15 * K::cospi_8_64 - x14 * K::cospi_24_64;
		s15	= x14 * K::cospi_8_64 + x15 * K::cospi_24_64;

		x0	= s0 + s2;
		x1	= s1 + s3;
		x2	= s0 - s2;
		x3	= s1 - s3;
		x4	= K::round(s4 + s6);
		x5	= K::round(s5 + s7);
		x6	= K::round(s4 - s6);
		x7	= K::round(s5 - s7);
		x8	= s8 + s10;
		x9	= s9 + s11;
		x10 = s8 - s10;
		x11 = s9 - s11;
		x12 = K::round(s12 + s14);
		x13 = K::round(s13 + s15);
		x14 = K::round(s12 - s14);
		x15 = K::round(s13 - s15);

		// stage 4
		s2	= -K::cospi_16_64 * (x2 + x3);
		s3	= K::cospi_16_64 * (x2 - x3);
		s6	= K::cospi_16_64 * (x6 + x7);
		s7	= K::cospi_16_64 * (-x6 + x7);
		s10	= K::cospi_16_64 * (x10 + x11);
		s11	= K::cospi_16_64 * (-x10 + x11);
		s14	= -K::cospi_16_64 * (x14 + x15);
		s15	= K::cospi_16_64 * (x14 - x15);

		x2	= K::round(s2);
		x3	= K::round(s3);
		x6	= K::round(s6);
		x7	= K::round(s7);
		x10	= K::round(s10);
		x11	= K::round(s11);
		x14	= K::round(s14);
		x15	= K::round(s15);

		output[0]	= x0;
		output[1]	= -x8;
		output[2]	= x12;
		output[3]	= -x4;
		output[4]	= x6;
		output[5]	= x14;
		output[6]	= x10;
		output[7]	= x2;
		output[8]	= x3;
		output[9]	= x11;
		output[10]	= x15;
		output[11]	= x7;
		output[12]	= x5;
		output[13]	= -x13;
		output[14]	= x9;
		output[15]	= -x1;
	}
	static void iadst(const T *input, T *output) {
		T2	x0	= input[15];
		T2	x1	= input[ 0];
		T2	x2	= input[13];
		T2	x3	= input[ 2];
		T2	x4	= input[11];
		T2	x5	= input[ 4];
		T2	x6	= input[ 9];
		T2	x7	= input[ 6];
		T2	x8	= input[ 7];
		T2	x9	= input[ 8];
		T2	x10	= input[ 5];
		T2	x11	= input[10];
		T2	x12	= input[ 3];
		T2	x13	= input[12];
		T2	x14	= input[ 1];
		T2	x15	= input[14];

		if (!(x0 || x1 || x2 || x3 || x4 || x5 || x6 || x7 || x8 || x9 || x10 || x11 || x12 || x13 || x14 || x15)) {
			output[ 0] = output[ 1] = output[ 2] = output[ 3] = output[ 4] = output[ 5] = output[ 6] = output[ 7]
			= output[ 8] = output[ 9] = output[10] = output[11] = output[12] = output[13] = output[14] = output[15] = 0;
			return;
		}

		// stage 1
		T2	s0	= x0  * K::cospi_1_64  + x1  * K::cospi_31_64;
		T2	s1	= x0  * K::cospi_31_64 - x1  * K::cospi_1_64;
		T2	s2	= x2  * K::cospi_5_64  + x3  * K::cospi_27_64;
		T2	s3	= x2  * K::cospi_27_64 - x3  * K::cospi_5_64;
		T2	s4	= x4  * K::cospi_9_64  + x5  * K::cospi_23_64;
		T2	s5	= x4  * K::cospi_23_64 - x5  * K::cospi_9_64;
		T2	s6	= x6  * K::cospi_13_64 + x7  * K::cospi_19_64;
		T2	s7	= x6  * K::cospi_19_64 - x7  * K::cospi_13_64;
		T2	s8	= x8  * K::cospi_17_64 + x9  * K::cospi_15_64;
		T2	s9	= x8  * K::cospi_15_64 - x9  * K::cospi_17_64;
		T2	s10 = x10 * K::cospi_21_64 + x11 * K::cospi_11_64;
		T2	s11 = x10 * K::cospi_11_64 - x11 * K::cospi_21_64;
		T2	s12 = x12 * K::cospi_25_64 + x13 * K::cospi_7_64;
		T2	s13 = x12 * K::cospi_7_64  - x13 * K::cospi_25_64;
		T2	s14 = x14 * K::cospi_29_64 + x15 * K::cospi_3_64;
		T2	s15 = x14 * K::cospi_3_64  - x15 * K::cospi_29_64;

		x0	= K::round(s0 + s8);
		x1	= K::round(s1 + s9);
		x2	= K::round(s2 + s10);
		x3	= K::round(s3 + s11);
		x4	= K::round(s4 + s12);
		x5	= K::round(s5 + s13);
		x6	= K::round(s6 + s14);
		x7	= K::round(s7 + s15);
		x8	= K::round(s0 - s8);
		x9	= K::round(s1 - s9);
		x10 = K::round(s2 - s10);
		x11 = K::round(s3 - s11);
		x12 = K::round(s4 - s12);
		x13 = K::round(s5 - s13);
		x14 = K::round(s6 - s14);
		x15 = K::round(s7 - s15);

		// stage 2
		s0	= x0;
		s1	= x1;
		s2	= x2;
		s3	= x3;
		s4	= x4;
		s5	= x5;
		s6	= x6;
		s7	= x7;
		s8	= x8  * K::cospi_4_64  + x9  * K::cospi_28_64;
		s9	= x8  * K::cospi_28_64 - x9  * K::cospi_4_64;
		s10 = x10 * K::cospi_20_64 + x11 * K::cospi_12_64;
		s11 = x10 * K::cospi_12_64 - x11 * K::cospi_20_64;
		s12 = x13 * K::cospi_4_64  - x12 * K::cospi_28_64;
		s13 = x12 * K::cospi_4_64  + x13 * K::cospi_28_64;
		s14 = x15 * K::cospi_20_64 - x14 * K::cospi_12_64;
		s15 = x14 * K::cospi_20_64 + x15 * K::cospi_12_64;

		x0	= s0 + s4;
		x1	= s1 + s5;
		x2	= s2 + s6;
		x3	= s3 + s7;
		x4	= s0 - s4;
		x5	= s1 - s5;
		x6	= s2 - s6;
		x7	= s3 - s7;
		x8	= K::round(s8  + s12);
		x9	= K::round(s9  + s13);
		x10 = K::round(s10 + s14);
		x11 = K::round(s11 + s15);
		x12 = K::round(s8  - s12);
		x13 = K::round(s9  - s13);
		x14 = K::round(s10 - s14);
		x15 = K::round(s11 - s15);

		// stage 3
		s0	= x0;
		s1	= x1;
		s2	= x2;
		s3	= x3;
		s4	= x4 * K::cospi_8_64  + x5 * K::cospi_24_64;
		s5	= x4 * K::cospi_24_64 - x5 * K::cospi_8_64;
		s6	= x7 * K::cospi_8_64  - x6 * K::cospi_24_64;
		s7	= x6 * K::cospi_8_64  + x7 * K::cospi_24_64;
		s8	= x8;
		s9	= x9;
		s10 = x10;
		s11 = x11;
		s12 = x12 * K::cospi_8_64  + x13 * K::cospi_24_64;
		s13 = x12 * K::cospi_24_64 - x13 * K::cospi_8_64;
		s14 = x15 * K::cospi_8_64  - x14 * K::cospi_24_64;
		s15 = x14 * K::cospi_8_64  + x15 * K::cospi_24_64;

		x0	= s0 + s2;
		x1	= s1 + s3;
		x2	= s0 - s2;
		x3	= s1 - s3;
		x4	= K::round(s4 + s6);
		x5	= K::round(s5 + s7);
		x6	= K::round(s4 - s6);
		x7	= K::round(s5 - s7);
		x8	= s8 + s10;
		x9	= s9 + s11;
		x10 = s8 - s10;
		x11 = s9 - s11;
		x12 = K::round(s12 + s14);
		x13 = K::round(s13 + s15);
		x14 = K::round(s12 - s14);
		x15 = K::round(s13 - s15);

		// stage 4
		s2	= -K::cospi_16_64 * (x2 + x3);
		s3	=  K::cospi_16_64 * (x2 - x3);
		s6	=  K::cospi_16_64 * (x6 + x7);
		s7	=  K::cospi_16_64 * (-x6 + x7);
		s10 =  K::cospi_16_64 * (x10 + x11);
		s11 =  K::cospi_16_64 * (-x10 + x11);
		s14 = -K::cospi_16_64 * (x14 + x15);
		s15 =  K::cospi_16_64 * (x14 - x15);

		x2	= K::round(s2);
		x3	= K::round(s3);
		x6	= K::round(s6);
		x7	= K::round(s7);
		x10 = K::round(s10);
		x11 = K::round(s11);
		x14 = K::round(s14);
		x15 = K::round(s15);

		output[ 0]	= x0;
		output[ 1]	= -x8;
		output[ 2]	= x12;
		output[ 3]	= -x4;
		output[ 4]	= x6;
		output[ 5]	= x14;
		output[ 6]	= x10;
		output[ 7]	= x2;
		output[ 8]	= x3;
		output[ 9]	= x11;
		output[10]	= x15;
		output[11]	= x7;
		output[12]	= x5;
		output[13]	= -x13;
		output[14]	= x9;
		output[15]	= -x1;
	}
};

template<typename T, int BITS> struct dct<T, BITS, 32> {
	typedef dct_consts<T, BITS>		K;
	typedef typename K::T2			T2;

	static void fdct(const T *input, T *output) {
		T step1[32], step2[32];
		// Stage 1
		step1[ 0] = input[ 0] + input[31];	// *4 in 1st pass, half_round in 2nd, as is final
		step1[ 1] = input[ 1] + input[30];
		step1[ 2] = input[ 2] + input[29];
		step1[ 3] = input[ 3] + input[28];
		step1[ 4] = input[ 4] + input[27];
		step1[ 5] = input[ 5] + input[26];
		step1[ 6] = input[ 6] + input[25];
		step1[ 7] = input[ 7] + input[24];
		step1[ 8] = input[ 8] + input[23];
		step1[ 9] = input[ 9] + input[22];
		step1[10] = input[10] + input[21];
		step1[11] = input[11] + input[20];
		step1[12] = input[12] + input[19];
		step1[13] = input[13] + input[18];
		step1[14] = input[14] + input[17];
		step1[15] = input[15] + input[16];
		step1[16] = input[15] - input[16];
		step1[17] = input[14] - input[17];
		step1[18] = input[13] - input[18];
		step1[19] = input[12] - input[19];
		step1[20] = input[11] - input[20];
		step1[21] = input[10] - input[21];
		step1[22] = input[ 9] - input[22];
		step1[23] = input[ 8] - input[23];
		step1[24] = input[ 7] - input[24];
		step1[25] = input[ 6] - input[25];
		step1[26] = input[ 5] - input[26];
		step1[27] = input[ 4] - input[27];
		step1[28] = input[ 3] - input[28];
		step1[29] = input[ 2] - input[29];
		step1[30] = input[ 1] - input[30];
		step1[31] = input[ 0] - input[31];

		// Stage 2
		step2[ 0] = step1[ 0] + step1[15];
		step2[ 1] = step1[ 1] + step1[14];
		step2[ 2] = step1[ 2] + step1[13];
		step2[ 3] = step1[ 3] + step1[12];
		step2[ 4] = step1[ 4] + step1[11];
		step2[ 5] = step1[ 5] + step1[10];
		step2[ 6] = step1[ 6] + step1[ 9];
		step2[ 7] = step1[ 7] + step1[ 8];
		step2[ 8] = step1[ 7] - step1[ 8];
		step2[ 9] = step1[ 6] - step1[ 9];
		step2[10] = step1[ 5] - step1[10];
		step2[11] = step1[ 4] - step1[11];
		step2[12] = step1[ 3] - step1[12];
		step2[13] = step1[ 2] - step1[13];
		step2[14] = step1[ 1] - step1[14];
		step2[15] = step1[ 0] - step1[15];

		step2[16] = step1[16];
		step2[17] = step1[17];
		step2[18] = step1[18];
		step2[19] = step1[19];

		step2[20] = K::round((-step1[20] + step1[27]) * K::cospi_16_64);
		step2[21] = K::round((-step1[21] + step1[26]) * K::cospi_16_64);
		step2[22] = K::round((-step1[22] + step1[25]) * K::cospi_16_64);
		step2[23] = K::round((-step1[23] + step1[24]) * K::cospi_16_64);

		step2[24] = K::round((step1[24] + step1[23]) * K::cospi_16_64);
		step2[25] = K::round((step1[25] + step1[22]) * K::cospi_16_64);
		step2[26] = K::round((step1[26] + step1[21]) * K::cospi_16_64);
		step2[27] = K::round((step1[27] + step1[20]) * K::cospi_16_64);

		step2[28] = step1[28];
		step2[29] = step1[29];
		step2[30] = step1[30];
		step2[31] = step1[31];

		// Stage 3
		step1[ 0] = step2[ 0] + step2[ 7];
		step1[ 1] = step2[ 1] + step2[ 6];
		step1[ 2] = step2[ 2] + step2[ 5];
		step1[ 3] = step2[ 3] + step2[ 4];
		step1[ 4] = step2[ 3] - step2[ 4];
		step1[ 5] = step2[ 2] - step2[ 5];
		step1[ 6] = step2[ 1] - step2[ 6];
		step1[ 7] = step2[ 0] - step2[ 7];
		step1[ 8] = step2[ 8];
		step1[ 9] = step2[ 9];
		step1[10] = K::round((-step2[10] + step2[13]) * K::cospi_16_64);
		step1[11] = K::round((-step2[11] + step2[12]) * K::cospi_16_64);
		step1[12] = K::round((step2[12] + step2[11]) * K::cospi_16_64);
		step1[13] = K::round((step2[13] + step2[10]) * K::cospi_16_64);
		step1[14] = step2[14];
		step1[15] = step2[15];

		step1[16] = step2[16] + step2[23];
		step1[17] = step2[17] + step2[22];
		step1[18] = step2[18] + step2[21];
		step1[19] = step2[19] + step2[20];
		step1[20] = step2[19] - step2[20];
		step1[21] = step2[18] - step2[21];
		step1[22] = step2[17] - step2[22];
		step1[23] = step2[16] - step2[23];
		step1[24] = step2[31] - step2[24];
		step1[25] = step2[30] - step2[25];
		step1[26] = step2[29] - step2[26];
		step1[27] = step2[28] - step2[27];
		step1[28] = step2[28] + step2[27];
		step1[29] = step2[29] + step2[26];
		step1[30] = step2[30] + step2[25];
		step1[31] = step2[31] + step2[24];

		// Stage 4
		step2[ 0] = step1[ 0] + step1[ 3];
		step2[ 1] = step1[ 1] + step1[ 2];
		step2[ 2] = step1[ 1] - step1[ 2];
		step2[ 3] = step1[ 0] - step1[ 3];
		step2[ 4] = step1[ 4];
		step2[ 5] = K::round((step1[ 6] - step1[ 5]) * K::cospi_16_64);
		step2[ 6] = K::round((step1[ 6] + step1[ 5]) * K::cospi_16_64);
		step2[ 7] = step1[ 7];
		step2[ 8] = step1[ 8] + step1[11];
		step2[ 9] = step1[ 9] + step1[10];
		step2[10] = step1[ 9] - step1[10];
		step2[11] = step1[ 8] - step1[11];
		step2[12] = step1[15] - step1[12];
		step2[13] = step1[14] - step1[13];
		step2[14] = step1[14] + step1[13];
		step2[15] = step1[15] + step1[12];

		step2[16] = step1[16];
		step2[17] = step1[17];
		step2[18] = K::round(step1[18] * -K::cospi_8_64  + step1[29] * K::cospi_24_64);
		step2[19] = K::round(step1[19] * -K::cospi_8_64  + step1[28] * K::cospi_24_64);
		step2[20] = K::round(step1[20] * -K::cospi_24_64 - step1[27] * K::cospi_8_64);
		step2[21] = K::round(step1[21] * -K::cospi_24_64 - step1[26] * K::cospi_8_64);
		step2[22] = step1[22];
		step2[23] = step1[23];
		step2[24] = step1[24];
		step2[25] = step1[25];
		step2[26] = K::round(step1[26] * K::cospi_24_64 - step1[21] * K::cospi_8_64);
		step2[27] = K::round(step1[27] * K::cospi_24_64 - step1[20] * K::cospi_8_64);
		step2[28] = K::round(step1[28] * K::cospi_8_64  + step1[19] * K::cospi_24_64);
		step2[29] = K::round(step1[29] * K::cospi_8_64  + step1[18] * K::cospi_24_64);
		step2[30] = step1[30];
		step2[31] = step1[31];

		// Stage 5
		step1[ 0] = K::round((step2[ 0] + step2[ 1]) * K::cospi_16_64);
		step1[ 1] = K::round((step2[ 0] - step2[ 1]) * K::cospi_16_64);
		step1[ 2] = K::round(step2[ 2] * K::cospi_24_64 + step2[ 3] * K::cospi_8_64);
		step1[ 3] = K::round(step2[ 3] * K::cospi_24_64 - step2[ 2] * K::cospi_8_64);
		step1[ 4] = step2[ 4] + step2[ 5];
		step1[ 5] = step2[ 4] - step2[ 5];
		step1[ 6] = step2[ 7] - step2[ 6];
		step1[ 7] = step2[ 7] + step2[ 6];
		step1[ 8] = step2[ 8];
		step1[ 9] = K::round(step2[ 9] * -K::cospi_8_64  + step2[14] * K::cospi_24_64);
		step1[10] = K::round(step2[10] * -K::cospi_24_64 - step2[13] * K::cospi_8_64);
		step1[11] = step2[11];
		step1[12] = step2[12];
		step1[13] = K::round(step2[13] * K::cospi_24_64 - step2[10] * K::cospi_8_64);
		step1[14] = K::round(step2[14] * K::cospi_8_64  + step2[ 9] * K::cospi_24_64);
		step1[15] = step2[15];

		step1[16] = step2[16] + step2[19];
		step1[17] = step2[17] + step2[18];
		step1[18] = step2[17] - step2[18];
		step1[19] = step2[16] - step2[19];
		step1[20] = step2[23] - step2[20];
		step1[21] = step2[22] - step2[21];
		step1[22] = step2[22] + step2[21];
		step1[23] = step2[23] + step2[20];
		step1[24] = step2[24] + step2[27];
		step1[25] = step2[25] + step2[26];
		step1[26] = step2[25] - step2[26];
		step1[27] = step2[24] - step2[27];
		step1[28] = step2[31] - step2[28];
		step1[29] = step2[30] - step2[29];
		step1[30] = step2[30] + step2[29];
		step1[31] = step2[31] + step2[28];

		// Stage 6
		step2[ 0] = step1[ 0];
		step2[ 1] = step1[ 1];
		step2[ 2] = step1[ 2];
		step2[ 3] = step1[ 3];
		step2[ 4] = K::round(step1[ 4] * K::cospi_28_64 + step1[ 7] * K::cospi_4_64);
		step2[ 5] = K::round(step1[ 5] * K::cospi_12_64 + step1[ 6] * K::cospi_20_64);
		step2[ 6] = K::round(step1[ 6] * K::cospi_12_64 + step1[ 5] * -K::cospi_20_64);
		step2[ 7] = K::round(step1[ 7] * K::cospi_28_64 + step1[ 4] * -K::cospi_4_64);
		step2[ 8] = step1[ 8] + step1[ 9];
		step2[ 9] = step1[ 8] - step1[ 9];
		step2[10] = step1[11] - step1[10];
		step2[11] = step1[11] + step1[10];
		step2[12] = step1[12] + step1[13];
		step2[13] = step1[12] - step1[13];
		step2[14] = step1[15] - step1[14];
		step2[15] = step1[15] + step1[14];

		step2[16] = step1[16];
		step2[17] = K::round(step1[17] * -K::cospi_4_64  + step1[30] * K::cospi_28_64);
		step2[18] = K::round(step1[18] * -K::cospi_28_64 - step1[29] * K::cospi_4_64);
		step2[19] = step1[19];
		step2[20] = step1[20];
		step2[21] = K::round(step1[21] * -K::cospi_20_64 + step1[26] * K::cospi_12_64);
		step2[22] = K::round(step1[22] * -K::cospi_12_64 - step1[25] * K::cospi_20_64);
		step2[23] = step1[23];
		step2[24] = step1[24];
		step2[25] = K::round(step1[25] * K::cospi_12_64 - step1[22] * K::cospi_20_64);
		step2[26] = K::round(step1[26] * K::cospi_20_64 + step1[21] * K::cospi_12_64);
		step2[27] = step1[27];
		step2[28] = step1[28];
		step2[29] = K::round(step1[29] * K::cospi_28_64 - step1[18] * K::cospi_4_64);
		step2[30] = K::round(step1[30] * K::cospi_4_64  + step1[17] * K::cospi_28_64);
		step2[31] = step1[31];

		// Stage 7
		step1[ 0] = step2[ 0];
		step1[ 1] = step2[ 1];
		step1[ 2] = step2[ 2];
		step1[ 3] = step2[ 3];
		step1[ 4] = step2[ 4];
		step1[ 5] = step2[ 5];
		step1[ 6] = step2[ 6];
		step1[ 7] = step2[ 7];
		step1[ 8] = K::round(step2[ 8] * K::cospi_30_64 + step2[15] * K::cospi_2_64);
		step1[ 9] = K::round(step2[ 9] * K::cospi_14_64 + step2[14] * K::cospi_18_64);
		step1[10] = K::round(step2[10] * K::cospi_22_64 + step2[13] * K::cospi_10_64);
		step1[11] = K::round(step2[11] * K::cospi_6_64  + step2[12] * K::cospi_26_64);
		step1[12] = K::round(step2[12] * K::cospi_6_64  - step2[11] * K::cospi_26_64);
		step1[13] = K::round(step2[13] * K::cospi_22_64 - step2[10] * K::cospi_10_64);
		step1[14] = K::round(step2[14] * K::cospi_14_64 - step2[ 9] * K::cospi_18_64);
		step1[15] = K::round(step2[15] * K::cospi_30_64 - step2[ 8] * K::cospi_2_64);

		step1[16] = step2[16] + step2[17];
		step1[17] = step2[16] - step2[17];
		step1[18] = step2[19] - step2[18];
		step1[19] = step2[19] + step2[18];
		step1[20] = step2[20] + step2[21];
		step1[21] = step2[20] - step2[21];
		step1[22] = step2[23] - step2[22];
		step1[23] = step2[23] + step2[22];
		step1[24] = step2[24] + step2[25];
		step1[25] = step2[24] - step2[25];
		step1[26] = step2[27] - step2[26];
		step1[27] = step2[27] + step2[26];
		step1[28] = step2[28] + step2[29];
		step1[29] = step2[28] - step2[29];
		step1[30] = step2[31] - step2[30];
		step1[31] = step2[31] + step2[30];

		// Final stage --- outputs indices are bit-reversed.
		output[ 0] = step1[ 0];
		output[16] = step1[ 1];
		output[ 8] = step1[ 2];
		output[24] = step1[ 3];
		output[ 4] = step1[ 4];
		output[20] = step1[ 5];
		output[12] = step1[ 6];
		output[28] = step1[ 7];
		output[ 2] = step1[ 8];
		output[18] = step1[ 9];
		output[10] = step1[10];
		output[26] = step1[11];
		output[ 6] = step1[12];
		output[22] = step1[13];
		output[14] = step1[14];
		output[30] = step1[15];

		output[ 1] = K::round(step1[16] * K::cospi_31_64 + step1[31] * K::cospi_1_64);
		output[17] = K::round(step1[17] * K::cospi_15_64 + step1[30] * K::cospi_17_64);
		output[ 9] = K::round(step1[18] * K::cospi_23_64 + step1[29] * K::cospi_9_64);
		output[25] = K::round(step1[19] * K::cospi_7_64  + step1[28] * K::cospi_25_64);
		output[ 5] = K::round(step1[20] * K::cospi_27_64 + step1[27] * K::cospi_5_64);
		output[21] = K::round(step1[21] * K::cospi_11_64 + step1[26] * K::cospi_21_64);
		output[13] = K::round(step1[22] * K::cospi_19_64 + step1[25] * K::cospi_13_64);
		output[29] = K::round(step1[23] * K::cospi_3_64  + step1[24] * K::cospi_29_64);
		output[ 3] = K::round(step1[24] * K::cospi_3_64  - step1[23] * K::cospi_29_64);
		output[19] = K::round(step1[25] * K::cospi_19_64 - step1[22] * K::cospi_13_64);
		output[11] = K::round(step1[26] * K::cospi_11_64 - step1[21] * K::cospi_21_64);
		output[27] = K::round(step1[27] * K::cospi_27_64 - step1[20] * K::cospi_5_64);
		output[ 7] = K::round(step1[28] * K::cospi_7_64  - step1[19] * K::cospi_25_64);
		output[23] = K::round(step1[29] * K::cospi_23_64 - step1[18] * K::cospi_9_64);
		output[15] = K::round(step1[30] * K::cospi_15_64 - step1[17] * K::cospi_17_64);
		output[31] = K::round(step1[31] * K::cospi_31_64 - step1[16] * K::cospi_1_64);
	}

	static void idct(const T *input, T *output) {
		T	step1[32], step2[32];

		// stage 1
		step1[ 0]	= input[ 0];
		step1[ 1]	= input[16];
		step1[ 2]	= input[ 8];
		step1[ 3]	= input[24];
		step1[ 4]	= input[ 4];
		step1[ 5]	= input[20];
		step1[ 6]	= input[12];
		step1[ 7]	= input[28];
		step1[ 8]	= input[ 2];
		step1[ 9]	= input[18];
		step1[10]	= input[10];
		step1[11]	= input[26];
		step1[12]	= input[ 6];
		step1[13]	= input[22];
		step1[14]	= input[14];
		step1[15]	= input[30];

		step1[16]	= K::round(input[ 1] * K::cospi_31_64 - input[31] * K::cospi_1_64);
		step1[31]	= K::round(input[ 1] * K::cospi_1_64  + input[31] * K::cospi_31_64);
		step1[17]	= K::round(input[17] * K::cospi_15_64 - input[15] * K::cospi_17_64);
		step1[30]	= K::round(input[17] * K::cospi_17_64 + input[15] * K::cospi_15_64);
		step1[18]	= K::round(input[ 9] * K::cospi_23_64 - input[23] * K::cospi_9_64);
		step1[29]	= K::round(input[ 9] * K::cospi_9_64  + input[23] * K::cospi_23_64);
		step1[19]	= K::round(input[25] * K::cospi_7_64  - input[ 7] * K::cospi_25_64);
		step1[28]	= K::round(input[25] * K::cospi_25_64 + input[ 7] * K::cospi_7_64);
		step1[20]	= K::round(input[ 5] * K::cospi_27_64 - input[27] * K::cospi_5_64);
		step1[27]	= K::round(input[ 5] * K::cospi_5_64  + input[27] * K::cospi_27_64);
		step1[21]	= K::round(input[21] * K::cospi_11_64 - input[11] * K::cospi_21_64);
		step1[26]	= K::round(input[21] * K::cospi_21_64 + input[11] * K::cospi_11_64);
		step1[22]	= K::round(input[13] * K::cospi_19_64 - input[19] * K::cospi_13_64);
		step1[25]	= K::round(input[13] * K::cospi_13_64 + input[19] * K::cospi_19_64);
		step1[23]	= K::round(input[29] * K::cospi_3_64  - input[ 3] * K::cospi_29_64);
		step1[24]	= K::round(input[29] * K::cospi_29_64 + input[ 3] * K::cospi_3_64);

		// stage 2
		step2[ 0]	= step1[ 0];
		step2[ 1]	= step1[ 1];
		step2[ 2]	= step1[ 2];
		step2[ 3]	= step1[ 3];
		step2[ 4]	= step1[ 4];
		step2[ 5]	= step1[ 5];
		step2[ 6]	= step1[ 6];
		step2[ 7]	= step1[ 7];
		step2[ 8]	= K::round(step1[ 8] * K::cospi_30_64 - step1[15] * K::cospi_2_64);
		step2[15]	= K::round(step1[ 8] * K::cospi_2_64  + step1[15] * K::cospi_30_64);
		step2[ 9]	= K::round(step1[ 9] * K::cospi_14_64 - step1[14] * K::cospi_18_64);
		step2[14]	= K::round(step1[ 9] * K::cospi_18_64 + step1[14] * K::cospi_14_64);
		step2[10]	= K::round(step1[10] * K::cospi_22_64 - step1[13] * K::cospi_10_64);
		step2[13]	= K::round(step1[10] * K::cospi_10_64 + step1[13] * K::cospi_22_64);
		step2[11]	= K::round(step1[11] * K::cospi_6_64  - step1[12] * K::cospi_26_64);
		step2[12]	= K::round(step1[11] * K::cospi_26_64 + step1[12] * K::cospi_6_64);

		step2[16]	= step1[16] + step1[17];
		step2[17]	= step1[16] - step1[17];
		step2[18]	= step1[19] - step1[18];
		step2[19]	= step1[18] + step1[19];
		step2[20]	= step1[20] + step1[21];
		step2[21]	= step1[20] - step1[21];
		step2[22]	= step1[23] - step1[22];
		step2[23]	= step1[22] + step1[23];
		step2[24]	= step1[24] + step1[25];
		step2[25]	= step1[24] - step1[25];
		step2[26]	= step1[27] - step1[26];
		step2[27]	= step1[26] + step1[27];
		step2[28]	= step1[28] + step1[29];
		step2[29]	= step1[28] - step1[29];
		step2[30]	= step1[31] - step1[30];
		step2[31]	= step1[30] + step1[31];

		// stage 3
		step1[ 0]	= step2[ 0];
		step1[ 1]	= step2[ 1];
		step1[ 2]	= step2[ 2];
		step1[ 3]	= step2[ 3];
		step1[ 4]	= K::round(step2[ 4] * K::cospi_28_64 - step2[ 7] * K::cospi_4_64);
		step1[ 5]	= K::round(step2[ 5] * K::cospi_12_64 - step2[ 6] * K::cospi_20_64);
		step1[ 6]	= K::round(step2[ 5] * K::cospi_20_64 + step2[ 6] * K::cospi_12_64);
		step1[ 7]	= K::round(step2[ 4] * K::cospi_4_64  + step2[ 7] * K::cospi_28_64);
		step1[ 8]	= step2[ 8] + step2[ 9];
		step1[ 9]	= step2[ 8] - step2[ 9];
		step1[10]	= step2[11] - step2[10];
		step1[11]	= step2[10] + step2[11];
		step1[12]	= step2[12] + step2[13];
		step1[13]	= step2[12] - step2[13];
		step1[14]	= step2[15] - step2[14];
		step1[15]	= step2[14] + step2[15];

		step1[16]	= step2[16];
		step1[31]	= step2[31];
		step1[17]	= K::round(-step2[17] * K::cospi_4_64  + step2[30] * K::cospi_28_64);
		step1[30]	= K::round( step2[17] * K::cospi_28_64 + step2[30] * K::cospi_4_64);
		step1[18]	= K::round(-step2[18] * K::cospi_28_64 - step2[29] * K::cospi_4_64);
		step1[29]	= K::round(-step2[18] * K::cospi_4_64  + step2[29] * K::cospi_28_64);
		step1[19]	= step2[19];
		step1[20]	= step2[20];
		step1[21]	= K::round(-step2[21] * K::cospi_20_64 + step2[26] * K::cospi_12_64);
		step1[26]	= K::round( step2[21] * K::cospi_12_64 + step2[26] * K::cospi_20_64);
		step1[22]	= K::round(-step2[22] * K::cospi_12_64 - step2[25] * K::cospi_20_64);
		step1[25]	= K::round(-step2[22] * K::cospi_20_64 + step2[25] * K::cospi_12_64);
		step1[23]	= step2[23];
		step1[24]	= step2[24];
		step1[27]	= step2[27];
		step1[28]	= step2[28];

		// stage 4
		step2[ 0]	= K::round((step1[ 0] + step1[ 1]) * K::cospi_16_64);
		step2[ 1]	= K::round((step1[ 0] - step1[ 1]) * K::cospi_16_64);
		step2[ 2]	= K::round(step1[ 2] * K::cospi_24_64 - step1[ 3] * K::cospi_8_64);
		step2[ 3]	= K::round(step1[ 2] * K::cospi_8_64  + step1[ 3] * K::cospi_24_64);
		step2[ 4]	= step1[ 4] + step1[ 5];
		step2[ 5]	= step1[ 4] - step1[ 5];
		step2[ 6]	= step1[ 7] - step1[ 6];
		step2[ 7]	= step1[ 6] + step1[ 7];
		step2[ 8]	= step1[ 8];
		step2[15]	= step1[15];
		step2[ 9]	= K::round(-step1[ 9] * K::cospi_8_64  + step1[14] * K::cospi_24_64);
		step2[14]	= K::round( step1[ 9] * K::cospi_24_64 + step1[14] * K::cospi_8_64);
		step2[10]	= K::round(-step1[10] * K::cospi_24_64 - step1[13] * K::cospi_8_64);
		step2[13]	= K::round(-step1[10] * K::cospi_8_64  + step1[13] * K::cospi_24_64);
		step2[11]	= step1[11];
		step2[12]	= step1[12];

		step2[16]	= step1[16] + step1[19];
		step2[17]	= step1[17] + step1[18];
		step2[18]	= step1[17] - step1[18];
		step2[19]	= step1[16] - step1[19];
		step2[20]	= step1[23] - step1[20];
		step2[21]	= step1[22] - step1[21];
		step2[22]	= step1[21] + step1[22];
		step2[23]	= step1[20] + step1[23];
		step2[24]	= step1[24] + step1[27];
		step2[25]	= step1[25] + step1[26];
		step2[26]	= step1[25] - step1[26];
		step2[27]	= step1[24] - step1[27];
		step2[28]	= step1[31] - step1[28];
		step2[29]	= step1[30] - step1[29];
		step2[30]	= step1[29] + step1[30];
		step2[31]	= step1[28] + step1[31];

		// stage 5
		step1[ 0]	= step2[ 0] + step2[ 3];
		step1[ 1]	= step2[ 1] + step2[ 2];
		step1[ 2]	= step2[ 1] - step2[ 2];
		step1[ 3]	= step2[ 0] - step2[ 3];
		step1[ 4]	= step2[ 4];
		step1[ 5]	= K::round((step2[ 6] - step2[ 5]) * K::cospi_16_64);
		step1[ 6]	= K::round((step2[ 5] + step2[ 6]) * K::cospi_16_64);
		step1[ 7]	= step2[ 7];
		step1[ 8]	= step2[ 8] + step2[11];
		step1[ 9]	= step2[ 9] + step2[10];
		step1[10]	= step2[ 9] - step2[10];
		step1[11]	= step2[ 8] - step2[11];
		step1[12]	= step2[15] - step2[12];
		step1[13]	= step2[14] - step2[13];
		step1[14]	= step2[13] + step2[14];
		step1[15]	= step2[12] + step2[15];

		step1[16]	= step2[16];
		step1[17]	= step2[17];
		step1[18]	= K::round(-step2[18] * K::cospi_8_64  + step2[29] * K::cospi_24_64);
		step1[29]	= K::round( step2[18] * K::cospi_24_64 + step2[29] * K::cospi_8_64);
		step1[19]	= K::round(-step2[19] * K::cospi_8_64  + step2[28] * K::cospi_24_64);
		step1[28]	= K::round( step2[19] * K::cospi_24_64 + step2[28] * K::cospi_8_64);
		step1[20]	= K::round(-step2[20] * K::cospi_24_64 - step2[27] * K::cospi_8_64);
		step1[27]	= K::round(-step2[20] * K::cospi_8_64  + step2[27] * K::cospi_24_64);
		step1[21]	= K::round(-step2[21] * K::cospi_24_64 - step2[26] * K::cospi_8_64);
		step1[26]	= K::round(-step2[21] * K::cospi_8_64  + step2[26] * K::cospi_24_64);
		step1[22]	= step2[22];
		step1[23]	= step2[23];
		step1[24]	= step2[24];
		step1[25]	= step2[25];
		step1[30]	= step2[30];
		step1[31]	= step2[31];

		// stage 6
		step2[ 0]	= step1[ 0] + step1[ 7];
		step2[ 1]	= step1[ 1] + step1[ 6];
		step2[ 2]	= step1[ 2] + step1[ 5];
		step2[ 3]	= step1[ 3] + step1[ 4];
		step2[ 4]	= step1[ 3] - step1[ 4];
		step2[ 5]	= step1[ 2] - step1[ 5];
		step2[ 6]	= step1[ 1] - step1[ 6];
		step2[ 7]	= step1[ 0] - step1[ 7];
		step2[ 8]	= step1[ 8];
		step2[ 9]	= step1[ 9];
		step2[10]	= K::round((-step1[10] + step1[13]) * K::cospi_16_64);
		step2[13]	= K::round(( step1[10] + step1[13]) * K::cospi_16_64);
		step2[11]	= K::round((-step1[11] + step1[12]) * K::cospi_16_64);
		step2[12]	= K::round(( step1[11] + step1[12]) * K::cospi_16_64);
		step2[14]	= step1[14];
		step2[15]	= step1[15];

		step2[16]	= step1[16] + step1[23];
		step2[17]	= step1[17] + step1[22];
		step2[18]	= step1[18] + step1[21];
		step2[19]	= step1[19] + step1[20];
		step2[20]	= step1[19] - step1[20];
		step2[21]	= step1[18] - step1[21];
		step2[22]	= step1[17] - step1[22];
		step2[23]	= step1[16] - step1[23];

		step2[24]	= step1[31] - step1[24];
		step2[25]	= step1[30] - step1[25];
		step2[26]	= step1[29] - step1[26];
		step2[27]	= step1[28] - step1[27];
		step2[28]	= step1[27] + step1[28];
		step2[29]	= step1[26] + step1[29];
		step2[30]	= step1[25] + step1[30];
		step2[31]	= step1[24] + step1[31];

		// stage 7
		step1[ 0]	= step2[ 0] + step2[15];
		step1[ 1]	= step2[ 1] + step2[14];
		step1[ 2]	= step2[ 2] + step2[13];
		step1[ 3]	= step2[ 3] + step2[12];
		step1[ 4]	= step2[ 4] + step2[11];
		step1[ 5]	= step2[ 5] + step2[10];
		step1[ 6]	= step2[ 6] + step2[ 9];
		step1[ 7]	= step2[ 7] + step2[ 8];
		step1[ 8]	= step2[ 7] - step2[ 8];
		step1[ 9]	= step2[ 6] - step2[ 9];
		step1[10]	= step2[ 5] - step2[10];
		step1[11]	= step2[ 4] - step2[11];
		step1[12]	= step2[ 3] - step2[12];
		step1[13]	= step2[ 2] - step2[13];
		step1[14]	= step2[ 1] - step2[14];
		step1[15]	= step2[ 0] - step2[15];

		step1[16]	= step2[16];
		step1[17]	= step2[17];
		step1[18]	= step2[18];
		step1[19]	= step2[19];
		step1[20]	= K::round((-step2[20] + step2[27]) * K::cospi_16_64);
		step1[27]	= K::round(( step2[20] + step2[27]) * K::cospi_16_64);
		step1[21]	= K::round((-step2[21] + step2[26]) * K::cospi_16_64);
		step1[26]	= K::round(( step2[21] + step2[26]) * K::cospi_16_64);
		step1[22]	= K::round((-step2[22] + step2[25]) * K::cospi_16_64);
		step1[25]	= K::round(( step2[22] + step2[25]) * K::cospi_16_64);
		step1[23]	= K::round((-step2[23] + step2[24]) * K::cospi_16_64);
		step1[24]	= K::round(( step2[23] + step2[24]) * K::cospi_16_64);
		step1[28]	= step2[28];
		step1[29]	= step2[29];
		step1[30]	= step2[30];
		step1[31]	= step2[31];

		// final stage
		output[ 0]	= step1[ 0] + step1[31];
		output[ 1]	= step1[ 1] + step1[30];
		output[ 2]	= step1[ 2] + step1[29];
		output[ 3]	= step1[ 3] + step1[28];
		output[ 4]	= step1[ 4] + step1[27];
		output[ 5]	= step1[ 5] + step1[26];
		output[ 6]	= step1[ 6] + step1[25];
		output[ 7]	= step1[ 7] + step1[24];
		output[ 8]	= step1[ 8] + step1[23];
		output[ 9]	= step1[ 9] + step1[22];
		output[10]	= step1[10] + step1[21];
		output[11]	= step1[11] + step1[20];
		output[12]	= step1[12] + step1[19];
		output[13]	= step1[13] + step1[18];
		output[14]	= step1[14] + step1[17];
		output[15]	= step1[15] + step1[16];
		output[16]	= step1[15] - step1[16];
		output[17]	= step1[14] - step1[17];
		output[18]	= step1[13] - step1[18];
		output[19]	= step1[12] - step1[19];
		output[20]	= step1[11] - step1[20];
		output[21]	= step1[10] - step1[21];
		output[22]	= step1[ 9] - step1[22];
		output[23]	= step1[ 8] - step1[23];
		output[24]	= step1[ 7] - step1[24];
		output[25]	= step1[ 6] - step1[25];
		output[26]	= step1[ 5] - step1[26];
		output[27]	= step1[ 4] - step1[27];
		output[28]	= step1[ 3] - step1[28];
		output[29]	= step1[ 2] - step1[29];
		output[30]	= step1[ 1] - step1[30];
		output[31]	= step1[ 0] - step1[31];
	}
	static void fadst(const T *input, T *output) {}	// not implemented
	static void iadst(const T *input, T *output) {}	// not implemented
};

//in-place 8x8 for jpeg/mpeg
void jpeg_fdct(int16 *block8x8);
void jpeg_idct(int16 *block8x8);

template<typename T> void do_fdct(T *in, T *out, int n) {
	dct<T, 0, 32>::fdct(in, out);
};

} // namespace iso
#endif	//DCT_H
