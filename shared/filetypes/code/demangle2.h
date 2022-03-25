#include "base/strings.h"
#include "base/array.h"

struct demangle_string {
	const char *str; int len;
	friend iso::count_string to_string(const demangle_string &s)	{ return iso::count_string(s.str, s.len); }
};
struct demangle_string2 : demangle_string {
	demangle_string2(const demangle_string &s)				{ str = s.str; len = s.len; }
	demangle_string2(const char *_str, int _len)			{ str = _str; len = _len; }
	demangle_string2(const char *_str)						{ str = _str; len = int(strlen(_str)); }
	template<int N> demangle_string2(const char (&_str)[N])	{ str = _str; len = N - 1; }
};
inline bool operator==(const demangle_string &a, const demangle_string2 &b) {
	return a.len == b.len && memcmp(a.str, b.str, a.len) == 0;
}

enum {
	DMGL_NO_OPTS		= 0,		// For readability...
	DMGL_PARAMS	 		= 1 << 0,	// Include function args
	DMGL_ANSI	 		= 1 << 1,	// Include const, volatile, etc
	DMGL_VERBOSE		= 1 << 3,	// Include implementation details.
	DMGL_TYPES			= 1 << 4,	// Also try to demangle type encodings.
	DMGL_RET_POSTFIX	= 1 << 5,	// Print function return types (when present) after function signature
	DMGL_AUTO	 		= 1 << 8,
	DMGL_GNU	 		= 1 << 9,
	DMGL_LUCID	 		= 1 << 10,
	DMGL_ARM	 		= 1 << 11,
	DMGL_HP 	 		= 1 << 12,	// For the HP aCC compiler; same as ARM except for template arguments, etc.
	DMGL_EDG	 		= 1 << 13,
	DMGL_GNU_V3	 		= 1 << 14,
	DMGL_GNAT	 		= 1 << 15,
	// If none of these are set, use 'current_demangling_style' as the default.
	DMGL_STYLE_MASK		= DMGL_AUTO|DMGL_GNU|DMGL_LUCID|DMGL_ARM|DMGL_HP|DMGL_EDG|DMGL_GNU_V3|DMGL_GNAT,
};

// Callback typedef for allocation-less demangler interfaces.
typedef void(*demangle_callback)(const char *, size_t, void *);

// Types which are only used internally.
enum builtin_type_print {
	D_PRINT_DEFAULT,
	D_PRINT_INT,
	D_PRINT_UNSIGNED,
	D_PRINT_LONG,
	D_PRINT_UNSIGNED_LONG,
	D_PRINT_LONG_LONG,
	D_PRINT_UNSIGNED_LONG_LONG,
	D_PRINT_BOOL,
	D_PRINT_FLOAT,
	D_PRINT_VOID
};

struct demangle_builtin_type_info {
	demangle_string		name;
	builtin_type_print print;
};

struct demangle_operator_info {
	const char			*code;
	demangle_string		name;
	int					args;
};

enum gnu_v3_xtor_kinds {
	gnu_v3_not_ctor_or_dtor = 0,

	gnu_v3_complete_object_ctor,
	gnu_v3_base_object_ctor,
	gnu_v3_complete_object_allocating_ctor,

	gnu_v3_deleting_dtor,
	gnu_v3_complete_object_dtor,
	gnu_v3_base_object_dtor
};

