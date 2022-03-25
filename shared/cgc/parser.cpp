/* A Bison parser, made by GNU Bison 3.0.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "parser.y" /* yacc.c:339  */

#include <stdio.h>
#include <stdlib.h>

#include "cg.h"
#include "compile.h"
#include "printutils.h"
#include "errors.h"

namespace cgc {
int yyparse (CG *cg);
void yyerror(CG *cg, const char *s);
int yylex(YYSTYPE *lvalp, CG *cg);
}

using namespace cgc;

#line 84 "parser.cpp" /* yacc.c:339  */

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "parser.hpp".  */
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
#line 24 "parser.y" /* yacc.c:355  */

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

#line 232 "parser.cpp" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (CG *cg);

#endif /* !YY_YY_PARSER_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 246 "parser.cpp" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if (! defined __GNUC__ || __GNUC__ < 2 \
      || (__GNUC__ == 2 && __GNUC_MINOR__ < 5))
#  define __attribute__(Spec) /* empty */
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  84
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2934

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  114
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  131
/* YYNRULES -- Number of rules.  */
#define YYNRULES  334
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  561

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   344

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   104,     2,     2,     2,   108,   109,     2,
      95,    96,   106,   102,    93,   103,   112,   107,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    97,    90,
      98,    94,    99,   113,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   100,     2,   101,   110,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    91,   111,    92,   105,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   269,   269,   270,   277,   279,   281,   283,   285,   288,
     290,   292,   296,   300,   301,   302,   307,   309,   311,   313,
     315,   317,   319,   321,   323,   325,   327,   329,   334,   336,
     338,   340,   342,   344,   346,   350,   352,   356,   358,   366,
     368,   372,   376,   378,   380,   384,   388,   389,   391,   395,
     403,   405,   407,   409,   411,   413,   415,   417,   419,   421,
     423,   434,   442,   450,   452,   454,   456,   458,   460,   462,
     470,   472,   480,   482,   484,   493,   495,   497,   501,   506,
     508,   510,   514,   515,   518,   523,   524,   527,   528,   534,
     537,   541,   543,   551,   551,   553,   556,   558,   562,   561,
     567,   569,   573,   575,   583,   585,   585,   585,   585,   589,
     590,   594,   596,   600,   601,   608,   608,   609,   609,   610,
     613,   617,   621,   622,   625,   627,   635,   635,   640,   641,
     648,   650,   652,   654,   662,   663,   666,   668,   670,   672,
     676,   678,   682,   684,   686,   688,   690,   694,   696,   700,
     701,   702,   703,   704,   705,   706,   707,   708,   709,   710,
     711,   712,   713,   714,   715,   716,   717,   718,   719,   720,
     724,   725,   727,   744,   746,   750,   752,   754,   759,   760,
     764,   770,   782,   784,   786,   788,   792,   794,   798,   800,
     804,   806,   810,   812,   813,   824,   826,   830,   838,   839,
     840,   842,   850,   851,   853,   855,   857,   859,   864,   865,
     868,   870,   874,   876,   884,   885,   887,   889,   891,   893,
     895,   903,   907,   915,   916,   918,   920,   928,   929,   931,
     939,   940,   942,   950,   951,   953,   955,   957,   965,   966,
     968,   976,   977,   985,   986,   994,   995,  1003,  1004,  1012,
    1013,  1021,  1022,  1026,  1034,  1045,  1047,  1052,  1054,  1062,
    1064,  1065,  1068,  1069,  1070,  1071,  1072,  1073,  1074,  1075,
    1076,  1079,  1080,  1087,  1094,  1096,  1106,  1114,  1118,  1120,
    1124,  1132,  1136,  1138,  1142,  1143,  1151,  1153,  1157,  1161,
    1169,  1170,  1174,  1175,  1183,  1184,  1188,  1190,  1192,  1194,
    1196,  1198,  1200,  1202,  1204,  1206,  1214,  1216,  1218,  1222,
    1224,  1229,  1233,  1235,  1238,  1239,  1251,  1253,  1256,  1258,
    1265,  1267,  1275,  1278,  1281,  1284,  1287,  1290,  1292,  1294,
    1296,  1298,  1300,  1302,  1306
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "AND_SY", "ASM_SY", "ASSIGNMINUS_SY",
  "ASSIGNMOD_SY", "ASSIGNPLUS_SY", "ASSIGNSLASH_SY", "ASSIGNSTAR_SY",
  "ASSIGNAND_SY", "ASSIGNOR_SY", "ASSIGNXOR_SY", "BOOLEAN_SY", "BREAK_SY",
  "CASE_SY", "CFLOATCONST_SY", "COLONCOLON_SY", "CONST_SY", "CONTINUE_SY",
  "DEFAULT_SY", "DISCARD_SY", "DO_SY", "EQ_SY", "ELSE_SY", "ERROR_SY",
  "EXTERN_SY", "FLOAT_SY", "FLOATCONST_SY", "FLOATHCONST_SY",
  "FLOATXCONST_SY", "FOR_SY", "GE_SY", "GG_SY", "GOTO_SY", "IDENT_SY",
  "PASTING_SY", "IF_SY", "IN_SY", "INLINE_SY", "INOUT_SY", "INT_SY",
  "UNSIGNED_SY", "INTCONST_SY", "UINTCONST_SY", "INTERNAL_SY", "LE_SY",
  "LL_SY", "MINUSMINUS_SY", "NE_SY", "OR_SY", "OUT_SY", "PACKED_SY",
  "PLUSPLUS_SY", "RETURN_SY", "STATIC_SY", "STRCONST_SY", "STRUCT_SY",
  "SWITCH_SY", "TEXOBJ_SY", "THIS_SY", "TYPEDEF_SY", "TYPEIDENT_SY",
  "TEMPLATEIDENT_SY", "UNIFORM_SY", "VARYING_SY", "VOID_SY", "WHILE_SY",
  "SAMPLERSTATE_SY", "TECHNIQUE_SY", "PASS_SY", "VERTEXSHADER_SY",
  "PIXELSHADER_SY", "COMPILE_SY", "ROWMAJOR_SY", "COLMAJOR_SY",
  "NOINTERP_SY", "PRECISE_SY", "SHARED_SY", "GROUPSHARED_SY",
  "VOLATILE_SY", "REGISTER_SY", "ENUM_SY", "LOWP_SY", "MEDIUMP_SY",
  "HIGHP_SY", "CBUFFER_SY", "TEMPLATE_SY", "OPERATOR_SY",
  "FIRST_USER_TOKEN_SY", "';'", "'{'", "'}'", "','", "'='", "'('", "')'",
  "':'", "'<'", "'>'", "'['", "']'", "'+'", "'-'", "'!'", "'~'", "'*'",
  "'/'", "'%'", "'&'", "'^'", "'|'", "'.'", "'?'", "$accept",
  "compilation_unit", "external_declaration", "declaration",
  "abstract_declaration", "declaration_specifiers",
  "abstract_declaration_specifiers", "abstract_declaration_specifiers2",
  "init_declarator_list", "init_declarator", "pass_list", "pass",
  "pass_item_list", "pass_item", "pass_state_value", "assembly",
  "type_specifier", "type_qualifier", "type_domain", "storage_class",
  "function_specifier", "in_out", "struct_or_connector_specifier",
  "struct_compound_header", "struct_or_connector_header",
  "struct_identifier", "untagged_struct_header", "struct_declaration_list",
  "struct_declaration", "cbuffer_decl", "cbuffer_compound_header",
  "cbuffer_header", "template_decl", "$@1", "template_decl_header",
  "template_params", "$@2", "template_param_list", "template_param",
  "templated_type", "$@3", "$@4", "$@5", "template_arg_list",
  "non_empty_template_arg_list", "template_arg", "enum_specifier", "$@6",
  "$@7", "enum_header", "untagged_enum_header", "enum_declaration_list",
  "enum_declaration", "annotation", "$@8", "annotation_decl_list",
  "attribute", "declarator", "semantic_declarator", "register_spec",
  "basic_declarator", "function_decl_header", "operator",
  "abstract_declarator", "parameter_list", "parameter_declaration",
  "abstract_parameter_list", "non_empty_abstract_parameter_list",
  "initializer", "initializer_list", "state_list", "state", "state_value",
  "variable", "basic_variable", "primary_expression", "postfix_expression",
  "actual_argument_list", "non_empty_argument_list", "expression_list",
  "unary_expression", "cast_expression", "multiplicative_expression",
  "additive_expression", "shift_expression", "relational_expression",
  "equality_expression", "AND_expression", "exclusive_OR_expression",
  "inclusive_OR_expression", "logical_AND_expression",
  "logical_OR_expression", "conditional_expression", "conditional_test",
  "expression", "function_definition", "function_definition_header",
  "statement", "balanced_statement", "dangling_statement", "asm_statement",
  "discard_statement", "break_statement", "if_statement", "dangling_if",
  "if_header", "switch_statement", "labeled_statement", "switch_item_list",
  "compound_statement", "compound_header", "compound_tail",
  "block_item_list", "block_item", "expression_statement",
  "expression_statement2", "iteration_statement", "dangling_iteration",
  "boolean_scalar_expression", "for_expression_opt", "for_expression",
  "for_expression_init", "boolean_expression_opt", "return_statement",
  "member_identifier", "scope_identifier", "semantics_identifier",
  "variable_identifier", "identifier", "constant", "constant_expression", YY_NULL
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
      59,   123,   125,    44,    61,    40,    41,    58,    60,    62,
      91,    93,    43,    45,    33,   126,    42,    47,    37,    38,
      94,   124,    46,    63
};
# endif

#define YYPACT_NINF -407

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-407)))

