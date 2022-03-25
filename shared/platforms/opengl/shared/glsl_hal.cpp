#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgc/cg.h"
#include "cgc/generic_hal.h"
#include "cgc/compile.h"

namespace cgc {

#define GLSL_VER	100

void GLSLIndent(CG *cg, int level) {
	while (level--)
		cg->outputf("\t");
}

void GLSLFormatTypeString(CG *cg, char *name, int size, char *name2, int size2, Type *fType, int Unqualified) {
	int qualifiers, category, base;
	char tname[32];
	int len, len2;

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

		if (fType->properties & (TYPE_MISC_PRECISION * 3)) {
			static const char *precisions[] = {"", "lowp ", "mediump ", "highp "};
			int		p = (fType->properties & (TYPE_MISC_PRECISION * 3)) / TYPE_MISC_PRECISION;
			strcat(name, precisions[p]);
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
			if (IsMatrix(fType, &len, &len2) && base == TYPE_BASE_FLOAT) {
#if GLSL_VER < 110
				sprintf(tname, "mat%d", len > len2 ? len : len2);
#else
				if (len == len2)
					sprintf(tname, "mat%d", len);
				else
					sprintf(tname, "mat%dx%d", len2, len);
#endif
				strcat(name, tname);
			} else if (IsVector(fType, &len) && (base == TYPE_BASE_FLOAT || base == TYPE_BASE_INT || base == TYPE_BASE_BOOLEAN)) {
				sprintf(tname, base == TYPE_BASE_FLOAT ? "vec%d" : base == TYPE_BASE_INT ? "ivec%d" : "bvec%d", len);
				strcat(name, tname);
			} else {
				GLSLFormatTypeString(cg, name, size, name2, size2, fType->arr.eltype, Unqualified);
				sprintf(tname, "[%d]", fType->arr.numels);
				strcat(name2, tname);
			}
			break;
		case TYPE_CATEGORY_FUNCTION:
			strcat(name, "FUNCTION");
			break;
		case TYPE_CATEGORY_STRUCT:
//			strcat(name, "struct ");
			strcat(name, cg->GetAtomString(fType->str.tag));
			break;
		case TYPE_CATEGORY_TEXOBJ: {
			static const char *sampler_names[] = {
				"sampler2D",
				"sampler1D",
				"sampler2D",
				"sampler2DMS",
				"sampler3D",
				"samplerCube",
				"samplerRect",
				"sampler2DShadow",
				"buffer",
				"byte",
				"structured",
				"append",
				"consume",
				"inputpatch",
				"outputpatch",
			};
			strcat(name, sampler_names[fType->tex.dims & 15]);
			break;
		}
		default:
			strcat(name, "<<bad-category>>");
			break;
		}
	} else {
		strcpy(name, "<<NULL>>");
	}
}

struct GLSLParen {
	bool	put;
	CG		*cg;
	GLSLParen(CG *_cg, int level0, int level1) : cg(_cg) {
		if (put = level1 > level0)
			cg->outputf("(");
	}
	~GLSLParen() {
		if (put)
			cg->outputf(")");
	}
};

char *GLSLGetSwizzle(char *s, int mask, int len, int stride) {
	if (len == 0)
		len = 1; // var.x is scalar, not array[1]
	s[len] = '\0';
	while (len--)
		s[len] = "xyzw"[(mask >> len * stride) & 3];
	return s;
}

inline bool is_int(float f) { return f == int(f); }

void GLSLPrintExpr(CG *cg, expr *fexpr, int level0 = 1000);

void GLSL_infix(CG *cg, expr *left, expr *right, const char *op, int level0, int level1) {
	GLSLParen	paren(cg, level0, level1);
	GLSLPrintExpr(cg, left, level1);
	cg->outputf(op);
	GLSLPrintExpr(cg, right, level1);
}
void GLSL_infix(CG *cg, expr *bin, const char *op, int level0, int level1) {
	GLSL_infix(cg, bin->bin.left, bin->bin.right, op, level0, level1);
}

void GLSL_vecrel(CG *cg, expr *bin, const char *fn, int sv) {
	cg->outputf("%s(", fn);

#if 0
	GLSLPrintExpr(cg, bin->left);
	cg->outputf(", ");
	GLSLPrintExpr(cg, bin->right);
#else
	if (sv == OFFSET_SV_OP) {
		char tname[100], uname[100];
		GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, bin->bin.right->type, 0);
		cg->outputf("%s(",tname);
		GLSLPrintExpr(cg, bin->bin.left);
		cg->outputf(")");
	} else {
		GLSLPrintExpr(cg, bin->bin.left);
	}
	cg->outputf(", ");

	if (sv == OFFSET_VS_OP) {
		char tname[100], uname[100];
		GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, bin->bin.left->type, 0);
		cg->outputf("%s(",tname);
		GLSLPrintExpr(cg, bin->bin.right);
		cg->outputf(")");
	} else {
		GLSLPrintExpr(cg, bin->bin.right);
	}
#endif
	cg->outputf(")");
}

