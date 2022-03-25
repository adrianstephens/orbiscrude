#ifndef PRINTUTILS_H
#define PRINTUTILS_H

namespace cgc {

struct CG;
struct Type;
struct Symbol;
struct stmt;
struct expr;

void FormatTypeString(CG *cg, char *name, int size, char *name2, int size2, Type *fType);
void FormatTypeStringRT(CG *cg, char *name, int size, char *name2, int size2, Type *fType, bool Unqualified);
void PrintType(CG *cg, Type *fType, int level);
void PrintSymbolTree(CG *cg, Symbol *fSymb);
void PrintScopeDeclarations(CG *cg);

void PrintExpression(CG *cg, expr *fexpr);
void BPrintExpression(CG *cg, expr *fexpr);
void PrintStmt(CG *cg, stmt *fstmt);
void BPrintStmt(CG *cg, stmt *fstmt);
void PrintStmtList(CG *cg, stmt *fstmt);
void BPrintStmtList(CG *cg, stmt *fstmt);
void PrintFunction(CG *cg, Symbol *symb);
void BPrintFunction(CG *cg, Symbol *symb);

} //namespace cgc

#endif // PRINTUTILS_H

