#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cg.h"
#include "support.h"
#include "errors.h"

using namespace cgc;
#include "parser.hpp"

namespace cgc {

static int bindAtom, constAtom, defaultAtom, defineAtom, definedAtom, elseAtom, elifAtom, endifAtom, ifAtom, ifdefAtom, ifndefAtom, includeAtom, lineAtom, pragmaAtom;
static int texunitAtom, undefAtom, __LINE__Atom, __FILE__Atom;


static Scope *macros = 0;
#define MAX_MACRO_ARGS  64

static int ifdepth = 0; // depth of #if nesting -- used to detect invalid #else/#endif
static SourceLoc ifloc; // outermost #if

struct _Block {
	Block			*next;
	size_t			count, max;
	unsigned char	data[1];
	_Block(size_t size) throw() : next(0), count(0), max(size)  {}
};

struct Block : _Block {
	Block(size_t size) : _Block(size)  {}
	static Block *make(size_t size) { return new(sizeof(Block) + size - 1 + 1) Block(size); }

	~Block() {
		for (_Block *p = next, *n; p; p = n) {
			n = p->next;
			delete p;
		}
	}
};

struct BlockOutput {
    Block	*head;
    Block	*b;

	void putch(char v) {
		if (!head)
			head = b = Block::make(256);

		if (b->count >= b->max)
			b = b->next	= Block::make(256);
		b->data[b->count++] = v;
	}
	void put(const void *p, size_t len) {
		if (!head)
			head = b = Block::make(len < 256 ? 256 : len);

		while (len) {
			size_t	remain = b->max - b->count;
			if (remain == 0)
				b = b->next	= Block::make(len < 256 ? 256 : len);
			if (remain > len)
				remain = len;
			memcpy(b->data + b->count, p, remain);
			b->count += remain;
			len		-= remain;
			p		= (char*)p + remain;
		}
	}
	void put(const char *s) {
		put(s, strlen(s));
	}
	BlockOutput() : head(0), b(0) {}
	operator Block*()	{ if (b) b->data[b->count] = 0; return head; }
};

struct BlockInput {
    Block	*b;
	int		offset;

	int getch() {
		int c = -1;
		if (b) {
			c = b->data[offset++];
			if (offset == b->count) {
				b	= b->next;
				offset	= 0;
			}
		}
		return c;
	}
	BlockInput(Block *b) : b(b), offset(0) {}
};

void InitCPP(CG *cg) {
	// Add various atoms needed by the CPP line scanner:
	bindAtom 		= cg->AddAtom("bind");
	constAtom 		= cg->AddAtom("const");
	defaultAtom		= cg->AddAtom("default");
	defineAtom 		= cg->AddAtom("define");
	definedAtom		= cg->AddAtom("defined");
	elifAtom 		= cg->AddAtom("elif");
	elseAtom 		= cg->AddAtom("else");
	endifAtom 		= cg->AddAtom("endif");
	ifAtom 			= cg->AddAtom("if");
	ifdefAtom 		= cg->AddAtom("ifdef");
	ifndefAtom 		= cg->AddAtom("ifndef");
	includeAtom		= cg->AddAtom("include");
	lineAtom 		= cg->AddAtom("line");
	pragmaAtom 		= cg->AddAtom("pragma");
	texunitAtom		= cg->AddAtom("texunit");
	undefAtom 		= cg->AddAtom("undef");
	__LINE__Atom	= cg->AddAtom("__LINE__");
	__FILE__Atom	= cg->AddAtom("__FILE__");
	macros 			= NewScopeInPool(mem_CreatePool(0, 0));
}

void FinalCPP(CG *cg) {
	if (ifdepth)
		SemanticWarning(cg, ifloc, WARNING___CPP_IF_MISMATCH, "if");
	mem_FreePool(macros->pool);
}

static int CPPdefine(CG *cg, YYSTYPE &yylval) {
	MacroSymbol	mac;
	memset(&mac, 0, sizeof(mac));

	int	token = Scan(cg, yylval);
	if (token != IDENT_SY) {
		SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "define");
		return token;
	}
	int	name	= yylval.sc_ident;
	token	= cg->getch();
	if (token == '(') {
		// gather arguments
		int	args[MAX_MACRO_ARGS];
		int	argc = 0;
		do {
			token = Scan(cg, yylval);
			if (argc == 0 && token == ')') break;
			if (token != IDENT_SY) {
				SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "define");
				return token;
			}
			if (argc < MAX_MACRO_ARGS)
				args[argc++] = yylval.sc_ident;
			token = Scan(cg, yylval);
		} while (token == ',');

