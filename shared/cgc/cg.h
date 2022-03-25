#ifndef CG_H
#define CG_H

#include "cgclib.h"
#include "hal.h"
#include "atom.h"
#include <stdarg.h>

namespace cgc {

struct InputSrc : SourceLoc {
	int			(*vgetch)(InputSrc*, CG*);
	template<typename T> static int	tgetch(InputSrc *me, CG *cg) { return static_cast<T*>(me)->getch(cg); }

	InputSrc	*prev;
	char		save[3];
	char		save_cnt;

	template<typename T> InputSrc(T *t) : prev(0), save_cnt(0), vgetch(tgetch<T>) {}
	int		getch(CG *cg)				{
		if (save_cnt)
			return save[--save_cnt];
		return vgetch(this, cg);
	}
	void	ungetch(int ch) {
		if (save_cnt < sizeof(save))
			save[save_cnt++] = ch;
	}
};

template<bool C, typename T=void>	struct enable_if { typedef T type; };
template<typename T>				struct enable_if<false, T> {};
template<typename T, T>				struct T_checktype;

struct yesno {
	typedef char yes[1], no[2];
};

#define HAS_MEM_FUNC(F) template<typename T, typename S> struct has_##F : yesno {	\
	template<typename U>	static yes	&test(T_checktype<S, &U::F>*);		\
	template<typename>		static no	&test(...);							\
	static bool const value = sizeof(test<T>(0)) == sizeof(yes);				\
}

typedef expr*	apply_expr(const void*,CG*,expr*, int);
typedef stmt*	apply_stmt(const void*,CG*,stmt*, int);

HAS_MEM_FUNC(top_expr);
HAS_MEM_FUNC(pre_expr);
HAS_MEM_FUNC(post_expr);
HAS_MEM_FUNC(pre_stmt);
HAS_MEM_FUNC(post_stmt);

struct CG {
private:
	AtomTable		*atable;
	BindingTree		*bindings;		// #pragma bind bindings
	Profile			*profiles;		// List of supported profiles
	int				error_count;
	int				warning_count;
	int				line_count;

public:
	enum SEVERITY {
		NOTICE, WARNING, ERROR, FATAL
	};
	unsigned int		opts;
	cgclib::Outputter	*errors;
	cgclib::Outputter	*output;
	cgclib::Outputter	*list;
	cgclib::item		*items;
	cgclib::Includer	*includer;
	HAL					*hal;		// Current profile's HAL

	// Scanner data:
	int				last_token;		// Most recent token seen by the scanner
	SourceLoc		tokenLoc;		// Source location of most recent token seen by the scanner
	SourceLoc		lastSourceLoc;
	InputSrc		*currentInput;
	
	int				func_index;
	bool			allow_semantic;
	dtype			type_specs;
	
	Scope			*current_scope;
	Scope			*popped_scope;
	Scope			*global_scope;
	Scope			*super_scope;
	
	Symbol			*varyingIn;
	Symbol			*varyingOut;
	SymbolList		*uniformParam;
	SymbolList		*uniformGlobal;
	SymbolList		*techniques;
	UniformSemantic *uniforms;

	BindingList		*constantBindings;
	BindingList		*defaultBindings;

	template<bool B> struct thunks {
		template<typename T> static apply_expr *get_top_expr ()	{ struct thunk { static expr *f(const void *me, CG *cg, expr *e, int arg2) { return ((T*)me)->top_expr(cg, e, arg2); } }; return &thunk::f;	}
		template<typename T> static apply_expr *get_pre_expr ()	{ struct thunk { static expr *f(const void *me, CG *cg, expr *e, int arg2) { return ((T*)me)->pre_expr(cg, e, arg2); } }; return &thunk::f;	}
		template<typename T> static apply_expr *get_post_expr()	{ struct thunk { static expr *f(const void *me, CG *cg, expr *e, int arg2) { return ((T*)me)->post_expr(cg, e, arg2);} }; return &thunk::f;	}
		template<typename T> static apply_stmt *get_pre_stmt ()	{ struct thunk { static stmt *f(const void *me, CG *cg, stmt *e, int arg2) { return ((T*)me)->pre_stmt(cg, e, arg2); } }; return &thunk::f;	}
		template<typename T> static apply_stmt *get_post_stmt()	{ struct thunk { static stmt *f(const void *me, CG *cg, stmt *e, int arg2) { return ((T*)me)->post_stmt(cg, e, arg2);} }; return &thunk::f;	}
	};

	template<typename T> apply_expr* get_top_expr()		{ return thunks<has_top_expr <T, expr*(T::*)(CG*,expr*,int)>::value>::template get_top_expr<T> (); }
	template<typename T> apply_expr* get_pre_expr()		{ return thunks<has_pre_expr <T, expr*(T::*)(CG*,expr*,int)>::value>::template get_pre_expr<T> (); }
	template<typename T> apply_expr* get_post_expr()	{ return thunks<has_post_expr<T, expr*(T::*)(CG*,expr*,int)>::value>::template get_post_expr<T>(); }
	template<typename T> apply_stmt* get_pre_stmt()		{ return thunks<has_pre_stmt <T, stmt*(T::*)(CG*,stmt*,int)>::value>::template get_pre_stmt<T> (); }
	template<typename T> apply_stmt* get_post_stmt()	{ return thunks<has_post_stmt<T, stmt*(T::*)(CG*,stmt*,int)>::value>::template get_post_stmt<T>(); }

