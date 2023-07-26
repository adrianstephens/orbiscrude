#include "demangle.h"
#include "demangle2.h"
#include "base/array.h"
#include "base/algorithm.h"

#ifdef PLAT_PC
#include <windows.h>
#include <DbgHelp.h>
#endif

using namespace iso;

namespace iso {
template<typename T> struct prefix_of;
template<typename T> class string_base<prefix_of<T*> > : public string_base<T*> {
public:
	string_base(T *_p) : string_base<prefix_of<T*> >(_p)	{}
};
template<typename T, typename T2> inline int compare(const string_base<prefix_of<T*> > &s1, const T2 *s2) {
	return string_cmp(s1.begin(), s2, s1.length());
}
typedef const string_base<prefix_of<const char*> >	prefix;
}

//-----------------------------------------------------------------------------
//	General demangling
//-----------------------------------------------------------------------------

Demangler::Record::~Record() {
	if (child && !(child->flags & FLAG_SUBST))
		delete child;
	switch (type) {
		case DR_name:
			if (!(flags & FLAG_BUILTIN))
				iso::free((void*)name);
			break;
		case DR_nested:
		case DR_pointer2member:
			if (child2 && !(child2->flags & FLAG_SUBST))
				delete child2;
			break;
		default:
			break;
	}
}

//-----------------------------------------------------------------------------
//	print demangling
//-----------------------------------------------------------------------------

class CodeDemangler : public Demangler {
	static const char *operator_names[];
	bool	Print(Record *r, string_accum &sa, const char *name = 0);
public:
	CodeDemangler(Record *r) { root = r; }
	void Enmangle(iso::string_accum &sa) {
		Print(root, sa, 0);
	}
};

const char *CodeDemangler::operator_names[] = {
	" new",		" new[]",	" delete",	" delete[]","+",		"-",		"&",		"*",
	"~",		"+",		"-",		"*",		"/",		"%",		"&",		"|",
	"^",		"=",		"+=",		"-=",		"*=",		"/=",		"%=",		"&=",
	"|=",		"^=",		"<<",		">>",		"<<=",		">>=",		"==",		"!=",
	"<",		">",		"<=",		">=",		"!",		"&&",		"||",		"++(int)",
	"--(int)",	",",		"->*",		"->",		"()",		"[]",		"?:",
};