		if (token != ')') {
			SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "define");
			return token;
		}

		mac.argc = argc;
		mac.args = (int*)mem_Alloc(macros->pool, argc * sizeof(int));
		memcpy(mac.args, args, argc * sizeof(int));
		token	= cg->getch();
	}

	mac.name	= name;
	BlockOutput	out;
	while (token != '\n') {
		while (token == '\\') {
			token = cg->getch();
			if (token == '\n')
				token = cg->getch();
			else
				out.putch('\\');
		}
		out.putch(token);
		token = cg->getch();
	};
	mac.body	= out;

	Symbol *symb = LookUpSymbol(cg, macros, name);
	if (symb) {
		if (!symb->mac.undef) {
			// already defined -- need to make sure they are identical
			bool	same = symb->mac.argc == mac.argc;

			for (int argc = 0; same && argc < mac.argc; argc++)
				same = symb->mac.args[argc] == mac.args[argc];

			if (same) {
				BlockInput	b0(symb->mac.body);
				BlockInput	b1(mac.body);
				do {
					token = b0.getch();
					same = token == b1.getch();
				} while (token > 0);
			}
			if (!same)
				SemanticWarning(cg, cg->tokenLoc, WARNING___CPP_MACRO_REDEFINED, cg->GetAtomString(name));

		}
		FreeMacro(&symb->mac);
	} else {
		SourceLoc	dummyLoc;
		symb = AddSymbol(cg, dummyLoc, macros, name, 0, MACRO_S);
	}
	symb->mac = mac;
	return '\n';
}

static int CPPundef(CG *cg, YYSTYPE &yylval) {
	int token = Scan(cg, yylval);
	Symbol *symb;

	if (token != IDENT_SY) goto error;
	symb = LookUpSymbol(cg, macros, yylval.sc_ident);
	if (symb) {
		symb->mac.undef = 1;
	}
	token = Scan(cg, yylval);
	if (token != '\n') {
	error:
		SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "undef");
	}
	return token;
}

static int CPPif(CG *cg, YYSTYPE &yylval);

// CPPelse -- skip forward to appropriate spot.  This is actually used to skip to and #endif after seeing an #else, AND to skip to a #else, #elif, or #endif after a #if/#ifdef/#ifndef/#elif test was false
static int CPPelse(CG *cg, YYSTYPE &yylval, bool matchelse) {
	int atom, depth = 0;
	int token = Scan(cg, yylval);
	while (token > 0) {
		while (token != '\n')
			token = Scan(cg, yylval);
		if ((token = Scan(cg, yylval)) != '#')
			continue;
		if ((token = Scan(cg, yylval)) != IDENT_SY)
			continue;
		atom = yylval.sc_ident;
		if (atom == ifAtom || atom == ifdefAtom || atom == ifndefAtom)
			depth++;
		else if (atom == endifAtom) {
			if (--depth < 0) {
				if (ifdepth) ifdepth--;
				break;
			}
		} else if (matchelse && depth == 0) {
			if (atom == elseAtom)
				break;
			else if (atom == elifAtom) {
				/* we decrement ifdepth here, because CPPif will increment
				 * it and we really want to leave it alone */
				if (ifdepth) ifdepth--;
				return CPPif(cg, yylval);
			}
		}
	};
	return token;
}

enum eval_prec {
	MIN_PREC,
	COND, LOGOR, LOGAND, OR, XOR, AND, EQUAL, RELATION, SHIFT, ADD, MUL, UNARY,
	MAX_PREC
};

