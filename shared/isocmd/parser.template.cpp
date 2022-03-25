$if 0
//-----------------------------------------------------------------------------
//	DEF.H
//-----------------------------------------------------------------------------
$endif
#ifndef _YYDEF_H
#define _YYDEF_H

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
$if $exists(union)
union YYSTYPE $(union);
$else
typedef int YYSTYPE;
$endif
#define YYSTYPE_IS_DECLARED 1
#endif

#endif // YYDEF_H
$if 0
//-----------------------------------------------------------------------------
//	.CPP
//-----------------------------------------------------------------------------
$endif
$output $(output_filename).cpp
$(include_source)

$if $exists($header)
#include "$(output_filename).h"
$else
enum yytokentype {
	$$(token_name) = $$(token_value),
};

#if YYDEBUG
extern int yydebug;
#endif

$if $exists(union)
typedef union $(union_name) $(union) YYSTYPE;
$else
typedef int YYSTYPE;
$endif

$endif

#include <stdlib.h>	// needed for malloc, etc
#include <string.h>	// needed for memset

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

#if YYDEBUG
int	yydebug = 1;
extern "C" void __stdcall OutputDebugStringA(const char *lpOutputString);

void debug_printf(const char *fmt, ...) {
	va_list valist;
	va_start(valist, fmt);
	char	buffer[1024];
	vsprintf(buffer, fmt, valist);
	OutputDebugStringA(buffer);
}
static inline const char *debug_name(yytokentype t) {
	return yyname[t];
//	const char *yys = t <= YYMAXTOKEN ? yyname[t] : 0;
//	return yys ? yys : yyname[YYUNDFTOKEN];
}
#endif

#define YYERROR_CALL(msg)	yyerror(YYERROR_ARGS msg)

$if $(api.token.destructor)
#define YYDESTRUCT_CALL(msg, state, val) yydestruct(yystos[state], val $separator($(parse_args)))
$endif

$if $(api.pure)
#define YYLEX yylex(&yylval $separator($(lex_args)))
$else
YYSTYPE	yylval;
#define YYLEX yylex($(lex_params))
$endif

inline int yy_index(int n, int token) {
	return n && (n += token) > 0 && n < sizeof(yytable)/sizeof(yytable[0]) && yycheck[n] == token ? n : 0;
}

inline int yy_goto(int state, int lhs) {
	if (int n = yy_index(yygindex[lhs], state))
		return yytable[n];
	return yydgoto[lhs];
}

#define YYMAXDEPTH		100000
#define YYINITSTACKSIZE	200

#define yyclearin		(yychar = YYEMPTY)
#define yyerrok			(yyerrflag = 0)
#define YYRECOVERING()	(yyerrflag != 0)
#define YYEMPTY			(yytokentype)-1

struct YYSTACKDATA {
	struct ENTRY {
		YYINT	state;
		YYSTYPE	lval;
	};

	ENTRY	*base, *end, *top;

	YYSTACKDATA(unsigned size = YYINITSTACKSIZE) {
		top	= base = size ? (ENTRY*)malloc(size * sizeof(ENTRY)) : nullptr;
		end		= base + size;
	}
	~YYSTACKDATA() {
		free(base);
	}
	void reset() {
		free(base);
		base = top = end = 0;
	}
	void copy(const YYSTACKDATA &b) {
		auto	size = b.size();
		top = base + size - 1;
		memcpy(base, b.base, size * sizeof(ENTRY));
	}
	bool resize(size_t size) {
		if (size > YYMAXDEPTH)
			size = YYMAXDEPTH;
		intptr_t	i = top - base;
		ENTRY		*newbase = (ENTRY*)realloc(base, size * sizeof(ENTRY));
		if (newbase) {
			base	= newbase;
			top	= newbase + i;
			end		= newbase + size;
			return true;
		}
		return false;
	}
	bool push(YYINT state, YYSTYPE lval) {
		if (top >= end && !(end - base < YYMAXDEPTH && resize((end - base) * 2)))
			return false;
		++top;
		top->state	= state;
		top->lval	= lval;
		return true;
	}

	inline void pop(int n) {
		top	-= n;
	}
	inline unsigned size() const {
		return unsigned(top - base + 1);
	}
	inline YYINT top_state() const {
		return top->state;
	}
	inline bool empty() const {
		return top <= base;
	}
	inline YYSTYPE& getl(int i) {
		return top[i].lval;
	}
};

$if (!$(api.backtrack) && !$(api.glr)) || !$exists(conflict_table) 
#define YYABORT		goto yyabort
#define YYREJECT	goto yyabort
#define YYACCEPT	goto yyaccept
#define YYERROR		goto yyerrlab
#define YYVALID
#define YYVALID_NESTED

