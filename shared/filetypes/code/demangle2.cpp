#include "base/defs.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "demangle2.h"

#ifdef PLAT_PC
#include <malloc.h>
#endif

using namespace iso;

//-----------------------------------------------------------------------------
//	structures
//-----------------------------------------------------------------------------

/* People building mangled trees are expected to allocate instances of struct demangle_component themselves.
They can then call one of the following functions to fill them in. */

#define NL(s) {s,(sizeof s) - 1}

const demangle_operator_info demangle_operators[] = {
	{ "aN", NL("&="),		2 },
	{ "aS", NL("="),		2 },
	{ "aa", NL("&&"),		2 },
	{ "ad", NL("&"),		1 },
	{ "an", NL("&"),		2 },
	{ "cl", NL("()"),		2 },
	{ "cm", NL(","),		2 },
	{ "co", NL("~"),		1 },
	{ "dV", NL("/="),		2 },
	{ "da", NL("delete[]"),	1 },
	{ "de", NL("*"),		1 },
	{ "dl", NL("delete"),	1 },
	{ "dt", NL("."),		2 },
	{ "dv", NL("/"),		2 },
	{ "eO", NL("^="),		2 },
	{ "eo", NL("^"),		2 },
	{ "eq", NL("=="),		2 },
	{ "ge", NL(">="),		2 },
	{ "gt", NL(">"),		2 },
	{ "ix", NL("[]"),		2 },
	{ "lS", NL("<<="),		2 },
	{ "le", NL("<="),		2 },
	{ "ls", NL("<<"),		2 },
	{ "lt", NL("<"),		2 },
	{ "mI", NL("-="),		2 },
	{ "mL", NL("*="),		2 },
	{ "mi", NL("-"),		2 },
	{ "ml", NL("*"),		2 },
	{ "mm", NL("--"),		1 },
	{ "na", NL("new[]"),	1 },
	{ "ne", NL("!="),		2 },
	{ "ng", NL("-"),		1 },
	{ "nt", NL("!"),		1 },
	{ "nw", NL("new"),		1 },
	{ "oR", NL("|="),		2 },
	{ "oo", NL("||"),		2 },
	{ "or", NL("|"),		2 },
	{ "pL", NL("+="),		2 },
	{ "pl", NL("+"),		2 },
	{ "pm", NL("->*"),		2 },
	{ "pp", NL("++"),		1 },
	{ "ps", NL("+"),		1 },
	{ "pt", NL("->"),		2 },
	{ "qu", NL("?"),		3 },
	{ "rM", NL("%="),		2 },
	{ "rS", NL(">>="),		2 },
	{ "rm", NL("%"),		2 },
	{ "rs", NL(">>"),		2 },
	{ "st", NL("sizeof "),	1 },
	{ "sz", NL("sizeof "),	1 },
	{ "at", NL("alignof "),	1 },
	{ "az", NL("alignof "),	1 },
	{ NULL, NULL, 0,		0 }
};

const demangle_builtin_type_info demangle_builtin_types[] = {
	/*a*/	{ NL("signed char"),		D_PRINT_DEFAULT		},
	/*b*/	{ NL("bool"),				D_PRINT_BOOL		},
	/*c*/	{ NL("char"),				D_PRINT_DEFAULT		},
	/*d*/	{ NL("double"),				D_PRINT_FLOAT		},
	/*e*/	{ NL("long double"),		D_PRINT_FLOAT		},
	/*f*/	{ NL("float"),				D_PRINT_FLOAT		},
	/*g*/	{ NL("__float128"),			D_PRINT_FLOAT		},
	/*h*/	{ NL("unsigned char"),		D_PRINT_DEFAULT		},
	/*i*/	{ NL("int"),				D_PRINT_INT			},
	/*j*/	{ NL("unsigned"),			D_PRINT_UNSIGNED	},
	/*k*/	{ NULL, 0,					D_PRINT_DEFAULT		},
	/*l*/	{ NL("long"),				D_PRINT_LONG		},
	/*m*/	{ NL("unsigned long"),		D_PRINT_UNSIGNED_LONG},
	/*n*/	{ NL("__int128"),			D_PRINT_DEFAULT		},
	/*o*/	{ NL("unsigned __int128"),	D_PRINT_DEFAULT		},
	/*p*/	{ NULL, 0,					D_PRINT_DEFAULT		},
	/*q*/	{ NULL, 0,					D_PRINT_DEFAULT		},
	/*r*/	{ NULL, 0,					D_PRINT_DEFAULT		},
	/*s*/	{ NL("short"),				D_PRINT_DEFAULT		},
	/*t*/	{ NL("unsigned short"), 	D_PRINT_DEFAULT		},
	/*u*/	{ NULL, 0,					D_PRINT_DEFAULT		},
	/*v*/	{ NL("void"),				D_PRINT_VOID		},
	/*w*/	{ NL("wchar_t"),			D_PRINT_DEFAULT		},
	/*x*/	{ NL("long long"),			D_PRINT_LONG_LONG	},
	/*y*/	{ NL("unsigned long long"), D_PRINT_UNSIGNED_LONG_LONG},
	/*z*/	{ NL("..."),				D_PRINT_DEFAULT		},
	/*26*/	{ NL("decimal32"),			D_PRINT_DEFAULT		},
	/*27*/	{ NL("decimal64"),			D_PRINT_DEFAULT		},
	/*28*/	{ NL("decimal128"),			D_PRINT_DEFAULT		},
	/*29*/	{ NL("half"),				D_PRINT_FLOAT		},
	/*30*/	{ NL("char16_t"),			D_PRINT_DEFAULT		},
	/*31*/	{ NL("char32_t"),			D_PRINT_DEFAULT		},
	/*32*/	{ NL("decltype(nullptr)"),	D_PRINT_DEFAULT		},
};

//-----------------------------------------------------------------------------
//	demangle_component
//-----------------------------------------------------------------------------

// Return whether a name is a constructor, a destructor, or a conversion operator.
bool demangle_component::is_ctor_dtor_or_conversion() const {
	switch (type) {
		default:
			return false;
		case demangle_component::QUAL_NAME:
		case demangle_component::LOCAL_NAME:
			return right && right->is_ctor_dtor_or_conversion();
		case demangle_component::XTOR:
		case demangle_component::CAST:
			return true;
	}
}

// Return whether a function should have a return type
// The rules are that template functions have return types with some exceptions, function types which are not part of a function name mangling have return types with some exceptions, and non-template function names do not have return types
// The exceptions are that constructors, destructors, and conversion operators do not have return types
bool demangle_component::has_return_type() const {
	switch (type) {
		default:
			return false;
		case demangle_component::TEMPLATE:
			return !left->is_ctor_dtor_or_conversion();
		case demangle_component::RESTRICT_THIS:
		case demangle_component::VOLATILE_THIS:
		case demangle_component::CONST_THIS:
			return left && left->has_return_type();
	}
}

demangle_component *deep_copy(const demangle_component *dc0) {
	if (!dc0)
		return 0;

	demangle_component	*dc = new demangle_component(*dc0);

	switch (dc->type) {
		case demangle_component::QUAL_NAME:
		case demangle_component::LOCAL_NAME:
		case demangle_component::TYPED_NAME:
		case demangle_component::TEMPLATE:
		case demangle_component::CONSTRUCTION_VTABLE:
		case demangle_component::FUNCTION_TYPE:
		case demangle_component::ARRAY_TYPE:
		case demangle_component::PTRMEM_TYPE:
		case demangle_component::VECTOR_TYPE:
		case demangle_component::ARGLIST:
		case demangle_component::TEMPLATE_ARGLIST:
		case demangle_component::UNARY:
		case demangle_component::BINARY:
		case demangle_component::BINARY_ARGS:
		case demangle_component::TRINARY:
		case demangle_component::TRINARY_ARG1:
		case demangle_component::TRINARY_ARG2:
		case demangle_component::LITERAL:
		case demangle_component::LITERAL_NEG:
		case demangle_component::COMPOUND_NAME:
			dc->left = deep_copy(dc->left);
			dc->right = deep_copy(dc->right);
			break;

		case demangle_component::VTABLE:
		case demangle_component::VTT:
		case demangle_component::TYPEINFO:
		case demangle_component::TYPEINFO_NAME:
		case demangle_component::TYPEINFO_FN:
		case demangle_component::THUNK:
		case demangle_component::VIRTUAL_THUNK:
		case demangle_component::COVARIANT_THUNK:
		case demangle_component::GUARD:
		case demangle_component::REFTEMP:
		case demangle_component::HIDDEN_ALIAS:
		case demangle_component::RESTRICT:
		case demangle_component::VOLATILE:
		case demangle_component::KONST:
		case demangle_component::RESTRICT_THIS:
		case demangle_component::VOLATILE_THIS:
		case demangle_component::CONST_THIS:
		case demangle_component::VENDOR_TYPE_QUAL:
		case demangle_component::POINTER:
		case demangle_component::REFERENCE:
		case demangle_component::RVALUE_REFERENCE:
		case demangle_component::COMPLEX:
		case demangle_component::IMAGINARY:
		case demangle_component::VENDOR_TYPE:
		case demangle_component::EXTENDED_OPERATOR:
		case demangle_component::DECLTYPE:
		case demangle_component::PACK_EXPANSION:
		case demangle_component::GLOBAL_CONSTRUCTORS:
		case demangle_component::GLOBAL_DESTRUCTORS:
		case demangle_component::LAMBDA:
			dc->left = deep_copy(dc->left);
			break;
	}
	return dc;
}

//-----------------------------------------------------------------------------
//	demangle_component makers
//-----------------------------------------------------------------------------
// Add a new generic component.
bool verify(const demangle_component *dc) {
	if (!dc)
		return false;
	switch (dc->type) {
		case demangle_component::NAME:
			return  *dc->string.str && dc->string.len < 0x1000;
	}
	return true;
}