#define YYTABLE_NINF -324

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    2157,  -407,  -407,  -407,     4,  -407,  -407,  -407,  -407,  -407,
    -407,    70,  -407,  -407,  2849,  -407,    27,  -407,  2849,  -407,
      36,  -407,  -407,    90,  2849,  2849,  -407,  -407,  -407,  -407,
    -407,    27,  2849,  2849,  2849,    27,    45,    90,  2077,  -407,
    -407,     9,  -407,  1707,  -407,  2849,  2849,  2849,  2849,  2849,
    -407,    86,    86,    89,    86,  -407,    92,   128,  -407,  -407,
     114,   116,  2645,  -407,   611,  -407,  -407,  -407,  -407,  -407,
     120,  -407,  -407,   126,    67,  -407,  -407,  -407,  -407,  -407,
    -407,   148,  -407,    62,  -407,  -407,   498,  -407,    51,  -407,
     109,   139,    78,  2509,  -407,  -407,  -407,  -407,  -407,  -407,
    -407,  -407,  -407,  -407,  -407,  -407,  -407,  2577,  -407,  2577,
    -407,  2577,  -407,  -407,    27,  -407,  -407,    43,  -407,   162,
     164,  -407,  1340,  1042,  -407,  -407,  -407,   166,   169,  -407,
    -407,  1935,  1935,  1411,  -407,   171,   172,  -407,  -407,   957,
    1935,  1935,  1935,  1935,  -407,     9,   173,  1042,  -407,  -407,
    -407,   346,  -407,  -407,    83,    59,    30,    29,    91,   160,
     147,   161,   267,   -23,  -407,   163,  -407,  -407,  -407,  -407,
    -407,  -407,  -407,  -407,  -407,  1042,  -407,  -407,   704,   797,
    -407,  -407,   183,  -407,  -407,  -407,   258,  -407,   260,  -407,
      41,  -407,   208,  -407,   188,   199,  2781,   238,  -407,  -407,
    -407,  -407,  -407,  -407,  -407,  -407,   186,  -407,  -407,   182,
    -407,  -407,  -407,  -407,  -407,  -407,  -407,  -407,  -407,  -407,
     190,  -407,    43,  -407,  1269,  -407,  -407,    11,  1482,  -407,
      43,    50,  2645,   108,  -407,   193,   197,  -407,  2237,  -407,
    -407,  2305,  2373,  2577,   196,    90,    90,   200,  -407,  -407,
    -407,   173,    35,   204,   228,   877,  2006,  2006,  -407,  -407,
    -407,   206,  2006,  2006,   201,  -407,   202,  -407,  -407,  -407,
    -407,   212,  2006,  -407,  2006,  2006,  2006,  2006,  2006,  2006,
    2006,  2006,  -407,  -407,  2006,  1562,  2006,    90,  2006,  2006,
    2006,  2006,  2006,  2006,  2006,  2006,  2006,  2006,  2006,  2006,
    2006,  2006,  2006,  2006,  2006,  2006,  2006,  -407,   276,  -407,
    -407,   704,  -407,  -407,  -407,    90,  -407,  -407,  -407,  1633,
      90,    32,  -407,  -407,   208,   213,  -407,    90,  -407,    73,
    -407,   140,  -407,  -407,  -407,  -407,   216,  1269,  -407,  -407,
    -407,   215,  -407,  -407,   209,   220,   218,  -407,  2645,  -407,
    -407,  2849,  -407,  -407,  -407,  -407,  2441,   247,   123,  -407,
     221,   130,   227,  -407,   225,    43,  -407,  -407,   231,   235,
    -407,   230,  -407,   232,   233,  2006,  -407,   142,  -407,  -407,
    -407,  -407,  -407,  -407,  -407,  -407,  -407,  -407,   234,   249,
    -407,   226,  -407,  -407,  -407,  -407,  -407,    83,    83,    59,
      59,    30,    30,    30,    30,    29,    29,    91,   160,   147,
     161,   267,   224,  1127,  -407,  -407,  -407,   173,  -407,   250,
    -407,    59,   240,  -407,  -407,  2713,    56,    90,  -407,  2781,
    -407,   304,   259,    58,  -407,   159,   199,  -407,  1269,  1713,
    -407,  -407,  -407,  -407,  -407,    90,   305,  -407,  -407,  2006,
     266,  2006,  1793,  -407,    86,  1127,  -407,  2006,  -407,  -407,
    2006,  -407,  2006,  -407,  -407,   262,  2006,    90,  -407,  -407,
    -407,   153,  -407,   154,  -407,   268,    44,   273,   270,  -407,
    1198,  -407,  -407,  -407,   264,  -407,  -407,   271,  -407,  -407,
     278,    49,  -407,  -407,  -407,  -407,  -407,  -407,  -407,    17,
    -407,   275,    90,  -407,   327,   274,    52,  -407,   281,  -407,
      52,  -407,  -407,  -407,   284,  1864,  2006,   283,  -407,    21,
      90,  -407,  1538,   282,   285,  -407,  2006,  -407,  -407,  -407,
    -407,  -407,  -407,   286,   287,  1042,  -407,  -407,  -407,   292,
      90,  -407,  -407,  -407,   288,    85,  1127,  1042,  -407,  -407,
      90,  -407,  -407,  -407,  -407,  -407,   293,   291,  -407,   295,
    -407
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,    60,    54,    61,     0,    64,    52,    72,    70,    74,
      50,     0,    71,    73,     0,    63,    84,    55,     0,    58,
     104,    62,    53,     0,     0,     0,    65,    66,    67,    68,
      69,   121,     0,     0,     0,     0,     0,     0,     0,     2,
       4,     0,    13,    16,    28,     0,     0,     0,     0,     0,
      57,    77,     0,     0,     0,    14,    95,     0,    59,    56,
     119,     0,     0,     8,     0,    11,    51,    22,   326,    83,
      79,    82,    15,     0,     0,    23,    24,   120,    25,    26,
      27,    91,    98,     0,     1,     3,     0,     9,     0,    35,
      37,   134,   136,     0,   142,    34,    29,    31,    30,    33,
      32,    17,    19,    18,    21,    20,   288,     0,    78,     0,
       5,     0,    90,    93,     0,   115,   117,     0,   257,     0,
       0,   329,     0,     0,   330,   331,   332,     0,     0,   327,
     328,     0,     0,     0,   333,     0,     0,   295,   256,     0,
       0,     0,     0,     0,   292,     0,    28,     0,   198,   195,
     202,   214,   221,   223,   227,   230,   233,   238,   241,   243,
     245,   247,   249,   251,   254,     0,   297,   293,   260,   261,
     270,   263,   268,   266,   271,     0,   267,   262,     0,     0,
     290,   264,     0,   265,   272,   269,     0,   197,   325,   199,
       0,   106,     0,   126,     0,     0,     0,     0,   130,   166,
     161,   160,   156,   159,   162,   167,     0,   157,   158,     0,
     149,   150,   151,   152,   153,   154,   155,   163,   164,   165,
       0,    10,     0,   258,     0,   135,   147,     0,     0,   180,
       0,   170,     0,     0,   173,     0,   179,    87,     0,    85,
      88,     0,     0,     0,    96,     0,     0,     0,    49,   276,
     274,     0,   214,     0,     0,     0,     0,     0,   216,   215,
     321,     0,     0,     0,     0,   170,     0,   217,   218,   219,
     220,    37,     0,   259,     0,     0,     0,     0,     0,     0,
       0,     0,   204,   203,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   278,   260,   289,
     287,     0,   255,   291,   294,     0,    81,    80,   324,     0,
       0,     0,    39,   128,     0,     0,    92,     0,   103,     0,
     100,     0,   168,   169,   148,    36,     0,     0,    38,   182,
     138,   137,   144,   334,     0,   176,    12,   175,     0,   145,
     146,     0,    75,    86,    76,    89,     0,     0,     0,   122,
     124,     0,     0,   275,     0,     0,   314,   317,   312,     0,
     311,     0,   320,     0,     0,     0,   200,     0,   212,   298,
     299,   300,   301,   302,   303,   304,   305,   296,     0,   209,
     210,     0,   205,   322,   224,   225,   226,   228,   229,   232,
     231,   237,   236,   234,   235,   239,   240,   242,   244,   246,
     248,   250,     0,     0,   286,   196,   325,   113,   107,   110,
     111,   114,     0,     6,    40,     0,     0,     0,   102,     0,
      99,     0,     0,     0,   186,     0,     0,   143,     0,     0,
     174,   181,    94,    97,   116,     0,     0,   118,   273,     0,
     316,     0,     0,   280,     0,     0,   222,     0,   201,   207,
       0,   206,     0,   277,   279,     0,     0,     0,   127,   129,
       7,     0,   101,     0,   131,     0,     0,     0,     0,   183,
       0,   139,   177,   172,     0,   123,   125,     0,   315,   318,
       0,     0,   306,   309,   213,   211,   252,   108,   112,     0,
      42,     0,     0,   140,     0,     0,     0,   185,     0,   188,
       0,   184,   187,   171,     0,     0,     0,     0,   284,     0,
      44,    41,     0,     0,     0,   132,     0,   191,   192,   193,
     189,   190,   307,     0,     0,     0,   285,   281,    43,     0,
       0,    45,    46,   141,     0,     0,     0,     0,   283,    49,
       0,   133,   194,   308,   310,   282,     0,     0,    48,     0,
      47
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -407,  -407,   349,     2,   -88,   -46,    25,  -407,    31,   176,
      69,  -289,  -407,  -125,  -407,  -149,   194,   358,   359,   360,
     361,   362,  -407,   354,  -407,     7,  -407,   -89,  -207,  -407,
    -407,  -407,  -407,  -407,  -407,  -407,  -407,  -407,   -22,  -407,
    -407,  -407,  -407,  -407,  -407,   -58,  -407,  -407,  -407,  -407,
    -407,   167,   -36,   319,  -407,  -407,   -56,   -17,  -407,  -216,
    -407,  -407,  -407,  -407,  -407,  -206,  -407,  -407,  -334,  -407,
    -407,   -62,  -405,  -407,   100,  -407,   -63,  -407,  -407,  -407,
      28,  -236,  -139,  -264,  -100,   -40,   117,   118,   119,   115,
     125,  -407,   -41,  -407,    60,    48,   363,  -119,  -169,  -390,
    -407,  -407,  -407,  -407,  -407,  -407,  -407,   -96,  -407,  -407,
     -42,  -294,   246,  -166,  -407,  -241,  -407,  -407,  -218,   -84,
    -407,  -407,  -407,  -407,  -407,  -407,   207,  -407,   -16,  -355,
    -406
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    38,    39,   237,   328,    41,    42,    43,    88,    89,
     321,   322,   499,   500,   541,   362,   251,    45,    46,    47,
      48,    49,    50,   107,    51,    70,    52,   238,   239,    53,
     111,    54,    55,   243,    56,    57,   196,   329,   330,    58,
      73,   319,   465,   418,   419,   420,    59,   245,   246,    60,
      61,   358,   359,   194,   323,   425,    62,   271,    91,   326,
      92,    93,   220,   346,   233,   234,   235,   236,   338,   435,
     476,   477,   527,   148,   149,   150,   252,   388,   389,   377,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   240,    64,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   518,   519,   177,
     178,   310,   179,   180,   181,   182,   183,   184,   371,   367,
     368,   369,   490,   185,   392,   186,   317,   187,   188,   189,
     344
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      71,   151,    40,   434,   254,   229,   308,    74,   147,   108,
     108,   340,   112,   313,   366,    71,   117,   414,   145,    71,
     241,    83,   242,   464,    90,    94,   347,   305,   273,   399,
     400,   353,   424,   484,   353,   353,   516,   232,    77,    67,
      40,   517,    81,    72,    68,   374,    68,   230,    63,    75,
      76,   264,   394,   395,   396,   421,   307,    78,    79,    80,
     151,   295,    68,   293,   516,   493,   144,   147,   121,   517,
     101,   102,   103,   104,   105,   296,    68,   294,    68,    68,
     124,   125,   126,   282,   151,   -13,    63,    68,   283,    69,
    -253,   147,   325,    68,    65,   129,   130,    86,    71,    87,
     247,    94,   320,   316,   482,   531,   475,   520,   134,   521,
     534,    66,   151,   309,   299,   151,   151,   542,   231,   147,
     475,   244,   147,   147,   423,    68,   320,   297,   298,    94,
     285,    86,   145,   145,  -105,   286,   507,   424,   -13,   456,
     300,   221,   440,    82,   222,   313,   512,   287,   470,   353,
     526,   529,   397,   398,   356,   529,   554,   197,   192,   258,
     259,   291,   292,   198,   265,   193,   429,   529,   267,   268,
     269,   270,   430,   226,   318,   227,   232,   106,   228,   110,
     144,   144,   253,   113,   552,   114,   230,   291,   292,   288,
     289,   290,   151,   261,    44,   401,   402,   403,   404,   266,
     223,   348,   421,   224,   349,   115,    94,   116,    44,   365,
     488,   318,    44,   345,    94,   444,   445,   190,    44,    44,
     481,   265,   447,   445,   191,   537,    44,    44,    44,   360,
     360,   487,    44,   431,   489,   457,   432,   193,   458,    44,
      44,    44,    44,    44,   463,   195,   502,   504,   151,   503,
     505,   479,   480,   248,   249,   147,    44,   302,   146,   405,
     406,   255,   545,   441,   256,   145,   262,   263,   272,   301,
     304,   393,   303,   314,   366,   315,   306,  -323,   320,   324,
     325,   331,   332,   333,   339,   334,   492,    44,   343,   350,
     351,   223,   232,   357,   363,   364,   372,   375,   376,   416,
     413,    44,   230,    44,   422,    44,   224,   433,   427,   443,
     437,   428,   436,   144,   438,   446,   370,   266,   439,   448,
     449,   462,   373,   370,   451,   452,   453,   461,   454,   455,
     459,   467,   378,   146,   379,   380,   381,   382,   383,   384,
     385,   386,   460,   466,   387,   390,   391,   473,   486,    94,
     151,   274,   275,   276,   277,   278,   279,   280,   281,   222,
     474,   497,   506,   509,   510,   513,   412,   514,   515,   522,
     524,   530,   146,   146,   532,   525,   265,   553,   543,   145,
     535,   544,   546,   549,   547,   558,   559,    85,   151,   551,
      44,   560,   151,   426,   282,   538,   450,   339,   335,   283,
     556,    96,    97,    98,    99,   100,   109,   472,   498,   485,
     225,   471,   491,   361,   508,   415,   548,   478,   407,   410,
     408,   496,   409,   536,   311,   118,    44,   469,   555,   360,
     411,   533,    44,     0,   341,    44,    44,    44,     0,     0,
     284,   285,     0,     0,     0,     0,   286,     0,     0,   146,
       0,   501,   151,     0,   265,     0,     0,     0,   287,     0,
     478,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   151,     0,     0,     0,     0,     0,     0,   147,
       0,     0,     0,   151,   151,     0,   523,     0,     0,     0,
     528,   147,     0,     0,   528,     0,     0,     0,   339,   343,
       0,   199,     0,     0,   501,   146,   528,     0,     0,   370,
       0,     0,   370,   417,     0,     0,     0,   494,     0,     0,
     495,   200,     0,     0,   550,     0,     0,     0,     0,     0,
     201,   202,     0,     0,   557,     0,     0,     0,     0,     0,
     339,     0,    44,     0,   203,    44,     0,   204,   205,     0,
      44,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   343,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   206,     0,     0,   207,   208,   209,     0,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
       0,     0,     1,     0,     0,   119,     0,     0,     0,    44,
       0,     0,     0,    44,     2,   120,     0,   121,     0,     3,
       0,     0,   122,   123,     0,     0,     4,     5,     6,   124,
     125,   126,   127,     0,     0,     0,    68,     0,   128,     7,
       8,     9,    10,    11,   129,   130,    12,     0,     0,   131,
     417,     0,    13,    14,   132,   133,    15,   134,    16,   135,
      17,     0,    18,    19,    20,    21,     0,    22,   136,     0,
       0,     0,     0,     0,     0,    24,    25,    26,    27,    28,
      29,    30,     0,    31,    32,    33,    34,     0,    36,     0,
       0,   137,   106,   138,     0,     1,   139,     0,   119,     0,
       0,    37,     0,   140,   141,   142,   143,     2,   120,     0,
     121,     0,     3,     0,     0,   122,   123,     0,     0,     4,
       5,     6,   124,   125,   126,   127,     0,     0,     0,    68,
       0,   128,     7,     8,     9,    10,    11,   129,   130,    12,
       0,     0,   131,     0,     0,    13,    14,   132,   133,    15,
     134,    16,   135,    17,     0,    18,    19,    20,    21,     0,
      22,   136,     0,     0,     0,     0,     0,     0,    24,    25,
      26,    27,    28,    29,    30,     0,    31,    32,    33,    34,
       0,    36,     0,     0,   137,   106,   309,     0,     1,   139,
       0,   119,     0,     0,    37,     0,   140,   141,   142,   143,
       2,   120,     0,   121,     0,     3,     0,     0,   122,   123,
       0,     0,     4,     5,     6,   124,   125,   126,   127,     0,
       0,     0,    68,     0,   128,     7,     8,     9,    10,    11,
     129,   130,    12,     0,     0,   131,     0,     0,    13,    14,
     132,   133,    15,   134,    16,   135,    17,     0,    18,    19,
      20,    21,     0,    22,   136,     0,     0,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     1,    31,
      32,    33,    34,     0,    36,     0,     0,   137,   106,   312,
       2,     0,   139,   121,     0,     3,     0,    37,     0,   140,
     141,   142,   143,     5,     6,   124,   125,   126,     0,     0,
       0,     0,    68,     0,     0,     7,     8,     9,    10,    11,
     129,   130,    12,     0,     0,   131,     0,     0,    13,    14,
     132,     0,    15,   134,    16,     0,    17,     0,    18,    19,
      20,    21,     0,    22,     0,     0,     0,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     1,    31,
      32,    33,    34,     0,    36,     0,     0,  -313,     0,     0,
       2,     0,   139,   121,     0,     3,     0,     0,     0,   140,
     141,   142,   143,     5,     6,   124,   125,   126,     0,     0,
       0,     0,    68,     0,     0,     7,     8,     9,    10,    11,
     129,   130,    12,     0,     0,   131,     0,     0,    13,    14,
     132,     0,    15,   134,    16,     0,    17,     0,     0,    19,
      20,    21,     0,    22,     0,     0,     0,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     0,    31,
      32,    33,    34,     1,     0,     0,   119,     0,     0,     0,
       0,     0,   139,     0,     0,     2,   120,     0,   121,   140,
     141,   142,   143,   122,   123,     0,     0,     0,     0,     6,
     124,   125,   126,   127,     0,     0,     0,    68,     0,   128,
       0,     0,     0,    10,    11,   129,   130,     0,     0,     0,
     131,     0,     0,     0,     0,   132,   133,     0,   134,    16,
     135,    17,     0,     0,    19,    20,     0,     0,    22,   136,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    31,     0,     0,     0,     1,     0,
       0,   119,   137,   106,     0,     0,     0,   139,     0,     0,
       2,   120,    37,   121,   140,   141,   142,   143,   122,   123,
       0,     0,     0,     0,     6,   124,   125,   126,   127,     0,
       0,     0,    68,     0,   128,     0,     0,     0,    10,    11,
     129,   130,     0,     0,     0,   131,     0,     0,     0,     0,
     132,   133,     0,   134,    16,   135,    17,     0,     0,    19,
      20,     0,     0,    22,   136,     0,     0,     0,     0,     1,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    31,
       0,     2,     0,     0,   121,     0,     0,   137,   106,     0,
       0,     0,   139,     0,     0,     6,   124,   125,   126,   140,
     141,   142,   143,    68,     0,     0,     0,     0,     0,    10,
      11,   129,   130,     0,     0,     0,   131,     0,     0,     0,
       0,   132,     0,     0,   134,    16,     0,    17,     0,     0,
      19,    20,     0,     0,    22,     0,   336,     0,     0,     0,
       1,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      31,     0,     2,     0,     0,   121,     0,     0,     0,   337,
     511,     0,     0,   139,     0,     0,     6,   124,   125,   126,
     140,   141,   142,   143,    68,     0,     0,     0,     0,     0,
      10,    11,   129,   130,     0,     0,     0,   131,     0,     0,
       0,     0,   132,     0,     0,   134,    16,     0,    17,     0,
       0,    19,    20,     0,     0,    22,     0,   336,     0,     0,
       0,     1,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    31,     0,     2,     0,     0,   121,     0,     0,     0,
     337,     0,     0,     0,   139,     0,     0,     6,   124,   125,
     126,   140,   141,   142,   143,    68,     0,     0,     0,     0,
       0,    10,    11,   129,   130,     0,     0,     0,   131,     0,
       0,     0,     0,   132,     0,     0,   134,    16,     0,    17,
       0,     0,    19,    20,     0,     0,    22,     0,     0,     0,
       0,     0,     1,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    31,     0,     2,     0,     0,   121,     0,     0,
     250,     0,     0,     0,     0,   139,     0,     0,     6,   124,
     125,   126,   140,   141,   142,   143,    68,     0,     0,     0,
       0,     0,    10,    11,   129,   130,     0,     0,     0,   131,
       0,     0,     0,     0,   132,     0,     0,   134,    16,     0,
      17,     0,     0,    19,    20,     0,     0,    22,     0,     0,
       0,     0,     0,     1,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    31,     0,     2,     0,     0,   121,     0,
       0,   260,     0,     0,     0,     0,   139,     0,     0,     6,
     124,   125,   126,   140,   141,   142,   143,    68,     0,     0,
       0,     0,     0,    10,    11,   129,   130,     0,     0,     0,
     131,     0,     0,     0,     0,   132,     0,     0,   134,    16,
       0,    17,   539,     0,    19,    20,     0,     0,    22,     0,
       0,     0,     0,     0,   121,     0,     0,     0,     0,     0,
       0,     0,     0,     1,    31,     0,   124,   125,   126,     0,
       0,     0,     0,    68,     0,     2,     0,   139,   121,     0,
       0,   129,   130,   342,   140,   141,   142,   143,     0,     6,
     124,   125,   126,     0,   134,     0,     0,    68,     0,     0,
       0,     0,     0,    10,    11,   129,   130,     0,     0,     0,
     131,   540,     0,     0,     0,   132,     0,     0,   134,    16,
       0,    17,     0,     0,    19,    20,     0,     0,    22,     0,
       0,     0,     0,     0,     1,     0,   526,     0,     0,     0,
       0,     0,     0,     0,    31,     0,     2,     0,     0,   121,
       0,     0,     0,     0,     0,     0,     0,   139,  -208,     0,
       6,   124,   125,   126,   140,   141,   142,   143,    68,     0,
       0,     0,     0,     0,    10,    11,   129,   130,     0,     0,
       0,   131,     0,     0,     0,     0,   132,     0,     0,   134,
      16,     0,    17,     0,     0,    19,    20,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     1,    31,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     3,     2,     0,   139,   121,
       0,     0,  -109,     5,     0,   140,   141,   142,   143,     0,
       6,   124,   125,   126,     0,     7,     8,     9,    68,     0,
       0,     0,    12,     0,    10,    11,   129,   130,    13,    95,
       0,   131,    15,     0,     0,     0,   132,     0,     0,   134,
      16,    21,    17,     0,     0,    19,    20,     0,     0,    22,
       0,     0,     0,    26,    27,    28,    29,    30,     0,     0,
       0,     0,     0,     0,     1,    31,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     2,     0,   139,   121,
       0,     0,     0,     0,   483,   140,   141,   142,   143,     0,
       6,   124,   125,   126,     0,     0,     0,     0,    68,     0,
       0,     0,     0,     0,    10,    11,   129,   130,     0,     0,
       0,   131,     0,     0,     0,     0,   132,     0,     0,   134,
      16,     0,    17,     0,     0,    19,    20,     0,     0,    22,
       0,     0,     0,     0,     0,     1,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    31,     0,     2,     0,     0,
     121,     0,     0,  -319,     0,     0,     0,     0,   139,     0,
       0,     6,   124,   125,   126,   140,   141,   142,   143,    68,
       0,     0,     0,     0,     0,    10,    11,   129,   130,     0,
       0,     0,   131,     0,     0,     0,     0,   132,     0,     0,
     134,    16,     0,    17,     0,     0,    19,    20,     0,     0,
      22,     0,     0,     0,     0,     0,     1,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    31,     0,     2,     0,
       0,   121,     0,     0,     0,     0,     0,     0,     0,   139,
    -313,     0,     6,   124,   125,   126,   140,   141,   142,   143,
      68,     0,     0,     0,     0,     0,    10,    11,   129,   130,
       0,     0,     0,   131,     0,     0,     0,     0,   132,     0,
       0,   134,    16,     0,    17,     0,     0,    19,    20,     0,
       0,    22,     0,     0,     0,     0,     0,     1,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    31,     0,     2,
       0,     0,   121,     0,     0,     0,     0,     0,     0,     0,
     257,     0,     0,     6,   124,   125,   126,   140,   141,   142,
     143,    68,     0,     0,     0,     0,     0,    10,    11,   129,
     130,     0,     0,     0,   131,     0,     0,     0,     0,   132,
       0,     0,   134,    16,     0,    17,     0,     0,    19,    20,
       0,     0,    22,     0,     0,     0,     0,    84,     1,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    31,     0,
       2,     0,     0,     0,     0,     3,     0,     0,     0,     0,
       0,   139,     4,     5,     6,     0,     0,     0,   140,   141,
     142,   143,     0,     0,     0,     7,     8,     9,    10,    11,
       0,     0,    12,     0,     0,     0,     0,     0,    13,    14,
       0,     0,    15,     0,    16,     0,    17,     0,    18,    19,
      20,    21,     0,    22,     0,     0,    23,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     1,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,     0,
       2,     0,     0,     0,     0,     3,     0,    37,     0,     0,
       0,     0,     4,     5,     6,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     7,     8,     9,    10,    11,
       0,     0,    12,     0,     0,     0,     0,     0,    13,    14,
       0,     0,    15,     0,    16,     0,    17,     0,    18,    19,
      20,    21,     0,    22,     0,     0,    23,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     1,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,     0,
       2,     0,     0,     0,     0,     3,     0,    37,     0,     0,
       0,     0,     4,     5,     6,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     7,     8,     9,    10,    11,
       0,     0,    12,     0,     0,     0,     0,     0,    13,    14,
       0,     0,    15,     0,    16,     0,    17,     0,    18,    19,
      20,    21,     0,    22,     0,     0,     1,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     2,    31,
      32,    33,    34,     3,    36,     0,     0,     0,     0,   352,
       4,     5,     6,     0,     0,     0,     0,    37,     0,     0,
       0,     0,     0,     7,     8,     9,    10,    11,     0,     0,
      12,     0,     0,     0,     0,     0,    13,    14,     0,     0,
      15,     0,    16,     0,    17,     0,    18,    19,    20,    21,
       0,    22,     0,     0,     1,     0,     0,     0,     0,    24,
      25,    26,    27,    28,    29,    30,     2,    31,    32,    33,
      34,     3,    36,     0,     0,     0,     0,   354,     4,     5,
       6,     0,     0,     0,     0,    37,     0,     0,     0,     0,
       0,     7,     8,     9,    10,    11,     0,     0,    12,     0,
       0,     0,     0,     0,    13,    14,     0,     0,    15,     0,
      16,     0,    17,     0,    18,    19,    20,    21,     0,    22,
       0,     0,     1,     0,     0,     0,     0,    24,    25,    26,
      27,    28,    29,    30,     2,    31,    32,    33,    34,     3,
      36,     0,     0,     0,     0,   355,     4,     5,     6,     0,
       0,     0,     0,    37,     0,     0,     0,     0,     0,     7,
       8,     9,    10,    11,     0,     0,    12,     0,     0,     0,
       0,     0,    13,    14,     0,     0,    15,     0,    16,     0,
      17,     0,    18,    19,    20,    21,     0,    22,     0,     0,
       1,     0,     0,     0,     0,    24,    25,    26,    27,    28,
      29,    30,     2,    31,    32,    33,    34,     3,    36,     0,
       0,     0,     0,   442,     0,     5,     6,     0,     0,     0,
       0,    37,     0,     0,     0,     0,     0,     7,     8,     9,
      10,    11,     0,     0,    12,     0,     0,     0,     0,     0,
      13,    14,     0,     0,    15,     0,    16,     0,    17,     0,
      18,    19,    20,    21,     0,    22,     0,     0,     1,     0,
       0,     0,     0,    24,    25,    26,    27,    28,    29,    30,
       2,    31,    32,    33,    34,     3,    36,     0,     0,     0,
       0,     0,     4,     5,     6,  -178,     0,     0,     0,    37,
       0,     0,     0,     0,     0,     7,     8,     9,    10,    11,
       0,     0,    12,     0,     0,     0,     0,     0,    13,    14,
       0,     0,    15,     0,    16,     0,    17,     0,    18,    19,
      20,    21,     0,    22,     0,     0,     1,     0,     0,     0,
       0,    24,    25,    26,    27,    28,    29,    30,     2,    31,
      32,    33,    34,     3,    36,     0,     0,     0,     0,     0,
       0,     5,     6,     0,     0,     0,     0,    37,     0,     0,
       0,     0,     0,     7,     8,     9,    10,    11,     0,     0,
      12,     0,     0,     0,     0,     0,    13,    14,     0,     0,
      15,     0,    16,     0,    17,     0,    18,    19,    20,    21,
       0,    22,     0,     0,     1,     0,     0,     0,     0,    24,
      25,    26,    27,    28,    29,    30,     2,    31,    32,    33,
      34,     3,    36,     0,     0,     0,     0,     0,     4,     5,
       6,     0,     0,     0,     0,    37,     0,     0,     0,     0,
       0,     7,     8,     9,    10,    11,     0,     0,    12,     0,
       0,     0,     0,     0,    13,    14,     0,     0,    15,     0,
      16,     0,    17,     0,    18,    19,    20,    21,     0,    22,
       0,     0,     1,     0,     0,     0,     0,    24,    25,    26,
      27,    28,    29,    30,     2,    31,    32,    33,    34,     3,
      36,     0,     0,     0,     0,     0,     0,     5,     6,     0,
       0,     0,   468,     0,     0,     0,     0,     0,     0,     7,
       8,     9,    10,    11,     0,     0,    12,     0,     0,     0,
       0,     0,    13,    14,     0,     0,    15,     0,    16,     0,
      17,     0,   327,    19,    20,    21,     0,    22,     0,     0,
       1,     0,     0,     0,     0,    24,    25,    26,    27,    28,
      29,    30,     2,    31,    32,    33,    34,     3,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     7,     8,     9,
      10,    11,     0,     0,    12,     0,     0,     0,     0,     0,
      13,    14,     0,     0,    15,     0,    16,     0,    17,     0,
       0,    19,    20,    21,     0,    22,     0,     0,     0,     0,
       0,     0,     0,    24,    25,    26,    27,    28,    29,    30,
       0,    31,    32,    33,    34
};