	CG();
	~CG();

	expr*	_ApplyToNodes(const void *me, apply_expr *pre, apply_expr *post, expr *e, int arg2 = 0);
	void	_ApplyToExpressions(const void *me, apply_expr *pre, apply_expr *post, stmt *s, int arg2 = 0);
	void	_ApplyToExpressionsLocal(const void *me, apply_expr *pre, apply_expr *post, stmt *s, int arg2 = 0);
	void	_ApplyToTopExpressions(const void *me, apply_expr *fun, stmt *s, int arg2 = 0);
	void	_ApplyToChildStatements(const void *me, apply_stmt *pre, apply_stmt *post, stmt *s, int arg2 = 0);
	stmt*	_ApplyToStatements(const void *me, apply_stmt *pre, apply_stmt *post, stmt *s, int arg2 = 0);

	template<typename T> expr *ApplyToNodes(const T &t, expr *e, int arg2 = 0) {
		return _ApplyToNodes(&t, get_pre_expr<T>(), get_post_expr<T>(), e);
	}
	template<typename T> void ApplyToExpressions(const T &t, stmt *s, int arg2 = 0) {
		_ApplyToExpressions(&t, get_pre_expr<T>(), get_post_expr<T>(), s);
	}
	template<typename T> void ApplyToExpressionsLocal(const T &t, stmt *s, int arg2 = 0) {
		_ApplyToExpressionsLocal(&t, get_pre_expr<T>(), get_post_expr<T>(), s);
	}
	template<typename T> void ApplyToTopExpressions(const T &t, stmt *s, int arg2 = 0) {
		_ApplyToTopExpressions(&t, get_top_expr<T>(), s);
	}
	template<typename T> stmt *ApplyToStatements(const T &t, stmt *s, int arg2 = 0) {
		return _ApplyToStatements(&t, get_pre_stmt<T>(), get_post_stmt<T>(), s, arg2);
	}
	template<typename T> void ApplyToChildStatements(const T &t, stmt *s, int arg2 = 0) {
		_ApplyToChildStatements(&t, get_pre_stmt<T>(), get_post_stmt<T>(), s, arg2);
	}

	//atom
	int			AddAtom(const char *s)		{ return atable->AddAtom(s); }
	const char *GetAtomString(int atom)		{ return atable->GetAtomString2(atom); }
	int			GetReversedAtom(int atom)	{ return atable->GetReversedAtom(atom); }

	//hal
	Profile		*RegisterProfile(HAL* (*CreateHAL)(CG*), const char *name, int id);
	Profile		*GetProfile(const char *name);
	bool		InitHAL(Profile *p);
	int			GetFloatSuffixBase(int suffix);

	//binding
	BindingTree *LookupBinding(int gname, int lname)	{ return cgc::LookupBinding(bindings, gname, lname); }
	void		AddBinding(BindingTree *fTree)			{ return cgc::AddBinding(&bindings, fTree); }
	void 		AddConstantBinding(Binding *fBind)		{ return cgc::AddBinding(&constantBindings, new(pool()) BindingList(fBind)); }
	void 		AddDefaultBinding(Binding *fBind)		{ return cgc::AddBinding(&defaultBindings, new(pool()) BindingList(fBind)); }

	//scope
	void		PushScope(Scope *fScope);
	void		PushScope();
	Scope		*PopScope();
	MemoryPool	*pool()						const		{ return current_scope->pool; }

	//dtype
	bool		SetTypeQualifiers(int qualifiers);
	bool		SetTypeDomain(int domain);
	bool		SetTypeMisc(int misc);
	bool		ClearTypeMisc(int misc);
	bool		SetStorageClass(int storage);

	// input
	void	PushInput(InputSrc *in) {
		in->prev		= currentInput;
		currentInput	= in;
	}
	int		getch()			{ int c = currentInput->getch(this); if (c == '\n') ++line_count; return c; }
	void	ungetch(int ch) { return currentInput->ungetch(ch);	}

	void	error(SourceLoc &loc, int num, SEVERITY severity, const char *mess, ...);
	void	error(SourceLoc &loc, int num, SEVERITY severity, const char *mess, va_list &args);

	void	printf(const char *fmt,...) {
		if (list) {
			va_list args;
			va_start(args, fmt);
			list->write(fmt, args);
		}
	}
	void	outputf(const char *fmt,...) {
		if (output) {
			va_list args;
			va_start(args, fmt);
			output->write(fmt, args);
		}
	}

	// compilation
	int		CompileProgram(Symbol *program);
	int		CompileProgram2(Symbol *program, cgclib::shader *cgcs);
	int		CompileTechniques();
	bool	DefineMacro(const char *s)	{ return cgc::PredefineMacro(this, s); }
};

template<> struct CG::thunks<false> {
	template<typename T> static apply_expr *get_top_expr ()	{ return 0; }
	template<typename T> static apply_expr *get_pre_expr ()	{ return 0; }
	template<typename T> static apply_expr *get_post_expr()	{ return 0; }
	template<typename T> static apply_stmt *get_pre_stmt ()	{ return 0; }
	template<typename T> static apply_stmt *get_post_stmt()	{ return 0; }
};

} //namespace cgc
#endif // CG_H