demangle_component *DemanglerGNU::make_comp(demangle_component::TYPE type, demangle_component *left, demangle_component *right) {
	// We check for errors here.  A typical error would be a NULL return from a subroutine.  We catch those here, and return NULL upward.
	switch (type) {
		// These types require two parameters.
		case demangle_component::QUAL_NAME:
		case demangle_component::LOCAL_NAME:
		case demangle_component::TYPED_NAME:
		case demangle_component::TEMPLATE:
		case demangle_component::CONSTRUCTION_VTABLE:
		case demangle_component::VENDOR_TYPE_QUAL:
		case demangle_component::PTRMEM_TYPE:
		case demangle_component::UNARY:
		case demangle_component::BINARY:
		case demangle_component::BINARY_ARGS:
		case demangle_component::TRINARY:
		case demangle_component::TRINARY_ARG1:
		case demangle_component::TRINARY_ARG2:
		case demangle_component::LITERAL:
		case demangle_component::LITERAL_NEG:
		case demangle_component::COMPOUND_NAME:
		case demangle_component::VECTOR_TYPE:
			if (!verify(left) || !verify(right))
				return NULL;
			break;
		// These types only require one parameter.
		case demangle_component::VTABLE:
		case demangle_component::VTT:
		case demangle_component::TYPEINFO:
		case demangle_component::TYPEINFO_NAME:
		case demangle_component::TYPEINFO_FN:
		case demangle_component::THUNK:
		case demangle_component::VIRTUAL_THUNK:
		case demangle_component::COVARIANT_THUNK:
		case demangle_component::GUARD:
		case demangle_component::REFTEMP:
		case demangle_component::HIDDEN_ALIAS:
		case demangle_component::POINTER:
		case demangle_component::REFERENCE:
		case demangle_component::RVALUE_REFERENCE:
		case demangle_component::COMPLEX:
		case demangle_component::IMAGINARY:
		case demangle_component::VENDOR_TYPE:
		case demangle_component::CAST:
		case demangle_component::DECLTYPE:
		case demangle_component::PACK_EXPANSION:
		case demangle_component::GLOBAL_CONSTRUCTORS:
		case demangle_component::GLOBAL_DESTRUCTORS:
			if (!verify(left))
				return NULL;
			break;
		// This needs a right parameter, but the left parameter can be empty.
		case demangle_component::ARRAY_TYPE:
			if (right == NULL)
				return NULL;
			break;
			// These are allowed to have no parameters--in some cases they will be filled in later.
		case demangle_component::FUNCTION_TYPE:
		case demangle_component::RESTRICT:
		case demangle_component::VOLATILE:
		case demangle_component::KONST:
		case demangle_component::RESTRICT_THIS:
		case demangle_component::VOLATILE_THIS:
		case demangle_component::CONST_THIS:
		case demangle_component::ARGLIST:
		case demangle_component::TEMPLATE_ARGLIST:
			break;
			// Other types should not be seen here.
		default:
			return NULL;
	}
	return comps.push_back(new demangle_component(type, left, right));
}

// Add a new demangle mangled name component.
demangle_component *DemanglerGNU::make_demangle_mangled_name(const char *s) {
	if (peek_char() != '_' || peek_next_char() != 'Z')
		return make_name(s);
	advance(2);
	return encoding(false);
}

// Add a new name component.
demangle_component *DemanglerGNU::make_name(const demangle_string2 &name) {
	return comps.push_back(new demangle_component(demangle_component::NAME, name));
}

// Add a new builtin type component.
demangle_component *DemanglerGNU::make_builtin_type(const demangle_builtin_type_info *type) {
	if (!type)
		return NULL;
	return comps.push_back(new demangle_component(type));
}

// Add a new operator component.
demangle_component *DemanglerGNU::make_operator(const demangle_operator_info *op) {
	return comps.push_back(new demangle_component(op));
}

// Add a new extended operator component.
demangle_component *DemanglerGNU::make_extended_operator(int args, demangle_component *name) {
	return comps.push_back(new demangle_component(demangle_component::EXTENDED_OPERATOR, name, args));
}

demangle_component *DemanglerGNU::make_default_arg(int num, demangle_component *sub) {
	demangle_component *p = new (comps) demangle_component(demangle_component::DEFAULT_ARG);
	p->num = num;
	p->arg = sub;
	return p;
}

// Add a new constructor/destructor component.
demangle_component *DemanglerGNU::make_xtor(gnu_v3_xtor_kinds kind, demangle_component *name) {
	return comps.push_back(new demangle_component(kind, name));
}

// Add a new template parameter.
demangle_component *DemanglerGNU::make_template_param(int i) {
	return comps.push_back(new demangle_component(demangle_component::TEMPLATE_PARAM, i));
}

// Add a new function parameter.
demangle_component *DemanglerGNU::make_function_param(int i) {
	return comps.push_back(new demangle_component(demangle_component::FUNCTION_PARAM, i));
}

// Add a new standard substitution component.
demangle_component *DemanglerGNU::make_sub(const demangle_string2 &name) {
	return comps.push_back(new demangle_component(demangle_component::SUB_STD, name));
}

//-----------------------------------------------------------------------------
//	DemanglerGNU
//-----------------------------------------------------------------------------

// Initialize the information structure we use to pass around information.
DemanglerGNU::DemanglerGNU(const char *mangled, int _options, size_t len) : n(mangled), end(mangled + len), options(_options) {
	if (n[1] == '_')
		++n;

	type = !(n[0] == '_' && n[1] == 'Z');
//	if (type && !(options & DMGL_TYPES))
//		return NULL;

	// Similarly, we can not need more substitutions than there are chars in the mangled string.
	did_subs	= 0;
	last_name	= NULL;
	expansion	= 0;
}

DemanglerGNU::~DemanglerGNU() {
	for (auto &i : comps)
		delete i;
}


demangle_component *DemanglerGNU::demangle() {
	if (type && !(options & DMGL_TYPES))
		return NULL;

	demangle_component *dc = type ? demangle_type() : demangle_mangled_name(true);

	// If DMGL_PARAMS is set, then if we didn't consume the entire mangled string, then we didn't successfully demangle it.
	if ((options & DMGL_PARAMS) && peek_char() != '\0')
		dc = NULL;
	return dc;
}


/* <mangled-name> ::= _Z <encoding>
TOP_LEVEL is non-zero when called at the top level.  */
demangle_component *DemanglerGNU::demangle_mangled_name(bool top_level) {
	// Allow missing _ if not at toplevel to work around a bug in G++ abi-version=2 mangling; see the comment in write_template_arg.
	return	!check_char('_') && top_level ? 0
		: !check_char('Z') ? 0
		: encoding(top_level);
}

/* <encoding> ::= <(function) name> <bare-function-type>
::= <(data) name>
::= <special-name>
TOP_LEVEL is true when called at the top level, in which case if DMGL_PARAMS is not set we do not demangle the function
parameters.  We only set this at the top level, because otherwise we would not correctly demangle names in local scopes.  */
demangle_component *DemanglerGNU::encoding(bool top_level) {
	char peek = peek_char();
	if (peek == 'G' || peek == 'T')
		return special_name();

	demangle_component *dc = name();

	if (dc && top_level && !(options & DMGL_PARAMS)) {
		// Strip off any initial CV-qualifiers, as they really apply o the `this' parameter, and they were not output by the v2 demangler without DMGL_PARAMS.
		while (dc->type == demangle_component::RESTRICT_THIS
			|| dc->type == demangle_component::VOLATILE_THIS
			|| dc->type == demangle_component::CONST_THIS)
			dc = dc->left;

		// If the top level is a demangle_component::LOCAL_NAME, then there may be CV-qualifiers on its right argument which really apply here; this happens when parsing a class which is local to a function
		if (dc->type == demangle_component::LOCAL_NAME) {
			demangle_component *dcr = dc->right;
			while (dcr && (dcr->type == demangle_component::RESTRICT_THIS
				|| dcr->type == demangle_component::VOLATILE_THIS
				|| dcr->type == demangle_component::CONST_THIS))
				dcr = dcr->left;
			dc->right = dcr;
		}
		return dc;
	}

	peek = peek_char();
	if (dc == NULL || peek == '\0' || peek == 'E')
		return dc;
	return make_comp(demangle_component::TYPED_NAME, dc, bare_function_type(dc->has_return_type()));
}

/* <name> ::= <nested-name>
::= <unscoped-name>
::= <unscoped-template-name> <template-args>
::= <local-name>

<unscoped-name> ::= <unqualified-name>
::= St <unqualified-name>

<unscoped-template-name> ::= <unscoped-name>
::= <substitution>
*/
demangle_component *DemanglerGNU::name() {
	demangle_component *dc;

	switch (peek_char()) {
		case 'N':	return nested_name();
		case 'Z':	return local_name();
		case 'L':
		case 'U':	return unqualified_name();
		case 'S': {
			int subst;
			if (peek_next_char() != 't') {
				dc = substitution(false);
				subst = 1;
			} else {
				advance(2);
				dc = make_comp(demangle_component::QUAL_NAME, make_name("std"), unqualified_name());
				expansion += 3;
				subst = 0;
			}
			if (peek_char() != 'I') {
				// The grammar does not permit this case to occur if we	called substitution() above(i.e., subst == 1).  We don't bother to check
			} else {
				// This is <template-args>, which means that we just saw <unscoped-template-name>, which is a substitution candidate if we didn't just get it from a substitution
				if (!subst && !add_substitution(dc))
					return NULL;
				dc = make_comp(demangle_component::TEMPLATE, dc, template_args());
			}
			return dc;
		}
		default:
			dc = unqualified_name();
			if (peek_char() == 'I') {
				// This is <template-args>, which means that we just saw <unscoped-template-name>, which is a substitution candidate
				if (!add_substitution(dc))
					return NULL;
				dc = make_comp(demangle_component::TEMPLATE, dc, template_args());
			}
			return dc;
	}
}

/* <nested-name> ::= N [<CV-qualifiers>] <prefix> <unqualified-name> E
::= N [<CV-qualifiers>] <template-prefix> <template-args> E */
demangle_component *DemanglerGNU::nested_name() {
	if (!check_char('N'))
		return NULL;

	demangle_component *ret;
	demangle_component **pret = cv_qualifiers(&ret, true);
	return pret && (*pret = prefix()) && check_char('E') ? ret : 0;
}

/* <prefix> ::= <prefix> <unqualified-name>
::= <template-prefix> <template-args>
::= <template-param>
::=
::= <substitution>

<template-prefix> ::= <prefix> <(template) unqualified-name>
::= <template-param>
::= <substitution>*/
demangle_component *DemanglerGNU::prefix() {
	for (demangle_component *ret = NULL;;) {
		demangle_component::TYPE comb_type;
		demangle_component		*dc;

		char	peek = peek_char();
		if (peek == '\0')
			return NULL;

		// The older code accepts a <local-name> here, but I don't see that in the grammar.  The older code does not accept a <template-param> here.
		comb_type = demangle_component::QUAL_NAME;
		if (is_digit(peek) || is_lower(peek) || peek == 'C' || peek == 'D' || peek == 'U' || peek == 'L')
			dc = unqualified_name();
		else if (peek == 'S')
			dc = substitution(true);
		else if (peek == 'I') {
			if (ret == NULL)
				return NULL;
			comb_type = demangle_component::TEMPLATE;
			dc = template_args();
		} else if (peek == 'T')
			dc = template_param();
		else if (peek == 'E')
			return ret;
		else if (peek == 'M') {
			// Initializer scope for a lambda.  We don't need to represent this; the normal code will just treat the variable as a type scope, which gives appropriate output
			if (ret == NULL)
				return NULL;
			advance(1);
			continue;
		} else
			return NULL;

		if (!dc)
			return NULL;
		ret = ret == NULL ? dc : make_comp(comb_type, ret, dc);

		if (peek != 'S' && peek_char() != 'E') {
			if (!add_substitution(ret))
				return NULL;
		}
	}
}

