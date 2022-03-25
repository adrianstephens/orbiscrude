#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cg.h"
#include "support.h"
#include "compile.h"
#include "errors.h"

namespace cgc {

static int CheckFunctionDefinition(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *funSymb, bool IsProgram);

#define NOT_CHECKED     0
#define BEING_CHECKED   1
#define ALREADY_CHECKED 2


// CheckSymbolTree

static int CheckSymbolTree(CG *cg, Scope *fScope, Symbol *fSymb, bool IsProgram) {
	int count = 0;
	if (fSymb) {
		cg->hal->CheckDefinition(cg, fSymb->loc, fSymb->name, fSymb->type);
		count += CheckSymbolTree(cg, fScope, fSymb->left, IsProgram);
		count += CheckSymbolTree(cg, fScope, fSymb->right, IsProgram);
	}
	return count;
}

static int CheckParamsAndLocals(CG *cg, Symbol *funSymb, bool IsProgram) {
	Scope *lScope = funSymb->fun.locals;
	return CheckSymbolTree(cg, lScope, lScope->symbols, IsProgram);
}

struct Check {
	SourceLoc	loc;
	int			count;
	Check(SourceLoc &_loc) : loc(_loc), count(0) {}
};
struct CheckForUndefinedFunctions : Check {
	expr *post_expr(CG *cg, expr *e, int arg2);
	expr *top_expr(CG *cg, expr *e, int arg2) { return cg->ApplyToNodes(*this, e, arg2); }
};
struct CheckForUnsupportedOperators : Check {
	expr *post_expr(CG *cg, expr *e, int arg2);
};
struct CheckForUnsupportedVariables : Check {
	expr *post_expr(CG *cg, expr *e, int arg2);
};
struct CheckForGlobalUniformReferences : Check {
	expr *post_expr(CG *cg, expr *e, int arg2);
};
struct CheckForUnsupportedStatements : Check {
	stmt *pre_stmt(CG *cg, stmt *s, int arg2);
};
struct CheckForReturnStmts : Check {
	stmt *post_stmt(CG *cg, stmt *s, int arg2);
};

// Check an expression nodefor calls to undefined functions.
expr *CheckForUndefinedFunctions::post_expr(CG *cg, expr *fExpr, int arg2) {
	switch (fExpr->kind) {
		case DECL_N:
		case SYMB_N:
		case CONST_N:
		case UNARY_N:
			break;
		case BINARY_N:
			if (fExpr->op == FUN_CALL_OP) {
				expr *lExpr = fExpr->bin.left;
				if (lExpr->kind == SYMB_N) {
					Symbol *lSymb = lExpr->sym.symbol;
					if (IsFunction(lSymb)) {
						if (!(lSymb->properties & SYMB_IS_DEFINED)) {
							SemanticError(cg, loc, ERROR_S_CALL_UNDEF_FUN, cg->GetAtomString(lSymb->name));
							count++;
						} else {
							if (lSymb->flags == BEING_CHECKED) {
								SemanticError(cg, loc, ERROR_S_RECURSION, cg->GetAtomString(lSymb->name));
								count++;
							} else {
								CheckFunctionDefinition(cg, loc, NULL, lSymb, false);
							}
						}
					}
				}
			}
			break;
		case TRINARY_N:
			break;
		default:
			assert(!"bad kind to CheckNodeForUndefinedFunctions()");
			break;
	}
	return fExpr;
}

// Check a node for operators not supported in the target profile.
expr *CheckForUnsupportedOperators::post_expr(CG *cg, expr *fExpr, int arg2) {
	switch (fExpr->kind) {
		case DECL_N:
		case SYMB_N:
		case CONST_N:
			break;
		case UNARY_N:
			if (!cg->hal->IsValidOperator(loc, opcode_atom[fExpr->op], fExpr->op, fExpr->un.subop))
				count++;
			break;
		case BINARY_N:
			if (!cg->hal->IsValidOperator(loc, opcode_atom[fExpr->op], fExpr->op, fExpr->bin.subop))
				count++;
			break;
		case TRINARY_N:
			if (!cg->hal->IsValidOperator(loc, opcode_atom[fExpr->op], fExpr->op, fExpr->tri.subop))
				count++;
			break;
		default:
			assert(!"bad kind to CheckNodeForUnsupportedOperators()");
			break;
	}
	return fExpr;
}

// Check for references to object that are not supported by the target profile.
expr *CheckForUnsupportedVariables::post_expr(CG *cg, expr *fExpr, int arg2) {
	switch (fExpr->kind) {
		case SYMB_N:
			if (fExpr->op == VARIABLE_OP) {
				///lSymb = fExpr->sym.symbol;
			}
			break;
		case DECL_N:
		case CONST_N:
		case UNARY_N:
		case BINARY_N:
		case TRINARY_N:
			break;
		default:
			assert(!"bad kind to CheckForUnsupportedVariables()");
			break;
	}
	return fExpr;
}

// Check for references to previously unreferenced non-static uniform global variables.
// These must have explicit or implied semantics
// Add them to to the $uniform connector and insert an initialization statement at the start of main.
expr *CheckForGlobalUniformReferences::post_expr(CG *cg, expr *fExpr, int arg2) {
	switch (fExpr->kind) {
		case SYMB_N:
			if ((fExpr->op == VARIABLE_OP) && (fExpr->sym.symbol->properties & SYMB_NEEDS_BINDING))
				// This is a non-static global and has not yet been bound
				BindDefaultSemantic(cg, fExpr->sym.symbol);
			break;
		default:
			break;
	}
	return fExpr;
}

// Issue an error if a return statement is encountered.
stmt *CheckForReturnStmts::post_stmt(CG *cg, stmt *fStmt, int arg2) {
	if (fStmt->kind == RETURN_STMT)
		SemanticError(cg, fStmt->loc, ERROR___RETURN_NOT_LAST);
	return fStmt;
}

// Issue an error if an unsupported statement is encountered.
stmt *CheckForUnsupportedStatements::pre_stmt(CG *cg, stmt *fStmt, int arg2) {
	if (fStmt) {
		if (!cg->hal->CheckStatement(cg, fStmt->loc, fStmt))
			++count;
	}
	return fStmt;
}

static int CheckFunctionDefinition(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *funSymb, bool IsProgram) {
	Check	check(loc);

	if (funSymb->flags == NOT_CHECKED) {
		funSymb->flags = BEING_CHECKED;
		stmt *lStmt = funSymb->fun.statements;
		CheckParamsAndLocals(cg, funSymb, IsProgram);

//		if (IsProgram) {
//			BuildReturnAssignments	lstr(fScope, funSymb);
//			funSymb->fun.statements = lStmt = cg->ApplyToStatements(lstr, lStmt);
//		}

		cg->ApplyToTopExpressions((CheckForUndefinedFunctions&)check, lStmt);
		cg->ApplyToExpressions((CheckForUnsupportedOperators&)check, lStmt);
		cg->ApplyToExpressions((CheckForUnsupportedVariables&)check, lStmt);
		cg->ApplyToExpressions((CheckForGlobalUniformReferences&)check, lStmt);
		cg->ApplyToStatements((CheckForUnsupportedStatements&)check, lStmt);

		if (cg->hal->GetCapsBit(CAPS_RESTRICT_RETURNS)) {
			while (lStmt) {
				if (lStmt->next)
					((CheckForReturnStmts&)check).post_stmt(cg, lStmt, 0);
				cg->ApplyToChildStatements((CheckForReturnStmts&)check, lStmt);
				lStmt = lStmt->next;
			}
		}
		funSymb->flags = ALREADY_CHECKED;
	}
	return check.count;
}

/*
* CheckFunctionDefinitions() - Walk a function and check for errors:
*     1. see if any functions it calls aren't defined,
*     2. detect recursion,
*     3. detect early return statements,
*     4. check for unsupported operators in the target profile.
*/
int CheckFunctionDefinitions(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program) {
	SetSymbolFlagsList(fScope, NOT_CHECKED);
	return CheckFunctionDefinition(cg, loc, fScope, program, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Check Connector Usage ////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

struct CheckConnectorUsage {
	SourceLoc loc;
	expr *top_expr(CG *cg, expr *e, int writing = 0);
	CheckConnectorUsage(SourceLoc &_loc) : loc(_loc) {}
};

// Check connector usage for illegal references.
expr *CheckConnectorUsage::top_expr(CG *cg, expr *fExpr, int writing) {
	expr	*lExpr			= fExpr;
	if (fExpr) {
		switch (fExpr->kind) {
			case SYMB_N: {
				Symbol *lSymb = lExpr->sym.symbol;
				if (lSymb->properties & SYMB_IS_CONNECTOR_REGISTER) {
					if (!(lSymb->properties & (SYMB_CONNECTOR_CAN_WRITE | SYMB_CONNECTOR_CAN_READ))) {
						SemanticError(cg, loc, ERROR_S_CMEMBER_NOT_VISIBLE, cg->GetAtomString(lSymb->name));
					} else {
						if (writing) {
							if (!(lSymb->properties & SYMB_CONNECTOR_CAN_WRITE)) {
								SemanticError(cg, loc, ERROR_S_CMEMBER_NOT_WRITABLE, cg->GetAtomString(lSymb->name));
							}
						} else {
							if (!(lSymb->properties & SYMB_CONNECTOR_CAN_READ)) {
								SemanticError(cg, loc, ERROR_S_CMEMBER_NOT_READABLE, cg->GetAtomString(lSymb->name));
							}
						}
					}
				}
				break;
			}
			case CONST_N:
				break;
			case UNARY_N:
				fExpr->un.arg = top_expr(cg, fExpr->un.arg, writing);
				break;
			case BINARY_N:
				switch (fExpr->op) {
					case MEMBER_SELECTOR_OP:
						lExpr = fExpr->bin.right;
						if (lExpr && lExpr->kind == SYMB_N && writing) {
							// Mark connector registers that are written.
							Symbol *lSymb = lExpr->sym.symbol;
							Binding *lBind = lSymb->var.bind;
							if (lBind)
								lBind->properties |= BIND_WAS_WRITTEN;
						}
						fExpr->bin.left = top_expr(cg, fExpr->bin.left, writing);
						fExpr->bin.right = top_expr(cg, fExpr->bin.right, writing);
						lExpr = fExpr;
						break;
					case ASSIGN_OP:
					case ASSIGN_V_OP:
					case ASSIGN_GEN_OP:
					case ASSIGN_MASKED_KV_OP:
						fExpr->bin.left = top_expr(cg, fExpr->bin.left, 1);
						fExpr->bin.right = top_expr(cg, fExpr->bin.right, 0);
						break;
					case FUN_ARG_OP:
						fExpr->bin.left = top_expr(cg, fExpr->bin.left, SUBOP_GET_MASK(fExpr->bin.subop) & 2 ? 1 : 0);
						fExpr->bin.right = top_expr(cg, fExpr->bin.right, 0);
						break;
					default:
						fExpr->bin.left = top_expr(cg, fExpr->bin.left, writing);
						fExpr->bin.right = top_expr(cg, fExpr->bin.right, writing);
						break;
				}
				break;
			case TRINARY_N:
				switch (fExpr->op) {
					case ASSIGN_COND_OP:
					case ASSIGN_COND_V_OP:
					case ASSIGN_COND_SV_OP:
					case ASSIGN_COND_GEN_OP:
						fExpr->tri.arg1 = top_expr(cg, fExpr->tri.arg1, 1);
						fExpr->tri.arg2 = top_expr(cg, fExpr->tri.arg2, 0);
						fExpr->tri.arg3 = top_expr(cg, fExpr->tri.arg3, 0);
						break;
					default:
						fExpr->tri.arg1 = top_expr(cg, fExpr->tri.arg1, writing);
						fExpr->tri.arg2 = top_expr(cg, fExpr->tri.arg2, writing);
						fExpr->tri.arg3 = top_expr(cg, fExpr->tri.arg3, writing);
						break;
				}
				break;
			default:
				FatalError(cg, "bad kind to CheckConnectorUsage()");
				break;
		}
	}
	return lExpr;
}

// Check connector usage for illegal references.
void CheckConnectorUsageMain(CG *cg, Symbol *program, stmt *fStmt) {
	Symbol *outConn = cg->varyingOut;
	if (!outConn || !outConn->type)
		return;

	Type *cType = outConn->type;
	CheckConnectorUsage	check(program->loc);
	cg->ApplyToTopExpressions(check, fStmt);
	Symbol *lSymb = cg->varyingOut->type->str.members->symbols;
	// This doesn't work!  The output value is always written by the return statement!  RSG
	while (lSymb) {
		if (Binding *lBind = lSymb->var.bind) {
			if ((lBind->properties & BIND_WRITE_REQUIRED) && !(lBind->properties & BIND_WAS_WRITTEN))
				SemanticWarning(cg, program->loc, WARNING_S_CMEMBER_NOT_WRITTEN, cg->GetAtomString(lBind->rname));
		}
		lSymb = lSymb->next;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

// Check for references to varying l-values with the "hidden" semantic bit set.  These shouldn't be referenced in this profile.
void CheckForHiddenVaryingReferences(CG *cg, stmt *fStmt) {
	struct Check {
		expr *post_expr(CG *cg, expr *fExpr, int arg2) {
			switch (fExpr->kind) {
				case SYMB_N:
					if (fExpr->op == VARIABLE_OP || fExpr->op == MEMBER_OP) {
						Symbol *lSymb = fExpr->sym.symbol;
						if (lSymb->kind == VARIABLE_S) {
							Type	*lType = fExpr->type;
							Binding	*lBind = lSymb->var.bind;
							if (lBind && lBind->properties & BIND_HIDDEN)
								SemanticError(cg, lSymb->loc, ERROR_SS_VAR_SEMANTIC_NOT_VISIBLE, cg->GetAtomString(lSymb->name), cg->GetAtomString(lBind->rname));
						}
					}
					break;
				default:
					break;
			}
			return fExpr;
		}
		expr *pre_expr(CG *cg, expr *fExpr, int arg2) { return fExpr; }
	} check;
	cg->ApplyToExpressions(check, fStmt);
}

} //namespace cgc