void GLSLPrintExpr(CG *cg, expr *fexpr, int level0) {
	int ii, mask, len, len2;
	char s[17];

	switch (fexpr->kind) {
	case SYMB_N: {
		const char *name = cg->GetAtomString(fexpr->sym.symbol->name);
		if (strcmp(name, "frac") == 0)
			cg->outputf("fract");
		else if (strcmp(name, "rsqrt") == 0)
			cg->outputf("inversesqrt");
		else if (strcmp(name, "lerp") == 0)
			cg->outputf("mix");
		else if (strcmp(name, "ddx") == 0)
			cg->outputf("dFdx");
		else if (strcmp(name, "ddy") == 0)
			cg->outputf("dFdy");
		else if (strncmp(name, "tex2D", 5) == 0)
			cg->outputf("texture2D%s", name + 5);
		else if (strncmp(name, "texCUBE", 7) == 0)
			cg->outputf("textureCube%s", name + 7);
		else if (strncmp(name, "shadow2D", 8) == 0)
			cg->outputf("shadow2D%sEXT", name + 8);
		else
			cg->outputf("%s", name);
		break;
	}
	case CONST_N:
		switch (fexpr->op) {
		case ICONST_OP:
			cg->outputf("%d", fexpr->co.val[0].i);
			break;
		case UCONST_OP:
			cg->outputf("%uu", fexpr->co.val[0].i);
			break;
		case ICONST_V_OP:
			len = SUBOP_GET_S(fexpr->co.subop);
			cg->outputf("ivec%i(", len);
			for (ii = 0; ii < len; ii++) {
				if (ii) cg->outputf(", ");
				cg->outputf("%d", fexpr->co.val[ii].i);
			}
			cg->outputf(")");
			break;
		case BCONST_OP:
			cg->outputf(fexpr->co.val[0].i == 0 ? "false" : "true");
			break;
		case BCONST_V_OP:
			len = SUBOP_GET_S(fexpr->co.subop);
			cg->outputf("bvec%i(", len);
			for (ii = 0; ii < len; ii++) {
				if (ii) cg->outputf(", ");
				cg->outputf(fexpr->co.val[ii].i == 0 ? "false" : "true");
			}
			cg->outputf(")");
			break;
		case FCONST_OP:
		case HCONST_OP:
		case XCONST_OP:
			cg->outputf(is_int(fexpr->co.val[0].f) ? "%.1f" : "%g", fexpr->co.val[0].f);
			break;
		case FCONST_V_OP:
		case HCONST_V_OP:
		case XCONST_V_OP:
			len = SUBOP_GET_S(fexpr->co.subop);
			cg->outputf("vec%i(", len);
			for (ii = 0; ii < len; ii++) {
				if (ii) cg->outputf(", ");
				cg->outputf(is_int(fexpr->co.val[ii].f) ? "%.1f" : "%g", fexpr->co.val[ii].f);
			}
			cg->outputf(")");
			break;
		}
		break;
	case UNARY_N: {
		int	level = 3;
		switch (fexpr->op) {
			case CAST_CS_OP: {
				int	t1 = SUBOP_GET_T1(fexpr->un.subop), t2 = SUBOP_GET_T2(fexpr->un.subop);
				if (t1 == t2 || (t1 == TYPE_BASE_CFLOAT && t2 == TYPE_BASE_FLOAT) || (t1 == TYPE_BASE_CINT && t2 == TYPE_BASE_INT)) {
					GLSLPrintExpr(cg, fexpr->un.arg, level);
				} else {
					cg->outputf("%s(", GetBaseTypeNameString(cg, SUBOP_GET_T2(fexpr->un.subop)));
					GLSLPrintExpr(cg, fexpr->un.arg, level);
					cg->outputf(")");
				}
				return;
			}
			case CAST_CV_OP: {
				int	base = SUBOP_GET_T2(fexpr->un.subop);
				cg->outputf("%svec%d(", base == TYPE_BASE_FLOAT ? "" : base == TYPE_BASE_INT ? "i" : "b", SUBOP_GET_S1(fexpr->un.subop));
				GLSLPrintExpr(cg, fexpr->un.arg);
				cg->outputf(")");
				return;
			}
			case VECTOR_V_OP: {
				int	base = SUBOP_GET_T1(fexpr->un.subop);
				cg->outputf("%svec%d(", base == TYPE_BASE_FLOAT ? "" : base == TYPE_BASE_INT ? "i" : "b", SUBOP_GET_S1(fexpr->un.subop));
				GLSLPrintExpr(cg, fexpr->un.arg);
				cg->outputf(")");
				return;
			}
			case CAST_CM_OP:
			case MATRIX_M_OP: {
				len		= SUBOP_GET_S1(fexpr->un.subop);
				len2	= SUBOP_GET_S2(fexpr->un.subop);
#if GLSL_VER < 110
				cg->outputf("mat%d(", len > len2 ? len : len2);
#else
				if (len1 == len2)
					cg->outputf("mat%d(", len);
				else
					cg->outputf("mat%dx%d(", len2, len);
#endif
				GLSLPrintExpr(cg, fexpr->un.arg);
				cg->outputf(")");
				return;
			}
			case NEG_OP:
			case NEG_V_OP:
				cg->outputf("-");
				break;
			case POS_OP:
			case POS_V_OP:
				cg->outputf("+");
				break;
			case NOT_OP:
			case NOT_V_OP:
				cg->outputf("~");
				break;
			case BNOT_OP:
			case BNOT_V_OP:
				cg->outputf("!");
				break;
			case PREDEC_OP:
				cg->outputf("--");
				break;
			case PREINC_OP:
				cg->outputf("++");
				break;
			case POSTDEC_OP:
			case POSTINC_OP:
				level	= 2;
				break;
			case SWIZZLE_Z_OP: {
				if (SUBOP_GET_S1(fexpr->un.subop) == 0) {
					cg->outputf("vec%d(", SUBOP_GET_S2(fexpr->un.subop));
					GLSLPrintExpr(cg, fexpr->un.arg, 2);
					cg->outputf(")");
				} else {
					GLSLParen	paren(cg, level0, 2);
					GLSLPrintExpr(cg, fexpr->un.arg, 2);
					cg->outputf(".%s", GLSLGetSwizzle(s, SUBOP_GET_MASK(fexpr->un.subop), SUBOP_GET_S2(fexpr->un.subop), 2));
				}
				return;
			}
			case SWIZMAT_Z_OP: {
				GLSLParen	paren(cg, level0, 2);
				mask = SUBOP_GET_MASK16(fexpr->un.subop);
				len = SUBOP_GET_SR(fexpr->un.subop);
				if (len == 0)
					len = 1; // var.x is scalar, not array[1]
				if (AllSameCol(mask, len)) {
					GLSLPrintExpr(cg, fexpr->un.arg, 2);
					cg->outputf("[%i]", mask & 3);
					cg->outputf(".%s", GLSLGetSwizzle(s, mask >> 2, len, 4));
				} else {
					cg->outputf("vec%i(", len);
					for (int i = 1; len; i++) {
						if (i == len || !AllSameCol(mask, i + 1)) {
							len	-= i;
							GLSLPrintExpr(cg, fexpr->un.arg, 2);
							cg->outputf("[%i]", mask & 3);
							cg->outputf(".%s%s", GLSLGetSwizzle(s, mask >> 2, i, 4), len ? ", " : ")");
							mask >>= (i * 4);
							i	= 0;
						}
					}
				}
				return;
			}
		}
		if (fexpr->un.arg) {
			GLSLParen	paren(cg, level0, level);
			GLSLPrintExpr(cg, fexpr->un.arg, level);
		}
		switch (fexpr->op) {
			case POSTDEC_OP:
				cg->outputf("--");
				break;
			case POSTINC_OP:
				cg->outputf("++");
				break;
		}
		break;
	}

	case BINARY_N:
		switch (fexpr->op) {
			case ARRAY_INDEX_OP: {
				GLSLParen	paren(cg, level0, 2);
				GLSLPrintExpr(cg, fexpr->bin.left, 2);
				cg->outputf("[");
				GLSLPrintExpr(cg, fexpr->bin.right);
				cg->outputf("]");
				break;
			}
			case FUN_CALL_OP:
			case FUN_BUILTIN_OP:
				if (fexpr->bin.left->kind == SYMB_N && strcmp(cg->GetAtomString(fexpr->bin.left->sym.symbol->loc.file), "<stdlib>") == 0) {
					const char *fname = cg->GetAtomString(fexpr->bin.left->sym.symbol->name);
					if (strcmp(fname, "atan2") == 0) {
						cg->outputf("atan(");
						GLSLPrintExpr(cg, fexpr->bin.right->bin.right);
						cg->outputf(", ");
						GLSLPrintExpr(cg, fexpr->bin.right->bin.left);
						cg->outputf(")");
						break;
					}
					if (strcmp(fname, "mul") == 0) {
						fexpr = fexpr->bin.right;
						expr	*e1	= fexpr->bin.left;
						fexpr = fexpr->bin.right;
						expr	*e2	= fexpr->bin.left;
#if GLSL_VER < 110
						if (IsMatrix(e2->type, &len, &len2) && len != len2) {
							if (len < len2) {	// shorten result
								cg->outputf("vec%d(", len);
								GLSLPrintExpr(cg, e2, 4);
								cg->outputf(" * ");
								GLSLPrintExpr(cg, e1, 4);
								cg->outputf(")");
							} else {			// lengthen input
								GLSLPrintExpr(cg, e2, 4);
								cg->outputf(" * ");
								cg->outputf("vec%d(", len);
								GLSLPrintExpr(cg, e1);
								while (len2++ < len)
									cg->outputf(",0");
								cg->outputf(")");
							}
						} else
#endif
						GLSL_infix(cg, e2, e1, " * ", level0, 4);
						return;
					}
				}
				GLSLPrintExpr(cg, fexpr->bin.left, 2);
				cg->outputf("(");
				if (fexpr->bin.right)
					GLSLPrintExpr(cg, fexpr->bin.right);
				cg->outputf(")");
				break;

			case FUN_ARG_OP:
			case EXPR_LIST_OP:
				GLSLPrintExpr(cg, fexpr->bin.left);
				if (fexpr->bin.right) {
					cg->outputf(", ");
					GLSLPrintExpr(cg, fexpr->bin.right);
				}
				break;

			case ASSIGN_MASKED_KV_OP:
				GLSLPrintExpr(cg, fexpr->bin.left, 16);
				cg->outputf("@@");
				mask = SUBOP_GET_MASK(fexpr->bin.subop);
				for (ii = 3; ii >= 0; ii--) {
					if (mask & 1)
						cg->outputf("%c", "wzyx"[ii]);
					mask >>= 1;
				}
				cg->outputf(" = ");
				GLSLPrintExpr(cg, fexpr->bin.right, 16);
				break;

			case MEMBER_SELECTOR_OP:
				GLSL_infix(cg, fexpr, ".", level0, 2);
				break;
			case MUL_OP:case MUL_SV_OP:case MUL_VS_OP:case MUL_V_OP:
				GLSL_infix(cg, fexpr, " * ", level0, 4);
				break;
			case DIV_OP:case DIV_SV_OP:case DIV_VS_OP:case DIV_V_OP:
				GLSL_infix(cg, fexpr, " / ", level0, 4);
				break;
			case MOD_OP:case MOD_SV_OP:case MOD_VS_OP:case MOD_V_OP:
				GLSL_infix(cg, fexpr, " % ", level0, 4);
				break;
			case ADD_OP:case ADD_SV_OP:case ADD_VS_OP:case ADD_V_OP:
				GLSL_infix(cg, fexpr, " + ", level0, 5);
				break;
			case SUB_OP:case SUB_SV_OP:case SUB_VS_OP:case SUB_V_OP:
				GLSL_infix(cg, fexpr, " - ", level0, 5);
				break;
			case SHL_OP:case SHL_V_OP:
				GLSL_infix(cg, fexpr, " << ", level0, 6);
				break;
			case SHR_OP:case SHR_V_OP:
				GLSL_infix(cg, fexpr, " >> ", level0, 6);
				break;
			case LT_OP:
				GLSL_infix(cg, fexpr, " < ", level0, 7);
				break;
			case LT_SV_OP:case LT_VS_OP:case LT_V_OP:
				GLSL_vecrel(cg, fexpr, "lessThan", fexpr->op - LT_OP);
				break;
			case GT_OP:
				GLSL_infix(cg, fexpr, " > ", level0, 7);
				break;
			case GT_SV_OP:case GT_VS_OP:case GT_V_OP:
				GLSL_vecrel(cg, fexpr, "greaterThan", fexpr->op - GT_OP);
				break;
			case LE_OP:
				GLSL_infix(cg, fexpr, " <= ", level0, 7);
				break;
			case LE_SV_OP:case LE_VS_OP:case LE_V_OP:
				GLSL_vecrel(cg, fexpr, "lessThanEqual", fexpr->op - LE_OP);
				break;
			case GE_OP:
				GLSL_infix(cg, fexpr, " >= ", level0, 7);
				break;
			case GE_SV_OP:case GE_VS_OP:case GE_V_OP:
				GLSL_vecrel(cg, fexpr, "greaterThanEqual", fexpr->op - GE_OP);
				break;
			case EQ_OP:
				GLSL_infix(cg, fexpr, " == ", level0, 8);
				break;
			case EQ_SV_OP:case EQ_VS_OP:case EQ_V_OP:
				GLSL_vecrel(cg, fexpr, "equal", fexpr->op - EQ_OP);
				break;
			case NE_OP:
				GLSL_infix(cg, fexpr, " != ", level0, 8);
				break;
			case NE_SV_OP:case NE_VS_OP:case NE_V_OP:
				GLSL_vecrel(cg, fexpr, "notEqual", fexpr->op - NE_OP);
				break;
			case AND_OP:case AND_SV_OP:case AND_VS_OP:case AND_V_OP:
				GLSL_infix(cg, fexpr, " & ", level0, 9);
				break;
			case XOR_OP:case XOR_SV_OP:case XOR_VS_OP:case XOR_V_OP:
				GLSL_infix(cg, fexpr, " ^ ", level0, 10);
				break;
			case OR_OP:case OR_SV_OP:case OR_VS_OP:case OR_V_OP:
				GLSL_infix(cg, fexpr, " | ", level0, 11);
				break;
			case BAND_OP:case BAND_SV_OP:case BAND_VS_OP:case BAND_V_OP:
				GLSL_infix(cg, fexpr, " && ", level0, 12);
				break;
			case BOR_OP:case BOR_SV_OP:case BOR_VS_OP:case BOR_V_OP:
				GLSL_infix(cg, fexpr, " || ", level0, 14);
				break;
			case ASSIGN_OP:
			case ASSIGN_V_OP:
			case ASSIGN_GEN_OP:
				GLSL_infix(cg, fexpr, " = ", level0, 16);
				break;
			case ASSIGNMINUS_OP:
				GLSL_infix(cg, fexpr, " -= ", level0, 16);
				break;
			case ASSIGNMOD_OP:
				GLSL_infix(cg, fexpr, " %= ", level0, 16);
				break;
			case ASSIGNPLUS_OP:
				GLSL_infix(cg, fexpr, " += ", level0, 16);
				break;
			case ASSIGNSLASH_OP:
				GLSL_infix(cg, fexpr, " /= ", level0, 16);
				break;
			case ASSIGNSTAR_OP:
				GLSL_infix(cg, fexpr, " *= ", level0, 16);
				break;
			case ASSIGNAND_OP:
				GLSL_infix(cg, fexpr, " &= ", level0, 16);
				break;
			case ASSIGNOR_OP:
				GLSL_infix(cg, fexpr, " |= ", level0, 16);
				break;
			case ASSIGNXOR_OP:
				GLSL_infix(cg, fexpr, " ^= ", level0, 16);
				break;
			case COMMA_OP:
				GLSL_infix(cg, fexpr, " , ", level0, 17);
				break;
		}
		break;
	case TRINARY_N:
		switch (fexpr->op) {
			case COND_OP:
			case COND_V_OP:
			case COND_SV_OP:
			case COND_GEN_OP: {
				GLSLParen	paren(cg, level0, 15);
				GLSLPrintExpr(cg, fexpr->tri.arg1, 15);
				cg->outputf(" ? ");
				if (fexpr->tri.arg2)
					GLSLPrintExpr(cg, fexpr->tri.arg2, 15);
				cg->outputf(" : ");
				if (fexpr->tri.arg3)
					GLSLPrintExpr(cg, fexpr->tri.arg3, 15);
				break;
			}
			case ASSIGN_COND_OP:
			case ASSIGN_COND_V_OP:
			case ASSIGN_COND_SV_OP:
			case ASSIGN_COND_GEN_OP:
				GLSLPrintExpr(cg, fexpr->tri.arg1);
				cg->outputf("@@(");
				if (fexpr->tri.arg2)
					GLSLPrintExpr(cg, fexpr->tri.arg2);
				cg->outputf(") = ");
				if (fexpr->tri.arg3)
					GLSLPrintExpr(cg, fexpr->tri.arg3);
				break;
			default:
				cg->outputf("<!TRIOP=%d>", fexpr->op);
				break;
		}
		break;
	default:
		cg->outputf("<!NODEKIND=%d>", fexpr->kind);
		break;
	}
}

