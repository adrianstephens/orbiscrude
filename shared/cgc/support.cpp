#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "support.h"
#include "cg.h"
#include "compile.h"
#include "printutils.h"
#include "errors.h"

using namespace cgc;
#include "parser.hpp"

namespace cgc {

#undef PICK
#define PICK(a, b, c, d, e, f) b
const char *opcode_name[] = { OPCODE_TABLE };

#undef PICK
#define PICK(a, b, c, d, e, f) c
int const opcode_atom[] = { OPCODE_TABLE };

#undef PICK
#define PICK(a, b, c, d, e, f) d
const char*opcode_operator[] = { OPCODE_TABLE };

#undef PICK
#define PICK(a, b, c, d, e, f) e
const nodekind opcode_nodekind[] = { OPCODE_TABLE };

#undef PICK
#define PICK(a, b, c, d, e, f) f
const subopkind opcode_subopkind[] = { OPCODE_TABLE };


#undef PICK

bool IsNullSwizzle(int mask, int len) {
	int	mask2 = (1 << (2 * (len + (len==0)))) - 1;
	return (mask & mask2) == (((0<<0) | (1<<2) | (2<<4) | (3<<6)) & mask2);
}

bool AllSameCol(int mask, int len) {
	if (len > 1) {
		int	mask2 = (1 << (4 * len)) - 1;
		return (mask & 0x3333 & mask2) % (0x1111 & mask2) == 0;
	}
	return 1;
}

bool AllSameRow(int mask, int len) {
	return AllSameCol(mask << 2, len);
}

int GetRowSwizzle(int mask, int len) {
	int	result	= 0;
	for (int i = 0, three = 3; i == 0 || i < len; i++, mask >>= 2, three <<= 2)
		result |= mask & three;
	return result;
}

int GetColSwizzle(int mask, int len) {
	return GetRowSwizzle(mask >> 2, len);
}

//	decl

decl::decl(SourceLoc &_loc, int _name, dtype *_type) {
	memset(this, 0, sizeof(*this));
	kind	= DECL_N;
	loc		= _loc;
	name	= _name;
	if (_type)
		type	= *_type;
}

decl *NewDeclNode(SourceLoc &loc, int atom, dtype *type) {
	return new decl(loc, atom, type);
}

int GetOperatorName(CG *cg, int op) {
	char	buffer[64];
	sprintf(buffer, "operator%s", opcode_operator[op]);
	return cg->AddAtom(buffer);
}
//	expr

expr *DupNode(MemoryPool *pool, expr *e) {
	return new(pool) expr(*e);
}

expr::expr(nodekind _kind) {
	memset(this, 0, sizeof(*this));
	kind			= _kind;
}
expr::expr(opcode _op, nodekind _kind) {
	assert(opcode_nodekind[_op] == _kind);
	memset(this, 0, sizeof(*this));
	kind			= _kind;
	op				= _op;
}
expr *NewSymbNode(CG *cg, opcode op, Symbol *fSymb) {
	expr *e = new(cg->pool()) expr(op, SYMB_N);
	e->type			= fSymb->type;
	e->is_lvalue	= true;
	e->is_const		= !!(GetQualifiers(e->type) & TYPE_QUALIFIER_CONST);
	e->sym.symbol	= fSymb;
	return e;
}
expr *NewIConstNode(CG *cg, opcode op, int fval, int base) {
	expr *e = new(cg->pool()) expr(op, CONST_N);
	e->type = GetStandardType(cg->hal, base, 0, 0);
	e->is_const		= true;
	e->co.subop		= SUBOP__(base);
	e->co.val[0].i	= fval;
	return e;
}
expr *NewBConstNode(CG *cg, opcode op, int fval, int base) {
	expr *e = new(cg->pool()) expr(op, CONST_N);
	e->type			= GetStandardType(cg->hal, base, 0, 0);
	e->is_const		= true;
	e->co.subop		= SUBOP__(base);
	e->co.val[0].i = fval;
	return e;
}
expr *NewFConstNode(CG *cg, opcode op, float fval, int base) {
	expr *e = new(cg->pool()) expr(op, CONST_N);
	e->type			= GetStandardType(cg->hal, base, 0, 0);
	e->is_const		= true;
	e->co.subop		= SUBOP__(base);
	e->co.val[0].f	= fval;
	return e;
}
expr *NewFConstNodeV(CG *cg, opcode op, float *fval, int len, int base) {
	expr *e = new(cg->pool()) expr(op, CONST_N);
	e->type			= GetStandardType(cg->hal, base, len, 0);
	e->is_const		= true;
	e->co.subop		= SUBOP_V(len, base);
	for (int i = 0; i < len; i++)
		e->co.val[i].f = fval[i];
	return e;
}
expr *NewUnopNode(CG *cg, opcode op, expr *arg) {
	expr *e = new(cg->pool()) expr(op, UNARY_N);
	e->has_sideeffects = arg && arg->has_sideeffects;
	e->un.arg		= arg;
	return e;
}
expr *NewUnopSubNode(CG *cg, opcode op, int subop, expr *arg) {
	expr *e = new(cg->pool()) expr(op, UNARY_N);
	e->has_sideeffects = arg && arg->has_sideeffects;
	e->un.subop		= subop;
	e->un.arg		= arg;
	return e;
}
expr *NewBinopNode(CG *cg, opcode op, expr *left, expr *right) {
	expr *e = new(cg->pool()) expr(op, BINARY_N);
	e->has_sideeffects	= left && left->has_sideeffects;
	e->has_sideeffects	= right && right->has_sideeffects;
	e->bin.left			= left;
	e->bin.right		= right;
	return e;
}
expr *NewBinopSubNode(CG *cg, opcode op, int subop, expr *left, expr *right) {
	expr *e = new(cg->pool()) expr(op, BINARY_N);
	e->has_sideeffects	= left && left->has_sideeffects;
	e->has_sideeffects	= right && right->has_sideeffects;
	e->bin.subop		= subop;
	e->bin.left			= left;
	e->bin.right		= right;
	return e;
}
expr *NewTriopNode(CG *cg, opcode op, expr *arg1, expr *arg2, expr *arg3) {
	expr *e = new(cg->pool()) expr(op, TRINARY_N);
	e->has_sideeffects	= arg1 && arg1->has_sideeffects;
	e->has_sideeffects	= arg2 && arg2->has_sideeffects;
	e->has_sideeffects	= arg3 && arg3->has_sideeffects;
	e->tri.arg1			= arg1;
	e->tri.arg2			= arg2;
	e->tri.arg3			= arg3;
	return e;
}
expr *NewTriopSubNode(CG *cg, opcode op, int subop, expr *arg1, expr *arg2, expr *arg3) {
	expr *e = new(cg->pool()) expr(op, TRINARY_N);
	e->has_sideeffects	= arg1 && arg1->has_sideeffects;
	e->has_sideeffects	= arg2 && arg2->has_sideeffects;
	e->has_sideeffects	= arg3 && arg3->has_sideeffects;
	e->tri.subop		= subop;
	e->tri.arg1			= arg1;
	e->tri.arg2			= arg2;
	e->tri.arg3			= arg3;
	return e;
}
expr *SymbolicConstant(CG *cg, int name, int name2) {
	expr *e = new(cg->pool()) expr(SYMBOLIC_N);
	e->blx.name			= name;
	e->blx.name2		= name2;
	return e;
}

expr *BasicVariable(CG *cg, int name) {
	Symbol *lSymb = LookUpSymbol(cg, cg->current_scope, name);
	if (!lSymb) {
		// check in cbuffers
		for (Symbol *c = cg->global_scope->params; c; c = c->next) {
			if (lSymb = LookUpLocalSymbol(cg, c->type->str.members, name)) {
				expr *fExpr = NewSymbNode(cg, VARIABLE_OP, c);
				expr *mExpr = NewSymbNode(cg, MEMBER_OP, lSymb);
				expr *lExpr = NewBinopNode(cg, MEMBER_SELECTOR_OP, fExpr, mExpr);
				lExpr->is_lvalue = fExpr->is_lvalue;
				lExpr->is_const = fExpr->is_const;
				lExpr->type		= lSymb->type;
				return lExpr;
			}
		}
		SemanticError(cg, cg->tokenLoc, ERROR_S_UNDEFINED_VAR, cg->GetAtomString(name));
		lSymb = DefineVar(cg, cg->tokenLoc, cg->current_scope, name, UndefinedType);
	}
	return NewSymbNode(cg, VARIABLE_OP, lSymb);
}

//	stmt

stmt::stmt(stmtkind _kind, SourceLoc &_loc) {
	memset(this, 0, sizeof(*this));
	kind			= _kind;
	loc				= _loc;
}

stmt *NewExprStmt(SourceLoc &loc, expr *fExpr) {
	stmt *s = new stmt(EXPR_STMT, loc);
	s->exprst.exp = fExpr;
	return s;
}

stmt *NewIfStmt(SourceLoc &loc, expr *fExpr, stmt *thenstmt, stmt *elsestmt) {
	stmt *s = new stmt(IF_STMT, loc);
	s->ifst.cond = fExpr;
	s->ifst.thenstmt = thenstmt;
	s->ifst.elsestmt = elsestmt;
	return s;
}

stmt *SetThenElseStmts(stmt *s, stmt *thenstmt, stmt *elsestmt) {
	assert(s->kind == IF_STMT);
	s->ifst.thenstmt = thenstmt;
	s->ifst.elsestmt = elsestmt;
	return s;
}

stmt *NewWhileStmt(SourceLoc &loc, stmtkind kind, expr *fExpr, stmt *body) {
	stmt *s = new stmt(kind, loc);
	s->whilest.cond = fExpr;
	s->whilest.body = body;
	return s;
}

stmt *NewForStmt(SourceLoc &loc, stmt *fexpr1, expr *fexpr2, stmt *fexpr3, stmt *body) {
	stmt *s = new stmt(FOR_STMT, loc);
	s->forst.init = fexpr1;
	s->forst.cond = fexpr2;
	s->forst.step = fexpr3;
	s->forst.body = body;
	return s;
}

stmt *NewBlockStmt(SourceLoc &loc, stmt *fStmt, Scope *fScope) {
	stmt *s = new stmt(BLOCK_STMT, loc);
	s->blockst.body = fStmt;
	s->blockst.scope = fScope;
	return s;
}

stmt *NewReturnStmt(CG *cg, SourceLoc &loc, Scope *fScope, expr *fExpr) {
	if (fScope  && (fScope = fScope->func_scope)) {
		fScope->flags |= Scope::has_return;
		if (fScope->rettype) {
			if (fScope->rettype == VoidType) {
				if (fExpr) {
					SemanticError(cg, loc, ERROR___VOID_FUN_RETURNS_VALUE);
				}
			} else if (fScope->rettype != UndefinedType) {
				expr *lExpr;
				if (ConvertType(cg, fExpr, fScope->rettype, fExpr->type, &lExpr, false, false)) {
					fExpr = lExpr;
				} else {
					SemanticError(cg, loc, ERROR___RETURN_EXPR_INCOMPAT);
				}
			}
		}
	}
	stmt *s = new stmt(RETURN_STMT, loc);
	s->returnst.exp = fExpr;
	return s;
}

stmt *NewDiscardStmt(SourceLoc &loc, expr *fExpr) {
	stmt *s = new stmt(DISCARD_STMT, loc);
	s->discardst.cond = fExpr;
	return s;
}

stmt *NewBreakStmt(SourceLoc &loc) {
	stmt *s = new stmt(BREAK_STMT, loc);
	return s;
}

stmt *NewCommentStmt(CG *cg, SourceLoc &loc, const char *str) {
	stmt *s = new stmt(COMMENT_STMT, loc);
	s->commentst.str = cg->AddAtom(str);
	return s;
}

stmt *NewSwitchStmt(SourceLoc &loc, expr *fExpr, stmt *fStmt, Scope *fScope) {
	return fStmt;
}

/************************************* dtype functions: *************************************/
// Strange function that returns a pointer to the type defined by it's argument.  There are 2 cases:
//
// A) is_derived is TRUE:  This type is a stack-frame resident copy of another type.
//          It has been modified by a qualifier, etc., and does not have a copy in the heap.
//          Copy the contents into a freshly malloc'ed type and return it's address.
// B) is_derived is FALSE: This type is the same as that pointed to by "base".  Return "base".

Type *GetTypePointer(CG *cg, SourceLoc &loc, const dtype *fDtype) {
	if (fDtype) {
		if (fDtype->is_derived) {
			cg->hal->CheckDeclarators(cg, loc, fDtype);
			Type	*t		= new(cg->pool()) Type(fDtype->type);
			t->properties	&= ~(TYPE_MISC_TYPEDEF | TYPE_MISC_PACKED_KW);
			t->size			= cg->hal->GetSizeof(t);
			return t;
		}
		return fDtype->basetype;
	}
	return UndefinedType;
}

dtype *SetDType(dtype *fDtype, Type *fType) {
	fDtype->basetype		= fType;
	fDtype->is_derived		= false;
	fDtype->num_new_dims	= 0;
	fDtype->storage			= SC_UNKNOWN;
	fDtype->type			= *fType;
	return fDtype;
}

dtype *NewDType(dtype *fDtype, Type *baseType, int category) {
	fDtype->basetype		= baseType;
	fDtype->is_derived		= true;
	fDtype->num_new_dims	= 0;
	fDtype->storage			= SC_UNKNOWN;
	memset(&fDtype->type, 0, sizeof(fDtype->type));
	fDtype->type.properties = category;
	return fDtype;
}

/********************************** Parser Semantic Rules: ***********************************/

// Create an EXPR_LIST_OP node with an expression argument.
expr *Initializer(CG *cg, expr *fExpr) {
	return NewBinopNode(cg, EXPR_LIST_OP, fExpr, NULL);
}

expr *StateInitializer(CG *cg, int name, expr *fExpr) {
	expr *lExpr = NewBinopNode(cg, EXPR_LIST_OP, fExpr, NULL);
	lExpr->bin.subop = name;
//	lExpr->bin.tempptr[0] = (void*)name;
	return lExpr;
}

// Add an expression to a list of expressions.  Either can be NULL.
// Assumes that the nodes on list are EXPR_LIST_OP binary nodes.
expr *InitializerList(expr *list, expr *last) {
	if (expr *lExpr = list) {
		if (last) {
			while (lExpr->bin.right)
				lExpr = lExpr->bin.right;
			lExpr->bin.right = last;
		}
		return list;
	} else {
		return last;
	}
}

// Add an actual argument to a list of parameters
expr *ArgumentList(CG *cg, expr *flist, expr *fExpr) {
	expr	*nExpr		= NewBinopNode(cg, FUN_ARG_OP, fExpr, NULL);
	nExpr->type			= fExpr->type;
	nExpr->is_lvalue	= is_lvalue(fExpr);
	nExpr->is_const		= !!(GetQualifiers(nExpr->type) & TYPE_QUALIFIER_CONST);
	if (expr *lExpr = flist) {
		while (lExpr->bin.right)
			lExpr = lExpr->bin.right;
		lExpr->bin.right = nExpr;
		return flist;
	} else {
		return nExpr;
	}
}

// Add an expression to the end of a list of expressions
expr *ExpressionList(CG *cg, expr *fList, expr *fExpr) {
	expr	*nExpr = NewBinopNode(cg, EXPR_LIST_OP, fExpr, NULL);
	nExpr->type = fExpr->type;
	if (expr *lExpr = fList) {
		while (lExpr->bin.right)
			lExpr = lExpr->bin.right;
		lExpr->bin.right = nExpr;
		return fList;
	} else {
		return nExpr;
	}
}

// Add an actual argument to a list of parameters
TypeList *AddtoTypeList(CG *cg, TypeList *list, Type *type) {
	TypeList *tl	= new(cg->pool()) TypeList(type);
	if (!list)
		return tl;

	while (TypeList *next = list->next)
		list = next;

	list->next = tl;
	return list;
}

// Add a declaration to a list of declarations - either can be NULL
decl *AddDecl(decl *first, decl *last) {
	if (first) {
		if (last) {
			decl *lDecl = first;
			while (lDecl->next)
				lDecl = lDecl->next;
			lDecl->next = last;
		}
		return first;
	} else {
		return last;
	}
}

// Add a list of statements to then end of another list.  Either can be NULL.
stmt *AddStmt(stmt *first, stmt *last) {
	if (first) {
		if (last) {
			stmt *lStmt = first;
			while (lStmt->next)
				lStmt = lStmt->next;
			lStmt->next = last;
		}
		return first;
	} else {
		return last;
	}
}

// See if this statement is supported by the target profile.
stmt *CheckStmt(stmt *fStmt) {
	// Can't do it here.  Must wait until we know which functions are being used.
	//if (fStmt)
	//    hal->CheckStatement(&fStmt->loc, fStmt);
	return fStmt;
}

// Combine function <declaration_specifiers> and <declarator>.
decl *Function_Definition_Header(CG *cg, decl *fDecl) {
	Symbol *lSymb = fDecl->symb;
	if (IsFunction(lSymb)) {
		if (fDecl->type.type.properties & TYPE_MISC_ABSTRACT_PARAMS)
			SemanticError(cg, cg->tokenLoc, ERROR_S_ABSTRACT_NOT_ALLOWED, cg->GetAtomString(fDecl->name));

		if (lSymb->fun.statements)
			SemanticError(cg, cg->tokenLoc, ERROR_S_FUN_ALREADY_DEFINED, cg->GetAtomString(fDecl->name));

		cg->PushScope(lSymb->fun.locals);
	} else {
		SemanticError(cg, cg->tokenLoc, ERROR_S_NOT_A_FUN, cg->GetAtomString(fDecl->name));
		cg->PushScope();
	}
	cg->current_scope->funindex = ++cg->func_index;
	return fDecl;
}

// Check data in an init_declarator for compatibility with variable.

static int lCheckInitializationData(CG *cg, SourceLoc &loc, Type *vType, expr *dExpr, bool is_global) {
	if (!dExpr || !vType)
		return 0;

	int		base		= GetBase(vType);
	int		category	= GetCategory(vType);
	switch (category) {
		default:
		case TYPE_CATEGORY_NONE:
			return 0;
		case TYPE_CATEGORY_SCALAR:
			if (dExpr->kind == BINARY_N && dExpr->op == EXPR_LIST_OP) {
				expr *lExpr = FoldConstants(cg, dExpr->bin.left, FC_DEFAULT);
				if (lExpr->kind == CONST_N) {
					expr *tExpr;
					if (ConvertType(cg, lExpr, vType, lExpr->type, &tExpr, true, false)) {
						dExpr->bin.left = tExpr;
						return 1;
					} else {
						SemanticError(cg, loc, ERROR___INVALID_INITIALIZATION);
						return 0;
					}
				} else {
	#if 000 // RSG
					if (is_global) {
						SemanticError(cg, loc, ERROR___NON_CONST_INITIALIZATION);
						return 0;
					} else {
	#endif // RSG
						return 1;
	#if 000 // RSG
					}
	#endif // RSG
				}
			} else {
				SemanticError(cg, loc, ERROR___INVALID_INITIALIZATION);
				return 0;
			}
		case TYPE_CATEGORY_ARRAY: {
			int	vlen = vType->arr.numels;
			if (dExpr->kind == BINARY_N && dExpr->op == EXPR_LIST_OP) {
				expr *lExpr = dExpr->bin.left;
				if (!lExpr) {
					SemanticError(cg, loc, ERROR___TOO_LITTLE_DATA);
					return 0;
				}
				if (lExpr->kind == BINARY_N && lExpr->op == EXPR_LIST_OP) {
					Type *eType = vType->arr.eltype;
					if (vType->properties & TYPE_MISC_PACKED) {
						int	vlen2;
						if (IsVector(eType, &vlen2)) {
							vlen *= vlen2;
							eType = eType->arr.eltype;
						}
					}

					for (int i = 0, n; i < vlen; i += n) {
						if (lExpr) {
							if (lExpr->kind == BINARY_N && lExpr->op == EXPR_LIST_OP) {
								if (IsVector(lExpr->type, &n)) {
									if (i + n > vlen)
										break;
								} else {
									if (!lCheckInitializationData(cg, loc, eType, lExpr, is_global))
										return 0;
									n = 1;
								}
							} else {
								SemanticError(cg, loc, ERROR___INVALID_INITIALIZATION);
								return 0;
							}
						} else {
							SemanticError(cg, loc, ERROR___TOO_LITTLE_DATA);
							return 0;
						}
						lExpr = lExpr->bin.right;
					}
					
					if (lExpr) {
						SemanticError(cg, loc, ERROR___TOO_MUCH_DATA);
						return 0;
					}
					
					int	vlen2;
					if (IsMatrix(vType, &vlen, &vlen2)) {
						int	subop = SUBOP_VM(vlen2, vlen, GetBase(vType));
						dExpr->bin.left =  NewUnopSubNode(cg, MATRIX_M_OP, subop, dExpr->bin.left);
						dExpr->bin.left->type = GetStandardType(cg->hal, GetBase(vType), vlen2, vlen);
						return vlen * vlen2;
						
					} else if (IsVector(vType, &vlen)) {
						int	subop = SUBOP_V(vlen, GetBase(vType));
						dExpr->bin.left =  NewUnopSubNode(cg, VECTOR_V_OP, subop, dExpr->bin.left);
						dExpr->bin.left->type = GetStandardType(cg->hal, GetBase(vType), vlen, 0);
						return vlen;
					}
					return 1;

				} else {
					expr *tExpr;
					if (ConvertType(cg, lExpr, vType, lExpr->type, &tExpr, false, false)) {
						dExpr->bin.left = tExpr;
						return 1;
					} else {
						SemanticError(cg, loc, ERROR___INCOMPAT_TYPE_INIT);
						return 0;
					}
				}
			} else {
				SemanticError(cg, loc, ERROR___INVALID_INITIALIZATION);
			}
			return 0;
		}
		case TYPE_CATEGORY_STRUCT:
			return 1;
		case TYPE_CATEGORY_FUNCTION:
			SemanticError(cg, loc, ERROR___INVALID_INITIALIZATION);
			return 0;
		case TYPE_CATEGORY_TEXOBJ:
			return 1;
	}
}

decl *Param_Init_Declarator(CG *cg, decl *fDecl, expr *fExpr) {
	if (fDecl) {
		Type *lType = (Type*)&fDecl->type.type;
		if (IsVoid(lType))
			SemanticError(cg, cg->tokenLoc, ERROR_S_VOID_TYPE_INVALID, cg->GetAtomString(fDecl->name));

		if (GetCategory(lType) == TYPE_CATEGORY_FUNCTION)
			SemanticError(cg, cg->tokenLoc, ERROR_S_FUN_TYPE_INVALID, cg->GetAtomString(fDecl->name));

		if (fExpr) {
			if (GetDomain(lType) != TYPE_DOMAIN_UNIFORM)
				SemanticError(cg, cg->tokenLoc, ERROR_S_NON_UNIFORM_PARAM_INIT, cg->GetAtomString(fDecl->name));
			if (lCheckInitializationData(cg, cg->tokenLoc, lType, fExpr, false))
				fDecl->initexpr = fExpr;
		}
	}
	return fDecl;
}

// Set initial value and/or semantics for this declarator.
stmt *Init_Declarator(CG *cg, decl *fDecl, expr *fExpr) {
	stmt *lStmt = NULL;

	if (fDecl) {
		Symbol *lSymb = fDecl->symb;
		if (fExpr) {
			if (lSymb->kind != VARIABLE_S) {
				SemanticError(cg, cg->tokenLoc, ERROR_S_INIT_NON_VARIABLE, cg->GetAtomString(lSymb->name));
			} else if (cg->current_scope->flags & Scope::is_struct) {
				SemanticError(cg, cg->tokenLoc, ERROR_S_INIT_STRUCT_MEMBER, cg->GetAtomString(lSymb->name));
			} else if (lSymb->storage == SC_EXTERN) {
				SemanticError(cg, cg->tokenLoc, ERROR_S_INIT_EXTERN, cg->GetAtomString(lSymb->name));
			} else {
				Type	*lType		= lSymb->type;
				bool	is_global	= cg->current_scope->level <= 1;
				bool	is_static	= lSymb->storage == SC_STATIC;
				bool	is_const	= !!(GetQualifiers(lType) & TYPE_QUALIFIER_CONST);
				bool	is_uniform	= GetDomain(lType) == TYPE_DOMAIN_UNIFORM;
				bool	is_param	= lSymb->properties & SYMB_IS_PARAMETER;
				bool	dont_assign	= (is_global && !is_static) || is_param || is_const;
				if (lCheckInitializationData(cg, cg->tokenLoc, lType, fExpr, is_global)) {
					int	category	= GetCategory(lType);
					int	base		= GetBase(lType);
					switch (category) {
						default:
						case TYPE_CATEGORY_NONE:
							SemanticError(cg, cg->tokenLoc, ERROR___INVALID_INITIALIZATION);
							break;
						case TYPE_CATEGORY_SCALAR:
						case TYPE_CATEGORY_ARRAY:
						case TYPE_CATEGORY_STRUCT:
							assert(fExpr->kind == BINARY_N && fExpr->op == EXPR_LIST_OP);
							if (dont_assign) {
								lSymb->var.init = fExpr;
							} else {
								expr *lExpr =  NewSymbNode(cg, VARIABLE_OP, lSymb);
								lStmt = NewSimpleAssignmentStmt(cg, cg->tokenLoc, lExpr, fExpr->bin.left, 1);
							}
							break;
						case TYPE_CATEGORY_TEXOBJ:
							assert(fExpr->kind == BINARY_N && fExpr->op == EXPR_LIST_OP);
							lSymb->var.init = fExpr;
							break;
					}
				}
			}
		}
		if (IsFunction(lSymb)) {
			lSymb = lSymb->fun.params;
			while (lSymb) {
				if (lSymb->kind == VARIABLE_S) {
					if (lSymb->var.semantics)
						SemanticWarning(cg, cg->tokenLoc, WARNING_S_FORWARD_SEMANTICS_IGNORED, cg->GetAtomString(lSymb->name));
				}
				lSymb = lSymb->next;
			}
		}
	}
	return lStmt;
}

decl *Declarator(CG *cg, decl *fDecl, int semantics, int reg) {
	if (cg->current_scope->formal) {
		// Don't add formal parameters to the symbol table until we're sure that we're in a function declaration.
		if (fDecl->type.storage != SC_UNKNOWN)
			SemanticError(cg, fDecl->loc, ERROR_S_STORAGE_NOT_ALLOWED, cg->GetAtomString(fDecl->name));
		fDecl->semantics = semantics;
	} else {
		Symbol *lSymb = LookUpLocalSymbol(cg, cg->current_scope, fDecl->name);
		if (!lSymb) {
			if (cg->current_scope->flags & Scope::is_cbuffer) {//|| (cg->current_scope == cg->global_scope && fDecl->type.storage != SC_STATIC)) {
				SetDomain(&fDecl->type.type, TYPE_DOMAIN_UNIFORM);
				fDecl->type.is_derived = true;
			}
			Type *lType = GetTypePointer(cg, fDecl->loc, &fDecl->type);
			if (IsVoid(lType))
				SemanticError(cg, fDecl->loc, ERROR_S_VOID_TYPE_INVALID, cg->GetAtomString(fDecl->name));

			if (fDecl->type.type.properties & TYPE_MISC_TYPEDEF) {
				lSymb = DefineTypedef(cg, cg->tokenLoc, cg->current_scope, fDecl->name, lType);
				if (semantics)
					SemanticError(cg, cg->tokenLoc, ERROR_S_SEMANTICS_NON_VARIABLE, cg->GetAtomString(fDecl->name));
				if (fDecl->type.storage != SC_UNKNOWN)
					SemanticError(cg, fDecl->loc, ERROR_S_STORAGE_NOT_ALLOWED_TYPEDEF, cg->GetAtomString(fDecl->name));

			} else {
				if (GetQualifiers(&fDecl->type.type) & TYPE_QUALIFIER_INOUT)
					SemanticError(cg, fDecl->loc, ERROR_S_IN_OUT_PARAMS_ONLY, cg->GetAtomString(fDecl->name));

				if (GetCategory(&fDecl->type.type) == TYPE_CATEGORY_FUNCTION) {
					Scope *lScope = NewScopeInPool(cg->current_scope->pool);
					lScope->func_scope = lScope;

					// add 'this' to front of params
					if ((cg->current_scope->flags & Scope::is_struct) && fDecl->type.storage != SC_STATIC) {
						decl	*me = NewDeclNode(cg->tokenLoc, cg->AddAtom("this"), 0);
						me->next = fDecl->params;
						fDecl->params = me;
					}

					Symbol *params = AddFormalParamDecls(cg, lScope, fDecl->params);
					lSymb = DeclareFunc(cg, fDecl->loc, cg->current_scope, NULL, fDecl->name, lType, lScope, params);
					lSymb->fun.semantics = semantics;

				} else {
					if (fDecl->type.type.properties & TYPE_MISC_INTERNAL)
						SemanticError(cg, fDecl->loc, ERROR_S_INTERNAL_FOR_FUN, cg->GetAtomString(fDecl->name));
					if (fDecl->type.type.properties & TYPE_MISC_INLINE)
						SemanticError(cg, fDecl->loc, ERROR_S_INLINE_FOR_FUN, cg->GetAtomString(fDecl->name));
					//if (IsUnsizedArray(lType))
					//	SemanticError(cg, fDecl->loc, ERROR_S_UNSIZED_ARRAY, cg->GetAtomString(fDecl->name));
					if (IsCategory(lType, TYPE_CATEGORY_ARRAY) && !IsPacked(lType)) {
						if (!cg->hal->GetCapsBit(CAPS_INDEXED_ARRAYS)) {
							// XYZZY - This test needs to be moved to later to support multiple profiles
							SemanticError(cg, fDecl->loc, ERROR_S_UNPACKED_ARRAY, cg->GetAtomString(fDecl->name));
						}
					}
					lSymb = DefineVar(cg, cg->tokenLoc, cg->current_scope, fDecl->name, lType);
					lSymb->storage = fDecl->type.storage;

					if (semantics) {
						if (cg->current_scope->flags & Scope::is_struct) {
							cg->current_scope->flags |= Scope::has_semantics;
						} else {
							if (cg->current_scope->level > 1)
								SemanticError(cg, fDecl->loc, ERROR_S_NO_LOCAL_SEMANTICS, cg->GetAtomString(fDecl->name));
							else if (fDecl->type.storage == SC_STATIC)
								SemanticError(cg, fDecl->loc, ERROR_S_STATIC_CANT_HAVE_SEMANTICS, cg->GetAtomString(fDecl->name));
						}
						lSymb->var.semantics = semantics;
					}

					if (reg) {
						BindingTree	*node		= NewRegArrayBindingTree(cg->pool(), cg->tokenLoc, 0, fDecl->name, reg, 0, 0);
						cg->AddBinding(node);
					}

					if (cg->current_scope->level == 1) {
						if (fDecl->type.storage != SC_STATIC && GetDomain(&fDecl->type.type) != TYPE_DOMAIN_VARYING)
							lSymb->properties |= SYMB_NEEDS_BINDING;
					}
				}
			}

			if (cg->current_scope->flags & Scope::is_struct)
				AddParameter(cg->current_scope, lSymb);

		} else {
			if (GetCategory(&fDecl->type.type) == TYPE_CATEGORY_FUNCTION) {
				Type *lType = GetTypePointer(cg, fDecl->loc, &fDecl->type);
				Scope *lScope = NewScopeInPool(cg->current_scope->pool);
				lScope->func_scope = lScope;
				Symbol *params = AddFormalParamDecls(cg, lScope, fDecl->params);
				lSymb = DeclareFunc(cg, fDecl->loc, cg->current_scope, lSymb, fDecl->name, lType, lScope, params);
				lSymb->storage = fDecl->type.storage;
				if (semantics)
					SemanticError(cg, cg->tokenLoc, ERROR_S_SEMANTICS_NON_VARIABLE, cg->GetAtomString(fDecl->name));
			} else {
				if (!IsTypeBase(&fDecl->type.type, TYPE_BASE_UNDEFINED_TYPE))
					SemanticError(cg, fDecl->loc, ERROR_S_NAME_ALREADY_DEFINED, cg->GetAtomString(fDecl->name));
			}
		}
		fDecl->symb = lSymb;
	}
	return fDecl;
}

// Insert a dimension below dims levels.
static int lInsertDimension(CG *cg, dtype *fDtype, int dims, int fnumels, int props) {
	int lnumels, lproperties;
	Type *lType, *elType;

	if (dims == 0) {
//		fDtype->is_derived = false;
		lnumels = fnumels;
	} else {
		lType = &fDtype->type;
		if (IsArray(lType)) {
			lnumels = lType->arr.numels;
			lproperties = fDtype->type.properties & TYPE_MISC_MASK;
			//lsize = lType->arr.size;
			elType = lType->arr.eltype;
			fDtype->type = *elType;
			if (!lInsertDimension(cg, fDtype, dims - 1, fnumels, props))
				return 0;  // error encountered below
			fDtype->type.properties |= lproperties;
		} else {
			return 0;
		}
	}
	lType = GetTypePointer(cg, cg->tokenLoc, fDtype);
	SetCategory(&fDtype->type, TYPE_CATEGORY_ARRAY);
	fDtype->is_derived		= true;
	fDtype->type.arr.eltype = lType;
	fDtype->type.arr.numels = lnumels;
	fDtype->num_new_dims = dims + 1;
	if (props & TYPE_MISC_PACKED_KW) {
		fDtype->type.properties |= TYPE_MISC_PACKED;
	} else {
		fDtype->type.properties &= ~TYPE_MISC_PACKED;
	}
	fDtype->is_derived = true;
	return 1;
}

decl *Array_Declarator(CG *cg, decl *fDecl, int size, int Empty) {
	Type *lType;
	int dims;

	dtype *lDtype = &fDecl->type;
	if (size <= 0 && !Empty) {
		SemanticError(cg, cg->tokenLoc, ERROR___DIMENSION_LT_1);
		size = 1;
	}
	if (IsVoid(&lDtype->type))
		SemanticError(cg, cg->tokenLoc, ERROR___ARRAY_OF_VOID);
	switch (GetCategory(&lDtype->type)) {
		case TYPE_CATEGORY_SCALAR:
		case TYPE_CATEGORY_TEXOBJ:
			lType = lDtype->basetype;
			SetCategory(&lDtype->type, TYPE_CATEGORY_ARRAY);
			lDtype->is_derived		= true;
			lDtype->type.arr.eltype = lType;
			lDtype->type.arr.numels = size;
			lDtype->num_new_dims	= 1;
			break;
		case TYPE_CATEGORY_ARRAY:
			dims = lDtype->num_new_dims;
			lInsertDimension(cg, lDtype, dims, size, lDtype->type.properties);
			// if (TotalNumberDimensions > MAX_ARRAY_DIMENSIONS)
			//    SemanticError(cg, loc, ERROR_D_EXCEEDS_MAX_DIMS, MAX_ARRAY_DIMENSIONS);
			break;
		case TYPE_CATEGORY_FUNCTION:
			SemanticError(cg, cg->tokenLoc, ERROR___ARRAY_OF_FUNS);
			break;
		case TYPE_CATEGORY_STRUCT:
			lType = GetTypePointer(cg, cg->tokenLoc, lDtype);
			NewDType(lDtype, lType, TYPE_CATEGORY_ARRAY);
			lDtype->type.properties |= lType->properties & (TYPE_DOMAIN_MASK | TYPE_QUALIFIER_MASK);
			lDtype->type.arr.eltype = lType;
			lDtype->type.arr.numels = size;
			lDtype->num_new_dims = 1;
			break;
		default:
			InternalError(cg, cg->tokenLoc, 999, "ArrayDeclarator(): unknown category");
			break;
	}
	lDtype->is_derived = true;
	return fDecl;
}

attr *Attribute(int name, int num_params, int p0, int p1) {
	attr *a	= new attr;
	a->name			= name;
	a->num_params	= num_params;
	a->p0			= p0;
	a->p1			= p1;
	a->next			= NULL;
	return a;
}

stmt *AddStmtAttribute(stmt *fStmt, attr *fAttr) {
	fAttr->next = fStmt->attributes;
	fStmt->attributes = fAttr;
	return fStmt;
}

decl *AddDeclAttribute(decl *fDecl, attr *fAttr) {
	fAttr->next = fDecl->attributes;
	fDecl->attributes = fAttr;
	return fDecl;
}

// Add a list of formal parameter declarations to a function definition's scope.
Symbol *AddFormalParamDecls(CG *cg, Scope *fScope, decl *params) {
	Symbol *first = NULL, *last;

	while (params) {
		Symbol *lSymb = LookUpLocalSymbol(cg, fScope, params->name);
		if (lSymb) {
			SemanticError(cg, params->loc, ERROR_S_PARAM_NAME_TWICE, cg->GetAtomString(params->name));
		} else {
			lSymb = AddSymbol(cg, params->loc, fScope, params->name, GetTypePointer(cg, params->loc, &params->type), VARIABLE_S);
			lSymb->properties |= SYMB_IS_PARAMETER;
			lSymb->var.semantics = params->semantics;
			lSymb->var.init = params->initexpr;
			if (first)
				last->next = lSymb;
			else
				first = lSymb;
			last = lSymb;
			Type *lType = lSymb->type;
			if (IsCategory(lType, TYPE_CATEGORY_ARRAY) && !IsPacked(lType)) {
				if (!cg->hal->GetCapsBit(CAPS_INDEXED_ARRAYS))
					SemanticError(cg, params->loc, ERROR_S_UNPACKED_ARRAY, cg->GetAtomString(params->name));
			}
		}
		params = params->next;
	}
	return first;
}

// Build a list of types and set this function type's abstract parameter types.
decl *SetFunTypeParams(CG *cg, decl *func, decl *params, decl *actuals) {
	TypeList *formals = 0, *prev = 0;
	cg->current_scope->formal--;
	while (params) {
		TypeList *lType = new(cg->pool()) TypeList(GetTypePointer(cg, params->loc, &params->type));
		if (formals)
			prev->next = lType;
		else
			formals = lType;
		prev = lType;
		params = params->next;
	}
	if (func && IsCategory(&func->type.type, TYPE_CATEGORY_FUNCTION)) {
		func->type.type.fun.paramtypes = formals;
		func->type.is_derived = true;
	}
	if (actuals) {
		func->params = actuals;
	} else {
//		if (!(cg->current_scope->flags & Scope::has_void_param))
//			func->type.properties |= TYPE_MISC_ABSTRACT_PARAMS;
	}
	return func;
}

decl *FunctionDeclHeader(CG *cg, SourceLoc &loc, Scope *fScope, decl *func) {
	Type *rtnType = GetTypePointer(cg, cg->tokenLoc, &func->type);

	if (IsUnsizedArray(rtnType))
		SemanticError(cg, loc, ERROR_S_UNSIZED_ARRAY, cg->GetAtomString(func->name));

	NewDType(&func->type, NULL, TYPE_CATEGORY_FUNCTION);
	cg->current_scope->formal++;
	func->type.type.properties |= rtnType->properties & (TYPE_MISC_INLINE | TYPE_MISC_INTERNAL);
	rtnType->properties &= ~(TYPE_MISC_INLINE | TYPE_MISC_INTERNAL);
	func->type.type.fun.paramtypes = NULL;
	func->type.type.fun.rettype = rtnType;
	func->type.is_derived = true;
	return func;
}

Type *StructHeader(CG *cg, SourceLoc &loc, Scope *fScope, int semantics, int tag) {
	Type *lType;

	if (tag) {
		Symbol *lSymb = LookUpTag(cg, fScope, tag);
		if (!lSymb) {
			lSymb = AddTag(cg, loc, fScope, tag, TYPE_CATEGORY_STRUCT);
			lSymb->type->str.tag = tag;
			lSymb->type->str.semantics = semantics;
//			lSymb->type->str.variety = CID_NONE_ID;
		}
		lType = lSymb->type;
		if (!IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
			SemanticError(cg, loc, ERROR_S_TAG_IS_NOT_A_STRUCT, cg->GetAtomString(tag));
			lType = UndefinedType;
		}
	} else {
		lType = NewType(TYPE_CATEGORY_STRUCT, 0);
	}
	return lType;
}

Type *AddStructBase(Type *fType, Type *fBase) {
	if (fType)
		fType->str.base		= fBase;
	return fType;
}

Type *TemplateHeader(CG *cg, SourceLoc &loc, Scope *fScope, int tag, decl *params) {
	Symbol *symb = LookUpTag(cg, fScope, tag);
	if (!symb) {
		Type *type = NewType(TYPE_CATEGORY_STRUCT, 0);
		type->str.unqualifiedtype = type;
		type->str.tag			= tag;
		type->str.members		= NewScopeInPool(fScope->pool);
		symb = AddSymbol(cg, loc, fScope, tag, type, TEMPLATE_S);
	}
	Type *type = symb->type;
	if (symb->kind != TEMPLATE_S) {
		SemanticError(cg, loc, ERROR_S_TAG_IS_NOT_A_TEMPLATE, cg->GetAtomString(tag));
		type = UndefinedType;
	}

	Symbol	**list	= &type->str.members->params;
	int		index	= 0;
	for (decl *p = params; p; p = p->next) {
		Type	*t	= GetTypePointer(cg, p->loc, &p->type);
		if (!t) {
			t = NewType(TYPE_BASE_UNDEFINED_TYPE | TYPE_CATEGORY_NONE, 0);
			t->typname.index = index++;
		}
		Symbol	*s	= AddSymbol(cg, p->loc, type->str.members, p->name, t, TYPEDEF_S);
		*list	= s;
		list	= &s->next;
	}

	return type;
}


Symbol *ConstantBuffer(CG *cg, SourceLoc &loc, Scope *fScope, int tag, int reg) {
	Symbol *lSymb = LookUpTag(cg, fScope, tag);
	if (!lSymb) {
		lSymb = AddTag(cg, loc, fScope, tag, TYPE_CATEGORY_STRUCT);
		lSymb->type->str.tag	= tag;
		lSymb->properties		|= SYMB_NEEDS_BINDING;

		if (reg) {
			BindingTree	*node = NewRegArrayBindingTree(cg->pool(), loc, 0, tag, reg, 0, 0);
			cg->AddBinding(node);
		}
	}
	Type *lType = lSymb->type;
	if (!IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
		SemanticError(cg, loc, ERROR_S_TAG_IS_NOT_A_CBUFFER, cg->GetAtomString(tag));
		lType = UndefinedType;
	}
	return lSymb;
}

Type *EnumHeader(CG *cg, SourceLoc &loc, Scope *fScope, int tag) {
	Type *lType;

	if (tag) {
		Symbol *lSymb = LookUpTag(cg, fScope, tag);
		if (!lSymb) {
			lSymb = AddTag(cg, loc, fScope, tag, TYPE_CATEGORY_ENUM);
			lSymb->type->enm.tag = tag;
		}
		lType = lSymb->type;
		if (!IsCategory(lType, TYPE_CATEGORY_ENUM)) {
			SemanticError(cg, loc, ERROR_S_TAG_IS_NOT_A_STRUCT, cg->GetAtomString(tag));
			lType = UndefinedType;
		}
	} else {
		lType = NewType(TYPE_CATEGORY_ENUM, 0);
	}
	return lType;
}

void EnumAdd(CG *cg, SourceLoc &loc, Scope *fScope, Type *lType, int id, int val) {
	Symbol	*lSymb = AddSymbol(cg, loc, fScope, id, lType, CONSTANT_S);
	lSymb->con.value = val;
}

// Define a new variable in the current scope.
Symbol *DefineVar(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType) {
	return AddSymbol(cg, loc, fScope, atom, fType, VARIABLE_S);
}

// Define a new type name in the current scope.
Symbol *DefineTypedef(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType) {
	return AddSymbol(cg, loc, fScope, atom, fType, TYPEDEF_S);
}

// Declare an identifier as a function in the scope fScope
// If it's already in the symbol table check the overloading rules to make sure that it either
// A) matches a previous declaration exactly, or B) is unambiguously resolvable.
Symbol *DeclareFunc(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *fSymb, int atom, Type *fType, Scope *locals, Symbol *params) {
	int DiffParamTypes, DiffParamQualifiers, DiffParamCount, DiffReturnType;
	TypeList *oldArgType, *newArgType;
	Symbol *lSymb;
	int index, group, OK;

	if (fSymb) {
		if (GetCategory(fSymb->type) != TYPE_CATEGORY_FUNCTION) {
			SemanticError(cg, loc, ERROR_S_NAME_ALREADY_DEFINED, cg->GetAtomString(atom));
			lSymb = fSymb;
		} else {
			OK = 1;
			lSymb = fSymb;
			while (lSymb) {
				if (GetCategory(fSymb->type) != TYPE_CATEGORY_FUNCTION) {
					InternalError(cg, loc, ERROR_S_SYMBOL_TYPE_NOT_FUNCTION, cg->GetAtomString(lSymb->name));
					return fSymb;
				}
				DiffParamTypes = DiffParamQualifiers = DiffParamCount = DiffReturnType = 0;
				if (!IsSameUnqualifiedType(lSymb->type->fun.rettype, fType->fun.rettype))
					DiffReturnType = 1;
				oldArgType = lSymb->type->fun.paramtypes;
				newArgType = fType->fun.paramtypes;
				while (newArgType && oldArgType) {
					if (!IsSameUnqualifiedType(oldArgType->p, newArgType->p)) {
						DiffParamTypes = 1;
					} else if (GetQualifiers(oldArgType->p) != GetQualifiers(newArgType->p)) {
						DiffParamQualifiers = 1;
					}
					oldArgType = oldArgType->next;
					newArgType = newArgType->next;
				}
				if (newArgType || oldArgType)
					DiffParamCount = 1;
				if (!DiffParamCount && !DiffParamTypes) {
					if (DiffParamQualifiers) {
						SemanticError(cg, loc, ERROR_S_OVERLOAD_DIFF_ONLY_QUALS, cg->GetAtomString(atom));
						OK = 0;
						break;
					}
					if (DiffReturnType) {
						SemanticError(cg, loc, ERROR_S_OVERLOAD_DIFF_ONLY_RETURN, cg->GetAtomString(atom));
						OK = 0;
						break;
					}
					break; // Found the matching function
				}
				lSymb = lSymb->fun.overload;
			}
			if (OK) {
				if (DiffParamCount || DiffParamTypes) {
					lSymb = NewSymbol(loc, fScope, atom, fType, FUNCTION_S);
					lSymb->fun.params = params;
					lSymb->fun.locals = locals;
					lSymb->fun.overload = fSymb->fun.overload;
					fSymb->fun.overload = lSymb;
					if (GetCategory(fType) == TYPE_CATEGORY_FUNCTION) {
						locals->rettype = fType->fun.rettype;
					} else {
						locals->rettype = UndefinedType;
					}
				} else {
					if (!(lSymb->properties & SYMB_IS_DEFINED)) {
						// Overwrite previous definitions if this function is not yet defined.
						// Prototype parameter names are ignored.
						lSymb->fun.params = params;
						lSymb->fun.locals = locals;
					} else {
						// Declarator for a function that's already been defined.  Not an error.
					}
				}
			} else {
				// Found a function that differs only by qualifiers or return type.  Error arleady issued.
				// lSymb = fSymb;
			}
		}
	} else {
		lSymb = AddSymbol(cg, loc, fScope, atom, fType, FUNCTION_S);
		lSymb->fun.params = params;
		lSymb->fun.locals = locals;
		if (GetCategory(fType) == TYPE_CATEGORY_FUNCTION) {
			locals->rettype = fType->fun.rettype;
		} else {
			locals->rettype = UndefinedType;
		}
	}
	if (lSymb->type->properties & TYPE_MISC_INTERNAL) {
		index = cg->hal->GetInternalFunction(lSymb, &group);
		if (index) {
			//
			// lSymb->InternalIndex = index; etc.
			//
			lSymb->properties |= SYMB_IS_DEFINED | SYMB_IS_BUILTIN;
			lSymb->fun.group = group;
			lSymb->fun.index = index;
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_INTERNAL_FUNCTION, cg->GetAtomString(atom));
		}
	}

