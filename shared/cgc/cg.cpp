#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "cg.h"
#include "errors.h"

using namespace cgc;

struct FileOutputData {
	FILE		*fd;
	void put(const void *p, size_t len) {
		fwrite(p, len, 1, fd);
	}
	int close() {
		fclose(fd);
		delete this;
		return 1;
	}
	FileOutputData(FILE *_fd) : fd(_fd) {}
};

CG::CG() :
	opts(0),
	errors(0), output(0), list(0),
	items(0),
	includer(0)
{
	// Initialise scanner
	static struct EOFInputSrc : InputSrc {
		int		getch(CG *cg)			{ return EOF;}
		void	ungetch(CG *cg, int ch)	{}
		EOFInputSrc() : InputSrc(this)	{}
	} eof_inputsrc;

	atable				= new AtomTable(0);
	InitCPP(this);

	tokenLoc.file		= 0;
	tokenLoc.line		= 0;

	lastSourceLoc.file	= 0;
    lastSourceLoc.line	= 0;

	func_index			= 0;
	last_token			= 0;
	error_count			= 0;
	warning_count		= 0;
	line_count			= 0;
	allow_semantic		= false;
	currentInput		= &eof_inputsrc;

    // Initialize public members:
    bindings			= 0;
    profiles			= 0;
    hal					= 0;

    // Initialize private members:

	current_scope		= NewScopeInPool(mem_CreatePool(0, 0));
	popped_scope		= 0;
	global_scope		= 0;
	super_scope			= 0;
	
	// used to be members of HAL
	varyingIn			= 0;
	varyingOut			= 0;
	uniformParam		= 0;
	uniformGlobal		= 0;
	uniforms			= 0;
	techniques			= 0;
	constantBindings	= 0;
	defaultBindings		= 0;

	for (ProfileRegistration *i = ProfileRegistration::head(); i; i = i->next)
		RegisterProfile(i->create, i->name, i->id);
}

CG::~CG() {
	if (super_scope)
		mem_FreePool(super_scope->pool);
	if (global_scope)
		mem_FreePool(global_scope->pool);
	delete atable;
}

//-----------------------------------------------------------------------------
//	error
//-----------------------------------------------------------------------------

void CG::error(SourceLoc &loc, int num, SEVERITY severity, const char *mess, va_list &args) {
	static const char *severities[] = {
		"notice", "warning", "error", "fatal error",
	};
	if (errors) {
		if (loc.file)
			errors->write("%s(%d) : %s C%04d: ", GetAtomString(loc.file), loc.line, severities[severity], num);
		else
			errors->write("(%d) : %s C%04d: ", loc.line, severities[severity], num);

		errors->write(mess, args);
		errors->write("\n");
	}
	switch (severity) {
		case ERROR: case FATAL:
			error_count++;
			break;
		case WARNING:
			warning_count++;
			break;
	}
}

void CG::error(SourceLoc &loc, int num, SEVERITY severity, const char *mess, ...) {
	va_list args;
	va_start(args, mess);
	error(loc, num, severity, mess, args);
}

//-----------------------------------------------------------------------------
//	Tree Walking
//-----------------------------------------------------------------------------

// Walk an expression tree and apply "pre" and "post" to each node
expr *CG::_ApplyToNodes(const void *me, apply_expr *pre, apply_expr *post, expr *e, int arg2) {
	if (e) {
		if (pre)
			e = pre(me, this, e, arg2);
		switch (e->kind) {
			case DECL_N:
			case SYMB_N:
			case CONST_N:
			case SYMBOLIC_N:
				break;
			case UNARY_N:
				e->un.arg = _ApplyToNodes(me, pre, post, e->un.arg, arg2);
				break;
			case BINARY_N:
				e->bin.left	= _ApplyToNodes(me, pre, post, e->bin.left, arg2);
				e->bin.right = _ApplyToNodes(me, pre, post, e->bin.right, arg2);
				break;
			case TRINARY_N:
				e->tri.arg1 = _ApplyToNodes(me, pre, post, e->tri.arg1, arg2);
				e->tri.arg2 = _ApplyToNodes(me, pre, post, e->tri.arg2, arg2);
				e->tri.arg3 = _ApplyToNodes(me, pre, post, e->tri.arg3, arg2);
				break;
			default:
				assert(!"bad kind to ApplyToNodes()");
				break;
		}
		if (post)
			e = post(me, this, e, arg2);
	}
	return e;
}

