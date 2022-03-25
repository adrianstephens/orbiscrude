#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "support.h"
#include "compile.h"
#include "errors.h"

#if 0
#define DB(X)   X
#else
#define DB(X)
#endif

namespace cgc {

// struct operations defines the set of possible basic operations we might
// want to do on some data type

typedef    void	(*optype_un)	(scalar_constant*, const scalar_constant*);
typedef    void	(*optype_bin)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
typedef    void	(*optype_shift)	(scalar_constant*, const scalar_constant*, int);
typedef    bool	(*optype_bool)	(const scalar_constant*, const scalar_constant*);


struct operations {
    opcode	const_opcode;   // opcode to use for constants of this type
    void	(*op_neg)	(scalar_constant*, const scalar_constant*);
    void	(*op_not)	(scalar_constant*, const scalar_constant*);
    void	(*op_bnot)	(scalar_constant*, const scalar_constant*);
    void	(*op_add)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_sub)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_mul)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_div)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_mod)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_and)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_or)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_xor)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_band)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_bor)	(scalar_constant*, const scalar_constant*, const scalar_constant*);
    void	(*op_shr)	(scalar_constant*, const scalar_constant*, int);
    void	(*op_shl)	(scalar_constant*, const scalar_constant*, int);
    bool	(*op_lt)	(const scalar_constant*, const scalar_constant*);
    bool	(*op_gt)	(const scalar_constant*, const scalar_constant*);
    bool	(*op_le)	(const scalar_constant*, const scalar_constant*);
    bool	(*op_ge)	(const scalar_constant*, const scalar_constant*);
    bool	(*op_eq)	(const scalar_constant*, const scalar_constant*);
    bool	(*op_ne)	(const scalar_constant*, const scalar_constant*);
    void	(*cvtTo[TYPE_BASE_LAST_USER+1])(scalar_constant*, const scalar_constant*);
    void	(*cvtFrom[TYPE_BASE_LAST_USER+1])(scalar_constant*, const scalar_constant*);
};

#if 1
template<typename L> struct lambda_s {
	const L	*x;
	lambda_s(const L &_x) { x = &_x; }
	template<typename F> operator F*() const { return static_cast<F*>(*x); }
};
template<typename L> lambda_s<L> lambda(const L &x) { return lambda_s<L>(x); }

#else

template<typename T> struct operations_t : operations {
	operations_t() : 
		op_neg(	[](scalar_constant *r, const scalar_constant *a)							{ r->i = -a->i; }		),
		op_not(	[](scalar_constant *r, const scalar_constant *a)							{ r->i = ~a->i; }		),
		op_bnot([](scalar_constant *r, const scalar_constant *a)							{ r->i = !a->i; }		),
		op_add(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i + b->i; }	),
		op_sub(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i - b->i; }	),
		op_mul(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i * b->i; }	),
		op_div(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i / b->i; }	),
		op_mod(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i % b->i; }	),
		op_and(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i & b->i; }	),
		op_or(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i | b->i; }	),
		op_xor(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i ^ b->i; }	),
		op_band([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i && b->i; }),
		op_bor(	[](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i || b->i; }),
		op_shr(	[](scalar_constant *r, const scalar_constant *a, int b)						{ r->i = a->i >> b; }	),
		op_shl(	[](scalar_constant *r, const scalar_constant *a, int b)						{ r->i = a->i << b; }	),
		op_lt(	[](const scalar_constant *a, const scalar_constant *b)						{ return a->i <  b->i; }),
		op_gt(	[](const scalar_constant *a, const scalar_constant *b)						{ return a->i >  b->i; }),
		op_le(	[](const scalar_constant *a, const scalar_constant *b)						{ return a->i <= b->i; }),
		op_ge(	[](const scalar_constant *a, const scalar_constant *b)						{ return a->i >= b->i; }),
		op_eq(	[](const scalar_constant *a, const scalar_constant *b)						{ return a->i == b->i; }),
		op_ne(	[](const scalar_constant *a, const scalar_constant *b)						{ return a->i != b->i; }),
	{}
};
#endif

extern operations *runtime_ops[];

// return true iff the argument is a constant or a list of constants
static int IsConstList(expr *fexpr) {
	while (fexpr &&
		   fexpr->kind == BINARY_N &&
		   fexpr->op == EXPR_LIST_OP
	) {
		if (!IsConstant(fexpr->bin.left)) return 0;
		if (!(fexpr = fexpr->bin.right)) return 1;
	}
	return fexpr && fexpr->kind == CONST_N;
}

// create a new blank constant node we can fill in
// This is a bit of a hack, as the opcode field is really redundant with
// the type info in the node, so we just create an opcode that's as close
// as possible to what we want -- which sometimes may not exist (vectors
// other than float
static expr *NewConst(CG *cg, int base, int len) {
	float       tmp[4] = { 0, 0, 0, 0 };
	opcode      op = FCONST_OP;

	// This is a bit of a hack -- we use NewFConstNodeV to allocate a node
	// even for non-FCONST_V nodes.  It turns out that it works out ok.
	if (runtime_ops[base]) op = runtime_ops[base]->const_opcode;
	if (len) op = opcode(op + 1);
	return NewFConstNodeV(cg, op, tmp, len, base);
}

// get the actual value from a constant node
static scalar_constant*GetConstVal(expr *e) {
	return e && e->kind == CONST_N ? e->co.val : 0;
}

struct constlist_iter {
	expr        *curr, *rest;
	int         i, lim;
};