#if YYDEBUG
#define DEBUGPRINTF(fmt, ...)	if (yydebug) debug_printf(fmt "\n", __VA_ARGS__);
#else
#define DEBUGPRINTF(...)
#endif

int yyparse($(parse_params)) {
	static const bool yytrial	= false;
	
	//from lex
	yytokentype		token		= YYEMPTY;
$if $(api.pure)
	YYSTYPE			yylval;
$endif

	//stack
	YYSTACKDATA		yystack;
	int				yystate		= 0;
	int				yyerrflag	= 0;
	int				yyresult	= 0;
	int				yynerrs		= 0;

	memset(&yylval, 0, sizeof(yylval));
	yystack.push(0, yylval);

	for (;;) {
		int		yyn;
		while (yyresult == 0 && !(yyn = yydefred[yystate])) {
			if (token < 0) {
				token = (yytokentype)YYLEX;
				if (token < 0)
					token = YYEOF;
				
				DEBUGPRINTF("reading %d (%s)", token, debug_name(token));
				if (token == YYEOF && yystate == YYFINAL)
					return 0;	// accept
			}

			if (yyn = yy_index(yysindex[yystate], token)) {
				//shift
				DEBUGPRINTF("state %d, shifting to state %d", yystate, yytable[yyn]);
				if (!yystack.push(yystate = yytable[yyn], yylval))
					yyresult = 2;

				token		= YYEMPTY;
				if (yyerrflag)
					--yyerrflag;

			} else if (yyn = yy_index(yyrindex[yystate], token)) {
				yyn = yytable[yyn];
				break;

			} else {
				if (yyerrflag == 0) {
					YYERROR_CALL("syntax error");
					goto yyerrlab; // redundant goto avoids 'unused label' warning
yyerrlab:			++yynerrs;
				}

				if (yyerrflag < 3) {
					yyerrflag = 3;
					//error recovery
					while (!(yyn = yy_index(yysindex[yystate], YYERRCODE))) {
						DEBUGPRINTF("error recovery discarding state %d", yystate);
						if (yystack.empty()) {
							yyresult = 1;	//abort
							break;
						}
$if $(api.token.destructor)
						YYDESTRUCT_CALL("error: discarding state", yystos[yystack.top_state()], yystack.getl(0));
$endif
						yystack.pop(1);
						yystate = yystack.top_state();
					}
					DEBUGPRINTF("state %d, error recovery shifting to state %d", yystate, yytable[yyn]);
					if (!yystack.push(yystate = yytable[yyn], yylval))
						yyresult = 2;

				} else if (token == YYEOF) {
					yyresult = 1;	//abort

				} else {
					DEBUGPRINTF("state %d, error recovery discards token %d (%s)", yystate, token, debug_name(token));
					token = YYEMPTY;
				}
			}
		}
		if (yyresult)
			break;

		//reduce
		DEBUGPRINTF("%s(%d) state %d, reducing by rule %d", YYFILENAME, yyline[yyn], yystate, yyn);
		int			yym = yylen[yyn];
		YYSTYPE		yyval;
		if (yym)
			yyval = yystack.getl(1 - yym);
		else
			memset(&yyval, 0, sizeof yyval);

		switch (yyn) {
$if $exists($$(rule_action)) case $$(rule_index): $$(rule_action); break;
		}

		yystack.pop(yym);
		yystate	= yy_goto(yystack.top_state(), yylhs[yyn]);
		DEBUGPRINTF("after reduction, shifting from state %d to state %d", yystack.top_state(), yystate);
		if (!yystack.push(yystate, yyval))
			yyresult = 2;
#if 0
		if (yystate == YYFINAL) {
			if (token < 0) {
				token = (yytokentype)YYLEX;
				if (token < 0)
					token = YYEOF;
				DEBUGPRINTF("reading %d (%s)", token, debug_name(token));
			}
			if (token == YYEOF)
				break;	// accept
		}
#endif
	}

	if (yyresult == 2)
		YYERROR_CALL("yacc stack overflow");

$if $(api.token.destructor)
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (auto *i = yystack.base; i != yystack.top; i++)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[i->state], &i->lval);
$endif

	return yyresult;
}

$elif $(api.backtrack)

struct YYPARSESTATE {
	struct YYPARSESTATE *save;		// Previously saved parser state
	YYSTACKDATA			stack;		// saved parser stack
	int					state;		// saved parser state
	int					errflag;	// saved error recovery status
	int					lexeme;		// saved index of the conflict lexeme in the lexical queue
	YYINT				ctry;		// saved index in yyctable[] for this conflict

