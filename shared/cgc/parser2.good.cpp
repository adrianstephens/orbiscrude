#include <stdio.h>
#include <stdlib.h>
#include "cg.h"
#include "compile.h"
#include "printutils.h"
#include "errors.h"

#define YYDEBUG 1

using namespace cgc;

#include "D:\dev\orbiscrude\shared\cgc\parser.def.h"
#include "D:\dev\orbiscrude\shared\cgc\parser.tab.h"

namespace cgc {
void yyerror(CG *cg, const char *s);
int yylex(YYSTYPE *lvalp, CG *cg);
}

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

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
	const char *yys = t <= YYMAXTOKEN ? yyname[t] : 0;
	return yys ? yys : yyname[YYUNDFTOKEN];
}
#endif

#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#define YYINITSTACKSIZE 200

#define STACK_STRUCT
#ifndef STACK_STRUCT

struct YYSTACKDATA {
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_end;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;

	YYSTACKDATA() : s_base(0), s_mark(0), s_end(0), l_base(0) {}

	~YYSTACKDATA() {
		free(s_base);
		free(l_base);
	}

	/* allocate initial stack or double stack size, up to YYMAXDEPTH */
	bool grow() {
		intptr_t newsize = s_end - s_base;
		if (newsize < YYMAXDEPTH) {
			newsize = newsize ? newsize * 2 : YYINITSTACKSIZE;
			if (newsize > YYMAXDEPTH)
				newsize = YYMAXDEPTH;

			intptr_t	i = s_mark - s_base;
			YYINT		*newss;
			YYSTYPE		*newvs;
			if (
				(newss = (YYINT*)realloc(s_base, newsize * sizeof(*newss)))
			&&	(newvs = (YYSTYPE*)realloc(l_base, newsize * sizeof(*newvs)))
			) {
				s_base = newss;
				s_mark = newss + i;

				l_base = newvs;
				l_mark = newvs + i;

				s_end = newss + newsize;
				return true;
			}
		}
		return false;
	}

	bool push(YYINT s, YYSTYPE l) {
		if (s_mark >= s_end && !grow())
			return false;
		*++s_mark = s;
		*++l_mark = l;
		return true;
	}
	inline void pop(int n) {
		s_mark	-= n;
		l_mark	-= n;
	}
	inline YYINT top() const {
		return *s_mark;
	}
	inline bool empty() const {
		return s_mark <= s_base;
	}
	inline YYSTYPE& get(int i) {
		return  l_mark[i];
	}
};


#else


struct YYSTACKDATA {
	struct ENTRY {
		YYINT	state;
		YYSTYPE	lval;
	};

	ENTRY	*base, *mark, *end;

	YYSTACKDATA() : base(0), mark(0), end(0) {}

	YYSTACKDATA(unsigned size) {
		mark	= base = size ? (ENTRY*)malloc(size * sizeof(ENTRY)) : nullptr;
		end		= base + size;
	}

	~YYSTACKDATA() {
		free(base);
	}
	void	copy(const YYSTACKDATA &b) {
		auto	size = b.size();
		mark  = base + size - 1;
		memcpy(base, b.base, size * sizeof(ENTRY));
	}

	bool grow() {
		intptr_t newsize = end - base;
		if (newsize < YYMAXDEPTH) {
			newsize = newsize ? newsize * 2 : YYINITSTACKSIZE;
			if (newsize > YYMAXDEPTH)
				newsize = YYMAXDEPTH;

			intptr_t	i = mark - base;
			ENTRY		*newbase = (ENTRY*)realloc(base, newsize * sizeof(ENTRY));
			if (newbase) {
				base	= newbase;
				mark	= newbase + i;
				end		= newbase + newsize;
				return true;
			}
		}
		return false;
	}

	bool push(YYINT state, YYSTYPE lval) {
		if (mark >= end && !grow())
			return false;
		++mark;
		mark->state	= state;
		mark->lval	= lval;
		return true;
	}

	inline void pop(int n) {
		mark	-= n;
	}
	inline unsigned size() const {
		return unsigned(mark - base + 1);
	}
	inline YYINT top() const {
		return mark->state;
	}
	inline bool empty() const {
		return mark <= base;
	}
	inline YYSTYPE& getl(int i) {
		return  mark[i].lval;
	}
};

#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

#if !YYBACKTRACKING

#define YYVALID
#define YYVALID_NESTED

#if YYDEBUG
#define DEBUGPRINTF(fmt, ...)	if (yydebug) debug_printf(fmt "\n", __VA_ARGS__);
#else
#define DEBUGPRINTF(...)
#endif