static void InitConstList_iter(constlist_iter *iter, expr *fexpr) {
	iter->curr = 0;
	iter->rest = fexpr;
	iter->i = iter->lim = 0;
}

static scalar_constant*ConstListNext(constlist_iter *iter) {
	if (iter->i >= iter->lim) {
		do {
			if (!iter->rest) return 0;
			if (iter->rest->kind == BINARY_N && iter->rest->op == EXPR_LIST_OP) {
				iter->curr = iter->rest->bin.left;
				iter->rest = iter->rest->bin.right;
			} else if (iter->rest->kind == CONST_N) {
				iter->curr = iter->rest;
				iter->rest = 0;
			} else {
				iter->rest = 0;
				return 0;
			}
		} while (!iter->curr || iter->curr->kind != CONST_N);
		iter->i = 0;
		iter->lim = SUBOP_GET_S(iter->curr->co.subop);
	}
	return &iter->curr->co.val[iter->i++];
}

DB(
static void pconst(scalar_constant*v, int type) {
	switch(type) {
	case TYPE_BASE_BOOLEAN:
		printf("%c", v->i ? 'T' : 'F');
		break;
	case TYPE_BASE_INT:
	case TYPE_BASE_CINT:
		printf("%d", v->i);
		break;
	default:
		printf("%g", v->f);
		break;
	}
}

static void DumpConstList(expr *fexpr, int type) {
	constlist_iter      iter;
	int                 first = 1;
	scalar_constant     *v;

	InitConstList_iter(&iter, fexpr);
	printf("(");
	while ((v = ConstListNext(&iter))) {
		if (first) first = 0;
		else printf(", ");
		pconst(v, type);
	}
	printf(")");
}
)

