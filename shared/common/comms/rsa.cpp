#include "rsa.h"

namespace iso {

RSA::RSA(const parameters &params) :
	E(params.e),
	D(params.d),
	P(params.p),
	Q(params.q)
{
	N	= P * Q;
	DP	= D % (P - 1);
	DQ	= D % (Q - 1);
	QP	= inv_mod(Q, P);
}

// Check a public RSA key
bool RSA::check_public_key() {
	return	N.is_odd()
		&&	E.is_odd()
		&&	highest_set_index(N) >= 128	&& highest_set_index(N) <= 4096
		&&	highest_set_index(E) >= 2	&& highest_set_index(E) <= 64;
}

// Check a private RSA key
bool RSA::check_private_key() {
	if (!check_public_key() || !P || !Q || !D)
		return false;

	mpi	PQ	= P * Q;
	mpi	DE	= D * E;
	mpi	P1	= P - 1;
	mpi	Q1	= Q - 1;
	mpi	H	= P1 * Q1;
	mpi	G	= gcd(E, H);
	mpi	G2	= gcd(P1, Q1);

	mpi L1	= divmod(H, G2);
	mpi	I	= DE % L1;

	return PQ == N && H == 0 && I == 1 && G == 1;
}

//Do an RSA public key operation
bool RSA::public_op(const const_memory_block &input, uint8 *output) {
	mpi T(input);
	return T < N && exp_mod(T, E, N, RN).save(output, input.size32());
}

// Do an RSA private key operation
bool RSA::private_op(const const_memory_block &input, uint8 *output) {
	mpi T(input);
	if (T >= N)
		return false;
#if 0
	T = exp_mod(T, D, N, RN);
#else
	// faster decryption using the CRT
	// T1 = input ^ dP mod P
	// T2 = input ^ dQ mod Q
	mpi	T1	= exp_mod(T, DP, P, RP);
	mpi	T2	= exp_mod(T, DQ, Q, RQ);
	// T = (T1 - T2) * (Q^-1 mod P) mod P
	T	= T1 - T2;
	T1	= T * QP;
	T	= T1 % P;
	// output = T2 + T * Q
	T1	= T * Q;
	T	= T2 + T1;
#endif
	return T.save(output, input.size32());
}

// Add the message padding, then do an RSA operation
bool RSA::encrypt(mode_enum mode, padding_enum padding, const const_memory_block &input, const memory_block &output, vrng &&rng) {
	uint8	*p		= output;
	uint32	len		= input.size32();

	if (padding != V15 || output.length() < len + 11)
		return false;

	int	nb_pad = output.size32() - 3 - len;
	*p++ = 0;
	*p++ = CRYPT;

	while (nb_pad-- > 0) {
		uint8	r;
		for (int i = 100; --i && !rng.set(r););
		if (r == 0)
			return false;
		*p++ = r;
	}
	*p++ = 0;
	input.copy_to(p);

	return mode == PUBLIC
		? public_op(output, output)
		: private_op(output, output);
}

//Do an RSA operation, then remove the message padding
int RSA::decrypt(mode_enum mode, padding_enum padding, const const_memory_block &input, const memory_block &output) {
	uint8 buf[1024];

	if (padding != V15 || input.length() < 16 || input.length() > sizeof(buf))
		return 0;

	if (!(mode == PUBLIC ? public_op(input, buf) : private_op(input, buf)))
		return 0;

	uint8 *p = buf;
	if (*p++ != 0 || *p++ != CRYPT)
		return 0;

	while (*p != 0) {
		if (p > buf + input.length())
			return 0;
		p++;
	}
	p++;

	int	olen = input.size32() - int(p - buf);
	if (olen > output.size32())
		return 0;

	memcpy(output, p, olen);
	return olen;
}

malloc_block RSA::encode_signature(const const_memory_block &input) {
	uint32			len	= input.size32();
	int				k	= N.num_bytes();
	malloc_block	output(k);

	uint8	*p	= output;
	p[0] = 0;
	p[1] = SIGN;
	memset(p + 2, 0xff, k - len - 3);
	p[k - len - 1] = 0;
	input.copy_to(p + k - len);

	private_op(output, output);
	return output;
}

malloc_block RSA::decode_signature(const memory_block &input) {
	int		k	= N.num_bytes();

	if (input.length() != k)
		return 0;

	malloc_block	output(k);
	public_op(input, output);

	uint8	*p = output;
	if (*p++ != 0)
		return 0;
	if (*p++ != SIGN)
		return 0;

	while (*p++ == 0xff);
	if (p[-1] != 0)
		return 0;

	return output.slice(p);
}


} //namespace iso