int YYPARSE_DECL() {
	//from lex
	yytokentype		token		= YYEMPTY;
	YYSTYPE			yylval;

	//stack
	YYSTACKDATA		yystack;
	int				yyn			= 0;
	int				yystate		= 0;
	int				yyerrflag	= 0;
	bool			yyoverflow	= false;
	int				yynerrs		= 0;

	memset(&yylval, 0, sizeof(yylval));
	yystack.push(0, yylval);

	for (;;) {
		while (!yyoverflow && !(yyn = yydefred[yystate])) {
			if (token < 0) {
				token = (yytokentype)YYLEX;
				if (token < 0)
					token = YYEOF;
				DEBUGPRINTF("state %d, reading %d (%s)", yystate, token, debug_name(token));
			}

			if ((yyn = yysindex[yystate]) && (yyn += token) > 0 && yyn <= YYTABLESIZE && yycheck[yyn] == token) {
				//shift
				DEBUGPRINTF("state %d, shifting to state %d", yystate, yytable[yyn]);
				yyoverflow	= !yystack.push(yystate = yytable[yyn], yylval);
				token		= YYEMPTY;
				if (yyerrflag)
					--yyerrflag;

			} else if ((yyn = yyrindex[yystate]) && (yyn += token) > 0 && yyn <= YYTABLESIZE && yycheck[yyn] == token) {
				yyn = yytable[yyn];
				break;

			} else {
				if (yyerrflag == 0) {
					++yynerrs;
					YYERROR_CALL("syntax error");
				}

				if (yyerrflag < 3) {
					yyerrflag = 3;
					//error recovery
					while (!(yyn = yysindex[yystack.top()]) || (yyn += YYERRCODE) <= 0 || yyn > YYTABLESIZE || yycheck[yyn] != YYERRCODE) {
						DEBUGPRINTF("error recovery discarding state %d", yystack.top());
						if (yystack.empty())
							return 1;	//abort
						yystack.pop(1);
					}

					DEBUGPRINTF("state %d, error recovery shifting to state %d", yystack.top(), yytable[yyn]);
					yyoverflow = !yystack.push(yystate = yytable[yyn], yylval);

				} else {
					if (token == YYEOF)
						return 1;	//abort
					DEBUGPRINTF("state %d, error recovery discards token %d (%s)", yystate, token, debug_name(token));
					token = YYEMPTY;
				}
			}
		}
		if (yyoverflow)
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
#include "parser.act.h"
		}

		yystack.pop(yym);
		yystate	= yystack.top();
		yym		= yylhs[yyn];

		if (yystate == 0 && yym == 0) {
			DEBUGPRINTF("after reduction, shifting from state 0 to state %d", YYFINAL);
			yystate			  = YYFINAL;
			yystack.push(YYFINAL, yyval);
			if (token < 0) {
				token = (yytokentype)YYLEX;
				if (token < 0)
					token = YYEOF;
				DEBUGPRINTF("state %d, reading %d (%s)", YYFINAL, token, debug_name(token));
			}
			if (token == YYEOF)
				return 0;	//accept

		} else {
			if ((yyn = yygindex[yym]) && (yyn += yystate) > 0 && yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
				yystate = yytable[yyn];
			else
				yystate = yydgoto[yym];

			DEBUGPRINTF("after reduction, shifting from state %d to state %d", yystack.top(), yystate);
			if (!yystack.push(yystate, yyval))
				break;
		}
	}

//yyoverflow:
	YYERROR_CALL("yacc stack overflow");
	return 1;
}

#else

struct YYPARSESTATE {
	struct YYPARSESTATE *save;		/* Previously saved parser state */
	YYSTACKDATA			yystack;	/* saved parser stack */
	int					state;		/* saved parser state */
	int					errflag;	/* saved error recovery status */
	int					lexeme;		/* saved index of the conflict lexeme in the lexical queue */
	YYINT				ctry;		/* saved index in yyctable[] for this conflict */

