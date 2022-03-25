#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cg.h"
#include "compile.h"
#include "printutils.h"

namespace cgc {

// Convert return statements into assignments.
struct ConvertReturnStatement {
	Symbol *retSymb;

	stmt *post_stmt(CG *cg, stmt *fStmt, int arg2) {
		stmt *lStmt = 0;
		if (fStmt) {
			switch (fStmt->kind) {
				case RETURN_STMT:
					if (retSymb) {
						expr	*lExpr		= NewSymbNode(cg, VARIABLE_OP, retSymb);
						lStmt	= NewSimpleAssignmentStmt(cg, cg->lastSourceLoc, lExpr, fStmt->exprst.exp, 1);
					}
					break;
				default:
					lStmt = fStmt;
					break;
			}
		}
		return lStmt;
	}
	ConvertReturnStatement(Symbol *_retSymb) : retSymb(_retSymb) {}
};

#define NOT_DUPLICATED          0
#define ALREADY_DUPLICATED      1
#define IN_MASTER_SCOPE         2
#define IN_GLOBAL_SCOPE         3
#define IN_SUPER_GLOBAL_SCOPE   4
	#define TEMP_ROOT      "$temp"

// Convert return statements into assignments.
// Assume that this can be done in place since we're working with a copy of the original symb structs.

struct InlineFunction {
	Scope		*scope;
	int			index;

	expr *post_expr(CG *cg, expr *fExpr, int arg2 = 0) {
		expr			*lExpr = fExpr;

		if (fExpr) {
			switch (fExpr->kind) {
			case SYMB_N: {
				Symbol *lSymb = lExpr->sym.symbol, *nSymb;
				if (fExpr->op == VARIABLE_OP && lSymb->kind == VARIABLE_S) {
					Symbol *nSymb = 0;
					switch (lSymb->flags) {
					case NOT_DUPLICATED: {
						int	name = GetNumberedAtom(cg, cg->GetAtomString(lSymb->name), index, 4, '-');
						if (LookUpLocalSymbol(cg, scope, name)) {
							InternalError(cg, cg->lastSourceLoc, 9999, "Name \"%s\"-%04d shouldn't be defined, but is!", cg->GetAtomString(lSymb->name), index);
							if (cg->opts & cgclib::DUMP_PARSETREE) {
								InternalError(cg, cg->lastSourceLoc, 9999, "*** Scope %d definitions ***", scope->level);
								PrintSymbolTree(cg, scope->symbols);
								InternalError(cg, cg->lastSourceLoc, 9999, "*** End of Scope %d ***", scope->level);
							}
						}
						// Remove qualifiers if any present:
						Type *lType = lSymb->type;
						int	qualifiers = GetQualifiers(lType);
						if (qualifiers) {
							lType = DupType(lType);
							lType->properties &= ~TYPE_QUALIFIER_MASK;
						}
						nSymb = DefineVar(cg, lSymb->loc, scope, name, lType);
						nSymb->properties = lSymb->properties;
						nSymb->flags = IN_MASTER_SCOPE;
						lSymb->tempptr = (void *) nSymb;
						lSymb->flags = ALREADY_DUPLICATED;
						break;
					}
					case IN_MASTER_SCOPE:
					case IN_GLOBAL_SCOPE:
					case IN_SUPER_GLOBAL_SCOPE:
						nSymb = lSymb;
						break;
					case ALREADY_DUPLICATED:
						nSymb = (Symbol *) lSymb->tempptr;
						break;
					default:
						FatalError(cg, "Bad scope in ConvertLocalReferences()");
						break;
					}
					lExpr->sym.symbol = nSymb;
				}
				break;
			}
			case CONST_N:
			case UNARY_N:
			case BINARY_N:
			case TRINARY_N:
				break;
			case DECL_N:
			default:
				assert(!"bad kind to ConvertLocalReferences()");
				break;
			}
		}
		return lExpr;
	}

