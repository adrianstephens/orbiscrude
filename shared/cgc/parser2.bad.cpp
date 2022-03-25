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

#include <stdlib.h>	// needed for malloc, etc
#include <string.h>	// needed for memset

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
static void debug_rule(char *out, int r) {
	strcpy(out, yyname[yylhs[r] + YYMAXTOKEN + 2]);
	out		+= strlen(out);
	*out++	= ':';
	*out	= 0;

	for (auto *rhs = yyrhs + yyrhsoffset[r], *end = rhs + yylen[r]; rhs != end; ++rhs) {
		*out++ = ' ';
		strcpy(out, yyname[*rhs]);
		out += strlen(out);
	}
}
static const char *debug_rule(int r) {
	static char buffer[512];
	debug_rule(buffer, r);
	return buffer;
}
#endif

inline int yy_index(int n, int token) {
	return n && (n += token) > 0 && n <= YYTABLESIZE && yycheck[n] == token ? n : 0;
}

inline int yy_goto(int state, int lhs) {
	if (int n = yy_index(yygindex[lhs], state))
		return yytable[n];
	return yydgoto[lhs];
}

#define YYSTACKSIZE		10000
#define YYMAXDEPTH		10000
#define YYINITSTACKSIZE	200

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

#if !YYBACKTRACKING// && !YYGLR

#define YYABORT		goto yyabort
#define YYREJECT	goto yyabort
#define YYACCEPT	goto yyaccept
#define YYERROR		goto yyerrlab
#define YYVALID
#define YYVALID_NESTED

#if YYDEBUG
#define DEBUGPRINTF(fmt, ...)	if (yydebug) debug_printf(YYPREFIX "debug:" fmt "\n", __VA_ARGS__);
#else
#define DEBUGPRINTF(...)
#endif

int YYPARSE_DECL() {
	static const bool yytrial	= false;
	
	//from lex
	yytokentype		token		= YYEMPTY;
	YYSTYPE			yylval;

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
				
				DEBUGPRINTF("state %d, reading %d (%s)", yystate, token, debug_name(token));
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
					while (!(yyn = yy_index(yysindex[yystack.top_state()], YYERRCODE))) {
						DEBUGPRINTF("error recovery discarding state %d", yystack.top_state());
						if (yystack.empty()) {
							yyresult = 1;	//abort
							break;
						}
#ifdef YYDESTRUCT_CALL
						YYDESTRUCT_CALL("error: discarding state", yystos[yystack.top_state()], yystack.getl(0));
#endif
						yystack.pop(1);
					}

					DEBUGPRINTF("state %d, error recovery shifting to state %d", yystack.top_state(), yytable[yyn]);
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
		DEBUGPRINTF("state %d, reducing by rule %d (%s)", yystate, yyn, debug_rule(yyn));
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
				DEBUGPRINTF("state %d, reading %d (%s)", YYFINAL, token, debug_name(token));
			}
			if (token == YYEOF)
				break;	// accept
		}
#endif
	}

	if (yyresult == 2)
		YYERROR_CALL("yacc stack overflow");

#ifdef YYDESTRUCT_CALL
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (auto *i = yystack.base; i != yystack.top; i++)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[i->state], &i->lval);
#endif

	return yyresult;
}

#elif YYBACKTRACKING

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
				DEBUGPRINTF("state %d, reading %d (%s)", yystate, token, debug_name(token));
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
#ifdef YYDESTRUCT_CALL
						if (!yytrial)
							YYDESTRUCT_CALL("error: discarding state", yystos[yystack.top_state()], &yystack.getl(0));
#endif
						yystack.pop(1);
					}
					DEBUGPRINTF("state %d, error recovery shifting to state %d", yystack.top_state(), yytable[yyn]);
					if (!yystack.push(yystate = yytable[yyn], yylval))
						yyresult = 2;

				} else if (token == YYEOF) {
					yyresult = 1;

				} else {
					DEBUGPRINTF("state %d, error recovery discarding token %d (%s)", yystate, token, debug_name(token));
			#ifdef YYDESTRUCT_CALL
					if (!yytrial)
						YYDESTRUCT_CALL("error: discarding token", token, &yylval);
			#endif
					token = YYEMPTY;
				}
			}
		}

		if (yyresult)
			break;

		//reduce
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

		if (yyresult)
			break;

		yystack.pop(yym);
		yystate	= yystack.top_state();
		
		yystate = yy_goto(yystate, yylhs[yyn]);
		if (!yystack.push(yystate = YYFINAL, yyval))
			yyresult = 2;

		DEBUGPRINTF("after reduction, shifting from state %d to state %d", yystack.top_state(), yystate);
		
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
				DEBUGPRINTF("state %d, reading %d (%s)", yystate, token, debug_name(token));
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

#ifdef YYDESTRUCT_CALL
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (auto *i = yystack.base; i != yystack.top; i++)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[i->state], &i->lval);
#endif

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


#elif YYGLR

//-----------------------------------------------------------------------------
// GLR
//-----------------------------------------------------------------------------

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

template<typename T> inline void swap(T &a, T &b) {
	T t(a);
	a = b;
	b = t;
}

template<typename I, typename P> void heap_siftdown(I begin, I end, I root, P comp) {
	for (;;) {
		I child = root + (root - begin + 1);
		if (child >= end)
			break;
		if (child + 1 < end && comp(child[1], child[0]))
			++child;
		if (!comp(*child, *root))
			break;
		swap(*root, *child);
		root = child;
	}
}
template<typename I, typename P> void heap_siftup(I begin, I end, I child, P comp) {
	while (child > begin) {
		I parent = begin + (child - begin - 1) / 2;
		if (comp(*parent, *child))
			break;
		swap(*parent, *child);
		child = parent;
	}
}

template<typename I, typename P> void heap_push(I begin, I end, P comp) {
	heap_siftup(begin, end, end - 1, comp);
}