	YYPARSESTATE(YYPARSESTATE *save, const YYSTACKDATA &stack, int state, int errflag) : save(save), yystack(stack.size()), state(state), errflag(errflag) {
		yystack.copy(stack);
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


// YYVALID_NESTED: When we have a nested conflict (conflict while processing in trial mode for another conflict), we want to relate YYVALID to a particular level of conflict being in trial.
// in 2-level conflict it is a no-op; a YYVALID for the outer conflict will be searched for

#define YYVALID        do if (yytrial)  goto yyvalid; while(0)
#define YYVALID_NESTED do if (yytrial && !yytrial->save) goto yyvalid; while(0)

#if YYDEBUG
#define DEBUGPRINTF(fmt, ...)	if (yydebug) debug_printf(YYPREFIX "debug%s:" fmt "\n", yytrial ? "(trial)" : yytokenq.can_get() ? "(playback)" : "", __VA_ARGS__);
#else
#define DEBUGPRINTF(...)
#endif

int YYPARSE_DECL() {
	//from lex
	yytokentype		token		= YYEMPTY;
	YYSTYPE			yylval;

	//stack
	YYSTACKDATA		yystack;
	int				yyn			= 0;
	int				yystate		= 0;
	int				yyerrflag	= 0;
	int				yyresult	= 0;
	int				yynerrs		= 0;

	memset(&yylval, 0, sizeof(yylval));
	yystack.push(0, yylval);

	YYPARSESTATE	*yyerrctx	= 0;
	YYPARSESTATE	*yypath		= 0;
	YYPARSESTATE	*yytrial	= 0;

	YYTOKENQUEUE	yytokenq;

yyloop:
	if ((yyn = yydefred[yystate]) != 0)
		goto yyreduce;

	if (token < 0) {
		if (yytokenq.can_get()) {
			token = yytokenq.get(yylval);
		} else {
			token = (yytokentype)YYLEX;
			if (yytrial)
				yytokenq.put(token, yylval);
		}
		if (token < 0)
			token = YYEOF;
		DEBUGPRINTF("state %d, reading %d (%s)", yystate, token, debug_name(token));
	}

	// Do we have a conflict?
	if ((yyn = yycindex[yystate]) && (yyn += token) >= 0 && yyn <= YYTABLESIZE && yycheck[yyn] == token) {
		int		ctry;

		if (yypath) {
			// Switch to the next conflict context
			DEBUGPRINTF("CONFLICT in state %d: following successful trial parse", yystate);

			YYPARSESTATE *save = yypath;
			yypath		= save->save;
			save->save	= 0;
			ctry		= save->ctry;
			if (save->state != yystate)
				goto yyabort;
			delete save;

		} else {
			// Unresolved conflict - start/continue trial parse
			DEBUGPRINTF("CONFLICT in state %d - %s", yystate, yytrial ? "ALREADY in conflict, continuing trial parse" : "starting trial parse");
			
			ctry	= yytable[yyn];
			if (yyctable[ctry] == -1) {
				DEBUGPRINTF("backtracking 1 token");
				ctry++;
			}
				
			if (!yytrial) {
				// If this is a first conflict in the stack, start saving lexemes
				if (!yytokenq.can_get()) {
					// reset put ptr
					yytokenq.reset();
					yytokenq.put(token, yylval);
				}
			}
			--yytokenq.pput;

			yytrial			= new YYPARSESTATE(yytrial, yystack, yystate, yyerrflag);
			yytrial->ctry	= ctry;
			yytrial->lexeme	= yytokenq.size();
			token			= YYEMPTY;
		}

		if (yytable[yyn] == ctry) {
			DEBUGPRINTF("state %d, shifting to state %d", yystate, yyctable[ctry]);

			if (token < 0)
				++yytokenq.pput;

			yystate = yyctable[ctry];
			if (!yystack.push(yystate, yylval))
				goto yyoverflow;

			token  = YYEMPTY;
			if (yyerrflag > 0)
				--yyerrflag;
			goto yyloop;
		} else {
			yyn = yyctable[ctry];
			goto yyreduce;
		}
	} /* End of code dealing with conflicts */


	if ((yyn = yysindex[yystate]) && (yyn += token) >= 0 && yyn <= YYTABLESIZE && yycheck[yyn] == token) {
		DEBUGPRINTF("state %d, shifting to state %d", yystate, yytable[yyn]);
		yystate = yytable[yyn];
		yystack.push(yystate, yylval);
		token = YYEMPTY;
		if (yyerrflag > 0)
			--yyerrflag;
		goto yyloop;
	}

	if ((yyn = yyrindex[yystate]) && (yyn += token) >= 0 && yyn <= YYTABLESIZE && yycheck[yyn] == token) {
		yyn = yytable[yyn];
		goto yyreduce;
	}

	if (yyerrflag == 0) {
		while (yytrial) {
			DEBUGPRINTF("ERROR in state %d, CONFLICT BACKTRACKING to state %d, %d tokens", yystate, yytrial->state, (int)(yytokenq.size() - yytrial->lexeme));

			// Memorize most forward-looking error state in case it's really an error
			if (!yyerrctx || yyerrctx->lexeme < yytokenq.size()) {
				delete yyerrctx;
				yyerrctx			= new YYPARSESTATE(yytrial, yystack, yystate, yyerrflag);
				yyerrctx->lexeme	= yytokenq.size();
			}

			yytokenq.start_playback(yytrial->lexeme);
			yystack.copy(yytrial->yystack);
			yystate		= yytrial->state;
			token		= YYEMPTY;

			int	ctry	= ++yytrial->ctry;
			// We tried shift, try reduce now
			if ((yyn = yyctable[ctry]) >= 0)
				goto yyreduce;

			YYPARSESTATE *save = yytrial;
			yytrial		= yytrial->save;
			delete save;

			// Nothing left on the stack -- error
			if (!yytrial) {
				DEBUGPRINTF("trial parse FAILED, entering ERROR mode");
				// Restore state as it was in the most forward-advanced error
				yytokenq.start_playback(yyerrctx->lexeme - 1);
				yystate		= yyerrctx->state;
				token		= yytokenq.get(yylval);
				yystack.copy(yyerrctx->yystack);

				delete yyerrctx;
				yyerrctx	= NULL;
			}
		}
		++yynerrs;
		YYERROR_CALL("syntax error");
	}
	if (yyerrflag < 3) {
		yyerrflag = 3;
		for (;;) {
			if (((yyn = yysindex[yystack.top()]) != 0) && (yyn += YYERRCODE) >= 0 && yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE) {
				DEBUGPRINTF("state %d, error recovery shifting to state %d", yystack.top(), yytable[yyn]);
				yystate = yytable[yyn];
				if (!yystack.push(yystate, yylval))
					goto yyoverflow;
				goto yyloop;

			} else {
				DEBUGPRINTF("error recovery discarding state %d", yystack.top());
				if (yystack.empty())
					goto yyabort;
#if defined(YYDESTRUCT_CALL)
				if (!yytrial)
					YYDESTRUCT_CALL("error: discarding state", yystos[*yystack.s_mark], yystack.l_mark);
#endif
				yystack.pop(1);
			}
		}
	} else {
		if (token == YYEOF)
			goto yyabort;
		DEBUGPRINTF("state %d, error recovery discarding token %d (%s)", yystate, token, debug_name(token));
#if defined(YYDESTRUCT_CALL)
		if (!yytrial)
			YYDESTRUCT_CALL("error: discarding token", token, &yylval);
#endif
		token = YYEMPTY;
		goto yyloop;
	}

yyreduce:
	DEBUGPRINTF("state %d, reducing by rule %d (%s)", yystate, yyn, yyrule[yyn]);

	int		yym = yylen[yyn];
	YYSTYPE	yyval;
	if (yym > 0)
		yyval = yystack.getl(1 - yym);
	else
		memset(&yyval, 0, sizeof yyval);

	switch (yyn) {
#include "parser.act.h"
	}

	yystack.pop(yym);
	yystate = yystack.top();
	yym = yylhs[yyn];

	if (yystate == 0 && yym == 0) {
		DEBUGPRINTF("after reduction, shifting from state 0 to state %d", YYFINAL);

		yystate = YYFINAL;
		yystack.push(YYFINAL, yyval);
		if (token < 0) {
			if (yytokenq.can_get()) {
				token = yytokenq.get(yylval);
			} else {
				token = (yytokentype)YYLEX;
				if (yytrial)
					yytokenq.put(token, yylval);
			}
			if (token < 0)
				token = YYEOF;

			DEBUGPRINTF("state %d, reading %d (%s)", yystate, token, debug_name(token));
		}
		if (token == YYEOF)
			goto yyaccept;
		goto yyloop;
	}
	if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 && yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
		yystate = yytable[yyn];
	else
		yystate = yydgoto[yym];


	DEBUGPRINTF("after reduction, shifting from state %d to state %d", yystack.top(), yystate);
	if (!yystack.push(yystate,yyval))
		goto yyoverflow;
	goto yyloop;


yyaccept:
	if (!yytrial) {
		yyresult = 0;
		goto yyreturn;
	}
		// Reduction declares that this path is valid. Set yypath and do a full parse

yyvalid:
	if (yypath)
		goto yyabort;

	while (auto *save = yytrial) {
		yytrial		= save->save;
		save->save	= yypath;
		yypath		= save;
	}

	DEBUGPRINTF("state %d, CONFLICT trial successful, backtracking to state %d, %d tokens", yystate, yypath->state, yytokenq.size() - yypath->lexeme);

	delete yyerrctx;
	yyerrctx = NULL;

	yytokenq.start_playback(yypath->lexeme);
	yystack.copy(yypath->yystack);
	yystate	= yypath->state;
	token	= YYEMPTY;
	goto yyloop;

yyoverflow:
	YYERROR_CALL("yacc stack overflow");
	yyresult = 2;
	goto yyreturn;

yyenomem:
	YYERROR_CALL("memory exhausted");
	yyresult = 2;
	goto yyreturn;

yyabort:
	yyresult = 1;
	goto yyreturn;


yyreturn:
#if defined(YYDESTRUCT_CALL)
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (YYSTYPE *pv = yystack.l_base; pv <= yystack.l_mark; ++pv)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[*(yystack.s_base + (pv - yystack.l_base))], pv);
#endif

	delete yyerrctx;

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

#endif