	InlineFunction(Scope *_scope, int _index) : scope(_scope), index(_index) {
	}
};

struct ExpandInlines {
	Scope		*superGlobalScope;
	Scope		*globalScope;
	Scope		*masterScope;
	Scope		*calleeScope;
	int			*nextFunInlineIndex;
	int			*nextTempIndex;
	int			inlineAll;
	StmtList	statements;
	void		*scratch;

	void AddComment(CG *cg, const char *str) {
		AppendStatements(&statements, NewCommentStmt(cg, cg->lastSourceLoc, str));
	}
	void Expand(CG *cg, Symbol *funSymb, Symbol *retSymb, expr *fActuals);

	// Expand inline function calls in an expression.
	expr *post_expr(CG *cg, expr *fExpr, int arg2) {
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
						Symbol *retSymb = 0;
						assert(IsFunction(lSymb));
						if (inlineAll || IsInline(lSymb)) {
	//						printf("inlining %s\n", cg->GetAtomString(lSymb->name));
							Type *funType = lSymb->type;
							Type *retType = funType->fun.rettype;
							if (!IsVoid(retType)) {
								int	vname = GetNumberedAtom(cg, TEMP_ROOT, (*nextTempIndex)++, 4, '\0');
								retSymb = DefineVar(cg, lSymb->loc, masterScope, vname, retType);
							}
							if (cg->opts & cgclib::GENERATE_COMMENTS) {
								AddComment(cg, "Begin inline function");
								AddComment(cg, cg->GetAtomString(lSymb->name));
							}
							Expand(cg, lSymb, retSymb, fExpr->bin.right);
							if (cg->opts & cgclib::GENERATE_COMMENTS) {
								AddComment(cg, "End inline function");
								AddComment(cg, cg->GetAtomString(lSymb->name));
							}
							if (retSymb) {
								fExpr =  NewSymbNode(cg, VARIABLE_OP, retSymb);
							} else {
								fExpr = NULL; // function returning void
							}
						} else {
							// Not done yet:  Must continue to traverse call tree to find other
							// functions that are called and inline their calls as needed.
							// Or some such stuff...
						}
					}
				}
			case TRINARY_N:
				break;
			default:
				assert(!"bad kind to ExpandInlineFunctionCallsNode()");
				break;
		}
		return fExpr;
	}


	// Expand inline function calls in a statement.
	stmt *post_stmt(CG *cg, stmt *fStmt, int arg2) {
		statements.first	= NULL;
		statements.last		= NULL;
		cg->ApplyToExpressionsLocal(*this, fStmt, arg2);
		if (statements.first) {
			statements.last->next = fStmt;
			fStmt = statements.first;
		}
		statements.first	= NULL;
		statements.last		= NULL;
		return fStmt;
	}

	ExpandInlines(Scope *fscope, int &funcount, int &tempcount, bool inline_all) {
		globalScope			= fscope->parent;
		superGlobalScope	= globalScope->parent;
		masterScope			= fscope;
		calleeScope			= NULL;
		nextFunInlineIndex	= &funcount;
		nextTempIndex		= &tempcount;
		inlineAll			= inline_all;

		scratch		= NULL;
	}
};