template<typename I, typename P> void heap_pop(I begin, I end, P comp) {
	swap(*--end, *begin);
	heap_siftdown(begin, end, begin, comp);
}


template<typename T, int N> struct priority_queue {
	T	heap[N];
	int	n;
	priority_queue() : n(0) {}

	T&		top() {
		return heap[0];
	}
	void	pop() {
		heap_pop(heap, heap + n);
		--n;
	}
	T pop_value() {
		heap_pop(C::begin(), C::end(), p);
		return heap[--n];
	}
	template<typename T2> void push(const T2& t) {
		heap[n++] = t;
		heap_push(heap, heap + n);
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
		int		top;
		int		next;			// next concurrent stack
		int		linear_depth;

		inline void *operator new(size_t, ENTRY *e)	{ return e; }
		inline void operator delete(void*,ENTRY *e)	{}

		STACK() : top(0), next(0), linear_depth(0) {}

		STACK	*get_next() const {
			return next ? (STACK*)((ENTRY*)this + next) : 0;
		}
		void	set_next(STACK *s) {
			next = int((ENTRY*)s - (ENTRY*)this);
		}

		ENTRY	*get_top() const {
			return top ? (ENTRY*)this + top : 0;
		}
		void	set_top(ENTRY *p) {
			top = int(p - (ENTRY*)this);
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
			e->next		= int(e - p);
			e->state	= state;
			e->lval		= lval;
			set_top(e);
			if (e->next != 1)
				linear_depth = 0;
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

	YYSTACKGLR(unsigned size = YYINITSTACKSIZE) {
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
	ENTRY	*alloc() {
		if (free >= end && !(end - base < YYMAXDEPTH && resize((end - base) * 2)))
			return 0;
		return free++;
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
};

int yyreduce(YYSTACKGLR::STACK &yystack, int rule, int len, YYSTYPE &yyval, CG *cg) {
	if (len)
		yyval = yystack.get_entry(1 - len)->lval;
	else
		memset(&yyval, 0, sizeof yyval);

	switch (rule) {
#include "parser.act.h"
	}

	yystack.pop(len);
	
	return yy_goto(yystack.top_state(), yylhs[rule]);
}

int YYPARSE_DECL() {
	//stack
	YYSTACKGLR		yystack;
	int				yyresult	= 0;

	typedef YYSTACKGLR::STACK STACK;

	struct reduction {
		STACK	*s;
		int		n;
		int		len;
	};
	reduction	reductions[32];


	YYSTYPE yylval;
	memset(&yylval, 0, sizeof(yylval));

	yystack.push(yystack.stack0(), 0, yylval);

	while (!yyresult) {
		yytokentype token = (yytokentype)YYLEX;
		if (token < 0)
			token = YYEOF;
		
		DEBUGPRINTF("reading %d (%s)", token, debug_name(token));

		STACK *s = yystack.stack0();
		if (!s->next) {
			//-------------------------
			// deterministic case
			//-------------------------
			int	state	= s->top_state();
			int	n, c;

			// reductions
			while ((n = yydefred[state]) || !(c = yy_index(yycindex[state], token)) && !(n = yy_index(yysindex[state], token))) {
				if (!n) {
					if (!(n = yy_index(yyrindex[state], token))) {
						//error
					} else {
						n = yytable[n];
					}
				}
				DEBUGPRINTF("%s(%i): state %d, reducing by rule %d (%s)", YYFILENAME, yyline[n], state, n, debug_rule(n));
				state = yyreduce(*s, n, yylen[n], yylval, cg);
				DEBUGPRINTF("after reduction, shifting from state %d to state %d", s->top_state(), state);
				if (!yystack.push(s, state, yylval))
					yyresult = 2;
			}

			if (c) {
				auto	*conflicts = yyctable + yytable[c];
				while (int c = *conflicts++)
					yystack.split(s);

			} else {
				//shift
				DEBUGPRINTF("state %d, shifting to state %d", state, yytable[n]);
				if (!yystack.push(s, state = yytable[n], yylval))
					yyresult = 2;
			}

		} else {
			//-------------------------
			// non-deterministic case
			//-------------------------
			reduction	*rp = reductions;

			// collect reductions
			for (STACK *s = yystack.stack0(); s; s = s->get_next()) {
				int	state	= s->top_state();

				//default reduction?
				if (int n = yydefred[state]) {
					DEBUGPRINTF("state %d, reducing by default rule %d (%s)", state, n, debug_rule(n));
					rp->s	= s;
					rp->n	= n;
					rp->len	= yylen[n];
					++rp;

					// Do we have a conflict?
				} else if (n = yy_index(yycindex[state], token)) {
					auto	*conflicts = yyctable + yytable[n];
					while (int c = *conflicts++)
						yystack.split(s);

				} else if (yy_index(yysindex[state], token)) {
					//on shift, go to next stack
					continue;

				} else if (n = yy_index(yyrindex[state], token)) {
					n		= yytable[n];
					DEBUGPRINTF("state %d, reducing by rule %d (%s)", state, n, debug_rule(n));
					rp->s	= s;
					rp->n	= n;
					rp->len	= yylen[n];
					++rp;

				} else {
					// error, so kill this stack
					s->top = 0;
				}
			}

			//perform (sorted) reductions
			for (reduction *i = reductions; i != rp; ++i) {
				YYSTYPE		yyval;
				int			state = yyreduce(*i->s, i->n, i->len, yyval, cg);

				if (STACK *s2 = yystack.find_merge(s, state)) {
					//merge
					s->top = 0;
				} else {
					yystack.push(s, state, yyval);
				}
			}

			for (STACK *s = yystack.stack0(); s; s = s->get_next()) {
				int	state = s->top_state();
				if (int n = yy_index(yysindex[state], token)) {
					DEBUGPRINTF("state %d, shifting to state %d", state, yytable[n]);
					state = yytable[n];
					if (STACK *s2 = yystack.find_merge(s, state)) {
						//merge
						s->top = 0;
					} else {
						yystack.push(s, state, yylval);
					}
				}
			}
		}
	}

	if (yyresult == 2)
		YYERROR_CALL("yacc stack overflow");

#ifdef YYDESTRUCT_CALL
	if (token != YYEOF && token != YYEMPTY)
		YYDESTRUCT_CALL("cleanup: discarding token", token, &yylval);

	for (auto *i = yystack.base; i != yystack.top; i++)
		YYDESTRUCT_CALL("cleanup: discarding state", yystos[i->state], &i->lval);
#endif

	return yyresult;
}

#endif




#ifdef GLR

# include <setjmp.h>

#define YYFREE free
#define YYMALLOC malloc
#define YYREALLOC realloc
#define YYDPRINTF(...)
#define YYASSERT(...)

#define YYTRANSLATE(YYX)	YYX	//((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

typedef int			yyStateNum;
typedef int			yyRuleNum;
typedef int			yySymbol;
typedef short int	yyItemNum;
typedef enum { yyok, yyaccept, yyabort, yyerr, yyoom } YYRESULTTAG;

extern const short int yypact[];
extern const unsigned short int yydefact[];
extern const short int yypgoto[];
extern const short int yydefgoto[];
extern const short int yycheck[];
extern const unsigned char yystos[];
extern const unsigned char yyr1[];
extern const unsigned char yyr2[];
extern const unsigned char yydprec[];
extern const unsigned char yymerger[];
extern const bool yyimmediate[];
extern const unsigned char yyconflp[];
extern const short int yyconfl[];

static YYSTYPE yyval_default;

//static const int YYEMPTY = -2;

#define YYLAST		2946
#define YYNTOKENS	111
#define YYNNTS		127
#define YYNRULES	326
#define YYNSTATES	549
#define YYMAXRHS	10
#define YYMAXLEFT	0

#define YYUNDEFTOK	2
#define YYMAXUTOK	341

#define YYINITDEPTH 200
#define YYMAXDEPTH 10000
#define YYHEADROOM 2

#define YYSTACKEXPANDABLE 1

//Release the memory associated to this symbol

static void yydestruct(int yytype, YYSTYPE* yyvaluep) {
}
// Number of symbols composing the right hand side of rule #RULE.
static inline int yyrhsLength(yyRuleNum rule) { return yyr2[rule]; }

// Left-hand-side symbol for rule #YYRULE.
static inline yySymbol yylhsNonterm(yyRuleNum rule) { return yyr1[rule]; }

#define yypact_value_is_default(Yystate) (Yystate == -459)

// True iff LR state YYSTATE has only a default reduction (regardless of token).
static inline bool yyisDefaultedState(yyStateNum state) { return yypact_value_is_default(yypact[state]); }

// Set *YYACTION to the action to take in YYSTATE on seeing YYTOKEN. Result R means:
// R < 0: Reduce on rule -R
// R = 0: Error
// R > 0: Shift to state R
// Set *YYCONFLICTS to a pointer into yyconfl to a 0-terminated list of conflicting reductions.
static inline int yygetLRActions(yyStateNum state, int token, const short int** conflicts) {
	int yyindex = yypact[state] + token;
	if (yypact_value_is_default(yypact[state]) || yyindex < 0 || YYLAST < yyindex || yycheck[yyindex] != token) {
		*conflicts	= yyconfl;
		return -yydefact[state];

	} else {
		*conflicts	= yyconfl + yyconflp[yyindex];
		return 0;
	}
}

static inline yyStateNum yyLRgotoState(yyStateNum state, yySymbol yylhs) {
	int yyr = yypgoto[yylhs - YYNTOKENS] + state;
	if (0 <= yyr && yyr <= YYLAST && yycheck[yyr] == state)
		return yytable[yyr];
	else
		return yydefgoto[yylhs - YYNTOKENS];
}


//-----------------------------------------------------------------------------

struct yyGLRState {
	bool			is_state;		// Type tag: always true.
	bool			resolved;		// Type tag for semantics. If true, yysval applies, otherwise first_val applies.
	yyStateNum		state;			// Number of corresponding LALR(1) machine state.
	yyGLRState*		pred;			// Preceding state in this stack
	size_t			yyposn;			// Source position of the last token produced by my symbol
	union {
		struct yySemanticOption* first_val;	// First in a chain of alternative reductions producing the non-terminal corresponding to this state, threaded through next.
		YYSTYPE		yysval;			// Semantic value for this state.
	} semantics;

	void destroy();

	// Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR state YYLRSTATE, at input position YYPOSN, with (resolved) semantic value *YYVALP and source location *YYLOCP.
	inline void shift(yyGLRState *_pred, yyStateNum _state, size_t _yyposn, YYSTYPE* _yyvalp) {
		state				= _state;
		yyposn				= _yyposn;
		resolved			= true;
		pred				= _pred;
		semantics.yysval	= *_yyvalp;
	}

	// Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR state YYLRSTATE, at input position YYPOSN, with the (unresolved) semantic value of YYRHS under the action for YYRULE.
	inline void shift_defer(yyGLRState *_pred, yyStateNum _state, size_t _yyposn) {
		state				= _state;
		yyposn				= _yyposn;
		resolved			= false;
		pred				= _pred;
		semantics.first_val	= NULL;
	}
};


struct yySemanticOption {
	bool				is_state;	// Type tag: always false.
	yyRuleNum			rule;		// Rule number for this reduction
	yyGLRState*			state;		// The last RHS state in the list of states to be reduced.
	int					rawchar;	// The lookahead for this reduction.
	YYSTYPE				val;
	yySemanticOption*	next;		// Next sibling in chain of options. To facilitate merging, options are chained in decreasing order by address.

	// Assuming identicalOptions (YYY0,YYY1), destructively merge the alternative semantic values for the RHS-symbols of YYY1 and YYY0.
	void merge(yySemanticOption* o1) {
		int			n = yyrhsLength(rule);
		for (yyGLRState *s0 = state, *s1 = o1->state; n--; s0 = s0->pred, s1 = s1->pred) {
			if (s0 == s1)
				break;
			else if (s0->resolved) {
				s1->resolved		 = true;
				s1->semantics.yysval = s0->semantics.yysval;
			} else if (s1->resolved) {
				s0->resolved		 = true;
				s0->semantics.yysval = s1->semantics.yysval;
			} else {
				for (yySemanticOption** z0p = &s0->semantics.first_val, *z1 = s1->semantics.first_val; z1 && z1 != *z0p; z0p = &(*z0p)->next) {
					if (!*z0p) {
						*z0p = z1;
						break;
					}
					if (*z0p < z1) {
						yySemanticOption* z = *z0p;
						*z0p	= z1;
						z1		= z1->next;
						(*z0p)->next = z;
					}
				}
				s1->semantics.first_val = s0->semantics.first_val;
			}
		}
	}
	friend bool identical(const yySemanticOption* o0, const yySemanticOption* o1) {
		if (o0->rule == o1->rule) {
			int	n = yyrhsLength(o0->rule);
			for (yyGLRState *s0 = o0->state, *s1 = o1->state; n-- > 0; s0 = s0->pred, s1 = s1->pred)
				if (s0->yyposn != s1->yyposn)
					return false;
			return true;
		} else
			return false;
	}

	// Y0 and Y1 represent two possible actions to take in a given parsing state; return 0 if no combination is possible, 1 if user-mergeable, 2 if Y0 is preferred, 3 if Y1 is preferred.
	friend int preference(yySemanticOption* y0, yySemanticOption* y1) {
		yyRuleNum	r0 = y0->rule, r1 = y1->rule;
		int			p0 = yydprec[r0], p1 = yydprec[r1];
		return p0 == p1
			? (yymerger[r0] == 0 || yymerger[r0] != yymerger[r1] ? 0 : 1)
			: p0 == 0 || p1 == 0 ? 0
			: p0 < p1 ? 3 : 2;
	}
};

void yyGLRState::destroy() {
	if (resolved)
		yydestruct(yystos[state], &semantics.yysval);

	else if (yySemanticOption *option = semantics.first_val) {
		int n = yyrhsLength(option->rule);
		for (yyGLRState *s = option->state; n--; s = s->pred)
			s->destroy();
	}
}

// Type of the items in the GLR stack. The is_state field indicates which item of the union is valid.
union yyGLRStackItem {
	yyGLRState		 state;
	yySemanticOption option;
};

#define YYRELOC(YYFROMITEMS, YYTOITEMS, YYX, YYTYPE) &((YYTOITEMS) - ((YYFROMITEMS) - (yyGLRStackItem*)(YYX)))->YYTYPE
#define YYSIZEMAX ((size_t) -1)


// Perform user action for rule number YYN, with RHS length YYRHSLEN, and top stack item YYVSP. YYLVALP points to place to put semantic value ($$), and yylocp points to place for location information (@$).
// Returns yyok for normal return, yyaccept for YYACCEPT, yyerr for YYERROR, yyabort for YYABORT.
static YYRESULTTAG yyuserAction(yyRuleNum yyn, yyGLRStackItem* yyvsp, YYSTYPE* yyvalp) {
	switch (yyn) {
		default:
			return yyok;
	}
}

static void yyuserMerge (int yyn, YYSTYPE* yy0, YYSTYPE* yy1) {
	switch (yyn) {
		default: break;
	}
}

struct yyGLRStack {
	int				yyerrState;
	yyGLRStackItem* items;
	yyGLRStackItem* next_free;
	size_t			space_left;
	yyGLRState*		split_point;
	yyGLRState*		last_deleted;

	yyGLRState**	tops;
	size_t			tops_size, tops_capacity;

	// Initialize *YYSTACKP to a single empty stack, with total maximum capacity for all stacks of YYSIZE.
	yyGLRStack(size_t size) : yyerrState(0), space_left(size), split_point(0), last_deleted(0), tops_size(1), tops_capacity(16) {
		items		= (yyGLRStackItem*)YYMALLOC(size * sizeof(yyGLRStackItem));
		next_free	= items;

		tops		= (yyGLRState**)YYMALLOC(16 * sizeof(yyGLRState*));
		tops[0]		= NULL;
	}

	~yyGLRStack() {
		free(items);
		YYFREE(tops);
	}

	bool expand() {
		size_t	size = next_free - items;
		if (YYMAXDEPTH - YYHEADROOM < size)
			return false;

		size_t	new_size = 2 * size;
		if (YYMAXDEPTH < new_size)
			new_size = YYMAXDEPTH;

		yyGLRStackItem	*new_items = (yyGLRStackItem*)YYMALLOC(new_size * sizeof(yyGLRStackItem));
		if (!new_items)
			return false;

		size_t	n = size;
		for (yyGLRStackItem *p0 = items, *p1 = new_items; n--; ++p0, ++p1) {
			*p1 = *p0;
			if (p0->state.is_state) {
				yyGLRState* s0 = &p0->state;
				yyGLRState* s1 = &p1->state;
				if (s0->pred)
					s1->pred = &(p1 + ((yyGLRStackItem*)s0->pred - p0))->state;
				if (!s0->resolved && s0->semantics.first_val)
					s1->semantics.first_val = YYRELOC(p0, p1, s0->semantics.first_val, option);
			} else {
				yySemanticOption* v0 = &p0->option;
				yySemanticOption* v1 = &p1->option;
				if (v0->state)
					v1->state = YYRELOC(p0, p1, v0->state, state);
				if (v0->next)
					v1->next = YYRELOC(p0, p1, v0->next, option);
			}
		}
		if (split_point)
			split_point = YYRELOC(items, new_items, split_point, state);

		for (n = 0; n < tops_size; ++n)
			if (tops[n])
				tops[n] = YYRELOC(items, new_items, tops[n], state);

		free(items);
		items		= new_items;
		next_free	= new_items	+ size;
		space_left	= new_size	- size;
		return true;
	}

	size_t split(size_t k) {
		if (!split_point) {
			YYASSERT(k == 0);
			split_point = tops[k];
		}

		if (tops_size >= tops_capacity) {
			if (tops_capacity > (YYSIZEMAX / (2 * sizeof yyGLRState)))
				return false;
			tops_capacity *= 2;

			yyGLRState** new_tops = (yyGLRState**)YYREALLOC(tops, (tops_capacity * sizeof(yyGLRState*)));
			if (!new_tops)
				return false;
			tops = new_tops;

		}
		tops[tops_size]	= tops[k];
		return tops_size++;
	}

	// Assuming that YYS is a GLRState somewhere on *YYSTACKP, update the splitpoint of *YYSTACKP, if needed, so that it is at least as deep as YYS.
	 inline void update_split(yyGLRState* yys) {
		if (split_point && split_point > yys)
			split_point = yys;
	}

	 // Invalidate stack #YYK in *YYSTACKP.
	 inline void mark_deleted(size_t k) {
		 if (tops[k])
			 last_deleted = tops[k];
		 tops[k] = 0;
	 }

	 // Undelete the last stack in *YYSTACKP that was marked as deleted. Can only be done once after a deletion, and only when all other stacks have been deleted.
	 bool undelete_last() {
		 if (!last_deleted || tops_size)
			 return false;
		 tops[0]		= last_deleted;
		 tops_size		= 1;
		 last_deleted	= 0;
		 return true;
	 }

	 inline void remove_deletes() {
		 for (size_t i = 0, j = 0; j < tops_size; ++i) {
			 if (!tops[i]) {
				 --tops_size;
			 } else {
				 tops[j] = tops[i];
				 ++j;
			 }
		 }
	 }

	 YYRESULTTAG resolve_value(yyGLRState* yys);
	 YYRESULTTAG resolve_action(yySemanticOption* yyopt, YYSTYPE* yyvalp);

	 // Resolve the previous N states starting at and including state S
	 YYRESULTTAG resolve_states(yyGLRState* s, int n) {
		 if (n > 0) {
			 YYASSERT(s->pred);
			 if (auto chk = resolve_states(s->pred, n - 1))
				 return chk;
			 if (!s->resolved)
				 return resolve_value(s);
		 }
		 return yyok;
	 }

	 //called when returning to deterministic
	 YYRESULTTAG resolve() {
		int	n = 0;
		for (yyGLRState *s = tops[0]; s != split_point; s = s->pred)
			++n;
		return resolve_states(tops[0], n);
	 }
	 void compress() {
		 yyGLRState *r = 0;
		 for (yyGLRState *p = tops[0], *q; p != split_point; r = p, p = q) {
			 q = p->pred;
			 p->pred = r;
		 }

		 space_left		+= next_free - items;
		 next_free		= ((yyGLRStackItem*)split_point) + 1;
		 space_left		-= next_free - items;
		 split_point	= 0;
		 last_deleted	= 0;

		 while (r) {
			 next_free->state	= *r;
			 r					= r->pred;
			 next_free->state.pred = &next_free[-1].state;
			 tops[0]				= &next_free->state;
			 ++next_free;
			 --space_left;
		 }
	 }


	// Return a fresh GLRStackItem in YYSTACKP. The item is an LR state if YYISSTATE, and otherwise a semantic option
	inline yyGLRStackItem* new_item(bool is_state) {
		yyGLRStackItem* i = next_free++;
		--space_left;
		i->state.is_state = is_state;
		return i;
	}

	// Add a new semantic action that will execute the action for rule YYRULE on the semantic values in YYRHS to the list of alternative actions for YYSTATE
	bool add_deferred(size_t k, yyGLRState* state, yyGLRState* rhs, yyRuleNum rule, int yychar, YYSTYPE yylval) {
		yySemanticOption* o	= &new_item(false)->option;
		o->state	= rhs;
		o->rule		= rule;
		o->rawchar	= yychar;
		o->val		= yylval;
		o->next		= state->semantics.first_val;
		state->semantics.first_val = o;

		return space_left >= YYHEADROOM || expand();
	}

	// Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR state YYLRSTATE, at input position YYPOSN, with (resolved) semantic value *YYVALP and source location *YYLOCP.
	inline bool shift(size_t k, yyStateNum state, size_t yyposn, YYSTYPE* yyvalp) {
		yyGLRState	*s = &new_item(true)->state;
		s->shift(tops[k], state, yyposn, yyvalp);
		tops[k] = s;
		return space_left >= YYHEADROOM || expand();
	}

	// Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR state YYLRSTATE, at input position YYPOSN, with the (unresolved) semantic value of YYRHS under the action for YYRULE.
	inline bool shift_defer(size_t k, yyStateNum state, size_t yyposn, yyGLRState* rhs, yyRuleNum rule, int yychar, YYSTYPE yylval) {
		yyGLRState	*s = &new_item(true)->state;
		s->shift_defer(tops[k], state, yyposn);
		tops[k]	= s;
		return add_deferred(k, s, rhs, rule, yychar, yylval);
	}

	// Pop the symbols consumed by reduction #YYRULE from the top of stack #YYK of *YYSTACKP, and perform the appropriate semantic action on their semantic values
	// Assumes that all ambiguities in semantic values have been previously resolved
	inline YYRESULTTAG action(size_t k, yyRuleNum rule, YYSTYPE* yyvalp) {
		int n = yyrhsLength(rule);

		if (!split_point) {
			// Standard special case: single stack
			YYASSERT(k == 0);
			yyGLRStackItem* rhs = (yyGLRStackItem*)tops[k];
			next_free	-= n;
			space_left	+= n;
			tops[0]		= &next_free[-1].state;
			return yyuserAction(rule, rhs, yyvalp);

		} else {
			yyGLRStackItem	vals[YYMAXRHS + YYMAXLEFT];
			yyGLRState*		s = tops[k];
			for (yyGLRStackItem	*p = vals + YYMAXRHS + YYMAXLEFT, *e = p - n; --p > e;) {
				p->state.resolved = s->resolved;
				if (s->resolved)
					p->state.semantics.yysval = s->semantics.yysval;
				else
					p->state.semantics.first_val = NULL;
				s = p->state.pred = s->pred;
			}
			update_split(s);
			tops[k] = s;
			return yyuserAction(rule, vals + YYMAXRHS + YYMAXLEFT - 1, yyvalp);
		}
	}

	YYRESULTTAG process1(size_t k, size_t yyposn, int yychar, YYSTYPE yylval);

	// Pop items off stack k according to grammar rule RULE, and push back on the resulting nonterminal symbol.
	// Perform the semantic action associated with RULE and store its value with the newly pushed state if YYFORCEEVAL or if *YYSTACKP is currently unambiguous.
	// Otherwise, store the deferred semantic action with the new state.
	inline YYRESULTTAG reduce(size_t k, yyRuleNum rule, int yychar, YYSTYPE yylval, bool force_eval) {
		size_t yyposn = tops[k]->yyposn;

		if (force_eval || !split_point) {
			YYSTYPE		yysval;
			if (YYRESULTTAG chk = action(k, rule, &yysval)) {
				if (chk == yyerr && split_point) {
					YYDPRINTF((stderr, "Parse on stack %lu rejected by rule #%d.\n", k, rule - 1));
				}
				return chk;
			}
			if (!shift(k, yyLRgotoState(tops[k]->state, yylhsNonterm(rule)), yyposn, &yysval))
				return yyoom;

		} else {
			yyGLRState *s0 = tops[k], *s = s0;
			for (int n = yyrhsLength(rule); n--;)
				s = s->pred;
			update_split(s);
			
			yyStateNum	state = yyLRgotoState(s->state, yylhsNonterm(rule));
			YYDPRINTF((stderr, "Reduced stack %lu by rule #%d; action deferred. Now in state %d.\n", k, rule - 1, state));

			// If the new state would have an identical input position, LR state, and predecessor to an existing state on the stack, it is identified with that existing state, eliminating stack #YYK from *YYSTACKP.
			// In this case, the semantic value is added to the options for the existing state's semantic value.
			for (size_t i = 0; i < tops_size; ++i) {
				if (i != k && tops[i]) {
					yyGLRState* yysplit	= split_point;
					yyGLRState* yyp		= tops[i];
					while (yyp != s && yyp != yysplit && yyp->yyposn >= yyposn) {
						if (yyp->state == state && yyp->pred == s) {
							if (!add_deferred(k, yyp, s0, rule, yychar, yylval))
								return yyoom;
							mark_deleted(k);
							YYDPRINTF((stderr, "Merging stack %lu into stack %lu.\n", k, i));
							return yyok;
						}
						yyp = yyp->pred;
					}
				}
			}
			tops[k] = s;
			if (!shift_defer(k, state, yyposn, s0, rule, yychar, yylval))
				return yyoom;
		}
		return yyok;
	}
};

// Resolve the states for the RHS of YYOPT on *YYSTACKP, perform its user action, and return the semantic value and location in *YYVALP and *YYLOCP. Regardless of whether result = yyok, all RHS states have been destroyed (assuming the user action destroys all RHS semantic values if invoked).
YYRESULTTAG yyGLRStack::resolve_action(yySemanticOption* yyopt, YYSTYPE* yyvalp) {
	int		n = yyrhsLength(yyopt->rule);

	if (YYRESULTTAG chk = resolve_states(yyopt->state, n)) {
		for (yyGLRState* s = yyopt->state; n > 0; s = s->pred, --n)
			s->destroy();
		return chk;
	}

	//recreate end of stack
	yyGLRStackItem	vals[YYMAXRHS + YYMAXLEFT];
	yyGLRState		*s	= yyopt->state;
	for (yyGLRStackItem	*p = vals + YYMAXRHS + YYMAXLEFT, *e = p - n; --p > e;) {
		p->state.resolved = s->resolved;
		if (s->resolved)
			p->state.semantics.yysval = s->semantics.yysval;
		else
			p->state.semantics.first_val = NULL;
		s = p->state.pred = s->pred;
	}

	if (n == 0)
		*yyvalp = yyval_default;
	else
		*yyvalp = vals[YYMAXRHS + YYMAXLEFT - n].state.semantics.yysval;

//	yychar		= yyopt->rawchar;
//	yylval		= yyopt->val;
	return yyuserAction(yyopt->rule, vals + YYMAXRHS + YYMAXLEFT, yyvalp);
}

// Resolve the ambiguity represented in state YYS in *YYSTACKP, perform the indicated actions, and set the semantic value of YYS. If result != yyok, the chain of semantic options in YYS has been cleared instead or it has been left unmodified except that redundant options may have been removed. Regardless of whether result = yyok, YYS has been left with consistent data so that yydestroyGLRState can be invoked if necessary.
YYRESULTTAG yyGLRStack::resolve_value(yyGLRState* yys) {
	yySemanticOption*	best	= yys->semantics.first_val;
	bool				merge	= false;

	for (yySemanticOption**pp = &best->next; *pp;) {
		yySemanticOption* p = *pp;

		if (identical(best, p)) {
			best->merge(p);
			*pp = p->next;

		} else {
			switch (preference(best, p)) {
				case 0:		yyerror(0, ("syntax is ambiguous")); return yyabort;
				case 1:		merge = true; break;
				case 2:		break;
				case 3:		best = p; merge = false; break;
				default:	break;
			}
			pp = &p->next;
		}
	}

	YYRESULTTAG	chk = resolve_action(best, &yys->semantics.yysval);

	if (merge && chk == yyok) {
		int yyprec	= yydprec[best->rule];
		for (yySemanticOption *p = best->next; p; p = p->next) {
			if (yyprec == yydprec[p->rule]) {
				YYSTYPE yysval_other;
				if (chk = resolve_action(p, &yysval_other)) {
					yydestruct(yystos[yys->state], &yys->semantics.yysval);
					break;
				}
				yyuserMerge(yymerger[p->rule], &yys->semantics.yysval, &yysval_other);
			}
		}
	}

	if (chk)
		yys->semantics.first_val = NULL;

	yys->resolved = chk == yyok;
	return chk;
}

YYRESULTTAG yyGLRStack::process1(size_t k, size_t yyposn, int yychar, YYSTYPE yylval) {
	while (tops[k]) {
		yyStateNum state = tops[k]->state;
		YYDPRINTF((stderr, "Stack %lu Entering state %d\n", k, state));
		YYASSERT(state != YYFINAL);

		if (yyisDefaultedState(state)) {
			yyRuleNum	rule = yydefact[state];
			if (rule == 0) {
				YYDPRINTF((stderr, "Stack %lu dies.\n", k));
				mark_deleted(k);
				return yyok;
			}
			if (YYRESULTTAG chk = reduce(k, rule, yychar, yylval, yyimmediate[rule])) {
				if (chk == yyerr) {
					YYDPRINTF((stderr, "Stack %lu dies (predicate failure or explicit user error).\n", k));
					mark_deleted(k);
					return yyok;
				}
				return chk;
			}

		} else {
			yySymbol	token;
			if (yychar <= YYEOF) {
				yychar = token = YYEOF;
				YYDPRINTF((stderr, "Now at end of input.\n"));
			} else {
				token = YYTRANSLATE(yychar);
			}

			const short int*	conflicts;
			int	action = yygetLRActions(state, token, &conflicts);
			while (int c = *conflicts++) {
				size_t	new_stack = split(k);
				YYDPRINTF((stderr, "Splitting off stack %lu from %lu.\n", new_stack, k));
				if (YYRESULTTAG chk = reduce(new_stack, c, yychar, yylval, yyimmediate[c])) {
					if (chk == yyerr) {
						YYDPRINTF((stderr, "Stack %lu dies.\n", new_stack));
						mark_deleted(new_stack);
					} else {
						return chk;
					}
				} else if (chk = process1(new_stack, yyposn, yychar, yylval)) {
					return chk;
				}
			}

			if (action > 0) //shift
				break;

			if (action == 0) {
				YYDPRINTF((stderr, "Stack %lu dies.\n", k));
				mark_deleted(k);
				break;
			}

			if (YYRESULTTAG chk = reduce(k, -action, yychar, yylval, yyimmediate[-action])) {
				if (chk == yyerr) {
					YYDPRINTF((stderr, "Stack %lu dies (predicate failure or explicit user error).\n", k));
					mark_deleted(k);
					break;
				}
				return chk;
			}
		}
	}
	return yyok;
}

static void yyreportSyntaxError(yyGLRStack* yystackp, CG* cg) {
	if (yystackp->yyerrState != 0)
		return;
	yyerror(cg, ("syntax error"));
}

#define YYTERROR 1

// Recover from a syntax error on *YYSTACKP, assuming that *YYSTACKP->YYTOKENP, yylval, and yylloc are the syntactic category, semantic value, and location of the lookahead.
static bool yyrecoverSyntaxError(yyGLRStack* yystackp, int yychar, YYSTYPE yylval, CG* cg) {
	size_t	k;
	int		yyj;

	if (yystackp->yyerrState == 3)
		// We just shifted the error token and (perhaps) took some reductions. Skip tokens until we can proceed.
		while (true) {
			yySymbol token;
			if (yychar == YYEOF)
				return false;

			if (yychar != YYEMPTY) {
				token = YYTRANSLATE(yychar);
				yydestruct(token, &yylval);
			}
			YYDPRINTF((stderr, "Reading a token: "));
			yychar = YYLEX;
			if (yychar <= YYEOF) {
				yychar = token = YYEOF;
				YYDPRINTF((stderr, "Now at end of input.\n"));
			} else {
				token = YYTRANSLATE(yychar);
				
			}
			yyj = yypact[yystackp->tops[0]->state];
			if (yypact_value_is_default(yyj))
				return true;

			yyj += token;
			if ((yyj < 0 || YYLAST < yyj || yycheck[yyj] != token) && yydefact[yystackp->tops[0]->state])
				return true;
		}

	// Reduce to one stack.
	for (k = 0; k < yystackp->tops_size; ++k)
		if (yystackp->tops[k])
			break;

	if (k >= yystackp->tops_size)
		return false;

	for (++k; k < yystackp->tops_size; ++k)
		yystackp->mark_deleted(k);
	yystackp->remove_deletes();

	// if we just became deterministic, compress
	if (yystackp->tops_size == 1 && yystackp->split_point)
		yystackp->compress();

	// Now pop stack until we find a state that shifts the error token.
	yystackp->yyerrState = 3;
	while (yystackp->tops[0]) {
		yyGLRState* yys = yystackp->tops[0];
		yyj				= yypact[yys->state];
		if (!yypact_value_is_default(yyj)) {
			yyj += YYTERROR;
			if (0 <= yyj && yyj <= YYLAST && yycheck[yyj] == YYTERROR && yytable[yyj] > 0) {
				// Shift the error token.
				if (!yystackp->shift(0, yytable[yyj], yys->yyposn, &yylval))
					return false;

				yys = yystackp->tops[0];
				break;
			}
		}
		if (yys->pred)
			yys->destroy();
		yystackp->tops[0] = yys->pred;
		--yystackp->next_free;
		++yystackp->space_left;
	}
	return yystackp->tops[0] != NULL;
}

int yyparse_glr(CG* cg) {
	int			yyresult;
	yyGLRStack	yystack(YYINITDEPTH);
	yyGLRStack*	const yystackp = &yystack;
	size_t		yyposn;
	YYDPRINTF((stderr, "Starting parse\n"));

	int		yychar	= YYEMPTY;
	YYSTYPE	yylval	= yyval_default;
	int		yynerrs	= 0;

	if (!yystack.items)
		goto yyexhaustedlab;

	yystack.shift(0, 0, 0, &yylval);
	yyposn = 0;

	YYRESULTTAG chk;

	while (true) {
		// For efficiency, we have two loops, the first of which is specialized to deterministic operation (single stack, no potential ambiguity).

		// Standard mode
		while (true) {
			yyStateNum		state = yystack.tops[0]->state;
			YYDPRINTF((stderr, "Entering state %d\n", state));

			if (state == YYFINAL)
				goto yyacceptlab;

			if (yyisDefaultedState(state)) {
				yyRuleNum	rule = yydefact[state];
				if (rule == 0) {
					yyreportSyntaxError(&yystack, cg);
					++yynerrs;
					goto yyuser_error;
				}
				if (chk = yystack.reduce(0, rule, yychar, yylval, true))
					goto yychklab;

			} else {
				yySymbol token;
				if (yychar == YYEMPTY) {
					YYDPRINTF((stderr, "Reading a token: "));
					yychar = YYLEX;
				}
				if (yychar <= YYEOF) {
					yychar = token = YYEOF;
					YYDPRINTF((stderr, "Now at end of input.\n"));
				} else {
					token = YYTRANSLATE(yychar);
				}

				const short int*	conflicts;
				int action = yygetLRActions(state, token, &conflicts);
				if (*conflicts != 0)
					break;

				if (action > 0) {//shift
					yychar = YYEMPTY;
					++yyposn;
					if (!yystack.shift(0, action, yyposn, &yylval))
						goto yyexhaustedlab;

					if (yystack.yyerrState)
						--yystack.yyerrState;

				} else if (action == 0) {
					yyreportSyntaxError(&yystack, cg);
					++yynerrs;
					goto yyuser_error;

				} else if (chk = yystack.reduce(0, -action, yychar, yylval, true)) {
					goto yychklab;

				}
			}
		}

		while (true) {
			if (yychar == YYEMPTY) {
				YYDPRINTF((stderr, "Reading a token: "));
				yychar = YYLEX;
			}

			// yyprocessOneStack returns one of three things: 
			// - An error flag. If the caller is yyprocessOneStack, it immediately returns as well. When the caller is finally yyparse, it jumps to an error label via YYCHK1.
			// - yyok, but yyprocessOneStack has invoked yymarkStackDeleted(&yystack, k), which sets the top state of k to NULL. Thus, yyparse's following invocation of yyremoveDeletes will remove the stack.
			// - yyok, when ready to shift a token.

			// Except in the first case, yyparse will invoke yyremoveDeletes and then shift the next token onto all remaining stacks.
			// This synchronization of the shift (that is, after all preceding reductions on all stacks) helps prevent double destructor calls on yylval in the event of memory exhaustion.

			for (size_t k = 0; k < yystack.tops_size; ++k)
				if (chk = yystack.process1(k, yyposn, yychar, yylval))
					goto yychklab;


			yystack.remove_deletes();
			if (yystack.tops_size == 0) {
				YYDPRINTF((stderr, "Restoring last deleted stack as stack #0.\n"));
				if (!yystack.undelete_last()) {
					yyerror(cg, "syntax error");
					goto yyabortlab;
				}

				if (chk = yystack.resolve())
					goto yychklab;

				YYDPRINTF((stderr, "Returning to deterministic operation.\n"));

				yyreportSyntaxError(&yystack, cg);
				++yynerrs;
				goto yyuser_error;
			}

			yySymbol yytoken_to_shift	= YYTRANSLATE(yychar);
			++yyposn;
			for (size_t k = 0; k < yystack.tops_size; ++k) {
				const short int* conflicts;
				yyStateNum		state	= yystack.tops[k]->state;
				int				action	= yygetLRActions(state, yytoken_to_shift, &conflicts);
				// Note that conflicts were handled by process1
				YYDPRINTF((stderr, "On stack %lu, ", k));
				
				if (!yystack.shift(k, action, yyposn, &yylval)) {
					// set to YYEMPTY to make sure the user destructor for yylval isn't called twice
					yychar = YYEMPTY;
					goto yyexhaustedlab;
				}

				YYDPRINTF((stderr, "Stack %lu now in state #%d\n", k, yystack.tops[k]->state));
			}

			if (yystack.tops_size == 1) {
				if (chk = yystack.resolve())
					goto yychklab;

				YYDPRINTF((stderr, "Returning to deterministic operation.\n"));
				yystack.compress();
				break;
			}
		}
		continue;


yychklab:
		switch (chk) {
			default:
			YYASSERT(false);
			case yyabort:
yyabortlab:
				yyresult = 1;
				goto yyreturn;

			case yyaccept:
yyacceptlab:
				yyresult = 0;
				goto yyreturn;

			case yyerr:
yyuser_error:
				if (yyrecoverSyntaxError(&yystack, yychar, yylval, cg)) {
					yyposn = yystack.tops[0]->yyposn;
					continue;
				}
			case yyoom:
yyexhaustedlab:
				yyerror(cg, ("memory exhausted"));
				yyresult = 2;
				goto yyreturn;
		}
	}

yyreturn:
	if (yychar != YYEMPTY)
		yydestruct(YYTRANSLATE(yychar), &yylval);

	// If the stack is well-formed, pop the stack until it is empty, destroying its entries as we go; but free the stack regardless of whether it is well-formed.
	if (yystack.items) {
		if (yyGLRState** states = yystack.tops) {
			for (size_t k = 0, size = yystack.tops_size; k < size; ++k) {
				if (states[k]) {
					while (states[k]) {
						yyGLRState* yys = states[k];
						if (yys->pred)
							yys->destroy();
						states[k] = yys->pred;
						--yystack.next_free;
						++yystack.space_left;
					}
					break;
				}
			}
		}
	}

	return yyresult;
}

#endif