	return lSymb;
}

// Set the body of the function "func" to the statements in "body".
void DefineFunction(CG *cg, decl *func, stmt *body) {
	Symbol	*lSymb = func->symb;

	if (IsFunction(lSymb)) {
		if (lSymb->properties & SYMB_IS_DEFINED) {
			SemanticError(cg, cg->tokenLoc, ERROR_S_FUN_ALREADY_DEFINED, cg->GetAtomString(lSymb->name));
		} else {
			lSymb->properties |= SYMB_IS_DEFINED;
			lSymb->fun.statements = body;
			Type *lType = lSymb->type;
			if (lType->properties & TYPE_MISC_INLINE)
				lSymb->properties |= SYMB_IS_INLINE_FUNCTION;
		}
		if (!(cg->current_scope->flags & Scope::has_return) && !IsVoid(cg->current_scope->rettype))
			SemanticError(cg, cg->tokenLoc, ERROR_S_FUNCTION_HAS_NO_RETURN, cg->GetAtomString(lSymb->name));
		if (cg->opts & cgclib::DUMP_PARSETREE) {
			PrintScopeDeclarations(cg);
			PrintFunction(cg, lSymb);
		}
	}
}

void DefineTechnique(CG *cg, int atom, expr *passes, stmt *anno) {
//	Symbol	*s = AddSymbol(loc, fScope, atom, NULL, TECHNIQUE_S);
	Symbol	*s = NewSymbol(cg->tokenLoc, cg->current_scope, atom, NULL, TECHNIQUE_S);
	s->var.init	= passes;
	AddToSymbolList(&cg->techniques, s);
}

