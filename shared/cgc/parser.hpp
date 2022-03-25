/* A Bison parser, made by GNU Bison 3.0.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_PARSER_HPP_INCLUDED
# define YY_YY_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    AND_SY = 258,
    ASM_SY = 259,
    ASSIGNMINUS_SY = 260,
    ASSIGNMOD_SY = 261,
    ASSIGNPLUS_SY = 262,
    ASSIGNSLASH_SY = 263,
    ASSIGNSTAR_SY = 264,
    ASSIGNAND_SY = 265,
    ASSIGNOR_SY = 266,
    ASSIGNXOR_SY = 267,
    BOOLEAN_SY = 268,
    BREAK_SY = 269,
    CASE_SY = 270,
    CFLOATCONST_SY = 271,
    COLONCOLON_SY = 272,
    CONST_SY = 273,
    CONTINUE_SY = 274,
    DEFAULT_SY = 275,
    DISCARD_SY = 276,
    DO_SY = 277,
    EQ_SY = 278,
    ELSE_SY = 279,
    ERROR_SY = 280,
    EXTERN_SY = 281,
    FLOAT_SY = 282,
    FLOATCONST_SY = 283,
    FLOATHCONST_SY = 284,
    FLOATXCONST_SY = 285,
    FOR_SY = 286,
    GE_SY = 287,
    GG_SY = 288,
    GOTO_SY = 289,
    IDENT_SY = 290,
    PASTING_SY = 291,
    IF_SY = 292,
    IN_SY = 293,
    INLINE_SY = 294,
    INOUT_SY = 295,
    INT_SY = 296,
    UNSIGNED_SY = 297,
    INTCONST_SY = 298,
    UINTCONST_SY = 299,
    INTERNAL_SY = 300,
    LE_SY = 301,
    LL_SY = 302,
    MINUSMINUS_SY = 303,
    NE_SY = 304,
    OR_SY = 305,
    OUT_SY = 306,
    PACKED_SY = 307,
    PLUSPLUS_SY = 308,
    RETURN_SY = 309,
    STATIC_SY = 310,
    STRCONST_SY = 311,
    STRUCT_SY = 312,
    SWITCH_SY = 313,
    TEXOBJ_SY = 314,
    THIS_SY = 315,
    TYPEDEF_SY = 316,
    TYPEIDENT_SY = 317,
    TEMPLATEIDENT_SY = 318,
    UNIFORM_SY = 319,
    VARYING_SY = 320,
    VOID_SY = 321,
    WHILE_SY = 322,
    SAMPLERSTATE_SY = 323,
    TECHNIQUE_SY = 324,
    PASS_SY = 325,
    VERTEXSHADER_SY = 326,
    PIXELSHADER_SY = 327,
    COMPILE_SY = 328,
    ROWMAJOR_SY = 329,
    COLMAJOR_SY = 330,
    NOINTERP_SY = 331,
    PRECISE_SY = 332,
    SHARED_SY = 333,
    GROUPSHARED_SY = 334,
    VOLATILE_SY = 335,
    REGISTER_SY = 336,
    ENUM_SY = 337,
    LOWP_SY = 338,
    MEDIUMP_SY = 339,
    HIGHP_SY = 340,
    CBUFFER_SY = 341,
    TEMPLATE_SY = 342,
    OPERATOR_SY = 343,
    FIRST_USER_TOKEN_SY = 344
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 24 "parser.y" /* yacc.c:1909  */

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

#line 162 "parser.hpp" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (CG *cg);

#endif /* !YY_YY_PARSER_HPP_INCLUDED  */