// ConstantFoldNode
// all the real work is done here
struct ConstantFold {
	expr *post_expr(CG *cg, expr *fexpr, int flags) {
		expr *rv = fexpr;
		expr *arg;
		int		base, target;
		int		len, len2, mask, mask2;
		int		group, index, i;
		void	(*unfn)(scalar_constant*, const scalar_constant*) = 0;
		void	(*binfn)(scalar_constant*, const scalar_constant*, const scalar_constant*) = 0;
		void	(*shfn)(scalar_constant*, const scalar_constant*, int) = 0;
		bool	(*cmpfn)(const scalar_constant*, const scalar_constant*) = 0;
		scalar_constant	*a1, *a2;
		int op_offset, a1_mask, a2_mask;

		if (fexpr) switch(fexpr->kind) {
			case UNARY_N:
				arg = fexpr->un.arg;
				base = SUBOP_GET_T(fexpr->un.subop);
				len = SUBOP_GET_S(fexpr->un.subop);
				switch (fexpr->op) {
					case VECTOR_V_OP:
						if (IsConstList(arg)) {
							constlist_iter      iter;
							DB (printf("fold VECTOR_V_OP[%d:%d]", len, base);
								DumpConstList(arg, base);
								printf(" -> ");)
							InitConstList_iter(&iter, arg);
							rv = NewConst(cg, base, len);
							for (i=0; i<len; i++) {
								rv->co.val[i] = *ConstListNext(&iter);
								DB (if (i) printf(", ");
									pconst(&rv->co.val[i], base);)
							}
							DB(printf("\n");)
							return rv;
						}
						break;
					case SWIZZLE_Z_OP:
						if (arg->kind == UNARY_N) {
							mask = SUBOP_GET_MASK(fexpr->un.subop);
							len	= SUBOP_GET_S2(fexpr->un.subop);
							len += (int)(len == 0);
							switch (arg->op) {
								case SWIZZLE_Z_OP: {
									int mask0 = SUBOP_GET_MASK(arg->un.subop) << 8;
									for (i=0; i < len; i++, mask >>= 2)
										mask |= (mask0 >> ((mask & 3) * 2)) & 0x300;
									mask >>= (4 - len) * 2;
									fexpr->un.subop = SUBOP_Z(mask, SUBOP_GET_S2(fexpr->un.subop), SUBOP_GET_S(arg->un.subop), base);
									fexpr->un.arg = arg->un.arg;
									break;
								}
								case SWIZMAT_Z_OP: {
									int mask0 = SUBOP_GET_MASK16(arg->un.subop);
									int	mask1 = 0;
									for (i=0; i < len; i++, mask >>= 2)
										mask1 |= ((mask0 >> ((mask & 3) * 4)) & 15) << (i * 4);
									fexpr->op = SWIZMAT_Z_OP;
									fexpr->un.subop = SUBOP_ZM(mask1, SUBOP_GET_S2(fexpr->un.subop), SUBOP_GET_S2(arg->un.subop), SUBOP_GET_S(arg->un.subop), base);
									fexpr->un.arg = arg->un.arg;
									break;
								}
								case VECTOR_V_OP: {
									expr*	used[4];
									int		nused = 0, need_swiz = 0, j, k;
									i	= k = 0;
									for (arg = arg->un.arg; arg && arg->kind == BINARY_N && arg->op == EXPR_LIST_OP; arg = arg->bin.right) {
										expr	*e		= arg->bin.left;
										int		len3	= 0;
										int		bits	= 0;
										len2	= 1;
										mask2	= 0;
										IsVector(e->type, &len2);
										for (j = len; j--; bits <<= 1) {
											int	f = ((mask >> (j * 2)) & 3) - i;
											if (f >= 0 && f < len2) {
												mask2 = (mask2 << 2) | f;
												mask -= (i - k) << (j * 2);
												len3++;
												bits++;
											}
										}
										bits		= bits | (bits - 1);
										need_swiz	|= bits & (bits + 1);	// if 1 bits aren't contiguous, need to swizzle final result

										if (len3) {
											if (len3 != len2 || mask2 != (0xe4 & ((1 << (len2 * 2)) - 1))) {
												e = NewUnopSubNode(cg, SWIZZLE_Z_OP, SUBOP_Z(mask2, len3, len2, base), e);
												e = post_expr(cg, e, flags);
											}
											used[nused++] = e;
											k += len3;
										}
										i += len2;
									}
									if (nused == 1)
										return used[0];

									rv = NULL;
									len	= SUBOP_GET_S2(fexpr->un.subop);
									if (need_swiz) {
										for (i = 0; i < nused; i++)
											rv = GenExprList(cg, rv, used[i], used[i]->type);
										SUBOP_SET_MASK(fexpr->un.subop, mask);
										fexpr->un.arg = rv;
										return fexpr;
									}
									for (i = 0; i < len; i++) {
										int	f = (mask >> (i * 2)) & 3;
										for (j = k = 0; f != k; j++, k += len2) {
											len2 = 1;
											IsVector(used[j]->type, &len2);
										}
										rv = GenExprList(cg, rv, used[j], fexpr->type);
									}
									return NewUnopSubNode(cg, VECTOR_V_OP, SUBOP_V(len, base), rv);
								}
							}
						}
						break;
					case SWIZMAT_Z_OP:
						if (arg->kind == UNARY_N) switch (arg->op) {
							case CAST_CM_OP:
								fexpr->un.subop = SUBOP_ZM(SUBOP_GET_MASK16(fexpr->un.subop), SUBOP_GET_T2(fexpr->un.subop), SUBOP_GET_S2(arg->un.subop), SUBOP_GET_S(arg->un.subop), base);
								fexpr->un.arg = arg->un.arg;
								break;
							case MATRIX_M_OP: {
								int		mask	= SUBOP_GET_MASK16(arg->un.subop);
								int		len		= SUBOP_GET_SR(arg->un.subop);
								if (AllSameRow(mask, len)) {
									int	swiz	= GetRowSwizzle(mask, len);
									for (i = (mask >> 2) & 3, arg = arg->un.arg; i--; arg = arg->bin.right);
									rv = arg->bin.left;
									if (!IsNullSwizzle(swiz, len))
										rv = NewUnopSubNode(cg, SWIZZLE_Z_OP, SUBOP_Z(swiz, len, 0, base), rv);
								} else {
	#if 0
									for (i=0; i < len; i++, mask >>= 4) {
										int	newrow = (mask >> 2) & 3, j;
										if (newrow != row) {
											for (fexpr = arg, j = newrow; j--; fexpr = fexpr->bin.right);
											fexpr = fexpr->bin.left;
											if (row == -1) {
												rv = fexpr;
											} else {
												explist = GenExprList(cg, explist, rv, type);
												rv = NewUnopSubNode(cg, VECTOR_V_OP, SUBOP_V(len, base), explist);
											}
											row = newrow;
											row_mask = row_count = 0;
										}
										mask2 |= (mask & 3) << (row_count * 2);
									}
	#endif
								}
							}
						}
						break;

					case POS_OP:
					case POS_V_OP:
						return arg;

					case NEG_OP:
					case NEG_V_OP:
						if (arg->kind == UNARY_N && arg->op == fexpr->op)
							return arg->un.arg;
						if (i = IsAddSub(arg)) {
							if (i < 0) {
								expr	*t = arg->bin.right;
								arg->bin.right = arg->bin.left;
								arg->bin.left = t;
								return arg;
							} else if (IsConstant(arg->bin.right)) {
								a1 = GetConstVal(arg->bin.right);
								a1_mask = IsVector(arg->bin.right->type, NULL) ? -1 : 0;
								rv = NewConst(cg, base, len);
								unfn = runtime_ops[base]->op_neg;
								for (i=0; i==0 || i<len; i++)
									unfn(&rv->co.val[i], &a1[i & a1_mask]);
								arg->bin.right = arg->bin.left;
								arg->bin.left = rv;
								return arg;
							}
						}
						break;
				}

				if (!IsConstant(arg) || !runtime_ops[base])
					break;

				a1 = GetConstVal(arg);
				switch (fexpr->op) {
					case SWIZZLE_Z_OP: {
						mask = SUBOP_GET_MASK(fexpr->un.subop);
						len = SUBOP_GET_S2(fexpr->un.subop);
						DB (printf("fold SWIZ[%d:%d].", len, base);
							for (i=0; i<len; i++)
								putchar("xyzw"[(mask>>(i*2))&3]);
							DumpConstList(arg, base);
							printf(" -> ");)
						rv = NewConst(cg, base, len);
						for (i=0; i==0 || i<len; i++) {
							rv->co.val[i] = a1[mask&3];
							mask >>= 2;
							DB (if (i) printf(", ");
								pconst(&rv->co.val[i], base);)
						}
						DB(printf("\n");)
						break; }
					case SWIZMAT_Z_OP:
						break;
					case CAST_CS_OP:
					case CAST_CV_OP:
						target = SUBOP_GET_T2(fexpr->un.subop);
						DB (printf("fold CAST[%d:%d->%d]", len, base, target);
							DumpConstList(arg, base);
							printf(" -> ");)
						unfn = runtime_ops[base]->cvtTo[target];
						if (!unfn && runtime_ops[target])
							unfn = runtime_ops[target]->cvtFrom[base];
						base = target;
						goto normal_unop;
					case CAST_CM_OP:
						break;
					case NEG_OP:
					case NEG_V_OP:
						DB (printf("fold NEG[%d:%d]", len, base);
							DumpConstList(arg, base);
							printf(" -> ");)
						unfn = runtime_ops[base]->op_neg;
					normal_unop:
						if (!unfn) {
							DB(printf("no function, abort\n");)
							break;
						}
						rv = NewConst(cg, base, len);
						a1_mask = IsVector(arg->type, NULL) ? -1 : 0;

						for (i=0; i==0 || i<len; i++) {
							unfn(&rv->co.val[i], &a1[i & a1_mask]);
							DB (if (i) printf(", ");
								pconst(&rv->co.val[i], base);)
						}
						DB(printf("\n");)
						break;
					case NOT_OP:
					case NOT_V_OP:
						DB (printf("fold NOT[%d:%d]", len, base);
							DumpConstList(arg, base);
							printf(" -> ");)
						unfn = runtime_ops[base]->op_not;
						goto normal_unop;
					case BNOT_OP:
					case BNOT_V_OP:
						DB (printf("fold BNOT[%d:%d]", len, base);
							DumpConstList(arg, base);
							printf(" -> ");)
						unfn = runtime_ops[base]->op_bnot;
						goto normal_unop;
						break;
					default:
						break;
				}
				break;

			case BINARY_N:
				if (IsBuiltin(fexpr, &group, &index)) {
					expr	*t = cg->hal->FoldInternalFunction(cg, group, index, fexpr->bin.right);
					if (t) {
						t->type = rv->type;
						rv = t;
					}
					break;
				}

				if (fexpr->op == MEMBER_SELECTOR_OP && fexpr->bin.left->op == EXPR_LIST_OP) {
					Type	*type	= fexpr->bin.right->type;
					if (IsStruct(type)) {
						expr	*e		= fexpr->bin.left;
						Symbol	*m		= fexpr->bin.right->sym.symbol;
						Symbol	*symb;
						for (symb = type->str.members->symbols; symb; symb = symb->next) {
							if (symb == m)
								return e;
							e = e->bin.right;
						}
					}
				}

				i = int(IsConstant(fexpr->bin.left)) | (IsConstant(fexpr->bin.right) ? 2 : 0);
				if (i == 0)
					break;

				base	= SUBOP_GET_T(fexpr->bin.subop);
				len		= SUBOP_GET_S(fexpr->bin.subop);

				if (i < 3) {
					int		v;
					expr	*cexpr, *ncexpr;
					if (i & 1) {
						cexpr = fexpr->bin.left;
						ncexpr = fexpr->bin.right;
					} else {
						ncexpr = fexpr->bin.left;
						cexpr = fexpr->bin.right;
					}

	//				if (SUBOP_GET_S(cexpr->co.subop) > 1)
	//					break;

					a1		= GetConstVal(cexpr);
					v		= a1->i;
					if (base == TYPE_BASE_FLOAT || base == TYPE_BASE_HALF) {
						if (a1->f == 1.f)
							v = 1;
						else if (a1->f == -1.f)
							v = -1;
					}
					switch(fexpr->op) {
						case ARRAY_INDEX_OP:
							if (fexpr->bin.left->kind == UNARY_N && fexpr->bin.left->op == MATRIX_M_OP) {
								fexpr = fexpr->bin.left->un.arg;
								while (v--)
									fexpr = fexpr->bin.right;
								return fexpr->bin.left;
							} else if ((flags & FC_MATRIX_INDEX_TO_SWIZMAT) && IsMatrix(fexpr->bin.left->type, &len, &len2)) {
								mask = 0x3210 + v * 0x4444;
								fexpr = NewUnopSubNode(cg, SWIZMAT_Z_OP, SUBOP_ZM(mask, len, len2, len, base), fexpr->bin.left);
								return post_expr(cg, fexpr, flags);
							}
							break;
						case MUL_OP:
						case MUL_V_OP:
						case MUL_SV_OP:
						case MUL_VS_OP:
							if (v == 0)
								return cexpr;
							if (v == 1)
								return ncexpr;
							if (v == -1) {
								rv = NewUnopSubNode(cg, NEG_OP, 0, ncexpr);
								rv->type = ncexpr->type;
							}
							if (ncexpr->kind == BINARY_N && ((v = IsConstant(ncexpr->bin.left)) || (IsConstant(ncexpr->bin.right)))) {
								switch (ncexpr->op) {
									case MUL_OP: case MUL_V_OP: case MUL_SV_OP: case MUL_VS_OP:
									case DIV_OP: case DIV_V_OP: case DIV_SV_OP: case DIV_VS_OP:
										cexpr = post_expr(cg, NewBinopSubNode(cg, ncexpr->op, ncexpr->bin.subop, cexpr, v ? ncexpr->bin.left : ncexpr->bin.right), flags);
										return (v
											?	NewBinopSubNode(cg, ncexpr->op,	ncexpr->bin.subop,	ncexpr->bin.right, cexpr)
											:	NewBinopSubNode(cg, fexpr->op,	fexpr->bin.subop,	cexpr, ncexpr->bin.left)
										);
								}
							}
							break;
						case DIV_OP:
						case DIV_V_OP:
						case DIV_SV_OP:
						case DIV_VS_OP:
						case MOD_OP:
						case MOD_V_OP:
						case MOD_SV_OP:
						case MOD_VS_OP:
							if (i & 1) {
								if (v == 0)
									return cexpr;
							} else {
								if (v == 1)
									return ncexpr;
								if (v == -1) {
									rv = NewUnopSubNode(cg, NEG_OP, 0, ncexpr);
									rv->type = ncexpr->type;
								}
							}
							if (ncexpr->kind == BINARY_N && ((v = IsConstant(ncexpr->bin.left)) || (IsConstant(ncexpr->bin.right)))) {
								switch (ncexpr->op) {
									case MUL_OP: case MUL_V_OP: case MUL_SV_OP: case MUL_VS_OP: {
										expr	*cexpr2		= v ? ncexpr->bin.left : ncexpr->bin.right;
										expr	*ncexpr2	= v ? ncexpr->bin.right : ncexpr->bin.left;
										if (i & 1) {
											cexpr = post_expr(cg, NewBinopSubNode(cg, fexpr->op, fexpr->bin.subop, cexpr, cexpr2), flags);
											return NewBinopSubNode(cg, fexpr->op, fexpr->bin.subop, cexpr, ncexpr2);
										} else {
											cexpr = post_expr(cg, NewBinopSubNode(cg, fexpr->op, fexpr->bin.subop, cexpr2, cexpr), flags);
											return NewBinopSubNode(cg, ncexpr->op, ncexpr->bin.subop, ncexpr2, cexpr);
										}
									}
									case DIV_OP: case DIV_V_OP: case DIV_SV_OP: case DIV_VS_OP: {
										expr	*cexpr2		= v ? ncexpr->bin.left : ncexpr->bin.right;
										expr	*ncexpr2	= v ? ncexpr->bin.right : ncexpr->bin.left;
										opcode	mulop		= MUL_OP;
										if (i & 1) {
											cexpr = post_expr(cg, NewBinopSubNode(cg, v ? ncexpr->op : mulop, ncexpr->bin.subop, cexpr, cexpr2), flags);
											return NewBinopSubNode(cg, v ? mulop : fexpr->op, fexpr->bin.subop, cexpr, ncexpr2);
										} else {
											cexpr = post_expr(cg, NewBinopSubNode(cg, v ? ncexpr->op : mulop, ncexpr->bin.subop, cexpr2, cexpr), flags);
											return NewBinopSubNode(cg, fexpr->op, fexpr->bin.subop, ncexpr2, cexpr);
										}
									}
								}
							}
							break;
						case ADD_OP:
						case ADD_V_OP:
						case ADD_SV_OP:
						case ADD_VS_OP:
						case SUB_OP:
						case SUB_V_OP:
						case SUB_SV_OP:
						case SUB_VS_OP:
							if (v && (i & 2)) {
								expr *t;
								int	dir = IsAddSub(fexpr), d;
								op_offset = fexpr->op - (dir < 0 ? SUB_OP : ADD_OP);
								for (t = fexpr->bin.left; d = IsAddSub(t); t = t->bin.left) {
									if (IsConstant(t->bin.right)) {
										t	= t->bin.right;
										dir *= d;
										break;
									}
								}
								if (IsConstant(t)) {
									a2		= a1;
									a1		= t->co.val;
									binfn = dir < 0 ? runtime_ops[base]->op_sub : runtime_ops[base]->op_add;
									a1_mask = 0 - (op_offset&1);
									a2_mask = 0 - (((op_offset>>1)^op_offset)&1);
									for (i=0; i==0 || i<len; i++)
										binfn(&t->co.val[i], &a1[i & a1_mask], &a2[i & a2_mask]);
									return ncexpr;
								}
								break;
							}
						case XOR_OP:
						case XOR_V_OP:
						case XOR_SV_OP:
						case XOR_VS_OP:
						case OR_OP:
						case OR_V_OP:
						case OR_SV_OP:
						case OR_VS_OP:
						case BOR_OP:
						case BOR_V_OP:
						case BOR_SV_OP:
						case BOR_VS_OP:
							if (v == 0)
								return ncexpr;
							break;
						case SHL_OP:
						case SHL_V_OP:
						case SHR_OP:
						case SHR_V_OP:
							if (v == 0)
								return ncexpr;
							break;
						case AND_OP:
						case AND_V_OP:
						case AND_SV_OP:
						case AND_VS_OP:
						case BAND_OP:
						case BAND_V_OP:
						case BAND_SV_OP:
						case BAND_VS_OP:
							if (v == 0)
								return cexpr;
							break;
					}
					break;
				}

				if (!runtime_ops[base])
					break;
				a1 = GetConstVal(fexpr->bin.left);
				a2 = GetConstVal(fexpr->bin.right);
				switch(fexpr->op) {
					case MEMBER_SELECTOR_OP:
					case ARRAY_INDEX_OP:
					case FUN_CALL_OP:
					//case FUN_BUILTIN_OP:
					case FUN_ARG_OP:
					case EXPR_LIST_OP:
						break;
					case MUL_OP:
					case MUL_V_OP:
					case MUL_SV_OP:
					case MUL_VS_OP:
						DB (printf("fold MUL");)
						binfn = runtime_ops[base]->op_mul;
						op_offset = fexpr->op - MUL_OP;
					normal_binop:
						DB (printf("[%d:%d]", len, base);
							DumpConstList(fexpr->bin.left, base);
							DumpConstList(fexpr->bin.right, base);
							printf(" -> ");)
						if (!binfn) {
							DB(printf("no function, abort\n");)
							break;
						}
						rv = NewConst(cg, base, len);
						// set a1_mask to all 0s or all 1s, depending on whether a1
						// (left arg) is scalar (all 0s) or vector (all 1s).  a2_mask
						// is set according to a2.  This is dependent on the ordering
						// of the OFFSET_ tags in supprt.h
						a1_mask = 0 - (op_offset&1);
						a2_mask = 0 - (((op_offset>>1)^op_offset)&1);
						for (i=0; i==0 || i<len; i++) {
							binfn(&rv->co.val[i], &a1[i & a1_mask], &a2[i & a2_mask]);
							DB (if (i) printf(", ");
								pconst(&rv->co.val[i], base);)
						}
						DB(printf("\n");)
						break;
					case DIV_OP:
					case DIV_V_OP:
					case DIV_SV_OP:
					case DIV_VS_OP:
						DB (printf("fold DIV");)
						binfn = runtime_ops[base]->op_div;
						op_offset = fexpr->op - DIV_OP;
						goto normal_binop;
					case MOD_OP:
					case MOD_V_OP:
					case MOD_SV_OP:
					case MOD_VS_OP:
						DB (printf("fold MOD");)
						binfn = runtime_ops[base]->op_mod;
						op_offset = fexpr->op - MOD_OP;
						goto normal_binop;
					case ADD_OP:
					case ADD_V_OP:
					case ADD_SV_OP:
					case ADD_VS_OP:
						DB (printf("fold ADD");)
						binfn = runtime_ops[base]->op_add;
						op_offset = fexpr->op - ADD_OP;
						goto normal_binop;
					case SUB_OP:
					case SUB_V_OP:
					case SUB_SV_OP:
					case SUB_VS_OP:
						DB (printf("fold SUB");)
						binfn = runtime_ops[base]->op_sub;
						op_offset = fexpr->op - SUB_OP;
						goto normal_binop;
					case SHL_OP:
					case SHL_V_OP:
						DB (printf("fold SHL");)
						shfn = runtime_ops[base]->op_shl;
					normal_shiftop:
						DB (printf("[%d:%d]", len, base);
							DumpConstList(fexpr->bin.left, base);
							DumpConstList(fexpr->bin.right, TYPE_BASE_CINT);
							printf(" -> ");)
						if (!shfn) {
							DB(printf("no function, abort\n");)
							break;
						}
						rv = NewConst(cg, base, len);
						for (i=0; i==0 || i<len; i++) {
							shfn(&rv->co.val[i], &a1[i], a2->i);
							DB (if (i) printf(", ");
								pconst(&rv->co.val[i], base);)
						}
						DB(printf("\n");)
						break;
					case SHR_OP:
					case SHR_V_OP:
						DB (printf("fold SHR");)
						shfn = runtime_ops[base]->op_shr;
						goto normal_shiftop;
					case LT_OP:
					case LT_V_OP:
					case LT_SV_OP:
					case LT_VS_OP:
						DB (printf("fold LT");)
						cmpfn = runtime_ops[base]->op_lt;
						op_offset = fexpr->op - LT_OP;
					normal_cmpop:
						DB (printf("[%d:%d]", len, base);
							DumpConstList(fexpr->bin.left, base);
							DumpConstList(fexpr->bin.right, TYPE_BASE_CINT);
							printf(" -> ");)
						if (!cmpfn) {
							DB(printf("no function, abort\n");)
							break;
						}
						rv = NewConst(cg, TYPE_BASE_BOOLEAN, len);
						// set a1_mask to all 0s or all 1s, depending on whether a1
						// (left arg) is scalar (all 0s) or vector (all 1s).  a2_mask
						// is set according to a2.  This is dependent on the ordering
						// of the OFFSET_ tags in supprt.h
						a1_mask = 0 - (op_offset&1);
						a2_mask = 0 - (((op_offset>>1)^op_offset)&1);
						for (i=0; i==0 || i<len; i++) {
							rv->co.val[i].i = cmpfn(&a1[i & a1_mask], &a2[i & a2_mask]);
							DB (if (i) printf(", ");
								pconst(&rv->co.val[i], TYPE_BASE_BOOLEAN);)
						}
						DB(printf("\n");)
						break;
					case GT_OP:
					case GT_V_OP:
					case GT_SV_OP:
					case GT_VS_OP:
						cmpfn = runtime_ops[base]->op_gt;
						op_offset = fexpr->op - GT_OP;
						goto normal_cmpop;
					case LE_OP:
					case LE_V_OP:
					case LE_SV_OP:
					case LE_VS_OP:
						cmpfn = runtime_ops[base]->op_le;
						op_offset = fexpr->op - LE_OP;
						goto normal_cmpop;
					case GE_OP:
					case GE_V_OP:
					case GE_SV_OP:
					case GE_VS_OP:
						cmpfn = runtime_ops[base]->op_ge;
						op_offset = fexpr->op - GE_OP;
						goto normal_cmpop;
					case EQ_OP:
					case EQ_V_OP:
					case EQ_SV_OP:
					case EQ_VS_OP:
						cmpfn = runtime_ops[base]->op_eq;
						op_offset = fexpr->op - EQ_OP;
						goto normal_cmpop;
					case NE_OP:
					case NE_V_OP:
					case NE_SV_OP:
					case NE_VS_OP:
						cmpfn = runtime_ops[base]->op_ne;
						op_offset = fexpr->op - NE_OP;
						goto normal_cmpop;
					case AND_OP:
					case AND_V_OP:
					case AND_SV_OP:
					case AND_VS_OP:
						DB (printf("fold AND");)
						binfn = runtime_ops[base]->op_and;
						op_offset = fexpr->op - AND_OP;
						goto normal_binop;
					case XOR_OP:
					case XOR_V_OP:
					case XOR_SV_OP:
					case XOR_VS_OP:
						DB (printf("fold XOR");)
						binfn = runtime_ops[base]->op_xor;
						op_offset = fexpr->op - XOR_OP;
						goto normal_binop;
					case OR_OP:
					case OR_V_OP:
					case OR_SV_OP:
					case OR_VS_OP:
						DB (printf("fold OR");)
						binfn = runtime_ops[base]->op_or;
						op_offset = fexpr->op - OR_OP;
						goto normal_binop;
					case BAND_OP:
					case BAND_V_OP:
					case BAND_SV_OP:
					case BAND_VS_OP:
						DB (printf("fold BAND");)
						binfn = runtime_ops[base]->op_band;
						op_offset = fexpr->op - BAND_OP;
						goto normal_binop;
					case BOR_OP:
					case BOR_V_OP:
					case BOR_SV_OP:
					case BOR_VS_OP:
						DB (printf("fold BOR");)
						binfn = runtime_ops[base]->op_bor;
						op_offset = fexpr->op - BOR_OP;
						goto normal_binop;
					case ASSIGN_OP:
					case ASSIGN_V_OP:
					case ASSIGN_GEN_OP:
					case ASSIGN_MASKED_KV_OP:
					default:
						break;
				}
				break;
			case TRINARY_N:
				switch(fexpr->op) {
					case COND_OP:
					case COND_V_OP:
					case COND_SV_OP:
					case COND_GEN_OP:
						if (IsConstant(fexpr->tri.arg1))
							rv = GetConstVal(fexpr->tri.arg1)->i ? fexpr->tri.arg2 : fexpr->tri.arg3;
						break;
					case ASSIGN_COND_OP:
					case ASSIGN_COND_V_OP:
					case ASSIGN_COND_SV_OP:
					case ASSIGN_COND_GEN_OP:
						if (IsConstant(fexpr->tri.arg2)) {
							if (GetConstVal(fexpr->tri.arg2)->i)
								rv = NewSimpleAssignment(cg, cg->tokenLoc, fexpr->tri.arg1, fexpr->tri.arg3, false);
							else
								rv = 0;
						}
						break;
				}
				break;
			default:
				break;
		}
		return rv;
	}
};