int GlobalInitStatements(Scope *fScope, stmt *fStmt) {
	if (fStmt) {
		if (fScope->init_stmts) {
			stmt *lStmt = fScope->init_stmts;
			while (lStmt->next)
				lStmt = lStmt->next;
			lStmt->next = fStmt;
		} else {
			fScope->init_stmts = fStmt;
		}
	}
	return 1;
}

bool is_lvalue(const expr *fExpr) {
	return fExpr && fExpr->is_lvalue;
}

bool is_const(const expr *fExpr) {
	return fExpr && fExpr->is_const;
}

bool IsArrayIndex(const expr *fExpr) {
	return fExpr && fExpr->kind == BINARY_N && fExpr->op == ARRAY_INDEX_OP;
}

static bool lIsBaseCastValid(CG *cg, int toBase, int fromBase, bool Explicit) {
	return !(toBase == TYPE_BASE_NO_TYPE || fromBase == TYPE_BASE_NO_TYPE || toBase == TYPE_BASE_VOID || fromBase == TYPE_BASE_VOID)
		&& (toBase == fromBase || cg->hal->IsValidScalarCast(toBase, fromBase, Explicit));
}

// Type cast fExpr from fromType to toType if needed.  Ignore qualifiers.
// If "result" is NULL just check validity of cast; don't allocate cast operator node.
bool ConvertType(CG *cg, expr *fExpr, Type *toType, Type *fromType, expr **result, bool IgnorePacked, bool Explicit) {
	int fcategory, tcategory;
	int fbase, tbase;
	Type *feltype, *teltype;
	expr *unnode;
	int ToPacked, FromPacked;

	if (Explicit && fExpr->kind == CONST_N && fExpr->co.val[0].i == 0) {
		if (result)		// expand to lots of 0 stores?
			*result = fExpr;
		return true;
	}

	ToPacked = toType && (toType->properties & TYPE_MISC_PACKED);
	FromPacked = fromType && (fromType->properties & TYPE_MISC_PACKED);
	if (IsSameUnqualifiedType(toType, fromType) && ((ToPacked == FromPacked) || IgnorePacked)) {
		if (result)
			*result = fExpr;
		return true;
	} else {
		fcategory = GetCategory(fromType);
		tcategory = GetCategory(toType);
		if (fcategory == tcategory) {
			switch (fcategory) {
				case TYPE_CATEGORY_SCALAR:
					fbase = GetBase(fromType);
					tbase = GetBase(toType);
					if (lIsBaseCastValid(cg, tbase, fbase, Explicit)) {
						if (result) {
							unnode = NewUnopSubNode(cg, CAST_CS_OP, SUBOP_CS(tbase, fbase), fExpr);
							unnode->type = GetStandardType(cg->hal, tbase, 0, 0);
							unnode->has_sideeffects = fExpr->has_sideeffects;
							*result =  unnode;
						}
						return true;
					}
					break;
				case TYPE_CATEGORY_ARRAY: {
					feltype = fromType->arr.eltype;
					teltype = toType->arr.eltype;
					fcategory = GetCategory(feltype);
					tcategory = GetCategory(teltype);
					fbase = GetBase(feltype);
					tbase = GetBase(teltype);
#if 0
					if ((toType->properties ^ fromType->properties) & TYPE_MISC_ROWMAJOR) {
						if (tcategory != TYPE_CATEGORY_ARRAY || fcategory != TYPE_CATEGORY_ARRAY)
							return 0;
						if (!Explicit && (toType->arr.numels != feltype->arr.numels || teltype->arr.numels != fromType->arr.numels))
							return 0;
						if (result) {
							unnode = NewUnopSubNode(cg, CAST_CM_OP, SUBOP_CM(teltype->arr.numels, tbase, toType->arr.numels, fbase), fExpr);
							unnode->type = GetStandardType(cg->hal, tbase, toType->arr.numels, teltype->arr.numels);
							unnode->has_sideeffects = fExpr->has_sideeffects;
							*result =  unnode;
						}
						return 1;
					}
#endif
					if (!(cg->opts & cgclib::IMPLICIT_DOWNCASTS) && !Explicit && toType->arr.numels != fromType->arr.numels)
						return false;
					if (toType->arr.numels > 4)
						return false;
					if (!IgnorePacked && (ToPacked != FromPacked))
						return false;
					if (tcategory != fcategory)
						return false;
					if (lIsBaseCastValid(cg, tbase, fbase, Explicit)) {
						switch (tcategory) {
							case TYPE_CATEGORY_SCALAR:
								if (result) {
									unnode = NewUnopSubNode(cg, CAST_CV_OP, SUBOP_CV(tbase, toType->arr.numels, fbase), fExpr);
									unnode->type = GetStandardType(cg->hal, tbase, toType->arr.numels, 0);
									unnode->has_sideeffects = fExpr->has_sideeffects;
									*result =  unnode;
								}
								return true;
							case TYPE_CATEGORY_ARRAY:
								if (!Explicit && teltype->arr.numels != feltype->arr.numels)
									return 0;
								if (result) {
									unnode = NewUnopSubNode(cg, CAST_CM_OP, SUBOP_CM(toType->arr.numels, tbase, teltype->arr.numels, fbase), fExpr);
									unnode->type = GetStandardType(cg->hal, tbase, toType->arr.numels, 0);
									unnode->has_sideeffects = fExpr->has_sideeffects;
									*result =  unnode;
								}
								return true;
						}
					}
					break;
				}
			}
		} else if (fcategory == TYPE_CATEGORY_SCALAR && tcategory == TYPE_CATEGORY_ARRAY) {
			fbase = GetBase(fromType);
			tbase = GetBase(toType->arr.eltype);
			if (lIsBaseCastValid(cg, tbase, fbase, Explicit)) {
				if (result) {
					unnode = NewUnopSubNode(cg, CAST_CV_OP, SUBOP_CV(tbase, toType->arr.numels, fbase), fExpr);
					unnode->type = GetStandardType(cg->hal, tbase, toType->arr.numels, 0);
					unnode->has_sideeffects = fExpr->has_sideeffects;
					*result =  unnode;
				}
				return true;
			}
		}
	}
	return false;
}