struct {
	int token, prec, (*op)(int, int);
} binop[] = {
	{ OR_SY, 	LOGOR, 		[](int a, int b)->int { return a || b; } },
	{ AND_SY, 	LOGAND, 	[](int a, int b)->int { return a && b; } },
	{ '|', 		OR, 		[](int a, int b)->int { return a |  b; } },
	{ '^', 		XOR, 		[](int a, int b)->int { return a ^  b; } },
	{ '&', 		AND, 		[](int a, int b)->int { return a &  b; } },
	{ EQ_SY, 	EQUAL, 		[](int a, int b)->int { return a == b; } },
	{ NE_SY, 	EQUAL, 		[](int a, int b)->int { return a != b; } },
	{ '>', 		RELATION, 	[](int a, int b)->int { return a >  b; } },
	{ GE_SY, 	RELATION, 	[](int a, int b)->int { return a >= b; } },
	{ '<', 		RELATION, 	[](int a, int b)->int { return a <  b; } },
	{ LE_SY, 	RELATION, 	[](int a, int b)->int { return a <= b; } },
	{ LL_SY, 	SHIFT, 		[](int a, int b)->int { return a << b; } },
	{ GG_SY, 	SHIFT, 		[](int a, int b)->int { return a >> b; } },
	{ '+', 		ADD, 		[](int a, int b)->int { return a +  b; } },
	{ '-', 		ADD, 		[](int a, int b)->int { return a -  b; } },
	{ '*', 		MUL, 		[](int a, int b)->int { return a *  b; } },
	{ '/', 		MUL, 		[](int a, int b)->int { return a /  b; } },
	{ '%', 		MUL, 		[](int a, int b)->int { return a %  b; } },
};

struct {
	int token, (*op)(int);
} unop[] = {
	{ '+', [](int a)->int { return a;  } },
	{ '-', [](int a)->int { return -a; } },
	{ '~', [](int a)->int { return ~a; } },
	{ '!', [](int a)->int { return !a; } },
};

#define ALEN(A) (sizeof(A)/sizeof(A[0]))

int eval(CG *cg, YYSTYPE &yylval, int token, int prec, int *res, int *err) {
	int         i, val;
	Symbol      *s;

	if (token == IDENT_SY) {
		if (yylval.sc_ident == definedAtom) {
			token = Scan(cg, yylval);

			bool	needclose = token == '(';
			if (needclose)
				token = Scan(cg, yylval);

			if (token != IDENT_SY)
				goto error;
			
			*res = (s = LookUpSymbol(cg, macros, yylval.sc_ident)) ? !s->mac.undef : 0;
			token = Scan(cg, yylval);
			if (needclose) {
				if (token != ')')
					goto error;
				token = Scan(cg, yylval);
			}
		} else if (token = MacroExpand(cg, yylval)) {
			return eval(cg, yylval, token, prec, res, err);
			
		} else {
			*res = 0;
			token = Scan(cg, yylval);
		}
	} else if (token == INTCONST_SY) {
		*res = yylval.sc_int;
		token = Scan(cg, yylval);
		
	} else if (token == '(') {
		token = Scan(cg, yylval);
		token = eval(cg, yylval, token, MIN_PREC, res, err);
		if (!*err) {
			if (token != ')')
				goto error;
			token = Scan(cg, yylval);
		}
	} else {
		for (i = ALEN(unop) - 1; i >= 0; i--) {
			if (unop[i].token == token)
				break;
		}
		if (i >= 0) {
			token 	= Scan(cg, yylval);
			token 	= eval(cg, yylval, token, UNARY, res, err);
			*res 	= unop[i].op(*res);
		} else {
			goto error;
		}
	}
	while (!*err) {
		if (token == ')' || token == '\n') break;
		for (i = ALEN(binop) - 1; i >= 0; i--) {
			if (binop[i].token == token)
				break;
		}
		if (i < 0 || binop[i].prec <= prec)
			break;
		val 	= *res;
		token 	= Scan(cg, yylval);
		token 	= eval(cg, yylval, token, binop[i].prec, res, err);
		*res 	= binop[i].op(val, *res);
	}
	return token;
error:
	SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "if");
	*err = 1;
	*res = 0;
	return token;
}