expr *FoldConstants(CG *cg, expr *fexpr, int flags) {
	return cg->ApplyToNodes(ConstantFold(), fexpr, flags);
}

void FoldConstants(CG *cg, stmt *fstmt, int flags) {
	cg->ApplyToExpressions(ConstantFold(), fstmt, flags);
}

int GetConstant(CG *cg, expr *fexpr, int flags) {
	expr *e = cgc::FoldConstants(cg, fexpr, flags);
	if (e->kind == CONST_N)
		return e->co.val[0].i;
	SemanticError(cg, cg->tokenLoc, ERROR___ARRAY_INDEX_NOT_CONSTANT);
	return 0;
}

template<typename A, typename R> void copy(scalar_constant*r, const scalar_constant*a) {
	r->as<R>() = a->as<A>();
}

static operations int_ops = {
	ICONST_OP,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->i = -a->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->i = ~a->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->i = !a->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  + b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  - b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  * b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  / b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  % b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  & b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  | b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i  ^ b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i && b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i || b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, int b)						{ r->i = a->i >> b; }),
	lambda([](scalar_constant *r, const scalar_constant *a, int b)						{ r->i = a->i << b; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i <  b->i; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i >  b->i; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i <= b->i; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i >= b->i; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i == b->i; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i != b->i; }),
	{ 0, 0, copy<int,float>, copy<int,int>, 0, copy<int,float>, copy<int,int>, copy<int,bool>, copy<int,uint>, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, copy<float,int>, copy<int,int>, 0, copy<float,int>, copy<int,int>, copy<bool,int>, copy<uint,int>, 0, 0, 0, 0, 0, 0, 0, },
};