void GLSLPrintLocals(CG *cg, Symbol *symb, int level) {
	char tname[100], uname[100];
	if (symb) {
		GLSLPrintLocals(cg, symb->left, level);
		if (!(symb->properties & SYMB_IS_PARAMETER)) {
			const char *pname = cg->GetAtomString(symb->name);
			GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, symb->type, 0);
			GLSLIndent(cg, level);
			cg->outputf("%s %s%s;\n",tname, pname, uname);
		}
		GLSLPrintLocals(cg, symb->right, level);
	}
}

static void GLSLPrintStmt(CG *cg, stmt *fstmt, int level) {
	for (auto s : fstmt) {
		switch (s->kind) {
		case EXPR_STMT:
			GLSLIndent(cg, level);
			if (s->exprst.exp) {
				GLSLPrintExpr(cg, s->exprst.exp);
			} else {
				cg->outputf("/* empty statement */");
			}
			cg->outputf(";\n");
			break;
		case IF_STMT:
			GLSLIndent(cg, level);
			cg->outputf("if (");
			GLSLPrintExpr(cg, s->ifst.cond);
			cg->outputf(")\n");
			GLSLPrintStmt(cg, s->ifst.thenstmt, level + 1);
			if (s->ifst.elsestmt) {
				GLSLIndent(cg, level);
				cg->outputf("else\n");
				GLSLPrintStmt(cg, s->ifst.elsestmt, level + 1);
			}
			break;
		case WHILE_STMT:
			GLSLIndent(cg, level);
			cg->outputf("while (");
			GLSLPrintExpr(cg, s->whilest.cond);
			cg->outputf(")\n");
			GLSLPrintStmt(cg, s->whilest.body, level + 1);
			break;
		case DO_STMT:
			GLSLIndent(cg, level);
			cg->outputf("do\n");
			GLSLPrintStmt(cg, s->whilest.body, level + 1);
			GLSLIndent(cg, level);
			cg->outputf("while (");
			GLSLPrintExpr(cg, s->whilest.cond);
			cg->outputf(");\n");
			break;
		case FOR_STMT:
			GLSLIndent(cg, level);
			cg->outputf("for (");
			for (auto s1 : s->forst.init) {
				if (s1->kind == EXPR_STMT) {
					GLSLPrintExpr(cg, s1->exprst.exp);
				} else {
					cg->outputf("*** BAD STMT KIND ***");
				}
				if (s1->next)
					cg->outputf(", ");
			}
			cg->outputf(";");
			if (s->forst.cond) {
				cg->outputf(" ");
				GLSLPrintExpr(cg, s->forst.cond);
			}
			cg->outputf(";");
			for (auto s1 : s->forst.step) {
				cg->outputf(s1 == s->forst.step ? " " : ", ");
				if (s1->kind == EXPR_STMT) {
					GLSLPrintExpr(cg, s1->exprst.exp);
				} else {
					cg->outputf("*** BAD STMT KIND ***");
				}
			}
			cg->outputf(")\n");
			GLSLPrintStmt(cg, s->forst.body, level + 1);
			break;
		case BLOCK_STMT:
			if (level > 1)
				GLSLIndent(cg, level - 1);
			cg->outputf("{\n");
			GLSLPrintLocals(cg, s->blockst.scope->symbols, level);
			GLSLPrintStmt(cg, s->blockst.body, level);
			if (level > 1)
				GLSLIndent(cg, level - 1);
			cg->outputf("}\n");
			break;
		case RETURN_STMT:
			GLSLIndent(cg, level);
			cg->outputf("return");
			if (s->returnst.exp) {
				cg->outputf(" ");
				GLSLPrintExpr(cg, s->returnst.exp);
			}
			cg->outputf(";\n");
			break;
		case DISCARD_STMT:
			GLSLIndent(cg, level);
			cg->outputf("discard");
			if (s->discardst.cond) {
				cg->outputf(" ");
				GLSLPrintExpr(cg, s->discardst.cond);
			}
			cg->outputf(";\n");
			break;
		case BREAK_STMT:
			GLSLIndent(cg, level);
			cg->outputf("break;\n");
			break;
		case COMMENT_STMT:
			GLSLIndent(cg, level);
			cg->outputf("// %s\n", cg->GetAtomString(s->commentst.str));
			break;
		default:
			GLSLIndent(cg, level);
			cg->outputf("<!BadStmt-0x%2x>\n", s->kind);
		}
	}
}