static int CPPif(CG *cg, YYSTYPE &yylval) {
	int token = Scan(cg, yylval);

	if (!ifdepth++)
		ifloc = cg->tokenLoc;
	
	int res = 0, err = 0;
	token = eval(cg, yylval, token, MIN_PREC, &res, &err);

	if (token != '\n') {
		SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "if");
	} else if (!res && !err) {
		token = CPPelse(cg, yylval, true);
	}
	return token;
}

static int CPPifdef(CG *cg, YYSTYPE &yylval, bool defined) {
	int token 	= Scan(cg, yylval);
	int name 	= yylval.sc_ident;
	ifdepth++;
	if (token != IDENT_SY) {
		SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, defined ? "ifdef" : "ifndef");
	} else {
		Symbol *s = LookUpSymbol(cg, macros, name);
		if (((s && !s->mac.undef) ? 1 : 0) != defined)
			token = CPPelse(cg, yylval, true);
	}
	return token;
}

static int CPPinclude(CG *cg, YYSTYPE &yylval) {
	int file 	= 0;
	int	tok 	= Scan(cg, yylval, true);

	if (tok == STRCONST_SY) {
		file = yylval.sc_ident;
	} else if (tok == '<') {
		char buf[MAX_STRING_LEN + 1];
		int len = 0, ch;
		while ((ch = cg->getch()) > 0 && ch != '\n' && ch != '>') {
			if (len < MAX_STRING_LEN)
				buf[len++] = ch;
		}
		buf[len] = 0;
		if (ch == '\n')
			cg->ungetch(ch);
		file = cg->AddAtom(buf);
	} else {
		SemanticError(cg, cg->tokenLoc, ERROR___CPP_SYNTAX, "include");
	}
	while (tok != '\n')
		tok = Scan(cg, yylval);

	const char		*fname	= cg->GetAtomString(file);
	cgclib::Blob	blob	= cg->includer->open(fname);
	if (!blob) {
		SemanticError(cg, cg->tokenLoc, ERROR___CPP_INCLUDE, fname);
	} else {
		ScanFromBlob(cg, blob, blob.size, cg->AddAtom(fname));
	}
	return '\n';
}

static int CPPline(CG *cg, YYSTYPE &yylval, int token) {
	if (token == INTCONST_SY) {
		int line = yylval.sc_int;
		token = Scan(cg, yylval);
		if (token == STRCONST_SY) {
			cg->currentInput->file = yylval.sc_ident;
			cg->currentInput->line = line - 1; // Will get bumped by one.
			token = Scan(cg, yylval);
		}
	}
	return token;
}

static inline int NextToken(CG *cg, YYSTYPE &yylval) {
	int token = Scan(cg, yylval);
	if (token == IDENT_SY) {
		if (int exp = MacroExpand(cg, yylval))
			token = exp;
	}
	return token;
}