static operations uint_ops = {
	UCONST_OP,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->u = -a->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->u = ~a->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->u = !a->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u + b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u - b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u * b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u / b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u % b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u & b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u | b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u ^ b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u && b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->u = a->u || b->u; }),
	lambda([](scalar_constant *r, const scalar_constant *a, int b)						{ r->u = a->u >> b; }),
	lambda([](scalar_constant *r, const scalar_constant *a, int b)						{ r->u = a->u << b; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->u <  b->u; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->u >  b->u; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->u <= b->u; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->u >= b->u; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->u == b->u; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->u != b->u; }),
	{ 0, 0, copy<uint,float>, copy<uint,int>, 0, copy<uint,float>, copy<uint,int>, copy<uint,bool>, copy<uint,uint>, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, copy<float,uint>, copy<int,uint>, 0, copy<float,uint>, copy<int,uint>, copy<bool,uint>, copy<uint,uint>, 0, 0, 0, 0, 0, 0, 0, },
};

static operations float_ops = {
	FCONST_OP,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->f = -a->f; }),
	0,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->f = !a->f; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->f = a->f + b->f; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->f = a->f - b->f; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->f = a->f * b->f; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->f = a->f / b->f; }),
	0,
	0,
	0,
	0,
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->f = a->f && b->f; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->f = a->f || b->f; }),
	0,
	0,
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->f <  b->f; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->f >  b->f; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->f <= b->f; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->f >= b->f; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->f == b->f; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->f != b->f; }),
	{ 0, 0, copy<float, float>, copy<float,int>, 0, copy<float, float>, copy<float,int>, copy<float,bool>, copy<float,uint>, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, copy<float, float>, copy<int,float>, 0, copy<float, float>, copy<int,float>, copy<bool,float>, copy<uint,float>, 0, 0, 0, 0, 0, 0, 0, },
};

