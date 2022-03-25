$if 0
//-----------------------------------------------------------------------------
//	DEF.H
//-----------------------------------------------------------------------------
$endif
$output $(output_filename).h
#ifndef _YYDEF_H
#define _YYDEF_H

$(include_source)

#define YYPREFIX $(api.prefix)

typedef int YYINT;

#ifndef YYTOKENTYPE
# define YYTOKENTYPE
enum yytokentype {
	$$(token_name) = $$(token_value),
};
#endif

/* Parameters sent to yyparse. */
#ifndef YYPARSE_PARAMS
# define YYPARSE_PARAMS	$separator($(parse_params))
# define YYPARSE_ARGS	$separator($(parse_args))
#endif
# define YYPARSE_DECL() yyparse($(parse_params))


/* Parameters sent to yylex. */
#ifndef YYLEX_PARAMS
# define YYLEX_PARAMS	$separator($(lex_params))
# define YYLEX_ARGS		$separator($(lex_args))
#endif
/* Parameters sent to yyerror. */
#ifndef YYERROR_PARAMS
# define YYERROR_PARAMS	$separator($(parse_params),,", ")
# define YYERROR_ARGS	$separator($(parse_args),,", ")
#endif

#if YYDEBUG
extern int yydebug;
#endif
#if !defined(YYSTYPE) && !defined(YYSTYPE_IS_DECLARED)
$if $exists(api.value.type)
typedef $(api.value.type) YYSTYPE;
$elif $exists(union)
typedef union $(union_name) $(union) YYSTYPE;
$else
typedef int YYSTYPE;
$endif
#define YYSTYPE_IS_DECLARED 1
#endif

#endif // YYDEF_H
$if 0
//-----------------------------------------------------------------------------
//	TAB.H
//-----------------------------------------------------------------------------
$endif
$output $(output_filename).tab.h

#undef YYBACKTRACKING
#define YYBACKTRACKING $(api.backtrack)
#undef YYGLR
#define YYGLR $(api.glr)
#ifndef YYDEBUG
#define YYDEBUG $(api.debug)
#endif
#define YYFINAL $(final_state)

// YYLHS[RULE]: Symbol that rule derives
static const $ctype($(rule_lhs_table)) yylhs[] = {
$(rule_lhs_table)
};
// YYLEN[RULE]: Number of symbols on the right side of rule
static const $ctype($(rule_len_table)) yylen[] = {
$(rule_len_table)
};
// YYDEFRED[STATE]: Default reduction number in state
static const $ctype($(default_reduce_table)) yydefred[] = {
$(default_reduce_table)
};
$if $(api.token.destructor)
// YYSTOS[STATE] -- The (internal number of the) accessing symbol of state
static const $ctype($(state_symbol_table)) yystos[] = {
$(state_symbol_table)
};
$endif
// YYDGOTO[STATE]: Default goto number in state
static const $ctype($(default_goto_table)) yydgoto[] = {
$(default_goto_table)
};
// YYSINDEX[STATE]: offset into YYTABLE for shifts
static const $ctype($(shift_offset_table)) yysindex[] = {
$(shift_offset_table)
};
// YYRINDEX[STATE]: offset into YYTABLE for reductions
static const $ctype($(reduce_offset_table)) yyrindex[] = {
$(reduce_offset_table)
};
// YYGINDEX[SYMBOL]: offset into YYTABLE for goto
static const $ctype($(goto_offset_table)) yygindex[] = {
$(goto_offset_table)
};

$if $exists(conflict_table) && ($(api.backtrack) || $(api.glr))
// YYCINDEX[STATE]: offset into YYTABLE for indices to YYCTABLE
static const $ctype($(conflict_offset_table)) yycindex[] = {
$(conflict_offset_table)
};
static const $ctype($(conflict_table)) yyctable[] = {
$(conflict_table)
};
$endif

// YYTABLE[]: rule to reduce by, or state to shift to
static const $ctype($(packed_table)) yytable[] = {
$(packed_table)
};
static const $ctype($(packed_check_table)) yycheck[] = {
$(packed_check_table)
};

#if YYDEBUG
static const char *const yyname[] = {
	$(symbol_name_table)
};
// YYRHSOFFSET[RULE]: offset in YYRLINE where rhs symbols of rule start
static const $ctype($(rule_rhs_offset_table)) yyrhsoffset[] = {
	$(rule_rhs_offset_table)
};
// YYRHS[YYRHSOFFSET[RULE]...]: rhs symbols of rule (count in YYLEN)
static const $ctype($(rule_rhs_table)) yyrhs[] = {
	$(rule_rhs_table)
};
// YYLINE[RULE]: source line where rule was defined
static const $ctype($(rule_line_table)) yyline[] = {
	$(rule_line_table)
};

#define YYFILENAME "$(input_filename)"

#endif

$if $(api.token.destructor)
void yydestruct(int sym, YYSTYPE *val $separator($(parse_params))) {
	switch (sym) {
$if $exists($$(symbol_destructor))	case $$(symbol_index): $$(symbol_destructor); break;
	}
}
#define YYDESTRUCT
#define YYDESTRUCT_CALL(msg, state, val) yydestruct(yystos[state], val $separator($(parse_args)))
$else
#define YYDESTRUCT_CALL(msg, state, val)
$endif

$(include_footer)

$if 0
//-----------------------------------------------------------------------------
//	ACT.H
//-----------------------------------------------------------------------------
$endif
$output $(output_filename).act.h
$if $exists($$(rule_action)) case $$(rule_index): $$(rule_action); break;