/* <unqualified-name> ::= <operator-name>
::= <ctor-dtor-name>
::= <source-name>
::= <local-source-name>

<local-source-name>	::= L <source-name> <discriminator> */
demangle_component *DemanglerGNU::unqualified_name() {
	char peek = peek_char();
	if (is_digit(peek))
		return source_name();

	if (is_lower(peek)) {
		demangle_component *ret = operator_name();
		if (ret != NULL && ret->type == demangle_component::OPERATOR)
			expansion += sizeof "operator" + ret->op->name.len - 2;
		return ret;
	}
	if (peek == 'C' || peek == 'D')
		return xtor_name();

	if (peek == 'L') {
		advance(1);
		demangle_component *ret = source_name();
		return ret && discriminator() ? ret : 0;
	}
	if (peek == 'U') {
		switch (peek_next_char()) {
			case 'l':	return lambda();
			case 't':	return unnamed_type();
			default:	return NULL;
		}
	}
	return NULL;
}

// <source-name> ::= <(positive length) number> <identifier>
demangle_component *DemanglerGNU::source_name() {
	int len = number();
	if (len <= 0)
		return NULL;
	return last_name = identifier(len);
}

// number ::= [n] <(non-negative decimal integer)>
int DemanglerGNU::number() {
	bool negative;
	char peek = peek_char();
	if (negative = (peek == 'n')) {
		advance(1);
		peek = peek_char();
	}

	int ret = 0;
	while (is_digit(peek)) {
		ret = ret * 10 + peek - '0';
		advance(1);
		peek = peek_char();
	}
	return negative ? -ret : ret;
}

// identifier ::= <(unqualified source code identifier)>

demangle_component *DemanglerGNU::identifier(int len) {
	const char *name = str();
	if (end - name < len)
		return NULL;

	advance(len);
	// Look for something which looks like a gcc encoding of an anonymous namespace, and replace it with a more user friendly name.
	if (len >= 10 && memcmp(name, "_GLOBAL_", 8) == 0) {
		const char *s = name + 8;
		if ((*s == '.' || *s == '_' || *s == '$') && s[1] == 'N') {
			expansion -= len - sizeof "(anonymous namespace)";
			return make_name("(anonymous namespace)");
		}
	}
	return make_name(demangle_string2(name, len));
}

/* operator_name ::= many different two character encodings.
::= cv <type>
::= v <digit> <source-name> */
demangle_component *DemanglerGNU::operator_name() {
	char	c1 = next_char();
	char	c2 = next_char();

	if (c1 == 'v' && is_digit(c2))
		return make_extended_operator(c2 - '0', source_name());

	if (c1 == 'c' && c2 == 'v')
		return make_comp(demangle_component::CAST, demangle_type(), NULL);

	// LOW is the inclusive lower bound. HIGH is the exclusive upper bound.  We subtract one to ignore the sentinel at the end of the array.
	int low = 0, high = ((sizeof(demangle_operators) / sizeof(demangle_operators[0])) - 1);

	for (;;) {
		int i = low + (high - low) / 2;
		const demangle_operator_info *p = demangle_operators + i;
		if (c1 == p->code[0] && c2 == p->code[1])
			return make_operator(p);

		if (c1 < p->code[0] || (c1 == p->code[0] && c2 < p->code[1]))
			high = i;
		else
			low = i + 1;
		if (low == high)
			return NULL;
	}
}

demangle_component *DemanglerGNU::make_character(int c) {
	return comps.push_back(new demangle_component(demangle_component::CHARACTER, c));
}

/* <special-name> ::= TV <type>
::= TT <type>
::= TI <type>
::= TS <type>
::= GV <(object) name>
::= T <call-offset> <(base) encoding>
::= Tc <call-offset> <call-offset> <(base) encoding>
Also g++ extensions:
::= TC <type> <(offset) number> _ <(base) type>
::= TF <type>
::= TJ <type>
::= GR <name>
::= GA <encoding>
::= Gr <resource name>*/
demangle_component *DemanglerGNU::special_name() {
	expansion += 20;
	if (check_char('T')) switch (next_char()) {
		case 'V':
			expansion -= 5;
			return make_comp(demangle_component::VTABLE, demangle_type(), NULL);
		case 'T':
			expansion -= 10;
			return make_comp(demangle_component::VTT, demangle_type(), NULL);
		case 'I':
			return make_comp(demangle_component::TYPEINFO, demangle_type(), NULL);
		case 'S':
			return make_comp(demangle_component::TYPEINFO_NAME, demangle_type(), NULL);
		case 'h':
			return call_offset('h')
				? make_comp(demangle_component::THUNK, encoding(false), NULL)
				: NULL;
		case 'v':
			return call_offset('v')
				? make_comp(demangle_component::VIRTUAL_THUNK, encoding(false), NULL)
				: NULL;
		case 'c':
			if (!call_offset('\0'))
				return NULL;
			if (!call_offset('\0'))
				return NULL;
			return make_comp(demangle_component::COVARIANT_THUNK, encoding(false), NULL);
		case 'C':
		{
			demangle_component *derived_type = demangle_type();
			int				offset = number();
			if (offset < 0)
				return NULL;
			if (!check_char('_'))
				return NULL;
			demangle_component *base_type = demangle_type();
			// We don't display the offset.  FIXME: We should display it in verbose mode.
			expansion += 5;
			return make_comp(demangle_component::CONSTRUCTION_VTABLE, base_type, derived_type);
		}
		case 'F':
			return make_comp(demangle_component::TYPEINFO_FN, demangle_type(), NULL);
		default:
			return NULL;
	}
	if (check_char('G')) switch (next_char()) {
		case 'V':	return make_comp(demangle_component::GUARD, name(), NULL);
		case 'R':	return make_comp(demangle_component::REFTEMP, name(), NULL);
		case 'A':	return make_comp(demangle_component::HIDDEN_ALIAS, encoding(false), NULL);
		default:	return NULL;
	}
	return NULL;
}

/* <call-offset> ::= h <nv-offset> _
::= v <v-offset> _

<nv-offset> ::= <(offset) number>

<v-offset> ::= <(offset) number> _ <(virtual offset) number>

The C parameter, if not '\0', is a character we just read which is
the start of the <call-offset>.

We don't display the offset information anywhere.  FIXME: We should
display it in verbose mode.  */

bool DemanglerGNU::call_offset(int c) {
	if (c == '\0')
		c = next_char();

	if (c == 'h')
		number();
	else if (c == 'v') {
		number();
		if (!check_char('_'))
			return false;
		number();
	} else
		return false;

	return check_char('_');
}

/* <ctor-dtor-name> ::= C1
::= C2
::= C3
::= D0
::= D1
::= D2 */
demangle_component *DemanglerGNU::xtor_name() {
	if (last_name) {
		if (last_name->type == demangle_component::NAME)
			expansion += last_name->string.len;
		else if (last_name->type == demangle_component::SUB_STD)
			expansion += last_name->string.len;
	}

	gnu_v3_xtor_kinds kind;

	switch (peek_char()) {
		case 'C': {
			switch (peek_next_char()) {
				case '1':	kind = gnu_v3_complete_object_ctor; 			break;
				case '2':	kind = gnu_v3_base_object_ctor;					break;
				case '3':	kind = gnu_v3_complete_object_allocating_ctor;	break;
				default:	return NULL;
			}
			advance(2);
			return make_xtor(kind, last_name);
		}
		case 'D': {
			switch (peek_next_char()) {
				case '0':	kind = gnu_v3_deleting_dtor;		break;
				case '1':	kind = gnu_v3_complete_object_dtor;	break;
				case '2':	kind = gnu_v3_base_object_dtor;		break;
				default:	return NULL;
			}
			advance(2);
			return make_xtor(kind, last_name);
		}
		default:
			return NULL;
	}
}

/* <type> ::= <builtin-type>
::= <function-type>
::= <class-enum-type>
::= <array-type>
::= <pointer-to-member-type>
::= <template-param>
::= <template-template-param> <template-args>
::= <substitution>
::= <CV-qualifiers> <type>
::= P <type>
::= R <type>
::= O <type>(C++0x)
::= C <type>
::= G <type>
::= U <source-name> <type>

<builtin-type> ::= various one letter codes
::= u <source-name> */
demangle_component *DemanglerGNU::demangle_type() {
	/* The ABI specifies that when CV-qualifiers are used, the base type is substitutable, and the fully qualified type is substitutable,
	but the base type with a strict subset of the CV-qualifiers is not substitutable.  The natural recursive implementation of the
	CV-qualifiers would cause subsets to be substitutable, so instead we pull them all off now.

	FIXME: The ABI says that order-insensitive vendor qualifiers should be handled in the same way, but we have no way to tell
	which vendor qualifiers are order-insensitive and which are order-sensitive.  So we just assume that they are all
	order-sensitive.  g++ 3.4 supports only one vendor qualifier, __vector, and it treats it as order-sensitive when mangling names.  */

	demangle_component *ret;
	char peek = peek_char();
	if (peek == 'r' || peek == 'V' || peek == 'K') {
		demangle_component **pret = cv_qualifiers(&ret, false);
		if (!pret)
			return NULL;
		*pret = demangle_type();
		if (!*pret || !add_substitution(ret))
			return NULL;
		return ret;
	}

	bool	can_subst = true;
	switch (peek) {
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j':           case 'l': case 'm': case 'n':
		case 'o':                               case 's': case 't':
		case 'v': case 'w': case 'x': case 'y': case 'z':
			ret = make_builtin_type(&demangle_builtin_types[peek - 'a']);
			expansion += ret->builtin->name.len;
			can_subst = false;
			advance(1);
			break;
		case 'u':
			advance(1);
			ret = make_comp(demangle_component::VENDOR_TYPE, source_name(), NULL);
			break;
		case 'F':
			ret = function_type();
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case 'N':
		case 'Z':
			ret = class_enum_type();
			break;
		case 'A':
			ret = array_type();
			break;
		case 'M':
			ret = pointer_to_member_type();
			break;
		case 'T':
			ret = template_param();
			if (peek_char() == 'I') {
				// This is <template-template-param> <template-args>.  The <template-template-param> part is a substitution candidate.
				if (!add_substitution(ret))
					return NULL;
				ret = make_comp(demangle_component::TEMPLATE, ret, template_args());
			}
			break;
		case 'S': {
			// If this is a special substitution, then it is the start of <class-enum-type>.
			char peek_next = peek_next_char();
			if (is_digit(peek_next) || peek_next == '_' || is_upper(peek_next)) {
				ret = substitution(false);
				// The substituted name may have been a template name and may be followed by tepmlate args.
				if (peek_char() == 'I')
					ret = make_comp(demangle_component::TEMPLATE, ret, template_args());
				else
					can_subst = false;
			} else {
				ret = class_enum_type();
				/* If the substitution was a complete type, then it is not a new substitution candidate.  However, if the
				substitution was followed by template arguments, then the whole thing is a substitution candidate.  */
				if (ret && ret->type == demangle_component::SUB_STD)
					can_subst = false;
			}
			break;
		}
		case 'O':
			advance(1);
			ret = make_comp(demangle_component::RVALUE_REFERENCE, demangle_type(), NULL);
			break;
		case 'P':
			advance(1);
			ret = make_comp(demangle_component::POINTER, demangle_type(), NULL);
			break;
		case 'R':
			advance(1);
			ret = make_comp(demangle_component::REFERENCE, demangle_type(), NULL);
			break;
		case 'C':
			advance(1);
			ret = make_comp(demangle_component::COMPLEX, demangle_type(), NULL);
			break;
		case 'G':
			advance(1);
			ret = make_comp(demangle_component::IMAGINARY, demangle_type(), NULL);
			break;
		case 'U':
			advance(1);
			ret = source_name();
			ret = make_comp(demangle_component::VENDOR_TYPE_QUAL, demangle_type(), ret);
			break;
		case 'D':
			can_subst = false;
			advance(1);
			switch (next_char()) {
				case 'T':
				case 't':	// decltype(expression)
					ret = make_comp(demangle_component::DECLTYPE, expression(), NULL);
					if (ret && next_char() != 'E')
						ret = NULL;
					break;
				case 'p':	// Pack expansion.
					ret = make_comp(demangle_component::PACK_EXPANSION, demangle_type(), NULL);
					break;
				case 'f':	// 32-bit decimal floating point
					ret = make_builtin_type(&demangle_builtin_types[26]);
					expansion += ret->builtin->name.len;
					break;
				case 'd':	// 64-bit DFP
					ret = make_builtin_type(&demangle_builtin_types[27]);
					expansion += ret->builtin->name.len;
					break;
				case 'e':	// 128-bit DFP
					ret = make_builtin_type(&demangle_builtin_types[28]);
					expansion += ret->builtin->name.len;
					break;
				case 'h':	// 16-bit half-precision FP
					ret = make_builtin_type(&demangle_builtin_types[29]);
					expansion += ret->builtin->name.len;
					break;
				case 's':	// char16_t
					ret = make_builtin_type(&demangle_builtin_types[30]);
					expansion += ret->builtin->name.len;
					break;
				case 'i':	// char32_t
					ret = make_builtin_type(&demangle_builtin_types[31]);
					expansion += ret->builtin->name.len;
					break;
				case 'F':	// Fixed point types. DF<int bits><length><fract bits><sat>
					ret = comps.push_back(new demangle_component(demangle_component::FIXED_TYPE));
					if ((ret->s_fixed.accum = is_digit(peek_char())))
						// For demangling we don't care about the bits.
						number();
					ret->s_fixed.length = demangle_type();
					if (ret->s_fixed.length == NULL)
						return NULL;
					number();
					ret->s_fixed.sat = (next_char() == 's');
					break;
				case 'v':
					ret = vector_type();
					break;
				case 'n':	// decltype(nullptr)
					ret = make_builtin_type(&demangle_builtin_types[32]);
					expansion += ret->builtin->name.len;
					break;

				default:
					return NULL;
			}
			break;

		default:
			return NULL;
	}
	return can_subst && !add_substitution(ret) ? NULL : ret;
}