	YYPARSESTATE() : lexeme(0) {}
	YYPARSESTATE(YYPARSESTATE *save, const YYSTACKDATA &stack, int state, int errflag, int lexeme, int ctry)
		: save(save), stack(stack.size()), state(state), errflag(errflag), lexeme(lexeme), ctry(ctry)
	{
		this->stack.copy(stack);
	}
	void set(const YYSTACKDATA &_stack, int _state, int _errflag, int _lexeme) {
		stack.resize(_stack.size());
		stack.copy(_stack);
		state	= _state;
		errflag	= _errflag;
		lexeme	= _lexeme;
	}
	void reset() {
		lexeme = 0;
		stack.reset();
	}
};

#define YYLVQUEUEGROWTH 32

struct YYTOKENQUEUE {
	struct ENTRY {
		yytokentype	tok;
		YYSTYPE		lval;
	};

	ENTRY	*base, *pget, *pput, *limit;

	YYTOKENQUEUE() : base(0), pget(0), pput(0), limit(0) {}

	bool can_get() const {
		return pget < pput;
	}
	yytokentype	get(YYSTYPE &lval) {
		lval = pget->lval;
		return pget++->tok;
	}
	int size() const {
		return int(pput - base);
	}
	void reset() {
		pput	= base;
	}
	void start_playback(int i) {
		pget	= base + i;
	}
	bool put(yytokentype tok, YYSTYPE lval) {
		if (pput == limit) {
			// Enlarge lexical value queue
			size_t	i		= pput - base;
			size_t	size	= i + YYLVQUEUEGROWTH;

			ENTRY	*newbase = (ENTRY*)realloc(base, size * sizeof(ENTRY));
			if (!newbase)
				return false;

			base	= newbase;
			limit	= newbase + size;
			pput	= newbase + i;
		}
		pput->lval	= lval;
		pput++->tok	= tok;
		pget		= pput;
		return true;
	}
};

#define YYABORT			yyresult = 1;
#define YYREJECT		YYABORT
#define YYACCEPT		goto yyaccept
#define YYERROR			goto yyerrlab
#define YYVALID			do if (yytrial) goto yyvalid; while(0)
#define YYVALID_NESTED	do if (yytrial && !yytrial->save) goto yyvalid; while(0)

// YYVALID_NESTED: When we have a nested conflict (conflict while processing in trial mode for another conflict), we want to relate YYVALID to a particular level of conflict being in trial.
// in 2-level conflict it is a no-op; a YYVALID for the outer conflict will be searched for

#if YYDEBUG
#define DEBUGPRINTF(fmt, ...)	if (yydebug) debug_printf("%s" fmt "\n", yytrial ? "(trial)" : yytokenq.can_get() ? "(playback)" : "", __VA_ARGS__);
#else
#define DEBUGPRINTF(...)
#endif