// A node in the tree representation is an instance of a struct demangle_component.	Note that the field names of the struct are
// not well protected against macros defined by the file including this one.	We can fix this if it ever becomes a problem.
struct demangle_component {
	enum TYPE {
 		NAME,					// A name, with a length and a pointer to a string.
		QUAL_NAME,				// A qualified name. The left subtree is a class or namespace or some such thing, and the right subtree is a name qualified by that class.
		LOCAL_NAME,				// A local name. The left subtree describes a function, and the right subtree is a name which is local to that function.
		TYPED_NAME,				// A typed name. The left subtree is a name, and the right subtree describes that name as a function.
		TEMPLATE,				// A template. The left subtree is a template name, and the right subtree is a template argument list.
		TEMPLATE_PARAM,			// A template parameter. This holds a number, which is the template parameter index.
		FUNCTION_PARAM,			// A function parameter. This holds a number, which is the index.
		XTOR,					// A constructor or destructor. This holds a name and the kind of constructor or destructor.
		VTABLE,					// A vtable. This has one subtree, the type for which this is a vtable.
		VTT,					// A VTT structure. This has one subtree, the type for which this is a VTT.
		CONSTRUCTION_VTABLE,	// A construction vtable. The left subtree is the type for which this is a vtable, and the right subtree is the derived type for which this vtable is built.
		TYPEINFO,				// A typeinfo structure. This has one subtree, the type for which this is the tpeinfo structure.
		TYPEINFO_NAME,			// A typeinfo name. This has one subtree, the type for which this is the typeinfo name.
		TYPEINFO_FN,			// A typeinfo function. This has one subtree, the type for which this is the tpyeinfo function.
		THUNK,					// A thunk. This has one subtree, the name for which this is a thunk.
		VIRTUAL_THUNK,			// A virtual thunk. This has one subtree, the name for which this is a virtual thunk.
		COVARIANT_THUNK,		// A covariant thunk. This has one subtree, the name for which this is a covariant thunk.
		GUARD,					// A guard variable. This has one subtree, the name for which this is a guard variable.
		REFTEMP,				// A reference temporary. This has one subtree, the name for which this is a temporary.
		HIDDEN_ALIAS,			// A hidden alias. This has one subtree, the encoding for which it is providing alternative linkage.
		SUB_STD,				// A standard substitution. This holds the name of the substitution.
		RESTRICT,				// The restrict qualifier. The one subtree is the type which is being qualified.
		VOLATILE,				// The volatile qualifier. The one subtree is the type which is being qualified.
		KONST,					// The const qualifier. The one subtree is the type which is being qualified.
		RESTRICT_THIS,			// The restrict qualifier modifying a member function. The one subtree is the type which is being qualified.
		VOLATILE_THIS,			// The volatile qualifier modifying a member function. The one subtree is the type which is being qualified.
		CONST_THIS,				// The const qualifier modifying a member function. The one subtree is the type which is being qualified.
		VENDOR_TYPE_QUAL,		// A vendor qualifier. The left subtree is the type which is being qualified, and the right subtree is the name of the qualifier.
		POINTER,				// A pointer. The one subtree is the type which is being pointed to.
		REFERENCE,				// A reference. The one subtree is the type which is being referenced.
		RVALUE_REFERENCE,		// C++0x: An rvalue reference. The one subtree is the type which is being referenced.
		COMPLEX,				// A complex type. The one subtree is the base type.
		IMAGINARY,				// An imaginary type. The one subtree is the base type.
		BUILTIN_TYPE,			// A builtin type. This holds the builtin type information.
		VENDOR_TYPE,			// A vendor's builtin type. This holds the name of the type.
		FUNCTION_TYPE,			// A function type. The left subtree is the return type. The right subtree is a list of ARGLIST nodes. Either or both may be NULL.
		ARRAY_TYPE,				// An array type. The left subtree is the dimension, which may be NULL, or a string(represented as DEMANGLE_COMPONENT_NAME), or an expression. The right subtree is the element type.
		PTRMEM_TYPE,			// A pointer to member type. The left subtree is the class type, and the right subtree is the member type. CV-qualifiers appear on the latter.
		FIXED_TYPE,				// A fixed-point type.
		VECTOR_TYPE,			// A vector type. The left subtree is the number of elements, the right subtree is the element type.
		ARGLIST,				// An argument list. The left subtree is the current argument, and the right subtree is either NULL or another ARGLIST node.
		TEMPLATE_ARGLIST,		// A template argument list. The left subtree is the current template argument, and the right subtree is either NULL or another TEMPLATE_ARGLIST node.
		OPERATOR,				// An operator. This holds information about a standard operator.
		EXTENDED_OPERATOR,		// An extended operator. This holds the number of arguments, and the name of the extended operator.
		CAST,					// A typecast, represented as a unary operator. The one subtree is the type to which the argument should be cast.
		UNARY,					// A unary expression. The left subtree is the operator, and the right subtree is the single argument.
		BINARY,					// A binary expression. The left subtree is the operator, and the right subtree is a BINARY_ARGS.
		BINARY_ARGS,			// Arguments to a binary expression. The left subtree is the first argument, and the right subtree is the second argument.
		TRINARY,				// A trinary expression. The left subtree is the operator, and the right subtree is a TRINARY_ARG1.
		TRINARY_ARG1,			// Arguments to a trinary expression. The left subtree is the first argument, and the right subtree is a TRINARY_ARG2.
		TRINARY_ARG2,			// More arguments to a trinary expression. The left subtree is the second argument, and the right subtree is the third argument.
		LITERAL,				// A literal. The left subtree is the type, and the right subtree is the value, represented as a DEMANGLE_COMPONENT_NAME.
		LITERAL_NEG,			// A negative literal. Like LITERAL, but the value is negated.
		COMPOUND_NAME,			// A name formed by the concatenation of two parts. The left subtree is the first part and the right subtree the second.
		CHARACTER,				// A name formed by a single character.
		NUMBER,					// A number.
		DECLTYPE,				// A decltype type.
		GLOBAL_CONSTRUCTORS,	// Global constructors keyed to name.
		GLOBAL_DESTRUCTORS,		// Global destructors keyed to name.
		LAMBDA,					// A lambda closure type.
		DEFAULT_ARG,			// A default argument scope.
		UNNAMED_TYPE,			// An unnamed type.
		PACK_EXPANSION			// A pack expansion.
	};
	TYPE type;	// The type of this component.
	union {
		const demangle_builtin_type_info *builtin;	// For DEMANGLE_COMPONENT_BUILTIN_TYPE.
		const demangle_operator_info	*op;		// For DEMANGLE_COMPONENT_OPERATOR.
		int								number;		// Parameter index.
		demangle_string					string;		// For DEMANGLE_COMPONENT_NAME.
		struct {									// For DEMANGLE_COMPONENT_EXTENDED_OPERATOR & DEMANGLE_COMPONENT_DEFAULT_ARG, DEMANGLE_COMPONENT_LAMBDA
			demangle_component	*arg;
			int					num;
		};
		struct {
			demangle_component	*left;
			demangle_component	*right;
		};
		struct {									// For DEMANGLE_COMPONENT_EXTENDED_OPERATOR
			int					args;				// Number of arguments
			demangle_component	*name;
		} s_extended_operator;
		struct {									// For DEMANGLE_COMPONENT_FIXED_TYPE
			demangle_component	*length;			// The length, indicated by a C integer type name
			short				accum;				// _Accum or _Fract?
			short				sat;				// Saturating or not?
		} s_fixed;
		struct {									// For DEMANGLE_COMPONENT_CTOR.
			gnu_v3_xtor_kinds	kind;				// Kind of constructor or destructor
			demangle_component	*name;				// Name.
		} s_xtor;
	};

