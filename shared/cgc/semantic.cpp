#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cg.h"
#include "errors.h"
#include "compile.h"

namespace cgc {

static void GetVectorConst(CG *cg, float *f, expr *e) {
	int Oops = 0;
	if (e) {
		switch (e->kind) {
		case UNARY_N:
			switch (e->op) {
				//case FCONST_V_OP:
				//    for (ii = 0; ii < SUBOP_GET_S1(pun->subop); ii++)
				//        f[ii] = 1.1f;
				//    break;
				case VECTOR_V_OP:
					Oops = 2;
					break;
				default:
					Oops = 1;
					break;
				}
			break;
		case BINARY_N:
			switch (e->op) {
				case EXPR_LIST_OP:
					if (e->bin.right == NULL && e->bin.left->kind == UNARY_N) {
						if (e->bin.left->op == MATRIX_M_OP)
							e = e->bin.left->un.arg;
					}
					while (e) {
						expr *left = e->bin.left;
						switch (left->kind) {
							case CONST_N:
								switch (left->op) {
									case FCONST_V_OP:
									case HCONST_V_OP:
									case XCONST_V_OP:
										for (int i = 0; i < SUBOP_GET_S1(left->co.subop); i++)
											*f++ = e->co.val[i].f;
										break;
									case ICONST_V_OP:
									case BCONST_V_OP:
										for (int i = 0; i < SUBOP_GET_S1(left->co.subop); i++)
											*f++ = e->co.val[i].i;
										break;
									case FCONST_OP:
									case HCONST_OP:
									case XCONST_OP:
										f[0] = left->co.val[0].f;
										break;
									case ICONST_OP:
									case UCONST_OP:
									case BCONST_OP:
										*f++ = left->co.val[0].i;
										break;
									default:
										Oops = 4;
										break;
								}
								break;
							default:
								GetVectorConst(cg, f, left);
								break;
						}
						e = e->bin.right;
					}
					if (e) {
						Oops = 3;
					}
					break;
				default:
					Oops = 1;
					break;
			}
			break;
		default:
			Oops = 1;
			break;
		}
	}
	if (Oops)
		SemanticWarning(cg, cg->tokenLoc, 9999, "*** GetVectorConst() not finished ***");
}

// Bind a uniform variable to $uniform connector.  Record semantics value if present.
static bool BindUniformVariable(CG *cg, Symbol *sym, int gname, bool is_parameter) {
	switch (GetCategory(sym->type)) {
		case TYPE_CATEGORY_SCALAR:
		case TYPE_CATEGORY_ARRAY:
		case TYPE_CATEGORY_STRUCT:
		case TYPE_CATEGORY_TEXOBJ: {
			UniformSemantic *lUniform = new(cg->pool()) UniformSemantic(gname, sym->name, sym->var.semantics);
			UniformSemantic	*nUniform = cg->uniforms;
			if (nUniform) {
				while (nUniform->next)
					nUniform = nUniform->next;
				nUniform->next = lUniform;
			} else {
				cg->uniforms = lUniform;
			}

			SymbolList **list	= is_parameter ? &cg->uniformParam : &cg->uniformGlobal;
			Binding		*bind	= new(cg->pool()) Binding(gname, sym->name);
			bind->properties	= BIND_INPUT | BIND_UNIFORM;

			if (BindingTree *ltree = cg->LookupBinding(gname, sym->name)) {
				if (cg->hal->BindUniformPragma(cg, sym->loc, sym, bind, &ltree->binding)) {
					if (!(bind->properties & BIND_UNIFORM)) {
						SemanticError(cg, sym->loc, ERROR_S_NON_UNIF_BIND_TO_UNIF_VAR, cg->GetAtomString(sym->name));
						list = NULL;
					}
				} else {
					SemanticError(cg, sym->loc, ERROR_S_INCOMPATIBLE_BIND_DIRECTIVE, cg->GetAtomString(sym->name));
					list = NULL;
				}
			}
			if (list && !(bind->properties & BIND_HIDDEN)) {
				sym->var.bind = bind;
				AddToSymbolList(list, sym);
				return true;
			}
			break;
		}
		default:
			SemanticError(cg, sym->loc, ERROR_S_ILLEGAL_TYPE_UNIFORM_VAR, cg->GetAtomString(sym->name));
			break;
	}
	return false;
}

// Bind a variable to $vin, $vout, or $uniform connector.
Symbol *BindVaryingVariable(CG *cg, Symbol *sym, int gname, Scope *inscope, Scope *outscope, bool is_out_val, int struct_semantics) {
	Scope	*scope		= NULL;
	Symbol	*lSymb		= NULL;
	int		semantics	= sym->var.semantics;

	switch (GetCategory(sym->type)) {
		case TYPE_CATEGORY_SCALAR:
		case TYPE_CATEGORY_ARRAY: {
			Binding		*bind = new(cg->pool()) Binding(gname, sym->name);
			BindingTree *ltree = cg->LookupBinding(gname, sym->name);
			if (semantics) {
				if (ltree)
					SemanticWarning(cg, sym->loc, WARNING_S_SEMANTICS_AND_BINDING, cg->GetAtomString(sym->name));

				if (cg->hal->BindVaryingSemantic(cg, sym->loc, sym, semantics, bind, is_out_val)) {
					if (bind->properties & BIND_INPUT) {
						if (is_out_val)
							SemanticError(cg, sym->loc, ERROR_S_OUT_QUALIFIER_IN_SEMANTIC, cg->GetAtomString(semantics));
						scope = inscope;
					} else {
						if (!is_out_val)
							SemanticError(cg, sym->loc, ERROR_S_IN_QUALIFIER_OUT_SEMANTIC, cg->GetAtomString(semantics));
						scope = outscope;
					}
				} else {
					SemanticError(cg, sym->loc, ERROR_S_UNKNOWN_SEMANTICS, cg->GetAtomString(semantics));
				}
			} else {
				// If no semantics, check for #pragma bind directive.
				semantics = sym->name;
				if (ltree) {
					if (cg->hal->BindVaryingPragma(cg, sym->loc, sym, bind, &ltree->binding, is_out_val)) {
						if (bind->properties & BIND_UNIFORM)
							SemanticError(cg, sym->loc, ERROR_S_UNIF_BIND_TO_NON_UNIF_VAR, cg->GetAtomString(sym->name));
						else
							scope = bind->properties & BIND_INPUT ? inscope : outscope;
					} else {
						SemanticError(cg, sym->loc, ERROR_S_INCOMPATIBLE_BIND_DIRECTIVE, cg->GetAtomString(sym->name));
					}
				} else {
					// If no semantics or #pragma bind, get default binding from profile if it allows them:
					if (cg->hal->BindVaryingUnbound(cg, sym->loc, sym, sym->name, struct_semantics, bind, is_out_val))
						scope = bind->properties & BIND_INPUT ? inscope : outscope;
					else
						SemanticError(cg, sym->loc, ERROR_S_SEMANTIC_NOT_DEFINED_VOUT, cg->GetAtomString(sym->name));
				}
			}
			if (scope) {
				if (!bind->lname)
					bind->lname = semantics;
				sym->var.bind = bind;
				if (!(bind->properties & BIND_HIDDEN)) {
					lSymb = LookUpLocalSymbol(cg, scope, bind->lname);
					if (lSymb) {
						// Already defined - second use of this name.
					} else {
						sym->type->properties &= ~TYPE_QUALIFIER_OUT;
						lSymb = AddMemberSymbol(cg, sym->loc, scope, bind->lname, sym->type);
						lSymb->var.bind = bind;
					}
				}
			}
			break;
		}
		case TYPE_CATEGORY_STRUCT:
			SemanticError(cg, sym->loc, ERROR_S_NESTED_SEMANTIC_STRUCT, cg->GetAtomString(semantics ? semantics : sym->name));
			break;
		default:
			SemanticError(cg, sym->loc, ERROR_S_ILLEGAL_PARAM_TO_MAIN, cg->GetAtomString(semantics ? semantics : sym->name));
			break;
	}
	return lSymb;
}

// Verify that this connector name is valid for the current profile and is of the appropriate direction
static void lVerifyConnectorDirection(CG *cg, SourceLoc &loc, int semantics, int IsOutParam) {
	if (semantics) {
		if (int cid = cg->hal->GetConnectorID(semantics)) {
			int	uses = cg->hal->GetConnectorUses(cid, cg->hal->pid);
			if (IsOutParam) {
				if (!(uses & CONNECTOR_IS_OUTPUT))
					SemanticError(cg, loc, ERROR_S_CONNECT_FOR_INPUT, cg->GetAtomString(semantics));
			} else {
				if (!(uses & CONNECTOR_IS_INPUT))
					SemanticError(cg, loc, ERROR_S_CONNECT_FOR_OUTPUT, cg->GetAtomString(semantics));
			}
		} else {
			SemanticError(cg, loc, ERROR_S_CONNECTOR_TYPE_INVALID, cg->GetAtomString(semantics));
		}
	}
}

// Insert a series of assignment statements before each return statement to set the values of the program's result for these members
// (Should only be applied to the main program.)  Deletes the return statement.
struct BuildReturnAssignments {
	Symbol	*program;
	Symbol	*varyingOut;
	stmt	*endstmts;
	stmt	*pre_stmt(CG *cg, stmt *s, int arg2);
	BuildReturnAssignments(Symbol *program, Symbol *varyingOut, stmt *endstmts) : program(program), varyingOut(varyingOut), endstmts(endstmts) {}
};

stmt *BuildReturnAssignments::pre_stmt(CG *cg, stmt *fStmt, int arg2) {
	if (fStmt->kind == RETURN_STMT) {
		Type	*rettype	= program->type->fun.rettype;
		int		category	= GetCategory(rettype);
		expr	*returnVar 	= fStmt->returnst.exp;
		stmt	*stmtlist	= NULL;

		if (category == TYPE_CATEGORY_STRUCT) {
			if (returnVar->kind != SYMB_N) {
				returnVar 	= NewTmp(cg, returnVar);
				stmtlist 	= NewSimpleAssignmentStmt(cg, program->loc, returnVar, fStmt->returnst.exp, 0);
			}

			Scope	*lScope = rettype->str.members;
			for (Symbol *lSymb = lScope->symbols; lSymb; lSymb = lSymb->next) {
				// Create an assignment statement of the bound variable to the $vout member:
				Symbol	*retSymb	= LookUpLocalSymbol(cg, lScope, lSymb->name);
				Symbol	*outSymb	= LookUpLocalSymbol(cg, varyingOut->type->str.members, lSymb->name);
				if (!outSymb) {
					outSymb	= LookUpLocalSymbol(cg, varyingOut->type->str.members, lSymb->var.semantics);

				}
				if (outSymb && retSymb) {
					// outSymb may not be in the symbol table if it's a "hidden" register.
					expr	*outputVar	= NewSymbNode(cg, VARIABLE_OP, varyingOut);
					expr	*lExpr		= GenMemberReference(cg, outputVar, outSymb);
					expr	*rexpr		= GenMemberReference(cg, returnVar, retSymb);
					int		len;
					if (IsScalar(lSymb->type) || IsVector(lSymb->type, &len)) {
						stmtlist = ConcatStmts(stmtlist, NewSimpleAssignmentStmt(cg, program->loc, lExpr, rexpr, 0));
					} else {
						FatalError(cg, "Return of unsupported type");
						// xxx
					}
				}
			}
			
		} else {
			Symbol	*outSymb	= LookUpLocalSymbol(cg, varyingOut->type->str.members, program->fun.semantics);
			expr	*outputVar	= NewSymbNode(cg, VARIABLE_OP, varyingOut);
			expr	*lExpr		= GenMemberReference(cg, outputVar, outSymb);
			stmtlist = NewSimpleAssignmentStmt(cg, program->loc, lExpr, DupExpr(cg, returnVar), 0);
		}

//		fStmt = ConcatStmts(DuplicateStatementTree(cg, endstmts), ConcatStmts(stmtlist, NewReturnStmt(cg, program->loc, NULL, GenSymb(cg, varyingOut))));
		fStmt = ConcatStmts(stmtlist, ConcatStmts(DuplicateStatementTree(cg, endstmts), NewReturnStmt(cg, program->loc, NULL, GenSymb(cg, varyingOut))));
	}
	return fStmt;
}

// Build the three global semantic type structures, Check main for type errors in its arguments.
static void BuildSemanticStructs2(CG *cg, StmtList *instmts, StmtList *outstmts, Symbol *varyingIn, Symbol *varyingOut, int gname, Symbol *sym, expr *e) {
	Type	*type		= sym->type;
	int		category	= GetCategory(type);
	int		qualifiers	= GetQualifiers(type);
	bool	IsOutParam	= (qualifiers & TYPE_QUALIFIER_OUT) != 0;
	int		len = 1, rlen = 1, nreg = 1;

	if (!IsOutParam)
		qualifiers |= TYPE_QUALIFIER_IN;

	switch (category) {
		case TYPE_CATEGORY_ARRAY:
			IsVector(type, &len);
		case TYPE_CATEGORY_SCALAR: {
			for (int out = 0; out < 2; out++) {
				if (qualifiers & (out ? TYPE_QUALIFIER_OUT : TYPE_QUALIFIER_IN)) {
					if (Symbol *lSymb = BindVaryingVariable(cg, sym, gname, varyingIn->type->str.members, varyingOut->type->str.members, out != 0, 0)) {
						Binding	*bind = lSymb->var.bind;
						if (bind && !(bind->properties & BIND_HIDDEN)) {
							if (bind->properties & BIND_INPUT) {
								// Assign $vin member to bound variable:
								expr	*rExpr = GenMemberReference(cg, NewSymbNode(cg, VARIABLE_OP, varyingIn), lSymb);
								if (IsVector(lSymb->type, &rlen))
									rExpr = GenConvertVectorLength(cg, rExpr, GetBase(lSymb->type), rlen, len);
								if (e->op == VARIABLE_OP)
									sym->var.init = rExpr;
								else
									AppendStatements(instmts, NewSimpleAssignmentStmt(cg, sym->loc, e, rExpr, 0));
							} else {
								// Assign bound variable to $vout member:
								expr	*rExpr = GenMemberReference(cg, NewSymbNode(cg, VARIABLE_OP, varyingOut), lSymb);
								if (IsVector(lSymb->type, &rlen))
									e = GenConvertVectorLength(cg, e, GetBase(type), len, rlen);
								AppendStatements(outstmts, NewSimpleAssignmentStmt(cg, sym->loc, rExpr, e, 0));
							}
						}
					}
				}
			}
			break;
		}
		case TYPE_CATEGORY_STRUCT: {
			lVerifyConnectorDirection(cg, sym->loc, type->str.semantics, IsOutParam);
			for (Symbol *member = type->str.members->symbols; member; member = member->next) {
				if (!IsFunction(member))
					BuildSemanticStructs2(cg, instmts, outstmts, varyingIn, varyingOut, type->str.tag, member, GenMemberReference(cg, e, member));
			}
			break;
		}
		default:
			SemanticError(cg, sym->loc, ERROR_S_ILLEGAL_PARAM_TO_MAIN, cg->GetAtomString(sym->name));
			break;
	}
}

stmt *BuildSemanticStructs(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program, const char *vin, const char *vout) {
	StmtList instmts, outstmts;

	// Define pseudo type structs for semantics:

	Symbol	*varyingIn, *varyingOut;
	{
		int		vinTag			= cg->AddAtom(vin);
		Type	*vinType		= StructHeader(cg, loc, fScope, 0, vinTag);
		Scope	*vinScope		= NewScopeInPool(fScope->pool);
		vinScope->level			= 1;
		vinScope->flags			|= Scope::is_struct | Scope::has_semantics;
		vinType->str.members	= vinScope;
		varyingIn 				= NewSymbol(loc, fScope, cg->AddAtom("vin"), vinType, VARIABLE_S);
	}

	{
		int		voutTag			= cg->AddAtom(vout);
		Type	*voutType		= StructHeader(cg, loc, fScope, 0, voutTag);
		Scope	*voutScope		= NewScopeInPool(fScope->pool);
		voutScope->flags		|= Scope::is_struct | Scope::has_semantics;
		voutScope->level		= 1;
		voutType->str.members	= voutScope;
		varyingOut				= NewSymbol(loc, fScope, cg->AddAtom("vout"), voutType, VARIABLE_S);
	}

	// Walk list of formals creating semantic struct members for all parameters:
	for (Symbol *formal = program->fun.params; formal; formal = formal->next) {
		int	domain = GetDomain(formal->type);

		if (domain == TYPE_DOMAIN_UNIFORM) {
			if (IsOut(formal->type))
				SemanticError(cg, formal->loc, ERROR_S_UNIFORM_ARG_CANT_BE_OUT, cg->GetAtomString(formal->name));
			switch (GetCategory(formal->type)) {
				case TYPE_CATEGORY_SCALAR:
				case TYPE_CATEGORY_ARRAY:
				case TYPE_CATEGORY_STRUCT:
					if (BindUniformVariable(cg, formal, program->name, true) && formal->var.init) {
						float	lVal[64];
						memset(lVal, 0, sizeof(lVal));
						formal->var.init = FoldConstants(cg, formal->var.init, FC_DEFAULT);
						GetVectorConst(cg, lVal, formal->var.init);
						Binding	*bind = NewConstDefaultBinding(cg->pool(), 0, formal->name, 4, 0, 0, lVal);
						bind->kind = BK_DEFAULT;
						cg->AddDefaultBinding(bind);
					}
					break;
				default:
					SemanticError(cg, formal->loc, ERROR_S_ILLEGAL_PARAM_TO_MAIN, cg->GetAtomString(formal->name));
					break;
			}
		} else {
			formal->properties &= ~SYMB_IS_PARAMETER;
			BuildSemanticStructs2(cg, &instmts, &outstmts, varyingIn, varyingOut, program->name, formal, NewSymbNode(cg, VARIABLE_OP, formal));
		}
	}

	// Add return value's semantics to the $vout connector:
	Type	*rettype 	= program->type->fun.rettype;
	if (!IsVoid(rettype)) {
		switch (GetCategory(rettype)) {
			case TYPE_CATEGORY_STRUCT:
				lVerifyConnectorDirection(cg, program->loc, rettype->str.semantics, 1);
				for (Symbol *member = rettype->str.members->symbols; member; member = member->next)
					BindVaryingVariable(cg, member, program->name, varyingIn->type->str.members, varyingOut->type->str.members, true, 0);
				break;
				
			default: {
				Binding	*bind = new(cg->pool()) Binding(program->name, program->name);
				if (cg->hal->BindVaryingSemantic(cg, program->loc, program, program->fun.semantics, bind, true)) {
					Symbol	*sym = AddMemberSymbol(cg, program->loc, varyingOut->type->str.members, program->fun.semantics, rettype);
					sym->var.bind = bind;
				}
				break;
			}
		}
	}
	
	if (varyingIn->type->str.members->symbols) {
		varyingIn->properties |= SYMB_IS_PARAMETER;
		AddToScope(cg, fScope, varyingIn);
		cg->varyingIn = varyingIn;
		program->fun.params	= varyingIn;
	}

	stmt 	*lStmt 		= program->fun.statements;

	if (varyingOut->type->str.members->symbols) {
		BuildReturnAssignments	lstr(program, varyingOut, outstmts.first);
		lStmt = cg->ApplyToStatements(lstr, lStmt);

		AddToScope(cg, fScope, varyingOut);
		cg->varyingOut 		= varyingOut;
		program->type->fun.rettype	= varyingOut->type;
	}
	
	// Append initial assignment statements to beginning and end of main:
	return ConcatStmts(instmts.first, lStmt);
}

void BindDefaultSemantic(CG *cg, Symbol *sym) {
	Binding *bind;
	switch (GetCategory(sym->type)) {
		case TYPE_CATEGORY_SCALAR:
		case TYPE_CATEGORY_ARRAY:
		case TYPE_CATEGORY_STRUCT:
			if (BindUniformVariable(cg, sym, 0, false) && sym->var.init) {
				sym->var.init = FoldConstants(cg, sym->var.init, FC_DEFAULT);
				float lVal[64];
				memset(lVal, 0, sizeof(lVal));
				GetVectorConst(cg, lVal, sym->var.init);
				bind = NewConstDefaultBinding(cg->pool(), 0, sym->name, sym->type->size, 0, 0, lVal);
				bind->kind = BK_DEFAULT;
				cg->AddDefaultBinding(bind);
			}
			break;
		case TYPE_CATEGORY_TEXOBJ:
			BindUniformVariable(cg, sym, 0, false);
			break;
		default:
			SemanticError(cg, sym->loc, ERROR_S_NON_STATIC_GLOBAL_TYPE_ERR, cg->GetAtomString(sym->name));
	}
	sym->properties &= ~SYMB_NEEDS_BINDING;
}

} //namespace cgc
