
#line 7 "isocmd\\separate.template.cpp"
#ifndef _YYDEF_H
#define _YYDEF_H


#line 2 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
#include <stdio.h>
#include <stdlib.h>

#include "cg.h"
#include "compile.h"
#include "printutils.h"
#include "errors.h"

namespace cgc {
void yyerror(CG *cg, const char *s);
int yylex(YYSTYPE *lvalp, CG *cg);
}

using namespace cgc;

#line 11 "isocmd\\separate.template.cpp"

#define YYPREFIX 
#line 14 "isocmd\\separate.template.cpp"
typedef int YYINT;
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
enum yytokentype {
	YYEOF = 0,
	YYERRCODE = 256,
	AND_SY = 257,
	ASM_SY = 258,
	ASSIGNMINUS_SY = 259,
	ASSIGNMOD_SY = 260,
	ASSIGNPLUS_SY = 261,
	ASSIGNSLASH_SY = 262,
	ASSIGNSTAR_SY = 263,
	BOOLEAN_SY = 264,
	BREAK_SY = 265,
	CASE_SY = 266,
	CFLOATCONST_SY = 267,
	COLONCOLON_SY = 268,
	CONST_SY = 269,
	CONTINUE_SY = 270,
	DEFAULT_SY = 271,
	DISCARD_SY = 272,
	DO_SY = 273,
	EQ_SY = 274,
	ELSE_SY = 275,
	ERROR_SY = 276,
	EXTERN_SY = 277,
	FLOAT_SY = 278,
	FLOATCONST_SY = 279,
	FLOATHCONST_SY = 280,
	FLOATXCONST_SY = 281,
	FOR_SY = 282,
	GE_SY = 283,
	GG_SY = 284,
	GOTO_SY = 285,
	IDENT_SY = 286,
	PASTING_SY = 287,
	IF_SY = 288,
	IN_SY = 289,
	INLINE_SY = 290,
	INOUT_SY = 291,
	INT_SY = 292,
	UNSIGNED_SY = 293,
	INTCONST_SY = 294,
	UINTCONST_SY = 295,
	INTERNAL_SY = 296,
	LE_SY = 297,
	LL_SY = 298,
	MINUSMINUS_SY = 299,
	NE_SY = 300,
	OR_SY = 301,
	OUT_SY = 302,
	PACKED_SY = 303,
	PLUSPLUS_SY = 304,
	RETURN_SY = 305,
	STATIC_SY = 306,
	STRCONST_SY = 307,
	STRUCT_SY = 308,
	SWITCH_SY = 309,
	TEXOBJ_SY = 310,
	THIS_SY = 311,
	TYPEDEF_SY = 312,
	TYPEIDENT_SY = 313,
	TEMPLATEIDENT_SY = 314,
	UNIFORM_SY = 315,
	VARYING_SY = 316,
	VOID_SY = 317,
	WHILE_SY = 318,
	SAMPLERSTATE_SY = 319,
	TECHNIQUE_SY = 320,
	PASS_SY = 321,
	VERTEXSHADER_SY = 322,
	PIXELSHADER_SY = 323,
	COMPILE_SY = 324,
	ROWMAJOR_SY = 325,
	COLMAJOR_SY = 326,
	NOINTERP_SY = 327,
	PRECISE_SY = 328,
	SHARED_SY = 329,
	GROUPSHARED_SY = 330,
	VOLATILE_SY = 331,
	REGISTER_SY = 332,
	ENUM_SY = 333,
	LOWP_SY = 334,
	MEDIUMP_SY = 335,
	HIGHP_SY = 336,
	CBUFFER_SY = 337,
	TEMPLATE_SY = 338,
	OPERATOR_SY = 339,
	FIRST_USER_TOKEN_SY = 340,
	AND_expression = 341,

};
#endif
/* Parameters sent to yyparse. */
#ifndef YYPARSE_PARAMS
# define YYPARSE_PARAMS	, CG *cg
# define YYPARSE_ARGS	, cg
#endif
# define YYPARSE_DECL() yyparse(CG *cg)
#line 30 "isocmd\\separate.template.cpp"

/* Parameters sent to yylex. */
#ifndef YYLEX_PARAMS
# define YYLEX_PARAMS	, CG *cg
# define YYLEX_ARGS		, cg
#endif
/* Parameters sent to yyerror. */
#ifndef YYERROR_PARAMS
# define YYERROR_PARAMS	CG *cg, 
# define YYERROR_ARGS	cg, 
#endif
#if YYDEBUG
extern int yydebug;
#endif
#if !defined(YYSTYPE) && !defined(YYSTYPE_IS_DECLARED)
#line 49 "isocmd\\separate.template.cpp"
typedef union YYSTYPE 
#line 23 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
	int			sc_token;
	int			sc_int;
	float		sc_fval;
	int			sc_ident;
	spec		sc_specifiers;
	dtype		sc_type;
	Type		*sc_ptype;
	TypeList	*sc_typelist;
	decl		*sc_decl;
	expr		*sc_expr;
	stmt		*sc_stmt;
	attr		*sc_attr;
	Symbol		*sc_sym;
	/* Dummy place holder: */
	int	dummy;
} YYSTYPE;
#line 53 "isocmd\\separate.template.cpp"
#define YYSTYPE_IS_DECLARED 1
#endif
#endif // YYDEF_H