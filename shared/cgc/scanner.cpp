#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define isinff(x) (((*(long*)&(x) & 0x7f800000L)==0x7f800000L) && ((*(long*)&(x) & 0x007fffffL)==0000000000L))

#include "scanner.h"
#include "cg.h"
#include "errors.h"

using namespace cgc;
#include "parser.hpp"

namespace cgc {

struct FileInputSrc : InputSrc {
	FILE	*fd;

	int		getch(CG *cg) {
		int ch;
		ch = getc(fd);
		if (ch == EOF) {
			cg->currentInput = prev;
			fclose(fd);
			delete this;
			return '\n';
		}
		if (ch == '\n') {
			line++;
			col = 0;
		}
		++col;
		return ch;
	}

	FileInputSrc(FILE *_fd, int _name) : InputSrc(this), fd(_fd) {
		file = _name;
	}
};

int ScanFromFile(CG *cg, const char *fname) {
	cg->PushInput(new FileInputSrc(fopen(fname, "r"), cg->AddAtom(fname)));
	return 1;
}

struct BlobInputSrc : InputSrc {
	const char	*p, *e;

	int		getch(CG *cg) {
		char	c;
		int		char13 = 0;
		do {
			if (p == e) {
				cg->currentInput = prev;
				delete this;
				return '\n';
			}
			c = *p++;
			++char13;
		} while (c == '\r');

		if (char13 > 1) {
			line += char13 - 2;
			if (c != '\n') {
				--p;
				c = '\n';
			}
		}
		if (c == '\n') {
			line++;
			col = 0;
		}
		++col;
		return c;
	}

