#ifndef SUPPORT_H
#define SUPPORT_H

#include "symbols.h"

namespace cgc {

#define OPENSL_TAG "cgc"

struct HAL;
typedef int spec;

enum stmtkind {
	EXPR_STMT, IF_STMT, WHILE_STMT, DO_STMT, FOR_STMT,
	BLOCK_STMT, RETURN_STMT, DISCARD_STMT, BREAK_STMT, COMMENT_STMT,
	LAST_STMTKIND
};

enum nodekind {
	DECL_N=LAST_STMTKIND, SYMB_N, CONST_N, UNARY_N, BINARY_N, TRINARY_N, SYMBOLIC_N,
	LAST_NODEKIND,
};

enum subopkind {
	SUB_NONE, SUB_S, SUB_V, SUB_VS, SUB_SV, SUB_M, SUB_VM, SUB_MV,
	SUB_Z, SUB_ZM, SUB_CS, SUB_CV, SUB_CM, SUB_KV,
};

#define OPCODE_TABLE \
	PICK(	VARIABLE_OP,		"var",		IDENT_SY,		0,		SYMB_N,		SUB_NONE),	\
	PICK(	MEMBER_OP,			"member",	IDENT_SY,		0,		SYMB_N,		SUB_NONE),	\
	\
	PICK(	ICONST_OP,			"iconst",	0,				0,		CONST_N,	SUB_S	),	\
	PICK(	ICONST_V_OP,		"iconstv",	0,				0,		CONST_N,	SUB_V	),	\
	PICK(	BCONST_OP,			"bconst",	0,				0,		CONST_N,	SUB_S	),	\
	PICK(	BCONST_V_OP,		"bconstv",	0,				0,		CONST_N,	SUB_V	),	\
	PICK(	FCONST_OP,			"fconst",	0,				0,		CONST_N,	SUB_S	),	\
	PICK(	FCONST_V_OP,		"fconstv",	0,				0,		CONST_N,	SUB_V	),	\
	PICK(	UCONST_OP,			"uconst",	0,				0,		CONST_N,	SUB_S	),	\
	PICK(	UCONST_V_OP,		"uconstv",	0,				0,		CONST_N,	SUB_V	),	\
	PICK(	HCONST_OP,			"hconst",	0,				0,		CONST_N,	SUB_S	),	\
	PICK(	HCONST_V_OP,		"hconstv",	0,				0,		CONST_N,	SUB_V	),	\
	PICK(	XCONST_OP,			"xconst",	0,				0,		CONST_N,	SUB_S	),	\
	PICK(	XCONST_V_OP,		"xconstv",	0,				0,		CONST_N,	SUB_V	),	\
	\
	PICK(	VECTOR_V_OP,		"vector",	0,				0,		UNARY_N,	SUB_V	),	\
	PICK(	MATRIX_M_OP,		"matrix",	0,				0,		UNARY_N,	SUB_M	),	\
	PICK(	SWIZZLE_Z_OP,		"swizzle",	'.',			0,		UNARY_N,	SUB_Z	),	\
	PICK(	SWIZMAT_Z_OP,		"swizmat",	'.',			0,		UNARY_N,	SUB_ZM	),	\
	PICK(	CAST_CS_OP,			"cast",		'(',			0,		UNARY_N,	SUB_CS	),	\
	PICK(	CAST_CV_OP,			"castv",	'(',			0,		UNARY_N,	SUB_CV	),	\
	PICK(	CAST_CM_OP,			"castm",	'(',			0,		UNARY_N,	SUB_CM	),	\
	PICK(	NEG_OP,				"neg",		'-',			"-",	UNARY_N,	SUB_S	),	\
	PICK(	NEG_V_OP,			"negv",		'-',			"-",	UNARY_N,	SUB_V	),	\
	PICK(	POS_OP,				"pos",		'+',			"+",	UNARY_N,	SUB_S	),	\
	PICK(	POS_V_OP,			"posv",		'+',			"+",	UNARY_N,	SUB_V	),	\
	PICK(	NOT_OP,				"not",		'~',			"~",	UNARY_N,	SUB_S	),	\
	PICK(	NOT_V_OP,			"notv",		'~',			"~",	UNARY_N,	SUB_V	),	\
	PICK(	BNOT_OP,			"bnot",		'!',			"!",	UNARY_N,	SUB_S	),	\
	PICK(	BNOT_V_OP,			"bnotv",	'!',			"!",	UNARY_N,	SUB_V	),	\
	\
	PICK(	KILL_OP,			"kill",		DISCARD_SY,		0,		UNARY_N,	SUB_S	),	\
	\
	PICK(	PREDEC_OP,			"predec",	MINUSMINUS_SY,	"--",	UNARY_N,	SUB_S	),	\
	PICK(	PREINC_OP,			"preinc",	PLUSPLUS_SY,	"++",	UNARY_N,	SUB_S	),	\
	PICK(	POSTDEC_OP,			"postdec",	MINUSMINUS_SY,	"--",	UNARY_N,	SUB_S	),	\
	PICK(	POSTINC_OP,			"postinc",	PLUSPLUS_SY,	"++",	UNARY_N,	SUB_S	),	\
	\
	PICK(	MEMBER_SELECTOR_OP,	"mselect",	'.',			".",	BINARY_N,	SUB_NONE),	\
	PICK(	ARRAY_INDEX_OP,		"index",	'[',			"[]",	BINARY_N,	SUB_NONE),	\
	PICK(	FUN_CALL_OP,		"call",		'(',			"()",	BINARY_N,	SUB_NONE),	\
	PICK(	FUN_BUILTIN_OP,		"builtin",	0,				0,		BINARY_N,	SUB_NONE),	\
	PICK(	FUN_ARG_OP,			"arg",		0,				0,		BINARY_N,	SUB_NONE),	\
	PICK(	EXPR_LIST_OP,		"list",		0,				0,		BINARY_N,	SUB_NONE),	\
	PICK(	MUL_OP,				"mul",		'*',			"*",	BINARY_N,	SUB_S	),	\
	PICK(	MUL_V_OP,			"mulv",		'*',			"*",	BINARY_N,	SUB_V	),	\
	PICK(	MUL_SV_OP,			"mulsv",	'*',			"*",	BINARY_N,	SUB_SV	),	\
	PICK(	MUL_VS_OP,			"mulvs",	'*',			"*",	BINARY_N,	SUB_VS	),	\
	PICK(	DIV_OP,				"div",		'/',			"/",	BINARY_N,	SUB_S	),	\
	PICK(	DIV_V_OP,			"divv",		'/',			"/",	BINARY_N,	SUB_V	),	\
	PICK(	DIV_SV_OP,			"divsv",	'/',			"/",	BINARY_N,	SUB_SV	),	\
	PICK(	DIV_VS_OP,			"divvs",	'/',			"/",	BINARY_N,	SUB_VS	),	\
	PICK(	MOD_OP,				"mod",		'%',			"%",	BINARY_N,	SUB_S	),	\
	PICK(	MOD_V_OP,			"modv",		'%',			"%",	BINARY_N,	SUB_V	),	\
	PICK(	MOD_SV_OP,			"modsv",	'%',			"%",	BINARY_N,	SUB_SV	),	\
	PICK(	MOD_VS_OP,			"modvs",	'%',			"%",	BINARY_N,	SUB_VS	),	\
	PICK(	ADD_OP,				"add",		'+',			"+",	BINARY_N,	SUB_S	),	\
	PICK(	ADD_V_OP,			"addv",		'+',			"+",	BINARY_N,	SUB_V	),	\
	PICK(	ADD_SV_OP,			"addsv",	'+',			"+",	BINARY_N,	SUB_SV	),	\
	PICK(	ADD_VS_OP,			"addvs",	'+',			"+",	BINARY_N,	SUB_VS	),	\
	PICK(	SUB_OP,				"sub",		'-',			"-",	BINARY_N,	SUB_S	),	\
	PICK(	SUB_V_OP,			"subv",		'-',			"-",	BINARY_N,	SUB_V	),	\
	PICK(	SUB_SV_OP,			"subsv",	'-',			"-",	BINARY_N,	SUB_SV	),	\
	PICK(	SUB_VS_OP,			"subvs",	'-',			"-",	BINARY_N,	SUB_VS	),	\
	PICK(	SHL_OP,				"shl",		LL_SY,			"<<",	BINARY_N,	SUB_S	),	\
	PICK(	SHL_V_OP,			"shlv",		LL_SY,			"<<",	BINARY_N,	SUB_V	),	\
	PICK(	SHR_OP,				"shr",		GG_SY,			">>",	BINARY_N,	SUB_S	),	\
	PICK(	SHR_V_OP,			"shrv",		GG_SY,			">>",	BINARY_N,	SUB_V	),	\
	PICK(	LT_OP,				"lt",		'<',			"<",	BINARY_N,	SUB_S	),	\
	PICK(	LT_V_OP,			"ltv",		'<',			"<",	BINARY_N,	SUB_V	),	\
	PICK(	LT_SV_OP,			"ltsv",		'<',			"<",	BINARY_N,	SUB_SV	),	\
	PICK(	LT_VS_OP,			"ltvs",		'<',			"<",	BINARY_N,	SUB_VS	),	\
	PICK(	GT_OP,				"gt",		'>',			">",	BINARY_N,	SUB_S	),	\
	PICK(	GT_V_OP,			"gtv",		'>',			">",	BINARY_N,	SUB_V	),	\
	PICK(	GT_SV_OP,			"gtsv",		'>',			">",	BINARY_N,	SUB_SV	),	\
	PICK(	GT_VS_OP,			"gtvs",		'>',			">",	BINARY_N,	SUB_VS	),	\
	PICK(	LE_OP,				"le",		LE_SY,			"<=",	BINARY_N,	SUB_S	),	\
	PICK(	LE_V_OP,			"lev",		LE_SY,			"<=",	BINARY_N,	SUB_V	),	\
	PICK(	LE_SV_OP,			"lesv",		LE_SY,			"<=",	BINARY_N,	SUB_SV	),	\
	PICK(	LE_VS_OP,			"levs",		LE_SY,			"<=",	BINARY_N,	SUB_VS	),	\
	PICK(	GE_OP,				"ge",		GE_SY,			">=",	BINARY_N,	SUB_S	),	\
	PICK(	GE_V_OP,			"gev",		GE_SY,			">=",	BINARY_N,	SUB_V	),	\
	PICK(	GE_SV_OP,			"gesv",		GE_SY,			">=",	BINARY_N,	SUB_SV	),	\
	PICK(	GE_VS_OP,			"gevs",		GE_SY,			">=",	BINARY_N,	SUB_VS	),	\
	PICK(	EQ_OP,				"eq",		EQ_SY,			"==",	BINARY_N,	SUB_S	),	\
	PICK(	EQ_V_OP,			"eqv",		EQ_SY,			"==",	BINARY_N,	SUB_V	),	\
	PICK(	EQ_SV_OP,			"eqsv",		EQ_SY,			"==",	BINARY_N,	SUB_SV	),	\
	PICK(	EQ_VS_OP,			"eqvs",		EQ_SY,			"==",	BINARY_N,	SUB_VS	),	\
	PICK(	NE_OP,				"ne",		NE_SY,			"!=",	BINARY_N,	SUB_S	),	\
	PICK(	NE_V_OP,			"nev",		NE_SY,			"!=",	BINARY_N,	SUB_V	),	\
	PICK(	NE_SV_OP,			"nesv",		NE_SY,			"!=",	BINARY_N,	SUB_SV	),	\
	PICK(	NE_VS_OP,			"nevs",		NE_SY,			"!=",	BINARY_N,	SUB_VS	),	\
	PICK(	AND_OP,				"and",		'&',			"&",	BINARY_N,	SUB_S	),	\
	PICK(	AND_V_OP,			"andv",		'&',			"&",	BINARY_N,	SUB_V	),	\
	PICK(	AND_SV_OP,			"andsv",	'&',			"&",	BINARY_N,	SUB_SV	),	\
	PICK(	AND_VS_OP,			"andvs",	'&',			"&",	BINARY_N,	SUB_VS	),	\
	PICK(	XOR_OP,				"xor",		'^',			"^",	BINARY_N,	SUB_S	),	\
	PICK(	XOR_V_OP,			"xorv",		'^',			"^",	BINARY_N,	SUB_V	),	\
	PICK(	XOR_SV_OP,			"xorsv",	'^',			"^",	BINARY_N,	SUB_SV	),	\
	PICK(	XOR_VS_OP,			"xorvs",	'^',			"^",	BINARY_N,	SUB_VS	),	\
	PICK(	OR_OP,				"or",		'|',			"|",	BINARY_N,	SUB_S	),	\
	PICK(	OR_V_OP,			"orv",		'|',			"|",	BINARY_N,	SUB_V	),	\
	PICK(	OR_SV_OP,			"orsv",		'|',			"|",	BINARY_N,	SUB_SV	),	\
	PICK(	OR_VS_OP,			"orvs",		'|',			"|",	BINARY_N,	SUB_VS	),	\
	PICK(	BAND_OP,			"band",		AND_SY,			"&&",	BINARY_N,	SUB_S	),	\
	PICK(	BAND_V_OP,			"bandv",	AND_SY,			"&&",	BINARY_N,	SUB_V	),	\
	PICK(	BAND_SV_OP,			"bandsv",	AND_SY,			"&&",	BINARY_N,	SUB_SV	),	\
	PICK(	BAND_VS_OP,			"bandvs",	AND_SY,			"&&",	BINARY_N,	SUB_VS	),	\
	PICK(	BOR_OP,				"bor",		OR_SY,			"||",	BINARY_N,	SUB_S	),	\
	PICK(	BOR_V_OP,			"borv",		OR_SY,			"||",	BINARY_N,	SUB_V	),	\
	PICK(	BOR_SV_OP,			"borsv",	OR_SY,			"||",	BINARY_N,	SUB_SV	),	\
	PICK(	BOR_VS_OP,			"borvs",	OR_SY,			"||",	BINARY_N,	SUB_VS	),	\
	PICK(	ASSIGN_OP,			"assign",	'=',			"=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGN_V_OP,		"assignv",	'=',			"=",	BINARY_N,	SUB_V	),	\
	PICK(	ASSIGN_GEN_OP,		"assigngen",'=',			"=",	BINARY_N,	SUB_NONE),	\
	PICK(	ASSIGN_MASKED_KV_OP,"assignm",	'=',			"=",	BINARY_N,	SUB_KV	),	\
	\
	PICK(	ASSIGNMINUS_OP,		"assign-",	ASSIGNMINUS_SY,	"-=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNMOD_OP,		"assign%",	ASSIGNMOD_SY,	"%=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNPLUS_OP,		"assign+",	ASSIGNPLUS_SY,	"+=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNSLASH_OP,		"assign/",	ASSIGNSLASH_SY,	"/=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNSTAR_OP,		"assign*",	ASSIGNSTAR_SY,	"*=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNAND_OP,		"assign&",	ASSIGNAND_SY,	"&=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNOR_OP,		"assign|",	ASSIGNOR_SY,	"|=",	BINARY_N,	SUB_S	),	\
	PICK(	ASSIGNXOR_OP,		"assign^",	ASSIGNXOR_SY,	"^=",	BINARY_N,	SUB_S	),	\
	PICK(	COMMA_OP,			"comma",	',',			0,		BINARY_N,	SUB_NONE),	\
	\
	PICK(	COND_OP,			"cond",		'?',			0,		TRINARY_N,	SUB_S	),	\
	PICK(	COND_V_OP,			"condv",	'?',			0,		TRINARY_N,	SUB_V	),	\
	PICK(	COND_SV_OP,			"condsv",	'?',			0,		TRINARY_N,	SUB_SV	),	\
	PICK(	COND_GEN_OP,		"condgen",	'?',			0,		TRINARY_N,	SUB_NONE),	\
	PICK(	ASSIGN_COND_OP,		"assc",		'@',			0,		TRINARY_N,	SUB_S	),	\
	PICK(	ASSIGN_COND_V_OP,	"asscv",	'@',			0,		TRINARY_N,	SUB_V	),	\
	PICK(	ASSIGN_COND_SV_OP,	"asscsc",	'@',			0,		TRINARY_N,	SUB_VS	),	\
	PICK(	ASSIGN_COND_GEN_OP,	"asscgen",	'@',			0,		TRINARY_N,	SUB_NONE),


// Description of opcode classes:
//
// SSSS:	Size or number of values in an operand:
//	0:	scalar or other (like struct)
//	1 to 4: vector or array dimension
// TTTT:	Base type (see type properties)
//
// _:	0000 0000 0000 0000 0000 0000 0000 TTTT
// V:	0000 0000 0000 0000 0000 0000 SSSS TTTT
// VS: 0000 0000 0000 0000 0000 0000 SSSS TTTT
// SV: 0000 0000 0000 0000 0000 0000 SSSS TTTT
// M:	--- Not used yet, reserved for matrix inner product ---
// VM: 0000 0000 0000 0000 SSS2 0000 SSS1 TTTT - Vector length SSS2, Mat size SSS2 by SSS1
// MV: 0000 0000 0000 0000 SSS2 0000 SSS1 TTTT - Vector length SSS1, Mat size SSS2 by SSS1
// Z:	0000 0000 MMMM-MMMM SSS2 0000 SSS1 TTTT - Swizzle vector/scalar size SSS2 to SSS1
//	CTD -- the above appears to be wrong;
//	should be SSS1 to SSS2
// ZM: MMMM-MMMM-MMMM-MMMM SSS2 SSSR SSS1 TTTT - Swizzle matrix size SSS2 by SSS1 to SSSR
// CS: 0000 0000 0000 0000 0000 TTT2 0000 TTT1 - Cast scalar type base1 to base2
// CV: 0000 0000 0000 0000 0000 TTT2 SSS1 TTT1 - Cast vector
// CM: 0000 0000 0000 0000 SSS2 TTT2 SSS1 TTT1 - Case matrix
// KV: 0000 0000 0000 MMMM 0000 0000 SSSS TTTT - Masked vector write

#undef PICK
#define PICK(a, b, c, d, e, f) a

enum opcode {
	UNKNOWN_OP	= -1,
	OPCODE_TABLE
	LAST_OPCODE,
	// Some useful offsets:
	OFFSET_S_OP = 0, OFFSET_V_OP = 1, OFFSET_SV_OP = 2, OFFSET_VS_OP = 3,
};

#undef PICK

extern const char		*opcode_name[];
extern const int		opcode_atom[];
extern const subopkind	opcode_subopkind[];

#define SUBOP__(base)								((base) & 15)
#define SUBOP_S(base)								((base) & 15)
#define SUBOP_V(size, base)							((((size) & 15) << 4) | ((base) & 15))
#define SUBOP_VS(size, base)						((((size) & 15) << 4) | ((base) & 15))
#define SUBOP_SV(size, base)						((((size) & 15) << 4) | ((base) & 15))
#define SUBOP_MV(size2, size1, base)				((((size2) & 15) << 12) | (((size1) & 15) << 4) | ((base) & 15))
#define SUBOP_VM(size2, size1, base)				((((size2) & 15) << 12) | (((size1) & 15) << 4) | ((base) & 15))
#define SUBOP_Z(mask, size2, size1, base)			(((mask) << 16) | (((size2) & 15) << 12) | (((size1) & 15) << 4) | ((base) & 15))
#define SUBOP_ZM(mask, sizer, size2, size1, base)	(((mask) << 16) | (((size2) & 15) << 12) | (((sizer) & 15) << 8) | (((size1) & 15) << 4) | ((base) & 15))
#define SUBOP_CS(base2, base1)						((((base2) & 15) << 8) | ((base1) & 15))
#define SUBOP_CV(base2, size, base1)				(((((base2) & 15) << 8) | ((size) & 15) << 4) | ((base1) & 15))
#define SUBOP_CM(size2, base2, size1, base1)		((((size2) & 15) << 12) | (((base2) & 15) << 8) | (((size1) & 15) << 4) | ((base1) & 15))
#define SUBOP_KV(mask, size, base)					((((mask) & 15) << 16) | (((size) & 15) << 4) | ((base) & 15))

#define SUBOP_SET_T(subop, base)		((subop) = ((subop) & ~15) | ((base) & 15))
#define SUBOP_SET_MASK(subop, mask)		((subop) = ((subop) & ~0x00ff0000) | (((mask) & 0xff) << 16))
#define SUBOP_SET_MASK16(subop, mask)	((subop) = ((subop) & ~0xffff0000) | (((mask) & 0xffff) << 16))

#define SUBOP_GET_S(subop)				(((subop) >> 4) & 15)
#define SUBOP_GET_T(subop)				((subop) & 15)
#define SUBOP_GET_S1(subop)				(((subop) >> 4) & 15)
#define SUBOP_GET_S2(subop)				(((subop) >> 12) & 15)
#define SUBOP_GET_SR(subop)				(((subop) >> 8) & 15)
#define SUBOP_GET_T1(subop)				((subop) & 15)
#define SUBOP_GET_T2(subop)				(((subop) >> 8) & 15)
#define SUBOP_GET_MASK(subop)			(((subop) >> 16) & 0xff)
#define SUBOP_GET_MASK16(subop)			(((subop) >> 16) & 0xffff)

#define PRAGMA_HASH_STR "#"
#define COMMENT_CPP_STR "//"

struct attr {
	int		name;
	int		num_params;
	int		p0, p1;
	attr	*next;
};

typedef unsigned int uint;

struct half {
	short i;
	operator float() const;
	half &operator=(float f);
};

struct fixed_as_float {
	float f;
	operator float() const {
		return f;
	}
	void	operator=(float v) {
		float	t = v * 1024;
		f = int(t > 2047 ? 2047 : t < -2048 ? -2048 : t + 0.5f) / 1024.0f;
	}
};

struct half_as_float {
	float	f;
	operator float() const {
		return f;
	}
	void	operator=(float v) {
		half h;
		f = h = v;
	}
};

union scalar_constant {
	float			f;
	int				i;
	uint			u;
	bool			b;
	half_as_float	h;
	fixed_as_float	x;
	template<typename T> T& as();
	template<typename T> T as() const { return const_cast<scalar_constant*>(this)->as<T>(); }
};

template<> inline float&			scalar_constant::as<float			>() { return f; }
template<> inline int&				scalar_constant::as<int				>() { return i; }
template<> inline uint&				scalar_constant::as<uint			>() { return u; }
template<> inline bool&				scalar_constant::as<bool			>() { return b; }
template<> inline half_as_float&	scalar_constant::as<half_as_float	>() { return h; }
template<> inline fixed_as_float&	scalar_constant::as<fixed_as_float	>() { return x; }

struct expr {
	nodekind	kind;
	opcode		op;
	Type		*type;
	bool		is_lvalue;
	bool		is_const;
	bool		has_sideeffects;
	void		*tempptr[4];	// Used by backends
	
	struct symb {
		Symbol		*symbol;
	};
	struct constant {
		int		subop;
		scalar_constant val[4];
	};
	struct unary {
		int		subop;
		expr	*arg;
	};
	struct binary {
		int		subop;
		expr	*left, *right;
	};
	struct trinary {
		int		subop;
		expr	*arg1, *arg2, *arg3;
	};
	struct symbolic {
		int name;
		int	name2;
	};

	union {
		symb		sym;
		constant	co;
		unary		un;
		binary		bin;
		trinary		tri;
		symbolic	blx;
	};
	expr(nodekind _kind);
	expr(opcode op, nodekind _kind);
};

// statements

struct stmt {
	stmtkind	kind;
	stmt		*next;
	attr		*attributes;
	SourceLoc	loc;

	struct expr_stmt {
		expr		*exp;
	};
	struct if_stmt {
		expr		*cond;
		stmt		*thenstmt;
		stmt		*elsestmt;
	};
	struct while_stmt {
		expr		*cond;
		stmt		*body;
	};
	struct for_stmt {
		stmt		*init;
		expr		*cond;
		stmt		*step;
		stmt		*body;
	};
	struct block_stmt {
		stmt		*body;
		Scope		*scope;
	};
	struct return_stmt {
		expr		*exp;
	};
	struct discard_stmt {
		expr		*cond;
	};
	struct comment_stmt {
		int			str;
	};
	union {
		expr_stmt		exprst;
		if_stmt			ifst;
		while_stmt		whilest;
		for_stmt		forst;
		block_stmt		blockst;
		return_stmt		returnst;
		discard_stmt	discardst;
		comment_stmt	commentst;
	};

	stmt(stmtkind _kind, SourceLoc &_loc);
	
	friend NextIterator<stmt> begin(stmt *s)	{ return s; }
	friend NextIterator<stmt> end(stmt *s)		{ return nullptr; }
};

// Possibly derived, stack-resident pointer to and copy of a type.
struct dtype {
	Type		*basetype;		// Pointer to non-derived
	bool		is_derived;		// TRUE if anything has been altered
	int			num_new_dims;	// Number of new dimensions added for this declarator
	StorageClass storage;		// Aplied to variables when defined, not part of the type
	Type		type;			// Local copy of type
};

// For declaration parsing
struct decl {
	decl		*next;
	nodekind	kind;
	SourceLoc	loc;		// Location for error reporting
	int			name;		// Symbol name atom
	int			semantics;
	dtype		type;		// Type collected while parsing
	Symbol		*symb;		// Symbol table definition of actual object
	decl		*params;	// Actual paramaters to function declaration
	expr		*initexpr;	// Initializer
	attr		*attributes;
	decl(SourceLoc &_loc, int _name, dtype *_type);
};

struct StmtList {
	stmt *first;
	stmt *last;
	StmtList() : first(0), last(0) {}
};

bool	IsNullSwizzle(int mask, int len);
bool	AllSameRow(int mask, int len);
bool	AllSameCol(int mask, int len);
int		GetRowSwizzle(int mask, int len);
int		GetColSwizzle(int mask, int len);

decl	*NewDeclNode(SourceLoc &loc, int atom, dtype *type);
int		GetOperatorName(CG *cg, int op);

expr	*DupNode(MemoryPool *pool, expr *e);
expr	*NewSymbNode(CG *cg, opcode op, Symbol *fSymb);
expr	*NewIConstNode(CG *cg, opcode op, int fval, int base);
expr	*NewBConstNode(CG *cg, opcode op, int fval, int base);
expr	*NewFConstNode(CG *cg, opcode op, float fval, int base);
expr	*NewFConstNodeV(CG *cg, opcode op, float *fval, int len, int base);
expr	*NewUnopNode(CG *cg, opcode op, expr *arg);
expr	*NewUnopSubNode(CG *cg, opcode op, int subop, expr *arg);
expr	*NewBinopNode(CG *cg, opcode op, expr *left, expr *right);
expr	*NewBinopSubNode(CG *cg, opcode op, int subop, expr *left, expr *right);
expr	*NewTriopNode(CG *cg, opcode op, expr *arg1, expr *arg2, expr *arg3);
expr	*NewTriopSubNode(CG *cg, opcode op, int subop, expr *arg1, expr *arg2, expr *arg3);
expr	*SymbolicConstant(CG *cg, int name, int name2);
expr	*BasicVariable(CG *cg, int name);

stmt	*NewExprStmt(SourceLoc &loc, expr *fExpr);
stmt	*NewIfStmt(SourceLoc &loc, expr *fExpr, stmt *thenStmt, stmt *elseStmt);
stmt	*SetThenElseStmts(stmt *ifStmt, stmt *thenStmt, stmt *elseStmt);
stmt	*NewSwitchStmt(SourceLoc &loc, expr *fExpr, stmt *fStmt, Scope *fScope);
stmt	*NewWhileStmt(SourceLoc &loc, stmtkind kind, expr *fExpr, stmt *body);
stmt	*NewForStmt(SourceLoc &loc, stmt *fExpr1, expr *fExpr2, stmt *fExpr3, stmt *body);
stmt	*NewBlockStmt(SourceLoc &loc, stmt *fStmt, Scope *fScope);
stmt	*NewReturnStmt(CG *cg, SourceLoc &loc, Scope *fScope, expr *fExpr);
stmt	*NewDiscardStmt(SourceLoc &loc, expr *fExpr);
stmt	*NewBreakStmt(SourceLoc &loc);
stmt	*NewCommentStmt(CG *cg, SourceLoc &loc, const char *str);

/************************************* dtype functions: *************************************/

Type	*GetTypePointer(CG *cg, SourceLoc &loc, const dtype *fDtype);
dtype	*SetDType(dtype *fDtype, Type *fType);
dtype	*NewDType(dtype *fDtype, Type *baseType, int category);

/********************************** Parser Semantic Rules: ***********************************/

expr	*Initializer(CG *cg, expr *fExpr);
expr	*StateInitializer(CG *cg, int name, expr *fExpr);
expr	*InitializerList(expr *list, expr *fExpr);
expr	*ArgumentList(CG *cg, expr *flist, expr *fExpr);
expr	*ExpressionList(CG *cg, expr *flist, expr *fExpr);
TypeList*AddtoTypeList(CG *cg, TypeList *list, Type *type);
decl	*AddDecl(decl *first, decl *last);
stmt	*AddStmt(stmt *first, stmt *last);
stmt	*CheckStmt(stmt *fStmt);

decl	*Function_Definition_Header(CG *cg, decl *fDecl);
decl	*Param_Init_Declarator(CG *cg, decl *fDecl, expr *fExpr);
stmt	*Init_Declarator(CG *cg, decl *fDecl, expr *fExpr);
decl	*Declarator(CG *cg, decl *fDecl, int semantics, int reg);
decl	*Array_Declarator(CG *cg, decl *fDecl, int size, int Empty);

attr	*Attribute(int name, int num_params, int p0, int p1);
stmt	*AddStmtAttribute(stmt *fStmt, attr *fAttr);
decl	*AddDeclAttribute(decl *fDecl, attr *fAttr);

Symbol	*AddFormalParamDecls(CG *cg, Scope *fScope, decl *params);
decl	*SetFunTypeParams(CG *cg, decl *func, decl *params, decl *actuals);
decl	*FunctionDeclHeader(CG *cg, SourceLoc &loc, Scope *fScope, decl *func);

Type	*StructHeader(CG *cg, SourceLoc &loc, Scope *fScope, int semantics, int tag);
Type*	AddStructBase(Type *fType, Type *fBase);
Type	*TemplateHeader(CG *cg, SourceLoc &loc, Scope *fScope, int tag, decl *params);
void	SetTemplateParams(Type *type);
Type	*EnumHeader(CG *cg, SourceLoc &loc, Scope *fScope, int tag);
void	EnumAdd(CG *cg, SourceLoc &loc, Scope *fScope, Type *lType, int id, int val);

Symbol	*DefineVar(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType);
Symbol	*DefineTypedef(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType);
Symbol	*DeclareFunc(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *fSymb, int atom, Type *fType, Scope *locals, Symbol *params);
Symbol	*ConstantBuffer(CG *cg, SourceLoc &loc, Scope *fScope, int reg, int tag);
void	DefineFunction(CG *cg, decl *func, stmt *body);
void	DefineTechnique(CG *cg, int atom, expr *passes, stmt *anno);
int		GlobalInitStatements(Scope *fScope, stmt *fStmt);

bool	is_lvalue(const expr *fExpr);
bool	is_const(const expr *fExpr);
bool	IsArrayIndex(const expr *fExpr);
bool	ConvertType(CG *cg, expr *fExpr, Type *toType, Type *fromType, expr **result, bool IgnorePacked, bool Explicit);
expr	*CastScalarVectorMatrix(CG *cg, expr *fExpr, int fbase, int tbase, int len, int len2);
int		ConvertNumericOperands(CG *cg, int baseop, expr **lexpr, expr **rexpr, int lbase, int rbase, int llen, int rlen, int llen2, int rlen2);
expr	*CheckBooleanExpr(CG *cg, SourceLoc &loc, expr *fExpr, int *AllowVector);

expr	*NewUnaryOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *fExpr, int IntegralOnly);
expr	*NewBinaryOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *lExpr, expr *rExpr, int IntegralOnly);
expr	*NewBinaryBooleanOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *lExpr, expr *rExpr);
expr	*NewBinaryComparisonOperator(CG *cg, SourceLoc &loc, opcode fop, int name, expr *lExpr, expr *rExpr);
expr	*NewConditionalOperator(CG *cg, SourceLoc &loc, expr *bExpr, expr *lExpr, expr *rExpr);
expr	*NewSwizzleOperator(CG *cg, SourceLoc &loc, expr *fExpr, int ident);
expr	*NewMatrixSwizzleOperator(CG *cg, SourceLoc &loc, expr *fExpr, int ident);
expr	*NewConstructor(CG *cg, SourceLoc &loc, Type *fType, expr *fExpr);
expr	*NewCastOperator(CG *cg, SourceLoc &loc, expr *fExpr, Type *toType);
expr	*NewMemberSelectorOrSwizzleOrWriteMaskOperator(CG *cg, SourceLoc &loc, expr *fExpr, int ident);
expr	*NewIndexOperator(CG *cg, SourceLoc &loc, expr *fExpr, expr *ixExpr);
expr	*NewFunctionCallOperator(CG *cg, SourceLoc &loc, expr *funExpr, expr *actuals);

expr	*NewSimpleAssignment(CG *cg, SourceLoc &loc, expr *fvar, expr *fExpr, bool InInit);
stmt	*NewSimpleAssignmentStmt(CG *cg, SourceLoc &loc, expr *fvar, expr *fExpr, bool InInit);
stmt	*NewCompoundAssignmentStmt(CG *cg, SourceLoc &loc, opcode op, expr *fvar, expr *fExpr);

stmt	*ParseAsm(CG *cg, SourceLoc &loc);

void	InitTempptr(expr *e, void *arg1, int arg2);

void	RecordErrorPos(SourceLoc &loc);
void	CheckAllErrorsGenerated(void);

struct ErrorLoc : SourceLoc	{
	bool		hit;	// an error was found on this line or before this line after the previous error token.
	ErrorLoc	*next;
	ErrorLoc(const SourceLoc &loc) : SourceLoc(loc), hit(false), next(0) {}
};

// Beginning and end of list of error locations, sorted by line.
extern ErrorLoc *ErrorLocsFirst;
extern ErrorLoc *ErrorLocsLast;

} //namespace cgc

#endif // SUPPORT_H