// Cast a scalar, vector, or matrix expression.
// Scalar: len = 0.
// Vector: len >= 1 and len2 = 0
// Matrix: len >= 1 and len2 >= 1
expr *CastScalarVectorMatrix(CG *cg, expr *fExpr, int fbase, int tbase, int len, int len2) {
	opcode	op;
	int		subop;

	if (len == 0) {
		op		= CAST_CS_OP;
		subop	= SUBOP_CS(tbase, fbase);
	} else if (len2 == 0) {
		op		= CAST_CV_OP;
		subop	= SUBOP_CV(tbase, len, fbase);
	} else {
		op		= CAST_CM_OP;
		subop	= SUBOP_CM(len2, tbase, len, fbase);
	}
	expr	*lExpr =  NewUnopSubNode(cg, op, subop, fExpr);
	lExpr->type = GetStandardType(cg->hal, tbase, len, len2);
	return lExpr;
}

// Convert two scalar, vector, or matrix expressions to the same type for use in an expression
// Number of dimensions and lengths may differ.
// Returns: base type of resulting values.
int ConvertNumericOperands(CG *cg, int baseop, expr **lExpr, expr **rexpr, int lbase, int rbase, int llen, int rlen, int llen2, int rlen2) {
	int nbase = cg->hal->GetBinOpBase(baseop, lbase, rbase, llen, rlen);
	if (nbase != lbase)
		*lExpr = CastScalarVectorMatrix(cg, *lExpr, lbase, nbase, llen, llen2);
	if (nbase != rbase)
		*rexpr = CastScalarVectorMatrix(cg, *rexpr, rbase, nbase, rlen, rlen2);
	return nbase;
}

