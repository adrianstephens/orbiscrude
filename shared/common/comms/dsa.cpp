#include "dsa.h"

namespace iso {

DSA::DSA(const parameters &params) :
	P(params.p),
	Q(params.q),
	G(params.g)
{
}

DSA::signature DSA::sign(const const_memory_block &input, vrng &&rng) const {
	uint32		n = Q.num_bits();
	mpi			k = mpi::random(rng, n);

	//Make sure that 0 < k < q
	if (k >= Q)
		k >>= 1;

	n = min(n, input.size32() * 8);
	mpi	z = input.slice_to((n + 7) / 8);

	//Keep the leftmost N bits of the hash value
	if (n % 8)
		z >>= 8 - (n % 8);

	//Compute r = (g ^ k mod p) mod q
	mpi	r = exp_mod(G, k, P) % Q;

	//Compute s = k ^ -1 * (z + priv_key * r) mod q
	mpi	s = (priv_key * r + z) % Q;
	s = (s * inv_mod(k, Q)) % Q;

	return signature(r, s);
}

bool DSA::verify(const const_memory_block &input, const signature &sig) const {
	if (sig.r <= zero || sig.r >= Q || sig.s <= zero || sig.s >= Q)
		return false;

	uint32	n = min(Q.num_bits(), input.size32() * 8);
	mpi		z = input.slice_to((n + 7) / 8);

	//Keep the leftmost N bits of the hash value
	if (n % 8)
		z >>= 8 - (n % 8);

	mpi	w	= inv_mod(sig.s, Q);
	mpi	u1	= mul_mod(z, w);
	mpi	u2	= mul_mod(sig.r, w);

	//Compute v = ((g ^ u1) * (y ^ u2) mod p) mod q
	mpi	v = exp_mod(G, u1, P);
	w = exp_mod(pub_key, u2, P);
	v = (v * w) % P;
	v = v % Q;

	return v == sig.r;
}

} //namespace iso