// Walk a source tree tree and apply "pre" and "post" to each node
void CG::_ApplyToExpressions(const void *me, apply_expr *pre, apply_expr *post, stmt *s, int arg2) {
	while (s) {
		lastSourceLoc = s->loc;
		switch (s->kind) {
			case EXPR_STMT:
				s->exprst.exp = _ApplyToNodes(me, pre, post, s->exprst.exp, arg2);
				break;
			case IF_STMT:
				s->ifst.cond = _ApplyToNodes(me, pre, post, s->ifst.cond, arg2);
				_ApplyToExpressions(me, pre, post, s->ifst.thenstmt, arg2);
				_ApplyToExpressions(me, pre, post, s->ifst.elsestmt, arg2);
				break;
			case WHILE_STMT:
			case DO_STMT:
				s->whilest.cond = _ApplyToNodes(me, pre, post, s->whilest.cond, arg2);
				_ApplyToExpressions(me, pre, post, s->whilest.body, arg2);
				break;
			case FOR_STMT:
				_ApplyToExpressions(me, pre, post, s->forst.init, arg2);
				s->forst.cond = _ApplyToNodes(me, pre, post, s->forst.cond, arg2);
				_ApplyToExpressions(me, pre, post, s->forst.body, arg2);
				_ApplyToExpressions(me, pre, post, s->forst.step, arg2);
				break;
			case BLOCK_STMT:
				_ApplyToExpressions(me, pre, post, s->blockst.body, arg2);
				break;
			case RETURN_STMT:
				s->returnst.exp = _ApplyToNodes(me, pre, post, s->returnst.exp, arg2);
				break;
			case DISCARD_STMT:
				s->discardst.cond = _ApplyToNodes(me, pre, post, s->discardst.cond, arg2);
				break;
			case BREAK_STMT:
			case COMMENT_STMT:
				break;
			default:
				assert(0);
				break;
		}
		s = s->next;
	}
}

// Apply a function to each node in the expressions contained in a single statement.
void CG::_ApplyToExpressionsLocal(const void *me, apply_expr *pre, apply_expr *post, stmt *s, int arg2) {
	if (s) {
		lastSourceLoc = s->loc;
		switch (s->kind) {
			case EXPR_STMT:
				s->exprst.exp = _ApplyToNodes(me, pre, post, s->exprst.exp, arg2);
				break;
			case IF_STMT:
				s->ifst.cond = _ApplyToNodes(me, pre, post, s->ifst.cond, arg2);
				break;
			case WHILE_STMT:
			case DO_STMT:
				s->whilest.cond = _ApplyToNodes(me, pre, post, s->whilest.cond, arg2);
				break;
			case FOR_STMT:
				s->forst.cond = _ApplyToNodes(me, pre, post, s->forst.cond, arg2);
				break;
			case BLOCK_STMT:
				break;
			case RETURN_STMT:
				s->returnst.exp = _ApplyToNodes(me, pre, post, s->returnst.exp, arg2);
				break;
			case DISCARD_STMT:
				s->discardst.cond = _ApplyToNodes(me, pre, post, s->discardst.cond, arg2);
				break;
			case BREAK_STMT:
			case COMMENT_STMT:
				break;
			default:
				assert(0);
				break;
		}
		//s = s->next;
	}
}

// Walk a source tree and apply a function to each expression.
void CG::_ApplyToTopExpressions(const void *me, apply_expr *fun, stmt *s, int arg2) {
	while (s) {
		lastSourceLoc = s->loc;
		switch (s->kind) {
			case EXPR_STMT:
				s->exprst.exp = fun(me, this, s->exprst.exp, arg2);
				break;
			case IF_STMT:
				s->ifst.cond = fun(me, this, s->ifst.cond, arg2);
				_ApplyToTopExpressions(me, fun, s->ifst.thenstmt, arg2);
				_ApplyToTopExpressions(me, fun, s->ifst.elsestmt, arg2);
				break;
			case WHILE_STMT:
			case DO_STMT:
				s->whilest.cond = fun(me, this, s->whilest.cond, arg2);
				_ApplyToTopExpressions(me, fun, s->whilest.body, arg2);
				break;
			case FOR_STMT:
				_ApplyToTopExpressions(me, fun, s->forst.init, arg2);
				s->forst.cond = fun(me, this, s->forst.cond, arg2);
				_ApplyToTopExpressions(me, fun, s->forst.body, arg2);
				_ApplyToTopExpressions(me, fun, s->forst.step, arg2);
				break;
			case BLOCK_STMT:
				_ApplyToTopExpressions(me, fun, s->blockst.body, arg2);
				break;
			case RETURN_STMT:
				s->returnst.exp = fun(me, this, s->returnst.exp, arg2);
				break;
			case DISCARD_STMT:
				s->discardst.cond = fun(me, this, s->discardst.cond, arg2);
				break;
			case BREAK_STMT:
			case COMMENT_STMT:
				break;
			default:
				assert(0);
				break;
		}
		s = s->next;
	}
}