expr *CheckBooleanExpr(CG *cg, SourceLoc &loc, expr *fExpr, int *AllowVector) {
	int		len = 0;
	Type	*leltype;
	bool	HasError = false;

	Type	*lType = leltype = fExpr->type;
	if (IsScalar(lType)) {
		if (!IsBoolean(lType)) {
			fExpr = CastScalarVectorMatrix(cg, fExpr, GetBase(lType), TYPE_BASE_BOOLEAN, 0, 0);
//			SemanticError(cg, loc, ERROR___BOOL_EXPR_EXPECTED);
//			HasError = true;
		}
	} else if (IsVector(lType, &len)) {
		leltype = lType->arr.eltype;
		if (AllowVector) {
			if (len > 4) {
				SemanticError(cg, loc, ERROR___VECTOR_EXPR_LEN_GR_4);
				HasError = true;
				len = 4;
			}
			if (!IsBoolean(lType)) {
				SemanticError(cg, loc, ERROR___BOOL_EXPR_EXPECTED);
				HasError = true;
			}
			*AllowVector = len;
		} else {
			SemanticError(cg, loc, ERROR___SCALAR_BOOL_EXPR_EXPECTED);
			HasError = true;
		}
	} else {
		SemanticError(cg, loc, ERROR___BOOL_EXPR_EXPECTED);
		HasError = true;
	}
	if (HasError)
		fExpr->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, len, 0);
	return fExpr;
}

// NewUnaryOperator() - See if this is a valid unary operation.  Return a new node with the
//         proper operator description.
//
// Valid operators are:
//
//     op      arg1    arg2   result
//   ------   ------  ------  ------
//   NEG      scalar  scalar  scalar
//   NEG_V    vector  vector  vector

expr *NewUnaryOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *fExpr, int IntegralOnly) {
	opcode	lop		= fop;
	int		subop	= 0, len = 0;
	expr	*result	= NULL;
	bool	HasError = false;
	bool	MustBeBoolean = fop == BNOT_OP;
	Type	*lType	= fExpr->type, *eltype = lType;

	if (IsScalar(lType)) {
		subop	= 0;
	} else if (IsVector(lType, &len)) {
		eltype	= lType->arr.eltype;
		lop		= opcode(fop + OFFSET_V_OP);
		subop	= SUBOP_V(len, 0);
	} else {
		if (IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
			if (Symbol *symb = LookUpLocalSymbol(cg, lType->str.members, GetOperatorName(cg, fop)))
				return NewFunctionCallOperator(cg, loc, NewSymbNode(cg, VARIABLE_OP, symb), ArgumentList(cg, 0, fExpr));
		}
		SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
		HasError = true;
	}
	if (!HasError) {
		if (len > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, cg->GetAtomString(name));
		} else {
			int	lbase = GetBase(lType);
			SUBOP_SET_T(subop, lbase);
			if (MustBeBoolean) {
				if (lbase != TYPE_BASE_BOOLEAN) {
					SemanticError(cg, loc, ERROR___BOOL_EXPR_EXPECTED);
					HasError = true;
				}
			} else {
				if (cg->hal->IsNumericBase(lbase)) {
					if (IntegralOnly && !cg->hal->IsIntegralBase(lbase)) {
						SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_INTEGRAL, cg->GetAtomString(name));
						HasError = true;
					}
				} else {
					SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_NUMERIC, cg->GetAtomString(name));
				}
			}
			if (!HasError) {
				result = NewUnopSubNode(cg, lop, subop, fExpr);
				result->type = GetStandardType(cg->hal, lbase, len, 0);
			}
		}
	}
	if (!result) {
		result = NewUnopSubNode(cg, lop, 0, fExpr);
		result->type = UndefinedType;
	}
	return  result;
}

