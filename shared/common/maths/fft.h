#ifndef FFT_H
#define FFT_H

#include "base/bits.h"
#include "base/maths.h"

namespace iso {

//-----------------------------------------------------------------------------
// converters
//-----------------------------------------------------------------------------

template<typename T> void real_post_fft(const complex<T>* z, int N, complex<T>* out, const complex<T> *twiddles = 0) {
	auto	a	=  z[0];
	out[0]		= make_complex(a.r + a.i);
	out[N / 2]	= make_complex(a.r - a.i);

	for (int k = 1; k < N / 2; k++) {
		auto	w	= twiddles ? twiddles[k] : exp(-two * pi * i * k / N);
		auto	a	=  z[k];
		auto	b	= ~z[N / 2 - k];
		auto	Ze	= a + b;
		auto	Zo	= i * (b - a);
		auto	Z	= (Ze + Zo * w) * half;
		out[k]		=  Z;
		out[N - k]	= ~Z;
	}
}

template<typename T> void real_pre_ifft(const complex<T>* z, int N, complex<T>* out, const complex<T> *twiddles = 0) {
	auto	a	=  z[0];
	out[0]		= a;
	out[N / 2]	= make_complex(a.r - a.i);

	for (int k = 1; k < N / 4; ++k) {
		auto	w	= twiddles ? twiddles[k] : exp(-two * T(pi) * i * k / N);
		auto	a	= z[k];
		auto	b	= ~z[N / 2 - k];
		auto	Ze	= a + b;
		auto	Zo	= i * (a - b);
		out[k]			= (Ze + Zo * w) * half;
		out[N / 2 - k]	= (Ze + Zo * ~w) * half;
	}
}


template<typename T> void post_dctII(const complex<T>* z, int N, T *out, const complex<T> *twiddles = 0) {
	out[0]		= z[0].r		* two;
	out[N / 2]	= z[N / 2].r	* sqrt2;

	for (int k = 1; k < N / 2; k++) {
		auto	w	= twiddles ? twiddles[k] : exp(pi * i * k / (N * 2));
		auto	Z	= z[k] * w * two;
		out[k]		=  Z.r;
		out[N - k]	= -Z.i;
	}
}

template<typename T> void pre_dctIII(const T* x, int N, complex<T> *out, const complex<T> *twiddles = 0) {
	out[0]		= make_complex(x[0]);
	out[N / 2]	= make_complex(x[N / 2] * sqrt2);
	for (int k = 0; k < N / 2; k++) {
		auto	w	= twiddles ? twiddles[k] : exp(pi * i * k / (2 * N));
		auto	Z	= make_complex(x[k], -x[N - k]) * w * 2;
		out[k]		=  Z;
		out[N - k]	= ~Z;
	}
}


//-----------------------------------------------------------------------------
// FFT for power of 2 sizes
// Cooley-Tukey
//-----------------------------------------------------------------------------

template<typename T> void fft_swizzled(complex<T>* X, int N) {
	for (int m = 1; m < N; m <<= 1) {
		//j == 0 special case:
		for (int k = 0; k < N; k += m * 2) {
			auto	t	= X[k + m];
			auto	u	= X[k];
			X[k]		= u + t;
			X[k + m]	= u - t;
		}

		complex<T>	wm	= exp(-pi * i / m);
		complex<T>	w	= wm;
		for (int j = 1; j < m; j++) {
		//	complex<T>	w	= exp(imaginary<T>(-pi * i * j / m));	// 'twiddle-factor'
			for (int k = j; k < N; k += m * 2) {
				auto	t	= w * X[k + m];
				auto	u	= X[k];
				X[k]		= u + t;
				X[k + m]	= u - t;
			}
			w = w * wm;
		}
	}
}

template<typename T> void fft_swizzled(complex<T>* X, int N, complex<T> *twiddles) {
	for (int m = 1; m < N; m <<= 1) {
		//j == 0 special case:
		for (int k = 0; k < N; k += m * 2) {
			auto	t	= X[k + m];
			auto	u	= X[k];
			X[k]		= u + t;
			X[k + m]	= u - t;
		}

		for (int j = 1; j < m; j++) {
			auto	w = twiddles[j];
			for (int k = j; k < N; k += m * 2) {
				auto	t	= w * X[k + m];
				auto	u	= X[k];
				X[k]		= u + t;
				X[k + m]	= u - t;
			}
		}
	}
}

template<typename T> void fft(complex<T>* X, int N) {
	const int	bits = log2(N);
	for (int k = 0; i < N; k++) {
		int	j = reverse_bits(k, bits);
		if (k < j)
			swap(X[k], X[j]);
	}

	fft_swizzled(X, N);
}

template<typename T> void fft(T* X, int N, complex<T>* out) {
	auto	z		= alloc_auto(complex<T>, N / 2);
	int		bits	= log2(N / 2);
	for (int k = 0; k < N / 2; k++)
		z[reverse_bits(k, bits)] = make_complex(X[k * 2 + 0], X[k * 2 + 1]);

	fft_swizzled(z, N / 2);
	real_post_fft(z, N, out);
}

template<typename T> void dctII(T* x, int N, T *out) {
	auto	*t = alloc_auto(T, N);
	for (int k = 0; k < N / 2; k++) {
		t[k]			= x[k * 2 + 0];
		t[N - 1 - k]	= x[k * 2 + 1];
	}
	auto	*z = alloc_auto(complex<T>, N);
	fft(t, N, z);
	post_dctII(z, N, out);
}

template<typename T> void dctIII(T* x, int N, complex<T> *out) {
	auto	*z	= alloc_auto(complex<T>, N);
	pre_dctIII(x, N, z);

	auto	*t	= alloc_auto(complex<T>, N);
	fft(z, N, t);
	for (int k = 0; k < N / 2; k++) {
		out[k * 2 + 0] = ~t[k];
		out[k * 2 + 1] = ~t[N - 1 - k];
	}
}

//-----------------------------------------------------------------------------
// conjugate-pair split radix FFT
//-----------------------------------------------------------------------------
/*
template<typename T> void dumb_fft(const complex<T>* x, int N, int offset, int stride, complex<T> *out) {
	if (N < 2) {
		out[0]	= x[offset & (stride - 1)];

	} else {
		dumb_fft(x, N/2, offset, stride * 2, out);					// recurse even items
		dumb_fft(x, N/2, offset + stride, stride * 2, out + N / 2);	// recurse odd  items

		// combine results of two half recursions
		for (int k = 0; k < N / 2; k++) {
			complex<T> w	= exp(imaginary<T>(-pi * two * k / N));// 'twiddle-factor'
			complex<T> e	= out[k];				// even
			complex<T> o	= w * out[k + N / 2];	// odd
			out[k]			= e + o;
			out[k + N / 2]	= e - o;
		}
	}
}

template<typename T> void dumb_fft(const T *x, int N, complex<T> *out) {
	auto	z	= alloc_auto(complex<T>, N);
	for (int i = 0; i < N; i++)
		z[i] = complex<T>(x[i]);
	return dumb_fft(z, N, 0, 1, out);
}
*/
// complex
template<typename T> void _split_fft(const complex<T> *x, int N, int offset, int stride, complex<T> *out) {
	if (N < 2) {
		out[0]	= x[offset & (stride - 1)];

	} else if (N < 4) {
		auto e	= x[offset & (stride * 2 - 1)];				// even
		auto o	= x[(offset + stride) & (stride * 2 - 1)];	// odd
		out[0]	= e + o;
		out[1]	= e - o;

	} else {
		auto	U	= out;
		auto	Z1	= out + N / 2;
		auto	Z2	= out + 3 * N / 4;

		_split_fft(x, N / 2, offset, stride * 2, U);
		_split_fft(x, N / 4, offset + stride, stride * 4, Z1);
		_split_fft(x, N / 4, offset - stride, stride * 4, Z2);

		//k==0 special case
		auto	z1	= Z1[0];
		auto	z2	= Z2[0];
		auto	u0	= U[0];
		auto	u1	= U[0 + N / 4];

		out[0]				= u0 +		(z1 + z2);
		out[0 + N / 2]		= u0 -		(z1 + z2);
		out[0 + N / 4]		= u1 - i *	(z1 - z2);
		out[0 + 3 * N / 4]	= u1 + i *	(z1 - z2);

		complex<T>	wm	= exp(-pi * two * i / N);
		complex<T>	w	= wm;
		for (int k = 1; k < N / 4; k++) {
//			complex<T>	w	= exp(imaginary<T>(-pi * two * k / N));	// 'twiddle-factor'
			auto	z1	=  w * Z1[k];
			auto	z2	= ~w * Z2[k];
			auto	u0	= U[k];
			auto	u1	= U[k + N / 4];

			out[k]				= u0 +		(z1 + z2);
			out[k + N / 2]		= u0 -		(z1 + z2);
			out[k + N / 4]		= u1 - i *	(z1 - z2);
			out[k + 3 * N / 4]	= u1 + i *	(z1 - z2);

			w = w * wm;
		}
	}
}
template<typename T> void split_fft(const complex<T> *x, int N, complex<T> *out) {
	return _split_fft(x, N, 0, 1, out);
}

// real
template<typename T> void _split_fft(const T *x, int N, int offset, int stride, complex<T> *out) {
	if (N < 4) {
		auto	x0	= x[offset & (stride * 2 - 1)];
		auto	x1	= x[(offset + stride) & (stride * 2 - 1)];
		out[0]	= complex<T>(x0 + x1, x0 - x1);

	} else if (N < 8) {
		auto	x0	= x[(offset + stride * 0) & (stride * 4 - 1)];
		auto	x1	= x[(offset + stride * 1) & (stride * 4 - 1)];
		auto	x2	= x[(offset + stride * 2) & (stride * 4 - 1)];
		auto	x3	= x[(offset + stride * 3) & (stride * 4 - 1)];

		out[0]	= complex<T>( (x0 + x1) + (x2 - x3), (x0 + x1) - (x2 - x3));
		out[1]	= complex<T>(-(x0 - x1) - (x2 + x3), (x0 - x1) - (x2 + x3));

	} else {
		auto	U	= out;
		auto	Z1	= out + N / 4;
		auto	Z2	= out + 3 * N / 8;

		_split_fft(x, N / 2, offset, stride * 2, U);
		_split_fft(x, N / 4, offset + stride, stride * 4, Z1);
		_split_fft(x, N / 4, offset - stride, stride * 4, Z2);

		//k==0 special case
		auto	z1	= Z1[0];
		auto	z2	= Z2[0];
		auto	u0	= U[0];
		auto	u1	= U[N / 4];

		out[0]		=  u0 +		 (z1 + z2);
		out[N / 4]	= ~u1 -	i *	 (z1 - z2);

		complex<T>	wm	= exp(-pi * two * i / N);
		complex<T>	w	= wm;
		for (int k = 1; k < N / 8; k++) {
//			complex<T>	w	= exp(imaginary<T>(-pi * two * k / N));	// 'twiddle-factor'
			auto	z1	=  w * Z1[k];
			auto	z2	= ~w * Z2[k];
			auto	u0	= U[k];
			auto	u1	= U[N / 4 - k];

			out[k]			=  u0 +		 (z1 + z2);
			out[k + N / 4]	= ~u1 -	i *	 (z1 - z2);
			out[N / 4 - k]	=  u1 - i *	~(z1 - z2);
			out[N / 2 - k]	= ~u0 -		~(z1 + z2);

			w = w * wm;
		}

		//k==N/8 special case
		w	= exp(imaginary<T>(-pi / 4));
		z1	=  w * z1;
		z2	= ~w * z2;

		out[N / 8]		=  u0 +		 (z1 + z2);
		out[N * 3 / 8]	= ~u0 -	i *	 (z1 - z2);
	}
}

template<typename T> void split_fft(const T *x, int N, complex<T> *out) {
	return _split_fft(x, N, 0, 1, out);
}

#if 1
template<typename T> void split_fft_real(T *X, int N) {
	const int bits = log2(N);
	for (int i = 0; i < N; i++) {
		int j = reverse_bits(i, bits);
		if (i < j)
			swap(X[i], X[j]);
	}

	// length two butterflies
	for (int m = 2; m < N; m <<= 2) {
		for (int k = m - 2; k < N; k += m * 2) {
			auto	x0	= X[k + 0];
			auto	x1	= X[k + 1];
			X[k + 0]	= x0 + x1;
			X[k + 1]	= x0 - x1;
		}
	}

	// L shaped butterflies
	for (int n = 2; n < N; n *= 2) {

		for (int m = n; m < N; m <<= 2) {
			for (int k = (m - n) * 2; k < N; k += m * 4) {
				auto	x0	= X[k + n * 0 / 4];
				auto	x4	= X[k + n * 4 / 4];
				auto	x6	= X[k + n * 6 / 4];
				auto	t0	= x6 + x4;

				X[k + n * 0 / 4]	= x0 + t0;
				X[k + n * 4 / 4]	= x0 - t0;
				X[k + n * 6 / 4]	= x6 - x4;

				if (n >= 4) {
					auto	x1	= X[k + n * 1 / 4];
					auto	x3	= X[k + n * 3 / 4];
					auto	x5	= X[k + n * 5 / 4];
					auto	x7	= X[k + n * 7 / 4];
					auto	t1	= (x5 + x7) * rsqrt2;
					auto	t2	= (x5 - x7) * rsqrt2;

					X[k + n * 1 / 4]	=  x1 + t2;
					X[k + n * 3 / 4]	=  x1 - t2;
					X[k + n * 5 / 4]	= -x3 - t1;
					X[k + n * 7 / 4]	=  x3 - t1;
				}
			}
		}

		for (int j = 1; j < n / 4; j++) {
			float	a   = pi * j / n;
			float	c1, s1; sincos(a, &s1, &c1);
			float	c3, s3; sincos(a * 3, &s3, &c3);
			for (int m = n; m < N; m <<= 2) {
				for (int k = (m - n) * 2; k < N; k += m * 4) {
					int		ka	= k + j;
					int		kb	= k + n * 2 - j;
					auto	x0	= X[ka + 0 * n / 2];
					auto	x1	= X[ka + 1 * n / 2];
					auto	x2	= X[ka + 2 * n / 2];
					auto	x3	= X[ka + 3 * n / 2];
					auto	x4	= X[kb - 3 * n / 2];
					auto	x5	= X[kb - 2 * n / 2];
					auto	x6	= X[kb - 1 * n / 2];
					auto	x7	= X[kb - 0 * n / 2];

					auto	t1	= x2 * c1 + x6 * s1;
					auto	t2	= x6 * c1 - x2 * s1;
					auto	t3	= x3 * c3 + x7 * s3;
					auto	t4	= x7 * c3 - x3 * s3;

					auto	t5	= t1 + t3;
					auto	t6	= t2 + t4;

					t3	= t1 - t3;
					t4	= t2 - t4;

					X[kb - 0 * n / 2] = x5 + t6;
					X[ka + 2 * n / 2] = t6 - x5;

					X[kb - 1 * n / 2] =-x1 - t3;
					X[ka + 3 * n / 2] = x1 - t3;

					X[kb - 2 * n / 2] = x0 - t5;
					X[ka + 0 * n / 2] = x0 + t5;

					X[kb - 3 * n / 2] = x4 - t4;
					X[ka + 1 * n / 2] = x4 + t4;
				}
			}
		}
	}
}
/*
template<typename T> void split_fft(complex<T>* X, int N) {
	const int bits = log2(N);
	for (int i = 0; i < N; i++) {
		int j = reverse_bits(i, bits);
		if (i < j)
			swap(X[i], X[j]);
	}
	split_fft_swizzled(X, N);
}

template<typename T> void split_fft(T* X, int N, complex<T>* out) {
	const int	bits = log2(N);
	for (int i = 0; i < N; i++)
		out[reverse_bits(i, bits)] = make_complex(X[i]);

	split_fft_swizzled(out, N);
}
*/
#endif

template<typename T> void split_dctII(T* x, int N, T *out) {
	auto	*t = alloc_auto(T, N);
	for (int k = 0; k < N / 2; k++) {
		t[k]			= x[k * 2 + 0];
		t[N - 1 - k]	= x[k * 2 + 1];
	}
	auto	*z = alloc_auto(complex<T>, N);
	split_fft(t, N, z);
	post_dctII(z, N, out);
}

template<typename T> void split_dctIII(T* x, int N, complex<T> *out) {
	auto	*z	= alloc_auto(complex<T>, N);
	pre_dctIII(x, N, z);

	auto	*t	= alloc_auto(complex<T>, N);
	split_fft(z, N, t);
	for (int k = 0; k < N / 2; k++) {
		out[k * 2 + 0] = ~t[k];
		out[k * 2 + 1] = ~t[N - 1 - k];
	}
}

//-----------------------------------------------------------------------------
//	Type-II/III DCT/DST algorithms with reduced number of arithmetic operations: Xuancheng Shao and Steven G. Johnson
//	only useful if w, s(N,k) precomputed
//-----------------------------------------------------------------------------

float s(int N, int k) {
	int	k4	= k & (N / 4 - 1);
	return	N <= 4 ? 1
		:	k4 <= N / 8
		?	s(N / 4, k4) * cos(k4 * pi * 2 / N)
		:	s(N / 4, k4) * sin(k4 * pi * 2 / N);
}

template<typename T> void _split_fftS(const T *x, int N, int L, int offset, int stride, complex<T> *out) {
	if (N < 4) {
		auto	x0	= x[offset & (stride * 2 - 1)];
		auto	x1	= x[(offset + stride) & (stride * 2 - 1)];
		out[0]	= complex<T>(x0 + x1, x0 - x1);

	} else if (N < 8) {
		auto	x0	= x[(offset + stride * 0) & (stride * 4 - 1)];
		auto	x1	= x[(offset + stride * 1) & (stride * 4 - 1)];
		auto	x2	= x[(offset + stride * 2) & (stride * 4 - 1)];
		auto	x3	= x[(offset + stride * 3) & (stride * 4 - 1)];

		out[0]	= complex<T>( (x0 + x1) + (x2 - x3), (x0 + x1) - (x2 - x3));
		out[1]	= complex<T>(-(x0 - x1) - (x2 + x3), (x0 - x1) - (x2 + x3));

	} else {
		auto	U	= out;
		auto	Z1	= out + N / 4;
		auto	Z2	= out + 3 * N / 8;

		_split_fftS(x, N / 2, L * 2, offset, stride * 2, U);
		_split_fftS(x, N / 4, 1, offset + stride, stride * 4, Z1);
		_split_fftS(x, N / 4, 1, offset - stride, stride * 4, Z2);

		//k==0 special case
		auto	z1	= Z1[0];
		auto	z2	= Z2[0];
		auto	u0	= U[0];
		auto	u1	= U[N / 4];

		out[0]		=  u0 +		 (z1 + z2) * (s(N, 0) / s(L * N, 0));
		out[N / 4]	= ~u1 -	i *	 (z1 - z2) * (s(N, 0) / s(L * N,  N / 4));

		complex<T>	wm	= exp(-pi * two * i / N);
		complex<T>	w	= wm;
		for (int k = 1; k < N / 8; k++) {
			auto	z1 =  w * s(N / 4, k) / s(N, k) * Z1[k];
			auto	z2 = ~w * s(N / 4, k) / s(N, k) * Z2[k];
			auto	u0	= U[k];
			auto	u1	= U[N / 4 - k];

			out[k]			=  u0	+		 (z1 + z2) * (s(N, k) / s(L * N, k));
			out[k + N / 4]	= ~u1	- i *	 (z1 - z2) * (s(N, k) / s(L * N, k + N / 4));
			out[N / 4 - k]	=  u1	- i *	~(z1 - z2) * (s(N, k) / s(L * N, k + N / 4));
			out[N / 2 - k]	= ~u0	-		~(z1 + z2) * (s(N, k) / s(L * N, k));

			w = w * wm;
		}

		//k==N/8 special case
		w	= exp(-pi * i / four);
		z1	=  w * z1;
		z2	= ~w * z2;

		out[N / 8]		=  u0 +		 (z1 + z2) * (s(N, k) / s(L * N, N / 8));
		out[N * 3 / 8]	= ~u0 -	i *	 (z1 - z2) * (s(N, k) / s(L * N, N * 3 / 8));
	}
}

template<typename T> void _split_fftS4(const T *x, int N, int offset, int stride, complex<T> *out) {
	auto	U	= out;
	auto	Z1	= out + N / 4;
	auto	Z2	= out + 3 * N / 8;

	_split_fftS4(N / 2, 2, strided(x, 2), U);
	_split_fftS4(N / 4, 1, strided(x + 1, 4), Z1);
	_split_fftS4(N / 4, 1, strided(x - 1, 4), Z2);

	//k==0 special case
	auto	z1	= Z1[0];
	auto	z2	= Z2[0];
	auto	u0	= U[0];
	auto	u1	= U[N / 4];

	out[0]		= ( u0 +		 (z1 + z2)) * (s(N, 0) / s(4 * N, 0));
	out[N / 4]	= (~u1 -	i *	 (z1 - z2)) * (s(N, 0) / s(4 * N, N / 4));

	complex<T>	wm	= exp(-pi * two * i / N);
	complex<T>	w	= wm;
	for (int k = 1; k < N / 8; k++) {
		auto	z1	=  w * s(N / 4, k) / s(N, k) * Z1[k];
		auto	z2	= ~w * s(N / 4, k) / s(N, k) * Z2[k];
		auto	u0	= U[0];
		auto	u1	= U[N / 4];

		out[k]			= ( u0	+		 (z1 + z2)) * (s(N, k) / s(4 * N, k));
		out[k + N / 4]	= (~u1	- i *	 (z1 - z2)) * (s(N, k) / s(4 * N, k + N / 4));
		out[N / 4 - k]	= ( u1	- i *	~(z1 - z2)) * (s(N, k) / s(4 * N, k + N / 4));
		out[N / 2 - k]	= (~u0	-		~(z1 + z2)) * (s(N, k) / s(4 * N, k));

		w = w * wm;
	}

	//k==N/8 special case
	w	= exp(-pi * i / four);
	z1	=  w * s(N / 4, N / 8) / s(N, N / 8) * z1;
	z2	= ~w * s(N / 4, N / 8) / s(N, N / 8) * z2;

	out[N / 8]		= ( u0 +		 (z1 + z2)) * (s(N, k) / s(4 * N, N / 8));
	out[N * 3 / 8]	= (~u0 -	i *	 (z1 - z2)) * (s(N, k) / s(4 * N, N * 3 / 8));
}

//-----------------------------------------------------------------------------
// general FFT
//-----------------------------------------------------------------------------

template<typename T> struct fft_state {
	enum { MAXFACTORS = 20 };
	typedef complex<T>	C;

	struct factor { int p, m; };

	int				nfft;
	bool			inverse;
	factor			factors[MAXFACTORS];
	dynamic_array<complex<T> >	twiddles;

	fft_state() {}
	fft_state(int _nfft, bool _inverse) { init(_nfft, _inverse); }

	void	init(int _nfft, bool _inverse);
	void	butterfly2(C *out, int stride, int m);
	void	butterfly3(C *out, int stride, int m);
	void	butterfly4(C *out, int stride, int m);
	void	butterfly5(C *out, int stride, int m);
	void	butterfly_generic(C *out, int stride, int m, int p);

	void	work(C *out, const C *in, int stride, factor* factors);
	void	process(const C *in, C *out);
	void	process(const T *in, C *out);
};

template<typename T> void fft_state<T>::init(int _nfft, bool _inverse) {
	nfft		= _nfft;
	inverse		= _inverse;

	int		n	= nfft;
	twiddles.resize(n);

	float	a	= (inverse ? 2 : -2) * pi / n;
	for (int k = 0; k < n; ++k)
		twiddles[k] = exp(i * a * k);

	auto	*p	= factors;

	//factor out powers of 4
	while ((n & 3) == 0) {
		n >>= 2;
		p->p = 4;
		p->m = n;
		++p;
	}

	//factor out powers of 2
	if ((n & 1) == 0) {
		n >>= 1;
		p->p = 2;
		p->m = n;
		++p;
	}

	//factor out any remaining primes
	for (int x = 3; x * x < n; x += 2) {
		while ((n % x) == 0) {
			n /= x;
			p->p = x;
			p->m = n;
			++p;
		}
	}

	p->p = n;
	p->m = 1;
}

template<typename T> void fft_state<T>::work(C* out, const C* in, int stride, factor* factors) {
	const int p	= factors[0].p;  // the radix
	const int m	= factors[0].m;  // stage's fft length/p
	const C*  end = out + p * m;

	if (m == 1) {
		for (C *i = out; i != end; ++i, in += stride)
			*i = *in;
	} else {
		for (C *i = out; i != end; i += m, in += stride)
			work(i, in, stride * p, factors + 1);
	}

	// recombine the p smaller DFTs
	switch (p) {
		case 2: butterfly2(out, stride, m); break;
		case 3: butterfly3(out, stride, m); break;
		case 4: butterfly4(out, stride, m); break;
		case 5: butterfly5(out, stride, m); break;
		default: butterfly_generic(out, stride, m, p); break;
	}
}

template<typename T> void fft_state<T>::process(const C *in, C *out) {
	if (in == out) {
		C * tmpbuf = alloc_auto(C, nfft);
		work(tmpbuf, in, 1, factors);
		memcpy(out, tmpbuf, sizeof(C) * nfft);
	} else {
		work(out, in, 1, factors);
	}
}

template<typename T> void fft_state<T>::process(const T *in, C *out) {
	C * tmpbuf = alloc_auto(C, nfft);
	for (int i = 0; i < nfft; i++)
		tmpbuf[i] = C(in[i]);
	work(out, tmpbuf, 1, factors);
}


template<typename T> void fft_state<T>::butterfly2(C *out, int stride, int m) {
	C *tw1 = twiddles;

	for (int k = m; k--; ++out) {
		C	t	= out[m] * *tw1;
		tw1		+= stride;
		out[m]	= out[0] - t;
		out[0]	+= t;
	}
}

template<typename T> void fft_state<T>::butterfly3(C *out, int stride, int m) {
	C *tw1 = twiddles, *tw2 = twiddles;
	C epi3 = twiddles[stride * m];

	for (int k = m; k--; ++out) {
		C	t1 = out[m * 1] * *tw1;
		C	t2 = out[m * 2] * *tw2;

		C	t3 = t1 + t2;
		C	t0 = t1 - t2;
		tw1 += stride;
		tw2 += stride * 2;

		out[m] = out[0] - t3 / 2;

		t0 *= epi3.i;

		out[0] += t3;

		out[m * 2].r = out[m].r + t0.i;
		out[m * 2].i = out[m].i - t0.r;

		out[m].r -= t0.i;
		out[m].i += t0.r;
	}
}

template<typename T> void fft_state<T>::butterfly4(C *out, int stride, int m) {
	C *tw1 = twiddles, *tw2 = twiddles, *tw3 = twiddles;

	for (int k = m; k--; ++out) {
		C	t0 = out[m * 1] * *tw1;
		C	t1 = out[m * 2] * *tw2;
		C	t2 = out[m * 3] * *tw3;

		C	t5	= out[0] - t1;
		out[0]	+= t1;
		C	t3	= t0 + t2;
		C	t4	= t0 - t2;
		out[m * 2]	= out[0] - t3;

		tw1		+= stride * 1;
		tw2		+= stride * 2;
		tw3		+= stride * 3;
		out[m * 0]	+= t3;

		if (inverse) {
			out[m * 1].r	= t5.r - t4.i;
			out[m * 1].i	= t5.i + t4.r;
			out[m * 3].r	= t5.r + t4.i;
			out[m * 3].i	= t5.i - t4.r;
		} else {
			out[m * 1].r	= t5.r + t4.i;
			out[m * 1].i	= t5.i - t4.r;
			out[m * 3].r	= t5.r - t4.i;
			out[m * 3].i	= t5.i + t4.r;
		}
	}
}

template<typename T> void fft_state<T>::butterfly5(C *out, int stride, int m) {
	C *tw	= twiddles;
	C ya	= twiddles[stride * m], yb = twiddles[stride * 2 * m];

	for (int u = 0; u < m; ++u, ++out) {
		C	t0 = out[0];

		C	t1	= out[m * 1] * tw[1 * u * stride];
		C	t2	= out[m * 2] * tw[2 * u * stride];
		C	t3	= out[m * 3] * tw[3 * u * stride];
		C	t4	= out[m * 4] * tw[4 * u * stride];

		C	t7	= t1 + t4;
		C	t10	= t1 - t4;
		C	t8	= t2 + t3;
		C	t9	= t2 - t3;

		out[m * 0]	+= t7 + t8;

		C	t5(t0.r + t7.r * ya.r + t8.r * yb.r,	t0.i + t7.i * ya.r + t8.i * yb.r);
		C	t6(t10.i * ya.i + t9.i * yb.i,			-t10.r * ya.i - t9.r * yb.i);

		out[m * 1]	= t5 - t6;
		out[m & 4]	= t5 + t6;

		C	t11(t0.r + t7.r * yb.r + t8.r * ya.r,	t0.i + t7.i * yb.r + t8.i * ya.r);
		C	t12(-t10.i * yb.i + t9.i * ya.i,		t10.r * yb.i - t9.r * ya.i);

		out[m * 2] = t11 + t12;
		out[m * 3] = t11 - t12;
	}
}

template<typename T> void fft_state<T>::butterfly_generic(C *out, int stride, int m, int p) {
	C *scratch = alloc_auto(C, p);

	for (int u = 0; u < m; ++u) {
		for (int q1 = 0, k = u; q1 < p; ++q1, k += m)
			scratch[q1] = out[k];

		for (int q1 = 0, k = u; q1 < p; ++q1, k += m) {
			out[k] = scratch[0];
			int tw = 0;
			for (int q = 1; q < p; ++q) {
				tw += stride * k;
				if (tw >= nfft)
					tw -= nfft;
				out[k] += scratch[q] * twiddles[tw];
			}
		}
	}
}

//-----------------------------------------------------------------------------
// FFTR
// perform the parallel fft of two real signals packed in real,imag
//-----------------------------------------------------------------------------

template<typename T> struct fftr_state : fft_state<T> {
	dynamic_array<C>	super_twiddles;

	fftr_state() {}
	fftr_state(int nfft) { init(nfft); }

	void	init(int nfft) {
		nfft >>= 1;
		fft_state<T>::init(nfft, false);
		super_twiddles.resize(nfft);

		float	a = -pi / nfft;
		for (int k = 0; k < nfft; ++k)
			super_twiddles[k] = exp(i * a * k);
	}

	void process(const T *timedata, C *freqdata) {
		C	*temp	= alloc_auto(C, nfft);
		fft_state<T>::process((const C*)timedata, temp);
		real_post_fft(temp, nfft, freqdata, super_twiddles.begin());
	}
};

template<typename T> struct inv_fftr_state : fft_state<T> {
	dynamic_array<C>	super_twiddles;

	inv_fftr_state() {}
	inv_fftr_state(int nfft) { init(nfft); }

	void	init(int nfft) {
		nfft >>= 1;
		fft_state<T>::init(nfft, true);
		super_twiddles.resize(nfft);

		float	a = pi / nfft;
		for (int k = 0; k < nfft; ++k)
			super_twiddles[k] = exp(i * a * k);
	}

	void process(const C *freqdata, T *timedata) {
		C	*temp	= alloc_auto(C, nfft);
		real_pre_ifft(freqdata, nfft, temp, super_twiddles.begin());
		fft_state<T>::process(temp, (C*)timedata);
	}
};

} // namespace iso
#endif //FFT_H
