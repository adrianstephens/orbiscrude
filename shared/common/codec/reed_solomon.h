#include "utilities.h"

//-----------------------------------------------------------------------------
// reed-solomon error correction
//-----------------------------------------------------------------------------

namespace iso {

class RScodec {
	void	generate_gf(uint32 pp);
	bool	generate_poly();

	int		mm; //RS code over GF(2^4)
	int		tt; //number of errors that can be corrected
	int		nn; //(2^mm) - 1	length of codeword
	int		kk; //nn-2*tt		length of original data

	bool	isOk;

	dynamic_array<int>		alpha_to;
	dynamic_array<uint32>	index_of;
	dynamic_array<int>		gg;
public:
	//pp: irreducible polynomial coeffs
	RScodec(uint32 pp, int mm, int tt) : mm(mm), tt(tt), nn(bits(mm)), kk(nn - (tt * 2)),
		alpha_to(nn + 1),
		index_of(nn + 1),
		gg(nn - kk + 1)
	{
		generate_gf(pp);
		isOk = generate_poly();
	}

	bool	encode(uint8 *data, uint8 *parity);
	int		decode(uint8 *data);
};

// N	length of output block
// P	polynomial
// M	length of codeword = (2^M) - 1
// T	number of errors that can be corrected
// in	input data (at least 255 * blk bytes)
// out	output data (at least N * blk bytes)
// blk	number of codewords
template<int N, int P, int M, int T> void decodeI(uint8 *in, uint8 *out, uint32 blk) {
	static const int S = bits(M);
	uint8	data[S];
	RScodec rsc(P, M, T);

	for (uint32 i = 0; i < blk; i++) {
		int	k = i;
		for (auto &j : data) {
			j = in[k];
			k +=blk;
		}
		int r = rsc.decode(data);
		//if (r<0)
		//	DRW_DBG("\nWARNING: decodeI, can't correct all errors");
		k = i * N;
		for (auto &j : data)
			out[k++] = j;
	}
}

}