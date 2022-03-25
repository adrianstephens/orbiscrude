#ifndef RSA_H
#define RSA_H

#include "maths/bignum.h"
#include "extra/random.h"

namespace iso {

struct RSA {
	enum mode_enum {
		PUBLIC	= 0,
		PRIVATE	= 1,
	};
	enum padding_enum {
		V15		= 0,
		V21		= 1,
	};
	enum {
		SIGN	= 1,
		CRYPT	= 2,
	};
	struct parameters {
		mpi e, d, p, q;
	};

	mpi 	N, E;		//	public exponent & modulus
	mpi 	D;			//	private exponent
	mpi 	P, Q;		//	1st & 2nd prime factors
	mpi 	DP, DQ, QP;	//	D % (P - 1), D % (Q - 1), (Q^-1 mod P)
	mpi 	RN, RP, RQ;	//	cached R^2 mod N, R^2 mod P, R^2 mod Q

	RSA() {}
	RSA(const parameters &params);
	void	init(param(mpi) n, param(mpi) e) {
		N = n;
		E = e;
	}
	bool	set_private(param(mpi) d, param(mpi) p, param(mpi) q, param(mpi) dp, param(mpi) dq, param(mpi) qp) {
		D	= d;
		P	= p;
		Q	= q;
		DP	= dp;
		DQ	= dq;
		QP	= qp;
		return check_private_key();
	}
	void	set_private(param(mpi) d, param(mpi) p) {
		D	= d;
		P	= p;
		Q	= inv_mod(P, N);
//		Q	= q;
		DP	= D % (P - 1);
		DQ	= D % (Q - 1);
		QP	= inv_mod(Q, P);
	}
	bool	check_public_key();
	bool	check_private_key();
	bool	public_op(const const_memory_block &input, uint8 *output);
	bool	private_op(const const_memory_block &input, uint8 *output);
	bool	encrypt(mode_enum mode, padding_enum padding, const const_memory_block &input, const memory_block &output, vrng &&rng);
	int		decrypt(mode_enum mode, padding_enum padding, const const_memory_block &input, const memory_block &output);
	malloc_block	encode_signature(const const_memory_block &input);
	malloc_block	decode_signature(const memory_block &signature);
};

} //namespace iso

#endif	// RSA_H