	demangle_component(TYPE _type)															: type(_type)			{}
	demangle_component(TYPE _type, int i)													: type(_type)			{ number = i; }
	demangle_component(TYPE _type, const demangle_string2 &s)								: type(_type)			{ string = s; }
	demangle_component(TYPE _type, demangle_component *_left, demangle_component *_right)	: type(_type)			{ left = _left; right = _right; }
	demangle_component(TYPE _type, demangle_component *_arg, int _num)						: type(_type)			{ arg = _arg; num = _num; }

	demangle_component(const demangle_builtin_type_info *p)									: type(BUILTIN_TYPE)	{ builtin = p; }
	demangle_component(const demangle_operator_info *p)										: type(OPERATOR)		{ op = p; }
	demangle_component(gnu_v3_xtor_kinds kind, demangle_component *name)					: type(XTOR)			{ s_xtor.kind = kind; s_xtor.name = name; }

	bool	has_return_type()				const;
	bool	is_ctor_dtor_or_conversion()	const;
};

demangle_component *deep_copy(const demangle_component *dc);

//-----------------------------------------------------------------------------
//	DemanglerGNU
//-----------------------------------------------------------------------------
struct DemanglerGNU {
	const char			*n;			// The next character in the string to consider.
//	const char			*s;			// The string we are demangling.
	const char			*end;		// The end of the string we are demangling.
	int					options;	// The options passed to the demangler.
	iso::dynamic_array<demangle_component*>	comps;		// The array of components.
	iso::dynamic_array<demangle_component*>	subs;		// The array of substitutions.
	int					did_subs;	// The number of substitutions which we actually made from the subs array, plus the number of template parameter references we saw.
	demangle_component *last_name;	// The last name we saw, for constructors and destructors.
	int					expansion;	// A running total of the length of large expansions from the mangled name to the demangled name, such as standard substitutions and builtin types.
	bool				type;		// true if demangling a type