int yyparse($(parse_params)) {
	//from lex
	yytokentype		token		= YYEMPTY;
$if $(api.pure)
	YYSTYPE			yylval;
$endif

	//stack
	YYSTACKDATA		yystack;
	int				yystate		= 0;
	int				yyerrflag	= 0;
	int				yyresult	= 0;
	int				yynerrs		= 0;

	memset(&yylval, 0, sizeof(yylval));
	yystack.push(0, yylval);

	YYPARSESTATE	yyerrctx;
	YYPARSESTATE	*yypath		= 0;
	YYPARSESTATE	*yytrial	= 0;

	YYTOKENQUEUE	yytokenq;

	for (;;) {
		int			yyn;
		while (yyresult == 0 && !(yyn = yydefred[yystate])) {
			if (token < 0) {
				if (yytokenq.can_get()) {
					token = yytokenq.get(yylval);
				} else {
					token = (yytokentype)YYLEX;
					if (yytrial && !yytokenq.put(token, yylval))
						yyresult = 3;
				}
				if (token < 0)
					token = YYEOF;
				DEBUGPRINTF("reading %d (%s)", token, debug_name(token));
			}

			// Do we have a conflict?
			if (yyn = yy_index(yycindex[yystate], token)) {
				int		ctry;

				if (yypath) {
					// Switch to the next conflict context
					DEBUGPRINTF("CONFLICT in state %d: following successful trial parse", yystate);

					YYPARSESTATE *save = yypath;
					yypath		= save->save;
					save->save	= 0;
					ctry		= save->ctry;
					yyresult	= save->state != yystate;
					delete save;

				} else {
					// Unresolved conflict - start/continue trial parse
					DEBUGPRINTF("CONFLICT in state %d - %s", yystate, yytrial ? "ALREADY in conflict, continuing trial parse" : "starting trial parse");

					ctry	= yytable[yyn];
					if (yyctable[ctry] == -1) {
						// no shift
						DEBUGPRINTF("backtracking 1 token");
						ctry++;
					}
				
					if (!yytrial) {
						// If this is a first conflict in the stack, start saving lexemes
						if (!yytokenq.can_get()) {
							yytokenq.reset();
							if (!yytokenq.put(token, yylval))
								yyresult = 3;
						}
					}
					--yytokenq.pput;

					yytrial = new YYPARSESTATE(yytrial, yystack, yystate, yyerrflag, yytokenq.size(), ctry);
					if (!yytrial)
						yyresult = 3;
				
					token	= YYEMPTY;
				}

				if (yytable[yyn] == ctry) {
					DEBUGPRINTF("state %d, shifting to state %d", yystate, yyctable[ctry]);
					if (token < 0)
						++yytokenq.pput;

					yystate		= yyctable[ctry];
					token		= YYEMPTY;
					if (!yystack.push(yystate, yylval))
						yyresult = 2;

					if (yyerrflag > 0)
						--yyerrflag;
				} else {
					yyn = yyctable[ctry];
					break;
				}
				// End of code dealing with conflicts

			} else if (yyn = yy_index(yysindex[yystate], token)) {
				DEBUGPRINTF("state %d, shifting to state %d", yystate, yytable[yyn]);
				if (!yystack.push(yystate = yytable[yyn], yylval))
					yyresult = 2;

				token		= YYEMPTY;
				if (yyerrflag > 0)
					--yyerrflag;

			} else if (yyn = yy_index(yyrindex[yystate], token)) {
				yyn = yytable[yyn];
				break;

			} else {
				if (yyerrflag == 0) {
					while (yytrial) {
						DEBUGPRINTF("ERROR in state %d, CONFLICT BACKTRACKING to state %d, %d tokens", yystate, yytrial->state, (int)(yytokenq.size() - yytrial->lexeme));

						// Memorize most forward-looking error state in case it's really an error
						if (yyerrctx.lexeme < yytokenq.size())
							yyerrctx.set(yystack, yystate, yyerrflag, yytokenq.size());

						yytokenq.start_playback(yytrial->lexeme);
						yystack.copy(yytrial->stack);
						yystate		= yytrial->state;
						token		= YYEMPTY;

						int	ctry	= ++yytrial->ctry;
						// We tried shift, try reduce now
						if ((yyn = yyctable[ctry]) >= 0)
							break;

						YYPARSESTATE *save = yytrial;
						yytrial		= yytrial->save;
						delete save;

						// Nothing left on the stack -- error
						if (!yytrial) {
							DEBUGPRINTF("trial parse FAILED, entering ERROR mode");
							// Restore state as it was in the most forward-advanced error
							yytokenq.start_playback(yyerrctx.lexeme - 1);
							yystack.copy(yyerrctx.stack);
							yystate		= yyerrctx.state;
							token		= yytokenq.get(yylval);
							yyerrctx.reset();
						}
					}
					if (yyn >= 0)
						break;

					YYERROR_CALL("syntax error");
					goto yyerrlab; // redundant goto avoids 'unused label' warning
yyerrlab:			++yynerrs;
				}

				if (yyerrflag < 3) {
					yyerrflag = 3;
					while (!(yyn = yy_index(yysindex[yystack.top_state()], YYERRCODE))) {
						DEBUGPRINTF("error recovery discarding state %d", yystack.top_state());
						if (yystack.empty()) {
							yyresult = 1;	//abort
							break;
						}
$if $(api.token.destructor)
						if (!yytrial)
							YYDESTRUCT_CALL("error: discarding state", yystos[yystack.top_state()], &yystack.getl(0));
$endif
						yystack.pop(1);
					}
					DEBUGPRINTF("state %d, error recovery shifting to state %d", yystack.top_state(), yytable[yyn]);
					if (!yystack.push(yystate = yytable[yyn], yylval))
						yyresult = 2;

				} else if (token == YYEOF) {
					yyresult = 1;

				} else {
					DEBUGPRINTF("state %d, error recovery discarding token %d (%s)", yystate, token, debug_name(token));
$if $(api.token.destructor)
					if (!yytrial)
						YYDESTRUCT_CALL("error: discarding token", token, &yylval);
$endif
					token = YYEMPTY;
				}
			}
		}

		if (yyresult)
			break;

		//reduce
		DEBUGPRINTF("%s(%d) state %d, reducing by rule %d", YYFILENAME, yyline[yyn], yystate, yyn);

		int		yym = yylen[yyn];
		YYSTYPE	yyval;
		if (yym > 0)
			yyval = yystack.getl(1 - yym);
		else
			memset(&yyval, 0, sizeof yyval);

		switch (yyn) {
$if $exists($$(rule_action)) case $$(rule_index): $$(rule_action); break;
		}

		if (yyresult)
			break;

		yystack.pop(yym);
		yystate	= yystack.top_state();
		
		yystate = yy_goto(yystate, yylhs[yyn]);
		DEBUGPRINTF("after reduction, shifting from state %d to state %d", yystack.top_state(), yystate);

		if (!yystack.push(yystate, yyval))
			yyresult = 2;
				
		if (yystate == YYFINAL) {
			if (token < 0) {
				if (yytokenq.can_get()) {
					token = yytokenq.get(yylval);
				} else {
					token = (yytokentype)YYLEX;
					if (yytrial && !yytokenq.put(token, yylval))
						yyresult = 3;
				}
				if (token < 0)
					token = YYEOF;
				DEBUGPRINTF("reading %d (%s)", token, debug_name(token));
			}
			if (token == YYEOF) {
				if (!yytrial)
					break;	//accept

				// Reduction declares that this path is valid. Set yypath and do a full parse
			yyvalid:
				if (!(yyresult = !!yypath)) {	//abort if yypath already set
					while (auto *save = yytrial) {
						yytrial		= save->save;
						save->save	= yypath;
						yypath		= save;
					}

					DEBUGPRINTF("state %d, CONFLICT trial successful, backtracking to state %d, %d tokens", yystate, yypath->state, yytokenq.size() - yypath->lexeme);
					yyerrctx.reset();
					yytokenq.start_playback(yypath->lexeme);
					yystack.copy(yypath->stack);
					yystate	= yypath->state;
					token	= YYEMPTY;
				}
			}
		}
	}

	switch (yyresult) {
		case 2:
			YYERROR_CALL("yacc stack overflow");
			break;
		case 3:
			YYERROR_CALL("memory exhausted");
			yyresult = 2;
			break;
	}

$if $(api.token.destructor)
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (auto *i = yystack.base; i != yystack.top; i++)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[i->state], &i->lval);
$endif

	while (auto *p = yytrial) {
		yytrial = p->save;
		delete p;
	}
	while (auto *p = yypath) {
		yypath = p->save;
		delete p;
	}
	return yyresult;
}

