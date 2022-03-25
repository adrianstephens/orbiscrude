#ifndef RESAMPLE_H
#define RESAMPLE_H

#include "base/block.h"

namespace iso {

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

template<typename D, typename S> inline void point_sample(D &d, const S &s) {
	d = s;
}

template<typename D, typename S, int N> inline void point_sample(const block<D, N> &d, const block<S, N> &s) {
	resample_point(d, s, d.size(), s.size());
}

template<typename S, typename D> void resample_point(const D &dest, const S &srce, int destw, int srcew) {
	for (int x = 0; x < destw; x++)
		point_sample(dest[x], srce[(x * srcew + destw / 2) / destw]);
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

}	// namespace iso

#endif	// RESAMPLE_H