void GLSLPrintFunction(CG *cg, Symbol *symb) {
	char tname[100], uname[100];

	if (IsFunction(symb)) {
		const char *sname = cg->GetAtomString(symb->name);
		if (symb->type) {
			GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, symb->type->fun.rettype, 0);
		} else {
			strcpy(tname, "NULL");
		}
		cg->outputf("%s %s%s(",tname, sname, uname);
		for (auto param : symb->fun.params) {
			const char *pname = cg->GetAtomString(param->name);
			GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, param->type, 0);
			cg->outputf("%s %s%s",tname, pname, uname);
			if (param->next)
				cg->outputf(", ");
		}
		cg->outputf(") {\n");
		GLSLPrintLocals(cg, symb->fun.locals->symbols, 1);
		GLSLPrintStmt(cg, symb->fun.statements, 1);
		cg->outputf("}\n");
	}
}

void GLSLPrintReferences(CG *cg, Type *type) {
	switch (GetCategory(type)) {
		case TYPE_CATEGORY_ARRAY:
			GLSLPrintReferences(cg, type->arr.eltype);
			break;

		case TYPE_CATEGORY_STRUCT:
			if (type->str.members->symbols->flags == 0) {
				type->str.members->symbols->flags = 1;
				for (auto symb : type->str.members->symbols)
					GLSLPrintReferences(cg, symb->type);
				cg->outputf("struct %s {\n", cg->GetAtomString(type->str.tag));
				for (auto symb : type->str.members->symbols) {
					char tname[100], uname[100];
					const char *pname = cg->GetAtomString(symb->name);
					GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, symb->type, 0);
					GLSLIndent(cg, 1);
					cg->outputf("%s %s%s;\n",tname, pname, uname);
				}
				cg->outputf("};\n");
			}
			break;
	}
}