$elif $(api.glr)

//-----------------------------------------------------------------------------
// GLR
//-----------------------------------------------------------------------------

#if YYDEBUG
#define DEBUGPRINTF(fmt, ...)	if (yydebug) debug_printf(fmt "\n", __VA_ARGS__);
#else
#define DEBUGPRINTF(...)
#endif

template<typename T> inline void swap(T &a, T &b) {
	T t(a);
	a = b;
	b = t;
}

template<typename T, int N> class priority_queue {
	T	heap[N];
	int	n;

	void heap_siftdown(T *begin, T *end, T *root) {
		for (;;) {
			T* child = root + (root - begin + 1);
			if (child >= end)
				break;
			if (child + 1 < end && child[1] < child[0])
				++child;
			if (!(*child < *root))
				break;
			swap(*root, *child);
			root = child;
		}
	}
	void heap_siftup(T* begin, T* end, T* child) {
		while (child > begin) {
			T* parent = begin + (child - begin - 1) / 2;
			if (*parent < *child)
				break;
			swap(*parent, *child);
			child = parent;
		}
	}
public:
	priority_queue() : n(0) {}

	bool empty() const {
		return n != 0;
	}
	const T& top() const {
		return heap[0];
	}
	void pop() {
		--n;
		swap(heap[0], heap[n]);
		heap_siftdown(heap, heap + n, heap);
	}
	T pop_value() {
		pop();
		return heap[n];
	}
	void push(const T& t) {
		heap[n++] = t;
		heap_siftup(heap, heap + n, heap + n - 1);
	}
};


struct YYSTACKGLR {
	struct ENTRY {
		int		next;
		YYINT	state;
		YYSTYPE	lval;
		const ENTRY	*get_next() const { return this - next; }
	};
	struct STACK {
		int		index;
		int		top;
		int		next;			// next concurrent stack
		int		linear_depth;

		inline void *operator new(size_t, ENTRY *e)	{ return e; }
		inline void operator delete(void*,ENTRY *e)	{}

		STACK() : index(0), top(0), next(0), linear_depth(0) {}