bool CodeDemangler::Print(Record *r, string_accum &sa, const char *name) {
	if (r) {
		if (r->type != DR_function) {
			if (r->flags & FLAG_RESTRICT)
				sa << "restrict ";
			if (r->flags & FLAG_VOLATILE)
				sa << "volatile ";
			if (r->flags & FLAG_CONST)
				sa << "const ";
		}
		switch (r->type) {
			case DR_name:
				sa << r->name;
				break;

			case DR_literal:
				sa << r->value;
				break;

			case DR_list:
				if (!Print(r->child, sa))
					return false;
				sa << ", ";
				return Print(r->child2, sa);

			case DR_local:
				if (!Print(r->child, sa))
					return false;
				sa << "::";
				return Print(r->child2, sa);

			case DR_nested:
				if (!Print(r->child, sa))
					return false;
				sa << "::";
				return Print(r->child2, sa);

			case DR_function:
				if (!Print(r->child, sa))
					return false;
				sa << '(';
				if (!Print(r->child2, sa))
					return false;
				sa << ')';
				if (r->flags & FLAG_RESTRICT)
					sa << " restrict";
				if (r->flags & FLAG_VOLATILE)
					sa << " volatile";
				if (r->flags & FLAG_CONST)
					sa << " const";
				break;

			case DR_rettype:
				Print(r->child2, sa);
				sa << ' ';
				Print(r->child, sa);
				break;

			case DR_template_arg:
				if (!Print(r->child, sa))
					return false;
				sa << '<';
				if (!Print(r->child2, sa))
					return false;
				sa << '>';
				break;

			case DR_array:
				if (!Print(r->child, sa))
					return false;
				sa << '[' << r->value << ']';
				break;

			case DR_pointer:
				if (!Print(r->child, sa))
					return false;
				sa << '*';
				break;

			case DR_reference:
				if (!Print(r->child, sa))
					return false;
				sa << '&';
				break;
	//		case DR_rvalue_ref:
	//		case DR_complex:
	//		case DR_imaginary:

			case DR_string:
				sa << "string " << r->value << " in ";
				return Print(r->child, sa);

			case DR_guard_variable:
				sa << "guard variable for ";
				return Print(r->child, sa);

			case DR_operator:
				if (r->value < OP_qu) {
					sa << "operator" << operator_names[r->value];
				} else switch (r->value) {
					case OP_st: case OP_sz: sa << "sizeof("; Print(r->child, sa); sa << ')'; break;
					case OP_at:	case OP_az: sa << "alignof("; Print(r->child, sa); sa << ')'; break;
					case OP_cv:	sa << "operator "; Print(r->child, sa); sa << "()"; break;
					case OP_Ut:	sa << "unnamed"; break;

					case OP_C1:	// complete object constructor
					case OP_C2:	// base object constructor
					case OP_C3:	sa << (name ? name : "constructor"); break; // complete object allocating constructor
					case OP_D0:	// deleting destructor
					case OP_D1:	// complete object destructor
					case OP_D2:	sa << '~' << (name ? name : "constructor"); break; // base object destructor
				}
				break;

			default:
				return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//	GCC demangling
//-----------------------------------------------------------------------------

class GCCDemangler : public Demangler {
	friend	bool cw_qual2(string_accum &sa, demangle_component *dc);

	enum TYPE {
		TYPE_void,		TYPE_wchar,			TYPE_bool,		TYPE_char,		TYPE_schar,	TYPE_uchar,		TYPE_short,	TYPE_ushort,
		TYPE_int,		TYPE_uint,			TYPE_long,		TYPE_ulong,		TYPE_int64,	TYPE_uint64,	TYPE_int128,	TYPE_uint128,
		TYPE_float,		TYPE_double,		TYPE_float80,	TYPE_float128,
		TYPE_ellipsis,	TYPE_vendor,
		TYPE_function,	TYPE_array,			TYPE_pointer,			TYPE_reference,			TYPE_rvalue_ref,
		TYPE_complex,	TYPE_imaginary,		TYPE_pointer2member,	TYPE_templateparam,		TYPE_vendor_qual,
		TYPE_nested,	TYPE_substitution,	TYPE_extended,

		_TYPE_null,
		TYPE_error,

		_TYPE_extended,
		TYPE_IEEE754rfloat64	= _TYPE_extended,
		TYPE_IEEE754rfloat128,	TYPE_IEEE754rfloat32,	TYPE_IEEE754rfloat16,
		TYPE_char32,	TYPE_char16,
		TYPE_auto,		TYPE_nullptr,

		TYPE_extended_error,
	};

	static const char	types[];
	static const char	extended_types[];
	static const char*	type_names[];
	static const char*	extended_type_names[];
	static const uint16	operators[];

	dynamic_array<Record*>	substitutions;

	void	AddSub(Record *r) {
		if (r) {
			r->flags |= FLAG_SUBST;
			substitutions.push_back(r);
		}
	}

	void	RemoveSub(Record *r) {
		if (r && (r->flags & FLAG_SUBST)) {
			auto i = find(substitutions, r);
			if (i != substitutions.end()) {
				substitutions.erase(i);
				r->flags &= ~FLAG_SUBST;
			}
		}
	}


	static int		GetCVQualifiers(const char *&m);
	static int		GetNumber36(const char *&m);
	static int		GetNumber10(const char *&m);

	Record*			GetUnqualifiedName(const char *&m);
	Record*			GetType(const char *&m);
	Record*			GetTemplateArgs(const char *&m);
	Record*			GetNestedName(const char *&m);
	Record*			GetName(const char *&m);
	Record*			GetEncoding(const char *&m);
public:
	GCCDemangler(const char *m)	{
		m += 2;
		root = GetEncoding(m);
	}
	~GCCDemangler();

	static bool	IsMangled(const char *m) { return str(m, 2) == "_Z" && !str(m).find('.'); }
};

const char GCCDemangler::types[]			= "vwbcahstijlmxynofdegzuFAPROCGMTUNSD";
const char GCCDemangler::extended_types[]	= "defhisan";

const char* GCCDemangler::type_names[] = {
	"void",		"wchar_t",		"bool",			"char",				"signed char",	"unsigned char",	"short",	"unsigned short",
	"int",		"unsigned int",	"long",			"unsigned long",	"__int64",		"unsigned __int64",	"__int128",	"unsigned __int128",
	"float",	"double",		"long double",	"__float128",
	"...",
};
const char* GCCDemangler::extended_type_names[] = {
	"IEEE 754r decimal floating point (64 bits)",
	"IEEE 754r decimal floating point (128 bits)",
	"IEEE 754r decimal floating point (32 bits)",
	"IEEE 754r half-precision floating point (16 bits)",
	"char32_t",	"char16_t",	"auto",	"std::nullptr_t",
};

const uint16 GCCDemangler::operators[] = {
	'nw','na','dl','da','ps','ng','ad','de',
	'co','pl','mi','ml','dv','rm','an','or',
	'eo','aS','pL','mI','mL','dV','rM','aN',
	'oR','eO','ls','rs','lS','rS','eq','ne',
	'lt','gt','le','ge','nt','aa','oo','pp',
	'mm','cm','pm','pt','cl','ix','qu','st',
	'sz','at','az','cv',//'v0'
	'Ut',
	'C1','C2','C3','D0','D1','D2',
	//	'Dt','DT',
	'gs', 'ti', 'te', 'dc', 'sc', 'rc', 'dt', 'ds', 'sp', 'tw', 'tr', 'sr',
};

GCCDemangler::~GCCDemangler() {
	if (root && !(root->flags & FLAG_SUBST))
		delete root;
	for (size_t i = substitutions.size(); i--; )
		delete substitutions[i];
}


int GCCDemangler::GetCVQualifiers(const char *&m) {
	int	flags = 0;
	if (*m == 'r') {
		flags |= FLAG_RESTRICT;
		m++;
	}
	if (*m == 'V') {
		flags |= FLAG_VOLATILE;
		m++;
	}
	if (*m == 'K') {
		flags |= FLAG_CONST;
		m++;
	}
	return flags;
}

int GCCDemangler::GetNumber36(const char *&m) {
	int		v	= 0;
	char	c	= *m++;
	if (c != '_') {
		while ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) {
			v = v * 36 + c - (c >= 'A' ? 'A' - 10 : '0');
			c = *m++;
		}
		v += 1;
	}
	return v;
}

int GCCDemangler::GetNumber10(const char *&m) {
	char	c	= *m;
	bool	neg = c == 'n';
	if (neg)
		c = *++m;
	int	v	= 0;
	while (c >= '0' && c <= '9') {
		v = v * 10 + c - '0';
		c = *++m;
	}
	return neg ? -v : v;
}

Demangler::Record *GCCDemangler::GetUnqualifiedName(const char *&m) {
	Record	*r = 0;
	int	c = *m;
	if (c >= '0' && c <= '9') {
		int		n	= GetNumber10(m);
		char	*p	= (char*)iso::malloc(n + 1);
		memcpy(p, m, n);
		p[n]		= 0;
		r			= new Record(DR_name);
		r->name		= (const char*)p;
		m += n;
		AddSub(r);
	} else if (c == 'T') {
		r = new Record(DR_template_param);
		r->value = GetNumber36(m);
	} else {
		c = (c << 8) | m[1];
		const uint16 *p = find(operators, c);
		if (p != end(operators)) {
			DEMANGLE_OPERATOR	op = DEMANGLE_OPERATOR(p - operators);
			m			+= 2;
			r			= new Record(DR_operator);
			r->value	= op;
			switch (op) {
				case OP_sr:
					GetType(m);
					break;
			}
		} else if (c == 'Ut') {
		} else {
			r = 0;
		}
	}
	return r;
}

Demangler::Record *GCCDemangler::GetTemplateArgs(const char *&m) {
	Record *r = 0;
	while (m[0] != 'E') {
		Record *r2 = 0;
		if (m[0] == 'L') {	//literal
			m++;
			r2			= new Record(DR_literal, GetType(m));
			r2->value	= GetNumber10(m);
			m++;	// final 'E'
		} else if (m[0] == 'X') {	//expression
			m++;
			while (m[0] != 'E') {
				Record *r2 = GetUnqualifiedName(m);
			}
			m++;
		} else if (m[0] == 'J') {	// argument pack (?)
		} else {
			r2			= GetType(m);
		}
		if (r) {
			r = new Record(DR_list, r);
			r->child2 = r2;
		} else {
			r = r2;
		}
	}
	m++;
	return r;
}

Demangler::Record *GCCDemangler::GetNestedName(const char *&m) {
	Record	*r = 0;
	while (m[0] != 'E') {
		Record *r2 = 0;
		switch (m[0]) {
			case 'T':
				r2 = new Record(DR_template_param);
				r2->value = GetNumber36(m);
				break;
			case 'I':
				m++;
				if (r->type == DR_nested) {
					r->child2 = new Record(DR_template_arg, r->child2);
					r->child2->child2 = GetTemplateArgs(m);
				} else {
					r = new Record(DR_template_arg, r);
					r->child2 = GetTemplateArgs(m);
				}
				break;
			case 'S':
				if (m[1] == 't') {
					m += 2;
					r2 = GetUnqualifiedName(m);
				} else {
					m++;
					int	n = GetNumber36(m);
					r2 = substitutions[n];
				}
				break;
			default:
				if (r2 = GetUnqualifiedName(m))
					break;
				delete r;
				return 0;
		}
		if (r2) {
			if (r) {
				r = new Record(DR_nested, r);
				r->child2 = r2;
			} else {
				r = r2;
			}
		}
	}
	m++;
	return r;
}

Demangler::Record *GCCDemangler::GetType(const char *&m) {
	Record	*r	= 0;
	int		flags	= GetCVQualifiers(m);
	TYPE	type	= TYPE(find(types, *m++) - types);

	if (type == TYPE_extended)
		type = TYPE(find(extended_types, *m++) - types + _TYPE_extended);

	switch (type) {
		case TYPE_error: case TYPE_extended_error:
		case TYPE_vendor:
			break;

		case TYPE_vendor_qual:
			if (*m == 't') {
				m++;
				GetNumber10(m);
				break;
			}
			r = new Record(DR_qualifier, GetUnqualifiedName(m));
			r->child2 = GetType(m);
			break;

		case TYPE_function: {
			if (*m == 'Y') {
				flags |= FLAG_EXTERNC;
				m++;
			}
			r = new Record(DR_function, 0, flags);
			Record	*p	= GetType(m);
			while (*m != 'E') {
				p = new Record(DR_list, p);
				p->child2 = GetType(m);
			}
			r->child = p;
			break;
		}

		case TYPE_array: {
			int	value = GetNumber10(m);
			m++;
			r			= new Record(DR_array, GetType(m), flags);
			r->value	= value;
			break;
		}

		case TYPE_pointer:		r = new Record(DR_pointer,		GetType(m), flags); break;
		case TYPE_reference:	r = new Record(DR_reference,	GetType(m), flags); break;
		case TYPE_rvalue_ref:	r = new Record(DR_rvalue_ref,	GetType(m), flags); break;
		case TYPE_complex:		r = new Record(DR_complex,		GetType(m), flags); break;
		case TYPE_imaginary:	r = new Record(DR_imaginary,	GetType(m), flags); break;

		case TYPE_pointer2member:
			r = new Record(DR_pointer2member, GetType(m), flags);
			r->child2 = GetType(m);
			break;

		case TYPE_templateparam:
			r = new Record(DR_template_param, 0, flags);
			r->value = GetNumber36(m);
			break;

		case TYPE_nested:
			r = GetNestedName(m);
			r->flags = flags;
			break;

		case TYPE_substitution:
			return substitutions[GetNumber36(m)];

		default:
			r = new Record(DR_name, 0, flags | FLAG_BUILTIN);
			r->name = type < _TYPE_extended ? type_names[type] : extended_type_names[type - _TYPE_extended];
			return r;
	}
	AddSub(r);
	return r;
}


Demangler::Record *GCCDemangler::GetName(const char *&m) {
	Record *r = 0;
	switch (m[0]) {
		case 'N': {	// nested
			m++;
			int	flags = GetCVQualifiers(m);
			if (r = GetNestedName(m)) {
				r->flags = flags;
//				substitutions.pop_back();
				AddSub(r);
			}
			break;
		}

		case 'S':
			switch (*++m) {
				case 't': // ::std::
					r = GetUnqualifiedName(m);
					break;
				case 'a': // ::std::allocator
				case 'b': // ::std::basic_string
				case 's': // ::std::basic_string < char, ::std::char_traits<char>, ::std::allocator<char> >
				case 'i': // ::std::basic_istream<char,  std::char_traits<char> >
				case 'o': // ::std::basic_ostream<char,  std::char_traits<char> >
				case 'd': // ::std::basic_iostream<char, std::char_traits<char> >
					break;
			}
			break;

		case 'I':	// template args
			m++;
			r = GetTemplateArgs(m);
			break;

		case 'Z': {	// local scope
			m++;
			r = new Record(DR_local, GetEncoding(m));
			m++;	//skip 'E'
			r->child2	= GetName(m);
			int	n		= 0;
			if (m[0] == '_') {
				if (m[1] == '_') {
					m += 2;
					n = GetNumber10(m) + 1;
				} else {
					n = m[1] - '0' + 1;
					m += 2;
				}
			}
			break;
		}

		default:
			r = GetUnqualifiedName(m);
			break;
	}
	return r;
}

Demangler::Record *GCCDemangler::GetEncoding(const char *&m) {
	Record	*r = 0;
	if (m[0] == 'T') {
		switch (*++m) {
			case 'V': r = new Record(DR_virtual_table,	GetType(m)); break;
			case 'T': r = new Record(DR_vtt_struc,		GetType(m)); break;
			case 'I': r = new Record(DR_typeinfo_struc,	GetType(m)); break;
			case 'S': r = new Record(DR_typeinfo_name,	GetType(m)); break;
			case 'c': //<call-offset> <call-offset> <base encoding>
			case 'h': {	//<nv-offset> _
				int	v = GetNumber10(m);
				++m;
				r = new Record(DR_thunk, GetEncoding(m), 0);
				r->value = v;
				break;
			}
			case 'v': {//<v-offset> _
				int	v = GetNumber10(m);
				++m;
				r = new Record(DR_thunk_v, GetEncoding(m), 0);
				r->value = v;
				break;
			}
		};
	} else if (m[0] == 'G' && m[1] == 'V') {	//Guard variable for one-time initialization
		m += 2;
		r = new Record(DR_guard_variable, GetName(m));

	} else {
		if ((r = GetName(m)) && *m && *m != '.') {

			Record	*p	= GetType(m);
			Record	*r2	= r;

			if (r->type == DR_nested && r->child2->type == DR_operator)
				r2 = r->child;

			while (r2->type == DR_nested)
				r2 = r2->child2;

			RemoveSub(r2);	// remove function name, per spec

			if (r2->type == DR_template_arg) {
				r = new Record(DR_rettype, r);
				r->child2 = p;
				p	= GetType(m);
			}

			int		f = r->flags;
			r->flags &= ~FLAG_CV;
			r = new Record(DR_function, r, f & FLAG_CV);

			while (*m && *m != '.' && *m != 'E') {
				p = new Record(DR_list, p);
				p->child2 = GetType(m);
			}
			r->child2 = p;
		}
	}
	return r;
}

//-----------------------------------------------------------------------------
//	Microsoft demangling
//-----------------------------------------------------------------------------

class MSDemangler : public Demangler {
public:
	Record *GetEncoding(const char *&m) {
		return 0;
	}
};


//-----------------------------------------------------------------------------
//	Codewarrior demangling
//-----------------------------------------------------------------------------

class CWDemangler : public Demangler {
	friend	bool cw_qual2(string_accum &sa, demangle_component *dc);
protected:
	enum TYPE {
		TYPE_void,		TYPE_wchar,		TYPE_bool,		TYPE_char,		TYPE_short,
		TYPE_int,		TYPE_long,		TYPE_int64,		TYPE_float,		TYPE_double,
		TYPE_vector,
		TYPE_ellipsis,	TYPE_special,	TYPE_qualified,
		TYPE_unsigned,	TYPE_const,
		TYPE_function,	TYPE_array,		TYPE_pointer,	TYPE_reference,	TYPE_constref,
		TYPE_null,		TYPE_error,
	};

	static const char	types[];
	static const char*	type_names[];
	static const char*	operators[];

	static int		GetCVQualifiers(const char *&m);
	static Record*	MakeName(const char *s, int n);
	static int		GetNumber(const char *&m);

	Record *GetType(const char *&m, const char *end);
	Record *GetName(const char *&m, const char *end);
	Record *GetQualifier(const char *&m, const char *end);
	Record *GetQualifiers(const char *&m, const char *end);
	Record *GetEncoding(const char *&m, const char *end);
	CWDemangler() {}
public:
	CWDemangler(const char *m) {
		root = GetEncoding(m, m + strlen(m));
	}
	static bool	IsMangled(const char *m) {
		char	c;
		if (*m == '.')
			return false;
		for (int i = 0, underscores = 0; c = m[i]; i++) {
			if (c == '<' || (i > 2 && underscores >= 2 && (c == 'Q' || c == 'F' || (c >= '0' && c <= '9'))))
				return true;
			if (c == '_')
				underscores++;
			else
				underscores = 0;
		}
		return false;
	}
};

const char CWDemangler::types[] = "vwbcsilxfd2e_QUCFAPR%";
const char* CWDemangler::type_names[] = {
	"void",		"wchar_t",	"bool",		"char",		"short",
	"int",		"long",		"__int64",	"float",	"double",
	"vector",
	"...",
};

const char *CWDemangler::operators[] = {
	"nw",	// OP_nw,	new
	"dl",	// OP_na,	new[]
	"nwa",	// OP_dl,	delete
	"dla",	// OP_da,	delete[]
	0,		// OP_ps,	+ (unary)
	0,		// OP_ng,	- (unary)
	0,		// OP_ad,	& (unary)
	0,		// OP_de,	* (unary)
	"co",	// OP_co,	~
	"pl",	// OP_pl,	+
	"mi",	// OP_mi,	-
	"ml",	// OP_ml,	*
	0,		// OP_dv,	/
	0,		// OP_rm,	%
	0,		// OP_an,	&
	0,		// OP_or,	|
	0,		// OP_eo,	^
	"as",	// OP_aS,	=
	"apl",	// OP_pL,	+=
	"ami",	// OP_mI,	-=
	0,		// OP_mL,	*=
	0,		// OP_dV,	/=
	0,		// OP_rM,	%=
	0,		// OP_aN,	&=
	0,		// OP_oR,	|=
	0,		// OP_eO,	^=
	"ls",	// OP_ls,	<<
	"rs",	// OP_rs,	>>
	0,		// OP_lS,	<<=
	0,		// OP_rS,	>>=
	"eq",	// OP_eq,	==
	0,		// OP_ne,	!=
	"lt",	// OP_lt,	<
	"gt",	// OP_gt,	>	(unconfirmed)
	0,		// OP_le,	<=
	0,		// OP_ge,	>=
	0,		// OP_nt,	!
	0,		// OP_aa,	&&
	0,		// OP_oo,	||
	"pp",	// OP_pp,	++ (postfix in <expression> context)
	"mm",	// OP_mm,	-- (postfix in <expression> context) (unconfirmed)
	0,		// OP_cm,	,
	0,		// OP_pm,	->*
	"rf",	// OP_pt,	->
	"cl",	// OP_cl,	()
	"vc",	// OP_ix,	[]
	0,		// OP_qu,	?
	0,		// OP_st,	sizeof (a type)
	0,		// OP_sz,	sizeof (an expression)
	0,		// OP_at,	alignof (a type)
	0,		// OP_az,	alignof (an expression)
	"op",	// OP_cv,	<type>		// (cast)
//	0,		// OP_v0	<digit> <source-name>		// vendor extended operator

	0,		// OP_Ut,	unnamed type

	"ct",	// OP_C1,	complete object constructor
	0,		// OP_C2,	base object constructor
	0,		// OP_C3,	complete object allocating constructor
	0,		// OP_D0,	deleting destructor
	"dt",	// OP_D1,	complete object destructor
	0,		// OP_D2,	base object destructor
};


int CWDemangler::GetNumber(const char *&m) {
	int		v	= 0;
	for (char c; (c = *m) >= '0' && c <= '9'; m++)
		v = v * 10 + c - '0';
	return v;
}

Demangler::Record *CWDemangler::MakeName(const char *s, int n) {
	Record	*r = new Record(DR_name);
	char	*name	= (char*)iso::malloc(n + 1);
	memcpy(name, s, n);
	name[n] = 0;
	r->name = (const char*)name;
	return r;
}

int CWDemangler::GetCVQualifiers(const char *&m) {
	int	flags = 0;
	if (*m == 'C') {
		flags |= FLAG_CONST;
		m++;
	}
	if (*m == 'V') {
		flags |= FLAG_VOLATILE;
		m++;
	}
	if (*m == 'S') {
		flags |= FLAG_STATIC;
		m++;
	}
	return flags;
}

Demangler::Record *CWDemangler::GetType(const char *&m, const char *end) {
	Record	*r = 0;
	int		flags	= GetCVQualifiers(m);
/*
	if (*m >= '0' && *m <= '9') {
		int	n = GetNumber(m);
		return GetName(m, m + n);
	}
*/

	TYPE	type	= TYPE(find(types, *m) - types);

	switch (type) {
		case TYPE_error:
			break;

		case TYPE_special:
			m++;
		case TYPE_qualified:
			r = GetQualifiers(m, end);
			break;

		case TYPE_function: {
			m++;
			r = new Record(DR_function, 0, flags);
			Record	*p = GetType(m, end), *p2;
			while (m < end && (p2 = GetType(m, end))) {
				p = new Record(DR_list, p);
				p->child2 = p2;
			}
			r->child2 = p;
			break;
		}

		case TYPE_array: {
			m++;
			int	n = GetNumber(m);
			m++;
			r = new Record(DR_array, GetType(m, end), flags);
			r->value = n;
			break;
		}

		case TYPE_pointer:
			m++;
			r = new Record(DR_pointer, GetType(m, end), flags);
			break;

		case TYPE_reference:
			m++;
			r = new Record(DR_reference, GetType(m, end), flags);
			break;

		case TYPE_unsigned:
			m++;
			r = GetType(m, end);
			if (r && r->type == DR_name) {
				r->name = iso::strdup(format_string("unsigned %s", r->name));
				r->flags &= ~FLAG_BUILTIN;
			}
			break;

		default:
			m++;
			r = new Record(DR_name, 0, FLAG_BUILTIN | flags);
			r->name = type_names[type];
			break;
	}
	return r;
}

Demangler::Record *CWDemangler::GetQualifier(const char *&m, const char *end) {
	Record	*r = 0;
	if (*m >= 0 && *m <= '9') {
		int	n	= GetNumber(m);
		const char *m2 = m;
		m		+= n;
		r		= GetName(m2, m);
	} else {
		r		= GetName(m, end);
	}
	return r;
}

Demangler::Record *CWDemangler::GetQualifiers(const char *&m, const char *end) {
	Record	*r = 0;
	if (m < end) switch (*m) {
		case 'Q': {
			int	nq = m[1] - '0';
			m		+= 2;
			r		= GetQualifier(m, end);
			while (--nq) {
				r = new Record(DR_nested, r);
				r->child2 = GetQualifier(m, end);
			}
			break;
		}
		case 'C': case 'F': case 'P': case 'R': case 'A':
			r = GetType(m, end);
			break;

		case 'T': {
			r = new Record(DR_template_param, r);
			m++;
			r->value = GetNumber(m);
			m++;
			break;
		}

		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
			int	n = GetNumber(m);
			r = GetName(m, m + n);
			break;
		}

		default:
			r = GetName(m, end);
			break;
	}
	return r;
}

Demangler::Record *CWDemangler::GetName(const char *&m, const char *end) {
	Record	*r = 0;

	if (m[0] == '_' && m[1] == '_') {
		prefix *o = find(
			(prefix*)begin(operators),
			(prefix*)iso::end(operators),
			m + 2
		);
		if (o != (prefix*)iso::end(operators)) {
			r	= new Record(DR_operator);
			r->value = int(o - (prefix*)begin(operators));
			m	+= 4;
			if (r->value == OP_cv) {
				r->child = GetQualifiers(m, end);
			}
		} else {
			m += 2;
			r = GetQualifiers(m, end);
		}

	} else {
		const char	*p = m;
		while (p < end && p[0] != '<' && p[0] != '>' && p[0] != ',' && !(p[0] == '_' && p[1] == '_'))
			p++;
		r = MakeName(m, int(p - m));
		m = p;
	}

	if (m < end) {
		if (m[0] == '<') {
			const char	*gt		= m;
			int			under	= 0;
			bool		got2	= false;
			for (int nest = 1; nest;) {
				char c = *++gt;
				if (c == '_') {
					if (++under == 2)
						got2 = true;
				} else {
					under = 0;
				}
				nest += int(c == '<') - int(c == '>');
			}

			r = new Record(DR_template_arg, r);
			m++;
			if ((*m >= '0' && *m <= '9') || !got2)
				r->child2 = GetName(m, gt);
			else
				r->child2 = GetQualifiers(m, gt);
			while (m[0] == ',') {
				m++;
				r->child2			= new Record(DR_list, r->child2);
				if (*m >= '0' && *m <= '9')
					r->child2->child2	= GetName(m, gt);
				else
					r->child2->child2	= GetQualifiers(m, gt);
			}
			m++;

		}
	}

	return r;
}

Demangler::Record *CWDemangler::GetEncoding(const char *&m, const char *end) {
	Record	*r = 0;
	if (*m == '@') {
		char	temp[256], *p = temp, c;
		while ((c = *++m) && c != '@')
			*p++ = c;
		m++;
		*p = 0;
		const char *at = strchr(m, '@');
		if (!at)
			at = end;
		if (str(temp) == "GUARD") {
			r = new Record(DR_nested, GetEncoding(m, at));
			m++;
			r->child2 = GetName(m, end);
			r = new Record(DR_guard_variable, r);
		} else if (str(temp) == "LOCAL") {
			r = new Record(DR_local, GetEncoding(m, at));
			m++;
			r->child2 = GetName(m, end);
		} else if (str(temp) == "STRING") {
			r = new Record(DR_string, GetEncoding(m, at));
			r->value = 0;
			if (m[0] == '@') {
				m++;
				r->value = GetNumber(m) + 1;
			}
		}
	} else {
		r = GetName(m, end);
		while (Record *t = GetQualifiers(m, end)) {
			switch (t->type) {
				case DR_name:
				case DR_nested:
					t = new Record(DR_nested, t);
					t->child2 = r;
					r = t;
					break;
				case DR_function:
					t->child = r;
					r = t;
					break;
			}
		}
	}
	return r;
}

//-----------------------------------------------------------------------------
//	CW enmangle
//-----------------------------------------------------------------------------

class CWEnmangler : public CWDemangler {
	friend void cw_enmangle(string_accum &sa, demangle_component *dc, int q);
	static const char *shortcuts[][2];
	bool	enmangle(string_accum &sa, Record *r, int q);
public:
	CWEnmangler(Record *r) { root = r; }
	bool	Enmangle(string_accum &sa) { return enmangle(sa, root, 0); }
};

const char *CWEnmangler::shortcuts[][2] = {
	{"void",				"v"	},
	{"wchar_t",				"w"	},
	{"bool",				"b"	},
	{"char",				"c"	},
	{"signed char",			"c"	},
	{"unsigned char",		"Uc"},
	{"short",				"s"	},
	{"unsigned short",		"Us"},
	{"int",					"i"	},
	{"unsigned",			"Ul"},
	{"unsigned int",		"Ui"},
	{"long",				"l"	},
	{"unsigned long",		"Ul"},
	{"__int64",				"x"	},
	{"unsigned __int64",	"Ux"},
	{"float",				"f"	},
	{"double",				"d"	},
	{"...",					"e"	},
};

bool CWEnmangler::enmangle(string_accum &sa, Record *r, int q) {

	if (r) switch (r->type) {
		case DR_name:
			for (int i = 0; i < num_elements(shortcuts); i++) {
				if (str(r->name) == shortcuts[i][0]) {
					sa << shortcuts[i][1];
					return true;
				}
			}
			sa << r->name;
			break;

		case DR_literal:
			sa << r->value;
			break;

		case DR_function: {
			enmangle(sa, r->child, q | 4);	// 4-> fn name
			if (r->flags & FLAG_CONST)
				sa << 'C';
			if (r->flags & FLAG_STATIC)
				sa << 'S';
			sa << 'F';
			enmangle(sa, r->child2, q | 3);
			break;
		}

		case DR_rettype:
			enmangle(sa, r->child, q);
			break;

		case DR_array:
			sa << 'A' << r->value << '_';
			enmangle(sa, r->child, q);
			break;

		case DR_pointer:
			sa << 'P';
			enmangle(sa, r->child, q);
			break;

		case DR_reference:
			sa << 'R';
			enmangle(sa, r->child, q);
			break;

		case DR_list:
			enmangle(sa, r->child, q);
			if (!(q & 2))
				sa << ',';
			enmangle(sa, r->child2, q);
			break;

		case DR_nested: {
			if (r->flags & FLAG_CONST)
				sa << 'C';
			if (r->flags & FLAG_STATIC)
				sa << 'S';

			string	s[16];
			int		n = 0;
			fixed_string<1024>	s0;
			Record	*r2;
			for (r2 = r; r2->type == DR_nested; r2 = r2->child) {
				enmangle(lvalue(fixed_accum(s0)), r2->child2, q | 1);
				if (s0.length())
					s[n++] = s0;
			}
			enmangle(lvalue(fixed_accum(s0)), r2, q | 1);
			if (s0.length())
				s[n++] = s0;

			if (!(q & 1)) {
				sa << s[0];
				sa << "__";
				if (n > 2)
					sa << "Q" << (n - 1);
				while (n-- > 1) {
					sa << s[n].length();
					sa << s[n];
				}
			} else {
				if (n > 1)
					sa << "Q" << n;
				while (n--) {
					sa << s[n].length();
					sa << s[n];
				}
			}
			break;
		}

		case DR_template_arg:
			enmangle(sa, r->child, q);
			sa << '<';
			enmangle(sa, r->child2, true);
			sa << '>';
			break;

		case DR_operator:
			if (r->value < num_elements(operators)) {
				sa << "__" << operators[r->value];
//			} else switch (r->value) {
			}
			break;
	}
	return true;
}

const char *cw_operators[] = {
	"nw",	// OP_nw,	new
	"nwa",	// OP_na,	new[]
	"dl",	// OP_dl,	delete
	"dla",	// OP_da,	delete[]
	"pl",	// OP_ps,	+ (unary)
	"mi",	// OP_ng,	- (unary)
	0,		// OP_ad,	& (unary)
	"ml",	// OP_de,	* (unary)
	"co",	// OP_co,	~
	"pl",	// OP_pl,	+
	"mi",	// OP_mi,	-
	"ml",	// OP_ml,	*
	0,		// OP_dv,	/
	0,		// OP_rm,	%
	0,		// OP_an,	&
	0,		// OP_or,	|
	0,		// OP_eo,	^
	"as",	// OP_aS,	=
	"apl",	// OP_pL,	+=
	"ami",	// OP_mI,	-=
	0,		// OP_mL,	*=
	0,		// OP_dV,	/=
	0,		// OP_rM,	%=
	0,		// OP_aN,	&=
	0,		// OP_oR,	|=
	0,		// OP_eO,	^=
	"ls",	// OP_ls,	<<
	"rs",	// OP_rs,	>>
	0,		// OP_lS,	<<=
	0,		// OP_rS,	>>=
	"eq",	// OP_eq,	==
	0,		// OP_ne,	!=
	"lt",	// OP_lt,	<
	"gt",	// OP_gt,	>	(unconfirmed)
	0,		// OP_le,	<=
	0,		// OP_ge,	>=
	0,		// OP_nt,	!
	0,		// OP_aa,	&&
	0,		// OP_oo,	||
	"pp",	// OP_pp,	++ (postfix in <expression> context)
	"mm",	// OP_mm,	-- (postfix in <expression> context) (unconfirmed)
	0,		// OP_cm,	,
	0,		// OP_pm,	->*
	"rf",	// OP_pt,	->
	"cl",	// OP_cl,	()
	"vc",	// OP_ix,	[]
	0,		// OP_qu,	?
	0,		// OP_st,	sizeof (a type)
	0,		// OP_sz,	sizeof (an expression)
	0,		// OP_at,	alignof (a type)
	0,		// OP_az,	alignof (an expression)
	"op",	// OP_cv,	<type>		// (cast)
//	0,		// OP_v0	<digit> <source-name>		// vendor extended operator

	0,		// OP_Ut,	unnamed type

	"ct",	// OP_C1,	complete object constructor
	0,		// OP_C2,	base object constructor
	0,		// OP_C3,	complete object allocating constructor
	0,		// OP_D0,	deleting destructor
	"dt",	// OP_D1,	complete object destructor
	0,		// OP_D2,	base object destructor
};
void cw_qual(string_accum &sa, demangle_component *dc);
bool cw_qual2(string_accum &sa, demangle_component *dc);

void cw_enmangle(string_accum &sa, demangle_component *dc, int q) {
	if (dc) switch (dc->type) {
		case demangle_component::LITERAL_NEG:
			sa << '-';
		case demangle_component::LITERAL:
			cw_enmangle(sa, dc->right, q);
			break;

		case demangle_component::NAME:
			if (dc->string == "__va_list_tag") {
				sa << "16__va_list_struct";
				break;
			}
			if (q == 0)
				sa << dc->string.len;
			sa << dc->string;
			break;

		case demangle_component::QUAL_NAME: {
			int					nq	= 1;
			demangle_component	*q	= dc;
			while (q->type == demangle_component::QUAL_NAME) {
				nq++;
				q = q->left;
			}
			if (q->type == demangle_component::TEMPLATE) {
				q = q->left;
				while (q->type == demangle_component::QUAL_NAME) {
					nq++;
					q = q->left;
				}
			}
			if (nq > 1)
				sa << "Q" << nq;
			cw_qual(sa, dc);
			break;
		}

		case demangle_component::FUNCTION_TYPE: {
			sa << 'F';
			cw_enmangle(sa, dc->right, 0);
			if (dc->left) {
				sa << '_';
				cw_enmangle(sa, dc->left, 0);
			}
			break;
		}

		case demangle_component::ARGLIST:
			if (dc->left)
				cw_enmangle(sa, dc->left, q);
			else
				sa << 'v';
			cw_enmangle(sa, dc->right, q);
			break;

		case demangle_component::TEMPLATE_PARAM:
			sa << 'T' << dc->number;
			break;

		case demangle_component::TEMPLATE: {
			int	nq	= 1;
			for (demangle_component *p = dc->left; p->type == demangle_component::QUAL_NAME; p = p->left)
				nq++;
			if (nq > 1)
				sa << "Q" << nq;
			cw_qual(sa, dc);
			break;
			cw_enmangle(sa, dc->left, q);
//			cw_qual2(sa, dc->left);
			sa << '<';
			cw_enmangle(sa, dc->right, q);
			sa << '>';
			break;
		}

		case demangle_component::TEMPLATE_ARGLIST:
			cw_enmangle(sa, dc->left, q);
			if (dc->right) {
				sa << ',';
				cw_enmangle(sa, dc->right, q);
			}
			break;

		case demangle_component::ARRAY_TYPE:
			sa << 'A';
			cw_enmangle(sa, dc->left, 1);
			sa << '_';
			cw_enmangle(sa, dc->right, 0);
			break;

		case demangle_component::REFERENCE:
			sa << 'R';
			cw_enmangle(sa, dc->left, q);
			break;
		case demangle_component::POINTER:
			sa << 'P';
			cw_enmangle(sa, dc->left, q);
			break;
		case demangle_component::RESTRICT:
			sa << '?';
			cw_enmangle(sa, dc->left, q);
			break;
		case demangle_component::VOLATILE:
			sa << '?';
			cw_enmangle(sa, dc->left, q);
			break;
		case demangle_component::KONST:
			sa << 'C';
			cw_enmangle(sa, dc->left, q);
			break;

		case demangle_component::BUILTIN_TYPE: {
			auto &name = dc->builtin->name;
			for (int i = 0; i < num_elements(CWEnmangler::shortcuts); i++) {
				if (name == CWEnmangler::shortcuts[i][0]) {
					sa << CWEnmangler::shortcuts[i][1];
					return;
				}
			}
			sa << name;
			break;
		}
		case demangle_component::VECTOR_TYPE:
			sa << '2';
			break;

		case demangle_component::VENDOR_TYPE_QUAL: {
			demangle_component	*r = dc->right;
			if (r && r->type == demangle_component::NAME && r->string == "__vector") {
				cw_enmangle(sa, dc->left, q);
				//sa << '2';
				break;
			}
			//sa << 'U';
			cw_enmangle(sa, dc->right, q);
			cw_enmangle(sa, dc->left, q);
			break;
		}

		default: {
			int	unhandled = 1;
		}
	}
}

void cw_qual(string_accum &sa, demangle_component *dc) {

	fixed_string<256>	t;
	switch (dc->type) {
		case demangle_component::QUAL_NAME:
			cw_qual(sa, dc->left);
			cw_qual(sa, dc->right);
			break;
		case demangle_component::TEMPLATE:
			{
				demangle_component *left = dc->left;
				fixed_accum	ta(t);
				if (left->type == demangle_component::QUAL_NAME) {
					cw_qual(sa, left->left);
					cw_enmangle(ta, left->right, 1);
				} else {
					cw_enmangle(ta, left, 1);
				}
				ta << '<';
				cw_enmangle(ta, dc->right, 0);
				ta << '>';
			}
			sa << t.length() << t;
			break;

		default: {
			cw_enmangle(lvalue(fixed_accum(t)), dc, 1);
			sa << t.length() << t;
			break;
		}
	}
}

bool cw_qual2(string_accum &sa, demangle_component *dc) {
	demangle_component	*p		= dc;
	demangle_component	*q		= 0;
	demangle_component	*t		= 0;

	for (bool stop = false; !stop; ) switch (p->type) {
		case demangle_component::QUAL_NAME:
			q	= p;
			p	= p->right;
			break;
		case demangle_component::TEMPLATE:
			t	= p;
			p	= p->left;
			break;
		case demangle_component::RESTRICT_THIS:
		case demangle_component::VOLATILE_THIS:
		case demangle_component::CONST_THIS:
			p	= p->left;
			break;
		default:
			stop = true;
			break;
	}

	switch (p->type) {
		case demangle_component::XTOR:
			if (p->s_xtor.kind >= gnu_v3_deleting_dtor)
				sa << "__dt";
			else
				sa << "__ct";
			break;
		case demangle_component::CAST:
			sa << "__op";
			cw_enmangle(sa, p->left, 1);
			break;
		case demangle_component::OPERATOR: {
			sa << "__";
			uint16			c	= (p->op->code[0] << 8) | p->op->code[1];
			const uint16	*i	= find(GCCDemangler::operators, c);
			const char		*o;
			if (i != end(GCCDemangler::operators) && (o = cw_operators[i - GCCDemangler::operators]))
				sa << o;
			else
				sa << p->op->code;
			break;
		}
		case demangle_component::NAME:
			sa << p->string;
			break;
	}

	if (t) {
		sa << '<';
		cw_enmangle(sa, t->right, 1);
		sa << '>';
	}

	if (q) {
		sa << "__";
		demangle_component *left = q->left;
		int	nq = 0;
		while (q->type == demangle_component::QUAL_NAME) {
			nq++;
			q = q->left;
		}
		if (q->type == demangle_component::TEMPLATE) {
			q = q->left;
			while (q->type == demangle_component::QUAL_NAME) {
				nq++;
				q = q->left;
			}
		}
		if (nq > 1)
			sa << "Q" << nq;
		cw_qual(sa, left);
	}

	if (!t && !q)
		sa << "__";

	for (p = dc;;) switch (p->type) {
		default:
			return true;
		case demangle_component::RESTRICT_THIS:
			sa << '?';
			p = p->left;
			break;
		case demangle_component::VOLATILE_THIS:
			sa << '?';
			p = p->left;
			break;
		case demangle_component::CONST_THIS:
			sa << 'C';
			p = p->left;
			break;
	}
}

bool CWEnmangle2(string_accum &sa, demangle_component *dc) {
	switch (dc->type) {
		case demangle_component::TYPED_NAME:
			if (!cw_qual2(sa, dc->left))
				return false;
			cw_enmangle(sa, dc->right, 1);
			return true;
		case demangle_component::LOCAL_NAME:
			sa << "@LOCAL@";
			if (!CWEnmangle2(sa, dc->left))
				return false;
			sa << '@';
			cw_enmangle(sa, dc->right, 1);
			return true;
		case demangle_component::GUARD:
			sa << "@GUARD@";
			while (dc->left->type == demangle_component::LOCAL_NAME)
				dc = dc->left;
			if (!CWEnmangle2(sa, dc->left))
				return false;
			sa << '@';
			cw_enmangle(sa, dc->right, 1);
			return true;
		default:
			return cw_qual2(sa, dc);
	}
}

//-----------------------------------------------------------------------------
//	demangle stub
//-----------------------------------------------------------------------------

string demangle(const char *m) {

#if 1
	DemanglerGNU		di(m, DMGL_PARAMS, strlen(m));
	if (demangle_component *dc = di.demangle()) {
		demangle_component	*copy = deep_copy(dc);
		return demangle_print(DMGL_PARAMS, copy);
	}

#endif
#if 0
	if (GCCDemangler::IsMangled(m)) {
		GCCDemangler	demangler(m);
		if (Demangler::Record *r = demangler) {
			CodeDemangler(r).Enmangle(fixed_accum(s));
			return s;
		}
	}
#endif
#if 0
	if (CWDemangler::IsMangled(m)) {
		CWDemangler	demangler(m);
		if (Demangler::Record *r = demangler) {
			CodeDemangler(r).Enmangle(fixed_accum(s).me());
			return s;
		}
	}
#endif

#ifdef PLAT_PC
	if (const char *q = str(m).find('?')) {
		fixed_string<1024>	s;
		if (UnDecorateSymbolName(q, s, sizeof(s), 0))
			return s;
	}
#endif

	return m;
}

string enmangle(const char *m) {
	trace_accum("\ninput:\t\t%s\n", m);
	DemanglerGNU		di(m, DMGL_PARAMS, strlen(m));

	if (demangle_component *dc = di.demangle()) {
		string d = demangle_print(DMGL_PARAMS, dc);
		trace_accum("demangled:\t") << d << '\n';
		string_builder	sb;
		if (CWEnmangle2(sb, dc))
			trace_accum("remangled:\t") << sb << '\n';
		return sb;
	}
	return 0;
	/*
	if (GCCDemangler::IsMangled(m)) {
		GCCDemangler	demangler(m);
		fixed_string<1024>	c;
		CodeDemangler(demangler).Enmangle(fixed_accum(c).me());
		if (!CWEnmangler(demangler).Enmangle(fixed_accum(s).me()))
			s.clear();
//		ISO_ASSERT(!s.blank());
	}
	return s;
	*/
}

