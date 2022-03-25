#ifndef FFT_H
#define FFT_H

#include "bits.h"

namespace iso {

struct complex {
	float	r, i;
};

template<int N> void FFT(float *samples, int offset, complex *output) {
	for (int i = 0; i < N; i++) {
		int	a = (i + offset) & (N - 1);
		int b = flip_bits(uint16(a));
		complex[b].r = samples[a];
		complex[b].i = 0;
	}

	// Danielson - Lancos
	for (int step = 1; step < N; step <<= 1) {
		float theta = -pi / float(step);
		float omega	= sin(0.5f * theta);
		float wpr	= -2 * omega * omega;
		float wpi	= sin(theta);
		float wr	= 1;
		float wi	= 0;

		for (int base_index = 0; base_index < step; base_index++) {
			for (int j = base_index; j < N; j += step) {
				int		t = j + step;
				float	r = wr * output[t].r - wi * output[t].i;
				float	i = wr * output[t].i + wi * output[t].r;

				output[t].r = output[j].r - r;
				output[t].i = output[j].i - i;

				output[j].r += r;
				output[j].i += i;
			}

			omega	= wr;
			wr		= wr * wpr - wi * wpi + wr;
			wi		= wi * wpr + omega * wpi + wi;
		}
	}
}

}//namespace iso

#endif // FFT_H