void GLSLPrintScopeReferences(CG *cg, Symbol *symb) {
	if (symb) {
		GLSLPrintScopeReferences(cg, symb->left);
		if (!(symb->properties & SYMB_IS_PARAMETER))
			GLSLPrintReferences(cg, symb->type);
		GLSLPrintScopeReferences(cg, symb->right);
	}
}

void GLSLPrintReferences(CG *cg, stmt *fstmt, Scope *scope);

static Scope *FindScope(CG *cg, Scope *scope, Symbol *symb) {
	while (scope) {
		if (LookUpLocalSymbol(cg, scope, symb->name) == symb)
			return scope;
		scope = scope->parent;
	}
	return NULL;
}

void GLSLPrintReferences(CG *cg, expr *fexpr, Scope *scope) {
	if (fexpr) switch (fexpr->kind) {
	case SYMB_N: {
		Symbol *symb = fexpr->sym.symbol;
		if (symb->flags == 0 && symb->kind == VARIABLE_S && FindScope(cg, scope, symb)/* == scope*/) {
			char tname[100], uname[100];
			const char *sname = cg->GetAtomString(symb->name);
			GLSLPrintReferences(cg, symb->type);
			GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, symb->type, 0);

			if (GetQualifiers(symb->type) & TYPE_QUALIFIER_CONST) {
				if (symb->var.init) {
					cg->outputf("%s %s%s = ",tname, sname, uname);
					GLSLPrintExpr(cg, symb->var.init);
				} else {
					cg->outputf("%s %s%s",tname, sname, uname);
				}
				cg->outputf(";\n");
			} else {
				cg->outputf("uniform %s %s%s;\n",tname, sname, uname);
			}
			symb->flags = 1;
		}
		break;
	}
	case CONST_N:
		break;
	case UNARY_N:
		GLSLPrintReferences(cg, fexpr->un.arg, scope);
		break;
	case BINARY_N:
		GLSLPrintReferences(cg, fexpr->bin.left, scope);
		GLSLPrintReferences(cg, fexpr->bin.right, scope);
		if (fexpr->op == FUN_CALL_OP) {
			expr	*lexpr = fexpr->bin.left;
			if (lexpr->kind == SYMB_N) {
				Symbol *symb = lexpr->sym.symbol;
				if (symb->flags == 0 && (strcmp(cg->GetAtomString(symb->loc.file), "<stdlib>") != 0 || strcmp(cg->GetAtomString(symb->name), "transpose") == 0)) {
					symb->flags = 1;
					for (auto param : symb->fun.params)
						param->flags = 1;
					if (symb->fun.locals)
						GLSLPrintReferences(cg, symb->fun.statements, symb->fun.locals->parent);
					for (auto param : symb->fun.params)
						GLSLPrintReferences(cg, param->type);
					if (symb->fun.locals)
						GLSLPrintScopeReferences(cg, symb->fun.locals->symbols);
					GLSLPrintFunction(cg, symb);
				}
			}
		}
		break;
	case TRINARY_N:
		GLSLPrintReferences(cg, fexpr->tri.arg1, scope);
		GLSLPrintReferences(cg, fexpr->tri.arg2, scope);
		GLSLPrintReferences(cg, fexpr->tri.arg3, scope);
		break;
	default:
		break;
	}
}