// NewBinaryOperator() - See if this is a valid binary operation.  Return a new node with the
//         proper operator description.
//
// Valid operators are:
//
//     op      arg1    arg2   result
//   ------   ------  ------  ------
//   MUL      scalar  scalar  scalar
//   MUL_V    vector  vector  vector
//   MUL_SV*  scalar  vector  vector
//   MUL_VS*  vector  scalar  vector
//
//    *only allowed for smearing operators MUL, DIV, ADD, SUB.
expr *NewBinaryOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *lExpr, expr *rexpr, int IntegralOnly) {
	opcode	lop	= fop;
	int		subop = 0, llen = 0, llen2 = 0, rlen = 0, rlen2 = 0, nlen, nlen2;
	int		lbase, rbase, nbase;
	expr	*result = NULL;
	bool	HasError = false;

	bool	CanSmear = fop == MUL_OP || fop == DIV_OP || fop == MOD_OP || fop == ADD_OP || fop == SUB_OP || fop == AND_OP || fop == OR_OP || fop == XOR_OP;
	Type	*lType = lExpr->type, *leltype = lType;
	Type	*rtype = rexpr->type, *reltype = rtype;

	if (IsScalar(lType)) {
		if (IsScalar(rtype)) {
			subop = 0;
		} else if (IsVector(rtype, &rlen)) {
			if (CanSmear) {
				reltype = rtype->arr.eltype;
				lop = opcode(fop + OFFSET_SV_OP);
				subop = SUBOP_SV(rlen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_SCALAR_OP_VECTOR_INVALID, cg->GetAtomString(name));
				HasError = true;
			}
		} else if (IsMatrix(rtype, &rlen, &rlen2)) {
			if (CanSmear) {
				reltype = rtype->arr.eltype;
				lop = opcode(fop + OFFSET_SV_OP);
				subop = SUBOP_SV(rlen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_SCALAR_OP_VECTOR_INVALID, cg->GetAtomString(name));
				HasError = true;
			}
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else if (IsVector(lType, &llen)) {
		leltype = lType->arr.eltype;
		if (IsScalar(rtype)) {
			if (CanSmear) {
				lop = opcode(fop + OFFSET_VS_OP);
				subop = SUBOP_VS(llen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_VECTOR_OP_SCALAR_INVALID, cg->GetAtomString(name));
				HasError = true;
			}
		} else if (IsVector(rtype, &rlen)) {
			reltype = rtype->arr.eltype;
			lop = opcode(fop + OFFSET_V_OP);
			subop = SUBOP_VS(llen, 0);
			if (llen != rlen) {
				SemanticError(cg, loc, ERROR_S_VECTOR_OPERANDS_DIFF_LEN, cg->GetAtomString(name));
				HasError = true;
			}
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else if (IsMatrix(lType, &llen, &llen2)) {
		if (IsScalar(rtype)) {
			if (CanSmear) {
				lop = opcode(fop + OFFSET_VS_OP);
				subop = SUBOP_VS(llen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_VECTOR_OP_SCALAR_INVALID, cg->GetAtomString(name));
				HasError = true;
			}
		} else if (IsMatrix(rtype, &rlen, &rlen2)) {
			reltype = rtype->arr.eltype;
			lop = opcode(fop + OFFSET_V_OP);
			subop = SUBOP_VS(llen, 0);
			if (llen != rlen || llen2 != rlen2) {
				SemanticError(cg, loc, ERROR_S_VECTOR_OPERANDS_DIFF_LEN, cg->GetAtomString(name));
				HasError = true;
			}
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else {
		if (IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
			if (Symbol *symb = LookUpLocalSymbol(cg, lType->str.members, GetOperatorName(cg, fop)))
				return NewFunctionCallOperator(cg, loc, NewSymbNode(cg, VARIABLE_OP, symb), ArgumentList(cg, ArgumentList(cg, 0, lExpr), rexpr));
		}
		SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
		HasError = true;
	}
	if (!HasError) {
		if (llen > 4 || rlen > 4 || llen2 > 4 || rlen2 > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, cg->GetAtomString(name));
		} else {
			lbase = GetBase(lType);
			rbase = GetBase(rtype);
			if (cg->hal->IsNumericBase(lbase) && cg->hal->IsNumericBase(rbase)) {
				nbase = ConvertNumericOperands(cg, fop, &lExpr, &rexpr, lbase, rbase, llen, rlen, 0, 0);
				SUBOP_SET_T(subop, nbase);
				nlen = llen > rlen ? llen : rlen;
				nlen2 = llen2 > rlen2 ? llen2 : rlen2;
#if 1
				result = NewBinopSubNode(cg, lop, subop, lExpr, rexpr);
				result->type = GetStandardType(cg->hal, nbase, nlen, nlen2);
#else
				if (llen2 == 0) {
					result = NewBinopSubNode(cg, lop, subop, lExpr, rexpr);
					result->type = GetStandardType(cg->hal, nbase, nlen, nlen2);
				} else {
					int	i;
					result = NULL;
					for (i = 0; i < llen2; i++) {
						binary	*t = NewBinopSubNode(cg, lop, subop, lExpr, rexpr);
						t->type = GetStandardType(cg->hal, nbase, nlen, 0);
						result	= (binary*)GenExprList(cg, result, t, t->type);
					}
					result = (binary*)NewUnopSubNode(cg, MATRIX_M_OP, SUBOP_VM(nlen2, nlen, nbase), result);
					result->type = GetStandardType(cg->hal, nbase, nlen, nlen2);
				}
#endif
				if (IntegralOnly && !cg->hal->IsIntegralBase(nbase))
					SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_INTEGRAL, cg->GetAtomString(name));
			} else {
				SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_NUMERIC, cg->GetAtomString(name));
			}
		}
	}
	if (!result) {
		result = NewBinopSubNode(cg, lop, 0, lExpr, rexpr);
		result->type = UndefinedType;
	}
	return  result;
}

// NewBinaryBooleanOperator() - See if this is a valid binary Boolean operator.  Return a new
//         node with the proper operator description.
//
// Valid operators are:
//
//     op      arg1    arg2   result
//   ------   ------  ------  ------
//   BAND     scalar  scalar  scalar
//   BAND_V   vector  vector  vector

expr *NewBinaryBooleanOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *lExpr, expr *rexpr) {
	opcode	lop;
	int		subop = 0, llen = 0, rlen = 0;
	int		lbase, rbase;
	Type	*lType, *rtype, *leltype, *reltype;
	expr	*result = NULL;
	bool	HasError = false;

	lop = fop;
	lType = leltype = lExpr->type;
	rtype = reltype = rexpr->type;
	if (IsScalar(lType)) {
		if (IsScalar(rtype)) {
			subop = SUBOP__(TYPE_BASE_BOOLEAN);
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else if (IsVector(lType, &llen)) {
		leltype = lType->arr.eltype;
		if (IsVector(rtype, &rlen)) {
			reltype = rtype->arr.eltype;
			lop = opcode(fop + OFFSET_V_OP);
			subop = SUBOP_V(llen, TYPE_BASE_BOOLEAN);
			if (llen != rlen) {
				SemanticError(cg, loc, ERROR_S_VECTOR_OPERANDS_DIFF_LEN, cg->GetAtomString(name));
				HasError = true;
			}
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else {
		if (IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
			if (Symbol *symb = LookUpLocalSymbol(cg, lType->str.members, GetOperatorName(cg, fop)))
				return NewFunctionCallOperator(cg, loc, NewSymbNode(cg, VARIABLE_OP, symb), ArgumentList(cg, ArgumentList(cg, 0, lExpr), rexpr));
		}
		SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
		HasError = true;
	}
	if (!HasError) {
		if (llen > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, cg->GetAtomString(name));
		} else {
			lbase = GetBase(lType);
			rbase = GetBase(rtype);
			if (lbase == TYPE_BASE_BOOLEAN && rbase == TYPE_BASE_BOOLEAN) {
				result = NewBinopSubNode(cg, lop, subop, lExpr, rexpr);
				result->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, llen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_BOOLEAN, cg->GetAtomString(name));
			}
			if (lExpr->has_sideeffects || rexpr->has_sideeffects) {
				SemanticError(cg, loc, ERROR_S_OPERANDS_HAVE_SIDE_EFFECTS, cg->GetAtomString(name));
			}
		}
	}
	if (!result) {
		result = NewBinopSubNode(cg, lop, 0, lExpr, rexpr);
		result->type = UndefinedType;
	}
	return result;
}

// NewBinaryComparisonOperator() - See if this is a valid binary comparison.  Return a new node
//         with the proper operator description.
//
// Valid operators are:
//
//    op     arg1    arg2   result
//   ----   ------  ------  ------
//   LT     scalar  scalar  scalar
//   LT_V   vector  vector  vector
expr *NewBinaryComparisonOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *lExpr, expr *rexpr) {
	opcode lop;
	int subop = 0, llen = 0, rlen = 0, nlen = 0;
	int lbase, rbase, nbase;
	Type *lType, *rtype, *leltype, *reltype;
	expr *result = NULL;
	bool	HasError = false;

	lop = fop;
	lType = leltype = lExpr->type;
	rtype = reltype = rexpr->type;
	if (IsScalar(lType)) {
		if (IsScalar(rtype)) {
			subop = 0;
		} else if (IsVector(rtype, &rlen)) {
			reltype = rtype->arr.eltype;
			lop = opcode(fop + OFFSET_SV_OP);
			subop = SUBOP_SV(rlen, 0);
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else if (IsVector(lType, &llen)) {
		leltype = lType->arr.eltype;
		if (IsScalar(rtype)) {
			lop = opcode(fop + OFFSET_VS_OP);
			subop = SUBOP_VS(llen, 0);
		} else if (IsVector(rtype, &rlen)) {
			reltype = rtype->arr.eltype;
			lop = opcode(fop + OFFSET_V_OP);
			subop = SUBOP_V(llen, 0);
			if (llen != rlen) {
				SemanticError(cg, loc, ERROR_S_VECTOR_OPERANDS_DIFF_LEN, cg->GetAtomString(name));
				HasError = true;
			}
			nlen = llen;
		} else {
			SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
			HasError = true;
		}
	} else {
		if (IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
			if (Symbol *symb = LookUpLocalSymbol(cg, lType->str.members, GetOperatorName(cg, fop)))
				return NewFunctionCallOperator(cg, loc, NewSymbNode(cg, VARIABLE_OP, symb), ArgumentList(cg, ArgumentList(cg, 0, lExpr), rexpr));
		}
		SemanticError(cg, loc, ERROR_S_INVALID_OPERANDS, cg->GetAtomString(name));
		HasError = true;
	}
	if (!HasError) {
		if (nlen > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, cg->GetAtomString(name));
		} else {
			lbase = GetBase(lType);
			rbase = GetBase(rtype);
			if (cg->hal->IsNumericBase(lbase) && cg->hal->IsNumericBase(rbase)) {
				nbase = ConvertNumericOperands(cg, fop, &lExpr, &rexpr, lbase, rbase, llen, rlen, 0, 0);
				SUBOP_SET_T(subop, nbase);
				nlen = llen > rlen ? llen : rlen;
				result = NewBinopSubNode(cg, lop, subop, lExpr, rexpr);
				result->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, nlen, 0);
			} else if (lbase == TYPE_BASE_BOOLEAN && rbase == TYPE_BASE_BOOLEAN) {
				subop = SUBOP_V(nlen, TYPE_BASE_BOOLEAN);
				result = NewBinopSubNode(cg, lop, subop, lExpr, rexpr);
				result->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, nlen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_NUMERIC, cg->GetAtomString(name));
			}
		}
	}
	if (!result) {
		result = NewBinopSubNode(cg, lop, 0, lExpr, rexpr);
		result->type = UndefinedType;
	}
	return result;
}

// NewConditionalOperator() - Check the types of the components of a conditional expression.
//         Return a new node with the proper operator description.
//
// Valid forma are:
//
//     op       cond    exp1    exp2   result
//   -------   ------  ------  ------  ------
//   COND      scalar  scalar  scalar  scalar
//   COND_SV   scalar  vector  vector  vector
//   COND_V    vector  vector  vector  vector
//   COND_GEN  scalar   type    type    type
expr *NewConditionalOperator(CG *cg, SourceLoc &loc, expr *bexpr, expr *lExpr, expr *rexpr) {
	int subop, blen = 0, llen = 0, rlen = 0, nlen = 0;
	int LIsNumeric, LIsBoolean, LIsSimple;
	int lbase, rbase, nbase, category;
	Type *btype, *lType, *rtype, *beltype, *leltype, *reltype;
	Type *resulttype = UndefinedType;
	expr *result = NULL;
	bool	HasError = false;

	// Type of conditional expression is checked elsewhere.

	opcode lop = COND_OP;
	subop = 0;
	btype = beltype = bexpr->type;
	lType = leltype = lExpr->type;
	rtype = reltype = rexpr->type;
	lbase = GetBase(leltype);
	rbase = GetBase(reltype);
	LIsNumeric = cg->hal->IsNumericBase(lbase) & cg->hal->IsNumericBase(rbase);
	LIsBoolean = (lbase == TYPE_BASE_BOOLEAN) & (rbase == TYPE_BASE_BOOLEAN);
	LIsSimple = LIsNumeric | LIsBoolean;
	if (LIsSimple) {

		// 1) Numeric

		if (IsScalar(btype)) {

			// 1A) Scalar ? Scalar : Scalar

			if (IsScalar(lType)) {
				if (IsScalar(rtype)) {
					// O.K.
				} else {
					SemanticError(cg, loc, ERROR___QSTN_SCALAR_3RD_OPND_EXPECTED);
					HasError = true;
				}

				// 1B) Scalar ? Vector : Vector

			} else if (IsVector(lType, &llen)) {
				leltype = lType->arr.eltype;
				if (IsVector(rtype, &rlen)) {
					reltype = rtype->arr.eltype;
					lbase = GetBase(leltype);
					rbase = GetBase(reltype);
					lop = COND_SV_OP;
					subop = SUBOP_SV(llen, 0);
				} else {
					SemanticError(cg, loc, ERROR___QSTN_VECTOR_3RD_OPND_EXPECTED);
					HasError = true;
				}

				// 1C) Scalar ? Array : Array >>--->> Treat as non-numeric case

			} else {
				LIsSimple = 0; // Check type compatibility later
			}
		} else if (IsVector(btype, &blen)) {

			// 1D) Vector ? Vector : Vector

			if (!IsVector(lType, &llen)) {
				lExpr = CastScalarVectorMatrix(cg, lExpr, lbase, lbase, blen, 0);
				lType = lExpr->type;
			}
			if (!IsVector(rtype, &rlen)) {
				rexpr = CastScalarVectorMatrix(cg, rexpr, rbase, rbase, blen, 0);
				rtype = lExpr->type;
			}
			if (IsVector(lType, &llen) && IsVector(rtype, &rlen)) {
				lop = COND_V_OP;
				subop = SUBOP_SV(llen, 0);
				leltype = lType->arr.eltype;
				reltype = rtype->arr.eltype;
				lbase = GetBase(leltype);
				rbase = GetBase(reltype);
			} else {
				SemanticError(cg, loc, ERROR___QSTN_VECTOR_23_OPNDS_EXPECTED);
				HasError = true;
			}
		} else {
			SemanticError(cg, loc, ERROR___QSTN_INVALID_1ST_OPERAND);
			HasError = true;
		}
	}
	if (!LIsSimple) {

		// 2) Not numeric - must be same type.  Requires scalar condition.

		if (IsScalar(btype)) {
			if (IsSameUnqualifiedType(lType, rtype)) {
				lop = COND_GEN_OP;
				resulttype = lType;
			} else {
				SemanticError(cg, loc, ERROR___QSTN_23_OPNDS_INCOMPAT);
				HasError = true;
			}
		} else {
			SemanticError(cg, loc, ERROR___QSTN_1ST_OPERAND_NOT_SCALAR);
			HasError = true;
		}
	}
	if (!HasError) {
		if (lExpr->has_sideeffects || rexpr->has_sideeffects) {
			SemanticError(cg, loc, ERROR_S_OPERANDS_HAVE_SIDE_EFFECTS, "?:");
		}
		if (LIsSimple) {
			nbase = ConvertNumericOperands(cg, COND_OP, &lExpr, &rexpr, lbase, rbase, llen, rlen, 0, 0);
			if (llen == rlen && (blen == 0 || blen == llen)) {
				SUBOP_SET_T(subop, nbase);
				result = NewTriopSubNode(cg, lop, subop, bexpr, lExpr, rexpr);
				result->type = GetStandardType(cg->hal, nbase, llen, 0);
			} else {
				SemanticError(cg, loc, ERROR_S_VECTOR_OPERANDS_DIFF_LEN, "\"? :\"");
				HasError = true;
			}
		} else {
			category = GetCategory(lType);
			if ((category == TYPE_CATEGORY_SCALAR ||
				category == TYPE_CATEGORY_ARRAY ||
				category == TYPE_CATEGORY_STRUCT) &&
				!IsVoid(lType))
			{
				result = NewTriopSubNode(cg, lop, 0, bexpr, lExpr, rexpr);
				result->type = lType;
			} else {
				SemanticError(cg, loc, ERROR___QSTN_23_OPNDS_INVALID);
				HasError = true;
			}
		}
	}
	if (!result) {
		result = NewTriopSubNode(cg, lop, 0, bexpr, lExpr, rexpr);
		result->type = UndefinedType;
	}
	return result;
}

// See if this is a valid swizzle operation.  Return a new node with the proper operator description.
expr *NewSwizzleOperator(CG *cg, SourceLoc &loc, expr *fExpr, int ident) {
	int		len = 0, ii, maxi, base, tmask, mask = 0, mlen = 0;
	bool	LIsLValue;
	Type *ftype, *feltype;
	expr *result = NULL;
	bool	HasError = false;

	mask = GetSwizzleOrWriteMask(cg, loc, ident, &LIsLValue, &mlen);
	ftype = fExpr->type;
	if (IsScalar(ftype)) {
		feltype = ftype;
		maxi = 0;
	} else if (IsVector(ftype, &len)) {
		feltype = ftype->arr.eltype;
		maxi = len - 1;
		if (len > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, ".");
			HasError = true;
		}
	} else {
		SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_SCALAR_VECTOR, ".");
		HasError = true;
	}
	if (!HasError) {
		base = GetBase(feltype);
		tmask = mask;
		for (ii = 0; ii < mlen; ii++) {
			if ((tmask & 0x3) > maxi) {
				SemanticError(cg, loc, ERROR_S_SWIZZLE_MASK_EL_MISSING,
					cg->GetAtomString(ident));
				HasError = true;
				break;
			}
			tmask >>= 2;
		}
		if (!HasError) {
			if (mlen == 1)
				mlen = 0; // I.e. scalar, not array[1]
			result = NewUnopSubNode(cg, SWIZZLE_Z_OP, SUBOP_Z(mask, mlen, len, base), fExpr);
			result->type = GetStandardType(cg->hal, base, mlen, 0);
			result->is_lvalue = LIsLValue & fExpr->is_lvalue;
			result->is_const = result->is_lvalue & fExpr->is_const;
		}
	}
	if (!result) {
		result = NewUnopSubNode(cg, SWIZZLE_Z_OP, 0, fExpr);
		result->type = UndefinedType;
	}
	return result;
}

