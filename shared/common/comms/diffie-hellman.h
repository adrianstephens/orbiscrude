#include "maths/bignum.h"
#include "extra/random.h"

namespace iso {

struct DH : mod_context {
	mpi				g;
	mpi				priv_key;

	DH() {}
	DH(param(mpi) p, param(mpi) g) : mod_context(p), g(g) {}
	DH(vrng &&rng, int prime_len, int subprime_len = 0, int generator = 0);

	void			init(param(mpi) _p, param(mpi) _g) { mod_context::init(_p); g = _g; }
	void			generate_private(vrng &&rng, int subprime_len = 0);
	void			generate_private(vrng &&rng, param(mpi) q);			// prime-order subgroup size

	malloc_block	get_public() const;
	bool			check_public_key(param(mpi) key) const;
	malloc_block	shared_secret(param(mpi) peer_pub_key) const;
};

} // namespace iso