static int CPPpragma(CG *cg, YYSTYPE &yylval) {
	int	token = NextToken(cg, yylval);

	if (token == IDENT_SY && yylval.sc_ident == bindAtom) {

		// Parse:  # "pragma" "bind" <conn-id> "." <memb-id> "=" <reg-id>
		//         # "pragma" "bind" <prog-id> "." <parm-id> "=" <reg-id> <i-const>
		//         # "pragma" "bind" <prog-id> "." <parm-id> "=" "texunit" <i-const>
		//         # "pragma" "bind" <prog-id> "." <memb-id> "=" "const" <number>+
		//         # "pragma" "bind" <prog-id> "." <memb-id> "=" "default" <number>+
		//
		//  <number> ::= [ "-" ] ( <i-const> | <f-const> )

		bool	ok		= true;
		int		identa, identb, identc;

		token = NextToken(cg, yylval);
		if (ok = token == IDENT_SY) {
			identa	= yylval.sc_ident;
			token	= NextToken(cg, yylval);

			if (ok = token == '.')
				token = NextToken(cg, yylval);

			if (ok = (ok && token == IDENT_SY)) {
				identb = yylval.sc_ident;
				token = NextToken(cg, yylval);
				if (ok = ok && token == '=')
					token = NextToken(cg, yylval);

				if (ok = ok && (token == IDENT_SY)) {
					identc	= yylval.sc_ident;
					token	= NextToken(cg, yylval);
				}

				float	fval[4];
				int		numfvals	= 0;
				int		ival		= 0;
				bool	has_ival	= false;
				while (ok && (token == INTCONST_SY || token == CFLOATCONST_SY || token == '-')) {
					bool	neg = token == '-';
					if (neg)
						token = NextToken(cg, yylval);

					if (token == INTCONST_SY) {
						if (numfvals == 0 && !neg) {
							ival	= yylval.sc_int;
							has_ival	= true;
						}
						if (ok = numfvals < 4) {
							fval[numfvals] = (float)(neg ? -yylval.sc_int : yylval.sc_int);
							numfvals++;
						}
						token = NextToken(cg, yylval);

					} else if (token == CFLOATCONST_SY) {
						if (ok = numfvals < 4) {
							fval[numfvals] = neg ? -yylval.sc_fval : yylval.sc_fval;
							numfvals++;
						}
						token = NextToken(cg, yylval);

					} else {
						ok = false;
					}
				}
				if (ok) {
					MemoryPool	*pool = cg->pool();
					if (BindingTree *tree = cg->LookupBinding(identa, identb)) {
						SemanticError(cg, cg->tokenLoc, ERROR_SSSD_DUPLICATE_BINDING,
							cg->GetAtomString(identa), cg->GetAtomString(identb),
							cg->GetAtomString(tree->loc.file), tree->loc.line
						);
					} else if (identc == texunitAtom) {
						if (ok = (has_ival && numfvals == 1))
							cg->AddBinding(NewTexunitBindingTree(pool, cg->tokenLoc, identa, identb, ival));
					} else if (identc == constAtom) {
						if (ok = numfvals > 0)
							cg->AddBinding(NewConstDefaultBindingTree(pool, cg->tokenLoc, BK_CONSTANT, identa, identb, numfvals, fval));
					} else if (identc == defaultAtom) {
						if (ok = numfvals > 0)
							cg->AddBinding(NewConstDefaultBindingTree(pool, cg->tokenLoc, BK_DEFAULT, identa, identb, numfvals, fval));
					} else if (has_ival) {
						cg->AddBinding(NewRegArrayBindingTree(pool, cg->tokenLoc, identa, identb, identc, ival, 0));
					}
				}
			}
		}
		if (!ok)
			SemanticError(cg, cg->tokenLoc, ERROR___CPP_BIND_PRAGMA_ERROR);
	}
	return token;
}

void readCPPline(CG *cg, YYSTYPE &yylval) {
	int token = Scan(cg, yylval);
	if (token == IDENT_SY) {
		if (yylval.sc_ident == defineAtom) {
			token = CPPdefine(cg, yylval);
		} else if (yylval.sc_ident == elseAtom) {
			if (!ifdepth)
				SemanticWarning(cg, cg->tokenLoc, WARNING___CPP_IF_MISMATCH, "else");
			token = CPPelse(cg, yylval, false);
		} else if (yylval.sc_ident == elifAtom) {
			if (!ifdepth)
				SemanticWarning(cg, cg->tokenLoc, WARNING___CPP_IF_MISMATCH, "elif");
			token = CPPelse(cg, yylval, false);
		} else if (yylval.sc_ident == endifAtom) {
			if (!ifdepth)
				SemanticWarning(cg, cg->tokenLoc, WARNING___CPP_IF_MISMATCH, "endif");
			else
				ifdepth--;
		} else if (yylval.sc_ident == ifAtom) {
			token = CPPif(cg, yylval);
		} else if (yylval.sc_ident == ifdefAtom) {
			token = CPPifdef(cg, yylval, true);
		} else if (yylval.sc_ident == ifndefAtom) {
			token = CPPifdef(cg, yylval, false);
		} else if (yylval.sc_ident == includeAtom) {
			token = CPPinclude(cg, yylval);
		} else if (yylval.sc_ident == lineAtom) {
			token = CPPline(cg, yylval, Scan(cg, yylval));
		} else if (yylval.sc_ident == pragmaAtom) {
			token = CPPpragma(cg, yylval);
		} else if (yylval.sc_ident == undefAtom) {
			token = CPPundef(cg, yylval);
		} else {
			SemanticError(cg, cg->tokenLoc, ERROR___CPP_UNKNOWN_DIRECTIVE, cg->GetAtomString(yylval.sc_ident));
		}
	} else if (token == INTCONST_SY) {
		token = CPPline(cg, yylval, token);
	}
	while (token != '\n' && token != 0 /* EOF */) {
		token = Scan(cg, yylval);
	}
}