// <CV-qualifiers> ::= [r] [V] [K]
demangle_component **DemanglerGNU::cv_qualifiers(demangle_component **pret, bool member_fn) {
	char peek = peek_char();
	while (peek == 'r' || peek == 'V' || peek == 'K') {
		demangle_component::TYPE t;
		advance(1);
		if (peek == 'r') {
			t = member_fn ? demangle_component::RESTRICT_THIS : demangle_component::RESTRICT;
			expansion += sizeof "restrict";
		} else if (peek == 'V') {
			t = member_fn ? demangle_component::VOLATILE_THIS : demangle_component::VOLATILE;
			expansion += sizeof "volatile";
		} else {
			t = member_fn ? demangle_component::CONST_THIS : demangle_component::KONST;
			expansion += sizeof "const";
		}
		*pret = make_comp(t, NULL, NULL);
		if (!*pret)
			return NULL;
		pret = &(*pret)->left;
		peek = peek_char();
	}
	return pret;
}

// <function-type> ::= F [Y] <bare-function-type> E
demangle_component *DemanglerGNU::function_type() {
	if (!check_char('F'))
		return NULL;
	if (peek_char() == 'Y')	// Function has C linkage.  We don't print this information. FIXME: We should print it in verbose mode.
		advance(1);
	demangle_component *ret = bare_function_type(true);
	return !check_char('E') ? NULL : ret;
}

// <type>+
demangle_component *DemanglerGNU::parmlist() {
	demangle_component *tl = NULL;
	demangle_component **ptl = &tl;
	for (;;) {
		char peek = peek_char();
		if (peek == '\0' || peek == 'E')
			break;
		demangle_component *type = demangle_type();
		if (!type)
			return NULL;
		*ptl = make_comp(demangle_component::ARGLIST, type, NULL);
		if (!*ptl)
			return NULL;
		ptl = &(*ptl)->right;
	}

	// There should be at least one parameter type besides the optional return type.  A function which takes no arguments will have a single parameter type void.
	if (!tl)
		return NULL;

	// If we have a single parameter type void, omit it.
	if (tl->right == NULL &&  tl->left && tl->left->type == demangle_component::BUILTIN_TYPE && tl->left->builtin->print == D_PRINT_VOID) {
		expansion -= tl->left->builtin->name.len;
		tl->left = NULL;
	}

	return tl;
}

// <bare-function-type> ::= [J]<type>+
demangle_component *DemanglerGNU::bare_function_type(bool has_return_type) {
	// Detect special qualifier indicating that the first argument is the return type.
	char peek = peek_char();
	if (peek == 'J') {
		advance(1);
		has_return_type = true;
	}

	demangle_component *return_type = NULL;
	if (has_return_type && (!(return_type = demangle_type())))
		return NULL;

	demangle_component *tl = parmlist();
	return tl ? make_comp(demangle_component::FUNCTION_TYPE, return_type, tl) : NULL;
}

// <class-enum-type> ::= <name>
demangle_component *DemanglerGNU::class_enum_type() {
	return name();
}

/* <array-type> ::= A <(positive dimension) number> _ <(element) type>
::= A [<(dimension) expression>] _ <(element) type> */
demangle_component *DemanglerGNU::array_type() {
	if (!check_char('A'))
		return NULL;

	demangle_component *dim;
	char	peek = peek_char();
	if (peek == '_')
		dim = NULL;
	else if (is_digit(peek)) {
		const char *s = str();
		do {
			advance(1);
			peek = peek_char();
		} while (is_digit(peek));
		dim = make_name(demangle_string2(s, int(str() - s)));
		if (!dim)
			return NULL;
	} else {
		dim = expression();
		if (dim == NULL)
			return NULL;
	}
	return check_char('_') ? make_comp(demangle_component::ARRAY_TYPE, dim, demangle_type()) : NULL;
}

/* <vector-type> ::= Dv <number> _ <type>
::= Dv _ <expression> _ <type> */
demangle_component *DemanglerGNU::vector_type() {
	demangle_component *dim;
	char peek = peek_char();
	if (peek == '_') {
		advance(1);
		dim = expression();
	} else {
		dim = comps.push_back(new demangle_component(demangle_component::NUMBER, number()));
	}
	return dim && check_char('_') ? make_comp(demangle_component::VECTOR_TYPE, dim, demangle_type()) : NULL;
}

// <pointer-to-member-type> ::= M <(class) type> <(member) type>
demangle_component *DemanglerGNU::pointer_to_member_type() {
	if (!check_char('M'))
		return NULL;

	demangle_component *cl = demangle_type();

	/* The ABI specifies that any type can be a substitution source, and that M is followed by two types, and that when a CV-qualified
	type is seen both the base type and the CV-qualified types are substitution sources.  The ABI also specifies that for a pointer
	to a CV-qualified member function, the qualifiers are attached to the second type.  Given the grammar, a plain reading of the ABI
	suggests that both the CV-qualified member function and the non-qualified member function are substitution sources.  However,
	g++ does not work that way.  g++ treats only the CV-qualified member function as a substitution source.  FIXME.  So to work
	with g++, we need to pull off the CV-qualifiers here, in order to avoid calling add_substitution() in demangle_type().  But
	for a CV-qualified member which is not a function, g++ does follow the ABI, so we need to handle that case here by calling
	add_substitution ourselves.  */

	demangle_component *mem;
	demangle_component **pmem = cv_qualifiers(&mem, true);
	if (!pmem)
		return NULL;
	*pmem = demangle_type();
	if (!*pmem)
		return NULL;
	if (pmem != &mem && (*pmem)->type != demangle_component::FUNCTION_TYPE && !add_substitution(mem))
		return NULL;
	return make_comp(demangle_component::PTRMEM_TYPE, cl, mem);
}

// <non-negative number> _
int DemanglerGNU::compact_number() {
	int num;
	if (peek_char() == '_')
		num = 0;
	else if (peek_char() == 'n')
		return -1;
	else
		num = number() + 1;
	return check_char('_') ? num : -1;
}

/* <template-param> ::= T_
::= T <(parameter-2 non-negative) number> _ */
demangle_component *DemanglerGNU::template_param() {
	if (!check_char('T'))
		return NULL;
	int param = compact_number();
	if (param < 0)
		return NULL;
	++did_subs;
	return make_template_param(param);
}

// <template-args> ::= I <template-arg>+ E
demangle_component *DemanglerGNU::template_args() {
	/* Preserve the last name we saw--don't let the template arguments
	clobber it, as that would give us the wrong name for a subsequent
	constructor or destructor.  */
	demangle_component *hold_last_name = last_name;

	if (!check_char('I'))
		return NULL;

	if (peek_char() == 'E') {	// An argument pack can be empty.
		advance(1);
		return make_comp(demangle_component::TEMPLATE_ARGLIST, NULL, NULL);
	}

	demangle_component *al = NULL;
	demangle_component **pal = &al;
	for (;;) {
		demangle_component *a = template_arg();
		if (!a)
			return NULL;

		*pal = make_comp(demangle_component::TEMPLATE_ARGLIST, a, NULL);
		if (!*pal)
			return NULL;
		pal = &(*pal)->right;
		if (peek_char() == 'E') {
			advance(1);
			break;
		}
	}
	last_name = hold_last_name;
	return al;
}

/* <template-arg> ::= <type>
::= X <expression> E
::= <expr-primary>	*/
demangle_component *DemanglerGNU::template_arg() {
	switch (peek_char()) {
		case 'X': {
			advance(1);
			demangle_component *ret = expression();
			return check_char('E') ? ret : NULL;
		}
		case 'L':	return expr_primary();
		case 'I':	return template_args();		// An argument pack.
		default:	return demangle_type();
	}
}

// Subroutine of <expression> ::= cl <expression>+ E
demangle_component *DemanglerGNU::exprlist() {
	if (peek_char() == 'E') {
		advance(1);
		return make_comp(demangle_component::ARGLIST, NULL, NULL);
	}

	demangle_component *list = NULL;
	demangle_component **p = &list;
	for (;;) {
		demangle_component *arg = expression();
		if (arg == NULL)
			return NULL;

		*p = make_comp(demangle_component::ARGLIST, arg, NULL);
		if (!*p)
			return NULL;
		p = &(*p)->right;
		if (peek_char() == 'E') {
			advance(1);
			break;
		}
	}
	return list;
}