static operations bool_ops = {
	BCONST_OP,
	0,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->i = !a->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->i = !a->i; }),
	0,
	0,
	0,
	0,
	0,
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i & b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i | b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i ^ b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i & b->i; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->i = a->i | b->i; }),
	0,
	0,
	0,
	0,
	0,
	0,
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i == b->i; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->i != b->i; }),
	{ 0, 0, copy<bool,float>, copy<bool,int>, 0, copy<bool,float>, copy<bool,int>, copy<bool,bool>, copy<bool,uint>, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, copy<float,bool>, copy<int,bool>, 0, copy<float,bool>, copy<int,bool>, copy<bool,bool>, copy<uint,bool>, 0, 0, 0, 0, 0, 0, 0, },
};

operations *runtime_ops[TYPE_BASE_LAST_USER+1] = {
	0,          // No type
	0,          // Undefined
	&float_ops, // cfloat
	&int_ops,   // cint
	0,          // void
	&float_ops, // float
	&int_ops,   // int
	&bool_ops,  // boolean
	&uint_ops,	// uint
	// profile defined types: these will be set up by the profile */
	0, 0, 0, 0, 0, 0, 0,
};

union float_parts {
	struct { int m:23, e:8, s:1; };
	float	f;
	float_parts(float _f) : f(_f)	{}
	float_parts()					{}
};