	BlobInputSrc(const char *start, size_t size, int _name) : InputSrc(this), p(start), e(p + size) {
		file = _name;
	}
};

int ScanFromBlob(CG *cg, const char *start, size_t size, int name) {
	cg->PushInput(new BlobInputSrc(start, size, name));
	return 1;
}

// Called by yyparse on an error:
void yyerror(CG *cg, const char *s) {
	cg->error(cg->tokenLoc, 0, CG::ERROR, "%s at token \"%s\"\n", s, cg->GetAtomString(cg->last_token));
	cg->allow_semantic = true;
}

void SemanticParseError(CG *cg, SourceLoc &loc, int num, const char *mess, ...) {
	if (cg->allow_semantic) {
		va_list args;
		va_start(args, mess);
		cg->error(loc, num, CG::ERROR, mess, args);
		cg->allow_semantic = false;
	}
}

void SemanticError(CG *cg, SourceLoc &loc, int num, const char *mess, ...) {
	va_list args;
	va_start(args, mess);
	cg->error(loc, num, CG::ERROR, mess, args);
}

void InternalError(CG *cg, SourceLoc &loc, int num, const char *mess, ...) {
	va_list args;
	va_start(args, mess);
	cg->error(loc, num, CG::ERROR, mess, args);
}

// Fatal internal compiler error.
void FatalError(CG *cg, const char *mess, ...) {
	va_list args;
	va_start(args, mess);
	cg->error(cg->tokenLoc, 9999, CG::FATAL, mess, args);
	throw("fatal");
}

// Compiler generated semantic warning
void SemanticWarning(CG *cg, SourceLoc &loc, int num, const char *mess, ...) {
	if (!(cg->opts & cgclib::NO_WARNINGS)) {
		va_list args;
		va_start(args, mess);
		cg->error(loc, num, CG::WARNING, mess, args);
	}
}

// Print a message from the compiler.
void InformationalNotice(CG *cg, SourceLoc &loc, int num, const char *mess, ...) {
	if (!(cg->opts & cgclib::NO_WARNINGS)) {
		va_list args;
		va_start(args, mess);
		cg->error(loc, num, CG::NOTICE, mess, args);
	}
}

/////////////////////////////////// Floating point constants: /////////////////////////////////

// Quick and dirty conversion to floating point.  Since all we need is single precision this should be quite precise.
static float BuildFloatValue(CG *cg, const char *str, int len, int exp) {
	double	val	= 0.0;
	for (int i = 0; i < len; i++)
		val = val * 10.0 + (str[i] - '0');

	if (exp != 0) {
		int		absexp	= exp > 0 ? exp : -exp;
		double	expval	= 1.0f;
		double	ten		= 10.0;
		while (absexp) {
			if (absexp & 1)
				expval *= ten;
			ten *= ten;
			absexp >>= 1;
		}
		if (exp >= 0)
			val *= expval;
		else
			val /= expval;
	}
	float rv = (float)val;
	if (isinff(rv))
		SemanticError(cg, cg->tokenLoc, ERROR___FP_CONST_OVERFLOW);
	return rv;
}

// Scan a floating point constant
// Assumes that the scanner has seen at least one digit, followed by either a decimal '.' or the letter 'e'.
static int FloatConst(CG *cg, YYSTYPE &yylval, char *str, int len, int ch) {
	bool	has_decimal = false;
	int		declen = 0;
	int		exp = 0;
	if (ch == '.') {
		has_decimal = true;
		ch = cg->currentInput->getch(cg);
		while (ch >= '0' && ch <= '9') {
			if (len < MAX_SYMBOL_NAME_LEN) {
				declen++;
				if (len > 0 || ch != '0') {
					str[len] = ch;
					len++;
				}
				ch = cg->currentInput->getch(cg);
			} else {
				SemanticError(cg, cg->tokenLoc, ERROR___FP_CONST_TOO_LONG);
				len = 1;
			}
		}
	}

	// Exponent:

	if (ch == 'e' || ch == 'E') {
		int	exp_sign = 1;
		ch = cg->currentInput->getch(cg);
		if (ch == '+') {
			ch = cg->currentInput->getch(cg);
		} else if (ch == '-') {
			exp_sign = -1;
			ch = cg->currentInput->getch(cg);
		}
		if (ch >= '0' && ch <= '9') {
			while (ch >= '0' && ch <= '9') {
				exp = exp*10 + ch - '0';
				ch = cg->currentInput->getch(cg);
			}
		} else {
			SemanticError(cg, cg->tokenLoc, ERROR___ERROR_IN_EXPONENT);
		}
		exp *= exp_sign;
	}

	yylval.sc_fval = len == 0 ? 0 : BuildFloatValue(cg, str, len, exp - declen);

	// Suffix:
	switch (ch) {
		case 'h': return FLOATHCONST_SY;
		case 'x': return FLOATXCONST_SY;
		case 'f': return FLOATCONST_SY;
		default:
			cg->ungetch(ch);
			return CFLOATCONST_SY;
	}
}

///////////////////////////////////////// Normal Scanner //////////////////////////////////////

int IntSuffix(CG *cg, int ch) {
	int	u = 0, l = 0;
	for (;;) {
		switch (ch) {
			case 'u': case 'U': ++u; break;
			case 'l': case 'L': ++l; break;
			default:
				cg->ungetch(ch);
				return u ? INTCONST_SY : INTCONST_SY;
		}
		ch = cg->getch();
	}
}

int Scan(CG *cg, YYSTYPE &yylval, bool keep_backslashes) {
	char	symbol_name[MAX_SYMBOL_NAME_LEN + 1];
	char	string_val[MAX_STRING_LEN + 1];

	for (;;) {
		yylval.sc_int = 0;

		int	ch = cg->getch();
		while (ch == ' ' || ch == '\t') {
			yylval.sc_int = 1;
			ch = cg->getch();
		}
		cg->tokenLoc = *cg->currentInput;

		switch (ch) {
			default:
				return ch; // Single character token

			case EOF:
				return 0;

			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
			case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z': case '_':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
			case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x': case 'y': case 'z': {
				int	len = 0;
				do {
					if (len < MAX_SYMBOL_NAME_LEN)
						symbol_name[len++] = ch;
					ch = cg->getch();
				} while ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_');

				symbol_name[len] = '\0';
				cg->ungetch(ch);
				yylval.sc_ident = cg->AddAtom(symbol_name);
				return IDENT_SY;
			}
			case '0':
				ch = cg->getch();
				if (ch == 'x' || ch == 'X') {
					bool	already_complained = false;
					int		ival = 0;
					ch = cg->getch();
					if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
						do {
							if (ival <= 0x0fffffff) {
								int	i = ch >= '0' && ch <= '9'	? ch - '0'
									:	ch >= 'A' && ch <= 'F'	? ch - 'A' + 10
									:	ch - 'a' + 10;
								ival = (ival << 4) | i;
							} else if (!already_complained) {
								SemanticError(cg, cg->tokenLoc, ERROR___HEX_CONST_OVERFLOW);
								already_complained = true;
							}
							ch = cg->getch();
						} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'));
					} else {
						SemanticError(cg, cg->tokenLoc, ERROR___ERROR_IN_HEX_CONSTANT);
					}
					yylval.sc_int = ival;
					return IntSuffix(cg, ch);

				}
				if (ch >= '0' && ch <= '7') { // octal integer constants
					bool	already_complained = false;
					int		ival = 0;
					do {
						if (ival <= 0x1fffffff) {
							ival = (ival << 3) | (ch - '0');
						} else if (!already_complained) {
							SemanticError(cg, cg->tokenLoc, ERROR___OCT_CONST_OVERFLOW);
							already_complained = true;
						}
						ch = cg->getch();
					} while (ch >= '0' && ch <= '7');
					yylval.sc_int = ival;
					return IntSuffix(cg, ch);
				}
				// Fall through...
			case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
				int	len = 0;
				while (ch >= '0' && ch <= '9') {
					if (len < MAX_SYMBOL_NAME_LEN) {
						if (len > 0 || ch != '0')
							symbol_name[len++] = ch;
						ch = cg->getch();
					}
				} 
				if (ch == '.' || ch == 'e' || ch == 'f' || ch == 'h' || ch == 'x')
					return FloatConst(cg, yylval, symbol_name, len, ch);

				int		type = IntSuffix(cg, ch);
				int		ival = 0;
				bool	already_complained = false;
				for (int i = 0; i < len; i++) {
					ch = symbol_name[i] - '0';
					if (ival > 214748364 || ival == 214748364 && ch >= 8 && !already_complained) {
						SemanticError(cg, cg->tokenLoc, ERROR___INTEGER_CONST_OVERFLOW);
						already_complained = true;
					}
					ival = ival * 10 + ch;
				}
				yylval.sc_int = ival;
				return type;
			}
			case '#':
				ch = cg->getch();
				if (ch == '#')
					return PASTING_SY;
				cg->ungetch(ch);
				return '#';

