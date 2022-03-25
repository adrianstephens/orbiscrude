#include "maths/bignum.h"
#include "extra/random.h"

namespace iso {

struct DSA {
	struct parameters {
		mpi p, q, g;
	};
	struct signature {
		mpi	r, s;
		signature(param(mpi) _r, param(mpi) _s) : r(_r), s(_s) {}
	};

	mpi					P;
	mpi					Q;			// == 20
	mpi					G;
	mpi					priv_key;	// x private key
	mpi					pub_key;	// y public key
	montgomery_context	mont;

	mpi		mul_mod(param(mpi) a, param(mpi) b) const { return (a * b) % Q; }

	DSA() {}
	DSA(const parameters &params);
	signature	sign(const const_memory_block &input, vrng &&rng) const;
	bool		verify(const const_memory_block &input, const signature &sig) const;

	void	generate_private(vrng &&rng) {
		priv_key	= mpi::random(rng, Q.num_bits());
		if (priv_key >= Q)
			priv_key >>= 1;
	}
};

} // namespace iso