/* <expression> ::= <(unary) operator-name> <expression>
::= <(binary) operator-name> <expression> <expression>
::= <(trinary) operator-name> <expression> <expression> <expression>
::= cl <expression>+ E
::= st <type>
::= <template-param>
::= sr <type> <unqualified-name>
::= sr <type> <unqualified-name> <template-args>
::= <expr-primary> */
demangle_component *DemanglerGNU::expression() {
	char peek = peek_char();
	if (peek == 'L')
		return expr_primary();
	if (peek == 'T')
		return template_param();
	if (peek == 's' && peek_next_char() == 'r') {
		advance(2);
		demangle_component *type = demangle_type();
		demangle_component *name = unqualified_name();
		return peek_char() == 'I'
			? make_comp(demangle_component::QUAL_NAME, type, make_comp(demangle_component::TEMPLATE, name, template_args()))
			: make_comp(demangle_component::QUAL_NAME, type, name);
	}
	if (peek == 's' && peek_next_char() == 'p') {
		advance(2);
		return make_comp(demangle_component::PACK_EXPANSION, expression(), NULL);
	}
	if (peek == 'f' && peek_next_char() == 'p') {
		// Function parameter used in a late-specified return type.
		advance(2);
		int index = compact_number();
		if (index < 0)
			return NULL;
		return make_function_param(index);
	}
	if (is_digit(peek) || (peek == 'o' && peek_next_char() == 'n')) {
		// We can get an unqualified name as an expression in the case of a dependent function call, i.e. decltype(f(t)).
		if (peek == 'o')	// operator-function-id, i.e. operator+(t).
			advance(2);

		demangle_component *name = unqualified_name();
		if (!name)
			return NULL;
		return peek_char() == 'I'
			? make_comp(demangle_component::TEMPLATE, name, template_args())
			: name;
	}
	demangle_component *op;
	int args;

	op = operator_name();
	if (op == NULL)
		return NULL;

	if (op->type == demangle_component::OPERATOR)
		expansion += op->op->name.len - 2;

	if (op->type == demangle_component::OPERATOR
		&& strcmp(op->op->code, "st") == 0)
		return make_comp(demangle_component::UNARY, op, demangle_type());

	switch (op->type) {
		default:									return NULL;
		case demangle_component::OPERATOR:			args = op->op->args;	break;
		case demangle_component::EXTENDED_OPERATOR:	args = op->num;			break;
		case demangle_component::CAST:				args = 1;				break;
	}
	switch (args) {
		case 1: {
			demangle_component *operand = op->type == demangle_component::CAST && check_char('_')
				? exprlist()
				: expression();
			return make_comp(demangle_component::UNARY, op, operand);
		}
		case 2: {
			const char *code = op->op->code;
			demangle_component *left = expression();
			demangle_component *right;
			if (!strcmp(code, "cl"))
				right = exprlist();
			else if (!strcmp(code, "dt") || !strcmp(code, "pt")) {
				right = unqualified_name();
				if (peek_char() == 'I')
					right = make_comp(demangle_component::TEMPLATE, right, template_args());
			} else
				right = expression();
			return make_comp(demangle_component::BINARY, op, make_comp(demangle_component::BINARY_ARGS, left, right));
		}
		case 3: {
			demangle_component *first = expression();
			demangle_component *second = expression();
			return make_comp(demangle_component::TRINARY, op,
				make_comp(demangle_component::TRINARY_ARG1,
					first,
					make_comp(demangle_component::TRINARY_ARG2, second, expression())
				)
			);
		}
		default:
			return NULL;
	}
}

/* <expr-primary> ::= L <type> <(value) number> E
::= L <type> <(value) float> E
::= L <mangled-name> E */
demangle_component *DemanglerGNU::expr_primary() {
	if (!check_char('L'))
		return NULL;

	demangle_component *ret;
	if (peek_char() == '_' || peek_char() == 'Z') {// Workaround for G++ bug; see comment in write_template_arg.
		ret = demangle_mangled_name(false);
	} else {
		demangle_component *type = demangle_type();
		if (!type)
			return NULL;

		// If we have a type we know how to print, we aren't going to print the type name itself.
		if (type->type == demangle_component::BUILTIN_TYPE && type->builtin->print != D_PRINT_DEFAULT)
			expansion -= type->builtin->name.len;

		/* Rather than try to interpret the literal value, we just collect it as a string.  Note that it's possible to have a
		floating point literal here.  The ABI specifies that the format of such literals is machine independent.  That's fine,
		but what's not fine is that versions of g++ up to 3.2 with -fabi-version=1 used upper case letters in the hex constant,
		and dumped out gcc's internal representation.  That makes it hard to tell where the constant ends, and hard to dump the
		constant in any readable form anyhow.  We don't attempt to handle these cases.  */
		demangle_component::TYPE t = demangle_component::LITERAL;
		if (peek_char() == 'n') {
			t = demangle_component::LITERAL_NEG;
			advance(1);
		}
		const char *s = str();
		while (peek_char() != 'E') {
			if (peek_char() == '\0')
				return NULL;
			advance(1);
		}
		ret = make_comp(t, type, make_name(demangle_string2(s, int(str() - s))));
	}
	return check_char('E') ? ret : NULL;
}

/* <local-name> ::= Z <(function) encoding> E <(entity) name> [<discriminator>]
::= Z <(function) encoding> E s [<discriminator>] */
demangle_component *DemanglerGNU::local_name() {
	if (!check_char('Z'))
		return NULL;

	demangle_component *function = encoding(false);

	if (!check_char('E'))
		return NULL;

	if (peek_char() == 's') {
		advance(1);
		if (!discriminator())
			return NULL;
		return make_comp(demangle_component::LOCAL_NAME, function, make_name("string literal"));
	}

	int num = -1;
	if (peek_char() == 'd') {	// Default argument scope: d <number> _.
		advance(1);
		num = compact_number();
		if (num < 0)
			return NULL;
	}

	demangle_component *n = name();
	if (n) switch (n->type) {
		// Lambdas and unnamed types have internal discriminators.
		case demangle_component::LAMBDA:
		case demangle_component::UNNAMED_TYPE:
			break;
		default:
			if (!discriminator())
				return NULL;
	}
	if (num >= 0)
		n = make_default_arg(num, n);
	return make_comp(demangle_component::LOCAL_NAME, function, n);
}

// <discriminator> ::= _ <(non-negative) number>
// We demangle the discriminator, but we don't print it out.  FIXME: We should print it out in verbose mode.
bool DemanglerGNU::discriminator() {
	if (peek_char() != '_')
		return true;
	advance(1);
	return number() >= 0;
}

// <closure-type-name> ::= Ul <lambda-sig> E [ <nonnegative number> ] _
demangle_component *DemanglerGNU::lambda() {
	if (!check_char('U'))
		return NULL;
	if (!check_char('l'))
		return NULL;

	demangle_component *tl = parmlist();
	if (!tl)
		return NULL;
	if (!check_char('E'))
		return NULL;

	int num = compact_number();
	if (num < 0)
		return NULL;

	demangle_component *ret = new demangle_component(demangle_component::LAMBDA, tl, num);
	return add_substitution(ret) ? ret : NULL;
}

// <unnamed-type-name> ::= Ut [ <nonnegative number> ] _
demangle_component *DemanglerGNU::unnamed_type() {
	if (!check_char('U'))
		return NULL;
	if (!check_char('t'))
		return NULL;

	int num = compact_number();
	if (num < 0)
		return NULL;

	demangle_component *ret = comps.push_back(new demangle_component(demangle_component::UNNAMED_TYPE, num));
	return add_substitution(ret) ? ret : NULL;
}

// Add a new substitution.
bool DemanglerGNU::add_substitution(demangle_component *dc) {
	subs.push_back(dc);
	return true;
}

//-----------------------------------------------------------------------------
//	PrintInfo
//-----------------------------------------------------------------------------

/* <substitution> ::= S <seq-id> _
::= S_
::= St
::= Sa
::= Sb
::= Ss
::= Si
::= So
::= Sd

If PREFIX is non-zero, then this type is being used as a prefix in a qualified name.
In this case, for the standard substitutions, we need to check whether we are being used as a prefix for a constructor or destructor, and return a full template name.
Otherwise we will get something like std::iostream::~iostream() which does not correspond particularly well to any function which actually appears in the source.
*/
struct standard_sub_info {
	char			code;
	demangle_string	simple, full, set_last;
};
static const standard_sub_info standard_subs[] = {
	{ 't', NL("std"),				NL("std"),																		NULL, 0 },
	{ 'a', NL("std::allocator"),	NL("std::allocator"),															NL("allocator") },
	{ 'b', NL("std::basic_string"),	NL("std::basic_string"),														NL("basic_string") },
	{ 's', NL("std::string"),		NL("std::basic_string<char, std::char_traits<char>, std::allocator<char> >"),	NL("basic_string") },
	{ 'i', NL("std::istream"),		NL("std::basic_istream<char, std::char_traits<char> >"),						NL("basic_istream") },
	{ 'o', NL("std::ostream"),		NL("std::basic_ostream<char, std::char_traits<char> >"),						NL("basic_ostream") },
	{ 'd', NL("std::iostream"),		NL("std::basic_iostream<char, std::char_traits<char> >"),						NL("basic_iostream") }
};


enum { D_PRINT_BUFFER_LENGTH = 256 };
struct PrintInfo {
	struct print_template {
		struct print_template		*next;
		const demangle_component	*template_decl;
	};

	struct print_mod {
		struct print_mod			*next;
		const demangle_component	*mod;
		bool						printed;
		print_template				*templates;
	};

	int						options;
	char					buf[D_PRINT_BUFFER_LENGTH];
	size_t					len;
	char					last_char;
	demangle_callback		callback;
	void					*opaque;
	print_template			*templates;
	print_mod				*modifiers;
	bool					demangle_failure;
	int						pack_index;
	uint32					flush_count;

	PrintInfo(int _options, demangle_callback _callback, void *_opaque) : options(_options), callback(_callback), opaque(_opaque) {
		len			= 0;
		last_char	= '\0';
		templates	= NULL;
		modifiers	= NULL;
		flush_count	= 0;
		demangle_failure = false;
	}

	void print_error()		{ demangle_failure = true; }
	bool print_saw_error()	{ return demangle_failure; }

	void print_flush() {
		buf[len] = '\0';
		callback(buf, len, opaque);
		len = 0;
		flush_count++;
	}
	void append_char(char c) {
		if (len == sizeof(buf) - 1)
			print_flush();
		buf[len++] = c;
		last_char = c;
	}
	void append_buffer(const char *s, size_t l) {
		for (size_t i = 0; i < l; i++)
			append_char(s[i]);
	}
	void append(const demangle_string &s) {
		append_buffer(s.str, s.len);
	}
	template<int N> void append(const char (&s)[N]) {
		append_buffer(s, N - 1);
	}
	void append_num(int l) {
		char buf[25];
		sprintf(buf, "%ld", l);
		append_buffer(buf, strlen(buf));
	}