		STACK	*get_next() const {
			return next ? (STACK*)((ENTRY*)this + next) : 0;
		}
		void	set_next(STACK *s) {
			next = s ? int((ENTRY*)s - (ENTRY*)this) : 0;
		}
		ENTRY	*get_top() const {
			return top ? (ENTRY*)this + top : 0;
		}
		void	set_top(ENTRY *p) {
			top = p ? int(p - (ENTRY*)this) : 0;
		}
		bool	empty() const {
			return top == 0;
		}
		inline void pop(int n) {
			if (n <= linear_depth) {
				linear_depth -= n;
				top	-= n;
			} else {
				ENTRY	*p = get_top() - linear_depth;
				for (n -= linear_depth; n; --n)
					p -= p->next;
				set_top(p);
				linear_depth = 0;
			}
		}
		void push(ENTRY *e, YYINT state, YYSTYPE lval) {
			ENTRY	*p	= get_top();
			e->next		= p ? int(e - p) : 0;
			e->state	= state;
			e->lval		= lval;
			set_top(e);
			if (e->next != 1)
				linear_depth = 0;
			else
				++linear_depth;
		}
		inline YYINT top_state() const {
			return get_top()->state;
		}
		inline ENTRY* get_entry(int i) {
			ENTRY	*p = get_top();
			if (-i <= linear_depth)
				return p + i;

			p -= linear_depth;
			for (i += linear_depth; i; i++)
				p -= p->next;
			return p;
		}
		inline YYSTYPE& getl(int i) {
			return get_entry(i)->lval;
		}

	};

	ENTRY	*base, *end, *free;
	int		next_stack;

	YYSTACKGLR(unsigned size = YYINITSTACKSIZE) : next_stack(0) {
		free	= base = size ? (ENTRY*)malloc(size * sizeof(ENTRY)) : nullptr;
		end		= base + size;
		new(alloc()) STACK;	//stack0
	}
	~YYSTACKGLR() {
		::free(base);
	}
	bool resize(size_t size) {
		if (size > YYMAXDEPTH)
			size = YYMAXDEPTH;
		intptr_t	i = free - base;
		ENTRY		*newbase = (ENTRY*)realloc(base, size * sizeof(ENTRY));
		if (newbase) {
			base	= newbase;
			free	= newbase + i;
			end		= newbase + size;
			return true;
		}
		return false;
	}
	bool	reserve(int n) {
		return free + n < end || (end - base < YYMAXDEPTH && resize((end - base) * 2));
	}
	ENTRY	*alloc() {
		return free < end ? free++ : 0;
	}
	STACK	*stack0() {
		return (STACK*)base;
	}
	bool push(STACK *s, YYINT state, YYSTYPE lval) {
		if (auto *e = alloc()) {
			s->push(e, state, lval);
			return true;
		}
		return false;
	}
	STACK* split(STACK *s) {
		STACK	*s1 = s->get_next();
		STACK	*s2 = new(alloc()) STACK;
		if (s2) {
			s2->index = ++next_stack;
			s2->set_top(s->get_top());
			s2->set_next(s1);
			s->set_next(s2);
		}
		return s2;
	}

	STACK* find_merge(STACK *s0, int state) {
		for (STACK *s = stack0(); s; s = s->get_next()) {
			if (s != s0) {
				if (ENTRY *top = s->get_top()) {
					if (top->state == state)
						return s;
				}
			}
		}
		return 0;
	}

	void compact(STACK *s) {
		ENTRY	*e = s->get_top();
		if (e + 1 != free)
			return;

		int		n	= 0;
		ENTRY	*d	= e;
		while (d->next) {
			++n;
			d -= d->next;
		}
		d		+= n;
		free	 = d + 1;

		s->set_top(d);
		s->linear_depth = n;

		while (e->next && e != d) {
			d->next		= 1;
			d->state	= e->state;
			d->lval		= e->lval;
			e	-= e->next;
			--d;
		}
	}

	STACK *kill(STACK *s, STACK *prev) {
		STACK	*next = s->get_next();

		if (prev) {
			prev->set_next(next);
			return next;
		}

		s->index		= next->index;
		s->linear_depth	= next->linear_depth;
		s->set_top(next->get_top());
		s->set_next(next->get_next());
		next->top	= 0;
		return s;
	}
};


struct YYREDUCTION {
	YYSTACKGLR::STACK	*s;
	int		n;
	int		len;
	YYREDUCTION() {}
	YYREDUCTION(YYSTACKGLR::STACK *s, int n) : s(s), n(n), len(yylen[n]) {}
	bool operator<(const YYREDUCTION &b) const { return len < b.len; }
};

enum YYRESULT {
	YY_ACCEPT	= -1,
	YY_ERROR	= -2,
	YY_ABORT	= -3,
	YY_OOM		= -4,
};
#define YYABORT		return YY_ABORT
#define YYREJECT	return YY_ABORT
#define YYACCEPT	return YY_ACCEPT
#define YYERROR		return YY_ERROR
#define YYVALID
#define YYVALID_NESTED

