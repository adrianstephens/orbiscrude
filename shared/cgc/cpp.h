#ifndef CPP_H
#define CPP_H

union YYSTYPE;

namespace cgc {

struct Block;
struct MacroSymbol {
    int			argc;
    int			*args;
	int			name;
    Block		*body;
    unsigned	busy:1;
    unsigned	undef:1;
};

void	InitCPP(CG *cg);
void	FinalCPP(CG *cg);
void	readCPPline(CG *cg, YYSTYPE &yylval);
int		MacroExpand(CG *cg, YYSTYPE &yylval);
void	FreeMacro(MacroSymbol *);
bool	PredefineMacro(CG *cg, const char *);

} //namespace cgc

#endif // CPP_H