void FreeMacro(MacroSymbol *s) {
	delete s->body;
}

struct MacroInputSrc : InputSrc, BlockInput {
	static MacroInputSrc	*top;

	MacroSymbol		*mac;
	Block			**args;
	MacroInputSrc	*next;
	char			stay_alive;

	int getch(CG *cg) {
		int c = BlockInput::getch();
		if (c > 0) {
			top	= this;
			return c;
		}

		cg->currentInput = prev;
		c = prev->getch(cg);
		if (--stay_alive == 0) {
			delete this;
		} else {
			prev = cg->currentInput;
			cg->currentInput	= this;
			top					= this;
		}
		return c;
	}

	MacroInputSrc(MacroSymbol *_mac, Block *b) : InputSrc(this), BlockInput(b), mac(_mac), stay_alive(2) {
		args	= mac && mac->args ? new Block*[mac->argc] : 0;
		if (mac)
			mac->busy = 1;
	}

	~MacroInputSrc() {
		if (mac)
			mac->busy = 0;
		delete[] args;
		top = 0;
	}
};

MacroInputSrc *MacroInputSrc::top = 0;

Block *FindMacroArg(int atom) {
	if (MacroInputSrc::top && MacroInputSrc::top->mac) {
		MacroSymbol		*mac	= MacroInputSrc::top->mac;
		Block			**arg	= 0;
		for (int i = 0, n = mac->argc; i < n; i++) {
			if (atom == mac->args[i])
				return MacroInputSrc::top->args[i];
		}
	}
	return 0;
}