int yyreduce(YYSTACKGLR::STACK &yystack, int rule, int len, yytokentype &yychar, int &yyerrflag, YYSTYPE &yyval $separator($(parse_params))) {
	if (len)
		yyval = yystack.get_entry(1 - len)->lval;
	else
		memset(&yyval, 0, sizeof yyval);

	switch (rule) {
$if $exists($$(rule_action)) case $$(rule_index): $$(rule_action); break;
	}

	yystack.pop(len);
	return yy_goto(yystack.top_state(), yylhs[rule]);
}

void yyconflicts(YYSTACKGLR &yystack, priority_queue<YYREDUCTION, 32> &red_queue, YYSTACKGLR::STACK *s, yytokentype token, YYSTYPE &yylval) {
	// Only the first action can be a shift (stored as usual); other actions must be reductions
	int		state		= s->top_state();
	int		shift		= yy_index(yysindex[state], token);
	auto	conflicts	= yyctable + yytable[yy_index(yycindex[state], token)];

	int		n;
	while ((n = *conflicts++) >= 0) {
		if (!shift && *conflicts < 0) {
			// if last conflict, reuse stack s
			red_queue.push(YYREDUCTION(s, n));
			DEBUGPRINTF("S%i: state %d, queuing reduction by rule %d", s->index, state, n);
			return;
		}

		auto	*s2 = yystack.split(s);
		DEBUGPRINTF("S%i: splitting off stack %i and queuing reduction by rule %d", s->index, s2->index, n);
		red_queue.push(YYREDUCTION(s2, n));
	}
}