// See if this is a valid matrix swizzle operation.  Return a new node with the proper operator description.
expr *NewMatrixSwizzleOperator(CG *cg, SourceLoc &loc, expr *fExpr, int ident) {
	int		len = 0, len2 = 0, ii, maxi, base, tmask, mask = 0, mlen = 0;
	Type	*ftype, *feltype;
	expr	*result = NULL;
	bool	LIsLValue;
	bool	HasError = false;

	mask = GetMatrixSwizzleOrWriteMask(cg, loc, ident, &LIsLValue, &mlen);
	ftype = fExpr->type;
	if (IsMatrix(ftype, &len, &len2)) {
		feltype = ftype->arr.eltype;
		maxi = len - 1;
		if (len > 4 || len2 > 4) {
			SemanticError(cg, loc, ERROR_S_MATRIX_OPERAND_GR_4, ".");
			HasError = true;
		}
	} else {
		SemanticError(cg, loc, ERROR_S_OPERANDS_NOT_MATRIX, ".");
		HasError = true;
	}
	if (!HasError) {
		base = GetBase(feltype);
		tmask = mask;
		for (ii = 0; ii < mlen; ii++) {
			if ((tmask & 0x3) >= len || ((tmask >> 2) & 0x3) >= len2) {
				SemanticError(cg, loc, ERROR_S_SWIZZLE_MASK_EL_MISSING, cg->GetAtomString(ident));
				HasError = true;
				break;
			}
			tmask >>= 4;
		}
		if (!HasError) {
			if (mlen == 1)
				mlen = 0; // I.e. scalar, not array[1]
			result = NewUnopSubNode(cg, SWIZMAT_Z_OP, SUBOP_ZM(mask, mlen, len2, len, base), fExpr);
			result->type = GetStandardType(cg->hal, base, mlen, 0);
			result->is_lvalue = LIsLValue & fExpr->is_lvalue;
			result->is_const = result->is_lvalue & fExpr->is_const;
		}
	}
	if (!result) {
		result = NewUnopSubNode(cg, SWIZMAT_Z_OP, 0, fExpr);
		result->type = UndefinedType;
	}
	return result;
}

// Construct a vector of length 1 to 4 from the expressions in fExpr.
expr *NewConstructor(CG *cg, SourceLoc &loc, Type *fType, expr *fExpr) {
	if (IsScalar(fType)) {
		Type	*lType		= fExpr->type;
		if (fExpr->bin.right || !IsScalar(lType)) {
			SemanticError(cg, loc, ERROR___TOO_MUCH_DATA_TYPE_FUN);
			return fExpr;
		}
		return CastScalarVectorMatrix(cg, fExpr->bin.left, GetBase(lType), GetBase(fType), 0, 0);

	}

	int		size = 0, nbase;
	Type	*rType;
	bool	nNumeric;
	bool	HasError = false;

	if (fType) {
		int		vlen, vlen2;
		rType = fType;
		if (IsVector(fType, &vlen)) {
			size	= vlen;
		} else if (IsMatrix(fType, &vlen, &vlen2)) {
			size	= vlen * vlen2;
		} else {
			SemanticError(cg, loc, ERROR___INVALID_TYPE_FUNCTION);
			rType	= UndefinedType;
		}
		nbase		= GetBase(rType);
		nNumeric	= cg->hal->IsNumericBase(nbase);
	} else {
		rType		= UndefinedType;
		nNumeric	= false;
	}

	int	exp_len	= 0;
	for (expr *lExpr = fExpr; lExpr; lExpr = lExpr->bin.right) {
		int		vlen;
		Type	*lType		= lExpr->type;
		int		lbase		= GetBase(lType);
		bool	lNumeric	= cg->hal->IsNumericBase(lbase);

		if (!lNumeric && lbase != TYPE_BASE_BOOLEAN) {
			SemanticError(cg, loc, ERROR___VECTOR_CONSTR_NOT_NUM_BOOL);
			HasError = true;
			break;
		}
		if (IsScalar(lType)) {
			vlen		= 1;
		} else if (!IsVector(lType, &vlen)) {
			int		vlen2;
			if (!IsMatrix(lType, &vlen, &vlen2)) {
				SemanticError(cg, loc, ERROR___VECTOR_CONSTR_NOT_SCALAR);
				HasError	= true;
				break;
			}
			vlen *= vlen2;
		}
		if (exp_len == 0 && !fType) {
			nbase		= lbase;
			nNumeric	= lNumeric;

		} else if (exp_len + vlen <= size) {
			if (lNumeric == nNumeric || lbase == TYPE_BASE_BOOLEAN) {
				//if (nNumeric) {
				//	nbase = cg->hal->GetBinOpBase(VECTOR_V_OP, nbase, lbase, 0, 0);
				//}
			} else {
				SemanticError(cg, loc, ERROR___MIXED_NUM_NONNUM_VECT_CNSTR);
				HasError = true;
				break;
			}
		} else {
			SemanticError(cg, loc, ERROR___CONSTRUCTER_VECTOR_LEN_GR_4);
			HasError = true;
			break;
		}
		exp_len += vlen;
	}
	if (size && !HasError) {
		if (size > exp_len) {
			SemanticError(cg, loc, ERROR___TOO_LITTLE_DATA_TYPE_FUN);
			HasError = true;
		} else if (size < exp_len) {
			SemanticError(cg, loc, ERROR___TOO_MUCH_DATA_TYPE_FUN);
			HasError = true;
		}
	}

	expr	*result = 0;
	if (!HasError) {
		for (expr *lExpr = fExpr; lExpr; lExpr = lExpr->bin.right) {
			Type	*lType	= lExpr->type;
			int		lbase	= GetBase(lType);

			if (lbase != nbase) {
				int		vlen	= 0;
				IsVector(lType, &vlen);
				lExpr->bin.left = CastScalarVectorMatrix(cg, lExpr->bin.left, lbase, nbase, vlen, 0);
			}
		}
		int		vlen, vlen2;
		if (IsVector(fType, &vlen)) {
			result = NewUnopSubNode(cg, VECTOR_V_OP, SUBOP_V(vlen, nbase), fExpr);
			result->type = GetStandardType(cg->hal, nbase, vlen, 0);
		} else if (IsMatrix(fType, &vlen, &vlen2)) {
			result = NewUnopSubNode(cg, MATRIX_M_OP, SUBOP_VM(vlen2, vlen, nbase), fExpr);
			result->type = GetStandardType(cg->hal, nbase, vlen, vlen2);
		}
	} else {
		result = NewUnopSubNode(cg, VECTOR_V_OP, 0, fExpr);
		result->type = rType;
	}
	return result;
}

expr *NewCastOperator(CG *cg, SourceLoc &loc, expr *fExpr, Type *toType) {
	expr *lExpr;

	if (ConvertType(cg, fExpr, toType, fExpr->type, &lExpr, false, true)) {
		lExpr->type = toType;
		return lExpr;
	} else {
		SemanticError(cg, loc, ERROR___INVALID_CAST);
		return fExpr;
	}
}

// Construct either a struct member operator, or a swizzle operator, or a writemask operator, depending upon the type of the expression "fExpr"
expr *NewMemberSelectorOrSwizzleOrWriteMaskOperator(CG *cg, SourceLoc &loc, expr *fExpr, int ident) {
	Type	*lType = fExpr->type;
	int		len, len2;
	expr	*lExpr, *mExpr;

	if (IsCategory(lType, TYPE_CATEGORY_STRUCT)) {
		if (Symbol *lSymb = LookUpLocalSymbol(cg, lType->str.members, ident)) {
			mExpr =  NewSymbNode(cg, MEMBER_OP, lSymb);
			lExpr =  NewBinopNode(cg, MEMBER_SELECTOR_OP, fExpr, mExpr);
			lExpr->is_lvalue = fExpr->is_lvalue;
			lExpr->is_const = fExpr->is_const;
			lExpr->type = lSymb->type;
		} else {
			SemanticError(cg, loc, ERROR_SS_NOT_A_MEMBER, cg->GetAtomString(ident), cg->GetAtomString(lType->str.tag));
			lExpr = fExpr;
		}
	} else if (IsCategory(lType, TYPE_CATEGORY_TEXOBJ)) {
		// object stuff
		lExpr	= fExpr;
	} else if (IsScalar(lType) || IsVector(lType, &len)) {
		lExpr = NewSwizzleOperator(cg, loc, fExpr, ident);
	} else if (IsMatrix(lType, &len, &len2)) {
		lExpr = NewMatrixSwizzleOperator(cg, loc, fExpr, ident);
	} else {
		SemanticError(cg, loc, ERROR_S_LEFT_EXPR_NOT_STRUCT_ARRAY, cg->GetAtomString(ident));
		lExpr = fExpr;
	}
	return lExpr;
}

expr *NewIndexOperator(CG *cg, SourceLoc &loc, expr *fExpr, expr *ixexpr) {
	expr *lExpr = 0;

	switch (GetCategory(fExpr->type)) {
		case TYPE_CATEGORY_ARRAY:
			lExpr = NewBinopNode(cg, ARRAY_INDEX_OP, fExpr, ixexpr);
			lExpr->is_lvalue	= fExpr->is_lvalue;
			lExpr->is_const		= fExpr->is_const;
			lExpr->type			= fExpr->type->arr.eltype;
			break;

		case TYPE_CATEGORY_STRUCT:
			if (Symbol *symb = LookUpLocalSymbol(cg, fExpr->type->str.members, GetOperatorName(cg, ARRAY_INDEX_OP))) {
				lExpr			= NewBinopNode(cg, MEMBER_SELECTOR_OP, fExpr, NewSymbNode(cg, MEMBER_OP, symb));
				lExpr->is_const	= fExpr->is_const;
				lExpr->type		= symb->type;
				lExpr = NewFunctionCallOperator(cg, loc, lExpr, ArgumentList(cg, 0, ixexpr));
			} else {
				SemanticError(cg, loc, ERROR___INDEX_OF_NON_ARRAY);
				lExpr = fExpr;
			}
			break;

		case TYPE_CATEGORY_TEXOBJ:
			lExpr = NewBinopNode(cg, ARRAY_INDEX_OP, fExpr, ixexpr);
			lExpr->is_lvalue	= fExpr->is_lvalue;
			lExpr->is_const		= fExpr->is_const;
			lExpr->type			= fExpr->type->tex.eltype;
			break;

		default:
			SemanticError(cg, loc, ERROR___INDEX_OF_NON_ARRAY);
			lExpr = fExpr;
			break;
	}
	return lExpr;
}

Symbol *lResolveOverloadedFunction(CG *cg, SourceLoc &loc, Symbol *fSymb, expr *actuals) {
	const int NO_MATCH = 0;
	const int EXACT_MATCH = 1;
	const int VALID_MATCH = 2;
	int paramno, numexact, numvalid, ii;
	Symbol *lSymb, *lExact, *lValid;
	TypeList *lFormals;

	lSymb = fSymb;
	while (lSymb) {
		lSymb->fun.flags = EXACT_MATCH;
		lSymb = lSymb->fun.overload;
	}
	paramno = 0;
	while (actuals) {
		numexact = numvalid = 0;
		lExact = lValid = fSymb;
		lSymb = fSymb;
		while (lSymb) {
			if (lSymb->fun.flags) {
				lFormals = lSymb->type->fun.paramtypes;
				for (ii = 0; ii < paramno; ii++) {
					if (lFormals) {
						lFormals = lFormals->next;
					} else {
						// Ran out of formals -- kick it out.
						lSymb->fun.flags = NO_MATCH;
					}
				}
				if (lFormals) {
					if (IsSameUnqualifiedType(lFormals->p, actuals->type)) {
						lSymb->fun.flags = EXACT_MATCH;
						lExact = lSymb;
						numexact++;
					} else if (ConvertType(cg, NULL, lFormals->p, actuals->type, NULL, false, false)) {
						lSymb->fun.flags = VALID_MATCH;
						lValid = lSymb;
						numvalid++;
					} else {
						lSymb->fun.flags = NO_MATCH;
					}
				} else {
					lSymb->fun.flags = NO_MATCH;
				}
			}
			lSymb = lSymb->fun.overload;
		}
		if (numexact > 0) {
			if (numexact == 1)
				return lExact;
			if (numvalid > 0) {
				// Disqualify non-exact matches:
				lSymb = fSymb;
				while (lSymb) {
					if (lSymb->fun.flags == VALID_MATCH)
						lSymb->fun.flags = NO_MATCH;
					lSymb = lSymb->fun.overload;
				}
			}
		} else {
			if (numvalid == 1)
				return lValid;
			if (numvalid == 0) {
				// Nothing matches.
				break;
			}
		}
		actuals = actuals->bin.right;
		paramno++;
	}
	// If multiple matches still present check number of args:
	if (numexact > 0 || numvalid > 0) {
		numvalid = 0;
		lSymb = lValid = fSymb;
		while (lSymb) {
			if (lSymb->fun.flags) {
				lFormals = lSymb->type->fun.paramtypes;
				for (ii = 0; ii < paramno; ii++) {
					if (lFormals) {
						lFormals = lFormals->next;
					} else {
						// Ran out of formals -- shouldn't happen.
						assert(0);
					}
				}
				if (lFormals) {
					lSymb->fun.flags = NO_MATCH;
				} else {
					numvalid++;
					lValid = lSymb;
				}
			}
			lSymb = lSymb->fun.overload;
		}
		if (numvalid == 1)
			return lValid;
	}
	if (numvalid > 0) {
		SemanticError(cg, loc, ERROR_S_AMBIGUOUS_FUN_REFERENCE, cg->GetAtomString(fSymb->name));
	} else {
		SemanticError(cg, loc, ERROR_S_NO_COMPAT_OVERLOADED_FUN, cg->GetAtomString(fSymb->name));
	}
#if 0 // Detailed error messages - requires printing of types
	lSymb = fSymb;
	numvalid = 0;
	while (lSymb) {
		if (lSymb->fun.flags) {
			printf("    #%d: ", ++numvalid);
			PrintType(cg, lSymb->type->fun.rettype, 0);
			printf(" %s", cg->GetAtomString(lSymb->name));
			PrintType(cg, lSymb->type, 0);
			printf("\n");
		}
		lSymb = lSymb->fun.overload;
	}
#endif
	return fSymb;
}