half::operator float() const {
	float_parts	p;
	p.s = i >> 15;

	int	e = (i >> 10) & 31;
	int	m = i & 0x3ff;
	if (e == 0 && m) {
		while (!(m & (1 << 10))) {
			++m;
			--e;
		}
	}
	p.m = m << 13;
	p.e = e - 15 + 127;
	return p.f;
}

half &half::operator=(float f) {
	float_parts	p(f);
	int	e = p.e - 127 + 15;
	int	s = p.s << 15;
	if (e > 30) {
		i = s | (31 << 10);
	} else if (e < -10) {
		i = s;
	} else {
		int m	= p.m >> 13;
		if (e < 0) {
			m = (m | (1 << 10)) >> -e;
			e = 0;
		}
		i = s | (e << 10) | m;
	}
	return *this;
}

#define half	half_as_float
#define fixed	fixed_as_float

static operations half_ops = {
	HCONST_OP,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->h = -a->h; }),
	0,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->h = !a->h; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->h = a->h + b->h; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->h = a->h - b->h; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->h = a->h * b->h; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->h = a->h / b->h; }),
	0,
	0,
	0,
	0,
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->h = a->h && b->h; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->h = a->h || b->h; }),
	0,
	0,
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->h <  b->h; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->h >  b->h; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->h <= b->h; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->h >= b->h; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->h == b->h; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->h != b->h; }),
	{ 0, 0, copy<half,float>, copy<half,int>, 0, copy<half,float>, copy<half,int>, copy<half,bool>, copy<half,uint>, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, copy<float,half>, copy<int,half>, 0, copy<float,half>, copy<int,half>, copy<bool,half>, copy<uint,half>, 0, 0, 0, 0, 0, 0, 0, },
};