			case '-':
				ch = cg->getch();
				if (ch == '-')
					return MINUSMINUS_SY;
				if (ch == '=')
					return ASSIGNMINUS_SY;
				cg->ungetch(ch);
				return '-';

			case '+':
				ch = cg->getch();
				if (ch == '+')
					return PLUSPLUS_SY;
				if (ch == '=')
					return ASSIGNPLUS_SY;
				cg->ungetch(ch);
				return '+';

			case '*':
				ch = cg->getch();
				if (ch == '=')
					return ASSIGNSTAR_SY;
				cg->ungetch(ch);
				return '*';

			case '%':
				ch = cg->getch();
				if (ch == '=')
					return ASSIGNMOD_SY;
				cg->ungetch(ch);
				return '%';

			case ':':
				ch = cg->getch();
				if (ch == ':')
					return COLONCOLON_SY;
				cg->ungetch(ch);
				return ':';

			case '=':
				ch = cg->getch();
				if (ch == '=')
					return EQ_SY;
				cg->ungetch(ch);
				return '=';

			case '!':
				ch = cg->getch();
				if (ch == '=')
					return NE_SY;
				cg->ungetch(ch);
				return '!';

			case '|':
				ch = cg->getch();
				if (ch == '|')
					return OR_SY;
				if (ch == '=')
					return ASSIGNOR_SY;
				cg->ungetch(ch);
				return '|';

			case '&':
				ch = cg->getch();
				if (ch == '&')
					return AND_SY;
				if (ch == '=')
					return ASSIGNAND_SY;
				cg->ungetch(ch);
				return '&';

			case '^':
				ch = cg->getch();
				if (ch == '=')
					return ASSIGNXOR_SY;
				cg->ungetch(ch);
				return '^';

			case '<':
				ch = cg->getch();
				if (ch == '<')
					return LL_SY;
				if (ch == '=')
					return LE_SY;
				cg->ungetch(ch);
				return '<';