int yyparse($(parse_params)) {
	typedef YYSTACKGLR::STACK STACK;
	
	YYSTACKGLR		yystack;
	priority_queue<YYREDUCTION, 32>	red_queue;

$if $(api.pure)
	YYSTYPE			yylval;
$endif

	yytokentype		token;
	int				yyresult	= 0;
	int				yyerrflag	= 0;
	int				yynerrs		= 0;

	memset(&yylval, 0, sizeof(yylval));
	yystack.push(yystack.stack0(), 0, yylval);


	while (!yyresult) {

		//-------------------------
		// deterministic loop
		//-------------------------

		while (!(yyresult = yystack.reserve(64) ? 0 : YY_OOM)) {
			//next token
			token = (yytokentype)YYLEX;
			if (token < 0)
				token = YYEOF;
		
			DEBUGPRINTF("reading %d (%s)", token, debug_name(token));

			STACK	*s		= yystack.stack0();
			int		state	= s->top_state();
			int		n, c;

			// reductions
			while ((n = yydefred[state]) ||
				!(c = yy_index(yycindex[state], token)) &&
				!(n = yy_index(yysindex[state], token))
			) {
				if (!n && (n = yy_index(yyrindex[state], token)))
					n = yytable[n];

				if (n) {
					DEBUGPRINTF("%s(%i): state %d, reducing by rule %d", YYFILENAME, yyline[n], state, n);
					int	len = yylen[n];
					if (yystack.free == s->get_top() + 1 && len <= s->linear_depth)
						yystack.free -= len;

					YYSTYPE yyval;
					yyresult = yyreduce(*s, n, len, token, yyerrflag, yyval $separator($(parse_args)));

					if (yyresult >= 0) {
						state		= yyresult;
						yyresult	= 0;
						DEBUGPRINTF("after reduction, shifting from state %d to state %d", s->top_state(), state);
						yystack.push(s, state, yyval);
					}

				} else if (token == YYEOF && state == YYFINAL) {
					yyresult = YY_ACCEPT;
					break;

				} else {
					yyresult = YY_ERROR;
				}

				if (yyresult == YY_ERROR) {
					yyresult = 0;

					if (yyerrflag == 0) {
						YYERROR_CALL("syntax error");
						++yynerrs;
					}

					if (yyerrflag == 3) {
						yyerrflag = 3;
						//error recovery
						while (!(n = yy_index(yysindex[state], YYERRCODE))) {
							DEBUGPRINTF("error recovery discarding state %d", state);
							if (s->empty()) {
								yyresult = YY_ABORT;
								break;
							}
							s->pop(1);
							state = s->top_state();
						}
						DEBUGPRINTF("state %d, error recovery shifting to state %d", state, yytable[n]);
						yystack.push(s, state = yytable[n], yylval);
					}
				}
			}

			if (yyresult)
				break;

			if (c) {
				DEBUGPRINTF("CONFLICT in state %d", state);
				yyconflicts(yystack, red_queue, s, token, yylval);
				break;
			}

			n = yy_index(yysindex[state], token);

			//shift
			DEBUGPRINTF("state %d, shifting to state %d", state, yytable[n]);
			yystack.push(s, state = yytable[n], yylval);

			if (yyerrflag)
				--yyerrflag;
		}

		//-------------------------
		// non-deterministic loop
		//-------------------------

		while (!yyresult) {

			//	perform queued reductions
			while (red_queue.empty()) {
				YYSTYPE		yyval;
				auto		&r		= red_queue.pop_value();
				auto		s		= r.s;
				
				DEBUGPRINTF("S%i: state %d, reducing by rule %d ", s->index, s->top_state(), r.n);
				int			state	= yyreduce(*s, r.n, r.len, token, yyerrflag, yyval $separator($(parse_args)));

				if (state == YY_ERROR) {
					DEBUGPRINTF("S%i: ERROR in state %d, destroying", s->index, state);
					s->top = 0;	// error, so kill this stack
					continue;
				}

				if (state < 0) {
					yyresult = state;
					break;
				}

				if (STACK *s2 = yystack.find_merge(s, state)) {
					//merge
					s->top = 0;

				} else {
					yystack.push(s, state, yyval);
					if (int n = yydefred[state]) {
						//default reduction
						DEBUGPRINTF("S%i: state %d, queuing reduction by rule %d ", s->index, state, n);
						red_queue.push(YYREDUCTION(s, n));

					} else if (int n = yy_index(yycindex[state], token)) {
						//conflict
						yyconflicts(yystack, red_queue, s, token, yylval);

					} else if (int n = yy_index(yysindex[state], token) ? 0 : yy_index(yyrindex[state], token)) {
						//reduction
						n = yytable[n];
						DEBUGPRINTF("S%i: state %d, queuing reduction by rule %d", s->index, state, n);
						red_queue.push(YYREDUCTION(s, n));
					}
				}
			}

			if (!yystack.reserve(64)) {
				yyresult = YY_OOM;
				break;
			}

			//	perform any shifts and kill any dead stacks
			for (STACK *s = yystack.stack0(), *prev = 0, *next; s; s = next) {
				if (s->top) {
					int	state = s->top_state();
					if (int n = yy_index(yysindex[state], token)) {
						DEBUGPRINTF("S%i: state %d, shifting to state %d", s->index, state, yytable[n]);
						state = yytable[n];
						if (STACK *s2 = yystack.find_merge(s, state)) {
							//merge
							s->top = 0;
						} else {
							yystack.push(s, state, yylval);
						}
					}
					next = s->get_next();
					prev = s;

				} else {
					next = yystack.kill(s, prev);
				}
			}

			if (!yystack.stack0()->next) {
				DEBUGPRINTF("RETURN to deterministic path");
				yystack.compact(yystack.stack0());
				break;
			}

			//next token
			token = (yytokentype)YYLEX;
			if (token < 0)
				token = YYEOF;

			DEBUGPRINTF("reading %d (%s)", token, debug_name(token));

			if (!yystack.reserve(64)) {
				yyresult = YY_OOM;
				break;
			}

			// collect reductions
			for (STACK *s = yystack.stack0(); s; s = s->get_next()) {
				int	state	= s->top_state();

				if (int n = yydefred[state]) {
					//	default reduction
					DEBUGPRINTF("S%i: state %d, queuing reduction by rule %d ", s->index, state, n);
					red_queue.push(YYREDUCTION(s, n));

				} else if (yy_index(yycindex[state], token)) {
					//	conflicts
					yyconflicts(yystack, red_queue, s, token, yylval);

				} else if (int n = yy_index(yysindex[state], token)) {
					//	stop processing on shifts
					continue;

				} else if (int n = yy_index(yyrindex[state], token)) {
					//	reductions
					n = yytable[n];
					DEBUGPRINTF("S%i: state %d, queuing reduction by rule %d", s->index, state, n);
					red_queue.push(YYREDUCTION(s, n));

				} else if (token == YYEOF && state == YYFINAL) {
					yyresult = YY_ACCEPT;
					break;

				} else {
					DEBUGPRINTF("S%i: ERROR in state %d, destroying", s->index, state);
					s->top = 0;	// error, so kill this stack
				}
			}

		}
	}

	switch (yyresult) {
		case YY_OOM:
			YYERROR_CALL("yacc stack overflow");
			yyresult = 2;
			break;
		case YY_ABORT:
			yyresult = 1;
			break;
		default:
			yyresult = 0;
			break;
	}

$if $(api.token.destructor)
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (auto *i = yystack.base; i != yystack.top; i++)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[i->state], &i->lval);
$endif

	return yyresult;
}

$endif

$if $(api.token.destructor)
void yydestruct(int sym, YYSTYPE *val $separator($(parse_params))) {
	switch (sym) {
$if $exists($$(symbol_destructor))	case $$(symbol_index): $$(symbol_destructor); break;
	}
}
$endif

$(include_footer)