static operations fixed_ops = {
	XCONST_OP,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->x = -a->x; }),
	0,
	lambda([](scalar_constant *r, const scalar_constant *a)								{ r->x = !a->x; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->x = a->x + b->x; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->x = a->x - b->x; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->x = a->x * b->x; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->x = a->x / b->x; }),
	0,
	0,
	0,
	0,
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->x = a->x && b->x; }),
	lambda([](scalar_constant *r, const scalar_constant *a, const scalar_constant *b)	{ r->x = a->x || b->x; }),
	0,
	0,
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->x <  b->x; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->x >  b->x; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->x <= b->x; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->x >= b->x; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->x == b->x; }),
	lambda([](const scalar_constant *a, const scalar_constant *b)						{ return a->x != b->x; }),
	{ 0, 0, copy<fixed,float>, copy<fixed,int>, 0, copy<fixed,float>, copy<fixed,int>, copy<fixed,bool>, copy<fixed,uint>, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, copy<float,fixed>, copy<int,fixed>, 0, copy<float,fixed>, copy<int,fixed>, copy<bool,fixed>, copy<uint,fixed>, 0, 0, 0, 0, 0, 0, 0, },
};

void HAL_SetupHalfFixedTypes(int h, int x) {
	if (h) {
		runtime_ops[h]		= &half_ops;
		half_ops.cvtTo[h]	= copy<half,half>;
		half_ops.cvtFrom[h]	= copy<half,half>;
	}
	if (x) {
		runtime_ops[x]		= &fixed_ops;
		fixed_ops.cvtTo[x]	= copy<fixed,fixed>;
		fixed_ops.cvtFrom[x]= copy<fixed,fixed>;
	}
	if (h && x) {
		half_ops.cvtTo[x]	= copy<half,fixed>;
		half_ops.cvtFrom[x] = copy<fixed,half>;
		fixed_ops.cvtTo[h]	= copy<fixed,half>;
		fixed_ops.cvtFrom[h] = copy<half,fixed>;
	}

}

} //namespace cgc