expr *NewFunctionCallOperator(CG *cg, SourceLoc &loc, expr *funExpr, expr *actuals) {
	Type *funType = funExpr->type;

	if (!IsCategory(funType, TYPE_CATEGORY_FUNCTION)) {
		if (IsCategory(funType, TYPE_CATEGORY_STRUCT)) {
			if (Symbol *symb = LookUpLocalSymbol(cg, funExpr->type->str.members, GetOperatorName(cg, FUN_CALL_OP)))
				return NewFunctionCallOperator(cg, loc, NewSymbNode(cg, VARIABLE_OP, symb), actuals);
		}
		SemanticError(cg, loc, ERROR___CALL_OF_NON_FUNCTION);
		expr	*result = NewBinopNode(cg, FUN_CALL_OP, funExpr, actuals);
		result->type = UndefinedType;
		return result;
	}

	opcode		lop		= FUN_CALL_OP;
	int			lsubop	= 0;
	expr		*me		= 0;
	Symbol		*lSymb;

	while (funExpr->kind == BINARY_N) {
		if (funExpr->op == MEMBER_SELECTOR_OP) {
			me		= funExpr->bin.left;
			funExpr = funExpr->bin.right;
		}
	}

	if (funExpr->kind == SYMB_N) {
		lSymb = funExpr->sym.symbol;
		if (lSymb->kind == FUNCTION_S) {
			if (lSymb->fun.overload) {
				lSymb = lResolveOverloadedFunction(cg, loc, lSymb, actuals);
				funExpr->sym.symbol = lSymb;
				funType = funExpr->type = lSymb->type;
			}
			if (funType->properties & TYPE_MISC_INTERNAL) {
				lop		= FUN_BUILTIN_OP;
				lsubop	= (lSymb->fun.group << 16) | lSymb->fun.index;
			}
			if (lSymb->storage == SC_STATIC)
				me = 0;
		} else {
			InternalError(cg, loc, ERROR_S_SYMBOL_NOT_FUNCTION, cg->GetAtomString(lSymb->name));
		}
	}
	TypeList	*lFormals	= funType->fun.paramtypes;
	expr		*lActuals	= actuals;
	int			paramno		= 0;

	// stick me at start of actual params
	if (me) {
		//lFormals	= lFormals->next;
		actuals		= ArgumentList(cg, 0, me);
		actuals->bin.right = lActuals;
	}

	bool	has_out_params = false;
	while (lFormals && lActuals) {
		paramno++;
		Type	*formalType	= lFormals->p;
		Type	*actualType	= lActuals->type;
		expr	*lExpr		= lActuals->bin.left;
		int		inout		= 0;

		if ((formalType->properties & TYPE_QUALIFIER_IN) || !(formalType->properties & TYPE_QUALIFIER_INOUT))
			inout |= 1;

		if (formalType->properties & TYPE_QUALIFIER_OUT) {
			inout |= 2;
			has_out_params = true;

			if (lExpr) {
				if (lExpr->is_lvalue) {
					if (!(GetQualifiers(actualType) & TYPE_QUALIFIER_CONST)) {
						if (IsSameUnqualifiedType(formalType, actualType) && IsPacked(formalType) == IsPacked(actualType)) {
							SUBOP_SET_MASK(lActuals->bin.subop, inout);
						} else {
							SemanticError(cg, loc, ERROR_D_OUT_PARAM_NOT_SAME_TYPE, paramno);
						}
					} else {
						SemanticError(cg, loc, ERROR_D_OUT_PARAM_IS_CONST, paramno);
					}
				} else {
					SemanticError(cg, loc, ERROR_D_OUT_PARAM_NOT_LVALUE, paramno);
				}
			}
		} else if (ConvertType(cg, lExpr, formalType, actualType, &lExpr, false, false)) {
			lActuals->bin.left = lExpr;
			SUBOP_SET_MASK(lActuals->bin.subop, inout);
		} else {
			SemanticError(cg, loc, ERROR_D_INCOMPATIBLE_PARAMETER, paramno);
		}
		lFormals = lFormals->next;
		lActuals = lActuals->bin.right;
	}
	if (lFormals) {
		if (!IsVoid(lFormals->p))
			SemanticError(cg, loc, ERROR___TOO_FEW_PARAMS);
	} else if (lActuals) {
		SemanticError(cg, loc, ERROR___TOO_MANY_PARAMS);
	}

	expr	*result = NewBinopSubNode(cg, lop, lsubop, funExpr, actuals);
	result->is_lvalue	= (funType->fun.rettype->properties & TYPE_QUALIFIER_INOUT) == TYPE_QUALIFIER_INOUT;
	result->is_const	= false;
	result->has_sideeffects |= has_out_params;
	result->type 		= funExpr->type->fun.rettype;

	return result;
}

expr *NewSimpleAssignment(CG *cg, SourceLoc &loc, expr *fVar, expr *fExpr, bool InInit) {
	int subop, base, len, vqualifiers, vdomain, edomain;
	opcode lop;
	Type *vType, *eType;
	expr *lExpr;

	vType = fVar->type;
	eType = fExpr->type;
	vqualifiers = GetQualifiers(vType);
	vdomain = GetDomain(vType);
	edomain = GetDomain(eType);
	if (!fVar->is_lvalue)
		SemanticError(cg, loc, ERROR___ASSIGN_TO_NON_LVALUE);
	//if ((vqualifiers & TYPE_QUALIFIER_CONST) && !InInit)
	if (fVar->is_const && !InInit)
		SemanticError(cg, loc, ERROR___ASSIGN_TO_CONST_VALUE);
	if (vdomain == TYPE_DOMAIN_UNIFORM && edomain == TYPE_DOMAIN_VARYING)
		SemanticError(cg, loc, ERROR___ASSIGN_VARYING_TO_UNIFORM);
	if (ConvertType(cg, fExpr, vType, eType, &lExpr, InInit, false)) {
		fExpr = lExpr;
	} else {
		if (vType != UndefinedType && eType != UndefinedType)
			SemanticError(cg, loc, ERROR___ASSIGN_INCOMPATIBLE_TYPES);
	}
	base = GetBase(vType);
	if (IsScalar(vType)) {
		lop = ASSIGN_OP;
		subop = SUBOP__(base);
	} else if (IsVector(vType, &len)) {
		lop = ASSIGN_V_OP;
		subop = SUBOP_V(len, base);
	} else {
		lop = ASSIGN_GEN_OP;
		subop = SUBOP__(base);
	}
	lExpr =  NewBinopSubNode(cg, lop, subop, fVar, fExpr);
	lExpr->type = vType;
	return lExpr;
}

stmt *NewSimpleAssignmentStmt(CG *cg, SourceLoc &loc, expr *fVar, expr *fExpr, bool InInit) {
	return NewExprStmt(loc, NewSimpleAssignment(cg, loc, fVar, fExpr, InInit));
}

stmt *NewCompoundAssignmentStmt(CG *cg, SourceLoc &loc, opcode op, expr *fVar, expr *fExpr) {
	return NewExprStmt(loc,  NewBinopNode(cg, op, fVar, fExpr));
}

#if 000
/*
* NewMaskedAssignment() - Add a new masked assignment to an assignment statement.
*
* "fStmt" is an assignment stmt of the form: "exp" or "var = exp" ...
* Returns a stmt of the form: "var@@mask = exp" or "var@@mask = (var = exp)" ...
*
*/

stmt *NewMaskedAssignment(SourceLoc &loc, int mask, expr *fExpr, stmt *fStmt)
{
	int lop, subop, base, len;
	expr *lExpr;
	Type *lType;
	char str[5];
	int ii, kk;

	if (!fExpr->is_lvalue)
		SemanticError(cg, loc, ERROR___ASSIGN_TO_NON_LVALUE);
	if (ConvertType(fStmt->exprst.exp, fExpr->type, fStmt->exprst.exp->type, &lExpr, 0)) {
		fStmt->exprst.exp = lExpr;
	} else {
		SemanticError(cg, loc, ERROR___ASSIGN_INCOMPATIBLE_TYPES);
	}
	lType = fExpr->type;
	base = GetBase(lType);
	if (IsScalar(lType)) {
		SemanticError(cg, loc, ERROR___MASKED_ASSIGN_TO_VAR);
		lop = ASSIGN_OP;
		subop = SUBOP__(base);
	} else if (IsVector(lType, &len)) {
		if (len > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, "@@");
			len = 4;
		}
		if ((~0 << len) & mask) {
			kk = 0;
			for (ii = len; ii < 4; ii++) {
				if (mask & (1 << ii))
					str[kk++] = "xyzw"[ii];
			}
			str[kk] = '\0';
			SemanticError(cg, loc, ERROR_S_MASKED_ASSIGN_NON_EXISTENT, str);
			mask &= (1 << len) - 1;
		}
		lop = ASSIGN_MASKED_KV_OP;
		subop = SUBOP_KV(mask, len, base);
	} else {
		SemanticError(cg, loc, ERROR___MASKED_ASSIGN_TO_VAR);
		lop = ASSIGN_GEN_OP;
		subop = SUBOP__(base);
	}
	fStmt->exprst.exp =  NewBinopSubNode(cg, lop, subop, fExpr, fStmt->exprst.exp);
	fStmt->exprst.exp->type = lType;
	return fStmt;
}

/*
* NewConditionalAssignment() - Add a new simple assignment to an assignment statement.
*
* "fStmt" is an assignment stmt of the form: "exp" or "var = exp" ...
* Returns a stmt of the form: "var@@(cond) = exp" or "var@@(cond) = (var = exp)" ...
*
*/

stmt *NewConditionalAssignment(SourceLoc &loc, expr *fcond, expr *fExpr, stmt *fStmt)
{
	int lop, subop, base, len, clen;
	expr *lExpr;
	Type *lType, *ctype;

	if (!fExpr->is_lvalue)
		SemanticError(cg, loc, ERROR___ASSIGN_TO_NON_LVALUE);
	if (ConvertType(fStmt->exprst.exp, fExpr->type, fStmt->exprst.exp->type, &lExpr, 0)) {
			fStmt->exprst.exp = lExpr;
	} else {
		SemanticError(cg, loc, ERROR___ASSIGN_INCOMPATIBLE_TYPES);
	}
	lType = fExpr->type;
	base = GetBase(lType);
	ctype = fcond->type;
	if (IsScalar(lType)) {
		if (!IsScalar(ctype))
			SemanticError(cg, loc, ERROR___SCALAR_BOOL_EXPR_EXPECTED);
		lop = ASSIGN_COND_OP;
		subop = SUBOP__(base);
	} else if (IsVector(lType, &len)) {
		if (len > 4) {
			SemanticError(cg, loc, ERROR_S_VECTOR_OPERAND_GR_4, "@@()");
			len = 4;
		}
		if (IsScalar(ctype)) {
			lop = ASSIGN_COND_SV_OP;
		} else {
			lop = ASSIGN_COND_V_OP;
			if (IsVector(ctype, &clen)) {
				if (clen != len)
					SemanticError(cg, loc, ERROR_S_VECTOR_OPERANDS_DIFF_LEN, "@@()");
			} else {
				SemanticError(cg, loc, ERROR_S_INVALID_CONDITION_OPERAND);
			}
		}
		subop = SUBOP_V(len, base);
	} else {
		lop = ASSIGN_COND_GEN_OP;
		subop = SUBOP__(base);
	}
	fStmt->exprst.exp =  NewTriopSubNode(lop, subop, fExpr, fcond, fStmt->exprst.exp);
	fStmt->exprst.exp->type = lType;
	return fStmt;
}
#endif

/*************************************** misc: ******************************************/

// initialize the tempptr fields of an expr node -- designed to be passed as an argument to ApplyToXXXX
// Sets tempptr[0] to arg1 and clears the rest of tempptr to NULL
void InitTempptr(expr *fExpr, void *arg1, int arg2) {
	fExpr->tempptr[0] = arg1;
	memset(&fExpr->tempptr[1], 0, sizeof(fExpr->tempptr) - sizeof(fExpr->tempptr[0]));
}

/********************************** Error checking: ******************************************/

ErrorLoc *ErrorLocsFirst = NULL;
ErrorLoc *ErrorLocsLast = NULL;
int ErrorPending = 0;

// Record the fact that an error token was seen at source location LOC.
void RecordErrorPos(SourceLoc &loc) {
	ErrorLoc *eloc = new ErrorLoc(loc);
	if (ErrorLocsFirst) {
		ErrorLocsLast->next = eloc;
	} else {
		ErrorLocsFirst = eloc;
	}
	ErrorLocsLast = eloc;

	// If an error has been generated during parsing before the error
	// token was seen, then we mark this error token as being hit and
	// clear the error pending flag.

	if (ErrorPending) {
		eloc->hit = true;
		ErrorPending = 0;
	}
}

// Verify that at least one error was generated in the appropriate location for each error token that appeared in the program.
void CheckAllErrorsGenerated(CG *cg) {
	// Turn off ErrorMode so we can generate real errors for the absence of errors.
	//cg->options.ErrorMode = 0;

	for (ErrorLoc *eloc = ErrorLocsFirst; eloc != NULL; eloc = eloc->next) {
		if (eloc->hit == 0)
			SemanticError(cg, *eloc, ERROR___NO_ERROR);
	}
}

extern int yylex(YYSTYPE *lvalp, CG *cg);
extern int yychar;
stmt* ParseAsm(CG *cg, SourceLoc &loc) {
	YYSTYPE	val;
	char	buffer[1024], *p = buffer;
	int	c;
	while ((c = yylex(&val, cg)) != '}')
		*p++ = c;
	cg->ungetch(c);
	*p = 0;

	return NewCommentStmt(cg, loc, buffer);
}

} //namespace cgc
