#ifndef SRP_H
#define SRP_H

#include "maths/bignum.h"

namespace iso {

struct SRP_group {
	const char	*id;
	mpi			N, g;
	static SRP_group* Find(const mpi& N, const mpi& g);
	static SRP_group* Find(const char* id);
};

struct SRP_entry {
	string	user;
	mpi		salt;
	mpi		verifier;

	SRP_entry(const char *user, const char *pass, const mpi &g, const mpi &N);
};

struct SRP : mod_context {
	mpi			g;
	mpi			ab;


	SRP()	{}
	SRP(const mpi &N, const mpi &g);
	SRP(const SRP_group &group) : SRP(group.N, group.g) {}
	
	bool	CheckNg()				const;
	bool	Check(const mpi &AB)	const;
	mpi		CalcU(const mpi &A, const mpi &B) const;

	//Server
	mpi		ServerCalcB(const mpi &v) const;
	mpi		ServerKey(const mpi &A, const mpi &B, const mpi &v) const;

	//Client:
	mpi		ClientCalcA() const;
	mpi		ClientKey(const mpi &A, const mpi &B, const mpi& salt, const char* user, const char* pass) const;
};

}

#endif //SRP_H