	void		print_comp(const demangle_component *);
	void		print_mod_list(print_mod *, bool);
	void		print_mod_comp(const demangle_component *);
	void		print_function_type(const demangle_component *, print_mod *);
	void		print_array_type(const demangle_component *, print_mod *);
	void		print_expr_op(const demangle_component *);
	void		print_cast(const demangle_component *);
	demangle_component *lookup_template_argument(const demangle_component *dc);
	demangle_component *find_pack(const demangle_component *dc);
	void		print_subexpr(const demangle_component *dc);
};

demangle_component *DemanglerGNU::substitution(bool prefix) {
	if (!check_char('S'))
		return NULL;

	char c = next_char();
	if (c == '_' || is_digit(c) || is_upper(c)) {
		unsigned id = 0;
		if (c != '_') {
			do {
				unsigned new_id;
				if (is_digit(c))
					new_id = id * 36 + c - '0';
				else if (is_upper(c))
					new_id = id * 36 + c - 'A' + 10;
				else
					return NULL;
				if (new_id < id)
					return NULL;
				id = new_id;
				c = next_char();
			} while (c != '_');
			++id;
		}

		if (id >= subs.size())
			return NULL;

		++did_subs;
		return subs[id];
	}

	bool verbose = !!(options & DMGL_VERBOSE);
	if (!verbose && prefix) {
		char peek = peek_char();
		if (peek == 'C' || peek == 'D')
			verbose = true;
	}

	const standard_sub_info *pend = (&standard_subs[0] + sizeof standard_subs / sizeof standard_subs[0]);
	for (const standard_sub_info *p = &standard_subs[0]; p < pend; ++p) {
		if (c == p->code) {
			if (p->set_last.str != NULL)
				last_name = make_sub(p->set_last);

			demangle_string	s = verbose ? p->full : p->simple;
			expansion += s.len;
			return make_sub(s);
		}
	}
	return NULL;
}


// Returns the I'th element of the template arglist ARGS, or NULL on failure.
static demangle_component *index_template_argument(demangle_component *args, int i) {
	demangle_component *a;

	for (a = args; a; a = a->right) {
		if (a->type != demangle_component::TEMPLATE_ARGLIST)
			return NULL;
		if (i <= 0)
			break;
		--i;
	}
	if (i != 0 || a == NULL)
		return NULL;
	return a->left;
}

// Returns the template argument from the current context indicated by DC, which is a demangle_component::TEMPLATE_PARAM, or NULL.
demangle_component *PrintInfo::lookup_template_argument(const demangle_component *dc) {
	if (!templates) {
		print_error();
		return NULL;
	}
	return index_template_argument(templates->template_decl->right, dc->number);
}

// Returns a template argument pack used in DC(any will do), or NULL.
demangle_component *PrintInfo::find_pack(const demangle_component *dc) {
	if (dc) switch (dc->type) {
		case demangle_component::TEMPLATE_PARAM:
		{
			demangle_component *a = lookup_template_argument(dc);
			if (a && a->type == demangle_component::TEMPLATE_ARGLIST)
				return a;
			break;
		}
		case demangle_component::PACK_EXPANSION:
		case demangle_component::LAMBDA:
		case demangle_component::NAME:
		case demangle_component::OPERATOR:
		case demangle_component::BUILTIN_TYPE:
		case demangle_component::SUB_STD:
		case demangle_component::CHARACTER:
		case demangle_component::FUNCTION_PARAM:
			break;
		case demangle_component::EXTENDED_OPERATOR:	return find_pack(dc->arg);
		case demangle_component::XTOR:				return find_pack(dc->s_xtor.name);
		default:
			if (demangle_component *a = find_pack(dc->left))
				return a;
			return find_pack(dc->right);
	}
	return NULL;
}

// Returns the length of the template argument pack DC.
static int pack_length(const demangle_component *dc) {
	int count = 0;
	while (dc && dc->type == demangle_component::TEMPLATE_ARGLIST && dc->left != NULL) {
		++count;
		dc = dc->right;
	}
	return count;
}

// DC is a component of a mangled expression.  Print it, wrapped in parens if needed.
void PrintInfo::print_subexpr(const demangle_component *dc) {
	bool simple = dc->type == demangle_component::NAME || dc->type == demangle_component::FUNCTION_PARAM;
	if (!simple)
		append_char('(');
	print_comp(dc);
	if (!simple)
		append_char(')');
}