void GLSLPrintReferences(CG *cg, stmt *fstmt, Scope *scope) {
	for (auto s : fstmt) {
		switch (s->kind) {
		case EXPR_STMT:
			GLSLPrintReferences(cg, s->exprst.exp, scope);
			break;
		case IF_STMT:
			GLSLPrintReferences(cg, s->ifst.cond, scope);
			GLSLPrintReferences(cg, s->ifst.thenstmt, scope);
			GLSLPrintReferences(cg, s->ifst.elsestmt, scope);
			break;
		case WHILE_STMT:
		case DO_STMT:
			GLSLPrintReferences(cg, s->whilest.cond, scope);
			GLSLPrintReferences(cg, s->whilest.body, scope);
			break;
		case FOR_STMT:
			GLSLPrintReferences(cg, s->forst.init, scope);
			GLSLPrintReferences(cg, s->forst.cond, scope);
			GLSLPrintReferences(cg, s->forst.step, scope);
			GLSLPrintReferences(cg, s->forst.body, scope);
			break;
		case BLOCK_STMT:
			GLSLPrintScopeReferences(cg, s->blockst.scope->symbols);
			GLSLPrintReferences(cg, s->blockst.body, s->blockst.scope);
			break;
		case RETURN_STMT:
			GLSLPrintReferences(cg, s->returnst.exp, scope);
			break;
		default:
			break;
		}
	}
}