static const yytype_int16 yycheck[] =
{
      16,    64,     0,   337,   123,    93,   175,    23,    64,    51,
      52,   227,    54,   179,   255,    31,    62,   311,    64,    35,
     109,    37,   111,   413,    41,    41,   232,    50,   147,   293,
     294,   238,   321,   439,   241,   242,    15,    93,    31,    14,
      38,    20,    35,    18,    35,   263,    35,    93,     0,    24,
      25,   139,   288,   289,   290,   319,   175,    32,    33,    34,
     123,    32,    35,    33,    15,   455,    64,   123,    16,    20,
      45,    46,    47,    48,    49,    46,    35,    47,    35,    35,
      28,    29,    30,    48,   147,    35,    38,    35,    53,    62,
     113,   147,    81,    35,    90,    43,    44,    88,   114,    90,
     117,   117,    70,    62,   438,   510,    62,    90,    56,    92,
     516,    41,   175,    92,    23,   178,   179,   522,    93,   175,
      62,   114,   178,   179,    92,    35,    70,    98,    99,   145,
      95,    88,   178,   179,    98,   100,    92,   426,    88,   375,
      49,    90,   348,    98,    93,   311,   480,   112,    92,   356,
      98,   506,   291,   292,   243,   510,   546,    95,    91,   131,
     132,   102,   103,   101,   139,    98,    93,   522,   140,   141,
     142,   143,    99,    95,   190,    97,   232,    91,   100,    90,
     178,   179,   122,    91,    99,    57,   232,   102,   103,   106,
     107,   108,   255,   133,     0,   295,   296,   297,   298,   139,
      91,    93,   466,    94,    96,    91,   222,    91,    14,   255,
     451,   227,    18,   230,   230,    92,    93,    97,    24,    25,
     436,   196,    92,    93,    98,   519,    32,    33,    34,   245,
     246,   449,    38,    93,   452,    93,    96,    98,    96,    45,
      46,    47,    48,    49,   413,    97,    93,    93,   311,    96,
      96,    92,    93,    91,    90,   311,    62,   110,    64,   299,
     300,    95,   526,   351,    95,   311,    95,    95,    95,   109,
       3,   287,   111,    90,   515,    17,   113,    17,    70,    91,
      81,    43,    96,   101,   224,    95,   455,    93,   228,    96,
      93,    91,   348,    97,    90,    67,    90,    96,    96,   315,
      24,   107,   348,   109,   320,   111,    94,    91,    95,    62,
     101,   327,    97,   311,    94,    94,   256,   257,   100,    92,
      95,    97,   262,   263,    93,    90,    96,   101,    96,    96,
      96,    91,   272,   139,   274,   275,   276,   277,   278,   279,
     280,   281,    93,    93,   284,   285,   286,    43,    43,   365,
     413,     5,     6,     7,     8,     9,    10,    11,    12,    93,
     101,    99,    94,    90,    94,   101,   306,    96,    90,    94,
      43,    90,   178,   179,    90,   101,   351,   546,    96,   425,
      97,    96,    96,    91,    97,    92,    95,    38,   451,   101,
     196,    96,   455,   324,    48,   520,   365,   337,   222,    53,
     549,    43,    43,    43,    43,    43,    52,   429,   466,   445,
      91,   427,   454,   246,   476,   315,   535,   433,   301,   304,
     302,   462,   303,   519,   178,    62,   232,   425,   547,   445,
     305,   515,   238,    -1,   227,   241,   242,   243,    -1,    -1,
      94,    95,    -1,    -1,    -1,    -1,   100,    -1,    -1,   255,
      -1,   467,   515,    -1,   429,    -1,    -1,    -1,   112,    -1,
     476,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   535,    -1,    -1,    -1,    -1,    -1,    -1,   535,
      -1,    -1,    -1,   546,   547,    -1,   502,    -1,    -1,    -1,
     506,   547,    -1,    -1,   510,    -1,    -1,    -1,   438,   439,
      -1,     3,    -1,    -1,   520,   311,   522,    -1,    -1,   449,
      -1,    -1,   452,   319,    -1,    -1,    -1,   457,    -1,    -1,
     460,    23,    -1,    -1,   540,    -1,    -1,    -1,    -1,    -1,
      32,    33,    -1,    -1,   550,    -1,    -1,    -1,    -1,    -1,
     480,    -1,   348,    -1,    46,   351,    -1,    49,    50,    -1,
     356,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   516,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    -1,    98,    99,   100,    -1,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
      -1,    -1,     1,    -1,    -1,     4,    -1,    -1,    -1,   425,
      -1,    -1,    -1,   429,    13,    14,    -1,    16,    -1,    18,
      -1,    -1,    21,    22,    -1,    -1,    25,    26,    27,    28,
      29,    30,    31,    -1,    -1,    -1,    35,    -1,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    -1,    -1,    48,
     466,    -1,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    -1,    61,    62,    63,    64,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    -1,    82,    83,    84,    85,    -1,    87,    -1,
      -1,    90,    91,    92,    -1,     1,    95,    -1,     4,    -1,
      -1,   100,    -1,   102,   103,   104,   105,    13,    14,    -1,
      16,    -1,    18,    -1,    -1,    21,    22,    -1,    -1,    25,
      26,    27,    28,    29,    30,    31,    -1,    -1,    -1,    35,
      -1,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      -1,    -1,    48,    -1,    -1,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    -1,    61,    62,    63,    64,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,
      76,    77,    78,    79,    80,    -1,    82,    83,    84,    85,
      -1,    87,    -1,    -1,    90,    91,    92,    -1,     1,    95,
      -1,     4,    -1,    -1,   100,    -1,   102,   103,   104,   105,
      13,    14,    -1,    16,    -1,    18,    -1,    -1,    21,    22,
      -1,    -1,    25,    26,    27,    28,    29,    30,    31,    -1,
      -1,    -1,    35,    -1,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    -1,    -1,    48,    -1,    -1,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    -1,    61,    62,
      63,    64,    -1,    66,    67,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,     1,    82,
      83,    84,    85,    -1,    87,    -1,    -1,    90,    91,    92,
      13,    -1,    95,    16,    -1,    18,    -1,   100,    -1,   102,
     103,   104,   105,    26,    27,    28,    29,    30,    -1,    -1,
      -1,    -1,    35,    -1,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    -1,    -1,    48,    -1,    -1,    51,    52,
      53,    -1,    55,    56,    57,    -1,    59,    -1,    61,    62,
      63,    64,    -1,    66,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,     1,    82,
      83,    84,    85,    -1,    87,    -1,    -1,    90,    -1,    -1,
      13,    -1,    95,    16,    -1,    18,    -1,    -1,    -1,   102,
     103,   104,   105,    26,    27,    28,    29,    30,    -1,    -1,
      -1,    -1,    35,    -1,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    -1,    -1,    48,    -1,    -1,    51,    52,
      53,    -1,    55,    56,    57,    -1,    59,    -1,    -1,    62,
      63,    64,    -1,    66,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    -1,    82,
      83,    84,    85,     1,    -1,    -1,     4,    -1,    -1,    -1,
      -1,    -1,    95,    -1,    -1,    13,    14,    -1,    16,   102,
     103,   104,   105,    21,    22,    -1,    -1,    -1,    -1,    27,
      28,    29,    30,    31,    -1,    -1,    -1,    35,    -1,    37,
      -1,    -1,    -1,    41,    42,    43,    44,    -1,    -1,    -1,
      48,    -1,    -1,    -1,    -1,    53,    54,    -1,    56,    57,
      58,    59,    -1,    -1,    62,    63,    -1,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    -1,     1,    -1,
      -1,     4,    90,    91,    -1,    -1,    -1,    95,    -1,    -1,
      13,    14,   100,    16,   102,   103,   104,   105,    21,    22,
      -1,    -1,    -1,    -1,    27,    28,    29,    30,    31,    -1,
      -1,    -1,    35,    -1,    37,    -1,    -1,    -1,    41,    42,
      43,    44,    -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,
      53,    54,    -1,    56,    57,    58,    59,    -1,    -1,    62,
      63,    -1,    -1,    66,    67,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    13,    -1,    -1,    16,    -1,    -1,    90,    91,    -1,
      -1,    -1,    95,    -1,    -1,    27,    28,    29,    30,   102,
     103,   104,   105,    35,    -1,    -1,    -1,    -1,    -1,    41,
      42,    43,    44,    -1,    -1,    -1,    48,    -1,    -1,    -1,
      -1,    53,    -1,    -1,    56,    57,    -1,    59,    -1,    -1,
      62,    63,    -1,    -1,    66,    -1,    68,    -1,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,    91,
      92,    -1,    -1,    95,    -1,    -1,    27,    28,    29,    30,
     102,   103,   104,   105,    35,    -1,    -1,    -1,    -1,    -1,
      41,    42,    43,    44,    -1,    -1,    -1,    48,    -1,    -1,
      -1,    -1,    53,    -1,    -1,    56,    57,    -1,    59,    -1,
      -1,    62,    63,    -1,    -1,    66,    -1,    68,    -1,    -1,
      -1,     1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      91,    -1,    -1,    -1,    95,    -1,    -1,    27,    28,    29,
      30,   102,   103,   104,   105,    35,    -1,    -1,    -1,    -1,
      -1,    41,    42,    43,    44,    -1,    -1,    -1,    48,    -1,
      -1,    -1,    -1,    53,    -1,    -1,    56,    57,    -1,    59,
      -1,    -1,    62,    63,    -1,    -1,    66,    -1,    -1,    -1,
      -1,    -1,     1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    13,    -1,    -1,    16,    -1,    -1,
      90,    -1,    -1,    -1,    -1,    95,    -1,    -1,    27,    28,
      29,    30,   102,   103,   104,   105,    35,    -1,    -1,    -1,
      -1,    -1,    41,    42,    43,    44,    -1,    -1,    -1,    48,
      -1,    -1,    -1,    -1,    53,    -1,    -1,    56,    57,    -1,
      59,    -1,    -1,    62,    63,    -1,    -1,    66,    -1,    -1,
      -1,    -1,    -1,     1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    13,    -1,    -1,    16,    -1,
      -1,    90,    -1,    -1,    -1,    -1,    95,    -1,    -1,    27,
      28,    29,    30,   102,   103,   104,   105,    35,    -1,    -1,
      -1,    -1,    -1,    41,    42,    43,    44,    -1,    -1,    -1,
      48,    -1,    -1,    -1,    -1,    53,    -1,    -1,    56,    57,
      -1,    59,     4,    -1,    62,    63,    -1,    -1,    66,    -1,
      -1,    -1,    -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     1,    82,    -1,    28,    29,    30,    -1,
      -1,    -1,    -1,    35,    -1,    13,    -1,    95,    16,    -1,
      -1,    43,    44,   101,   102,   103,   104,   105,    -1,    27,
      28,    29,    30,    -1,    56,    -1,    -1,    35,    -1,    -1,
      -1,    -1,    -1,    41,    42,    43,    44,    -1,    -1,    -1,
      48,    73,    -1,    -1,    -1,    53,    -1,    -1,    56,    57,
      -1,    59,    -1,    -1,    62,    63,    -1,    -1,    66,    -1,
      -1,    -1,    -1,    -1,     1,    -1,    98,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    13,    -1,    -1,    16,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    -1,
      27,    28,    29,    30,   102,   103,   104,   105,    35,    -1,
      -1,    -1,    -1,    -1,    41,    42,    43,    44,    -1,    -1,
      -1,    48,    -1,    -1,    -1,    -1,    53,    -1,    -1,    56,
      57,    -1,    59,    -1,    -1,    62,    63,    -1,    -1,    66,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    82,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    18,    13,    -1,    95,    16,
      -1,    -1,    99,    26,    -1,   102,   103,   104,   105,    -1,
      27,    28,    29,    30,    -1,    38,    39,    40,    35,    -1,
      -1,    -1,    45,    -1,    41,    42,    43,    44,    51,    52,
      -1,    48,    55,    -1,    -1,    -1,    53,    -1,    -1,    56,
      57,    64,    59,    -1,    -1,    62,    63,    -1,    -1,    66,
      -1,    -1,    -1,    76,    77,    78,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    82,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    13,    -1,    95,    16,
      -1,    -1,    -1,    -1,   101,   102,   103,   104,   105,    -1,
      27,    28,    29,    30,    -1,    -1,    -1,    -1,    35,    -1,
      -1,    -1,    -1,    -1,    41,    42,    43,    44,    -1,    -1,
      -1,    48,    -1,    -1,    -1,    -1,    53,    -1,    -1,    56,
      57,    -1,    59,    -1,    -1,    62,    63,    -1,    -1,    66,
      -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    13,    -1,    -1,
      16,    -1,    -1,    90,    -1,    -1,    -1,    -1,    95,    -1,
      -1,    27,    28,    29,    30,   102,   103,   104,   105,    35,
      -1,    -1,    -1,    -1,    -1,    41,    42,    43,    44,    -1,
      -1,    -1,    48,    -1,    -1,    -1,    -1,    53,    -1,    -1,
      56,    57,    -1,    59,    -1,    -1,    62,    63,    -1,    -1,
      66,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    13,    -1,
      -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,
      96,    -1,    27,    28,    29,    30,   102,   103,   104,   105,
      35,    -1,    -1,    -1,    -1,    -1,    41,    42,    43,    44,
      -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,    53,    -1,
      -1,    56,    57,    -1,    59,    -1,    -1,    62,    63,    -1,
      -1,    66,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    13,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    -1,    -1,    27,    28,    29,    30,   102,   103,   104,
     105,    35,    -1,    -1,    -1,    -1,    -1,    41,    42,    43,
      44,    -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,    53,
      -1,    -1,    56,    57,    -1,    59,    -1,    -1,    62,    63,
      -1,    -1,    66,    -1,    -1,    -1,    -1,     0,     1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      13,    -1,    -1,    -1,    -1,    18,    -1,    -1,    -1,    -1,
      -1,    95,    25,    26,    27,    -1,    -1,    -1,   102,   103,
     104,   105,    -1,    -1,    -1,    38,    39,    40,    41,    42,
      -1,    -1,    45,    -1,    -1,    -1,    -1,    -1,    51,    52,
      -1,    -1,    55,    -1,    57,    -1,    59,    -1,    61,    62,
      63,    64,    -1,    66,    -1,    -1,    69,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,     1,    82,
      83,    84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      13,    -1,    -1,    -1,    -1,    18,    -1,   100,    -1,    -1,
      -1,    -1,    25,    26,    27,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    38,    39,    40,    41,    42,
      -1,    -1,    45,    -1,    -1,    -1,    -1,    -1,    51,    52,
      -1,    -1,    55,    -1,    57,    -1,    59,    -1,    61,    62,
      63,    64,    -1,    66,    -1,    -1,    69,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,     1,    82,
      83,    84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      13,    -1,    -1,    -1,    -1,    18,    -1,   100,    -1,    -1,
      -1,    -1,    25,    26,    27,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    38,    39,    40,    41,    42,
      -1,    -1,    45,    -1,    -1,    -1,    -1,    -1,    51,    52,
      -1,    -1,    55,    -1,    57,    -1,    59,    -1,    61,    62,
      63,    64,    -1,    66,    -1,    -1,     1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    13,    82,
      83,    84,    85,    18,    87,    -1,    -1,    -1,    -1,    92,
      25,    26,    27,    -1,    -1,    -1,    -1,   100,    -1,    -1,
      -1,    -1,    -1,    38,    39,    40,    41,    42,    -1,    -1,
      45,    -1,    -1,    -1,    -1,    -1,    51,    52,    -1,    -1,
      55,    -1,    57,    -1,    59,    -1,    61,    62,    63,    64,
      -1,    66,    -1,    -1,     1,    -1,    -1,    -1,    -1,    74,
      75,    76,    77,    78,    79,    80,    13,    82,    83,    84,
      85,    18,    87,    -1,    -1,    -1,    -1,    92,    25,    26,
      27,    -1,    -1,    -1,    -1,   100,    -1,    -1,    -1,    -1,
      -1,    38,    39,    40,    41,    42,    -1,    -1,    45,    -1,
      -1,    -1,    -1,    -1,    51,    52,    -1,    -1,    55,    -1,
      57,    -1,    59,    -1,    61,    62,    63,    64,    -1,    66,
      -1,    -1,     1,    -1,    -1,    -1,    -1,    74,    75,    76,
      77,    78,    79,    80,    13,    82,    83,    84,    85,    18,
      87,    -1,    -1,    -1,    -1,    92,    25,    26,    27,    -1,
      -1,    -1,    -1,   100,    -1,    -1,    -1,    -1,    -1,    38,
      39,    40,    41,    42,    -1,    -1,    45,    -1,    -1,    -1,
      -1,    -1,    51,    52,    -1,    -1,    55,    -1,    57,    -1,
      59,    -1,    61,    62,    63,    64,    -1,    66,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    13,    82,    83,    84,    85,    18,    87,    -1,
      -1,    -1,    -1,    92,    -1,    26,    27,    -1,    -1,    -1,
      -1,   100,    -1,    -1,    -1,    -1,    -1,    38,    39,    40,
      41,    42,    -1,    -1,    45,    -1,    -1,    -1,    -1,    -1,
      51,    52,    -1,    -1,    55,    -1,    57,    -1,    59,    -1,
      61,    62,    63,    64,    -1,    66,    -1,    -1,     1,    -1,
      -1,    -1,    -1,    74,    75,    76,    77,    78,    79,    80,
      13,    82,    83,    84,    85,    18,    87,    -1,    -1,    -1,
      -1,    -1,    25,    26,    27,    96,    -1,    -1,    -1,   100,
      -1,    -1,    -1,    -1,    -1,    38,    39,    40,    41,    42,
      -1,    -1,    45,    -1,    -1,    -1,    -1,    -1,    51,    52,
      -1,    -1,    55,    -1,    57,    -1,    59,    -1,    61,    62,
      63,    64,    -1,    66,    -1,    -1,     1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    13,    82,
      83,    84,    85,    18,    87,    -1,    -1,    -1,    -1,    -1,
      -1,    26,    27,    -1,    -1,    -1,    -1,   100,    -1,    -1,
      -1,    -1,    -1,    38,    39,    40,    41,    42,    -1,    -1,
      45,    -1,    -1,    -1,    -1,    -1,    51,    52,    -1,    -1,
      55,    -1,    57,    -1,    59,    -1,    61,    62,    63,    64,
      -1,    66,    -1,    -1,     1,    -1,    -1,    -1,    -1,    74,
      75,    76,    77,    78,    79,    80,    13,    82,    83,    84,
      85,    18,    87,    -1,    -1,    -1,    -1,    -1,    25,    26,
      27,    -1,    -1,    -1,    -1,   100,    -1,    -1,    -1,    -1,
      -1,    38,    39,    40,    41,    42,    -1,    -1,    45,    -1,
      -1,    -1,    -1,    -1,    51,    52,    -1,    -1,    55,    -1,
      57,    -1,    59,    -1,    61,    62,    63,    64,    -1,    66,
      -1,    -1,     1,    -1,    -1,    -1,    -1,    74,    75,    76,
      77,    78,    79,    80,    13,    82,    83,    84,    85,    18,
      87,    -1,    -1,    -1,    -1,    -1,    -1,    26,    27,    -1,
      -1,    -1,    99,    -1,    -1,    -1,    -1,    -1,    -1,    38,
      39,    40,    41,    42,    -1,    -1,    45,    -1,    -1,    -1,
      -1,    -1,    51,    52,    -1,    -1,    55,    -1,    57,    -1,
      59,    -1,    61,    62,    63,    64,    -1,    66,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    13,    82,    83,    84,    85,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    26,    27,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    38,    39,    40,
      41,    42,    -1,    -1,    45,    -1,    -1,    -1,    -1,    -1,
      51,    52,    -1,    -1,    55,    -1,    57,    -1,    59,    -1,
      -1,    62,    63,    64,    -1,    66,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    74,    75,    76,    77,    78,    79,    80,
      -1,    82,    83,    84,    85
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,    13,    18,    25,    26,    27,    38,    39,    40,
      41,    42,    45,    51,    52,    55,    57,    59,    61,    62,
      63,    64,    66,    69,    74,    75,    76,    77,    78,    79,
      80,    82,    83,    84,    85,    86,    87,   100,   115,   116,
     117,   119,   120,   121,   130,   131,   132,   133,   134,   135,
     136,   138,   140,   143,   145,   146,   148,   149,   153,   160,
     163,   164,   170,   209,   210,    90,    41,   120,    35,    62,
     139,   242,   120,   154,   242,   120,   120,   139,   120,   120,
     120,   139,    98,   242,     0,   116,    88,    90,   122,   123,
     171,   172,   174,   175,   242,    52,   131,   132,   133,   134,
     135,   120,   120,   120,   120,   120,    91,   137,   224,   137,
      90,   144,   224,    91,    57,    91,    91,   119,   210,     4,
      14,    16,    21,    22,    28,    29,    30,    31,    37,    43,
      44,    48,    53,    54,    56,    58,    67,    90,    92,    95,
     102,   103,   104,   105,   117,   119,   130,   170,   187,   188,
     189,   190,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   223,   224,   226,
     227,   228,   229,   230,   231,   237,   239,   241,   242,   243,
      97,    98,    91,    98,   167,    97,   150,    95,   101,     3,
      23,    32,    33,    46,    49,    50,    95,    98,    99,   100,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     176,    90,    93,    91,    94,   167,    95,    97,   100,   118,
     119,   120,   170,   178,   179,   180,   181,   117,   141,   142,
     209,   141,   141,   147,   139,   161,   162,   171,    91,    90,
      90,   130,   190,   208,   211,    95,    95,    95,   194,   194,
      90,   208,    95,    95,   118,   120,   208,   194,   194,   194,
     194,   171,    95,   211,     5,     6,     7,     8,     9,    10,
      11,    12,    48,    53,    94,    95,   100,   112,   106,   107,
     108,   102,   103,    33,    47,    32,    46,    98,    99,    23,
      49,   109,   110,   111,     3,    50,   113,   211,   212,    92,
     225,   226,    92,   227,    90,    17,    62,   240,   242,   155,
      70,   124,   125,   168,    91,    81,   173,    61,   118,   151,
     152,    43,    96,   101,    95,   123,    68,    91,   182,   208,
     173,   240,   101,   208,   244,   171,   177,   179,    93,    96,
      96,    93,    92,   142,    92,    92,   141,    97,   165,   166,
     242,   165,   129,    90,    67,   119,   229,   233,   234,   235,
     208,   232,    90,   208,   232,    96,    96,   193,   208,   208,
     208,   208,   208,   208,   208,   208,   208,   208,   191,   192,
     208,   208,   238,   242,   195,   195,   195,   196,   196,   197,
     197,   198,   198,   198,   198,   199,   199,   200,   201,   202,
     203,   204,   208,    24,   225,   188,   242,   130,   157,   158,
     159,   197,   242,    92,   125,   169,   124,    95,   242,    93,
      99,    93,    96,    91,   182,   183,    97,   101,    94,   100,
     179,   118,    92,    62,    92,    93,    94,    92,    92,    95,
     122,    93,    90,    96,    96,    96,   195,    93,    96,    96,
      93,   101,    97,   212,   213,   156,    93,    91,    99,   117,
      92,   242,   152,    43,   101,    62,   184,   185,   242,    92,
      93,   173,   182,   101,   244,   166,    43,   232,   229,   232,
     236,   224,   212,   213,   208,   208,   206,    99,   159,   126,
     127,   242,    93,    96,    93,    96,    94,    92,   185,    90,
      94,    92,   182,   101,    96,    90,    15,    20,   221,   222,
      90,    92,    94,   242,    43,   101,    98,   186,   242,   243,
      90,   186,    90,   233,   244,    97,   221,   225,   127,     4,
      73,   128,   186,    96,    96,   197,    96,    97,   211,    91,
     242,   101,    99,   212,   213,   211,   129,   242,    92,    95,
      96
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   114,   115,   115,   116,   116,   116,   116,   116,   117,
     117,   117,   118,   119,   119,   119,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   121,   121,
     121,   121,   121,   121,   121,   122,   122,   123,   123,   124,
     124,   125,   126,   126,   126,   127,   128,   128,   128,   129,
     130,   130,   130,   130,   130,   130,   130,   130,   130,   130,
     130,   131,   132,   133,   133,   133,   133,   133,   133,   133,
     134,   134,   135,   135,   135,   136,   136,   136,   137,   138,
     138,   138,   139,   139,   140,   141,   141,   142,   142,   143,
     144,   145,   145,   147,   146,   146,   148,   148,   150,   149,
     151,   151,   152,   152,   153,   154,   155,   156,   153,   157,
     157,   158,   158,   159,   159,   161,   160,   162,   160,   160,
     163,   164,   165,   165,   166,   166,   168,   167,   169,   169,
     170,   170,   170,   170,   171,   171,   172,   172,   172,   172,
     173,   173,   174,   174,   174,   174,   174,   175,   175,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     177,   177,   177,   178,   178,   179,   179,   179,   180,   180,
     181,   181,   182,   182,   182,   182,   183,   183,   184,   184,
     185,   185,   186,   186,   186,   187,   187,   188,   189,   189,
     189,   189,   190,   190,   190,   190,   190,   190,   191,   191,
     192,   192,   193,   193,   194,   194,   194,   194,   194,   194,
     194,   195,   195,   196,   196,   196,   196,   197,   197,   197,
     198,   198,   198,   199,   199,   199,   199,   199,   200,   200,
     200,   201,   201,   202,   202,   203,   203,   204,   204,   205,
     205,   206,   206,   207,   208,   209,   209,   210,   210,   211,
     211,   211,   212,   212,   212,   212,   212,   212,   212,   212,
     212,   213,   213,   214,   215,   215,   216,   217,   218,   218,
     219,   220,   221,   221,   222,   222,   223,   223,   224,   225,
     226,   226,   227,   227,   228,   228,   229,   229,   229,   229,
     229,   229,   229,   229,   229,   229,   230,   230,   230,   231,
     231,   232,   233,   233,   234,   234,   235,   235,   236,   236,
     237,   237,   238,   239,   240,   241,   242,   243,   243,   243,
     243,   243,   243,   243,   244
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     1,     2,     5,     6,     1,     2,
       3,     2,     2,     1,     1,     2,     1,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     1,     2,
       2,     2,     2,     2,     2,     1,     3,     1,     3,     1,
       2,     5,     1,     3,     2,     3,     1,     5,     4,     0,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     4,     4,     1,     1,     2,
       4,     4,     1,     1,     1,     1,     2,     1,     1,     4,
       1,     2,     4,     0,     5,     1,     3,     5,     0,     5,
       1,     3,     2,     1,     1,     0,     0,     0,     7,     0,
       1,     1,     3,     1,     1,     0,     5,     0,     5,     1,
       2,     1,     1,     3,     1,     3,     0,     4,     0,     2,
       3,     6,     8,    10,     1,     2,     1,     3,     3,     5,
       4,     6,     1,     4,     3,     3,     3,     2,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       0,     4,     3,     1,     3,     2,     2,     4,     0,     1,
       1,     3,     1,     3,     4,     4,     1,     3,     2,     3,
       3,     3,     1,     1,     3,     1,     3,     1,     1,     1,
       3,     4,     1,     2,     2,     3,     4,     4,     0,     1,
       1,     3,     1,     3,     1,     2,     2,     2,     2,     2,
       2,     1,     4,     1,     3,     3,     3,     1,     3,     3,
       1,     3,     3,     1,     3,     3,     3,     3,     1,     3,
       3,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     5,     1,     1,     3,     2,     2,     3,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     4,     2,     3,     2,     4,     2,     4,
       4,     7,     4,     3,     1,     2,     3,     2,     1,     1,
       1,     2,     1,     1,     2,     1,     3,     1,     3,     3,
       3,     3,     3,     3,     3,     3,     5,     7,     9,     5,
       9,     1,     1,     0,     1,     3,     2,     1,     1,     0,
       3,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (cg, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, cg); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, CG *cg)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (cg);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, CG *cg)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, cg);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, CG *cg)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , cg);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, cg); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, CG *cg)
{
  YYUSE (yyvaluep);
  YYUSE (cg);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (CG *cg)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, cg);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:
