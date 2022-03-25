#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cg.h"
#include "compile.h"
#include "errors.h"

namespace cgc {

int IsSimple(expr *fExpr) {
	if (IsConstant(fExpr))
		return 1;

	if (IsCastOp(fExpr))
		return IsSimple(fExpr->un.arg);

	if (fExpr->kind == BINARY_N && fExpr->op == MEMBER_SELECTOR_OP)
		return IsSimple(fExpr->bin.left);

	if (fExpr->kind == UNARY_N && (fExpr->op == SWIZZLE_Z_OP || fExpr->op == SWIZMAT_Z_OP))
		return IsSimple(fExpr->un.arg);

	if (fExpr->kind == SYMB_N && fExpr->op == VARIABLE_OP)
		return fExpr->sym.symbol->kind == VARIABLE_S;

	if (IsArrayIndex(fExpr))
		return IsSimple(fExpr->bin.right);

	return 0;
}

static expr *GetSwizzledExpr(expr *fExpr, expr *sExpr)
{
	if (fExpr->kind == UNARY_N) switch (fExpr->op) {
		case VECTOR_V_OP: {
			int		i;
			int		subop = sExpr->un.subop;
			int		mask = SUBOP_GET_MASK(subop);
			int		len = SUBOP_GET_S2(subop);
			int		base = SUBOP_GET_T(subop);
			if (len == 0)
				len = 1;

			for (i = 0; i < len; i++, mask >>= 2) {
				int		f = mask & 3, len2;
				expr	*e	= fExpr->un.arg;
				while (e && e->kind == BINARY_N && e->op == EXPR_LIST_OP) {
					len2 = 1;
					IsVector(e->bin.left->type, &len2);
					if (f < len2)
						break;
					f -= len2;
					e = e->bin.right;
				}
				e = e->bin.left;
				if (len == 1 && IsSimple(e)) {
					if (len2 == 1)
						return e;
					mask = f * (((1<<0) + (1<<2) + (1<<4) + (1<<6)) & ((1 << (2 * len)) - 1));
					sExpr->un.subop = SUBOP_Z(mask, SUBOP_GET_S2(subop), SUBOP_GET_S1(subop), base);
					sExpr->un.arg	= e;
				}
			}
			break;
		}
//		case CAST_CV_OP:
//			break;
//		case CAST_CM_OP:
//			break;
//		case MATRIX_M_OP:
//			break;
	}
	return sExpr;
}

// lSymb->tempptr is last use of this symbol
// fExpr->tempptr[0] is PREV pointer - previous use of the symbol in this program
// fExpr->tempptr[1] is NEXT pointer - next use of the symbol in this program
// fExpr->tempptr[2] is assignment expr - used for SWIZMAT
// first link in list should be to assignment expr

static expr *get_assignment(expr *fExpr) {
	expr	*a, *e;
	for (e = (expr*)fExpr->tempptr[0]; e; e = (expr*)e->tempptr[0])
		a = e;
	return a;
}

struct StaticAnalysis1 {
	expr *post_expr(CG *cg, expr *e, int arg2);
	stmt *pre_stmt(CG *cg, stmt *s, int arg2);
};

struct StaticAnalysis2 {
	expr *post_expr(CG *cg, expr *e, int arg2);
	stmt *post_stmt(CG *cg, stmt *s, int arg2);
};

expr *StaticAnalysis1::post_expr(CG *cg, expr *fExpr, int arg2) {
	if (fExpr->kind == SYMB_N && fExpr->op == VARIABLE_OP) {
		Symbol	*lSymb		= fExpr->sym.symbol;
		expr	*last_expr	= (expr*)lSymb->tempptr;
//		printf("Reference to %s\n", GetAtomString(atable, lSymb->name));
		fExpr->tempptr[1] = NULL;
		fExpr->tempptr[2] = NULL;
		if (last_expr) {
			if (last_expr != fExpr) {
				fExpr->tempptr[0]		= last_expr;
				last_expr->tempptr[1]	= lSymb->tempptr = fExpr;
			}
		} else {
			fExpr->tempptr[0]	= NULL;	// symbol hasn't been set yet
		}
	}
	return fExpr;
}

stmt *StaticAnalysis1::pre_stmt(CG *cg, stmt *fStmt, int arg2) {
	if (fStmt) switch (fStmt->kind) {
		case EXPR_STMT: {
			expr	*fExpr = fStmt->exprst.exp;
			if (IsAssignSVOp(fExpr)) {
				expr	*lExpr	= fExpr->bin.left;
				int		swiz	= 0;
				if (lExpr->op == ARRAY_INDEX_OP)
					lExpr->bin.right = cg->ApplyToNodes(*this, lExpr->bin.right, arg2);
				fExpr->bin.right = cg->ApplyToNodes(*this, fExpr->bin.right, arg2);
				while (lExpr->kind == UNARY_N && lExpr->op == SWIZZLE_Z_OP) {
					swiz	= lExpr->un.subop;
					lExpr	= lExpr->un.arg;
				}
				if (lExpr->kind == SYMB_N) {
					fExpr->tempptr[0] = NULL;	// initialise list
					fExpr->tempptr[1] = NULL;

					lExpr->tempptr[0] = NULL;	// terminate prev list here
					lExpr->sym.symbol->tempptr = swiz ? NULL : fExpr;
				}
				return fStmt;
			}
			break;
		}

		case WHILE_STMT:
		case DO_STMT:
			fStmt->whilest.cond = cg->ApplyToNodes(*this, fStmt->whilest.cond, arg2);
			break;

		case FOR_STMT: {
			StmtList list;
			stmt	*init, *body, *step;
			expr	*cond;

			init	= cg->ApplyToStatements(*this, fStmt->forst.init, arg2);
			cond	= cg->ApplyToNodes(*this, DupExpr(cg, fStmt->forst.cond), arg2);

			init	= cg->ApplyToStatements(StaticAnalysis2(), init, arg2);
			AppendStatements(&list, init);

			for (;;) {
				cond	= FoldConstants(cg, cg->ApplyToNodes(StaticAnalysis2(), cond, arg2), FC_MATRIX_INDEX_TO_SWIZMAT);

				if (!IsConstant(cond)) {
					InternalError(cg, fStmt->loc, ERROR___FOR_LOOP, "conditional indeterminate");
					break;
				}

				if (cond->co.val[0].i == 0)
					return list.first;

				body	= cg->ApplyToStatements(*this, DuplicateStatementTree(cg, fStmt->forst.body), arg2);
				step	= cg->ApplyToStatements(*this, DuplicateStatementTree(cg, fStmt->forst.step), arg2);
				body	= cg->ApplyToStatements(StaticAnalysis2(), body, 1);
				AppendStatements(&list, body);
				step	= cg->ApplyToStatements(StaticAnalysis2(), step, 1);
				AppendStatements(&list, step);

				cond	= cg->ApplyToNodes(*this, DupExpr(cg, fStmt->forst.cond), arg2);
			}
		}
	}

	cg->ApplyToExpressionsLocal(*this, fStmt, arg2);
	return fStmt;
}

expr *StaticAnalysis2::post_expr(CG *cg, expr *fExpr, int arg2) {
	expr	*last_expr;
	expr	*e;
	switch (fExpr->kind) {
		case SYMB_N:
			if (fExpr->op == VARIABLE_OP && fExpr->tempptr[0]) {
//				printf("Collapsing %s\n", GetAtomString(atable, fExpr->sym.symbol->name));
				expr	*ass = get_assignment(fExpr);
				if (!IsAssignSVOp(ass))
					return ass;
				return DupExpr(cg, get_assignment(fExpr)->bin.right);
//				return last_expr->bin.right;
			}
			break;
#if 1
		case UNARY_N:
			switch (fExpr->op) {
				case SWIZZLE_Z_OP:
				case SWIZMAT_Z_OP:
					e = fExpr->un.arg;
					if (e->kind == SYMB_N && e->op == VARIABLE_OP) {
//						printf("Examining %s\n", GetAtomString(atable, e->sym.symbol->name));
						if (last_expr = (expr*)e->tempptr[2])
							return GetSwizzledExpr(last_expr, fExpr);
					}
					break;
			}
			break;
		case BINARY_N:
			switch (fExpr->op) {
				case MEMBER_SELECTOR_OP:
					e = fExpr->bin.left;
					if (e->kind == SYMB_N && e->op == VARIABLE_OP) {
//						printf("Examining %s\n", GetAtomString(atable, e->sym.symbol->name));
					}
					break;
			}
			break;
#endif
	}
	return fExpr;
}

stmt *StaticAnalysis2::post_stmt(CG *cg, stmt *fStmt, int arg2) {
	if (fStmt && fStmt->kind == EXPR_STMT) {
		expr	*fExpr = fStmt->exprst.exp;
		if (IsAssignSVOp(fExpr)) {
			expr *lExpr	= fExpr->bin.left;
			if (lExpr->op == ARRAY_INDEX_OP)
				lExpr->bin.right = cg->ApplyToNodes(*this, lExpr->bin.right, arg2);
			fExpr->bin.right = cg->ApplyToNodes(*this, fExpr->bin.right, arg2);
			fExpr->bin.right = FoldConstants(cg, fExpr->bin.right, FC_MATRIX_INDEX_TO_SWIZMAT);

			if (lExpr->kind == SYMB_N) {
				expr	*texpr, *texpr2;
				int		n = 0;
				for (texpr = (expr*)fExpr->tempptr[1]; texpr; texpr = (expr*)texpr->tempptr[1])
					n++;
//				printf("%i uses of %s\n", n, GetAtomString(atable, fExpr->bin.left->sym.symbol->name));
//				if ((n == 0 && !fExpr->has_sideeffects) || n == 1 || IsSimple(fExpr->bin.right))
				if ((n == 0 && !fExpr->has_sideeffects) || IsSimple(fExpr->bin.right))
					return arg2 & 1 ? fStmt : NULL;

				for (texpr = (expr*)fExpr->tempptr[1]; texpr; texpr = texpr2) {
					texpr2 = (expr*)texpr->tempptr[1];
					texpr->tempptr[0] = NULL;	// disable patching
					texpr->tempptr[2] = fExpr->bin.right;
				}
			}
			return fStmt;
		} else if (!fExpr->has_sideeffects) {
			return NULL;
		}
	}
	cg->ApplyToExpressionsLocal(*this, fStmt, arg2);
	return fStmt;
}

stmt *StaticAnalysis(CG *cg, Scope *fscope, stmt *body) {
	ClearAllSymbolTempptr();
	body = cg->ApplyToStatements(StaticAnalysis1(), body, 0);
	body = cg->ApplyToStatements(StaticAnalysis2(), body, 0);
	return body;
}

} //namespace cgc