const char *HLSLSemanticName(CG *cg, char *s, int semantics, bool pixel, bool out, int index = -1) {
	const char	*p = cg->GetAtomString(semantics);

	if (out) {
		if (pixel) {
			if (strcmp(p, "COLOR") == 0 || strcmp(p, "COLOR0") == 0)
				return "gl_FragColor";
			if (strcmp(p, "DEPTH") == 0)
				return "gl_FragDepth";
		} else {
			if (strcmp(p, "POSITION") == 0)
				return "gl_Position";
			if (strcmp(p, "PSIZE") == 0)
				return "gl_PointSize";
		}
	} else {
		if (pixel && strcmp(p, "POSITION") == 0)
			return "vec4(0.0)";
	}

	char	*e = s + sprintf(s, "%s%s", out || pixel ? "ps" : "", p);
	if (index >= 0) {
		if (e[-1] < '0' || e[-1] > '9') {
			*e++ = '0';
			*e = 0;
		}
		e[-1] += index;
	}
	return s;
}

void GLSLPrintMainParam2(CG *cg, Type *type, int semantics, bool pixel, bool out, int index = -1) {
	if (GetCategory(type) == TYPE_CATEGORY_STRUCT) {
		for (auto symb : type->str.members->symbols)
			GLSLPrintMainParam2(cg, symb->type, symb->var.semantics, pixel, out);

	} else if (/*!pixel &&*/ GetCategory(type) == TYPE_CATEGORY_ARRAY && !(type->properties & TYPE_MISC_PACKED)) {
		for (int i = 0; i < type->arr.numels; i++)
			GLSLPrintMainParam2(cg, type->arr.eltype, semantics, pixel, out, i);

	} else if (semantics) {
		const char *pname = cg->GetAtomString(semantics);

		if (out) {
			if (pixel
				? (strcmp(pname, "COLOR") == 0		|| strcmp(pname, "COLOR0") == 0	|| strcmp(pname, "DEPTH") == 0)
				: (strcmp(pname, "POSITION") == 0	|| strcmp(pname, "PSIZE") == 0)
			)
				return;
		} else {
			if (pixel && strcmp(pname, "POSITION") == 0)
				return;
		}

		char tname[100], uname[100], pname2[100];
#if 0
		if (index >= 0) {
			strcpy(pname2, pname);
			char *e = pname2 + strlen(pname2);
			if (e[-1] < '0' || e[-1] > '9') {
				*e++ = '0';
				*e = 0;
			}
			e[-1] += index;
			pname = pname2;
		}
#else
		pname = HLSLSemanticName(cg, pname2, semantics, pixel, out, index);
#endif

		GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, type, 1);
		cg->outputf(pixel || out ? "varying %s %s%s;\n" : "attribute %s %s%s;\n", tname, pname, uname);
	}
}

void GLSLPrintMainParam3(CG *cg, Type *type, int semantics, bool pixel, bool out, int index = -1) {
	if (GetCategory(type) == TYPE_CATEGORY_STRUCT) {
		cg->outputf("%s(", cg->GetAtomString(type->str.tag));
		for (auto symb : type->str.members->symbols) {
			GLSLPrintMainParam3(cg, symb->type, symb->var.semantics, pixel, out);
			cg->outputf(symb->next ? ", " : ")");
		}

	} else if (/*!pixel &&*/ GetCategory(type) == TYPE_CATEGORY_ARRAY && !(type->properties & TYPE_MISC_PACKED)) {
		char tname[100], uname[100];
		GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, type, 1);
		cg->outputf("%s%s(", tname, uname);
		for (int i = 0; i < type->arr.numels; i++) {
			if (i)
				cg->outputf(",");
			GLSLPrintMainParam3(cg, type->arr.eltype, semantics, pixel, out, i);
		}
		cg->outputf(")");

	} else if (semantics) {
		char	temp[100];
		cg->outputf(HLSLSemanticName(cg, temp, semantics, pixel, out, index));
	}
}