// Expand a function inline by duplicating its statements.
void ExpandInlines::Expand(CG *cg, Symbol *funSymb, Symbol *retSymb, expr *fActuals) {

	// Duplicate the body of the function:

	stmt *body = DuplicateStatementTree(cg, funSymb->fun.statements);

	// Change return statements into assignments:

	ConvertReturnStatement	conv(retSymb);
	body = cg->ApplyToStatements(conv, body);

	// Add numbered copies of expanded function's local variables to current scope,
	// and convert any references in copied body of function to new symbols:

	SetSymbolFlagsList(Scope::list/*global_scope*/, NOT_DUPLICATED);
	SetSymbolFlags(masterScope->symbols, IN_MASTER_SCOPE);
	SetSymbolFlags(globalScope->symbols, IN_GLOBAL_SCOPE);
	SetSymbolFlags(superGlobalScope->symbols, IN_SUPER_GLOBAL_SCOPE);

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	// !!! BEGIN !!! Temporary patch for "texobj" parameters!!! Don't assign!!!
	Symbol	*lFormal = funSymb->fun.params;
	expr	*lActual = fActuals;
	while (lFormal) {
		if (GetBase(lFormal->type) == TYPE_BASE_TEXOBJ) {
			assert(lActual->bin.left->kind == SYMB_N);
			lFormal->tempptr = (void*)lActual->bin.left->sym.symbol;
			lFormal->flags = ALREADY_DUPLICATED;
		}
		lFormal = lFormal->next;
		lActual = lActual->bin.right;
	}
	// !!! END !!! Temporary patch for "texobj" parameters!!! Don't assign!!!
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	// Bump function counter:
	InlineFunction	infn(masterScope, (*nextFunInlineIndex)++);
	cg->ApplyToExpressions(infn, body);

	// Assign actual parameter expressions to parameters:

	StmtList newStmts;
	lFormal = funSymb->fun.params;
	lActual = fActuals;
	while (lFormal) {
		assert(lActual->kind == BINARY_N);
		assert(lActual->op == FUN_ARG_OP);
		if (((GetQualifiers(lFormal->type) & TYPE_QUALIFIER_IN) ||
			 !(GetQualifiers(lFormal->type) & TYPE_QUALIFIER_OUT)) &&
			//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
			//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
			//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
			// !!! BEGIN !!! Temporary patch for "texobj" parameters!!! Don't assign!!!
			 (GetBase(lFormal->type) != TYPE_BASE_TEXOBJ) &&
			// !!! Temporary patch for "texobj" parameters!!! Don't assign!!!
			//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			 1)
		{
			expr	*lExpr = NewSymbNode(cg, VARIABLE_OP, lFormal);
			lExpr = infn.post_expr(cg, lExpr);
			AppendStatements(&newStmts, NewSimpleAssignmentStmt(cg, cg->lastSourceLoc, lExpr, lActual->bin.left, 1));
		}
		lFormal = lFormal->next;
		lActual = lActual->bin.right;
	}
	AppendStatements(&newStmts, body);

	// Build assignment statements to copy "out" param's final values to actual parameters:

	lFormal = funSymb->fun.params;
	lActual = fActuals;
	while (lFormal) {
		if (GetQualifiers(lFormal->type) & TYPE_QUALIFIER_OUT) {
			expr	*lExpr = NewSymbNode(cg, VARIABLE_OP, lFormal);
			lExpr = infn.post_expr(cg, lExpr);
			AppendStatements(&newStmts, NewSimpleAssignmentStmt(cg, cg->lastSourceLoc, lActual->bin.left, lExpr, 0));
		}
		lFormal = lFormal->next;
		lActual = lActual->bin.right;
	}

	// Recursively expands calls in newly inlined finction:

	ExpandInlines lFunData		= *this;
	lFunData.calleeScope		= funSymb->fun.locals;
	lFunData.statements.first	= NULL;
	lFunData.statements.last	= NULL;
	lFunData.scratch			= NULL;
	body = cg->ApplyToStatements(lFunData, newStmts.first);

	// Do some more stuff here...

	// Append the new statements:

	AppendStatements(&statements, body);
}


// Recursively walk a program'm call graph from main expanding function calls that have the attribute "inline".
// Assumes no recursion in inlined functions.
stmt *ExpandInlineFunctionCalls(CG *cg, Scope *fscope, stmt *body, bool inline_all) {
	int funcount = 0, tempcount = 0;
	ExpandInlines lFunData(fscope, funcount, tempcount, inline_all);
	return cg->ApplyToStatements(lFunData, body);
}

} //namespace cgc