			case '>':
				ch = cg->getch();
				if (ch == '>')
					return GG_SY;
				if (ch == '=')
					return GE_SY;
				cg->ungetch(ch);
				return '>';

			case '.':
				ch = cg->getch();
				if (ch >= '0' && ch <= '9') {
					cg->ungetch(ch);
					return FloatConst(cg, yylval, symbol_name, 0, '.');
				}
				if (ch == '.')
					return -1; // Special EOF hack
				cg->ungetch(ch);
				return '.';

			case '/':
				ch = cg->getch();
				if (ch == '/') {
					do {
						ch = cg->getch();
					} while (ch != '\n' && ch != EOF);
					if (ch == EOF)
						return -1;
					return '\n';

				} else if (ch == '*') {
					int nlcount = 0;
					ch = cg->getch();
					do {
						while (ch != '*') {
							if (ch == '\n') nlcount++;
							if (ch == EOF) {
								SemanticError(cg, cg->tokenLoc, ERROR___EOF_IN_COMMENT);
								return -1;
							}
							ch = cg->getch();
						}
						ch = cg->getch();
						if (ch == EOF) {
							SemanticError(cg, cg->tokenLoc, ERROR___EOF_IN_COMMENT);
							return -1;
						}
					} while (ch != '/');

					if (nlcount)
						return '\n';
					// Go try it again...
				} else if (ch == '=') {
					return ASSIGNSLASH_SY;
				} else {
					cg->ungetch(ch);
					return '/';
				}
				break;

			case '"': {
				int	len = 0;
				ch = cg->getch();
				while (ch != '"' && ch != '\n' && ch != EOF) {
					if (ch == '\\' && !keep_backslashes) {
						ch = cg->getch();
						if (ch == '\n' || ch == EOF)
							break;
					}
					if (len < MAX_STRING_LEN) {
						string_val[len] = ch;
						len++;
						ch = cg->getch();
					}
				};
				string_val[len] = '\0';
				if (ch == '"') {
					yylval.sc_ident = cg->AddAtom(string_val);
					return STRCONST_SY;
				} else {
					SemanticError(cg, cg->tokenLoc, ERROR___CPP_EOL_IN_STRING);
					return ERROR_SY;
				}
			}
		}
	}
}

const char *GetTokenString(CG *cg, YYSTYPE &yylval, int token) {
	static char buffer[64];
	if (token < 256) {
		buffer[0] = (char)token;
		buffer[1] = 0;
		return buffer;
	}
	switch (token) {
		case FLOATCONST_SY:
		case FLOATHCONST_SY:
		case FLOATXCONST_SY:
			sprintf(buffer, "%f", yylval.sc_fval);
			return buffer;
		case INTCONST_SY:
			sprintf(buffer, "%i", yylval.sc_int);
			return buffer;
//		case STRCONST_SY:
//		case TYPEIDENT_SY:
		default:
			return cg->GetAtomString(yylval.sc_token);
	}
}

typedef int yytokentype;
int yylex(YYSTYPE *lvalp, CG *cg) {
	YYSTYPE	&yylval = *lvalp;
	static int last_token = '\n';

	for(;;) {
		yytokentype token = yytokentype(Scan(cg, yylval));

		if (token == '#' && last_token == '\n') {
			readCPPline(cg, *lvalp);
			continue;
		}
		last_token = token;

		// expand macros
		if (token == IDENT_SY) {
			if (int exp = MacroExpand(cg, yylval))
				token = yytokentype(exp);
		}

		// convert IDENTs to reserved words or TYPEIDENT as appropriate
		if (token == IDENT_SY) {
			cg->last_token = yylval.sc_ident;
			if (yylval.sc_ident >= FIRST_USER_TOKEN_SY) {
				Symbol *pSymb = LookUpSymbol(cg, cg->current_scope, yylval.sc_ident);
				if (IsTypedef(pSymb))
					token = TYPEIDENT_SY;
				else if (IsTemplate(pSymb))
					token = TEMPLATEIDENT_SY;

			} else {
				token = yytokentype(yylval.sc_ident);
			}
		} else {
			cg->last_token = token;
		}

		if (token != '\n')
			return token;
	}
}

} //namespace cgc
