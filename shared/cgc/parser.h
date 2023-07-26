#ifndef _yy_defines_h_
#define _yy_defines_h_

#ifndef YYDEBUG
#define YYDEBUG 0
#endif
typedef int YYINT;
#define AND_SY 257
#define ASM_SY 258
#define ASSIGNMINUS_SY 259
#define ASSIGNMOD_SY 260
#define ASSIGNPLUS_SY 261
#define ASSIGNSLASH_SY 262
#define ASSIGNSTAR_SY 263
#define BOOLEAN_SY 264
#define BREAK_SY 265
#define CASE_SY 266
#define CFLOATCONST_SY 267
#define COLONCOLON_SY 268
#define CONST_SY 269
#define CONTINUE_SY 270
#define DEFAULT_SY 271
#define DISCARD_SY 272
#define DO_SY 273
#define EQ_SY 274
#define ELSE_SY 275
#define ERROR_SY 276
#define EXTERN_SY 277
#define FLOAT_SY 278
#define FLOATCONST_SY 279
#define FLOATHCONST_SY 280
#define FLOATXCONST_SY 281
#define FOR_SY 282
#define GE_SY 283
#define GG_SY 284
#define GOTO_SY 285
#define IDENT_SY 286
#define PASTING_SY 287
#define IF_SY 288
#define IN_SY 289
#define INLINE_SY 290
#define INOUT_SY 291
#define INT_SY 292
#define UNSIGNED_SY 293
#define INTCONST_SY 294
#define UINTCONST_SY 295
#define INTERNAL_SY 296
#define LE_SY 297
#define LL_SY 298
#define MINUSMINUS_SY 299
#define NE_SY 300
#define OR_SY 301
#define OUT_SY 302
#define PACKED_SY 303
#define PLUSPLUS_SY 304
#define RETURN_SY 305
#define STATIC_SY 306
#define STRCONST_SY 307
#define STRUCT_SY 308
#define SWITCH_SY 309
#define TEXOBJ_SY 310
#define THIS_SY 311
#define TYPEDEF_SY 312
#define TYPEIDENT_SY 313
#define TEMPLATEIDENT_SY 314
#define UNIFORM_SY 315
#define VARYING_SY 316
#define VOID_SY 317
#define WHILE_SY 318
#define SAMPLERSTATE_SY 319
#define TECHNIQUE_SY 320
#define PASS_SY 321
#define VERTEXSHADER_SY 322
#define PIXELSHADER_SY 323
#define COMPILE_SY 324
#define ROWMAJOR_SY 325
#define COLMAJOR_SY 326
#define NOINTERP_SY 327
#define PRECISE_SY 328
#define SHARED_SY 329
#define GROUPSHARED_SY 330
#define VOLATILE_SY 331
#define REGISTER_SY 332
#define ENUM_SY 333
#define LOWP_SY 334
#define MEDIUMP_SY 335
#define HIGHP_SY 336
#define CBUFFER_SY 337
#define TEMPLATE_SY 338
#define OPERATOR_SY 339
#define FIRST_USER_TOKEN_SY 340
#define YYERRCODE 256

#if YYDEBUG
extern int yydebug;
#endif
extern int YYPARSE_DECL();
#if !defined(YYSTYPE) && !defined(YYSTYPE_IS_DECLARED)
#line 24 "parser.y"
typedef union {
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
#define YYSTYPE_IS_DECLARED 1
#endif
#line 120 "parser.h"

#endif /* _yy_defines_h_ */