// Subroutine to handle components.
void PrintInfo::print_comp(const demangle_component *dc) {
	if (!dc) {
		print_error();
		return;
	}
	if (print_saw_error())
		return;

	switch (dc->type) {
		case demangle_component::NAME:
			append(dc->string);
			return;

		case demangle_component::QUAL_NAME:
		case demangle_component::LOCAL_NAME:
			print_comp(dc->left);
			append("::");
			print_comp(dc->right);
			return;

		case demangle_component::TYPED_NAME:
		{
			/* Pass the name down to the type so that it can be printed in the right place for the type.  We also have to pass down
			any CV-qualifiers, which apply to the this parameter.  */
			print_mod adpm[4];
			print_mod *hold_modifiers = modifiers;
			modifiers = 0;
			unsigned	i = 0;
			demangle_component *typed_name = dc->left;
			while (typed_name) {
				if (i >= sizeof(adpm) / sizeof(adpm[0])) {
					print_error();
					return;
				}

				adpm[i].next = modifiers;
				modifiers = &adpm[i];
				adpm[i].mod = typed_name;
				adpm[i].printed = false;
				adpm[i].templates = templates;
				++i;

				if (typed_name->type != demangle_component::RESTRICT_THIS
					&& typed_name->type != demangle_component::VOLATILE_THIS
					&& typed_name->type != demangle_component::CONST_THIS)
					break;

				typed_name = typed_name->left;
			}

			if (!typed_name) {
				print_error();
				return;
			}

			// If typed_name is a template, then it applies to the function type as well.
			print_template dpt;
			if (typed_name->type == demangle_component::TEMPLATE) {
				dpt.next = templates;
				templates = &dpt;
				dpt.template_decl = typed_name;
			}

			/* If typed_name is a demangle_component::LOCAL_NAME, then there may be CV-qualifiers on its right argument which
			really apply here; this happens when parsing a class which is local to a function.  */
			if (typed_name->type == demangle_component::LOCAL_NAME) {
				demangle_component *local_name = typed_name->right;
				if (local_name->type == demangle_component::DEFAULT_ARG)
					local_name = local_name->arg;
				while (local_name->type == demangle_component::RESTRICT_THIS
					|| local_name->type == demangle_component::VOLATILE_THIS
					|| local_name->type == demangle_component::CONST_THIS) {
					if (i >= sizeof adpm / sizeof adpm[0]) {
						print_error();
						return;
					}

					adpm[i] = adpm[i - 1];
					adpm[i].next = &adpm[i - 1];
					modifiers = &adpm[i];

					adpm[i - 1].mod = local_name;
					adpm[i - 1].printed = false;
					adpm[i - 1].templates = templates;
					++i;

					local_name = local_name->left;
				}
			}

			print_comp(dc->right);

			if (typed_name->type == demangle_component::TEMPLATE)
				templates = dpt.next;

			// If the modifiers didn't get printed by the type, print them now.
			while (i--) {
				if (!adpm[i].printed) {
					append_char(' ');
					print_mod_comp(adpm[i].mod);
				}
			}

			modifiers = hold_modifiers;
			return;
		}
		case demangle_component::TEMPLATE: {
			/* Don't push modifiers into a template definition.  Doing so could give the wrong definition for a template argument.
			Instead, treat the template essentially as a name.  */

			print_mod			*hold_dpm = modifiers;
			demangle_component	*dcl = dc->left;

			modifiers = NULL;
			print_comp(dcl);
			if (last_char == '<')
				append_char(' ');
			append_char('<');
			print_comp(dc->right);
			// Avoid generating two consecutive '>' characters, to avoid the C++ syntactic ambiguity.
			if (last_char == '>')
				append_char(' ');
			append_char('>');

			modifiers = hold_dpm;
			return;
		}
		case demangle_component::TEMPLATE_PARAM: {
			demangle_component *a = lookup_template_argument(dc);
			if (a && a->type == demangle_component::TEMPLATE_ARGLIST)
				a = index_template_argument(a, pack_index);
			if (!a) {
				print_error();
				return;
			}
			/* While processing this parameter, we need to pop the list of templates.  This is because the template parameter may
			itself be a reference to a parameter of an outer template.  */
			print_template *hold_dpt = templates;
			templates = hold_dpt->next;
			print_comp(a);
			templates = hold_dpt;
			return;
		}
		case demangle_component::XTOR:
			print_comp(dc->s_xtor.name);
			return;
		case demangle_component::VTABLE:
			append("vtable for ");
			print_comp(dc->left);
			return;
		case demangle_component::VTT:
			append("VTT for ");
			print_comp(dc->left);
			return;
		case demangle_component::CONSTRUCTION_VTABLE:
			append("construction vtable for ");
			print_comp(dc->left);
			append("-in-");
			print_comp(dc->right);
			return;
		case demangle_component::TYPEINFO:
			append("typeinfo for ");
			print_comp(dc->left);
			return;
		case demangle_component::TYPEINFO_NAME:
			append("typeinfo name for ");
			print_comp(dc->left);
			return;
		case demangle_component::TYPEINFO_FN:
			append("typeinfo fn for ");
			print_comp(dc->left);
			return;
		case demangle_component::THUNK:
			append("non-virtual thunk to ");
			print_comp(dc->left);
			return;
		case demangle_component::VIRTUAL_THUNK:
			append("virtual thunk to ");
			print_comp(dc->left);
			return;
		case demangle_component::COVARIANT_THUNK:
			append("covariant return thunk to ");
			print_comp(dc->left);
			return;
		case demangle_component::GUARD:
			append("guard variable for ");
			print_comp(dc->left);
			return;
		case demangle_component::REFTEMP:
			append("reference temporary for ");
			print_comp(dc->left);
			return;
		case demangle_component::HIDDEN_ALIAS:
			append("hidden alias for ");
			print_comp(dc->left);
			return;
		case demangle_component::SUB_STD:
			append(dc->string);
			return;
		case demangle_component::RESTRICT:
		case demangle_component::VOLATILE:
		case demangle_component::KONST: {
			/* When printing arrays, it's possible to have cases where the same CV-qualifier gets pushed on the stack multiple times.
			We only need to print it once.  */
			for (print_mod *pdpm = modifiers; pdpm != NULL; pdpm = pdpm->next) {
				if (!pdpm->printed) {
					if (pdpm->mod->type != demangle_component::RESTRICT
						&& pdpm->mod->type != demangle_component::VOLATILE
						&& pdpm->mod->type != demangle_component::KONST)
						break;
					if (pdpm->mod->type == dc->type) {
						print_comp(dc->left);
						return;
					}
				}
			}
		}
		// Fall through.
		case demangle_component::RESTRICT_THIS:
		case demangle_component::VOLATILE_THIS:
		case demangle_component::CONST_THIS:
		case demangle_component::VENDOR_TYPE_QUAL:
		case demangle_component::POINTER:
		case demangle_component::REFERENCE:
		case demangle_component::RVALUE_REFERENCE:
		case demangle_component::COMPLEX:
		case demangle_component::IMAGINARY: {
			// We keep a list of modifiers on the stack.
			print_mod dpm;
			dpm.next = modifiers;
			modifiers = &dpm;
			dpm.mod = dc;
			dpm.printed = false;
			dpm.templates = templates;

			print_comp(dc->left);

			// If the modifier didn't get printed by the type, print it now.
			if (!dpm.printed)
				print_mod_comp(dc);
			modifiers = dpm.next;
			return;
		}
		case demangle_component::BUILTIN_TYPE:
			append(dc->builtin->name);
			return;
		case demangle_component::VENDOR_TYPE:
			print_comp(dc->left);
			return;
		case demangle_component::FUNCTION_TYPE: {
			if ((options & DMGL_RET_POSTFIX) != 0)
				print_function_type(dc, modifiers);
			// Print return type if present
			if (dc->left) {
				// We must pass this type down as a modifier in order to print it in the right location.
				print_mod dpm;
				dpm.next = modifiers;
				dpm.mod = dc;
				dpm.printed = false;
				dpm.templates = templates;

				modifiers = &dpm;
				print_comp(dc->left);
				modifiers = dpm.next;

				if (dpm.printed)
					return;

				// In standard prefix notation, there is a space between the return type and the function signature.
				if ((options & DMGL_RET_POSTFIX) == 0)
					append_char(' ');
			}

			if ((options & DMGL_RET_POSTFIX) == 0)
				print_function_type(dc, modifiers);
			return;
		}
		case demangle_component::ARRAY_TYPE: {
			/* We must pass this type down as a modifier in order to print multi-dimensional arrays correctly.  If the array itself is
			CV-qualified, we act as though the element type were CV-qualified.  We do this by copying the modifiers down
			rather than fiddling pointers, so that we don't wind up with a print_mod higher on the stack pointing into our
			stack frame after we return.  */
			print_mod *hold_modifiers = modifiers;
			print_mod adpm[4];
			adpm[0].next = hold_modifiers;
			adpm[0].mod = dc;
			adpm[0].printed = false;
			adpm[0].templates = templates;
			modifiers = &adpm[0];

			unsigned i = 1;
			print_mod *pdpm = hold_modifiers;
			while (pdpm && (
				pdpm->mod->type == demangle_component::RESTRICT
				|| pdpm->mod->type == demangle_component::VOLATILE
				|| pdpm->mod->type == demangle_component::KONST
				)) {
				if (!pdpm->printed) {
					if (i >= sizeof adpm / sizeof adpm[0]) {
						print_error();
						return;
					}
					adpm[i] = *pdpm;
					adpm[i].next = modifiers;
					modifiers = &adpm[i];
					pdpm->printed = true;
					++i;
				}
				pdpm = pdpm->next;
			}

			print_comp(dc->right);
			modifiers = hold_modifiers;
			if (adpm[0].printed)
				return;

			while (--i > 0)
				print_mod_comp(adpm[i].mod);

			print_array_type(dc, modifiers);
			return;
		}
		case demangle_component::PTRMEM_TYPE:
		case demangle_component::VECTOR_TYPE: {
			print_mod dpm;
			dpm.next = modifiers;
			dpm.mod = dc;
			dpm.printed = false;
			dpm.templates = templates;
			modifiers = &dpm;

			print_comp(dc->right);
			// If the modifier didn't get printed by the type, print it now.
			if (!dpm.printed)
				print_mod_comp(dc);
			modifiers = dpm.next;
			return;
		}
		case demangle_component::FIXED_TYPE:
			if (dc->s_fixed.sat)
				append("_Sat ");
			// Don't print "int _Accum".
			if (dc->s_fixed.length->builtin != &demangle_builtin_types['i' - 'a']) {
				print_comp(dc->s_fixed.length);
				append_char(' ');
			}
			dc->s_fixed.accum ? append("_Accum") : append("_Fract");
			return;
		case demangle_component::ARGLIST:
		case demangle_component::TEMPLATE_ARGLIST:
			if (dc->left != NULL)
				print_comp(dc->left);
			if (dc->right) {
				// Make sure ", " isn't flushed by append, otherwise len -= 2 wouldn't work.
				if (len >= sizeof(buf) - 2)
					print_flush();
				append(", ");
				size_t		my_len = len;
				uint32		my_flush_count = flush_count;
				print_comp(dc->right);
				// If that didn't print anything(which can happen with empty template argument packs), remove the comma and space.
				if (flush_count == my_flush_count && len == my_len)
					len -= 2;
			}
			return;
		case demangle_component::OPERATOR: {
			append("operator");
			char c = dc->op->name.str[0];
			if (is_lower(c))
				append_char(' ');
			append(dc->op->name);
			return;
		}
		case demangle_component::EXTENDED_OPERATOR:
			append("operator ");
			print_comp(dc->arg);
			return;
		case demangle_component::CAST:
			append("operator ");
			print_cast(dc);
			return;
		case demangle_component::UNARY:
			if (dc->left->type != demangle_component::CAST)
				print_expr_op(dc->left);
			else {
				append_char('(');
				print_cast(dc->left);
				append_char(')');
			}
			print_subexpr(dc->right);
			return;
		case demangle_component::BINARY: {
			if (dc->right->type != demangle_component::BINARY_ARGS) {
				print_error();
				return;
			}
			/* We wrap an expression which uses the greater-than operator in an extra layer of parens so that it does not get confused
			with the '>' which ends the template parameters.  */
			bool	paren = dc->left->type == demangle_component::OPERATOR && dc->left->op->name == ">";
			if (paren)
				append_char('(');

			print_subexpr(dc->right->left);
			if (strcmp(dc->left->op->code, "ix") == 0) {
				append_char('[');
				print_comp(dc->right->right);
				append_char(']');
			} else {
				if (strcmp(dc->left->op->code, "cl") != 0)
					print_expr_op(dc->left);
				print_subexpr(dc->right->right);
			}

			if (paren)
				append_char(')');
			return;
		}
		case demangle_component::BINARY_ARGS: // We should only see this as part of demangle_component::BINARY.
			print_error();
			return;
		case demangle_component::TRINARY:
			if (dc->right->type != demangle_component::TRINARY_ARG1 || dc->right->right->type != demangle_component::TRINARY_ARG2) {
				print_error();
				return;
			}
			print_subexpr(dc->right->left);
			print_expr_op(dc->left);
			print_subexpr(dc->right->right->left);
			append(" : ");
			print_subexpr(dc->right->right->right);
			return;
		case demangle_component::TRINARY_ARG1:
		case demangle_component::TRINARY_ARG2: // We should only see these are part of demangle_component::TRINARY.
			print_error();
			return;
		case demangle_component::LITERAL:
		case demangle_component::LITERAL_NEG: {
			// For some builtin types, produce simpler output.
			builtin_type_print tp = D_PRINT_DEFAULT;
			if (dc->left->type == demangle_component::BUILTIN_TYPE) {
				tp = dc->left->builtin->print;
				switch (tp) {
					case D_PRINT_INT:
					case D_PRINT_UNSIGNED:
					case D_PRINT_LONG:
					case D_PRINT_UNSIGNED_LONG:
					case D_PRINT_LONG_LONG:
					case D_PRINT_UNSIGNED_LONG_LONG:
						if (dc->right->type == demangle_component::NAME) {
							if (dc->type == demangle_component::LITERAL_NEG)
								append_char('-');
							print_comp(dc->right);
							switch (tp) {
								default:							break;
								case D_PRINT_UNSIGNED:				append_char('u');		break;
								case D_PRINT_LONG:					append_char('l');		break;
								case D_PRINT_UNSIGNED_LONG:			append("ul");	break;
								case D_PRINT_LONG_LONG:				append("ll");	break;
								case D_PRINT_UNSIGNED_LONG_LONG:	append("ull");	break;
							}
							return;
						}
						break;
					case D_PRINT_BOOL:
						if (dc->right->type == demangle_component::NAME && dc->right->string.len == 1 && dc->type == demangle_component::LITERAL) {
							switch (dc->right->string.str[0]) {
								case '0':	append("false");	return;
								case '1':	append("true");	return;
								default:	break;
							}
						}
						break;
					default:
						break;
				}
			}
			append_char('(');
			print_comp(dc->left);
			append_char(')');
			if (dc->type == demangle_component::LITERAL_NEG)
				append_char('-');
			if (tp == D_PRINT_FLOAT)
				append_char('[');
			print_comp(dc->right);
			if (tp == D_PRINT_FLOAT)
				append_char(']');
			return;
		}
		case demangle_component::NUMBER:
			append_num(dc->number);
			return;
		case demangle_component::COMPOUND_NAME:
			print_comp(dc->left);
			print_comp(dc->right);
			return;
		case demangle_component::CHARACTER:
			append_char(dc->number);
			return;
		case demangle_component::DECLTYPE:
			append("decltype(");
			print_comp(dc->left);
			append_char(')');
			return;
		case demangle_component::PACK_EXPANSION: {
			demangle_component *a = find_pack(dc->left);
			if (!a) {	// find_pack won't find anything if the only packs involved in this expansion are function parameter packs; in that case, just print the pattern and "...".
				print_subexpr(dc->left);
				append("...");
				return;
			}
			int	len = pack_length(a);
			dc = dc->left;
			for (int i = 0; i < len; ++i) {
				pack_index = i;
				print_comp(dc);
				if (i < len - 1)
					append(", ");
			}
			return;
		}
		case demangle_component::FUNCTION_PARAM:
			append("{parm#");
			append_num(dc->number + 1);
			append_char('}');
			return;
		case demangle_component::GLOBAL_CONSTRUCTORS:
			append("global constructors keyed to ");
			print_comp(dc->left);
			return;
		case demangle_component::GLOBAL_DESTRUCTORS:
			append("global destructors keyed to ");
			print_comp(dc->left);
			return;
		case demangle_component::LAMBDA:
			append("{lambda(");
			print_comp(dc->arg);
			append(")#");
			append_num(dc->num + 1);
			append_char('}');
			return;
		case demangle_component::UNNAMED_TYPE:
			append("{unnamed type#");
			append_num(dc->number + 1);
			append_char('}');
			return;
		default:
			print_error();
			return;
	}
}

