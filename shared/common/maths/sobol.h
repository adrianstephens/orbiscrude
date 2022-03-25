#ifndef SOBOL_H
#define SOBOL_H

#include "galois2.h"

namespace iso {

class _Sobol {
protected:
	static const uint32 MAX_DIMS	= 32;  // (up to 1111) number of dimensions of sequence we're going to generate
	static uint64	cjn[MAX_DIMS][63];

	static void	GenerateCJ();
	static void	GeneratePolynomials(uint32 *buffer, int dims, bool primitive);

	_Sobol() {
		if (cjn[0][0] == 0)
			GenerateCJ();
	}
};

template<int D> class Sobol : _Sobol {
	gf2_vec<64>	prev_vals[D];
	uint64		seed;

public:
	Sobol() : seed(2048) {
		clear(prev_vals);
	}
	
//	void	prepareForIntegration() { seed = 2048 + D * 1024; }

	template<typename T> void	generate(T *p) {
		uint64		prev_gray	= (seed >> 1) ^ seed;
		++seed;
		gf2_vec<63>	changed		= prev_gray ^ (seed >> 1) ^ seed;

		for (int i = 0; i < D; i++)
			p[i] = (prev_vals[i] = madd(*(gf2_mat<64, 63>*)cjn[i], changed, prev_vals[i])).u / (T)0x8000000000000000ULL;
	}
};

} // namespace iso

#endif // SOBOL_H
