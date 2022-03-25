#include "filters.h"

namespace iso {

//-----------------------------------------------------------------------------
// filters
//-----------------------------------------------------------------------------

void normalise_samples(float *coeffs, int n, float scale) {
	float	t = 0;
	for (int i = 0; i < n; i++)
		t += coeffs[i];

	t = scale / t;
	for (int i = 0; i < n; i++)
		coeffs[i] *= t;
}

int optimise_samples(float *coeffs, int n, float2 *out, float threshold) {
	int	n1 = 0;
	for (unsigned i = 0; i < n; i += 2) {
		float c0 = coeffs[i + 0];
		float c1 = coeffs[i + 1];
		float c2 = c0 + c1;
		if (c2 < threshold)
			break;
		float x2 = float(i) + c1 / c2;
		out[n1++] = float2{x2, c2};
	}
	return n1;
}

void make2d_samples(float2 *in, int n, float w, float h, float3 *out) {
	float3	s{w ? 1 / w : 0, h ? 1 / h : 0, 1};
	for (int i = 0; i < n; i++)
		out[i] = s * in[i].xxy;
}

int optimise_samples(float3 *v, int n, float threshold) {
	int	n1 = 0;
	for (int i0 = 0; i0 < n;) {
		float2	v0 = v[i0].xy - half, vi0 = floor(v0);
		float4	weights = bilinear_weights(frac(v0));

		i0++;
		for (int i1 = i0; i1 < n; i1++) {
			float2	v1 = v[i1].xy - half;
			if (all(vi0 == floor(v1))) {
				swap(v[i0++], v[i1]);
				weights += bilinear_weights(frac(v1));
			}
		}
		float	t	= reduce_add(weights);
		if (t < threshold)
			continue;

		float4	s	= (swizzle<0,1,0,2>(weights) + swizzle<2,3,1,3>(weights)) / t;
		float3	r	= concat(s.yz, t);
		v[n1++] = 0;
	}
	return n1;
}

void gaussian(float *coeffs, int n, float rho, float scale) {
	if (rho == 0) {
		coeffs[0] = scale;
		for (unsigned i = 1; i < n; i++)
			coeffs[i] = 0;
	} else {
		float	a = 2.f * square(rho);
		float	b = scale * rsqrt(a * pi);
		float	c = -1 / a;
		for (unsigned i = 0; i < n; i++)
			coeffs[i] = exp(square(uint2float(i)) * c) * b;
	}
}

int gaussian(float *coeffs, int n, float rho, float scale, float threshold) {
	if (rho == 0) {
		coeffs[0] = scale;
		return 1;
	} else {
		float	a = 2.f * square(rho);
		float	b = scale * rsqrt(a * pi);
		float	c = -1 / a;
		for (int i = 0; i < n; i++) {
			float	t = exp(square(uint2float(i)) * c) * b;
			if (t < threshold)
				return i;
			coeffs[i] = t;
		}
		return n;
	}
}

int gaussian_kernel(float3 *out, int n, float rho, float scale, int width, float threshold) {
	float	*samples1 = alloc_auto(float, n * 2);
	float2	*samples2 = alloc_auto(float2, n);
	gaussian(samples1, n * 2, rho, scale);
	normalise_samples(samples1, n * 2);
	samples1[0] /= 2;
	n = optimise_samples(samples1, n * 2, samples2, threshold);
	make2d_samples(samples2, n, width, 0, out);
	return n;
}

}//namespace iso