// Apply a transformation to each child statememt of this statement.
void CG::_ApplyToChildStatements(const void *me, apply_stmt *pre, apply_stmt *post, stmt *s, int arg2) {
	// _Apply a transformation to each nested statement, but not the top level statements:
	lastSourceLoc = s->loc;
	switch (s->kind) {
		case EXPR_STMT:
			break;
		case IF_STMT:
			s->ifst.thenstmt = _ApplyToStatements(me, pre, post, s->ifst.thenstmt, arg2);
			s->ifst.elsestmt = _ApplyToStatements(me, pre, post, s->ifst.elsestmt, arg2);
			break;
		case WHILE_STMT:
		case DO_STMT:
			s->whilest.body = _ApplyToStatements(me, pre, post, s->whilest.body, arg2);
			break;
		case FOR_STMT:
			s->forst.init = _ApplyToStatements(me, pre, post, s->forst.init, arg2);
			s->forst.body = _ApplyToStatements(me, pre, post, s->forst.body, arg2);
			s->forst.step = _ApplyToStatements(me, pre, post, s->forst.step, arg2);
			break;
		case BLOCK_STMT:
			s->blockst.body = _ApplyToStatements(me, pre, post, s->blockst.body, arg2);
			break;
		case RETURN_STMT:
		case DISCARD_STMT:
		case BREAK_STMT:
		case COMMENT_STMT:
			break;
		default:
			assert(0);
			break;
	}
}

// Walk a source tree and apply a transformations to each statememt
stmt *CG::_ApplyToStatements(const void *me, apply_stmt *pre, apply_stmt *post, stmt *s, int arg2) {
	stmt *head = NULL, *last = NULL, *lStmt, *next, *rest = s;

	while (s) {
		// Transform each statement into a possible NULL list of statements:
		// Prepend any statements returned to the list to be processed, and
		// remember what the next one to be done is (rest), so we don't
		// rerun pre on any of the returned statements directly.
		if (pre && rest == s) {
			lastSourceLoc = s->loc;
			rest = s->next;
			s->next = NULL;
			lStmt = pre(me, this, s, arg2);
			if (lStmt) {
				s = lStmt;
				while (lStmt->next && lStmt->next != rest)
					lStmt = lStmt->next;
				lStmt->next = rest;
			} else {
				// Nothing returned - go to next statement:
				s = rest;
				continue;
			}
		}

		// Now apply transformation to substatements:
		_ApplyToChildStatements(me, pre, post, s);

		// Append any statements returned by "post" to the end of the list:
		next = s->next;
		if (post) {
			lastSourceLoc = s->loc;
			lStmt = post(me, this, s, arg2);
		} else {
			lStmt = s;
		}
		if (lStmt) {
			if (head) {
				last->next = lStmt;
			} else {
				head = lStmt;
			}
			last = lStmt;
			while (last->next && last->next != next) {
				last = last->next;
			}
			last->next = NULL;
		}
		s = next;
	}
	return head;
}

//-----------------------------------------------------------------------------
//	Scope
//-----------------------------------------------------------------------------

void CG::PushScope() {
	PushScope(NewScopeInPool(current_scope->pool));
}

void CG::PushScope(Scope *fScope) {
	if (current_scope) {
		fScope->level = current_scope->level + 1;
		if (!fScope->func_scope)
			fScope->func_scope = current_scope->func_scope;
#if 0
		if (fScope->level == 1) {
			if (!global_scope) {
				/* HACK - CTD -- if global_scope==NULL and level==1, we're
				* defining a function in the superglobal scope.  Things
				* will break if we leave the level as 1, so we arbitrarily
				* set it to 2 */
				fScope->level = 2;
			}
		}
		if (fScope->level >= 2) {
			Scope *lScope = fScope;
			while (lScope->level > 2)
				lScope = lScope->next;
			fScope->func_scope = lScope;
		}
#endif
	} else {
		fScope->level = 0;
	}
	fScope->parent = current_scope;
	current_scope = fScope;
}