// Print a list of modifiers.  SUFFIX is 1 if we are printing qualifiers on this after printing a function.
void PrintInfo::print_mod_list(print_mod *mods, bool suffix) {
	if (mods == NULL || print_saw_error())
		return;

	if (mods->printed || (!suffix &&
		(mods->mod->type == demangle_component::RESTRICT_THIS
		|| mods->mod->type == demangle_component::VOLATILE_THIS
		|| mods->mod->type == demangle_component::CONST_THIS
		)
	)) {
		print_mod_list(mods->next, suffix);
		return;
	}

	mods->printed = true;

	print_template *hold_dpt = templates;
	templates = mods->templates;

	if (mods->mod->type == demangle_component::FUNCTION_TYPE) {
		print_function_type(mods->mod, mods->next);
		templates = hold_dpt;
		return;
	}
	if (mods->mod->type == demangle_component::ARRAY_TYPE) {
		print_array_type(mods->mod, mods->next);
		templates = hold_dpt;
		return;
	}
	if (mods->mod->type == demangle_component::LOCAL_NAME) {
		/* When this is on the modifier stack, we have pulled any qualifiers off the right argument already.  Otherwise, we
		print it as usual, but don't let the left argument see any modifiers.  */
		print_mod *hold_modifiers = modifiers;
		modifiers = NULL;
		print_comp(mods->mod->left);
		modifiers = hold_modifiers;

		append("::");

		demangle_component *dc = mods->mod->right;
		if (dc->type == demangle_component::DEFAULT_ARG) {
			append("{default arg#");
			append_num(dc->num + 1);
			append("}::");
			dc = dc->arg;
		}

		while (dc->type == demangle_component::RESTRICT_THIS
			|| dc->type == demangle_component::VOLATILE_THIS
			|| dc->type == demangle_component::CONST_THIS)
			dc = dc->left;

		print_comp(dc);
		templates = hold_dpt;
		return;
	}

	print_mod_comp(mods->mod);
	templates = hold_dpt;
	print_mod_list(mods->next, suffix);
}

// Print a modifier.
void PrintInfo::print_mod_comp(const demangle_component *mod) {
	switch (mod->type) {
		case demangle_component::RESTRICT:
		case demangle_component::RESTRICT_THIS:		append(" restrict");	return;
		case demangle_component::VOLATILE:
		case demangle_component::VOLATILE_THIS:		append(" volatile");	return;
		case demangle_component::KONST:
		case demangle_component::CONST_THIS:			append(" const");		return;
		case demangle_component::VENDOR_TYPE_QUAL:	append_char(' '); print_comp(mod->right); return;
		case demangle_component::POINTER:			append_char('*');		return;
		case demangle_component::REFERENCE:			append_char('&');		return;
		case demangle_component::RVALUE_REFERENCE:	append("&&");			return;
		case demangle_component::COMPLEX:			append("complex ");		return;
		case demangle_component::IMAGINARY:			append("imaginary ");	return;
		case demangle_component::PTRMEM_TYPE:
			if (last_char != '(')
				append_char(' ');
			print_comp(mod->left);
			append("::*");
			return;
		case demangle_component::TYPED_NAME:			print_comp(mod->left);	return;
		case demangle_component::VECTOR_TYPE:
			append(" __vector(");
			print_comp(mod->left);
			append_char(')');
			return;

		default:
			// Otherwise, we have something that won't go back on the modifier stack, so we can just print it.
			print_comp(mod);
			return;
	}
}

// Print a function type, except for the return type.
void PrintInfo::print_function_type(const demangle_component *dc, print_mod *mods) {
	bool need_paren = false;
	bool need_space = false;
	for (print_mod *p = mods; p && !need_paren; p = p->next) {
		if (p->printed)
			break;
		switch (p->mod->type) {
			case demangle_component::RESTRICT:
			case demangle_component::VOLATILE:
			case demangle_component::KONST:
			case demangle_component::VENDOR_TYPE_QUAL:
			case demangle_component::COMPLEX:
			case demangle_component::IMAGINARY:
			case demangle_component::PTRMEM_TYPE:
				need_space = true;
			case demangle_component::POINTER:
			case demangle_component::REFERENCE:
			case demangle_component::RVALUE_REFERENCE:
				need_paren = true;
			default:
				break;
		}
	}
	if (need_paren) {
		need_space = need_space || (last_char != '(' && last_char != '*');
		if (need_space && last_char != ' ')
			append_char(' ');
		append_char('(');
	}

	print_mod *hold_modifiers = modifiers;
	modifiers = NULL;
	print_mod_list(mods, false);
	if (need_paren)
		append_char(')');
	append_char('(');
	if (dc->right)
		print_comp(dc->right);
	append_char(')');
	print_mod_list(mods, true);
	modifiers = hold_modifiers;
}

// Print an array type, except for the element type.
void PrintInfo::print_array_type(const demangle_component *dc, print_mod *mods) {
	bool need_space = true;
	if (mods) {
		bool need_paren = false;
		for (print_mod *p = mods; p != NULL; p = p->next) {
			if (!p->printed) {
				if (p->mod->type == demangle_component::ARRAY_TYPE) {
					need_space = false;
					break;
				} else {
					need_paren = need_space = true;
					break;
				}
			}
		}
		if (need_paren)
			append("(");
		print_mod_list(mods, false);
		if (need_paren)
			append_char(')');
	}
	if (need_space)
		append_char(' ');
	append_char('[');
	if (dc->left)
		print_comp(dc->left);
	append_char(']');
}

// Print an operator in an expression.
void PrintInfo::print_expr_op(const demangle_component *dc) {
	if (dc->type == demangle_component::OPERATOR)
		append(dc->op->name);
	else
		print_comp(dc);
}

// Print a cast.
void PrintInfo::print_cast(const demangle_component *dc) {
	if (dc->left->type != demangle_component::TEMPLATE) {
		print_comp(dc->left);
		return;
	}
	/* It appears that for a templated cast operator, we need to put the template parameters in scope for the operator name, but
	not for the parameters.  The effect is that we need to handle the template printing here.  */
	print_mod *hold_dpm = modifiers;
	modifiers = NULL;

	print_template dpt;
	dpt.next = templates;
	templates = &dpt;
	dpt.template_decl = dc->left;

	print_comp(dc->left->left);
	templates = dpt.next;

	if (last_char == '<')
		append_char(' ');
	append_char('<');
	print_comp(dc->left->right);
	// Avoid generating two consecutive '>' characters, to avoid the C++ syntactic ambiguity.
	if (last_char == '>')
		append_char(' ');
	append_char('>');
	modifiers = hold_dpm;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

/* Turn components into a human readable string.  OPTIONS is the options bits passed to the demangler.  DC is the tree to print.
CALLBACK is a function to call to flush demangled string segments as they fill the intermediate buffer, and OPAQUE is a generalized
callback argument.  On success, this returns 1.  On failure, it returns 0, indicating a bad parse.  It does not use heap
memory to build an output string, so cannot encounter memory allocation failure.  */
bool demangle_print(int options, const demangle_component *dc, demangle_callback callback, void *opaque) {
	PrintInfo dpi(options, callback, opaque);
	dpi.print_comp(dc);
	dpi.print_flush();
	return !dpi.print_saw_error();
}

/* Turn components into a human readable string.  OPTIONS is the options bits passed to the demangler.  DC is the tree to print.
ESTIMATE is a guess at the length of the result.  This returns a string allocated by malloc, or NULL on error.  On success, this
sets *PALC to the size of the allocated buffer.  On failure, this sets *PALC to 0 for a bad parse, or to 1 for a memory allocation
failure.  */

iso::string demangle_print(int options, const demangle_component *dc) {
	iso::string_builder	builder;
	if (demangle_print(options, dc, [](const char *s, size_t l, void *opaque) {
		*(iso::string_builder*)opaque << iso::str(s, l);
	}, &builder))
		return builder;

	return 0;
}

//If MANGLED is a g++ v3 ABI mangled name, return strings in repeated callback giving the demangled name
//OPTIONS is the usual libiberty demangler options
bool demangle(const char *mangled, int options, demangle_callback callback, void *opaque) {
	enum {
		DCT_TYPE,
		DCT_MANGLED,
		DCT_GLOBAL_CTORS,
		DCT_GLOBAL_DTORS
	} type;

	if (mangled[0] == '_' && mangled[1] == 'Z')
		type = DCT_MANGLED;
	else if (strncmp(mangled, "_GLOBAL_", 8) == 0
		&& (mangled[8] == '.' || mangled[8] == '_' || mangled[8] == '$')
		&& (mangled[9] == 'D' || mangled[9] == 'I')
		&& mangled[10] == '_') {
		type = mangled[9] == 'I' ? DCT_GLOBAL_CTORS : DCT_GLOBAL_DTORS;
		mangled += 11;
	} else {
		if ((options & DMGL_TYPES) == 0)
			return false;
		type = DCT_TYPE;
	}

	DemanglerGNU	di(mangled, options, strlen(mangled));

	demangle_component	*dc;
	switch (type) {
		case DCT_TYPE:
			dc = di.demangle_type();
			break;
		case DCT_MANGLED:
			dc = di.demangle_mangled_name(true);
			break;
		case DCT_GLOBAL_CTORS:
		case DCT_GLOBAL_DTORS:
			dc = di.make_comp(
				type == DCT_GLOBAL_CTORS ? demangle_component::GLOBAL_CONSTRUCTORS : demangle_component::GLOBAL_DESTRUCTORS,
				di.make_demangle_mangled_name(di.str()),
				NULL
			);
			di.advance(strlen(di.str()));
			break;
	}

	// If DMGL_PARAMS is set, then if we didn't consume the entire mangled string, then we didn't successfully demangle it
	//If DMGL_PARAMS is not set, we didn't look at the trailing parameters
	if ((options & DMGL_PARAMS) && di.peek_char() != '\0')
		return false;

	return dc && demangle_print(options, dc, callback, opaque);
}

iso::string demangle(const char *mangled, int options) {
	iso::string_builder	builder;

	if (demangle(mangled, options, [](const char *s, size_t l, void *opaque) {
		*(iso::string_builder*)opaque << iso::str(s, l);
	}, &builder))
		return builder;
	return 0;
}

// Demangle a string in order to find out whether it is a constructor or destructor
gnu_v3_xtor_kinds is_ctor_or_dtor(const char *mangled) {
	DemanglerGNU di(mangled, DMGL_GNU_V3, strlen(mangled));

	// Note that because we did not pass DMGL_PARAMS, we don't expect to demangle the entire string.
	for (demangle_component *dc = di.demangle_mangled_name(true); dc;) {
		switch (dc->type) {
			default:								return gnu_v3_not_ctor_or_dtor;
			case demangle_component::TYPED_NAME:
			case demangle_component::TEMPLATE:
			case demangle_component::RESTRICT_THIS:
			case demangle_component::VOLATILE_THIS:
			case demangle_component::CONST_THIS:	dc = dc->left; break;
			case demangle_component::QUAL_NAME:
			case demangle_component::LOCAL_NAME:	dc = dc->right; break;
			case demangle_component::XTOR:			return dc->s_xtor.kind;
		}
	}
	return gnu_v3_not_ctor_or_dtor;
}
