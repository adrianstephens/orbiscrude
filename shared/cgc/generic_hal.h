#ifndef GENERIC_HAL_H
#define GENERIC_HAL_H

#include "hal.h"

namespace cgc {

struct HAL_generic : HAL {
	void	RegisterNames(CG *cg);
	bool	GetCapsBit(int bitNumber);
	int		GetConnectorID(int name);
	int		GetConnectorUses(int cid, int pid);
	int		GetInternalFunction(Symbol *fSymb, int *group);

	bool	BindUniformUnbound(CG *cg, SourceLoc &loc, Symbol *fSymb, Binding *fBind);
	bool	BindUniformPragma(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind, const Binding *pragma);
	bool	BindVaryingSemantic(CG *cg, SourceLoc &loc, Symbol *fSymb, int semantic, Binding *fBind, bool is_out_val);
	int		GenerateCode(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program);

	HAL_generic();
};

} //namespace cgc

#endif