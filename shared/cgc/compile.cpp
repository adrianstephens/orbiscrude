#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cgclib.h"
#include "compile.h"
#include "errors.h"
#include "printutils.h"

namespace cgc {

stmt *StaticAnalysis(CG *cg, Scope *fscope, stmt *body);

// If there is a numeric suffix on a string, strip it off and retrun the value as an integer
bool HasNumericSuffix(const char *fStr, char *root, size_t size, int *suffix) {
	strncpy(root, fStr, size - 1);
	size_t	len = strlen(fStr);
	if (len >= size)
		len = size - 1;
	root[len] = 0;
	
	int		val			= 0;
	bool	has_suffix	= false;
	int		scale		= 1;
	
	char	*s = &root[len];
	for (;;) {
		char ch = *--s;
		if (ch >= '0' && ch <= '9' && s >= root) {
			val = val + scale * (ch - '0');
			scale *= 10;
			has_suffix = true;
		} else {
			break;
		}
	}
	s[1]	= '\0';
	*suffix	= val;
	return has_suffix;
}
//-----------------------------------------------------------------------------
//	Analysis and Code Generation Control Functions
//-----------------------------------------------------------------------------

// SetSymbolFlags() - Set the flags field for all symbols in this tree.
void SetSymbolFlags(Symbol *fSymb, int fVal, bool params) {
	if (fSymb) {
		fSymb->flags = fVal;
		if (params) {
			if (IsFunction(fSymb))
				SetSymbolFlags(fSymb->fun.overload, fVal, params);
			else if (fSymb->kind == TYPEDEF_S && GetCategory(fSymb->type) == TYPE_CATEGORY_STRUCT)
				SetSymbolFlags(fSymb->type->str.members->symbols, fVal, params);
		}
		SetSymbolFlags(fSymb->left, fVal, params);
		SetSymbolFlags(fSymb->right, fVal, params);
	}
}

// SetSymbolFlagsList() - Walk a list of scopes and set the flags field for all symbols.
void SetSymbolFlagsList(Scope *fScope, int fVal) {
	while (fScope) {
		SetSymbolFlags(fScope->symbols, fVal);
		fScope = fScope->next;
	}
}

//-----------------------------------------------------------------------------
//	Various Utility Functions
//-----------------------------------------------------------------------------

// Create an atom with the given root string and a numbered suffix.
int GetNumberedAtom(CG *cg, const char *root, int number, int digits, char ch) {
	char str[256];
	
	strcpy(str, root);
	size_t	len = strlen(str);
	if (ch != '\0') {
		str[len] = ch;
		len++;
	}
	char	*s = &str[len + digits];
	*s = '\0';
	while (digits-- > 0) {
		*--s = '0' + number % 10;
		number /= 10;
	}
	return cg->AddAtom(str);
}

// Return a string representation of the variable described by fExpr.
int GetVarExprName(CG *cg, char *str, int size, expr *fExpr) {
	int len = 0;

	*str = '\0';
	switch (fExpr->kind) {
	case SYMB_N:
		switch (fExpr->op) {
		case VARIABLE_OP:
			case MEMBER_OP: {
				const char	*name = cg->GetAtomString(fExpr->sym.symbol->name);
				size_t		len2 = strlen(name);
				if (len2 >= size)
					len2 = size - 1;
				len += len2;
				while (len2--)
					*str++ = *name++;
				*str = '\0';
				break;
			}
		}
		break;
	case BINARY_N:
		// Put struct.member code here:
		break;
	}
	return len;
}

// Create an atom with the given root string representing the variable by fExpr and a numbered suffix for the index.
int GenerateIndexName(CG *cg, expr *fExpr, int index) {
	char str[256];
	GetVarExprName(cg, str, 256, fExpr);
	return GetNumberedAtom(cg, str, index, 1, '$');
}

// Concatenate two lists of statements.
stmt *ConcatStmts(stmt *first, stmt *last) {
	if (first) {
		if (last) {
			stmt *lStmt;
			for (lStmt = first; lStmt->next; lStmt = lStmt->next);
			lStmt->next = last;
		}
		return first;
	} else {
		return last;
	}
}

// Append a list of statements to the end of another list.
void AppendStatements(StmtList *fStatements, stmt *fStmt) {
	if (fStmt) {
		if (fStatements->first) {
			fStatements->last->next = fStmt;
		} else {
			fStatements->first = fStmt;
		}
		while (fStmt->next)
			fStmt = fStmt->next;
		fStatements->last = fStmt;
	}
}

//-----------------------------------------------------------------------------
//	Expression Functions
//-----------------------------------------------------------------------------

// Create a node that references a symbol.
expr *GenSymb(CG *cg, Symbol *fSymb) {
	return NewSymbNode(cg, VARIABLE_OP, fSymb);
}

// Create a node that references a member of a struct or connector.
expr *GenMember(CG *cg, Symbol *fSymb) {
	return NewSymbNode(cg, MEMBER_OP, fSymb);
}
// Create a node that references a member of a struct or connector.
expr *GenMemberSelector(CG *cg, expr *sExpr, expr *mExpr) {
	expr *lExpr =  NewBinopNode(cg, MEMBER_SELECTOR_OP, sExpr, mExpr);
	lExpr->type = mExpr->type;
	return lExpr;
}

// Build an expression to reference a member of a struct.
expr *GenMemberReference(CG *cg, expr *sExpr, Symbol *mSymb) {
	// sExpr =  NewSymbNode(cg, VARIABLE_OP, sSymb);
	expr *mExpr = NewSymbNode(cg, MEMBER_OP, mSymb);
	expr *lExpr = NewBinopNode(cg, MEMBER_SELECTOR_OP, sExpr, mExpr);
	lExpr->type = mExpr->type;
	lExpr->is_lvalue = 1;
	lExpr->is_const = (GetQualifiers(lExpr->type) & TYPE_QUALIFIER_CONST) != 0;
	return lExpr;
}

// Create a vector index node.
expr *GenVecIndex(CG *cg, expr *vexpr, expr *xexpr, int base, int len) {
	expr *lExpr =  NewBinopNode(cg, ARRAY_INDEX_OP, vexpr, xexpr);
	lExpr->type = GetStandardType(cg->hal, base, 0, 0);
	return lExpr;
}

// Create a matrix index node.
expr *GenMatIndex(CG *cg, expr *mExpr, expr *xexpr, int base, int len) {
	expr *lExpr =  NewBinopNode(cg, ARRAY_INDEX_OP, mExpr, xexpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Create a array index node.
expr *GenArrayIndex(CG *cg, expr *mExpr, expr *xexpr, int base, int len) {
	expr *lExpr =  NewBinopNode(cg, ARRAY_INDEX_OP, mExpr, xexpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Create a Boolean constant node.
expr *GenBoolConst(CG *cg, int fval) {
	return  NewBConstNode(cg, BCONST_OP, fval, TYPE_BASE_BOOLEAN);
}

// Create an integer constant node.
expr *GenIntConst(CG *cg, int fval) {
	return  NewIConstNode(cg, ICONST_OP, fval, TYPE_BASE_INT);
}

// Create a vector floating point constant node.
expr *GenFConstV(CG *cg, float *fval, int len, int base) {
	return  NewFConstNodeV(cg, FCONST_V_OP, fval, len, base);
}

// Create a Boolean NOT.
expr *GenBoolNot(CG *cg, expr *fExpr) {
	expr *lExpr =  NewUnopSubNode(cg, BNOT_OP, SUBOP__(TYPE_BASE_BOOLEAN), fExpr);
	lExpr->type = BooleanType;
	return lExpr;
}

// Create a Boolean assignment.
expr *GenBoolAssign(CG *cg, expr *fVar, expr *fExpr) {
	expr *lExpr =  NewBinopSubNode(cg, ASSIGN_OP, SUBOP__(TYPE_BASE_BOOLEAN), fVar, fExpr);
	lExpr->type = BooleanType;
	return lExpr;
}

// Create a scalar assignment.
expr *GenSAssign(CG *cg, expr *fVar, expr *fExpr, int base) {
	expr *lExpr =  NewBinopSubNode(cg, ASSIGN_OP, SUBOP_S(base), fVar, fExpr);
	lExpr->type = GetStandardType(cg->hal, base, 0, 0);
	return lExpr;
}

// Create a vector assignment.
expr *GenVAssign(CG *cg, expr *fVar, expr *fExpr, int base, int len) {
	expr *lExpr =  NewBinopSubNode(cg, ASSIGN_V_OP, SUBOP_V(len, base), fVar, fExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Create a matrix assignment.
expr *GenMAssign(CG *cg, expr *fVar, expr *fExpr, int base, int len, int len2) {
	expr *lExpr =  NewBinopNode(cg, ASSIGN_GEN_OP, fVar, fExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, len2);
	return lExpr;
}

// Create a scalar conditional assignment.
expr *GenCondSAssign(CG *cg, expr *fVar, expr *fCond, expr *fExpr, int base) {
	expr *lExpr =  NewTriopSubNode(cg, ASSIGN_COND_OP, SUBOP_S(base), fVar, fCond, fExpr);
	lExpr->type = GetStandardType(cg->hal, base, 0, 0);
	return lExpr;
}

// Create a vector conditional assignment with a scalar Boolean argument.
expr *GenCondSVAssign(CG *cg, expr *fVar, expr *fCond, expr *fExpr, int base, int len) {
	expr *lExpr =  NewTriopSubNode(cg, ASSIGN_COND_SV_OP, SUBOP_SV(len, base), fVar, fCond, fExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Create a vector conditional assignment with a vector Boolean argument.
expr *GenCondVAssign(CG *cg, expr *fVar, expr *fCond, expr *fExpr, int base, int len) {
	expr *lExpr =  NewTriopSubNode(cg, ASSIGN_COND_V_OP, SUBOP_V(len, base), fVar, fCond, fExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}
// Create a geeral conditional assignment.
expr *GenCondGenAssign(CG *cg, expr *fVar, expr *fCond, expr *fExpr) {
	expr *lExpr =  NewTriopSubNode(cg, ASSIGN_COND_GEN_OP, 0, fVar, fCond, fExpr);
	lExpr->type = fVar->type;
	return lExpr;
}

// Create a Boolean &&.
expr *GenBoolAnd(CG *cg, expr *aExpr, expr *bExpr) {
	expr *lExpr =  NewBinopSubNode(cg, BAND_OP, SUBOP__(TYPE_BASE_BOOLEAN), aExpr, bExpr);
	lExpr->type = BooleanType;
	return lExpr;
}

// Create a Boolean Vector &&.
expr *GenBoolAndVec(CG *cg, expr *aExpr, expr *bExpr, int len) {
	expr *lExpr =  NewBinopSubNode(cg, BAND_V_OP, SUBOP_V(len, TYPE_BASE_BOOLEAN), aExpr, bExpr);
	lExpr->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, len, 0);
	return lExpr;
}

// Smear a Boolean Vector @xxxx.
expr *GenBoolSmear(CG *cg, expr *fExpr, int len) {
	int		mask	= (0 << 6) | (0 << 4) | (0 << 2) | 0;
	int		lsubop	= SUBOP_Z(mask, len, 0, TYPE_BASE_BOOLEAN);
	expr	*lExpr	=  NewUnopSubNode(cg, SWIZZLE_Z_OP, lsubop, fExpr);
	lExpr->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, len, 0);
	return lExpr;
}

// Add a node to an expression list.
expr *GenExprList(CG *cg, expr *fExpr, expr *nExpr, Type *ftype) {
	expr *lExpr =  NewBinopNode(cg, EXPR_LIST_OP, nExpr, NULL);
	lExpr->type = ftype;
	if (fExpr) {
		expr *tExpr = fExpr;
		while (tExpr->bin.right)
			tExpr = tExpr->bin.right;
		tExpr->bin.right = lExpr;
	} else {
		fExpr = lExpr;
	}
	return fExpr;
}

// Create a Vector Multiply
expr *GenVecMult(CG *cg, expr *aExpr, expr *bExpr, int base, int len) {
	expr *lExpr =  NewBinopSubNode(cg, MUL_V_OP, SUBOP_V(len, base), aExpr, bExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Create a smeared Scalar-Vector Multiply
expr *GenScaleVecMult(CG *cg, expr *aExpr, expr *bExpr, int base, int len) {
	expr *lExpr =  NewBinopSubNode(cg, MUL_VS_OP, SUBOP_V(len, base), aExpr, bExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Create a Vector Addition
expr *GenVecAdd(CG *cg, expr *aExpr, expr *bExpr, int base, int len) {
	expr *lExpr =  NewBinopSubNode(cg, ADD_V_OP, SUBOP_V(len, base), aExpr, bExpr);
	lExpr->type = GetStandardType(cg->hal, base, len, 0);
	return lExpr;
}

// Convert a vector to another length, or to a scalar if newlen is 1.
// Return original expression if no conversion needed.
expr *GenConvertVectorLength(CG *cg, expr *fExpr, int base, int len, int newlen) {
	expr *lExpr;

	if (newlen != len) {
		int	mask = 0;
		for (int ii = 0; ii < newlen; ii++) {
			if (ii < len) {
				mask |= ii << (ii*2);
			} else {
				mask |= len - 1; // Use last component if source shorter than dest
			}
		}
		if (newlen == 1)
			newlen = 0; // I.e. scalar, not array[1]
		lExpr =  NewUnopSubNode(cg, SWIZZLE_Z_OP, SUBOP_Z(mask, newlen, len, base), fExpr);
		lExpr->type = GetStandardType(cg->hal, base, newlen, 0);
	} else {
		lExpr = fExpr;
	}
	return lExpr;
}

//-----------------------------------------------------------------------------
//	Duplicate Functions
//-----------------------------------------------------------------------------

struct Duplicator {
	expr	*pre_expr(CG *cg, expr *fExpr, int arg2);
};

// Duplicate a node.
expr *Duplicator::pre_expr(CG *cg, expr *fExpr, int arg2) {
	return fExpr ? DupNode(cg->pool(), fExpr) : 0;
}

// Duplicate an expression tree.
expr *DupExpr(CG *cg, expr *fExpr) {
	return cg->ApplyToNodes(Duplicator(), fExpr);
}

stmt *DuplicateStatementTree(CG *cg, stmt *fStmt) {
	stmt *lStmt, *mStmt, *nStmt, *pStmt, *headStmt, *lastStmt;
	expr *lExpr;

	headStmt = NULL;
	while (fStmt) {
		switch (fStmt->kind) {
			case EXPR_STMT:
				lExpr = DupExpr(cg, fStmt->exprst.exp);
				lStmt = NewExprStmt(fStmt->loc, lExpr);
				break;
			case IF_STMT:
				lExpr = DupExpr(cg, fStmt->ifst.cond);
				mStmt = DuplicateStatementTree(cg, fStmt->ifst.thenstmt);
				nStmt = DuplicateStatementTree(cg, fStmt->ifst.elsestmt);
				lStmt = NewIfStmt(fStmt->loc, lExpr, mStmt, nStmt);
				break;
			case WHILE_STMT:
			case DO_STMT:
				lExpr = DupExpr(cg, fStmt->whilest.cond);
				mStmt = DuplicateStatementTree(cg, fStmt->whilest.body);
				lStmt = NewWhileStmt(fStmt->loc, fStmt->kind, lExpr, mStmt);
				break;
			case FOR_STMT:
				mStmt = DuplicateStatementTree(cg, fStmt->forst.init);
				lExpr = DupExpr(cg, fStmt->forst.cond);
				nStmt = DuplicateStatementTree(cg, fStmt->forst.step);
				pStmt = DuplicateStatementTree(cg, fStmt->forst.body);
				lStmt = NewForStmt(fStmt->loc, mStmt, lExpr, nStmt, pStmt);
				break;
			case BLOCK_STMT:
				mStmt = DuplicateStatementTree(cg, fStmt->blockst.body);
				lStmt = NewBlockStmt(fStmt->loc, mStmt, fStmt->blockst.scope);
				break;
			case RETURN_STMT:
				lExpr = DupExpr(cg, fStmt->exprst.exp);
				lStmt = NewReturnStmt(cg, fStmt->loc, NULL, lExpr);
				break;
			case DISCARD_STMT:
				lExpr = DupExpr(cg, fStmt->discardst.cond);
				lStmt = NewDiscardStmt(fStmt->loc, lExpr);
				break;
			case BREAK_STMT:
				lStmt = NewBreakStmt(fStmt->loc);
				break;
			case COMMENT_STMT:
				lStmt = NewCommentStmt(cg, fStmt->loc, cg->GetAtomString(fStmt->commentst.str));
				break;
			default:
				lStmt = fStmt;
				assert(!"DuplicateStatementTree() - not yet finished");
				break;
		}
		if (headStmt) {
			lastStmt->next = lStmt;
		} else {
			headStmt = lStmt;
		}
		lastStmt = lStmt;
		fStmt = fStmt->next;
	}
	return headStmt;
}

//-----------------------------------------------------------------------------
//	Named Constant Substitution
//-----------------------------------------------------------------------------
// Replace references to names constants with a const node.

struct ConvertNamedConstants {
	expr	*post_expr(CG *cg, expr *fExpr, int arg2) {
		if (fExpr) {
			switch (fExpr->kind) {
				case SYMB_N: {
					// The only named constants are "true" and "false":
					Symbol	*lSymb = fExpr->sym.symbol;
					Type	*lType = lSymb->type;
					if (lSymb->kind == CONSTANT_S) {
						if (IsScalar(lType) && GetBase(lType) == TYPE_BASE_BOOLEAN)
							fExpr = NewBConstNode(cg, BCONST_OP, lSymb->con.value, TYPE_BASE_BOOLEAN);
						else if (IsEnum(lType))
							fExpr = NewIConstNode(cg, ICONST_OP, lSymb->con.value, TYPE_BASE_INT);
					}
					break;
				}
				case CONST_N:
				case UNARY_N:
				case BINARY_N:
				case TRINARY_N:
					break;
				default:
					FatalError(cg, "bad kind to ConvertNamedConstantsExpr()");
					break;
			}
		}
		return fExpr;
	}
};

//-----------------------------------------------------------------------------
//	Remove Empty Statements
//-----------------------------------------------------------------------------

// Expand inline function calls in a statement.
struct RemoveEmptyStatements {
	stmt	*post_stmt(CG *cg, stmt *fStmt, int arg2) {
		return fStmt->kind == EXPR_STMT && !fStmt->exprst.exp ? NULL : fStmt;
	};
};

struct DebugFunctionData {
	Symbol *color;
	Symbol *flag;
	Symbol *outConnector;
	Symbol *COLSymb;
	stmt	*post_stmt(CG *cg, stmt *fStmt, int arg2);
};

// Expand inline function calls in a statement.
stmt *DebugFunctionData::post_stmt(CG *cg, stmt *fStmt, int arg2) {
	expr *eExpr, *lExpr, *mExpr, *bExpr, *rExpr;
	stmt *lStmt, *mStmt;
	int	group, index;

	if (fStmt->kind == EXPR_STMT) {
		// Look for a call to "debug(float4)":

		eExpr = fStmt->exprst.exp;
#define BUILTIN_GROUP_NV30FP_DBG     0
		if (IsBuiltin(eExpr, &group, &index) && group == BUILTIN_GROUP_NV30FP_DBG && index == 0x444) {
			if (arg2) {
				// Turn: "debug(arg);" statements into:
				//
				//     $debug-color@@(!$debug-set) = arg;
				//     $debug-set = true;
				rExpr = eExpr->bin.right->bin.left;
				mExpr = GenSymb(cg, flag);
				mExpr = GenBoolNot(cg, mExpr);
				lExpr = GenSymb(cg, color);
				lExpr = GenCondSVAssign(cg, lExpr, mExpr, rExpr, TYPE_BASE_FLOAT, 4);
				lStmt = NewExprStmt(fStmt->loc, lExpr);
				lExpr = GenSymb(cg, flag);
				rExpr = GenBoolConst(cg, 1);
				lExpr = GenBoolAssign(cg, lExpr, rExpr);
				mStmt = NewExprStmt(fStmt->loc, lExpr);
				lStmt->next = mStmt;
				fStmt = lStmt;
			} else {
				// Eliminate: "debug(arg);" statements:
				fStmt = NULL;
			}
		}
	} else if (arg2 && fStmt->kind == RETURN_STMT) {
		rExpr = GenSymb(cg, color);
		mExpr = GenSymb(cg, flag);
		lExpr = GenSymb(cg, outConnector);
		bExpr = GenMember(cg, COLSymb);
		lExpr = GenMemberSelector(cg, lExpr, bExpr);
		lExpr = GenCondSVAssign(cg, lExpr, mExpr, rExpr, TYPE_BASE_FLOAT, 4);
		lStmt = NewExprStmt(fStmt->loc, lExpr);
		lStmt->next = fStmt;
		fStmt = lStmt;
	}
	return fStmt;
}

// Convert calls to debug to either assignments to global variables, or delete them, depending on the value of DebugFlag.
// Also defines global variables.
stmt *ConvertDebugCalls(CG *cg, SourceLoc &loc, Scope *fScope, stmt *fStmt, bool DebugFlag) {
	Symbol				*debugColor, *debugSet;
	DebugFunctionData	debugData;
	expr				*lExpr, *rExpr;
	stmt				*lStmt;
	Type				*lType;
	int					vname, COLname;
	float				fdata[4];

	stmt	*rStmt = fStmt;
	if (DebugFlag) {
		StmtList		lStatements;
		//outputSymb = fScope->outConnector;

		// 1) Define global vars used by "-debug" mode:
		//
		//     float $debug-color[4];
		//     bool  $debug-set = 0.0f;

		vname = cg->AddAtom("$debug-color");
		lType = GetStandardType(cg->hal, TYPE_BASE_FLOAT, 4, 0);
		debugColor = DefineVar(cg, loc, fScope, vname, lType);
		vname = cg->AddAtom("$debug-set");
		debugSet = DefineVar(cg, loc, fScope, vname, BooleanType);
		lExpr = GenSymb(cg, debugSet);
		rExpr = GenBoolConst(cg, 0);
		lExpr = GenBoolAssign(cg, lExpr, rExpr);
		lStmt = NewExprStmt(loc, lExpr);
		AppendStatements(&lStatements, lStmt);

		// 1A) Must initialize $debug-color to something or else the code generator
		//     gets all bent out of shape:
		//
		//     $debug-color = { 0.0f, 0.0f, 0.0f, .0f };

		fdata[0] = fdata[1] = fdata[2] = fdata[3] = 0.0f;
		rExpr = GenFConstV(cg, fdata, 4, TYPE_BASE_FLOAT);
		lExpr = GenSymb(cg, debugColor);
		lExpr = GenVAssign(cg, lExpr, rExpr, TYPE_BASE_FLOAT, 4);
		lStmt = NewExprStmt(loc, lExpr);
		AppendStatements(&lStatements, lStmt);

		// 2) Expand calls to "debug(float4);" into the following code:
		//
		//     $debug-color@@(!$debug-set) = float4;
		//     $debug-set = true;

		debugData.color = debugColor;
		debugData.flag = debugSet;
		debugData.outConnector = cg->varyingOut;
		assert(debugData.outConnector);
		COLname = cg->AddAtom("COL");
		lType = debugData.outConnector->type;
		debugData.COLSymb = LookUpLocalSymbol(cg, lType->str.members, COLname);

		// 3) And add the following statement to the end of the program (i.e. before each
		//    "return" statement):
		//
		//     output.COL@@($debug-set) = $debug-color;

		lStmt = cg->ApplyToStatements(debugData, fStmt, 1);
		AppendStatements(&lStatements, lStmt);

		rStmt = lStatements.first;
	} else {
		rStmt = cg->ApplyToStatements(debugData, fStmt, 0);//PostApplyToStatements(ConvertDebugCallsStmt, fStmt, NULL, 0);
	}
	return rStmt;
}

//-----------------------------------------------------------------------------
//	FlattenChainedAssignments
//-----------------------------------------------------------------------------
// Transform chained assignments into multiple simple assignments.
//
//  float3 A, C;  half B;
//
//  Simle case:
//
//  A = B = C;    (which is equivanemt to:)     A = (float) ( B = (half) C ) ;
//
//      =                        =               =
//    /   \                    /   \           /   \
//  A      (f)     >>-->>     B     (h)      A      (f)
//            \                        \               \
//              =                        C               B'
//            /   \
//          B      (h)
//                    \
//                      C
//
//  where B' is a duplicate of B.
//
//  If B contains any function calls, they must be hoisted prior to this step.

bool IsAssignSVOp(expr *fExpr) {
	if (fExpr && fExpr->kind == BINARY_N) {
		int lop = fExpr->op;
		return lop == ASSIGN_OP || lop == ASSIGN_V_OP || lop == ASSIGN_GEN_OP || lop == ASSIGN_MASKED_KV_OP;
	}
	return false;
}

bool IsAssignCondSVOp(expr *fExpr) {
	if (fExpr && fExpr->kind == TRINARY_N) {
		int lop = fExpr->op;
		return lop == ASSIGN_COND_OP || lop == ASSIGN_COND_V_OP || lop == ASSIGN_COND_SV_OP || lop == ASSIGN_COND_GEN_OP;
	}
	return false;
}

bool IsCastOp(expr *fExpr) {
	if (fExpr && fExpr->kind == UNARY_N) {
		int	lop = fExpr->op;
		return lop == CAST_CS_OP || lop == CAST_CV_OP || lop == CAST_CM_OP;
	}
	return false;
}

// Return a symbol expression for a temp symbol that can hold the value of fExpr.
expr *NewTmp(CG *cg, const expr *fExpr) {
	Type *tmpType = DupType(fExpr->type);
	tmpType->properties &= ~(TYPE_DOMAIN_MASK | TYPE_QUALIFIER_MASK);
	Symbol *tmpSym = UniqueSymbol(cg, cg->current_scope, tmpType, VARIABLE_S);
	return NewSymbNode(cg, VARIABLE_OP, tmpSym);
}


struct FlattenChainedAssignments {
	stmt	*post_stmt(CG *cg, stmt *fStmt, int arg2) {
		stmt *bStmt, *rStmt;
		expr *assigna, *assignb;
		expr *pb, **ppassignb;

		if (fStmt->kind == EXPR_STMT) {
			rStmt = fStmt;
			assigna = fStmt->exprst.exp;
			while (1) {
				if (IsAssignSVOp(assigna)) {
					ppassignb = &assigna->bin.right;
				} else if (IsAssignCondSVOp(assigna)) {
					ppassignb = &assigna->tri.arg3;
				} else {
					break;
				}
				// Traverse list of type casts, if any:
				while (IsCastOp(*ppassignb))
					ppassignb = &((**ppassignb).un.arg);
				if (IsAssignSVOp(*ppassignb)) {
					pb = (**ppassignb).bin.left;
				} else if (IsAssignCondSVOp(*ppassignb)) {
					pb = (**ppassignb).tri.arg1;
				} else {
					break;
				}
				assignb = *ppassignb;
				bStmt = NewExprStmt(fStmt->loc, assignb);
				*ppassignb = cg->ApplyToNodes(Duplicator(), pb);
				bStmt->next = rStmt;
				rStmt = bStmt;
				assigna = assignb;
			}
		} else {
			rStmt = fStmt;
		}
		return rStmt;
	}
};

// Put all array indicies in temps and builds a comma list of assignments of the indices to the temporaries.
// A[e1]...[eN] -> t1 = e1, ..., tN = eN : A[t1]...[tN]
expr *PutIndexEpxrsInTemps(CG *cg, expr *fExpr) {
	expr *assignments = NULL;
	expr *assign;
	expr *tmp;

	assert(IsArrayIndex(fExpr));
	if (IsArrayIndex(fExpr->bin.left))
		assignments = PutIndexEpxrsInTemps(cg, fExpr->bin.left);

	tmp =  NewTmp(cg, fExpr->bin.right);
	assign = NewSimpleAssignment(cg, cg->lastSourceLoc, DupNode(cg->pool(), tmp), fExpr->bin.right, 0);

	if (!assignments)
		assignments = assign;
	else
		assignments =  NewBinopNode(cg, COMMA_OP, assignments, assign);

	if (IsArrayIndex(fExpr->bin.right))
		assignments =  NewBinopNode(cg, COMMA_OP, assignments,
		PutIndexEpxrsInTemps(cg, fExpr->bin.right));

	fExpr->bin.right = tmp;
	return assignments;
}

//-----------------------------------------------------------------------------
//	Expand Increment/Decrement Expressions
//-----------------------------------------------------------------------------
//Expand increment/decrement operations.
//
//Pre increment/decrement is the simple case:
//
//  ++A -> A += 1
//
//Post increment/decrement is a little more tricky:
//
//  If A is simple (i.e. not an array reference):
//
//    A++ -> tmp = A, A += 1, tmp
//
//  If A is an array reference:
//
//    A[i1]...[iN]++ ->
//      tmpi1 = i1, ..., tmpiN = iN, tmpv = A[tmpi1]...[tmpiN],
//      A[tmpi1]...[tmpiN] += 1, tmpv
//
struct ExpandIncDec {
	expr *post_expr(CG *cg, expr *fExpr, int arg2) {
		int pre = 0;
		opcode newop = UNKNOWN_OP;
		expr *oneExpr;
		expr *result = NULL;

		if (fExpr->kind == UNARY_N) {
			switch (fExpr->op) {
				case PREDEC_OP:
					pre = 1;
				case POSTDEC_OP:
					newop = ASSIGNMINUS_OP;
					break;
				case PREINC_OP:
					pre = 1;
				case POSTINC_OP:
					newop = ASSIGNPLUS_OP;
					break;
			}
		}

		if (newop == UNKNOWN_OP)
			return fExpr;

		oneExpr =  NewIConstNode(cg, ICONST_OP, 1, TYPE_BASE_INT);

		if (pre)
			result =  NewBinopNode(cg, newop, fExpr->un.arg, oneExpr);
		else {
			expr *idxAssigns = IsArrayIndex(fExpr->un.arg) ? PutIndexEpxrsInTemps(cg, fExpr->un.arg) : NULL;
			expr *tmp =  NewTmp(cg, fExpr->un.arg);
			expr *tmpAssign = NewSimpleAssignment(cg, cg->lastSourceLoc, tmp, fExpr->un.arg, 0);
			expr *incdec =  NewBinopNode(cg, newop, DupNode(cg->pool(), fExpr->un.arg), oneExpr);
			expr *rval = DupNode(cg->pool(), tmp);
			result =  NewBinopNode(cg, COMMA_OP, incdec, rval);
			result =  NewBinopNode(cg, COMMA_OP, tmpAssign, result);
			if (idxAssigns)
				result =  NewBinopNode(cg, COMMA_OP, idxAssigns, result);
		}

		return result;

	}
};

//-----------------------------------------------------------------------------
//	Expand Compound Assignment Expressions
//-----------------------------------------------------------------------------
// Transform compound assignments into simple assignments.
// 
// If A is simple (i.e. not an array reference):
// 
//   A op= B -> A = A op B
// 
// If A is an array reference:
// 
//   A[e1]...[eN] op= B ->
//     tmp1 = e1, ..., tmpN = eN, A[tmp1]...[tmpN] = A[tmp1]...[tmpN] op B
// 
struct ExpandCompoundAssignment {
	expr *post_expr(CG *cg, expr *fExpr, int arg2) {
		opcode newop = UNKNOWN_OP;
		int opname = 0;
		int intOnly = 0;
		expr *lexpr, *rExpr;
		expr *result = NULL;
		expr *idxAssigns = NULL;

		if (fExpr->kind == BINARY_N) {
			switch (fExpr->op) {
				case ASSIGNMINUS_OP:
					newop = SUB_OP;
					opname = '-';
					break;
				case ASSIGNMOD_OP:
					newop = MOD_OP;
					opname = '%';
					intOnly = 1;
					break;
				case ASSIGNPLUS_OP:
					newop = ADD_OP;
					opname = '+';
					break;
				case ASSIGNSLASH_OP:
					newop = DIV_OP;
					opname = '/';
					break;
				case ASSIGNSTAR_OP:
					newop = MUL_OP;
					opname = '*';
					break;
				case ASSIGNAND_OP:
					newop = AND_OP;
					opname = '&';
					break;
				case ASSIGNOR_OP:
					newop = OR_OP;
					opname = '|';
					break;
				case ASSIGNXOR_OP:
					newop = XOR_OP;
					opname = '^';
					break;
			}
		}

		if (newop == UNKNOWN_OP)
			return fExpr;

		lexpr = fExpr->bin.left;
		rExpr = fExpr->bin.right;

		if (IsArrayIndex(lexpr))
			idxAssigns = PutIndexEpxrsInTemps(cg, lexpr);

		result = NewBinaryOperator(cg, cg->lastSourceLoc, newop, opname, lexpr, rExpr, intOnly);
		result = NewSimpleAssignment(cg, cg->lastSourceLoc, DupNode(cg->pool(), lexpr), result, 0);
		if (idxAssigns)
			result =  NewBinopNode(cg, COMMA_OP, idxAssigns, result);

		return result;
	}
};

//-----------------------------------------------------------------------------
//	Flatten Chained Assignment Statements
//-----------------------------------------------------------------------------

bool IsConstant(expr *fExpr) {
	return fExpr && fExpr->kind == CONST_N;
}
bool IsVariable(expr *fExpr) {
	return fExpr && fExpr->kind == SYMB_N;
}
bool IsBuiltin(expr *fExpr, int *group, int *index) {
	if (fExpr && fExpr->kind == BINARY_N && fExpr->op == FUN_BUILTIN_OP) {
		expr *left = fExpr->bin.left;
		if (left->kind == SYMB_N) {
			Symbol	*s = left->sym.symbol;
			*group = s->fun.group;
			*index = s->fun.index;
		} else {
			*group = *index = 0;
		}
		return true;
	}
	return false;
}

bool IsCompileTimeAddress(expr *fExpr) {
	if (fExpr->is_lvalue) {
		switch (fExpr->kind) {
			case SYMB_N:
				return true;
			case BINARY_N:
				switch (fExpr->op) {
					case MEMBER_SELECTOR_OP:
						return IsCompileTimeAddress(fExpr->bin.left);
					case ARRAY_INDEX_OP:
						return IsCompileTimeAddress(fExpr->bin.left) && IsConstant(fExpr->bin.right);
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
	return false;
}

int GetConstIndex(expr *fExpr) {
	if (fExpr->kind == CONST_N) {
		switch (fExpr->op) {
			case ICONST_OP:
			case UCONST_OP:
			case BCONST_OP:
				return fExpr->co.val[0].i;
			case FCONST_OP:
			case HCONST_OP:
			case XCONST_OP:
				return (int) fExpr->co.val[0].f;
			default:
				break;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	Flatten comma expressions into statement lists
//-----------------------------------------------------------------------------

// Gets rid of all top-level comma expressions by pulling them out into an expression list (returned through arg1).
// op (a, b) -> a, (op b)
// (a, b) op c -> a, (b op c)
// A[e1]...[eN] = (b, c) -> (tmp1 = e1), ..., (tmpN = eN), b, A[tmp1]...[tmpN] = c
// a = (b, c) -> b, a = c
// a op (b, c) -> (tmpa = a), b, (tmpa op c)
// (a, b) ? c : d -> a, (b ? c : d)
// a ? (b, c) : d -> (tmpa = a), b, (tmpa ? c : d)
// a ? b : (c, d) -> (tmpa = a), (tmpb = b), c, (tmpa ? tmpb : d)

struct FlattenCommas {
	static bool IsComma(const expr *fExpr) {
		return fExpr && fExpr->kind == BINARY_N && fExpr->op == COMMA_OP;
	}

	struct LinearizeCommas {
		stmt *&fStmts;

		expr *pre_expr(CG *cg, expr *fExpr, int arg2) {
			while (IsComma(fExpr)) {
				stmt * fStmt = NewExprStmt(cg->lastSourceLoc, fExpr->bin.left);
				fStmts = ConcatStmts(fStmts, fStmt);
				fExpr = fExpr->bin.right;
			}
			return fExpr;
		}
		LinearizeCommas(stmt *&_fStmts) : fStmts(_fStmts) {}
	};

	expr *pre_expr(CG *cg, expr *fExpr, int arg2) {
		switch (fExpr->kind) {
			case UNARY_N:
				if (IsComma(fExpr->un.arg)) {
					expr *comma = fExpr->un.arg;
					fExpr->un.arg = comma->bin.right;
					fExpr =  NewBinopNode(cg, COMMA_OP, comma->bin.left, fExpr);
				}
				break;
			case BINARY_N: {
				if (IsComma(fExpr->bin.left)) {
					expr *comma = fExpr->bin.left;
					fExpr->bin.left = comma->bin.right;
					fExpr =  NewBinopNode(cg, COMMA_OP, comma->bin.left, fExpr);
				}
				if (fExpr->bin.right && IsComma(fExpr->bin.right)) {
					// Assignments/lvalues are special
					if (fExpr->op == ASSIGN_OP || fExpr->op == ASSIGN_V_OP) {
						expr *idxAssigns = IsArrayIndex(fExpr->bin.left) ? PutIndexEpxrsInTemps(cg, fExpr->bin.left) : NULL;
						fExpr =  NewBinopNode(cg, COMMA_OP, fExpr->bin.right->bin.left, fExpr);
						fExpr->bin.right = fExpr->bin.right->bin.right;
						if (idxAssigns)
							fExpr =  NewBinopNode(cg, COMMA_OP, idxAssigns, fExpr);
					} else if (fExpr->op != COMMA_OP) {
						// no need to lift comma ops above a comma op
						expr *comma = fExpr->bin.right;
						expr *tmp =  NewTmp(cg, fExpr->bin.left);
						expr *assign = NewSimpleAssignment(cg, cg->lastSourceLoc, tmp, fExpr->bin.left, 0);
						fExpr->bin.left = DupNode(cg->pool(), tmp);
						fExpr->bin.right = comma->bin.right;
						fExpr =  NewBinopNode(cg, COMMA_OP, comma->bin.left, fExpr);
						fExpr =  NewBinopNode(cg, COMMA_OP, assign, fExpr);
					}
				}
				break;
			}
			case TRINARY_N: {
				if (IsComma(fExpr->tri.arg1)) {
					expr *comma = fExpr->tri.arg1;
					fExpr->tri.arg1 = comma->bin.right;
					fExpr =  NewBinopNode(cg, COMMA_OP, comma->bin.left, fExpr);
				}
				if (IsComma(fExpr->tri.arg2)) {
					expr *comma = fExpr->tri.arg2;
					expr *tmp =  NewTmp(cg, fExpr->tri.arg1);
					expr *assign = NewSimpleAssignment(cg, cg->lastSourceLoc, tmp, fExpr->tri.arg1, 0);
					fExpr->tri.arg1 = DupNode(cg->pool(), tmp);
					fExpr->tri.arg2 = comma->bin.right;
					fExpr =  NewBinopNode(cg, COMMA_OP, comma->bin.left, fExpr);
					fExpr =  NewBinopNode(cg, COMMA_OP, assign, fExpr);
				}
				if (IsComma(fExpr->tri.arg3)) {
					expr *comma = fExpr->tri.arg3;
					expr *tmp1 =  NewTmp(cg, fExpr->tri.arg1);
					expr *tmp2 =  NewTmp(cg, fExpr->tri.arg2);
					expr *assign1 = NewSimpleAssignment(cg, cg->lastSourceLoc, tmp1, fExpr->tri.arg1, 0);
					expr *assign2 = NewSimpleAssignment(cg, cg->lastSourceLoc, tmp2, fExpr->tri.arg2, 0);
					fExpr->tri.arg1 = DupNode(cg->pool(), tmp1);
					fExpr->tri.arg2 = DupNode(cg->pool(), tmp2);
					fExpr->tri.arg3 = comma->bin.right;
					fExpr =  NewBinopNode(cg, COMMA_OP, comma->bin.left, fExpr);
					fExpr =  NewBinopNode(cg, COMMA_OP, assign2, fExpr);
					fExpr =  NewBinopNode(cg, COMMA_OP, assign1, fExpr);
				}
				break;
			}
		}

		return fExpr;
	}
	stmt	*post_stmt(CG *cg, stmt *fStmt, int arg2) {
		stmt *preCommaStmts = NULL;
		cg->ApplyToExpressionsLocal(*this, fStmt);
		cg->ApplyToExpressionsLocal(LinearizeCommas(preCommaStmts), fStmt);
		return ConcatStmts(preCommaStmts, fStmt);
	}
};

//-----------------------------------------------------------------------------
//	Chop Matrices up into Vectors
//-----------------------------------------------------------------------------

struct DeconstructMatrices {
	Scope		*scope;
	int			numtemps;
	StmtList	statements;

	expr	*post_expr(CG *cg, expr *fExpr, int arg2) {
		if (fExpr->kind == BINARY_N && fExpr->op == ARRAY_INDEX_OP) {
			Type	*lType = fExpr->type;
			int		len, len2;
			if (IsMatrix(fExpr->bin.left->type, &len, &len2)) {
				int	base = GetBase(lType);
				if (IsConstant(fExpr->bin.right)) {
					if (IsVariable(fExpr->bin.left)) {
						int		index = GetConstIndex(fExpr->bin.right);
						int		vname = GenerateIndexName(cg, fExpr->bin.left, index);
						Symbol *lSymb = LookUpLocalSymbol(cg, scope, vname);
						if (!lSymb)
							lSymb = DefineVar(cg, lSymb->loc, scope, vname, lType);
						fExpr = GenSymb(cg, lSymb);
					} else {
						SemanticError(cg, cg->lastSourceLoc, ERROR___MATRIX_NOT_SIMPLE);
					}
				} else {
					SemanticError(cg, cg->lastSourceLoc, ERROR___ARRAY_INDEX_NOT_CONSTANT);
				}
			}
		}
		return fExpr;
	}

	stmt *pre_stmt(CG *cg, stmt *fStmt, int arg2) {
		cg->ApplyToExpressionsLocal(*this, fStmt);
		if (statements.first) {
			statements.last->next = fStmt;
			fStmt = statements.first;
		}
		return fStmt;
	}

	DeconstructMatrices(Scope *fscope) : scope(fscope), numtemps(0) {
	}
};

//-----------------------------------------------------------------------------
//	Chop Vectors up into Scalars
//-----------------------------------------------------------------------------

struct DeconstructVectors {
	Scope		*scope;
	int			numtemps;
	StmtList	statements;

	expr	*post_expr(CG *cg, expr *fExpr, int arg2) {

		if (fExpr->kind == BINARY_N && fExpr->op == ARRAY_INDEX_OP) {
			Type	*lType = fExpr->type;
			int		len;
			if (IsVector(fExpr->bin.left->type, &len)) {
				int	base = GetBase(lType);
				if (IsConstant(fExpr->bin.right)) {
					if (IsVariable(fExpr->bin.left)) {
						int		index	= GetConstIndex(fExpr->bin.right);
						int		vname	= GenerateIndexName(cg, fExpr->bin.left, index);
						Symbol	*lSymb	= LookUpLocalSymbol(cg, scope, vname);
						if (!lSymb)
							lSymb = DefineVar(cg, lSymb->loc, scope, vname, lType);
						fExpr = GenSymb(cg, lSymb);
					} else {
						SemanticError(cg, cg->lastSourceLoc, ERROR___MATRIX_NOT_SIMPLE);
					}
				} else {
					SemanticError(cg, cg->lastSourceLoc, ERROR___ARRAY_INDEX_NOT_CONSTANT);
				}
			}
		}
		return fExpr;
	}

	stmt *pre_stmt(CG *cg, stmt *fStmt, int arg2) {
		cg->ApplyToExpressionsLocal(*this, fStmt);
		if (statements.first) {
			statements.last->next = fStmt;
			fStmt = statements.first;
		}
		return fStmt;
	}

	DeconstructVectors(Scope *fscope) : scope(fscope), numtemps(0) {
	}
};

//-----------------------------------------------------------------------------
//	Flatten Struct Assignments
//-----------------------------------------------------------------------------
// Assign individual memebers of one struct to another.

static void AssignStructMembers(CG *cg, StmtList *fStatements, int fop, Type *fType, expr *varExpr, expr *valExpr, expr *condExpr, int VectorCond) {
	expr *lExpr, *rExpr;
	int base, len, len2;
	Symbol *lSymb;
	Type *lType;
	stmt *lStmt;

	if (IsStruct(fType)) {
		for (lSymb = fType->str.members->symbols; lSymb; lSymb = lSymb->next) {
			len = len2 = 0;
			lType = lSymb->type;
			if (IsScalar(lType) || IsVector(lType, &len) || IsMatrix(lType, &len, &len2)) {
				base = GetBase(lType);
				lExpr = GenMemberSelector(cg, DupExpr(cg, varExpr), GenMember(cg, lSymb));
				rExpr = GenMemberSelector(cg, DupExpr(cg, valExpr), GenMember(cg, lSymb));
				if (condExpr != NULL) {
					if (len2 > 0) {
						lExpr = GenCondGenAssign(cg, lExpr, rExpr, condExpr);
					} else if (len > 0) {
						lExpr = VectorCond ? GenCondSVAssign(cg, lExpr, rExpr, condExpr, base, len) : GenCondVAssign(cg, lExpr, rExpr, condExpr, base, len);
					} else {
						lExpr = GenCondSAssign(cg, lExpr, rExpr, condExpr, base);
					}
					lStmt = NewExprStmt(cg->lastSourceLoc, lExpr);
				} else {
					if (len2 > 0) {
						lExpr = GenMAssign(cg, lExpr, rExpr, base, len, len2);
					} else if (len > 0) {
						lExpr = GenVAssign(cg, lExpr, rExpr, base, len);
					} else {
						lExpr = GenSAssign(cg, lExpr, rExpr, base);
					}
					lStmt = NewExprStmt(cg->lastSourceLoc, lExpr);
				}
				AppendStatements(fStatements, lStmt);
			} else {
				switch (GetCategory(lType)) {
					case TYPE_CATEGORY_STRUCT:
					case TYPE_CATEGORY_ARRAY:
						lExpr = GenMemberSelector(cg, DupExpr(cg, varExpr), GenMember(cg, lSymb));
						rExpr = GenMemberSelector(cg, DupExpr(cg, valExpr), GenMember(cg, lSymb));
						AssignStructMembers(cg, fStatements, fop, lType, lExpr, rExpr, condExpr, VectorCond);
						break;
				}
			}
		}
	} else {
		len = len2 = 0;
		lType = fType->arr.eltype;
		for (int i = 0, n = fType->arr.numels; i < n; i++) {
			if (IsScalar(lType) || IsVector(lType, &len) || IsMatrix(lType, &len, &len2)) {
				base = GetBase(lType);
				lExpr = NewBinopNode(cg, ARRAY_INDEX_OP, DupExpr(cg, varExpr), GenIntConst(cg, i));
				lExpr->type = lType;

				if (valExpr->kind == UNARY_N && valExpr->op == MATRIX_M_OP) {
					int	v = i;
					rExpr = valExpr->un.arg;
					while (v--)
						rExpr = rExpr->bin.right;
					rExpr = rExpr->bin.left;
				} else {
					rExpr = NewBinopNode(cg, ARRAY_INDEX_OP, DupExpr(cg, valExpr), GenIntConst(cg, i));
					rExpr->type = lType;
				}

				if (condExpr != NULL) {
					if (len2 > 0) {
						lExpr = GenCondGenAssign(cg, lExpr, rExpr, condExpr);
					} else if (len > 0) {
						lExpr = VectorCond ? GenCondSVAssign(cg, lExpr, rExpr, condExpr, base, len) : GenCondVAssign(cg, lExpr, rExpr, condExpr, base, len);
					} else {
						lExpr = GenCondSAssign(cg, lExpr, rExpr, condExpr, base);
					}
					lStmt = NewExprStmt(cg->lastSourceLoc, lExpr);
				} else {
					if (len2 > 0) {
						lExpr = GenMAssign(cg, lExpr, rExpr, base, len, len2);
					} else if (len > 0) {
						lExpr = GenVAssign(cg, lExpr, rExpr, base, len);
					} else {
						lExpr = GenSAssign(cg, lExpr, rExpr, base);
					}
					lStmt = NewExprStmt(cg->lastSourceLoc, lExpr);
				}
				AppendStatements(fStatements, lStmt);
			} else {
				switch (GetCategory(lType)) {
					case TYPE_CATEGORY_STRUCT:
					case TYPE_CATEGORY_ARRAY:
						lExpr = NewBinopNode(cg, ARRAY_INDEX_OP, DupExpr(cg, varExpr), GenIntConst(cg, i));
						rExpr = NewBinopNode(cg, ARRAY_INDEX_OP, DupExpr(cg, valExpr), GenIntConst(cg, i));
						AssignStructMembers(cg, fStatements, fop, lType, lExpr, rExpr, condExpr, VectorCond);
						break;
					default:
						break;
				}
			}
		}
	}
}

// Convert struct assignments into multiple assignments of members.
struct FlattenStructAssignment {
	stmt *pre_stmt(CG *cg, stmt *fStmt, int flevel) {
		StmtList lStatements;
		if (fStmt) {
			switch (fStmt->kind) {
				case EXPR_STMT: {
					expr	*eExpr = fStmt->exprst.exp;
					if (IsAssignSVOp(eExpr)) {
						Type *lType = eExpr->type;
						switch(GetCategory(lType)) {
							case TYPE_CATEGORY_ARRAY: {
								int	len;
								if (IsVector(lType, &len))
									break;
							}
							case TYPE_CATEGORY_STRUCT:
								if (IsAssignCondSVOp(eExpr)) {
									AssignStructMembers(cg, &lStatements, eExpr->op, lType, eExpr->tri.arg1, eExpr->tri.arg3, eExpr->tri.arg2, !IsScalar(eExpr->tri.arg2->type));
									fStmt = lStatements.first;
								} else {
									AssignStructMembers(cg, &lStatements, eExpr->op, lType, eExpr->bin.left, eExpr->bin.right, NULL, 0);
									fStmt = lStatements.first;
								}
								break;
						}
					}
					break;
				}
				default:
					break;
			}
		}
		return fStmt;
	}
};

//-----------------------------------------------------------------------------
//	Flatten Structs
//-----------------------------------------------------------------------------

// make structures (and arrays) into multiple simple variables
static void SubBind(CG *cg, Symbol *sym, Symbol *parent, int i, int s = 1) {
	if (Binding	*bp = parent->var.bind) {
		Binding	*b = sym->var.bind;
		if (!b)
			b = sym->var.bind = DupBinding(cg->pool(), bp);
		b->regno	+= i * s;
		b->size		= s;
	}
}

struct FlattenStruct {
	Scope	*fScope;
	expr	*post_expr(CG *cg, expr *fExpr, int arg2) {
		char	buffer[256];

		switch (fExpr->kind) {
			case BINARY_N: {
				expr	*left	= fExpr->bin.left;
				expr	*right	= fExpr->bin.right;

				if (fExpr->op == MEMBER_SELECTOR_OP) {
					if (left->kind == SYMB_N && left->op == VARIABLE_OP) {
						Symbol	*str = left->sym.symbol;
						Symbol	*mem = right->sym.symbol;
						if (str != cg->varyingIn && str != cg->varyingOut) {
							sprintf(buffer, "%s-%s", cg->GetAtomString(str->name), cg->GetAtomString(mem->name));
							int		name	= cg->AddAtom(buffer);
							Symbol	*sym	= LookUpLocalSymbol(cg, fScope, name);
							if (!sym)
								sym = DefineVar(cg, str->loc, fScope, name, fExpr->type);
							SubBind(cg, sym, str, mem->var.addr);
							fExpr = NewSymbNode(cg, VARIABLE_OP, sym);
						}
					}
				} else if (fExpr->op == ARRAY_INDEX_OP && IsConstant(right)) {
					int		i = right->co.val[0].i;
					int		len, len2;
					if (IsMatrix(left->type, &len, &len2) && left->op == MATRIX_M_OP) {
						int	base = SUBOP_GET_S(fExpr->bin.subop);

						if (left->kind == UNARY_N && left->op == MATRIX_M_OP) {
							fExpr = fExpr->bin.left->un.arg;
							while (i--)
								fExpr = fExpr->bin.right;
							fExpr = fExpr->bin.left;

						} else {
							int	mask = 0x3210 + i * 0x4444;
							fExpr = NewUnopSubNode(cg, SWIZMAT_Z_OP, SUBOP_ZM(mask, len, len2, len, base), left);
						}

					} else if (left->kind == SYMB_N && left->op == VARIABLE_OP) {
						Symbol	*arr	= left->sym.symbol;
						sprintf(buffer, "%s-%i", cg->GetAtomString(arr->name), i);
						int		name	= cg->AddAtom(buffer);
						Symbol	*sym	= LookUpLocalSymbol(cg, fScope, name);

						if (!sym)
							sym = DefineVar(cg, arr->loc, fScope, name, fExpr->type);
						int	size = cg->hal->GetSizeof(arr->type->arr.eltype);
						SubBind(cg, sym, arr, i, size);
						fExpr = NewSymbNode(cg, VARIABLE_OP, sym);
					}
				}
				break;
			}
			case UNARY_N: {
				expr	*arg	= fExpr->un.arg;
				if (fExpr->op == SWIZMAT_Z_OP) {
					if (arg->kind == SYMB_N && arg->op == VARIABLE_OP) {
						Symbol	*arr	= arg->sym.symbol;
						Type	*type	= fExpr->type;
						int		base	= SUBOP_GET_T(fExpr->un.subop);
						int		mask	= SUBOP_GET_MASK16(fExpr->un.subop);
						int		len		= SUBOP_GET_SR(fExpr->un.subop);
						int		len0	= SUBOP_GET_S(fExpr->un.subop);
						int		swiz;

						if (arg->type->properties & TYPE_MISC_ROWMAJOR)
							mask = ((mask >> 2) & 0x3333) | ((mask << 2) & 0xcccc);

						if (AllSameCol(mask, len)) {
							int		i		= mask & 3;
							sprintf(buffer, "%s-%i", cg->GetAtomString(arr->name), i);
							int		name	= cg->AddAtom(buffer);
							Symbol	*sym	= LookUpLocalSymbol(cg, fScope, name);
							if (!sym)
								sym = DefineVar(cg, arr->loc, fScope, name, type);
							SubBind(cg, sym, arr, i);
							fExpr	= NewSymbNode(cg, VARIABLE_OP, sym);
							swiz	= GetColSwizzle(mask, len);
							if (len != len0 || !IsNullSwizzle(swiz, len))
								fExpr = NewUnopSubNode(cg, SWIZZLE_Z_OP, SUBOP_Z(swiz, len, 0, base), fExpr);
						} else {
							int	remain, len2;
							fExpr = NULL;
							for (remain = len; remain; remain -= len2, mask >>= len2 * 4) {
								for (len2 = remain; !AllSameCol(mask, len2); len2--);
								int		i		= mask & 3;
								sprintf(buffer, "%s-%i", cg->GetAtomString(arr->name), i);
								int		name	= cg->AddAtom(buffer);
								Symbol	*sym	= LookUpLocalSymbol(cg, fScope, name);
								if (!sym)
									sym = DefineVar(cg, arr->loc, fScope, name, type);
								SubBind(cg, sym, arr, i);
								arg		= NewSymbNode(cg, VARIABLE_OP, sym);
								swiz	= GetColSwizzle(mask, len);
								arg		= NewUnopSubNode(cg, SWIZZLE_Z_OP, SUBOP_Z(swiz, len2, 0, base), arg);
								fExpr	= GenExprList(cg, fExpr, arg, type);
							}
							fExpr = NewUnopSubNode(cg, VECTOR_V_OP, SUBOP_V(len, base), fExpr);
						}
					}
					break;
				}
				break;
			}
		}

		return fExpr;
	}

	FlattenStruct(Scope	*_fScope) : fScope(_fScope) {}
};

//-----------------------------------------------------------------------------
//	Factorise
//-----------------------------------------------------------------------------
// Factorise() - combine common factors
int IsAddSub(expr *fExpr) {
	if (fExpr->kind == BINARY_N) {
		switch (fExpr->op) {
			case ADD_OP:
			case ADD_V_OP:
			case ADD_SV_OP:
			case ADD_VS_OP:
				return +1;
			case SUB_OP:
			case SUB_V_OP:
			case SUB_SV_OP:
			case SUB_VS_OP:
				return -1;
		}
	}
	return 0;
}
int IsMulDiv(expr *fExpr) {
	if (fExpr->kind == BINARY_N) {
		switch (fExpr->op) {
			case MUL_OP:
			case MUL_V_OP:
			case MUL_SV_OP:
			case MUL_VS_OP:
				return +1;
			case DIV_OP:
			case DIV_V_OP:
			case DIV_SV_OP:
			case DIV_VS_OP:
				return -1;
		}
	}
	return 0;
}

bool AreSame(expr *lExpr, expr *rExpr) {
	if (lExpr == rExpr)
		return true;

	if (lExpr->kind != rExpr->kind)
		return false;

	switch (lExpr->kind) {
		case SYMB_N:
			return lExpr->op == rExpr->op
				&& lExpr->sym.symbol == rExpr->sym.symbol;
		case CONST_N:
			return lExpr->op == rExpr->op
				&& memcmp(lExpr->co.val, rExpr->co.val, sizeof(scalar_constant)) == 0;
		case UNARY_N:
			return lExpr->op == rExpr->op
				&& lExpr->un.subop == rExpr->un.subop
				&& AreSame(lExpr->un.arg, rExpr->un.arg);
		case BINARY_N:
			return lExpr->op == rExpr->op
				&& lExpr->bin.subop == rExpr->bin.subop
				&& AreSame(lExpr->bin.left, rExpr->bin.left)
				&& AreSame(lExpr->bin.right, rExpr->bin.right);
		case TRINARY_N:
			return lExpr->op == rExpr->op
				&& AreSame(lExpr->tri.arg1, rExpr->tri.arg1)
				&& AreSame(lExpr->tri.arg2, rExpr->tri.arg2)
				&& AreSame(lExpr->tri.arg3, rExpr->tri.arg3);
		default:
			return false;
	}
}

struct Factorise {
	expr *post_expr(CG *cg, expr *fExpr, int arg2) {
		int	root, left, right;
		if (root = IsAddSub(fExpr)) {
			expr	*lExpr = fExpr->bin.left;
			expr	*rExpr = fExpr->bin.right;
			if ((left = IsAddSub(lExpr)) && (right = IsAddSub(rExpr))) {
				fExpr->bin.right	= rExpr->bin.left;
				rExpr->bin.left		= fExpr;
				if (root < 0)
					rExpr->op = opcode(rExpr->op + right * (SUB_OP - ADD_OP));//flip
				lExpr	= fExpr;
				fExpr	= rExpr;
				rExpr	= fExpr->bin.right;
				root	= right;
			}
			if ((left = IsMulDiv(lExpr)) && (right = IsMulDiv(rExpr)) && left == right) {
				if (AreSame(lExpr->bin.right, rExpr->bin.right)) {
					fExpr->bin.left = lExpr->bin.left;
					fExpr->bin.right = rExpr->bin.left;
					lExpr->bin.left = fExpr;
					fExpr = lExpr;
				} else if (left > 0) {
					if (AreSame(lExpr->bin.left, rExpr->bin.left)) {
						fExpr->bin.left = lExpr->bin.right;
						fExpr->bin.right = rExpr->bin.right;
						lExpr->bin.right = fExpr;
						fExpr = lExpr;
					} else if (AreSame(lExpr->bin.left, rExpr->bin.right)) {
						fExpr->bin.left = lExpr->bin.right;
						fExpr->bin.right = rExpr->bin.left;
						lExpr->bin.right = fExpr;
						fExpr = lExpr;
					} else if (AreSame(lExpr->bin.right, rExpr->bin.left)) {
						fExpr->bin.left = lExpr->bin.left;
						fExpr->bin.right = rExpr->bin.right;
						lExpr->bin.left = fExpr;
						fExpr = lExpr;
					}
				}
			}
		}
		return fExpr;
	}
};

//-----------------------------------------------------------------------------
//	Flatten If Statements
//-----------------------------------------------------------------------------
// Convert if statements into conditional assignments.
//
//     if (A > B)
//         C = D;
//     else
//         E = F;
//
// becomes:
//
//     $if1 = A > B;
//     C@@($if1) = D;
//     E@@(!$if1) = F;
//
// and:
//
//     if (A > B)
//         if (C > D)
//             E = F;
//         else
//             G = H;
//     else
//         if (J > K)
//             L = M;
//         else
//             N = P;
//
// becomes:
//
//     $if1 = A > B;
//     $ife2 = C > D;
//     $if2 = $if1 && $ife2;
//     E@@($if2) = F;
//     $if2 = $if1 && !$ife2;
//     G@@($if2) = H;
//     $ife2 = J > K;
//     $if2 = !$if1 && $ife2;
//     L@@($if2) = M;
//     $if2 = !$if1 && !$ife2;
//     N@@($if2) = P;
//
// Existing conditional assignments:
//
//     A@@XZ = B;
//     C@@(D) = E;
//
// become:
//
//     A@@({ $if1, 0, $if1, 0 }) = B;
//     C@@($if1@xyzw && D) = E;     or:        C@@($if1 && D) = E;
//
// Issues:
//
//   1) "out" parameters to function calls: not a problem if function calls
//      have been previously inlined.
//   2) Assumes "chained" assignments have already been split into simple assignments.
//   3) Assumes all large asignments (structs, matrices, vectors > 4) have been eliminated.

struct FlattenIf {
	Scope *func_scope;
	Symbol *ifSymbTotal;  // Current combined if value: "$if2" for level 2, NULL for level 0.
	Symbol *ifSymbParent; // Enclosing if's combined value: "$if1" for level 2, NULL for level <= 1.
	Symbol *ifSymbLocal;  // Current level's simple if value:  "$ife2" for level 2

	stmt	*pre_stmt(CG *cg, stmt *fStmt, int flevel);

	FlattenIf(Scope *fScope) : func_scope(fScope), ifSymbTotal(NULL), ifSymbParent(NULL), ifSymbLocal(NULL) {}
};

stmt *FlattenIf::pre_stmt(CG *cg, stmt *fStmt, int flevel) {
#define IF_ROOT "$if"
#define IF_ROOT_E "$iflocal"
	stmt	*rStmt, *lStmt, *nStmt;
	expr	*eExpr, *lExpr, *rExpr, *ifvar, *nExpr, *mExpr, *tExpr;
	opcode	nop;
	int		level, lop, lsubop, nsubop, vname, mask, len, base, ii;
	Symbol	*lSymbTotal, *lSymbLocal, *ifSymbTotalSave, *ifSymbParentSave;
	Type	*lType;

	if (fStmt) {
		level = flevel >= 0 ? flevel : -flevel;
		switch (fStmt->kind) {
			case IF_STMT: {

				// Find $if1 variable(s) for this level:

				vname = GetNumberedAtom(cg, IF_ROOT, level + 1, 1, '\0');
				lSymbTotal = LookUpSymbol(cg, func_scope, vname);
				if (!lSymbTotal) {
					lSymbTotal = DefineVar(cg, fStmt->loc, func_scope, vname, BooleanType);
				}
				if (level > 0) {
					vname = GetNumberedAtom(cg, IF_ROOT_E, level + 1, 1, '\0');
					lSymbLocal = LookUpSymbol(cg, func_scope, vname);
					if (!lSymbLocal) {
						lSymbLocal = DefineVar(cg, fStmt->loc, func_scope, vname, BooleanType);
					}
				} else {
					lSymbLocal = lSymbTotal;
				}

				// Create assignment statement for local expression:

				StmtList lStatements;
				lExpr = GenSymb(cg, lSymbLocal);
				lExpr = GenBoolAssign(cg, lExpr, fStmt->ifst.cond);
				lStmt = NewExprStmt(fStmt->loc, lExpr);
				AppendStatements(&lStatements, lStmt);

				ifSymbTotalSave = ifSymbTotal;
				ifSymbParentSave = ifSymbParent;
				ifSymbParent = ifSymbLocal;
				ifSymbLocal = lSymbLocal;
				ifSymbTotal = lSymbTotal;

				// Compute effective Boolean expression if necessary:

				if (level > 0) {
					lExpr = GenSymb(cg, ifSymbParent);
					if (flevel == -1) {
						// Top level if's don't create a negated value:
						lExpr = GenBoolNot(cg, lExpr);
					}
					rExpr = GenSymb(cg, lSymbLocal);
					rExpr = GenBoolAnd(cg, lExpr, rExpr);
					lExpr = GenSymb(cg, ifSymbTotal);
					lExpr = GenBoolAssign(cg, lExpr, rExpr);
					lStmt = NewExprStmt(fStmt->loc, lExpr);
					AppendStatements(&lStatements, lStmt);
				}

				// Walk sub-statements and transform assignments into conditional assignments:

				lStmt = fStmt->ifst.thenstmt;
				fStmt->ifst.thenstmt = NULL;
				while (lStmt) {
					nStmt = lStmt->next;
					lStmt->next = NULL;
					lStmt = pre_stmt(cg, lStmt, level + 1);
					AppendStatements(&lStatements, lStmt);
					lStmt = nStmt;
				}
				if (fStmt->ifst.elsestmt) {

					// Compute effective Boolean expression if necessary:

					if (level > 0) {
						lExpr = GenSymb(cg, ifSymbParent);
						if (flevel == -1)
							lExpr = GenBoolNot(cg, lExpr);
						rExpr = GenSymb(cg, lSymbLocal);
						rExpr = GenBoolNot(cg, rExpr);
						rExpr = GenBoolAnd(cg, lExpr, rExpr);
						lExpr = GenSymb(cg, ifSymbTotal);
						lExpr = GenBoolAssign(cg, lExpr, rExpr);
						lStmt = NewExprStmt(fStmt->loc, lExpr);
						AppendStatements(&lStatements, lStmt);
					}
					lStmt = fStmt->ifst.elsestmt;
					fStmt->ifst.elsestmt = NULL;
					while (lStmt) {
						nStmt = lStmt->next;
						lStmt->next = NULL;
						lStmt = pre_stmt(cg, lStmt, -(level + 1));
						AppendStatements(&lStatements, lStmt);
						lStmt = nStmt;
					}
				}
				ifSymbTotal = ifSymbTotalSave;
				ifSymbLocal = ifSymbParent;
				ifSymbParent = ifSymbParentSave;
				rStmt = lStatements.first;
				break;
			}
			case EXPR_STMT:
				if (level > 0) {
					eExpr = fStmt->exprst.exp;
					if (IsAssignSVOp(eExpr)) {
						lExpr = eExpr->bin.left;
						rExpr = eExpr->bin.right;
						lop = eExpr->op;
						lsubop = eExpr->bin.subop;
						lType = eExpr->type;
						ifvar = GenSymb(cg, ifSymbTotal);
						if (flevel == -1)
							ifvar = GenBoolNot(cg, ifvar);
						if (lop == ASSIGN_MASKED_KV_OP) {
							mask = SUBOP_GET_MASK(lsubop);
							len = SUBOP_GET_S(lsubop);
							base = SUBOP_GET_T(lsubop);
							// Create vector of $if/FALSE values:
							mExpr = NULL;
							for (ii = 0; ii < len; ii++) {
								if (mask & 1) {
									tExpr = GenSymb(cg, ifSymbTotal);
								} else {
									tExpr = GenBoolConst(cg, 0);
								}
								mExpr = GenExprList(cg, mExpr, tExpr, BooleanType);
								mask >>= 1;
							}
							ifvar =  NewUnopSubNode(cg, VECTOR_V_OP, SUBOP_V(len, TYPE_BASE_BOOLEAN), mExpr);
							ifvar->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, len, 0);
							nop = ASSIGN_COND_V_OP;
							nsubop = SUBOP_V(len, base);
						} else {
							// Normal assign.  Convert it to simple conditional assignment:
							switch (lop) {
								case ASSIGN_OP:
									nop = ASSIGN_COND_OP;
									nsubop = lsubop;
									break;
								case ASSIGN_V_OP:
									nop = ASSIGN_COND_SV_OP;
									nsubop = lsubop;
									break;
								case ASSIGN_GEN_OP:
									nop = ASSIGN_COND_GEN_OP;
									nsubop = lsubop;
									break;
								default:
									assert(0);
									break;
							}
						}
						nExpr =  NewTriopSubNode(cg, nop, nsubop, lExpr, ifvar, rExpr);
						nExpr->type = lType;
						fStmt->exprst.exp = nExpr;
						rStmt = fStmt;
					} else if (IsAssignCondSVOp(eExpr)) {
						switch (eExpr->op) {
							case ASSIGN_COND_OP:
							case ASSIGN_COND_SV_OP:
							case ASSIGN_COND_GEN_OP:
								ifvar = GenSymb(cg, ifSymbTotal);
								if (flevel == -1)
									ifvar = GenBoolNot(cg, ifvar);
								eExpr->tri.arg2 = GenBoolAnd(cg, eExpr->tri.arg2, ifvar);
								break;
							case ASSIGN_COND_V_OP:
								lsubop = eExpr->tri.subop;
								len = SUBOP_GET_S(lsubop);
								// Create vector of $if values:
								mExpr = NULL;
								for (ii = 0; ii < len; ii++) {
									tExpr = GenSymb(cg, ifSymbTotal);
									if (flevel == -1)
										tExpr = GenBoolNot(cg, tExpr);
									mExpr = GenExprList(cg, mExpr, tExpr, BooleanType);
								}
								ifvar =  NewUnopSubNode(cg, VECTOR_V_OP, SUBOP_V(len, TYPE_BASE_BOOLEAN), mExpr);
								ifvar->type = GetStandardType(cg->hal, TYPE_BASE_BOOLEAN, len, 0);
								eExpr->tri.arg2 = GenBoolAndVec(cg, eExpr->tri.arg2, ifvar, len);
								break;
							default:
								assert(0);
								break;
						}
						rStmt = fStmt;
					} else {
						rStmt = fStmt;
					}
				} else {
					rStmt = fStmt;
				}
				break;
			case BLOCK_STMT:
				if (level > 0) {
					StmtList lStatements;
					lStmt = fStmt->blockst.body;
					while (lStmt) {
						nStmt = lStmt->next;
						lStmt->next = NULL;
						lStmt = pre_stmt(cg, lStmt, flevel);
						AppendStatements(&lStatements, lStmt);
						lStmt = nStmt;
					}
					rStmt = lStatements.first;
				} else {
					rStmt = fStmt;
				}
				break;
			case DISCARD_STMT:
				if (level > 0) {
					ifvar = GenSymb(cg, ifSymbTotal);
					if (flevel == -1)
						ifvar = GenBoolNot(cg, ifvar);
					if (fStmt->discardst.cond->un.arg) {
						lType = fStmt->discardst.cond->type;
						if (IsVector(lType, &len)) {
							ifvar = GenBoolSmear(cg, ifvar, len);
							ifvar = GenBoolAndVec(cg, ifvar, fStmt->discardst.cond->un.arg, len);
						} else {
							ifvar = GenBoolAnd(cg, ifvar, fStmt->discardst.cond->un.arg);
						}
					}
					fStmt->discardst.cond->un.arg = ifvar;
				}
				rStmt = fStmt;
				break;
			default:
				rStmt = fStmt;
				break;
		}
	} else {
		rStmt = fStmt;
	}
	return rStmt;
}


//-----------------------------------------------------------------------------
//	Main Compile Control
//-----------------------------------------------------------------------------

void PrintFunction1(CG *cg, Symbol *program, const char *label) {
	if (cg->opts & cgclib::DUMP_STAGES) {
		cg->printf("=======================================================================\n");
		cg->printf("%s\n", label);
		cg->printf("=======================================================================\n");
		PrintFunction(cg, program);
	}
}

void PrintFunction2(CG *cg, Symbol *program, stmt *lStmt, const char *label) {
	program->fun.statements = lStmt;
	PrintFunction1(cg, program, label);
}

void ResetBindings(SymbolList *&_list) {
	for (SymbolList *list = _list, *next; list; list = next) {
		next	= list->next;
		if (Symbol *s = list->p) {
			if (Binding *b = s->var.bind)
				b->properties &= ~BIND_IS_BOUND;
			s->properties |= SYMB_NEEDS_BINDING;
			delete list;
		}
	}
	_list = 0;
}


// Bind any members that are currently unbound.  Must be a uniform pseudo-connector.
void BindUnboundUniformMembers(CG *cg, SymbolList *list) {
	while (list != NULL) {
		if (Symbol *s = list->p) {
			Binding *b = s->var.bind;
			if (b && !(b->properties & BIND_IS_BOUND)) {
				if (!cg->hal->BindUniformUnbound(cg, s->loc, s, b))
					SemanticWarning(cg, s->loc, WARNING_S_CANT_BIND_UNIFORM_VAR, cg->GetAtomString(s->name));
			}
		}
		list = list->next;
	}
}

// If there weren't any errors, perform optimizations, register allocation, code generation, etc.
// Returns: Number of errors.
int CG::CompileProgram(Symbol *program) {
	ResetBindings(uniformParam);
	ResetBindings(uniformGlobal);

	int		ret = 0;
	stmt	*origstatements = program->fun.statements;
	Scope	*lScope = program->fun.locals, *gScope = lScope->parent;

	Scope	*tScope = NewScopeInPool(mem_CreatePool(0, 0));
	tScope->parent	= lScope;
	tScope->level	= 2;
//	tScope->func_scope = lScope;

	PrintFunction1(this, program, "Original");

	if (!hal->GetCapsBit(CAPS_NO_OPTIMISATION)) {
		program->fun.statements = DuplicateStatementTree(this, program->fun.statements);
		program->fun.statements = BuildSemanticStructs(this, program->loc, tScope, program, "$vin", "$vout");
		PrintFunction1(this, program, "Semantics");
		
		if (!CheckFunctionDefinitions(this, program->loc, global_scope, program)) {
			SetStructMemberOffsets(hal, varyingIn->type);
			SetStructMemberOffsets(hal, varyingOut->type);
			BindUnboundUniformMembers(this, uniformParam);
			BindUnboundUniformMembers(this, uniformGlobal);

			// Convert to Basic Blocks Format goes here...

			// Inline appropriate function calls...
			stmt	*lStmt = program->fun.statements;
			lStmt = ConcatStmts(DuplicateStatementTree(this, global_scope->init_stmts), lStmt);
			PrintFunction2(this, program, lStmt, "Output");

			if (error_count == 0) {
				lStmt = ExpandInlineFunctionCalls(this, tScope, lStmt, hal->GetCapsBit(CAPS_INLINE_ALL_FUNCTIONS));
				PrintFunction2(this, program, lStmt, "Inlining");

				CheckForHiddenVaryingReferences(this, lStmt);
				if (opts & (cgclib::DUMP_PARSETREE | cgclib::DUMP_NODETREE)) {
					program->fun.statements = lStmt;
					PrintSymbolTree(this, program->fun.locals->symbols);
					if (opts & cgclib::DUMP_PARSETREE)
						PrintFunction(this, program);
					if (opts & cgclib::DUMP_NODETREE)
						BPrintFunction(this, program);
				}

				CheckConnectorUsageMain(this, program, lStmt);
				lStmt = ConvertDebugCalls(this, program->loc, lScope, lStmt, !!(opts & cgclib::DEBUG_ENABLE));
				ApplyToExpressions(ExpandIncDec(), lStmt, 0);
				ApplyToExpressions(ExpandCompoundAssignment(), lStmt, 0);
				lStmt = ApplyToStatements(FlattenCommas(), lStmt, 0);
				lStmt = ApplyToStatements(RemoveEmptyStatements(), lStmt, 0);
				lStmt = ApplyToStatements(FlattenChainedAssignments(), lStmt, 0);
				ApplyToExpressions(ConvertNamedConstants(), lStmt, 0);
				FoldConstants(this, lStmt, FC_MATRIX_INDEX_TO_SWIZMAT);

				PrintFunction2(this, program, lStmt, "Transforms");

				lStmt = StaticAnalysis(this, lScope, lStmt);
				PrintFunction2(this, program, lStmt, "StaticAnalysis");

				if (hal->GetCapsBit(CAPS_DECONSTRUCT_MATRICES)) {
					lStmt = ApplyToStatements(DeconstructMatrices(lScope), lStmt);
					PrintFunction2(this, program, lStmt, "DeconstructMatrices");
				}

				if (hal->GetCapsBit(CAPS_DECONSTRUCT_VECTORS)) {
					lStmt = ApplyToStatements(DeconstructVectors(lScope), lStmt);
					PrintFunction2(this, program, lStmt, "DeconstructVectors");
				}

				lStmt = ApplyToStatements(FlattenStructAssignment(), lStmt, 0);
				PrintFunction2(this, program, lStmt, "FlattenStructAssignments");
				if (!hal->GetCapsBit(CAPS_DONT_FLATTEN_IF_STATEMENTS))
					lStmt =  ApplyToStatements(FlattenIf(global_scope), lStmt);
				FoldConstants(this, lStmt, FC_MATRIX_INDEX_TO_SWIZMAT);

				// Optimizations:

				ApplyToExpressions(Factorise(), lStmt);
				PrintFunction2(this, program, lStmt, "FactoriseExpr");
				ApplyToExpressions(FlattenStruct(lScope), lStmt);
				PrintFunction2(this, program, lStmt, "FlattenStructExpr");
				lStmt = StaticAnalysis(this, lScope, lStmt);
				PrintFunction2(this, program, lStmt, "StaticAnalysis");
				FoldConstants(this, lStmt, 0);
				lStmt = StaticAnalysis(this, lScope, lStmt);
				PrintFunction2(this, program, lStmt, "StaticAnalysis again");

				// Lots more optimization stuff goes here...

				if (opts & cgclib::DUMP_FINALTREE) {
	//				PrintSymbolTree(program->fun.locals->symbols);
					PrintFunction2(this, program, lStmt, "Final program");
					if (opts & cgclib::DUMP_NODETREE)
						BPrintFunction(this, program);
				}

				program->fun.statements = lStmt;
			}
		}
	}

	ret = hal->GenerateCode(this, program->loc, global_scope, program);

	if (varyingIn)
		RemoveSymbol(this, program->loc, tScope, varyingIn);
	if (varyingOut)
		RemoveSymbol(this, program->loc, tScope, varyingOut);
	varyingIn = NULL;
	varyingOut = NULL;

	program->fun.statements = origstatements;
//	if (initend)
//		initend->next = NULL;
	return ret;//cg->error_count;
}

void GetSamplers(CG *cg, cgclib::item **samplers, Symbol *sym) {
	expr	*e, *s;
	while (sym) {
		if (sym->left)
			GetSamplers(cg, samplers, sym->left);

		switch (GetCategory(sym->type)) {
			case TYPE_CATEGORY_TEXOBJ: {
				cgclib::sampler	*cgcs	= cgclib::AddItem<cgclib::sampler>(samplers, cg->GetAtomString(sym->name));
				for (e = sym->var.init; e; e = e->bin.right) {
					s = e->bin.left;
					switch (s->kind) {
						case SYMBOLIC_N: {
							cgclib::state_symb	*cgcx = cgclib::AddItem<cgclib::state_symb>(&cgcs->states, cg->GetAtomString(e->bin.subop));
							cgcx->value	= strdup(cg->GetAtomString(s->blx.name));
							break;
						}
					}
				}
				break;
			}
		}

		sym = sym->right;
	}
}

struct BufferOutputData : cgclib::Outputter {
	void	*buffer;
	size_t	size, space;
	void	put(const void *p, size_t len) {
		if (size + len > space) {
			space	= space ? space * 2 : len < 512 ? 1024 : len * 2;
			buffer	= realloc(buffer, space);
		}
		memcpy((char*)buffer + size, p, len);
		size += len;
	}
	BufferOutputData() : cgclib::Outputter(this), buffer(0), size(0), space(0) {}
	operator void*()		{ void *p = buffer; buffer = 0; return p;	}
};

int CG::CompileProgram2(Symbol *program, cgclib::shader *cgcs) {
	BufferOutputData	buff;
	output		= &buff;
	int		e	= error_count;
	int		r	= CompileProgram(program);
	if (r == 0) {
		cgcs->size	= buff.size;
		cgcs->code	= buff;
		getbindings(this, &cgcs->bindings, uniformParam);
		getbindings(this, &cgcs->bindings, uniformGlobal);
	}
	error_count		= e;
	return r;
}

#ifndef WIN32
int	stricmp(const char *a, const char *b) {
	for (;;) {
		char ca = *a++, cb = *b++;
		if (ca >= 'A' && ca <= 'Z')
			ca += 'a' - 'A';
		if (cb >= 'A' && cb <= 'Z')
			cb += 'a' - 'A';

		int	d = ca - cb;
		if (d || !ca)
			return d;
	}
}
#endif

int CG::CompileTechniques() {
	if (hal->GetCapsBit(CAPS_FX)) {
		SetSymbolFlagsList(global_scope, 0);
		
		BufferOutputData	buff;
		output		= &buff;
		int		e	= error_count;
		int		r 	= hal->GenerateCode(this, lastSourceLoc, NULL, NULL);
		error_count	= e;
		
	} else {
		for (SymbolList *nList = techniques; nList; nList = nList->next) {
			Symbol				*sym	= nList->p;
			cgclib::technique	*cgct	= cgclib::AddItem<cgclib::technique>(&items, GetAtomString(sym->name));

			for (expr *pass = sym->var.init; pass; pass = pass->bin.right) {
				cgclib::pass	*cgcp	= cgclib::AddItem<cgclib::pass>((cgclib::item**)&cgct->passes, GetAtomString(pass->bin.subop));
				bool			had_ps	= false;
	//			printf("\npass: %s\n", cgcp->item.name);

				for (expr *e = pass->bin.left; e; e = e->bin.right) {
					if (e->op == EXPR_LIST_OP && e->bin.left->kind == SYMBOLIC_N) {
						const char *name = GetAtomString(e->bin.subop);
						if (!hal->GetCapsBit(CAPS_FX) && (stricmp(name, "pixelshader") == 0 || stricmp(name, "vertexshader") == 0)) {
							if (opts & cgclib::PARSE_ONLY) {
								cgclib::state_symb	*cgcs = cgclib::AddItem<cgclib::state_symb>(&cgcp->shaders, name);
								cgcs->value	= strdup(GetAtomString(e->bin.left->blx.name));
								
							} else {
								Symbol	*prog = LookUpSymbol(this, current_scope, e->bin.left->blx.name);
								if (!prog || !IsFunction(prog) || !(prog->properties & SYMB_IS_DEFINED)) {
									SemanticError(this, sym->loc, ERROR_S_CALL_UNDEF_FUN, GetAtomString(e->bin.left->blx.name));
								} else {
									const char	*profile = GetAtomString(e->bin.left->blx.name2);
									if (!InitHAL(GetProfile(profile))) {
										SemanticError(this, sym->loc, ERROR_S_UNRECOGNIZED_PROFILE, profile);
									} else {
										SetSymbolFlagsList(global_scope, 0);
										CompileProgram2(prog, cgclib::AddItem<cgclib::shader>(&cgcp->shaders, GetAtomString(prog->name)));
									}
								}
							}
							had_ps = had_ps || stricmp(name, "pixelshader") == 0;

						} else {
							cgclib::state_symb	*cgcs = cgclib::AddItem<cgclib::state_symb>(&cgcp->states, name);
							cgcs->value	= strdup(GetAtomString(e->bin.left->blx.name));
						}
					}

				}
				if (!hal->GetCapsBit(CAPS_FX) && !(opts & cgclib::PARSE_ONLY) && !had_ps) {
					cgclib::shader		*cgcs = cgclib::AddItem<cgclib::shader>(&cgcp->shaders, "dummy");
					BufferOutputData	buff;
					output 				= &buff;
					hal->GenerateCode(this, lastSourceLoc, NULL, NULL);
					cgcs->size			= buff.size;
					cgcs->code			= buff.buffer;
					getbindings(this, &cgcs->bindings, uniformParam);
					getbindings(this, &cgcs->bindings, uniformGlobal);
				}

			}
		}
	}
	
	GetSamplers(this, &items, current_scope->symbols);
	return error_count;
}

} //namespace cgc
