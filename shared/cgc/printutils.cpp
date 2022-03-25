#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "printutils.h"
#include "cg.h"
#include "support.h"

namespace cgc {

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Debug Printing Functions: //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

static void lIndent(CG *cg, int level) {
	cg->printf("\n");
	for (int i = 0; i < level; i++)
		cg->printf("    ");
}

// Print a low level representation of an expression tree.
static void lBPrintExpression(CG *cg, expr *fexpr, int level) {
	char s[32];
	int subop, mask, ii, nn, bval;
	int OK = 1, HasString = 0;

	lIndent(cg, level);
	if (fexpr) {
		switch (fexpr->kind) {
		case DECL_N:
			cg->printf("DECLARATION");
			OK = 0;
			break;
		case SYMB_N:
			cg->printf("S ");
			break;
		case CONST_N:
			cg->printf("C ");
			break;
		case UNARY_N:
			cg->printf("U ");
			break;
		case BINARY_N:
			cg->printf("B ");
			break;
		case TRINARY_N:
			cg->printf("T ");
			break;
		default:
			cg->printf("<kind=%02x>", fexpr->kind);
			OK = 0;
			break;
		}
		if (OK) {
			cg->printf("%c%c", fexpr->is_lvalue ? 'L' : ' ',
					fexpr->has_sideeffects ? '+' : ' ');
			subop = fexpr->co.subop;
			switch (opcode_subopkind[fexpr->op]) {
				case SUB_NONE:
					cg->printf("- - - ");
					mask = ~0;
					break;
				case SUB_S:
					cg->printf("- - %1x ", SUBOP_GET_T(subop));
					mask = 0x0000000f;
					break;
				case SUB_V:
				case SUB_VS:
				case SUB_SV:
					cg->printf("- %1x %1x ", SUBOP_GET_S(subop), SUBOP_GET_T(subop));
					mask = 0x000000ff;
					break;
				case SUB_M:
				case SUB_VM:
				case SUB_MV:
					cg->printf("%1x %1x %1x ", SUBOP_GET_S2(subop), SUBOP_GET_S1(subop),
							SUBOP_GET_T1(subop));
					mask = 0x0000f0ff;
					break;
				case SUB_Z:
					cg->printf("%1x %1x %1x ", SUBOP_GET_S2(subop), SUBOP_GET_S1(subop),
							SUBOP_GET_T1(subop));
					mask = SUBOP_GET_MASK(subop);
					nn = SUBOP_GET_S2(subop);
					if (nn == 0)
						nn = 1; // var.x is scalar, not array[1]
					s[nn] = '\0';
					for (ii = 0; ii < nn; ii++)
						s[ii] = "xyzw"[(mask >> ii*2) & 3];
					mask = 0x00fff0ff;
					HasString = 1;
					break;
				case SUB_ZM:
					cg->printf("%1x %1x %1x %1x ", SUBOP_GET_S2(subop), SUBOP_GET_T2(subop),
							SUBOP_GET_S1(subop), SUBOP_GET_T1(subop));
					mask = SUBOP_GET_MASK16(subop);
					nn = SUBOP_GET_T2(subop);
					if (nn == 0)
						nn = 1; // var.x is scalar, not array[1]
					s[nn*3] = '\0';
					for (ii = 0; ii < nn; ii++) {
						s[ii*3] = '_';
						s[ii*3 + 1] = '0' + ((mask >> (ii*4 + 2)) & 3);
						s[ii*3 + 2] = '0' + ((mask >> ii*4) & 3);
					}
					mask = 0xffffffff;
					HasString = 1;
					break;
				case SUB_CS:
					cg->printf("%1x - %1x ", SUBOP_GET_T2(subop), SUBOP_GET_T1(subop));
					mask = 0x00000f0f;
					break;
				case SUB_CV:
					cg->printf("%1x %1x %1x ", SUBOP_GET_T2(subop), SUBOP_GET_S1(subop),
							SUBOP_GET_T1(subop));
					mask = 0x00000fff;
					break;
				case SUB_CM:
					cg->printf("%1x %1x %1x %1x ", SUBOP_GET_S2(subop), SUBOP_GET_T2(subop),
							SUBOP_GET_S1(subop), SUBOP_GET_T1(subop));
					mask = 0x0000ffff;
					break;
				case SUB_KV:
					cg->printf("- %1x %1x ", SUBOP_GET_S(subop), SUBOP_GET_T(subop));
					mask = SUBOP_GET_MASK(subop) & 0xf;
					for (ii = 0; ii < 4; ii++)
						cg->printf("%c", (mask >> ii) & 1 ? "xyzw"[ii] : '-');
					cg->printf(" ");
					mask = 0x000f00ff;
					break;
				default:
					mask = 0;
					break;
			}
			if (subop & ~mask)
				cg->printf("<<non-zero:%08x>> ", subop & ~mask);
			cg->printf("%-6s ", opcode_name[fexpr->op]);
			switch (fexpr->kind) {
			case SYMB_N:
				cg->printf("\"%s\"", cg->GetAtomString(fexpr->sym.symbol->name));
				break;
			case CONST_N:
				switch (fexpr->op) {
				case ICONST_OP:
					cg->printf("%d", fexpr->co.val[0].i);
					break;
				case ICONST_V_OP:
					nn = SUBOP_GET_S(subop);
					cg->printf("{ ");
					for (ii = 0; ii < nn; ii++) {
						if (ii > 0)
							cg->printf(", ");
						cg->printf("%d", fexpr->co.val[ii].i);
					}
					cg->printf(" }");
					break;
				case UCONST_OP:
					cg->printf("%u", fexpr->co.val[0].i);
					break;
				case UCONST_V_OP:
					nn = SUBOP_GET_S(subop);
					cg->printf("{ ");
					for (ii = 0; ii < nn; ii++) {
						if (ii > 0)
							cg->printf(", ");
						cg->printf("%u", fexpr->co.val[ii].i);
					}
					cg->printf(" }");
					break;
				case BCONST_OP:
					bval = fexpr->co.val[0].i;
					if (bval == 0) {
						cg->printf("false");
					} else if (bval == 1) {
						cg->printf("true");
					} else {
						cg->printf("<<bad-bool-%08x>>", fexpr->co.val[0].i);
					}
					break;
				case BCONST_V_OP:
					nn = SUBOP_GET_S(subop);
					cg->printf("{ ");
					for (ii = 0; ii < nn; ii++) {
						if (ii > 0)
							cg->printf(", ");
						bval = fexpr->co.val[ii].i;
						if (bval == 0) {
							cg->printf("false");
						} else if (bval == 1) {
							cg->printf("true");
						} else {
							cg->printf("<<bad-bool-%08x>>", fexpr->co.val[ii].i);
						}
					}
					cg->printf(" }");
					break;
				case FCONST_OP:
				case HCONST_OP:
				case XCONST_OP:
					cg->printf("%1.6g", fexpr->co.val[0].f);
					break;
				case FCONST_V_OP:
				case HCONST_V_OP:
				case XCONST_V_OP:
					nn = SUBOP_GET_S(subop);
					cg->printf("{ ");
					for (ii = 0; ii < nn; ii++) {
						if (ii > 0)
							cg->printf(", ");
						cg->printf("%1.6g", fexpr->co.val[ii].f);
					}
					cg->printf(" }");
					break;
				default:
					cg->printf("UNKNOWN-CONSTANT");
					break;
				}
				break;
			case UNARY_N:
				break;
			case BINARY_N:
				break;
			case TRINARY_N:
				break;
			}
			if (HasString)
				cg->printf(" %s", s);
			cg->printf(" ");
			PrintType(cg, fexpr->type, 1);
			cg->printf("\n");
			switch (fexpr->kind) {
			case SYMB_N:
				break;
			case CONST_N:
				break;
			case UNARY_N:
				lBPrintExpression(cg, fexpr->un.arg, level + 1);
				break;
			case BINARY_N:
				lBPrintExpression(cg, fexpr->bin.left, level + 1);
				lBPrintExpression(cg, fexpr->bin.right, level + 1);
				break;
			case TRINARY_N:
				lBPrintExpression(cg, fexpr->tri.arg1, level + 1);
				lBPrintExpression(cg, fexpr->tri.arg2, level + 1);
				lBPrintExpression(cg, fexpr->tri.arg3, level + 1);
				break;
			}
		} else {
			cg->printf("\n");
		}
	} else {
		cg->printf("NULL\n");
	}
}

void BPrintExpression(CG *cg, expr *fexpr) {
	lBPrintExpression(cg, fexpr, 0);
}

static void lBPrintStmt(CG *cg, stmt *fstmt, int level);

void lBPrintStmtList(CG *cg, stmt *fstmt, int level) {
	while (fstmt) {
		lBPrintStmt(cg, fstmt, level);
		fstmt = fstmt->next;
	}
}

void BPrintStmtList(CG *cg, stmt *fstmt) {
	lBPrintStmtList(cg, fstmt, 0);
}

static void lBPrintStmt(CG *cg, stmt *fstmt, int level) {
	stmt *lstmt;

	switch (fstmt->kind) {
	case EXPR_STMT:
		if (fstmt->exprst.exp) {
			lBPrintExpression(cg, fstmt->exprst.exp, level);
		} else {
			cg->printf("/* empty statement */\n");
		}
		break;
	case IF_STMT:
		lIndent(cg, level);
		cg->printf("if\n");
		lBPrintExpression(cg, fstmt->ifst.cond, level + 1);
		lIndent(cg, level);
		cg->printf("then\n");
		lBPrintStmtList(cg, fstmt->ifst.thenstmt, level + 1);
		if (fstmt->ifst.elsestmt) {
			lIndent(cg, level);
			cg->printf("else\n");
			lBPrintStmtList(cg, fstmt->ifst.elsestmt, level + 1);
		}
		break;
	case WHILE_STMT:
		lIndent(cg, level);
		cg->printf("while\n");
		lBPrintExpression(cg, fstmt->whilest.cond, level + 1);
		lBPrintStmtList(cg, fstmt->whilest.body, level + 1);
		break;
	case DO_STMT:
		lIndent(cg, level);
		cg->printf("do\n");
		lBPrintStmtList(cg, fstmt->whilest.body, level + 1);
		lIndent(cg, level);
		cg->printf("while\n");
		lBPrintExpression(cg, fstmt->whilest.cond, level + 1);
		break;
	case FOR_STMT:
		lIndent(cg, level);
		cg->printf("for\n");
		lstmt = fstmt->forst.init;
		if (lstmt) {
			lBPrintStmtList(cg, fstmt->forst.init, level + 1);
		}
		cg->printf("for-cond\n");
		if (fstmt->forst.cond) {
			lBPrintExpression(cg, fstmt->forst.cond, level + 1);
		}
		cg->printf("for-step\n");
		lstmt = fstmt->forst.step;
		if (lstmt) {
			lBPrintStmtList(cg, fstmt->forst.step, level + 1);
		}
		cg->printf("for-body\n");
		lBPrintStmtList(cg, fstmt->forst.body, level + 1);
		break;
	case BLOCK_STMT:
		if (level > 1)
			lIndent(cg, level - 1);
		cg->printf("{\n");
		lBPrintStmtList(cg, fstmt->blockst.body, level);
		if (level > 1)
			lIndent(cg, level - 1);
		cg->printf("}\n");
		break;
	case RETURN_STMT:
		lIndent(cg, level);
		cg->printf("return\n");
		if (fstmt->returnst.exp) {
			lBPrintExpression(cg, fstmt->returnst.exp, level + 1);
		}
		break;
	case DISCARD_STMT:
		lIndent(cg, level);
		cg->printf("discard\n");
		if (fstmt->discardst.cond)
			lBPrintExpression(cg, fstmt->discardst.cond, level + 1);
		break;
	case BREAK_STMT:
		lIndent(cg, level);
		cg->printf("break\n");
		break;
	case COMMENT_STMT:
		lIndent(cg, level);
		cg->printf("// %s\n", cg->GetAtomString(fstmt->commentst.str));
		break;
	default:
		lIndent(cg, level);
		cg->printf("<!BadStmt-0x%2x>\n", fstmt->kind);
	}
}

void BPrintStmt(CG *cg, stmt *fstmt) {
	lBPrintStmt(cg, fstmt, 0);
}

// Build a printable string of a type.
// Arrays are shown as: "packed float[4]" instead of "float4".
void FormatTypeString(CG *cg, char *name, int size, char *name2, int size2, Type *fType) {
	int qualifiers, category, base;
	char tname[32];

	strcpy(name2, "");
	if (fType) {
		strcpy(name, "");

		base = GetBase(fType);

		qualifiers = GetQualifiers(fType);
		if (qualifiers & TYPE_QUALIFIER_CONST)
			strcat(name, "const ");
		if ((qualifiers & TYPE_QUALIFIER_INOUT) == TYPE_QUALIFIER_INOUT) {
			strcat(name, "inout ");
		} else {
			if (qualifiers & TYPE_QUALIFIER_IN)
				strcat(name, "in ");
			if (qualifiers & TYPE_QUALIFIER_OUT)
				strcat(name, "out ");
		}

		category = GetCategory(fType);
		switch (category) {
		case TYPE_CATEGORY_NONE:
			strcat(name, "<<category=NONE>>");
			break;
		case TYPE_CATEGORY_SCALAR:
			strcat(name, GetBaseTypeNameString(cg, base));
			break;
		case TYPE_CATEGORY_ARRAY:
			FormatTypeString(cg, name, size, name2, size2, fType->arr.eltype);
			sprintf(tname, "[%d]", fType->arr.numels);
			strcat(name2, tname);
			break;
		case TYPE_CATEGORY_FUNCTION:
			strcat(name, "FUNCTION");
			break;
		case TYPE_CATEGORY_STRUCT:
			strcat(name, "struct ");
			strcat(name, cg->GetAtomString(fType->str.tag));
			break;
		default:
			strcat(name, "<<bad-category>>");
			break;
		}
	} else {
		strcpy(name, "<<NULL>>");
	}
}

// Build a printable string of a type for export to the run-time.
// Arrays are shown as predefined typedefs: "float4" instead of "packed float[4]"
void FormatTypeStringRT(CG *cg, char *name, int size, char *name2, int size2, Type *fType, bool Unqualified) {
	int qualifiers, category, base;
	char tname[32];
	int len, len2;

	if (name2)
		strcpy(name2, "");
		
	if (fType) {
		strcpy(name, "");

		base = GetBase(fType);

		if (!Unqualified) {
			qualifiers = GetQualifiers(fType);
			if (qualifiers & TYPE_QUALIFIER_CONST)
				strcat(name, "const ");
			if ((qualifiers & TYPE_QUALIFIER_INOUT) == TYPE_QUALIFIER_INOUT) {
				strcat(name, "inout ");
			} else {
				if (qualifiers & TYPE_QUALIFIER_IN)
					strcat(name, "in ");
				if (qualifiers & TYPE_QUALIFIER_OUT)
					strcat(name, "out ");
			}
		}

		category = GetCategory(fType);
		switch (category) {
		case TYPE_CATEGORY_NONE:
			sprintf(name + strlen(name), "%i", fType->typname.index);
//			strcat(name, "<<category=NONE>>");
			break;
		case TYPE_CATEGORY_SCALAR:
			strcat(name, GetBaseTypeNameString(cg, base));
			break;
		case TYPE_CATEGORY_ARRAY:
			if (IsMatrix(fType, &len, &len2)) {
				strcat(name, GetBaseTypeNameString(cg, base));
				sprintf(tname, "%dx%d", len2, len);
				strcat(name, tname);
			} else if (IsVector(fType, &len)) {
				strcat(name, GetBaseTypeNameString(cg, base));
				tname[0] = '0' + len;
				tname[1] = '\0';
				strcat(name, tname);
			} else {
				FormatTypeStringRT(cg, name, size, name2, size2, fType->arr.eltype, Unqualified);
				sprintf(tname, "[%d]", fType->arr.numels);
				strcat(name2 ? name2 : name, tname);
			}
			break;
		case TYPE_CATEGORY_FUNCTION:
			strcat(name, "FUNCTION");
			break;
		case TYPE_CATEGORY_STRUCT:
			//strcat(name, "struct ");
			strcat(name, cg->GetAtomString(fType->str.tag));
			break;
		default:
			strcat(name, "<<bad-category>>");
			break;
		}
	} else {
		strcpy(name, "<<NULL>>");
	}
}

void PrintType(CG *cg, Type *fType, int level) {
	int base, category, qualifiers, domain;
	TypeList *lTypeList;

	if (fType) {
		qualifiers = GetQualifiers(fType);
		if (qualifiers & TYPE_QUALIFIER_CONST)
			cg->printf("const ");
		if (qualifiers & TYPE_QUALIFIER_IN)
			cg->printf("in ");
		if (qualifiers & TYPE_QUALIFIER_OUT)
			cg->printf("out ");

		domain = GetDomain(fType);
		switch (domain) {
		case TYPE_DOMAIN_UNKNOWN:
			break;
		case TYPE_DOMAIN_UNIFORM:
			cg->printf("uniform ");
			break;
		case TYPE_DOMAIN_VARYING:
			cg->printf("varying ");
			break;
		default:
			cg->printf("<<domain=%02x>>", domain >> TYPE_DOMAIN_SHIFT);
			break;
		}

		category = GetCategory(fType);
		switch (category) {
		case TYPE_CATEGORY_NONE:
			cg->printf("<<category=NONE>>");
			break;
		case TYPE_CATEGORY_SCALAR:
			base = GetBase(fType);
			cg->printf("%s", GetBaseTypeNameString(cg, base));
			break;
		case TYPE_CATEGORY_ARRAY:
			PrintType(cg, fType->arr.eltype, level);
			cg->printf("[%d]", fType->arr.numels);
			break;
		case TYPE_CATEGORY_FUNCTION:
			cg->printf("(");
			lTypeList = fType->fun.paramtypes;
			while (lTypeList) {
				PrintType(cg, lTypeList->p, level);
				lTypeList = lTypeList->next;
				if (lTypeList)
					cg->printf(", ");
			}
			cg->printf(")");
			break;
		case TYPE_CATEGORY_STRUCT:
			if (fType->str.tag) {
				cg->printf("struct %s", cg->GetAtomString(fType->str.tag));
			} else {
				cg->printf("struct");
			}
			break;
		default:
			cg->printf("<<category=%02x>>", category >> TYPE_CATEGORY_SHIFT);
			break;
		}
		//cg->printf(" ");
	} else {
		cg->printf("<<NULL-TYPE>>");
	}
}

void PrintSymbolTree(CG *cg, Symbol *fSymb) {
	Symbol *lSymb;
	int DoType;

	if (fSymb) {
		DoType = 1;
		PrintSymbolTree(cg, fSymb->left);
		switch (fSymb->kind) {
		case TYPEDEF_S:
			cg->printf("TYP: %s : %d:%d", cg->GetAtomString(fSymb->name), cg->hal->GetSizeof(fSymb->type), cg->hal->GetAlignment(fSymb->type));
			break;
		case VARIABLE_S:
			if (fSymb->properties & SYMB_IS_PARAMETER) {
				cg->printf("PAR: %s ", cg->GetAtomString(fSymb->name));
			} else {
				cg->printf("VAR: %s ", cg->GetAtomString(fSymb->name));
			}
			break;
		case CONSTANT_S:
			cg->printf("CON: %s ", cg->GetAtomString(fSymb->name));
			break;
		case FUNCTION_S:
			lSymb = fSymb;
			while (lSymb) {
				if (lSymb == fSymb) {
					cg->printf("FUN");
				} else {
					cg->printf("   ");
				}
				cg->printf(": %s ", cg->GetAtomString(fSymb->name));
				PrintType(cg, lSymb->type, 0);
				if (!(lSymb->properties & SYMB_IS_DEFINED))
					cg->printf(" UNDEFINED");
				if (lSymb->properties & SYMB_IS_BUILTIN)
					cg->printf(" BUILTIN");
				if (lSymb->properties & SYMB_IS_INLINE_FUNCTION)
					cg->printf(" INLINE");
				cg->printf("\n");
				lSymb = lSymb->fun.overload;
			}
			DoType = 0;
			break;
		default:
			cg->printf("???%04x???: %s ", fSymb->kind, cg->GetAtomString(fSymb->name));
			break;
		}
		if (DoType) {
			PrintType(cg, fSymb->type, 0);
			cg->printf("\n");
		}
		PrintSymbolTree(cg, fSymb->right);
	}
}

void PrintScopeDeclarations(CG *cg) {
	cg->printf("*** Scope %d definitions: ***\n", cg->current_scope->level);
	PrintSymbolTree(cg, cg->current_scope->symbols);
	cg->printf("*** End of Scope %d ***\n\n", cg->current_scope->level);
}

void lPrintExpr(CG *cg, expr *fexpr) {
	int ii, mask, len;
	unsigned int uval;
	char s[17], tag;

	switch (fexpr->kind) {
	case SYMB_N:
		cg->printf(cg->GetAtomString(fexpr->sym.symbol->name));
		break;
	case CONST_N:
		switch (fexpr->op) {
		case ICONST_OP:
			cg->printf("%d", fexpr->co.val[0].i);
			break;
		case ICONST_V_OP:
			cg->printf("{ %d", fexpr->co.val[0].i);
			len = SUBOP_GET_S(fexpr->co.subop);
			for (ii = 1; ii < len; ii++)
				cg->printf(", %d", fexpr->co.val[ii].i);
			cg->printf(" }");
			break;
		case UCONST_OP:
			cg->printf("%uu", fexpr->co.val[0].i);
			break;
		case UCONST_V_OP:
			cg->printf("{ %uu", fexpr->co.val[0].i);
			len = SUBOP_GET_S(fexpr->co.subop);
			for (ii = 1; ii < len; ii++)
				cg->printf(", %uu", fexpr->co.val[ii].i);
			cg->printf(" }");
			break;
		case BCONST_OP:
			if (fexpr->co.val[0].i == 0) {
				cg->printf("false");
			} else if (fexpr->co.val[0].i == 1) {
				cg->printf("true");
			} else {
				cg->printf("<<BBCONST=%d>>", fexpr->co.val[0].i);
			}
			break;
		case BCONST_V_OP:
			cg->printf("{ ");
			len = SUBOP_GET_S(fexpr->co.subop);
			for (ii = 0; ii < len; ii++)
				if (ii) cg->printf(", ");
				if (fexpr->co.val[ii].i == 0) {
					cg->printf("false");
				} else if (fexpr->co.val[ii].i == 1) {
					cg->printf("true");
				} else {
					cg->printf("<<BBCONST=%d>>", fexpr->co.val[ii].i);
				}
			cg->printf(" }");
			break;
		case FCONST_OP:
			cg->printf("%.6gf", fexpr->co.val[0].f);
			break;
		case HCONST_OP:
			cg->printf("%.6gh", fexpr->co.val[0].f);
			break;
		case XCONST_OP:
			cg->printf("%.6gx", fexpr->co.val[0].f);
			break;
		case FCONST_V_OP:
			tag = 'f';
			goto floatvec;
		case HCONST_V_OP:
			tag = 'h';
			goto floatvec;
		case XCONST_V_OP:
			tag = 'x';
		floatvec:
			cg->printf("{ %.6g%c", fexpr->co.val[0].f, tag);
			len = SUBOP_GET_S(fexpr->co.subop);
			for (ii = 1; ii < len; ii++)
				cg->printf(", %.6g%c", fexpr->co.val[ii].f, tag);
			cg->printf(" }");
			break;
		}
		break;
	case UNARY_N:
		switch (fexpr->op) {
		case CAST_CS_OP:
			cg->printf("(%s) ", GetBaseTypeNameString(cg, SUBOP_GET_T2(fexpr->un.subop)));
			break;
		case CAST_CV_OP:
			cg->printf("(%s [%d]) ",
				   GetBaseTypeNameString(cg, SUBOP_GET_T2(fexpr->un.subop)),
				   SUBOP_GET_S1(fexpr->un.subop));
			break;
		case CAST_CM_OP:
			cg->printf("(%s [%d][%d]) ",
				   GetBaseTypeNameString(cg, SUBOP_GET_T2(fexpr->un.subop)),
				   SUBOP_GET_S2(fexpr->un.subop), SUBOP_GET_S1(fexpr->un.subop));
			break;
		case VECTOR_V_OP:
		case MATRIX_M_OP:
			cg->printf("{ ");
			break;
		case NEG_OP:
		case NEG_V_OP:
			cg->printf("-");
			break;
		case POS_OP:
		case POS_V_OP:
			cg->printf("+");
			break;
		case NOT_OP:
		case NOT_V_OP:
			cg->printf("~");
			break;
		case BNOT_OP:
		case BNOT_V_OP:
			cg->printf("!");
			break;
		case SWIZZLE_Z_OP:
		case SWIZMAT_Z_OP:
			break;
		case PREDEC_OP:
			cg->printf("--");
			break;
		case PREINC_OP:
			cg->printf("++");
			break;
		}
		if (fexpr->un.arg)
			lPrintExpr(cg, fexpr->un.arg);
		switch (fexpr->op) {
		case SWIZZLE_Z_OP:
			mask = SUBOP_GET_MASK(fexpr->un.subop);
			ii = SUBOP_GET_S2(fexpr->un.subop);
			if (ii == 0)
				ii = 1; // var.x is scalar, not array[1]
			s[ii] = '\0';
			while (ii > 0) {
				ii--;
				s[ii] = "xyzw"[(mask >> ii*2) & 3];
			}
			cg->printf(".%s", s);
			break;
		case SWIZMAT_Z_OP:
			mask = SUBOP_GET_MASK16(fexpr->un.subop);
			ii = SUBOP_GET_SR(fexpr->un.subop);
			if (ii == 0)
				ii = 1; // var.x is scalar, not array[1]
			s[ii*4] = '\0';
			while (ii > 0) {
				ii--;
				uval = (mask >> ii*4) & 15;
				s[ii*4 + 0] = '_';
				s[ii*4 + 1] = 'm';
				s[ii*4 + 2] = '0' + ((uval >> 2) & 3);
				s[ii*4 + 3] = '0' + (uval & 3);
			}
			cg->printf(".%s", s);
			break;
		case VECTOR_V_OP:
		case MATRIX_M_OP:
			cg->printf(" }");
			break;
		case POSTDEC_OP:
			cg->printf("--");
			break;
		case POSTINC_OP:
			cg->printf("++");
			break;
		}
		break;
	case BINARY_N:
		lPrintExpr(cg, fexpr->bin.left);
		switch (fexpr->op) {
		case MEMBER_SELECTOR_OP:
			cg->printf(".");
			break;
		case ARRAY_INDEX_OP:
			cg->printf("[");
			break;
		case FUN_CALL_OP:
		case FUN_BUILTIN_OP:
			cg->printf("(");
			break;
		case FUN_ARG_OP:
		case EXPR_LIST_OP:
			if (fexpr->bin.right)
				cg->printf(", ");
			break;
		case MUL_OP:
		case MUL_SV_OP:
		case MUL_VS_OP:
		case MUL_V_OP:
			cg->printf("*");
			break;
		case DIV_OP:
		case DIV_SV_OP:
		case DIV_VS_OP:
		case DIV_V_OP:
			cg->printf("/");
			break;
		case MOD_OP:
		case MOD_SV_OP:
		case MOD_VS_OP:
		case MOD_V_OP:
			cg->printf("%");
			break;
		case ADD_OP:
		case ADD_SV_OP:
		case ADD_VS_OP:
		case ADD_V_OP:
			cg->printf(" + ");
			break;
		case SUB_OP:
		case SUB_SV_OP:
		case SUB_VS_OP:
		case SUB_V_OP:
			cg->printf(" - ");
			break;
		case SHL_OP:
		case SHL_V_OP:
			cg->printf(" << ");
			break;
		case SHR_OP:
		case SHR_V_OP:
			cg->printf(" >> ");
			break;
		case LT_OP:
		case LT_SV_OP:
		case LT_VS_OP:
		case LT_V_OP:
			cg->printf(" < ");
			break;
		case GT_OP:
		case GT_SV_OP:
		case GT_VS_OP:
		case GT_V_OP:
			cg->printf(" > ");
			break;
		case LE_OP:
		case LE_SV_OP:
		case LE_VS_OP:
		case LE_V_OP:
			cg->printf(" <= ");
			break;
		case GE_OP:
		case GE_SV_OP:
		case GE_VS_OP:
		case GE_V_OP:
			cg->printf(" >= ");
			break;
		case EQ_OP:
		case EQ_SV_OP:
		case EQ_VS_OP:
		case EQ_V_OP:
			cg->printf(" == ");
			break;
		case NE_OP:
		case NE_SV_OP:
		case NE_VS_OP:
		case NE_V_OP:
			cg->printf(" != ");
			break;
		case AND_OP:
		case AND_SV_OP:
		case AND_VS_OP:
		case AND_V_OP:
			cg->printf(" & ");
			break;
		case XOR_OP:
		case XOR_SV_OP:
		case XOR_VS_OP:
		case XOR_V_OP:
			cg->printf(" ^ ");
			break;
		case OR_OP:
		case OR_SV_OP:
		case OR_VS_OP:
		case OR_V_OP:
			cg->printf(" | ");
			break;
		case BAND_OP:
		case BAND_SV_OP:
		case BAND_VS_OP:
		case BAND_V_OP:
			cg->printf(" && ");
			break;
		case BOR_OP:
		case BOR_SV_OP:
		case BOR_VS_OP:
		case BOR_V_OP:
			cg->printf(" || ");
			break;
		case ASSIGN_OP:
		case ASSIGN_V_OP:
		case ASSIGN_GEN_OP:
			cg->printf(" = ");
			break;
		case ASSIGNMINUS_OP:
			cg->printf(" -= ");
			break;
		case ASSIGNMOD_OP:
			cg->printf(" %= ");
			break;
		case ASSIGNPLUS_OP:
			cg->printf(" += ");
			break;
		case ASSIGNSLASH_OP:
			cg->printf(" /= ");
			break;
		case ASSIGNSTAR_OP:
			cg->printf(" *= ");
			break;
		case ASSIGNAND_OP:
			cg->printf(" &= ");
			break;
		case ASSIGNOR_OP:
			cg->printf(" |= ");
			break;
		case ASSIGNXOR_OP:
			cg->printf(" ^= ");
			break;
		case ASSIGN_MASKED_KV_OP:
			cg->printf("@@");
			mask = SUBOP_GET_MASK(fexpr->bin.subop);
			for (ii = 3; ii >= 0; ii--) {
				if (mask & 1)
					cg->printf("%c", "wzyx"[ii]);
				mask >>= 1;
			}
			cg->printf(" = ");
			break;
		case COMMA_OP:
			cg->printf(" , ");
			break;
		default:
			cg->printf("<!BINOP=%d>", fexpr->op);
			break;
		}
		if (fexpr->bin.right)
			lPrintExpr(cg, fexpr->bin.right);
		switch (fexpr->op) {
		case ARRAY_INDEX_OP:
			cg->printf("]");
			break;
		case FUN_CALL_OP:
		case FUN_BUILTIN_OP:
			cg->printf(")");
			break;
		default:
			break;
		}
		break;
	case TRINARY_N:
		lPrintExpr(cg, fexpr->tri.arg1);
		switch (fexpr->op) {
		case COND_OP:
		case COND_V_OP:
		case COND_SV_OP:
		case COND_GEN_OP:
			cg->printf(" ? ");
			if (fexpr->tri.arg2)
				lPrintExpr(cg, fexpr->tri.arg2);
			cg->printf(" : ");
			if (fexpr->tri.arg3)
				lPrintExpr(cg, fexpr->tri.arg3);
			break;
		case ASSIGN_COND_OP:
		case ASSIGN_COND_V_OP:
		case ASSIGN_COND_SV_OP:
		case ASSIGN_COND_GEN_OP:
			cg->printf("@@(");
			if (fexpr->tri.arg2)
				lPrintExpr(cg, fexpr->tri.arg2);
			cg->printf(") = ");
			if (fexpr->tri.arg3)
				lPrintExpr(cg, fexpr->tri.arg3);
			break;
		default:
			cg->printf("<!TRIOP=%d>", fexpr->op);
			break;
		}
		break;
	default:
		cg->printf("<!NODEKIND=%d>", fexpr->kind);
		break;
	}
}

void PrintExpression(CG *cg, expr *fexpr) {
	cg->printf("expr: ");
	lPrintExpr(cg, fexpr);
	cg->printf("\n");
}

static void lPrintStmt(CG *cg, stmt *fstmt, int level, const char *fcomment);

static void lPrintStmtList(CG *cg, stmt *fstmt, int level, const char *fcomment) {
	while (fstmt) {
		lPrintStmt(cg, fstmt, level, fcomment);
		fstmt = fstmt->next;
	}
}

static void lPrintStmt(CG *cg, stmt *fstmt, int level, const char *fcomment) {
	stmt *lstmt;

	switch (fstmt->kind) {
	case EXPR_STMT:
		lIndent(cg, level);
		if (fstmt->exprst.exp) {
			lPrintExpr(cg, fstmt->exprst.exp);
		} else {
			cg->printf("/* empty statement */");
		}
		cg->printf(";");
		break;
	case IF_STMT:
		lIndent(cg, level);
		cg->printf("if (");
		lPrintExpr(cg, fstmt->ifst.cond);
		cg->printf(")");
		lPrintStmtList(cg, fstmt->ifst.thenstmt, level + 1, NULL);
		if (fstmt->ifst.elsestmt) {
			lIndent(cg, level);
			cg->printf("else");
			lPrintStmtList(cg, fstmt->ifst.elsestmt, level + 1, NULL);
		}
		break;
	case WHILE_STMT:
		lIndent(cg, level);
		cg->printf("while (");
		lPrintExpr(cg, fstmt->whilest.cond);
		cg->printf(")");
		lPrintStmtList(cg, fstmt->whilest.body, level + 1, NULL);
		break;
	case DO_STMT:
		lIndent(cg, level);
		cg->printf("do");
		lPrintStmtList(cg, fstmt->whilest.body, level + 1, NULL);
		lIndent(cg, level);
		cg->printf("while (");
		lPrintExpr(cg, fstmt->whilest.cond);
		cg->printf(");");
		break;
	case FOR_STMT:
		lIndent(cg, level);
		cg->printf("for (");
		lstmt = fstmt->forst.init;
		if (lstmt) {
			while (lstmt) {
				if (lstmt->kind == EXPR_STMT) {
					lPrintExpr(cg, lstmt->exprst.exp);
				} else {
					cg->printf("*** BAD STMT KIND ***");
				}
				if (lstmt->next)
					cg->printf(", ");
				lstmt = lstmt->next;
			}
		}
		cg->printf(";");
		if (fstmt->forst.cond) {
			cg->printf(" ");
			lPrintExpr(cg, fstmt->forst.cond);
		}
		cg->printf(";");
		lstmt = fstmt->forst.step;
		if (lstmt) {
			cg->printf(" ");
			while (lstmt) {
				if (lstmt->kind == EXPR_STMT) {
					lPrintExpr(cg, lstmt->exprst.exp);
				} else {
					cg->printf("*** BAD STMT KIND ***");
				}
				if (lstmt->next)
					cg->printf(", ");
				lstmt = lstmt->next;
			}
		}
		cg->printf(")");
		lPrintStmtList(cg, fstmt->forst.body, level + 1, NULL);
		break;
	case BLOCK_STMT:
		cg->printf("{\n");
		lPrintStmtList(cg, fstmt->blockst.body, level, NULL);
		lIndent(cg, level - 1);
		if (fcomment)
			cg->printf("} // %s", fcomment);
		else
			cg->printf("}");
		break;
	case RETURN_STMT:
		lIndent(cg, level);
		cg->printf("return");
		if (fstmt->returnst.exp) {
			cg->printf(" ");
			lPrintExpr(cg, fstmt->returnst.exp);
		}
		cg->printf(";");
		break;
	case DISCARD_STMT:
		lIndent(cg, level);
		cg->printf("discard");
		if (fstmt->discardst.cond) {
			cg->printf(" ");
			lPrintExpr(cg, fstmt->discardst.cond);
		}
		cg->printf(";");
		break;
	case BREAK_STMT:
		lIndent(cg, level);
		cg->printf("break;");
		break;
	case COMMENT_STMT:
		lIndent(cg, level);
		cg->printf("// %s", cg->GetAtomString(fstmt->commentst.str));
		break;
	default:
		lIndent(cg, level);
		cg->printf("<!BadStmt-0x%2x>", fstmt->kind);
	}
}

void PrintStmt(CG *cg, stmt *fstmt) {
	lPrintStmt(cg, fstmt, 0, NULL);
}

void PrintStmtList(CG *cg, stmt *fstmt) {
	lPrintStmtList(cg, fstmt, 0, NULL);
}

void PrintFunction(CG *cg, Symbol *symb) {
	const char *sname, *pname;
	char tname[100], uname[100];
	Symbol *params;

	if (symb) {
		sname = cg->GetAtomString(symb->name);
		if (symb->kind == FUNCTION_S) {
			if (symb->type) {
				FormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, symb->type->fun.rettype);
			} else {
				strcpy(tname, "NULL");
			}
			cg->printf("%s %s%s(",tname, sname, uname);
			params = symb->fun.params;
			while (params) {
				pname = cg->GetAtomString(params->name);
				FormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, params->type);
				cg->printf("%s %s%s",tname, pname, uname);
				params = params->next;
				if (params)
					cg->printf(", ");
			}
			cg->printf(")");
			if (symb->fun.semantics)
				cg->printf(" : %s", cg->GetAtomString(symb->fun.semantics));
			cg->printf("\n{\n");
			lPrintStmtList(cg, symb->fun.statements, 1, "function");
			cg->printf("}\n");
		} else {
			cg->printf("PrintFunction: Symbol \"%s\" not a function\n", sname);
		}
	} else {
		cg->printf("<<NULL-Function-Symbol>>\n");
	}
}

void BPrintFunction(CG *cg, Symbol *symb) {
	const char *sname;

	if (symb) {
		sname = cg->GetAtomString(symb->name);
		if (symb->kind == FUNCTION_S) {
			cg->printf("{\n");
			lBPrintStmtList(cg, symb->fun.statements, 0);
			cg->printf("}\n");
		} else {
			cg->printf("BPrintFunction: Symbol \"%s\" not a function\n", sname);
		}
	} else {
		cg->printf("<<NULL-Function-Symbol>>\n");
	}
}

} //namespace cgc