// Check an identifier (atom) to see if it's a macro that should be expanded.
// If it is, push an InputSrc that will produce the appropriate expansion and loop
int MacroExpand(CG *cg, YYSTYPE &yylval) {
	bool	expanded	= false;
	int		token		= IDENT_SY;

	while (token == IDENT_SY) {
		int	atom	= yylval.sc_ident;

		int		ch	= cg->getch();
		bool	sp	= false;
		while (ch == ' ' || ch == '\t' || ch == '\r') {
			ch = cg->getch();
			sp = true;
		}

		if (ch == '#') {
			ch = cg->getch();
			if (ch == '#') {
				char	symbol_name[MAX_SYMBOL_NAME_LEN + 1];

				ch = cg->getch();
				while (ch == ' ' || ch == '\t' || ch == '\r')
					ch = cg->getch();
				
				size_t	len;
				if (Block *arg = FindMacroArg(atom)) {
					len = arg->count;
					if (len > MAX_SYMBOL_NAME_LEN)
						len = MAX_SYMBOL_NAME_LEN;
					memcpy(symbol_name, arg->data, len);
					symbol_name[len] = 0;
				} else {
					strcpy(symbol_name, cg->GetAtomString(atom));
					len	= strlen(symbol_name);
				}

				size_t	len0 = len;
				do {
					if (len < MAX_SYMBOL_NAME_LEN) {
						symbol_name[len++] = ch;
						ch = cg->getch();
					}
				} while ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_');

				symbol_name[len] = 0;
				cg->ungetch(ch);

				if (Block *arg = FindMacroArg(cg->AddAtom(symbol_name + len0))) {
					size_t	len2 = arg->count;
					if (len2 > MAX_SYMBOL_NAME_LEN - len0)
						len2 = MAX_SYMBOL_NAME_LEN - len0;
					memcpy(symbol_name + len0, arg->data, len2);
					symbol_name[len0 + len2] = 0;
				}

				yylval.sc_ident = cg->AddAtom(symbol_name);
				expanded		= true;
				continue;
			}
		}
		cg->ungetch(ch);
		if (sp)
			cg->ungetch(' ');

		if (atom == __LINE__Atom) {
			yylval.sc_int = cg->currentInput->line;
			return INTCONST_SY;
		}
		if (atom == __FILE__Atom) {
			yylval.sc_ident = cg->currentInput->file;
			return STRCONST_SY;
		}

		if (Block *arg = FindMacroArg(atom)) {
			MacroInputSrc	*in = new MacroInputSrc(0, arg);
			SourceLoc		loc	= cg->tokenLoc;
			in->line	= cg->currentInput->line;
			in->file	= cg->currentInput->file;
			cg->PushInput(in);
			token		= Scan(cg, yylval);
			expanded	= true;
			continue;
		}

		Symbol	*sym = LookUpSymbol(cg, macros, atom);
		if (!sym || sym->mac.undef || sym->mac.busy)
			return expanded ? IDENT_SY : 0;

		expanded	= true;

		if (sym->mac.args) {
			token = Scan(cg, yylval);
			if (token != '(') {
				yylval.sc_ident = atom;
				return IDENT_SY;
			}
		}

		MacroInputSrc	*in = new MacroInputSrc(&sym->mac, sym->mac.body);
		SourceLoc		loc	= cg->tokenLoc;
		in->line	= cg->currentInput->line;
		in->file	= cg->currentInput->file;

		if (sym->mac.args) {
			int	i, depth = 0;
			for (i = 0; i < in->mac->argc; i++) {
				BlockOutput	out;
				char		symbol_name[MAX_SYMBOL_NAME_LEN + 1];
				int			len	= 0;
				for (;;) {
					ch = cg->getch();
					if (ch <= 0) {
						SemanticError(cg, loc, ERROR___CPP_MACRO_EOF, cg->GetAtomString(atom));
						return 1;
					}
					if ((ch == ' ' || ch == '\t') && len == 0 && !out.head)
						continue;
					if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
						symbol_name[len++] = ch;
					} else {
						if (len) {
							symbol_name[len] = 0;
							yylval.sc_ident = cg->AddAtom(symbol_name);
							cg->ungetch(ch);
							if (int token = MacroExpand(cg, yylval)) {
								out.put(GetTokenString(cg, yylval, token));
							} else {
								out.put(symbol_name, len);
							}
							len = 0;
						} else {
							if (depth == 0 && (ch == ',' || ch == ')'))
								break;
							if (ch == '(') depth++;
							if (ch == ')') depth--;
							out.putch(ch);
						}
					}
				}
				in->args[i] = out;
				if (ch == ')') {
					i++;
					break;
				}
			}
			if (i < in->mac->argc) {
				SemanticError(cg, loc, ERROR___CPP_MACRO_TOOFEW, cg->GetAtomString(atom));

			} else if (ch != ')') {
				while (ch >= 0 && (depth > 0 || ch != ')')) {
					if (ch == ')') depth--;
					ch = cg->getch();
					if (ch == '(') depth++;
				}
				if (ch <= 0) {
					SemanticError(cg, loc, ERROR___CPP_MACRO_EOF, cg->GetAtomString(atom));
					return IDENT_SY;
				}
				SemanticError(cg, loc, ERROR___CPP_MACRO_TOOMANY, cg->GetAtomString(atom));
			}
		}

		cg->PushInput(in);
		token = Scan(cg, yylval);
	}
	return token;
}

bool PredefineMacro(CG *cg, const char *def) {
	char		name[256], *p = name;

	while (isalnum(*def) || *def == '_')
		*p++ = *def++;
	*p = 0;

	if (*def != 0) {
		if (*def == '=')
			def++;
		else
			return false;
	}

	MacroSymbol mac;
	memset(&mac, 0, sizeof(mac));
	mac.name	= cg->AddAtom(name);
	
	BlockOutput	out;
	while (char c = *def++)
		out.putch(c);
	
	mac.body	= out;

	Symbol	*symb = LookUpSymbol(cg, macros, mac.name);
	if (symb) {
		FreeMacro(&symb->mac);
	} else {
		SourceLoc	dummyLoc;
		memset(&dummyLoc, 0, sizeof(dummyLoc));
		symb = AddSymbol(cg, dummyLoc, macros, mac.name, 0, MACRO_S);
	}
	symb->mac = mac;
	return true;
}

} //namespace cgc