	DemanglerGNU(const char *mangled, int options, size_t len);
	~DemanglerGNU();

	demangle_component *demangle();

	demangle_component *demangle_mangled_name(bool top_level);
	demangle_component *demangle_type();


	char				peek_char()			{ return *n; }
	char				peek_next_char()	{ return n[1]; }
	void				advance(size_t i)	{ n += i; }
	bool				check_char(char c)	{ return peek_char() == c ? (n++, true) : false; }
	char				next_char()			{ return peek_char() == '\0' ? '\0' : *n++; }
	const char*			str()				{ return n; }

	demangle_component	*make_comp(demangle_component::TYPE, demangle_component *, demangle_component *);
	demangle_component	*make_name(const demangle_string2 &name);
	demangle_component	*make_demangle_mangled_name(const char *);
	demangle_component	*make_builtin_type(const demangle_builtin_type_info *);
	demangle_component	*make_operator(const demangle_operator_info *);
	demangle_component	*make_extended_operator(int, demangle_component *);
	demangle_component	*make_default_arg(int num, demangle_component *sub);
	demangle_component	*make_xtor(gnu_v3_xtor_kinds, demangle_component *);
	demangle_component	*make_template_param(int);
	demangle_component	*make_function_param(int i);
	demangle_component	*make_sub(const demangle_string2 &name);

	demangle_component	*encoding(bool);
	demangle_component	*name();
	demangle_component	*nested_name();
	demangle_component	*prefix();
	demangle_component	*unqualified_name();
	demangle_component	*source_name();
	int					number();
	int					compact_number();
	demangle_component	*identifier(int);
	demangle_component	*operator_name();
	demangle_component	*make_character(int c);
	demangle_component	*special_name();
	bool				call_offset(int);
	demangle_component	*xtor_name();
	demangle_component	**cv_qualifiers(demangle_component **, bool);
	demangle_component	*function_type();
	demangle_component	*parmlist();
	demangle_component	*bare_function_type(bool);
	demangle_component	*class_enum_type();
	demangle_component	*array_type();
	demangle_component	*vector_type();
	demangle_component	*pointer_to_member_type();
	demangle_component	*template_param();
	demangle_component	*template_args();
	demangle_component	*template_arg();
	demangle_component	*exprlist();
	demangle_component	*expression();
	demangle_component	*expr_primary();
	demangle_component	*local_name();
	bool				discriminator();
	demangle_component	*lambda();
	demangle_component	*unnamed_type();
	bool				add_substitution(demangle_component *);
	demangle_component	*substitution(bool);
};

extern bool					demangle(const char *mangled, int options, demangle_callback callback, void *opaque);
extern iso::string			demangle(const char *mangled, int options);
extern bool					demangle_print(int options, const demangle_component *tree, demangle_callback callback, void *opaque);
extern iso::string			demangle_print(int options, const demangle_component *dc);
gnu_v3_xtor_kinds			is_ctor_or_dtor(const char *mangled);