#line 278 "parser.y" /* yacc.c:1646  */
    { (yyval.dummy) = GlobalInitStatements(cg->current_scope, (yyvsp[0].sc_stmt)); }
#line 2255 "parser.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 280 "parser.y" /* yacc.c:1646  */
    { (yyval.dummy) = NULL; }
#line 2261 "parser.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 282 "parser.y" /* yacc.c:1646  */
    { DefineTechnique(cg, (yyvsp[-3].sc_ident), (yyvsp[-1].sc_expr), NULL); }
#line 2267 "parser.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 284 "parser.y" /* yacc.c:1646  */
    { DefineTechnique(cg, (yyvsp[-4].sc_ident), (yyvsp[-1].sc_expr), (yyvsp[-3].sc_stmt)); }
#line 2273 "parser.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 289 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NULL; }
#line 2279 "parser.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 291 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[-1].sc_stmt); }
#line 2285 "parser.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 293 "parser.y" /* yacc.c:1646  */
    { RecordErrorPos(cg->tokenLoc); (yyval.sc_stmt) = NULL; }
#line 2291 "parser.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 297 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = (yyvsp[0].sc_decl); }
#line 2297 "parser.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 303 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_TYPEDEF); }
#line 2303 "parser.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 308 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_type) = (yyvsp[0].sc_type); }
#line 2309 "parser.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 310 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeQualifiers((yyvsp[-1].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2315 "parser.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 312 "parser.y" /* yacc.c:1646  */
    { cg->SetStorageClass((yyvsp[-1].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2321 "parser.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 314 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeDomain((yyvsp[-1].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2327 "parser.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 316 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeQualifiers((yyvsp[-1].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2333 "parser.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 318 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc((yyvsp[-1].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2339 "parser.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 320 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_PACKED | TYPE_MISC_PACKED_KW); (yyval.sc_type) = cg->type_specs; }
#line 2345 "parser.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 322 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_ROWMAJOR); (yyval.sc_type) = cg->type_specs; }
#line 2351 "parser.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 324 "parser.y" /* yacc.c:1646  */
    { cg->ClearTypeMisc(TYPE_MISC_ROWMAJOR); (yyval.sc_type) = cg->type_specs; }
#line 2357 "parser.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 326 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_PRECISION*1); (yyval.sc_type) = cg->type_specs; }
#line 2363 "parser.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 328 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_PRECISION*2); (yyval.sc_type) = cg->type_specs; }
#line 2369 "parser.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 330 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_PRECISION*3); (yyval.sc_type) = cg->type_specs; }
#line 2375 "parser.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 335 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_type) = *SetDType(&cg->type_specs, (yyvsp[0].sc_ptype)); }
#line 2381 "parser.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 337 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeQualifiers((yyvsp[0].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2387 "parser.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 339 "parser.y" /* yacc.c:1646  */
    { cg->SetStorageClass((yyvsp[0].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2393 "parser.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 341 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeDomain((yyvsp[0].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2399 "parser.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 343 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeQualifiers((yyvsp[0].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2405 "parser.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 345 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc((yyvsp[0].sc_int)); (yyval.sc_type) = cg->type_specs; }
#line 2411 "parser.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 347 "parser.y" /* yacc.c:1646  */
    { cg->SetTypeMisc(TYPE_MISC_PACKED | TYPE_MISC_PACKED_KW); (yyval.sc_type) = cg->type_specs; }
#line 2417 "parser.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 351 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[0].sc_stmt); }
#line 2423 "parser.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 353 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = AddStmt((yyvsp[-2].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 2429 "parser.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 357 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = Init_Declarator(cg, (yyvsp[0].sc_decl), NULL); }
#line 2435 "parser.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 359 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = Init_Declarator(cg, (yyvsp[-2].sc_decl), (yyvsp[0].sc_expr)); }
#line 2441 "parser.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 367 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[0].sc_expr), NULL); }
#line 2447 "parser.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 369 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[-1].sc_expr), (yyvsp[0].sc_expr)); }
#line 2453 "parser.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 373 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = StateInitializer(cg, (yyvsp[-3].sc_ident), (yyvsp[-1].sc_expr)); }
#line 2459 "parser.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 377 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[0].sc_expr), NULL); }
#line 2465 "parser.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 379 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 2471 "parser.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 381 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = (yyvsp[-1].sc_expr); }
#line 2477 "parser.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 385 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = StateInitializer(cg, (yyvsp[-2].sc_ident), (yyvsp[0].sc_expr)); }
#line 2483 "parser.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 390 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = SymbolicConstant(cg, (yyvsp[-2].sc_ident), (yyvsp[-3].sc_ident)); }
#line 2489 "parser.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 392 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = SymbolicConstant(cg, 0, 0); }
#line 2495 "parser.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 395 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = ParseAsm(cg, cg->tokenLoc); }
#line 2501 "parser.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 404 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, INT_SY); }
#line 2507 "parser.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 406 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, INT_SY); }
#line 2513 "parser.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 408 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, FLOAT_SY); }
#line 2519 "parser.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 410 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, VOID_SY); }
#line 2525 "parser.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 412 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, BOOLEAN_SY); }
#line 2531 "parser.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 414 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, TEXOBJ_SY); }
#line 2537 "parser.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 416 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = (yyvsp[0].sc_ptype); }
#line 2543 "parser.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 418 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = (yyvsp[0].sc_ptype); }
#line 2549 "parser.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 420 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = LookUpTypeSymbol(cg, (yyvsp[0].sc_ident)); }
#line 2555 "parser.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 422 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = (yyvsp[0].sc_ptype); }
#line 2561 "parser.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 424 "parser.y" /* yacc.c:1646  */
    {
								SemanticParseError(cg, cg->tokenLoc, ERROR_S_TYPE_NAME_EXPECTED, cg->GetAtomString(cg->last_token /* yychar */));
								(yyval.sc_ptype) = UndefinedType;
							}
#line 2570 "parser.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 435 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_QUALIFIER_CONST; }
#line 2576 "parser.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 443 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_DOMAIN_UNIFORM; }
#line 2582 "parser.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 451 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_STATIC; }
#line 2588 "parser.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 453 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_EXTERN; }
#line 2594 "parser.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 455 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_NOINTERP; }
#line 2600 "parser.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 457 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_PRECISE; }
#line 2606 "parser.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 459 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_SHARED; }
#line 2612 "parser.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 461 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_GROUPSHARED; }
#line 2618 "parser.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 463 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = SC_UNKNOWN; }
#line 2624 "parser.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 471 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_MISC_INLINE; }
#line 2630 "parser.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 473 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_MISC_INTERNAL; }
#line 2636 "parser.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 481 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_QUALIFIER_IN; }
#line 2642 "parser.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 483 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_QUALIFIER_OUT; }
#line 2648 "parser.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 485 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = TYPE_QUALIFIER_INOUT; }
#line 2654 "parser.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 494 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = SetStructMembers(cg, (yyvsp[-3].sc_ptype), cg->PopScope()); }
#line 2660 "parser.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 496 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = SetStructMembers(cg, (yyvsp[-3].sc_ptype), cg->PopScope()); }
#line 2666 "parser.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 498 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = (yyvsp[0].sc_ptype); }
#line 2672 "parser.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 502 "parser.y" /* yacc.c:1646  */
    { cg->current_scope->flags |= Scope::is_struct; (yyval.dummy) = (yyvsp[0].dummy); }
#line 2678 "parser.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 507 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, (yyvsp[0].sc_ident)); }
#line 2684 "parser.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 509 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = StructHeader(cg, cg->tokenLoc, cg->current_scope, (yyvsp[0].sc_ident), (yyvsp[-2].sc_ident)); }
#line 2690 "parser.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 511 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = AddStructBase(StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, (yyvsp[-2].sc_ident)), LookUpTypeSymbol(cg, (yyvsp[0].sc_ident))); }
#line 2696 "parser.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 519 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, 0); }
#line 2702 "parser.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 535 "parser.y" /* yacc.c:1646  */
    { SetConstantBuffer(cg, cg->tokenLoc, (yyvsp[-3].sc_sym), cg->PopScope()); }
#line 2708 "parser.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 538 "parser.y" /* yacc.c:1646  */
    { cg->current_scope->flags |= Scope::is_struct | Scope::is_cbuffer; (yyval.dummy) = (yyvsp[0].dummy); }
#line 2714 "parser.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 542 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_sym) = ConstantBuffer(cg, cg->tokenLoc, cg->current_scope, (yyvsp[0].sc_ident), 0); }
#line 2720 "parser.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 544 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_sym) = ConstantBuffer(cg, cg->tokenLoc, cg->current_scope, (yyvsp[-2].sc_ident), (yyvsp[0].sc_ident)); }
#line 2726 "parser.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 551 "parser.y" /* yacc.c:1646  */
    { cg->PushScope((yyvsp[-1].sc_ptype)->str.members); cg->current_scope->flags |= Scope::is_struct; }
#line 2732 "parser.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 552 "parser.y" /* yacc.c:1646  */
    { cg->PopScope(); (yyval.sc_ptype) = (yyvsp[-4].sc_ptype); }
#line 2738 "parser.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 557 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = TemplateHeader(cg, cg->tokenLoc, cg->current_scope, (yyvsp[0].sc_ident), (yyvsp[-2].sc_decl)); }
#line 2744 "parser.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 559 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = AddStructBase(TemplateHeader(cg, cg->tokenLoc, cg->current_scope, (yyvsp[-2].sc_ident), (yyvsp[-4].sc_decl)), LookUpTypeSymbol(cg, (yyvsp[0].sc_ident))); }
#line 2750 "parser.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 562 "parser.y" /* yacc.c:1646  */
    { cg->current_scope->formal++; }
#line 2756 "parser.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 564 "parser.y" /* yacc.c:1646  */
    { cg->current_scope->formal--; (yyval.sc_decl) = (yyvsp[-1].sc_decl); }
#line 2762 "parser.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 568 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = (yyvsp[0].sc_decl); }
#line 2768 "parser.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 570 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = AddDecl((yyvsp[-2].sc_decl), (yyvsp[0].sc_decl)); }
#line 2774 "parser.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 574 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = NewDeclNode(cg->tokenLoc, (yyvsp[0].sc_ident), 0); }
#line 2780 "parser.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 576 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = (yyvsp[0].sc_decl); }
#line 2786 "parser.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 584 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = InstantiateTemplate(cg, cg->tokenLoc, cg->current_scope, LookUpTypeSymbol(cg, (yyvsp[0].sc_ident)), 0); }
#line 2792 "parser.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 585 "parser.y" /* yacc.c:1646  */
    {}
#line 2798 "parser.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 585 "parser.y" /* yacc.c:1646  */
    {}
#line 2804 "parser.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 585 "parser.y" /* yacc.c:1646  */
    {}
#line 2810 "parser.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 586 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = InstantiateTemplate(cg, cg->tokenLoc, cg->current_scope, LookUpTypeSymbol(cg, (yyvsp[-6].sc_ident)), (yyvsp[-2].sc_typelist)); }
#line 2816 "parser.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 589 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_typelist) = NULL; }
#line 2822 "parser.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 595 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_typelist) = AddtoTypeList(cg, NULL, (yyvsp[0].sc_ptype)); }
#line 2828 "parser.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 597 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_typelist) = AddtoTypeList(cg, (yyvsp[-2].sc_typelist), (yyvsp[0].sc_ptype)); }
#line 2834 "parser.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 602 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = IntToType(cg, GetConstant(cg, (yyvsp[0].sc_expr), 0)); }
#line 2840 "parser.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 608 "parser.y" /* yacc.c:1646  */
    {SetDType(&cg->type_specs, (yyvsp[-1].sc_ptype));}
#line 2846 "parser.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 609 "parser.y" /* yacc.c:1646  */
    {SetDType(&cg->type_specs, (yyvsp[-1].sc_ptype));}
#line 2852 "parser.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 614 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = EnumHeader(cg, cg->tokenLoc, cg->current_scope, (yyvsp[0].sc_ident)); }
#line 2858 "parser.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 618 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ptype) = EnumHeader(cg, cg->tokenLoc, cg->current_scope, 0); }
#line 2864 "parser.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 626 "parser.y" /* yacc.c:1646  */
    { EnumAdd(cg, cg->tokenLoc, cg->current_scope, cg->type_specs.basetype, (yyvsp[0].sc_ident), 0); }
#line 2870 "parser.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 628 "parser.y" /* yacc.c:1646  */
    { EnumAdd(cg, cg->tokenLoc, cg->current_scope, cg->type_specs.basetype, (yyvsp[-2].sc_ident), (yyvsp[0].sc_int)); }
#line 2876 "parser.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 635 "parser.y" /* yacc.c:1646  */
    { cg->PushScope(); }
#line 2882 "parser.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 636 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[-1].sc_stmt); cg->PopScope(); }
#line 2888 "parser.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 640 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = 0; }
#line 2894 "parser.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 649 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_attr) = Attribute((yyvsp[-1].sc_ident), 0, 0, 0); }
#line 2900 "parser.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 651 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_attr) = Attribute((yyvsp[-4].sc_ident), 1, (yyvsp[-2].sc_int), 0); }
#line 2906 "parser.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 653 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_attr) = Attribute((yyvsp[-6].sc_ident), 2, (yyvsp[-4].sc_int), (yyvsp[-2].sc_int)); }
#line 2912 "parser.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 655 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_attr) = Attribute((yyvsp[-8].sc_ident), 2, (yyvsp[-6].sc_int), (yyvsp[-4].sc_int)); }
#line 2918 "parser.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 667 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Declarator(cg, (yyvsp[0].sc_decl), 0, 0); }
#line 2924 "parser.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 669 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Declarator(cg, (yyvsp[-2].sc_decl), (yyvsp[0].sc_ident), 0); }
#line 2930 "parser.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 671 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Declarator(cg, (yyvsp[-2].sc_decl), 0, (yyvsp[0].sc_ident)); }
#line 2936 "parser.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 673 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Declarator(cg, (yyvsp[-4].sc_decl), (yyvsp[-2].sc_ident), (yyvsp[0].sc_ident)); }
#line 2942 "parser.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 677 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = (yyvsp[-1].sc_ident); }
#line 2948 "parser.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 679 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = (yyvsp[-1].sc_ident); }
#line 2954 "parser.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 683 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = NewDeclNode(cg->tokenLoc, (yyvsp[0].sc_ident), &cg->type_specs); }
#line 2960 "parser.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 685 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Array_Declarator(cg, (yyvsp[-3].sc_decl), (yyvsp[-1].sc_int), 0); }
#line 2966 "parser.cpp" /* yacc.c:1646  */
    break;

  case 144:
#line 687 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Array_Declarator(cg, (yyvsp[-2].sc_decl), 0, 1); }
#line 2972 "parser.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 689 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = SetFunTypeParams(cg, (yyvsp[-2].sc_decl), (yyvsp[-1].sc_decl), (yyvsp[-1].sc_decl)); }
#line 2978 "parser.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 691 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = SetFunTypeParams(cg, (yyvsp[-2].sc_decl), (yyvsp[-1].sc_decl), NULL); }
#line 2984 "parser.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 695 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = FunctionDeclHeader(cg, (yyvsp[-1].sc_decl)->loc, cg->current_scope, (yyvsp[-1].sc_decl)); }
#line 2990 "parser.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 697 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = FunctionDeclHeader(cg, cg->tokenLoc, cg->current_scope, NewDeclNode(cg->tokenLoc, GetOperatorName(cg, (yyvsp[-1].sc_ident)), &cg->type_specs)); }
#line 2996 "parser.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 700 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = POS_OP;		}
#line 3002 "parser.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 701 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = NEG_OP;		}
#line 3008 "parser.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 702 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = BNOT_OP;		}
#line 3014 "parser.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 703 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = NOT_OP;		}
#line 3020 "parser.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 704 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = MUL_OP;		}
#line 3026 "parser.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 705 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = DIV_OP;		}
#line 3032 "parser.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 706 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = MOD_OP;		}
#line 3038 "parser.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 707 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = SHR_OP;		}
#line 3044 "parser.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 708 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = LT_OP;		}
#line 3050 "parser.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 709 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = GT_OP;		}
#line 3056 "parser.cpp" /* yacc.c:1646  */
    break;

  case 159:
#line 710 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = LE_OP;		}
#line 3062 "parser.cpp" /* yacc.c:1646  */
    break;

  case 160:
#line 711 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = GE_OP;		}
#line 3068 "parser.cpp" /* yacc.c:1646  */
    break;

  case 161:
#line 712 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = EQ_OP;		}
#line 3074 "parser.cpp" /* yacc.c:1646  */
    break;

  case 162:
#line 713 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = NE_OP;		}
#line 3080 "parser.cpp" /* yacc.c:1646  */
    break;

  case 163:
#line 714 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = AND_OP;		}
#line 3086 "parser.cpp" /* yacc.c:1646  */
    break;

  case 164:
#line 715 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = XOR_OP;		}
#line 3092 "parser.cpp" /* yacc.c:1646  */
    break;

  case 165:
#line 716 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = OR_OP;		}
#line 3098 "parser.cpp" /* yacc.c:1646  */
    break;

  case 166:
#line 717 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = BAND_OP;		}
#line 3104 "parser.cpp" /* yacc.c:1646  */
    break;

  case 167:
#line 718 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = BOR_OP;		}
#line 3110 "parser.cpp" /* yacc.c:1646  */
    break;

  case 168:
#line 719 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = FUN_CALL_OP;		}
#line 3116 "parser.cpp" /* yacc.c:1646  */
    break;

  case 169:
#line 720 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_ident) = ARRAY_INDEX_OP;	}
#line 3122 "parser.cpp" /* yacc.c:1646  */
    break;

  case 170:
#line 724 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = NewDeclNode(cg->tokenLoc, 0, &cg->type_specs); }
#line 3128 "parser.cpp" /* yacc.c:1646  */
    break;

  case 171:
#line 726 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Array_Declarator(cg, (yyvsp[-3].sc_decl), (yyvsp[-1].sc_int), 0); }
#line 3134 "parser.cpp" /* yacc.c:1646  */
    break;

  case 172:
#line 728 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Array_Declarator(cg, (yyvsp[-2].sc_decl), 0 , 1); }
#line 3140 "parser.cpp" /* yacc.c:1646  */
    break;

  case 173:
#line 745 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = (yyvsp[0].sc_decl); }
#line 3146 "parser.cpp" /* yacc.c:1646  */
    break;

  case 174:
#line 747 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = AddDecl((yyvsp[-2].sc_decl), (yyvsp[0].sc_decl)); }
#line 3152 "parser.cpp" /* yacc.c:1646  */
    break;

  case 175:
#line 751 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = AddDeclAttribute((yyvsp[0].sc_decl), (yyvsp[-1].sc_attr)); }
#line 3158 "parser.cpp" /* yacc.c:1646  */
    break;

  case 176:
#line 753 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Param_Init_Declarator(cg, (yyvsp[0].sc_decl), NULL); }
#line 3164 "parser.cpp" /* yacc.c:1646  */
    break;

  case 177:
#line 755 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Param_Init_Declarator(cg, (yyvsp[-2].sc_decl), (yyvsp[0].sc_expr)); }
#line 3170 "parser.cpp" /* yacc.c:1646  */
    break;

  case 178:
#line 759 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = NULL; }
#line 3176 "parser.cpp" /* yacc.c:1646  */
    break;

  case 180:
#line 765 "parser.y" /* yacc.c:1646  */
    {
								if (IsVoid(&(yyvsp[0].sc_decl)->type.type))
								cg->current_scope->flags |= Scope::has_void_param;
								(yyval.sc_decl) = (yyvsp[0].sc_decl);
							}
#line 3186 "parser.cpp" /* yacc.c:1646  */
    break;

  case 181:
#line 771 "parser.y" /* yacc.c:1646  */
    {
								if ((cg->current_scope->flags & Scope::has_void_param) || IsVoid(&(yyvsp[-2].sc_decl)->type.type))
								SemanticError(cg, cg->tokenLoc, ERROR___VOID_NOT_ONLY_PARAM);
								(yyval.sc_decl) = AddDecl((yyvsp[-2].sc_decl), (yyvsp[0].sc_decl));
							}
#line 3196 "parser.cpp" /* yacc.c:1646  */
    break;

  case 182:
#line 783 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = Initializer(cg, (yyvsp[0].sc_expr)); }
#line 3202 "parser.cpp" /* yacc.c:1646  */
    break;

  case 183:
#line 785 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = Initializer(cg, (yyvsp[-1].sc_expr)); }
#line 3208 "parser.cpp" /* yacc.c:1646  */
    break;

  case 184:
#line 787 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = Initializer(cg, (yyvsp[-2].sc_expr)); }
#line 3214 "parser.cpp" /* yacc.c:1646  */
    break;

  case 185:
#line 789 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = (yyvsp[-1].sc_expr);}
#line 3220 "parser.cpp" /* yacc.c:1646  */
    break;

  case 186:
#line 793 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[0].sc_expr), NULL); }
#line 3226 "parser.cpp" /* yacc.c:1646  */
    break;

  case 187:
#line 795 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3232 "parser.cpp" /* yacc.c:1646  */
    break;

  case 188:
#line 799 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[-1].sc_expr), NULL); }
#line 3238 "parser.cpp" /* yacc.c:1646  */
    break;

  case 189:
#line 801 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = InitializerList((yyvsp[-2].sc_expr), (yyvsp[-1].sc_expr)); }
#line 3244 "parser.cpp" /* yacc.c:1646  */
    break;

  case 190:
#line 805 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = StateInitializer(cg, (yyvsp[-2].sc_ident), (yyvsp[0].sc_expr)); }
#line 3250 "parser.cpp" /* yacc.c:1646  */
    break;

  case 191:
#line 807 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = StateInitializer(cg, (yyvsp[-2].sc_ident), (yyvsp[0].sc_expr)); }
#line 3256 "parser.cpp" /* yacc.c:1646  */
    break;

  case 192:
#line 811 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = SymbolicConstant(cg, (yyvsp[0].sc_ident), 0); }
#line 3262 "parser.cpp" /* yacc.c:1646  */
    break;

  case 194:
#line 814 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = (yyvsp[-1].sc_expr); }
#line 3268 "parser.cpp" /* yacc.c:1646  */
    break;

  case 195:
#line 825 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = (yyvsp[0].sc_expr); }
#line 3274 "parser.cpp" /* yacc.c:1646  */
    break;

  case 196:
#line 827 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = (yyvsp[0].sc_expr); }
#line 3280 "parser.cpp" /* yacc.c:1646  */
    break;

  case 197:
#line 831 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = BasicVariable(cg, (yyvsp[0].sc_ident)); }
#line 3286 "parser.cpp" /* yacc.c:1646  */
    break;

  case 200:
#line 841 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = (yyvsp[-1].sc_expr); }
#line 3292 "parser.cpp" /* yacc.c:1646  */
    break;

  case 201:
#line 843 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewConstructor(cg, cg->tokenLoc, (yyvsp[-3].sc_ptype), (yyvsp[-1].sc_expr)); }
#line 3298 "parser.cpp" /* yacc.c:1646  */
    break;

  case 203:
#line 852 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewUnopNode(cg, POSTINC_OP, (yyvsp[-1].sc_expr)); }
#line 3304 "parser.cpp" /* yacc.c:1646  */
    break;

  case 204:
#line 854 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewUnopNode(cg, POSTDEC_OP, (yyvsp[-1].sc_expr)); }
#line 3310 "parser.cpp" /* yacc.c:1646  */
    break;

  case 205:
#line 856 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewMemberSelectorOrSwizzleOrWriteMaskOperator(cg, cg->tokenLoc, (yyvsp[-2].sc_expr), (yyvsp[0].sc_ident)); }
#line 3316 "parser.cpp" /* yacc.c:1646  */
    break;

  case 206:
#line 858 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewIndexOperator(cg, cg->tokenLoc, (yyvsp[-3].sc_expr), (yyvsp[-1].sc_expr)); }
#line 3322 "parser.cpp" /* yacc.c:1646  */
    break;

  case 207:
#line 860 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewFunctionCallOperator(cg, cg->tokenLoc, (yyvsp[-3].sc_expr), (yyvsp[-1].sc_expr)); }
#line 3328 "parser.cpp" /* yacc.c:1646  */
    break;

  case 208:
#line 864 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NULL; }
#line 3334 "parser.cpp" /* yacc.c:1646  */
    break;

  case 210:
#line 869 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = ArgumentList(cg, NULL, (yyvsp[0].sc_expr)); }
#line 3340 "parser.cpp" /* yacc.c:1646  */
    break;

  case 211:
#line 871 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = ArgumentList(cg, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3346 "parser.cpp" /* yacc.c:1646  */
    break;

  case 212:
#line 875 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = ExpressionList(cg, NULL, (yyvsp[0].sc_expr)); }
#line 3352 "parser.cpp" /* yacc.c:1646  */
    break;

  case 213:
#line 877 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = ExpressionList(cg, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3358 "parser.cpp" /* yacc.c:1646  */
    break;

  case 215:
#line 886 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewUnopNode(cg, PREINC_OP, (yyvsp[0].sc_expr)); }
#line 3364 "parser.cpp" /* yacc.c:1646  */
    break;

  case 216:
#line 888 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewUnopNode(cg, PREDEC_OP, (yyvsp[0].sc_expr)); }
#line 3370 "parser.cpp" /* yacc.c:1646  */
    break;

  case 217:
#line 890 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewUnaryOperator(cg, cg->tokenLoc, POS_OP, '+', (yyvsp[0].sc_expr), 0); }
#line 3376 "parser.cpp" /* yacc.c:1646  */
    break;

  case 218:
#line 892 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewUnaryOperator(cg, cg->tokenLoc, NEG_OP, '-', (yyvsp[0].sc_expr), 0); }
#line 3382 "parser.cpp" /* yacc.c:1646  */
    break;

  case 219:
#line 894 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewUnaryOperator(cg, cg->tokenLoc, BNOT_OP, '!', (yyvsp[0].sc_expr), 0); }
#line 3388 "parser.cpp" /* yacc.c:1646  */
    break;

  case 220:
#line 896 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewUnaryOperator(cg, cg->tokenLoc, NOT_OP, '~', (yyvsp[0].sc_expr), 1); }
#line 3394 "parser.cpp" /* yacc.c:1646  */
    break;

  case 222:
#line 908 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewCastOperator(cg, cg->tokenLoc, (yyvsp[0].sc_expr), GetTypePointer(cg, (yyvsp[-2].sc_decl)->loc, &(yyvsp[-2].sc_decl)->type)); }
#line 3400 "parser.cpp" /* yacc.c:1646  */
    break;

  case 224:
#line 917 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, MUL_OP, '*', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 0); }
#line 3406 "parser.cpp" /* yacc.c:1646  */
    break;

  case 225:
#line 919 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, DIV_OP, '/', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 0); }
#line 3412 "parser.cpp" /* yacc.c:1646  */
    break;

  case 226:
#line 921 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, MOD_OP, '%', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 1); }
#line 3418 "parser.cpp" /* yacc.c:1646  */
    break;

  case 228:
#line 930 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, ADD_OP, '+', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 0); }
#line 3424 "parser.cpp" /* yacc.c:1646  */
    break;

  case 229:
#line 932 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, SUB_OP, '-', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 0); }
#line 3430 "parser.cpp" /* yacc.c:1646  */
    break;

  case 231:
#line 941 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, SHL_OP, LL_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 1); }
#line 3436 "parser.cpp" /* yacc.c:1646  */
    break;

  case 232:
#line 943 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, SHR_OP, GG_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 1); }
#line 3442 "parser.cpp" /* yacc.c:1646  */
    break;

  case 234:
#line 952 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryComparisonOperator(cg, cg->tokenLoc, LT_OP, '<', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3448 "parser.cpp" /* yacc.c:1646  */
    break;

  case 235:
#line 954 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryComparisonOperator(cg, cg->tokenLoc, GT_OP, '>', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3454 "parser.cpp" /* yacc.c:1646  */
    break;

  case 236:
#line 956 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryComparisonOperator(cg, cg->tokenLoc, LE_OP, LE_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3460 "parser.cpp" /* yacc.c:1646  */
    break;

  case 237:
#line 958 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryComparisonOperator(cg, cg->tokenLoc, GE_OP, GE_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3466 "parser.cpp" /* yacc.c:1646  */
    break;

  case 239:
#line 967 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryComparisonOperator(cg, cg->tokenLoc, EQ_OP, EQ_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3472 "parser.cpp" /* yacc.c:1646  */
    break;

  case 240:
#line 969 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryComparisonOperator(cg, cg->tokenLoc, NE_OP, NE_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3478 "parser.cpp" /* yacc.c:1646  */
    break;

  case 242:
#line 978 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, AND_OP, '&', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 1); }
#line 3484 "parser.cpp" /* yacc.c:1646  */
    break;

  case 244:
#line 987 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, XOR_OP, '^', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 1); }
#line 3490 "parser.cpp" /* yacc.c:1646  */
    break;

  case 246:
#line 996 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryOperator(cg, cg->tokenLoc, OR_OP, '|', (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 1); }
#line 3496 "parser.cpp" /* yacc.c:1646  */
    break;

  case 248:
#line 1005 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryBooleanOperator(cg, cg->tokenLoc, BAND_OP, AND_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3502 "parser.cpp" /* yacc.c:1646  */
    break;

  case 250:
#line 1014 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewBinaryBooleanOperator(cg, cg->tokenLoc, BOR_OP, OR_SY, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3508 "parser.cpp" /* yacc.c:1646  */
    break;

  case 252:
#line 1023 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NewConditionalOperator(cg, cg->tokenLoc, (yyvsp[-4].sc_expr), (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3514 "parser.cpp" /* yacc.c:1646  */
    break;

  case 253:
#line 1027 "parser.y" /* yacc.c:1646  */
    {	int len; (yyval.sc_expr) = CheckBooleanExpr(cg, cg->tokenLoc, (yyvsp[0].sc_expr), &len); }
#line 3520 "parser.cpp" /* yacc.c:1646  */
    break;

  case 255:
#line 1046 "parser.y" /* yacc.c:1646  */
    { DefineFunction(cg, (yyvsp[-2].sc_decl), (yyvsp[-1].sc_stmt)); cg->PopScope(); }
#line 3526 "parser.cpp" /* yacc.c:1646  */
    break;

  case 256:
#line 1048 "parser.y" /* yacc.c:1646  */
    { DefineFunction(cg, (yyvsp[-1].sc_decl), NULL); cg->PopScope(); }
#line 3532 "parser.cpp" /* yacc.c:1646  */
    break;

  case 257:
#line 1053 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = AddDeclAttribute((yyvsp[0].sc_decl), (yyvsp[-1].sc_attr)); }
#line 3538 "parser.cpp" /* yacc.c:1646  */
    break;

  case 258:
#line 1055 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_decl) = Function_Definition_Header(cg, (yyvsp[-1].sc_decl)); }
#line 3544 "parser.cpp" /* yacc.c:1646  */
    break;

  case 259:
#line 1063 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = AddStmtAttribute((yyvsp[0].sc_stmt), (yyvsp[-1].sc_attr)); }
#line 3550 "parser.cpp" /* yacc.c:1646  */
    break;

  case 273:
#line 1088 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[-1].sc_stmt); }
#line 3556 "parser.cpp" /* yacc.c:1646  */
    break;

  case 274:
#line 1095 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewDiscardStmt(cg->tokenLoc, NewUnopSubNode(cg, KILL_OP, SUBOP_V(0, TYPE_BASE_BOOLEAN), NULL)); }
#line 3562 "parser.cpp" /* yacc.c:1646  */
    break;

  case 275:
#line 1097 "parser.y" /* yacc.c:1646  */
    {
								int	len;
								expr *e = CheckBooleanExpr(cg, cg->tokenLoc, (yyvsp[-1].sc_expr), &len);
								(yyval.sc_stmt) = NewDiscardStmt(cg->tokenLoc, NewUnopSubNode(cg, KILL_OP, SUBOP_V(len, TYPE_BASE_BOOLEAN), e));
							}
#line 3572 "parser.cpp" /* yacc.c:1646  */
    break;

  case 276:
#line 1107 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewBreakStmt(cg->tokenLoc); }
#line 3578 "parser.cpp" /* yacc.c:1646  */
    break;

  case 277:
#line 1115 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	SetThenElseStmts((yyvsp[-3].sc_stmt), (yyvsp[-2].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 3584 "parser.cpp" /* yacc.c:1646  */
    break;

  case 278:
#line 1119 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	SetThenElseStmts((yyvsp[-1].sc_stmt), (yyvsp[0].sc_stmt), NULL); }
#line 3590 "parser.cpp" /* yacc.c:1646  */
    break;

  case 279:
#line 1121 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	SetThenElseStmts((yyvsp[-3].sc_stmt), (yyvsp[-2].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 3596 "parser.cpp" /* yacc.c:1646  */
    break;

  case 280:
#line 1125 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewIfStmt(cg->tokenLoc, (yyvsp[-1].sc_expr), NULL, NULL); ; }
#line 3602 "parser.cpp" /* yacc.c:1646  */
    break;

  case 281:
#line 1133 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewSwitchStmt(cg->tokenLoc, (yyvsp[-4].sc_expr), (yyvsp[-1].sc_stmt), cg->popped_scope); }
#line 3608 "parser.cpp" /* yacc.c:1646  */
    break;

  case 282:
#line 1137 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[0].sc_stmt); }
#line 3614 "parser.cpp" /* yacc.c:1646  */
    break;

  case 283:
#line 1139 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[0].sc_stmt); }
#line 3620 "parser.cpp" /* yacc.c:1646  */
    break;

  case 285:
#line 1144 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = AddStmt((yyvsp[-1].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 3626 "parser.cpp" /* yacc.c:1646  */
    break;

  case 286:
#line 1152 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewBlockStmt(cg->tokenLoc, (yyvsp[-1].sc_stmt), cg->popped_scope); }
#line 3632 "parser.cpp" /* yacc.c:1646  */
    break;

  case 287:
#line 1154 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NULL; }
#line 3638 "parser.cpp" /* yacc.c:1646  */
    break;

  case 288:
#line 1158 "parser.y" /* yacc.c:1646  */
    { cg->PushScope(); cg->current_scope->funindex = cg->func_index; }
#line 3644 "parser.cpp" /* yacc.c:1646  */
    break;

  case 289:
#line 1162 "parser.y" /* yacc.c:1646  */
    {
								if (cg->opts & cgclib::DUMP_PARSETREE)
									PrintScopeDeclarations(cg);
								cg->PopScope();
							}
#line 3654 "parser.cpp" /* yacc.c:1646  */
    break;

  case 291:
#line 1171 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = AddStmt((yyvsp[-1].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 3660 "parser.cpp" /* yacc.c:1646  */
    break;

  case 293:
#line 1176 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = CheckStmt((yyvsp[0].sc_stmt)); }
#line 3666 "parser.cpp" /* yacc.c:1646  */
    break;

  case 295:
#line 1185 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NULL; }
#line 3672 "parser.cpp" /* yacc.c:1646  */
    break;

  case 296:
#line 1189 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewSimpleAssignmentStmt(cg, cg->tokenLoc, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr), 0); }
#line 3678 "parser.cpp" /* yacc.c:1646  */
    break;

  case 297:
#line 1191 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewExprStmt(cg->tokenLoc, (yyvsp[0].sc_expr)); }
#line 3684 "parser.cpp" /* yacc.c:1646  */
    break;

  case 298:
#line 1193 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNMINUS_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3690 "parser.cpp" /* yacc.c:1646  */
    break;

  case 299:
#line 1195 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNMOD_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3696 "parser.cpp" /* yacc.c:1646  */
    break;

  case 300:
#line 1197 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNPLUS_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3702 "parser.cpp" /* yacc.c:1646  */
    break;

  case 301:
#line 1199 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNSLASH_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3708 "parser.cpp" /* yacc.c:1646  */
    break;

  case 302:
#line 1201 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNSTAR_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3714 "parser.cpp" /* yacc.c:1646  */
    break;

  case 303:
#line 1203 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNAND_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3720 "parser.cpp" /* yacc.c:1646  */
    break;

  case 304:
#line 1205 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNOR_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3726 "parser.cpp" /* yacc.c:1646  */
    break;

  case 305:
#line 1207 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNXOR_OP, (yyvsp[-2].sc_expr), (yyvsp[0].sc_expr)); }
#line 3732 "parser.cpp" /* yacc.c:1646  */
    break;

  case 306:
#line 1215 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewWhileStmt(cg->tokenLoc, WHILE_STMT, (yyvsp[-2].sc_expr), (yyvsp[0].sc_stmt)); }
#line 3738 "parser.cpp" /* yacc.c:1646  */
    break;

  case 307:
#line 1217 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewWhileStmt(cg->tokenLoc, DO_STMT, (yyvsp[-2].sc_expr), (yyvsp[-5].sc_stmt)); }
#line 3744 "parser.cpp" /* yacc.c:1646  */
    break;

  case 308:
#line 1219 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewForStmt(cg->tokenLoc, (yyvsp[-6].sc_stmt), (yyvsp[-4].sc_expr), (yyvsp[-2].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 3750 "parser.cpp" /* yacc.c:1646  */
    break;

  case 309:
#line 1223 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewWhileStmt(cg->tokenLoc, WHILE_STMT, (yyvsp[-2].sc_expr), (yyvsp[0].sc_stmt)); }
#line 3756 "parser.cpp" /* yacc.c:1646  */
    break;

  case 310:
#line 1225 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewForStmt(cg->tokenLoc, (yyvsp[-6].sc_stmt), (yyvsp[-4].sc_expr), (yyvsp[-2].sc_stmt), (yyvsp[0].sc_stmt)); }
#line 3762 "parser.cpp" /* yacc.c:1646  */
    break;

  case 311:
#line 1230 "parser.y" /* yacc.c:1646  */
    {	(yyval.sc_expr) = CheckBooleanExpr(cg, cg->tokenLoc, (yyvsp[0].sc_expr), NULL); }
#line 3768 "parser.cpp" /* yacc.c:1646  */
    break;

  case 313:
#line 1235 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = NULL; }
#line 3774 "parser.cpp" /* yacc.c:1646  */
    break;

  case 315:
#line 1240 "parser.y" /* yacc.c:1646  */
    {
								if (stmt *lstmt = (yyvsp[-2].sc_stmt)) {
									while (lstmt->next)
										lstmt = lstmt->next;
									lstmt->next = (yyvsp[0].sc_stmt);
									(yyval.sc_stmt) = (yyvsp[-2].sc_stmt);
								} else {
									(yyval.sc_stmt) = (yyvsp[0].sc_stmt);
								}
						}
#line 3789 "parser.cpp" /* yacc.c:1646  */
    break;

  case 316:
#line 1252 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) = (yyvsp[0].sc_stmt); }
#line 3795 "parser.cpp" /* yacc.c:1646  */
    break;

  case 319:
#line 1258 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) = NULL; }
#line 3801 "parser.cpp" /* yacc.c:1646  */
    break;

  case 320:
#line 1266 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewReturnStmt(cg, cg->tokenLoc, cg->current_scope, (yyvsp[-1].sc_expr)); }
#line 3807 "parser.cpp" /* yacc.c:1646  */
    break;

  case 321:
#line 1268 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_stmt) =	NewReturnStmt(cg, cg->tokenLoc, cg->current_scope, NULL); }
#line 3813 "parser.cpp" /* yacc.c:1646  */
    break;

  case 327:
#line 1291 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewIConstNode(cg, ICONST_OP, (yyvsp[0].sc_int), TYPE_BASE_CINT); }
#line 3819 "parser.cpp" /* yacc.c:1646  */
    break;

  case 328:
#line 1293 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewIConstNode(cg, ICONST_OP, (yyvsp[0].sc_int), TYPE_BASE_CINT); }
#line 3825 "parser.cpp" /* yacc.c:1646  */
    break;

  case 329:
#line 1295 "parser.y" /* yacc.c:1646  */
    { int base = cg->GetFloatSuffixBase(' '); (yyval.sc_expr) = NewFConstNode(cg, FCONST_OP, (yyvsp[0].sc_fval), base); }
#line 3831 "parser.cpp" /* yacc.c:1646  */
    break;

  case 330:
#line 1297 "parser.y" /* yacc.c:1646  */
    { int base = cg->GetFloatSuffixBase('f'); (yyval.sc_expr) = NewFConstNode(cg, FCONST_OP, (yyvsp[0].sc_fval), base); }
#line 3837 "parser.cpp" /* yacc.c:1646  */
    break;

  case 331:
#line 1299 "parser.y" /* yacc.c:1646  */
    { int base = cg->GetFloatSuffixBase('h'); (yyval.sc_expr) = NewFConstNode(cg, FCONST_OP, (yyvsp[0].sc_fval), base); }
#line 3843 "parser.cpp" /* yacc.c:1646  */
    break;

  case 332:
#line 1301 "parser.y" /* yacc.c:1646  */
    {int base = cg->GetFloatSuffixBase('x'); (yyval.sc_expr) = NewFConstNode(cg, FCONST_OP, (yyvsp[0].sc_fval), base); }
#line 3849 "parser.cpp" /* yacc.c:1646  */
    break;

  case 333:
#line 1303 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_expr) =	NewIConstNode(cg, ICONST_OP, (yyvsp[0].sc_token), TYPE_BASE_STRING); }
#line 3855 "parser.cpp" /* yacc.c:1646  */
    break;

  case 334:
#line 1307 "parser.y" /* yacc.c:1646  */
    { (yyval.sc_int) = GetConstant(cg, (yyvsp[0].sc_expr), 0); }
#line 3861 "parser.cpp" /* yacc.c:1646  */
    break;


#line 3865 "parser.cpp" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (cg, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (cg, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, cg);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, cg);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (cg, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, cg);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, cg);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1310 "parser.y" /* yacc.c:1906  */


