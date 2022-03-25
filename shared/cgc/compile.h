#ifndef COMPILE_H
#define COMPILE_H

#include "cg.h"

namespace cgc {

stmt*		ExpandInlineFunctionCalls(CG *cg, Scope *fscope, stmt *body, bool inline_all);
int			CheckFunctionDefinitions(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program);
void		CheckConnectorUsageMain(CG *cg, Symbol *program, stmt *fStmt);
void		CheckForHiddenVaryingReferences(CG *cg, stmt *fStmt);

#define		FC_MATRIX_INDEX_TO_SWIZMAT	1
#define		FC_DEFAULT					0
expr*		FoldConstants(CG *cg, expr *fexpr, int flags);
void		FoldConstants(CG *cg, stmt *fstmt, int flags);
int			GetConstant(CG *cg, expr *fexpr, int flags);

Symbol*		BindVaryingVariable(CG *cg, Symbol *sym, int gname, Scope *inscope, Scope *outscope, bool is_out_val, int struct_semantics);
stmt*		BuildSemanticStructs(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program, const char *vin, const char *vout);
void		BindDefaultSemantic(CG *cg, Symbol *lSymb);
void 		BindUnboundUniformMembers(CG *cg, SymbolList *list);
void 		ResetBindings(SymbolList *&list);

int			CountTechniques(Scope *fScope);
void		SetSymbolFlagsList(Scope *fScope, int fVal);
void		SetSymbolFlags(Symbol *fSymb, int fVal, bool params = true);

bool		HasNumericSuffix(const char *fStr, char *root, size_t size, int *suffix);

int			GetNumberedAtom(CG *cg, const char *root, int number, int digits, char ch);
stmt		*ConcatStmts(stmt *first, stmt *last);
void		AppendStatements(StmtList *fStatements, stmt *fStmt);
expr		*GenSymb(CG *cg, Symbol *fSymb);
expr		*GenMember(CG *cg, Symbol *fSymb);
expr		*GenMemberSelector(CG *cg, expr *sexpr, expr *mExpr);
expr		*GenMemberReference(CG *cg, expr *sexpr, Symbol *mSymb);
expr		*GenVecIndex(expr *vexpr, expr *xexpr, int base, int len);
expr		*GenMatIndex(expr *mExpr, expr *xexpr, int base, int len);
expr		*GenArrayIndex(expr *mExpr, expr *xexpr, int base, int len);
expr		*GenBoolConst(CG *cg, int fval);
expr		*GenIntConst(CG *cg, int fval);
expr		*GenFConstV(CG *cg, float *fval, int len, int base);
expr		*GenConvertVectorLength(CG *cg, expr *fExpr, int base, int len, int newlen);
expr		*GenExprList(CG *cg, expr *fExpr, expr *nExpr, Type *ftype);
expr		*DupExpr(CG *cg, expr *fExpr);
expr		*NewTmp(CG *cg, const expr *fExpr);

stmt		*DuplicateStatementTree(CG *cg, stmt *fStmt);

bool		IsAssignSVOp(expr *fExpr);
bool		IsCastOp(expr *fExpr);
bool		IsConstant(expr *fExpr);
bool		IsVariable(expr *fExpr);
bool		IsBuiltin(expr *fExpr, int *group, int *index);
int			IsAddSub(expr *fExpr);
int			IsMulDiv(expr *fExpr);
bool		AreSame(expr *lExpr, expr *rExpr);

} //namespace cgc

#endif // !defined(__COMPILE_H)