void GLSLPrintMainParam4(CG *cg, Type *type, int semantics, bool pixel, char *name, int index = -1) {
	if (GetCategory(type) == TYPE_CATEGORY_STRUCT) {
		char	*name_end = name + strlen(name);
		*name_end = '.';
		for (auto symb : type->str.members->symbols) {
			strcpy(name_end + 1, cg->GetAtomString(symb->name));
			GLSLPrintMainParam4(cg, symb->type, symb->var.semantics, pixel, name);
		}
		*name_end = 0;

	} else if (/*!pixel &&*/ GetCategory(type) == TYPE_CATEGORY_ARRAY && !(type->properties & TYPE_MISC_PACKED)) {
		char	*name_end = name + strlen(name);
		for (int i = 0; i < type->arr.numels; i++) {
			sprintf(name_end, "[%i]", i);
			GLSLPrintMainParam4(cg, type->arr.eltype, semantics, pixel, name, i);
		}

	} else if (semantics) {
		char	temp[100];
		cg->outputf("\t%s = %s;\n", HLSLSemanticName(cg, temp, semantics, pixel, true, index), name);
	}
}

void GLSLPrintMainFunction(CG *cg, Symbol *symb, bool pixel) {
	if (IsFunction(symb)) {
		Type	*rettype	= symb->type->fun.rettype;
		char tname[100], uname[100];

		SetSymbolFlagsList(symb->fun.locals, 0);

//		cg->outputf("#version 120\n");
//		cg->outputf("precision mediump float;\n");

		GLSLPrintMainParam2(cg, rettype, symb->fun.semantics, pixel, true);
		for (auto param : symb->fun.params)
			GLSLPrintMainParam2(cg, param->type, param->var.semantics, pixel, !!(GetQualifiers(param->type) & TYPE_QUALIFIER_OUT));
		for (auto param : symb->fun.params)
			GLSLPrintReferences(cg, param->type);

		for (auto param : symb->fun.params)
			param->flags = 1;
		GLSLPrintReferences(cg, symb->fun.statements, symb->fun.locals->parent);
		GLSLPrintScopeReferences(cg, symb->fun.locals->symbols);
		GLSLPrintFunction(cg, symb);

		cg->outputf("void main() {\n");

		if (GetCategory(rettype) == TYPE_CATEGORY_STRUCT) {
			GLSLFormatTypeString(cg, tname, sizeof tname, uname, sizeof uname, rettype, 1);
			cg->outputf("\t%s t%s", tname, uname);
		} else {
			cg->outputf("\t%s", pixel ? "gl_FragColor" : "gl_Position");
		}

		cg->outputf(" = %s(", cg->GetAtomString(symb->name));
		for (auto param : symb->fun.params) {
			GLSLPrintMainParam3(cg, param->type, param->var.semantics, pixel, !!(GetQualifiers(param->type) & TYPE_QUALIFIER_OUT));
			if (param->next)
				cg->outputf(", ");
		}
		cg->outputf(");\n");

		strcpy(uname, "t");
		GLSLPrintMainParam4(cg, rettype, 0, pixel, uname);

/*		if (GetCategory(rettype) == TYPE_CATEGORY_STRUCT) {
			for (Symbol *symb = rettype->str.members->symbols; symb; symb = symb->next) {
				if (int semantics = symb->var.semantics)
					cg->outputf("\t%s = t.%s;\n", HLSLSemanticName(tname, semantics, pixel, true), cg->GetAtomString(symb->name));
			}
		}
*/
		cg->outputf("}");

	}
}

struct HAL_vs_glsl : HAL_generic {
	bool	BindVaryingSemantic(CG *cg, SourceLoc &loc, Symbol *fSymb, int semantic, Binding *fBind, bool is_out_val) {
		fBind->kind			= BK_SEMANTIC;
		fBind->rname		= semantic;
		fBind->regno		= 0;
		fBind->properties	|= BIND_IS_BOUND | BIND_VARYING | (is_out_val ? BIND_OUTPUT : BIND_INPUT);
		return true;
	}

	int GenerateCode(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program) {
		GLSLPrintMainFunction(cg, program, false);
		cg->output->put("\0", 1);
		return 0;
	}

	bool GetCapsBit(int bit) {
		switch (bit) {
			case CAPS_NO_OPTIMISATION:
				return true;
			default:
				return false;
		}
	}

	static HAL* Create(CG *cg) { return new HAL_vs_glsl(); }
};

struct HAL_ps_glsl : HAL_generic {
	bool	BindVaryingSemantic(CG *cg, SourceLoc &loc, Symbol *fSymb, int semantic, Binding *fBind, bool is_out_val) {
		fBind->kind			= BK_SEMANTIC;
		fBind->rname		= semantic;
		fBind->regno		= 0;
		fBind->properties	|= BIND_IS_BOUND | BIND_VARYING | (is_out_val ? BIND_OUTPUT : BIND_INPUT);
		return true;
	}

	bool GetCapsBit(int bit) {
		switch (bit) {
			case CAPS_NO_OPTIMISATION:
				return true;
			default:
				return false;
		}
	}

	int GenerateCode(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program) {
		GLSLPrintMainFunction(cg, program, true);
		return 0;
	}
};

ProfileRegistration	glsl_vs_reg(HAL::create<HAL_vs_glsl>, "vs_glsl", 10);
ProfileRegistration	glsl_ps_reg(HAL::create<HAL_ps_glsl>, "ps_glsl", 10);

} //namespace cgc