Scope *CG::PopScope() {
	Scope *s = current_scope;
	if (s)
		current_scope = s->parent;
	return popped_scope = s;
}
//-----------------------------------------------------------------------------
//	dtype
//-----------------------------------------------------------------------------

// Set a type's qualifier bits.  Issue an error if any bit is already set.
bool CG::SetTypeQualifiers(int qualifiers) {
	qualifiers &= TYPE_QUALIFIER_MASK;
	int lqualifiers = type_specs.type.properties & TYPE_QUALIFIER_MASK;
	if (lqualifiers & qualifiers)
		SemanticWarning(this, tokenLoc, WARNING___QUALIFIER_SPECIFIED_TWICE);
	if (lqualifiers != qualifiers) {
		type_specs.type.properties |= qualifiers & TYPE_QUALIFIER_MASK;
		type_specs.is_derived = true;
		if ((type_specs.type.properties & (TYPE_QUALIFIER_CONST | TYPE_QUALIFIER_OUT)) == (TYPE_QUALIFIER_CONST | TYPE_QUALIFIER_OUT))
			SemanticError(this, tokenLoc, ERROR___CONST_OUT_INVALID);
	}
	return true;
}

// Set the domain of a type.  Issue an error if it's already set to a conflicting domain.
bool CG::SetTypeDomain(int domain) {
	int ldomain = type_specs.type.properties & TYPE_DOMAIN_MASK;
	if (ldomain == TYPE_DOMAIN_UNKNOWN) {
		SetDomain(&type_specs.type, domain);
		type_specs.is_derived = true;
	} else {
		if (ldomain == domain) {
			SemanticWarning(this, tokenLoc, WARNING___DOMAIN_SPECIFIED_TWICE);
		} else {
			SemanticError(this, tokenLoc, ERROR___CONFLICTING_DOMAIN);
			return false;
		}
	}
	return true;
}

// Set a bit in the misc field a type.  Issue an error if it's already set.
bool CG::SetTypeMisc(int misc) {
	if (type_specs.type.properties & (misc & ~TYPE_MISC_PACKED)) {
		SemanticError(this, tokenLoc, ERROR___REPEATED_TYPE_ATTRIB);
		return false;
	}
	if (misc & ~TYPE_MISC_TYPEDEF)
		type_specs.is_derived = true;
	type_specs.type.properties |= misc;
	return true;
}

bool CG::ClearTypeMisc(int misc) {
	type_specs.type.properties &= ~misc;
	return true;
}
// Set the storage class of a type.  Issue an error if it's already set to a conflicting value.
bool CG::SetStorageClass(int storage) {
	if (type_specs.storage == SC_UNKNOWN) {
		type_specs.storage = (StorageClass) storage;
		return true;
	}
	if (type_specs.storage == (StorageClass) storage)
		SemanticError(this, tokenLoc, ERROR___STORAGE_SPECIFIED_TWICE);
	else
		SemanticError(this, tokenLoc, ERROR___CONFLICTING_STORAGE);
	return false;
}

//-----------------------------------------------------------------------------
//	HAL
//-----------------------------------------------------------------------------

// RegisterProfile() - Add a profile to the list of available profiles.
Profile *CG::RegisterProfile(HAL* (*CreateHAL)(CG*), const char *name, int id) {
	Profile *p		= new Profile;
	p->next			= profiles;
	p->CreateHAL	= CreateHAL;
	p->name			= AddAtom(name);
	p->id			= id;
	profiles		= p;
	return p;
}

// RegisterProfile() - Add a profile to the list of available profiles.
Profile *CG::GetProfile(const char *name) {
	int		atom = AddAtom(name);
	for (Profile *p = profiles; p; p = p->next) {
		if (atom == p->name)
			return p;
	}
	return 0;
}

bool CG::InitHAL(Profile *p) {
	if (p && (hal = p->CreateHAL(this))) {
		hal->profileName	= p->name;
		hal->pid			= p->id;
		return true;
	}
	return false;
}

int CG::GetFloatSuffixBase(int suffix) {
	int base = hal->GetFloatSuffixBase(tokenLoc, suffix);
	if (base == TYPE_BASE_UNDEFINED_TYPE)
		SemanticError(this, tokenLoc, ERROR_C_UNSUPPORTED_FP_SUFFIX, suffix);
	return base;
}
