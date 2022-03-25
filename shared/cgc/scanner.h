#ifndef SCANNER_H
#define SCANNER_H

union YYSTYPE;

namespace cgc {

#define MAX_SYMBOL_NAME_LEN 128
#define MAX_STRING_LEN 512

struct CG;

struct SourceLoc {
    unsigned short file, line, col;
	SourceLoc() : file(0), line(1), col(0) {}
};

int Scan(CG *cg, YYSTYPE &yylval, bool keep_backslashes = false);
const char *GetTokenString(CG *cg, YYSTYPE &yylval, int token);

int ScanFromFile(CG *cg, const char *fname);
int ScanFromBlob(CG *cg, const char *start, size_t size, int name);

void SemanticParseError(CG *cg, SourceLoc &loc, int num, const char *mess, ...);
void SemanticError(CG *cg, SourceLoc &loc, int num, const char *mess, ...);
void InternalError(CG *cg, SourceLoc &loc, int num, const char *mess, ...);
void SemanticWarning(CG *cg, SourceLoc &loc, int num, const char *mess, ...);
void InformationalNotice(CG *cg, SourceLoc &loc, int num, const char *mess, ...);
void FatalError(CG *cg, const char *mess, ...);

} //namespace cgc

#endif // SCANNER_H

