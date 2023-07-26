#ifndef FILTERS_H
#define FILTERS_H

#include "base/vector.h"
#include "base/block.h"
#undef NO

namespace iso {

//-----------------------------------------------------------------------------
// filters
//-----------------------------------------------------------------------------

inline float4	bilinear_weights(param(float2) a) {
	float2 b = float2(one) - a;
	return float4(swizzle<2,0,2,0>(a, b)) * swizzle<3,3,1,1>(a, b);
}

void			normalise_samples(float *coeffs, int n, float scale = 1);
int				optimise_samples(float *coeffs, int n, float2 *out, float threshold = 0.005f);
int				optimise_samples(float3 *v, int n, float threshold = 1 / 64.f);
void			make2d_samples(float2 *in, int n, float w, float h, float3 *out);

void			gaussian(float *coeffs, int n, float rho, float scale = 1);
int				gaussian(float *coeffs, int n, float rho, float scale, float threshold);
int				gaussian_kernel(float3 *out, int n, float rho, float scale, int width, float threshold = 1 / 64.f);

#if 0
struct filter_trapezoid {
	float	scale;
	float filter(float x)			{ return clamp(0.5f + (0.5f - abs(x)) / scale, 0, 1); }
	float support()					{ return (scale + 1) / 2; }
	filter_trapezoid(float scale) : scale(scale) {}
};

struct filter_triangle {
	static float filter(float x)	{ return max(1 - abs(x), 0); }
	static float support()			{ return 1; }
};

struct filter_cubic {
	static float filter(float x) {
		x = abs(x);
		return	x < 1 ? (4 + x * x * (3 * x - 6)) / 6
			:	x < 2 ? (8 + x * (-12 + x * (6 - x))) / 6
			:	0;
	}
	static float support() { return 2; }
};

struct filter_catmullrom {
	static float filter(float x) {
		x = abs(x);
		return	x < 1 ? 1 - x * x * (2.5f - 1.5f * x)
			:	x < 2 ? 2 - x * (4 + x * (0.5f * x - 2.5f))
			:	0;
	}
	static float support() { return 2; }
};

struct filter_mitchell {
	static float filter(float x) {
		x = abs(x);
		return	x < 1 ? (16 + x * x * (21 * x - 36)) / 18
			:	x < 2 ? (32 + x * (-60 + x * (36 - 7 * x))) / 18
			:	0;
	}
	static float support() { return 2; }
};
#endif

//-----------------------------------------------------------------------------
//	n-linear sampling
//-----------------------------------------------------------------------------

template<typename D> D linear_sample(const block<D, 1>& b, float x) {
	int		ix	= int(x);
	if (ix < 0 || ix >= b.size())
		return 0;

	auto	r	= b[ix];
	if (float fx = frac(x))
		r = lerp(r, b[ix + 1], fx);
	return r;
}

template<typename D, int N> D linear_sample(const block<D, N>& b, vec<float, N> c) {
	float	x	= c[N - 1];
	int		ix	= int(x);
	if (ix < 0 || ix >= b.size())
		return 0;

	auto	r	= linear_sample(b[ix], shrink<N - 1>(c));
	if (float fx = frac(x))
		r = lerp(r, linear_sample(b[ix + 1], shrink<N - 1>(c)), fx);
	return r;
}

template<typename D> auto point_sample(const block<D, 1>& b, float x) {
	return b[int(x)];
}
template<typename D, int N> auto point_sample(const block<D, N>& b, vec<float, N> c) {
	return point_sample(b[int(c[N - 1])], shrink<N - 1>(c));
}

template<typename D, typename S> inline void muladd(D &d, const S &s, float f) {
	d += D(s * f);
}

template<typename D, typename S, int N> inline void muladd(const block<D, N> &d, const block<S, N> &s, float f) {
	resample(d, s, d.size(), s.size(), f);
}

template<typename S, typename D> void resample(const D &dest, const S &srce, int destw, int srcew, float f2 = 1.0f) {
	if (srcew == destw) {
		for (int x = 0; x < destw; x++)
			muladd(dest[x], srce[x], f2);

	} else {
		float	d	= float(srcew) / float(destw);
		int		n	= int(d) + (d > int(d)) + 1;

		if (d > 1)
			f2 /= d;

		for (int i = 0; i < n; i++) {
			float	f1	= 0;//d < 1 ? -.5f : 0;
			for (int x = 0; x < destw; x++, f1 += d) {
				int		ix	= i + int(f1);
				float	f;
				if (d > 1) {
					f = min(f1 + d - ix, 1.f);
					if (i == 0)
						f += ix - f1;
					if (f <= 0)
						continue;
				} else {
					f	= i ? 1 + (f1 - ix) : 1 - (f1 - ix);
				}
				muladd(dest[x], srce[clamp(ix, 0, srcew - 1)], f * f2);
			}
		}
	}
}

template<typename D, typename S, int N> void resample(const block<D, N> &d, const block<S, N> &s) {
	fill(d, 0);
	resample(d, s, d.size(), s.size(), 1);
}

template<typename T, typename D, typename S> void resample_via(const block<D, 2> &d, const block<S, 2> &s) {
	auto	s2 = make_auto_block_using<T>(s);
	auto	d2 = make_auto_block_using<T>(d);
	copy(s, s2.get());
	resample(d2.get(), s2.get());
	copy(d2.get(), d);
}

//-----------------------------------------------------------------------------
//	box filter
//-----------------------------------------------------------------------------

template<typename D, typename S> void downsample2x2(const block<D, 2> &d, const block<S, 2> &s) {
	auto	sy = s.begin();
	for (auto dy : d) {
		auto	s0	= sy++.begin();
		auto	s1	= sy++.begin();
		for (auto &dx : dy) {
			dx = (s0[0] + s0[1] + s1[0] + s1[1]) / 4;
			s0 += 2;
			s1 += 2;
		}
	}
}

template<typename D, typename S> auto_block<D, 2> downsample2x2(const block<S, 2> &s) {
	auto	d = make_auto_block<D>(s.template size<1>() / 2, s.template size<2>() / 2);
	downsample2x2(d, s);
	return d;
}

template<typename S> auto_block<S, 2> downsample2x2(const block<S, 2> &s) {
	auto	d = make_auto_block<S>(s.template size<1>() / 2, s.template size<2>() / 2);
	downsample2x2(d, s);
	return d;
}

//-----------------------------------------------------------------------------
//	point sampling
//-----------------------------------------------------------------------------

template<typename D, typename S> inline void point_resample(D &d, const S &s) {
	d = s;
}

template<typename D, typename S, int N> inline void point_resample(const block<D, N> &d, const block<S, N> &s) {
	resample_point(d, s, d.size(), s.size());
}

template<typename S, typename D> void resample_point(const D &dest, const S &srce, int destw, int srcew) {
	for (int x = 0; x < destw; x++)
		point_resample(dest[x], srce[(x * srcew + destw / 2) / destw]);
}

template<typename D, typename S, int N> void resample_point(const block<D, N> &d, const block<S, N> &s) {
	resample_point(d, s, d.size(), s.size());
}

//-----------------------------------------------------------------------------
//	seperable filtering
//-----------------------------------------------------------------------------

template<typename S, typename D> void filter(const D &dest, const S &srce, int w, const float *kernel, int n, int o) {
	for (int i = 0; i < n; i++) {
		float	f	= kernel[i];
		int		d	= i - o;
		for (int x = max(-d, 0), x1 = w + min(-d, 0); x < x1; x++)
			muladd(dest[x], srce[x + d], f);
	}
}

template<typename S, typename D> void hfilter(const block<D, 1> &dest, const block<S, 1> &srce, const float *kernel, int n, int o) {
	filter(dest.begin(), srce.begin(), dest.size(), kernel, n, o);
}
template<typename S, typename D> void hfilter(const block<D, 2> &dest, const block<S, 2> &srce, const float *kernel, int n, int o) {
	for (int y = 0; y < dest.size(); y++)
		hfilter(dest[y], srce[y], kernel, n, o);
}
template<typename S, typename D> void vfilter(const block<D, 2> &dest, const block<S, 2> &srce, const float *kernel, int n, int o) {
	filter(dest.begin(), srce.begin(), dest.size(), kernel, n, o);
}

template<typename T, int N> bool is_seperable(const block<T, N> &filter, float *factors = 0, int *index = 0) {
	T	*pmax	= 0;
	for_each(filter, [&pmax](T &i) { if (!pmax || bigger(i, *pmax)) pmax = &i; });

	int	imax	= filter.index_of(pmax);
	if (index)
		*index = imax;

	auto	i0	= filter[imax];
	for (auto i : filter) {
		float	scale = 1;
		if (i.begin() != i0.begin() && !find_scalar_multiple(i0, i, scale))
			return false;
		if (factors)
			*factors++ = scale;
	}
	return true;
}

//-----------------------------------------------------------------------------
//	convolve block
//-----------------------------------------------------------------------------

struct conv_params {
	int			*offset;
	int			*stride;
	int			*dilate;
	const float	*bias;
	interval<float>	activation;
	conv_params() { clear(*this); activation.b = 1; }
};

//	convolve0 - single scalar convolve output

template<typename T, typename F, int N> T convolve0(const block<T, N> &in, const block<F, N> &kernel) {
	T		t(zero);
	if (int n = min(kernel.size(), in.size())) {
		auto	i	= in.begin();
		auto	k	= kernel.begin();
		while (n--)
			t += convolve0(*i++, *k++);
	}
	return t;
}

//2d with extra check for vertical kernel
template<typename T, typename F> T convolve0(const block<T, 2> &in, const block<F, 2> &kernel) {
	T		t(zero);
	if (int n = min(kernel.size(), in.size())) {
		auto	i	= in.begin();
		auto	k	= kernel.begin();

		if (int n1 = kernel.template size<1>()) {
			if (n1 == 1) {
				while (n--)
					t += **k++ * **i++;

			} else {
				while (n--)
					t += convolve0(*i++, *k++);
			}
		}
	}
	return t;
}

//1d
template<typename T, typename F> T convolve0(const block<T, 1> &in, const block<F, 1> &kernel) {
	T	t(zero);
	T	*i	= in.begin();
	F	*k	= kernel.begin();
	for (int n = min(kernel.size(), in.size()); n--;)
		t += *k++ * *i++;
	return t;
}

//	convolve1 - single block convolve output

template<typename T, typename F, int NO, int NI> void convolve1(const block<T, NO> &out, const block<T, NI> &in, const block<F, NI> &kernel, const conv_params &params) {
	if (int n = min(kernel.size(), in.size())) {
		auto	i	= in.begin();
		auto	k	= kernel.begin();
		while (n--)
			convolve1(out, *i++, *k++, params);
	}
}

//1d
template<typename T, typename F, int NO> void convolve1(const block<T, NO> &out, const block<T, NO> &in, const block<F, NO> &kernel, const conv_params &params) {
	if (int n = min(kernel.size(), in.size())) {
		auto	i	= in.begin();
		auto	o	= out.begin();
		auto	k	= kernel.begin();
		while (n--)
			*o++ += *k++ * *i++;
	}
}

// kernel is lower dim than in, so apply to each 'layer'
template<int L, typename T, typename F, int NO, int NI, int NF, typename V = enable_if_t<(NF < NI)>> void convolve(const block<T, NO> &out, const block<T, NI> &in, const block<F, NF> &kernel, const conv_params &params) {
	if (false && can_flatten<NI>(in) && can_flatten<NI>(out)) {
		convolve<L>(flatten<NI>(out), flatten<NI>(in), kernel, params);

	} else {
		auto	i = in.begin();
		for (auto &o : out)
			convolve<L>(o, *i++, kernel, params);
	}
}

// kernel is same dim as in, so apply
template<int L, typename T, typename F, int NO, int NI, typename V = enable_if_t<L != NO>> void convolve(const block<T, NO> &out, const block<T, NI> &in, const block<F, NI> &kernel, const conv_params &params, V* = 0) {
//	int	i = 0, n = kernel.size<NO>();
	int		i	= params.offset[NO - 1], s = params.stride[NO - 1], k = -i / s;
	ISO_ASSERT(i >= 0 || i % s == 0);
	for (auto o : out) {
		if (i < 0)
			convolve<L>(o, in, kernel.template slice<NO>(k--), params);
		else
			convolve<L>(o, in.template slice<NO>(i), kernel, params);
		i += s;
	}
}

// out is 0D, so convolve and apply clamp
template<int L, typename T, typename F, int NI> void convolve(const block<T, L> &out, const block<T, NI> &in, const block<F, NI> &kernel, const conv_params &params) {
	if (params.bias)
		out.data().copy_from(params.bias);
	else
		out.data().clear_contents();
	convolve1(out, in.template skip<L+1>(params.dilate[0]).template skip<L+2>(params.dilate[1]), kernel, params);
	clamp(out, params.activation.a, params.activation.b);
}

// out is 0D, so convolve and apply clamp
template<int L, typename T, typename F, int NI> void convolve(T &out, const block<T, NI> &in, const block<F, NI> &kernel, const conv_params &params) {
	out = params.activation.clamp((params.bias ? params.bias[0] : 0) + convolve0(in.template skip<2>(params.dilate[0]).skip<3>(params.dilate[1]), kernel));
}

//-----------------------------------------------------------------------------
//	softmax
//-----------------------------------------------------------------------------

template<typename I, typename O, typename T> void softmax(I first, I end, O dest, T beta) {
	// Find the max coeff
	I		i	= first;
	auto	max	= *i;
	while (++i != end) {
		if (*i > max)
			max = *i;
	}

	// Compute the normalized sum of exps
	T		sum = zero;
	auto	j	= dest;
	for (auto i = first; i != end; ++i, ++j)
		sum += (*j = exp((*i - max) * beta));

	// Divide by the sum of exps
	auto	r = reciprocal(sum);
	for (auto i = dest; i != j; ++i)
		*i *= r;
}

template<typename I, typename O, typename T> void logsoftmax(I first, I end, O dest, T beta) {
	// Find the max coeff
	I		i	= first;
	auto	max	= *i;
	while (++i != end) {
		if (*i > max)
			max = *i;
	}

	// Compute sum
	T		sum = zero;
	for (auto i = first; i != end; ++i)
		sum += exp(*i - max);

	// Compute result
	auto	log_sum = max + log(sum);
	auto	j		= dest;
	for (auto i = first; i != end; ++i, ++j)
		*j = *i - log_sum;
}

template<typename T, int N> void softmax(const block<T, N> &in, const block<T, N> &out, T beta) {
	auto	j = out.begin();
	for (auto &i : in) {
		softmax(i, *j);
		j++;
	}
}

template<typename T> void softmax(const block<T, 1> &in, const block<T, 1> &out, T beta) {
	softmax(in.begin(), in.end(), out.begin(), beta);
}


//-----------------------------------------------------------------------------
//	one euro
//	see http://www.lifl.fr/~casiez/1euro
//-----------------------------------------------------------------------------

struct OneEuro {
	struct Config {
		float			cutoff_min;
		float			cutoff_slope;
		float			dcutoff;
	};
	//The state data for a simple low pass filter
	struct LowPassFilter {
		float	hatx;
		float operator()(float x, float alpha) {
			return hatx	= alpha * x + (1 - alpha) * hatx;
		}
	};

	LowPassFilter	xfilt, dxfilt;
	float			xprev;
	bool			started;

	OneEuro() : started(false) {}

	static float alpha(float freq, float cutoff) {
		return 1 / (1 + freq / (pi * 2 * cutoff));
	}

	float process(float x, float dt, const Config &config) {
		if (!started) {
			started		= true;
			dxfilt.hatx	= 0;
			return xfilt.hatx = xprev = x;
		}
		float	freq	= 1 / dt;
		float	dx		= (x - xprev) * freq;	//maybe x - xfilt.hatx
		xprev	= x;

		float	edx		= dxfilt(dx, alpha(freq, config.dcutoff));
		return xfilt(x, alpha(freq, config.cutoff_min + config.cutoff_slope * abs(edx)));
	}
};

}//namespace iso

#endif // FILTERS_H
