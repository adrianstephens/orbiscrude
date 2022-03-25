#include "base/array.h"
#include "base/strings.h"
#include <DbgHelp.h>

using namespace iso;

enum {
	FLAG_NO_LEADING_UNDERSCORES	= 0x0001,		// Removes leading underscores from Microsoft extended keywords.
	FLAG_NO_MS_KEYWORDS			= 0x0002,		// Disables expansion of Microsoft extended keywords.
	FLAG_NO_FUNCTION_RETURNS	= 0x0004,		// Disables expansion of return type for primary declaration.
	FLAG_NO_ALLOCATION_MODEL	= 0x0008,		// Disables expansion of the declaration model.
	FLAG_NO_ALLOCATION_LANGUAGE	= 0x0010,		// Disables expansion of the declaration language specifier.
	FLAG_NO_MS_THISTYPE			= 0x0020,		// NYI: Disable expansion of MS keywords on the 'this' type for primary declaration
	FLAG_NO_CV_THISTYPE			= 0x0040,		// NYI: Disable expansion of CV modifiers on the 'this' type for primary declaration
	FLAG_NO_THISTYPE			= 0x0060,		// Disables all modifiers on the this type.
	FLAG_NO_ACCESS_SPECIFIERS	= 0x0080,		// Disables expansion of access specifiers for members.
	FLAG_NO_THROW_SIGNATURES	= 0x0100,		// Disables expansion of "throw-signatures" for functions and pointers to functions.
	FLAG_NO_MEMBER_TYPE			= 0x0200,		// Disables expansion of static or virtual members.
	FLAG_NO_RETURN_UDT_MODEL	= 0x0400,		// Disables expansion of the Microsoft model for UDT returns.
	FLAG_32_BIT_DECODE			= 0x0800,		// Undecorates 32-bit decorated names.
	FLAG_NAME_ONLY				= 0x1000,		// Gets only the name for primary declaration; returns just [scope::]name. Expands template params.
	FLAG_TYPE_ONLY				= 0x2000,		// Input is just a type encoding; composes an abstract declarator.
	FLAG_HAVE_PARAMETERS		= 0x4000,		// The real template parameters are available.
	FLAG_NO_ECSU				= 0x8000,		// Suppresses enum/class/struct/union.
	FLAG_NO_IDENT_CHAR_CHECK	= 0x10000,		// Suppresses check for valid identifier characters.
	FLAG_NO_PTR64				= 0x20000,		// Does not include ptr64 in output.
	FLAG_NO_ELLIPSIS			= 0x40000,
	FLAG_NO_COMPLEX_TYPE		= 0x80000,
	FLAG_NO_ARGUMENTS			= 0x100000,
	FLAG_COMPLETE				= 0x200000,		// Enables full undecoration.
};
// Type for parsing mangled types
struct datatype_t {
	string	left;
	string	right;
};

// Structure holding a parsed symbol
struct parsed_symbol {
	uint32			flags;			// the FLAG_ flags used for demangling
	const char*		current;		// pointer in input (mangled) string
	string			result;			// demangled string

	dynamic_array<string>	names;		// array of names for back reference
	dynamic_array<string>	types;		// array of type strings for back reference
	dynamic_array<string>	stack;		// stack of components
	uint32		names_start;
	uint32		types_start;

	bool		get_number(int &n);
	string		get_args(bool z_term);
	bool		get_modifier(const char **ret, const char **ptr_modif);
	bool		get_modified_type(datatype_t &ct, char modif, bool in_args);
	char*		get_literal_string();
	string		get_template_name();
	bool		get_class();
	string		get_class_string(int start);
	string		get_class_name();

	bool		handle_method(string name, bool cast_op);
	bool		handle_data(const char *name);

	bool		demangle();
	bool		demangle_datatype(datatype_t &ct, bool in_args);

	parsed_symbol(const char *name, uint32 flags) : current(name), flags(flags), names_start(0), types_start(0) {}
};

bool parsed_symbol::get_number(int &n) {
	bool	sgn = *current == '?';
	current += sgn;

	if (between(*current, '0', '9')) {
		n = (sgn ? -1 : 1) * int(*current++ + 1 - '0');
		return true;
	}

	int	ret = 0;
	while (between(*current, 'A', 'P')) {
		ret *= 16;
		ret += *current++ - 'A';
	}

	n = sgn ? -ret : ret;
	return *current++ == '@';
}

// Parses a list of function/method arguments, creates a string corresponding to the arguments' list

 string parsed_symbol::get_args(bool z_term) {
	string_builder b;

	int	i = 0;
	while (*current) {
		// Decode each data type and append it to the argument list
		if (*current == '@') {
			current++;
			break;
		}
		datatype_t	ct;
		if (!demangle_datatype(ct, true))
			return NULL;

		// 'void' terminates an argument list in a function
		if (z_term && !strcmp(ct.left, "void"))
			break;

		b << onlyif(i++, ',') << ct.left << ct.right;

		if (!strcmp(ct.left, "..."))
			break;
	}

	// Functions are always terminated by 'Z'; if we made it this far and don't find it, we have incorrectly identified a data type
	if (z_term && *current++ != 'Z')
		return NULL;

	if (i == 0)
		b << "void";
	
	return b;
}

//*****************************************************************
//		get_modifier
// Parses the type modifier. Always returns static strings

bool parsed_symbol::get_modifier(const char **ret, const char **ptr_modif) {
	*ptr_modif = NULL;
	if (*current == 'E') {
		if (!(flags & FLAG_NO_MS_KEYWORDS)) {
			*ptr_modif = "__ptr64";
			if (flags & FLAG_NO_LEADING_UNDERSCORES)
				*ptr_modif += 2;
		}
		current++;
	}
	switch (*current++) {
		case 'A': *ret = NULL; break;
		case 'B': *ret = "const"; break;
		case 'C': *ret = "volatile"; break;
		case 'D': *ret = "const volatile"; break;
		default: return false;
	}
	return true;
}

bool parsed_symbol::get_modified_type(datatype_t &ct, char modif, bool in_args) {
	const char *ptr_modif = "";

	if (*current == 'E') {
		if (!(flags & FLAG_NO_MS_KEYWORDS)) {
			if (flags & FLAG_NO_LEADING_UNDERSCORES)
				ptr_modif = " ptr64";
			else
				ptr_modif = " __ptr64";
		}
		current++;
	}

	string	str_modif;
	switch (modif) {
		case 'A': str_modif = format_string(" &%s", ptr_modif); break;
		case 'B': str_modif = format_string(" &%s volatile", ptr_modif); break;
		case 'P': str_modif = format_string(" *%s", ptr_modif); break;
		case 'Q': str_modif = format_string(" *%s const", ptr_modif); break;
		case 'R': str_modif = format_string(" *%s volatile", ptr_modif); break;
		case 'S': str_modif = format_string(" *%s const volatile", ptr_modif); break;
		case '?': str_modif = ""; break;
		default: return false;
	}

	const char* modifier;
	if (get_modifier(&modifier, &ptr_modif)) {
		// multidimensional arrays
		if (*current == 'Y') {
			current++;
			
			int		num;
			if (!get_number(num))
				return false;

			if (str_modif[0] == ' ' && !modifier)
				str_modif = str_modif.substr(1);

			if (modifier) {
				str_modif = format_string(" (%s%s)", modifier, str_modif);
				modifier = NULL;
			} else {
				str_modif = format_string(" (%s)", str_modif);
			}

			while (num--) {
				int	d;
				get_number(d);
				str_modif << '[' << d << ']';
			}
		}

		// Recurse to get the referred-to type
		datatype_t	sub_ct;
		if (!demangle_datatype(sub_ct, false))
			return false;

		if (modifier) {
			ct.left = sub_ct.left << ' ' << modifier << str_modif;

		} else {
			// don't insert a space between duplicate '*'
			if (!in_args && str_modif && str_modif[1] == '*' && sub_ct.left[strlen(sub_ct.left) - 1] == '*')
				str_modif = str_modif.substr(1);

			ct.left = sub_ct.left + str_modif;
		}
		ct.right = sub_ct.right;
	}
	return true;
}

//*****************************************************************
//             get_literal_string
// Gets the literal name from the current position in the mangled symbol to the first '@' character.
// It pushes the parsed name to the symbol names stack and returns a pointer to it or NULL in case of an error

char* parsed_symbol::get_literal_string() {
	const char *ptr = current;

	do {
		if (!(is_alphanum(*current) || *current == '_' || *current == '$'))
			return NULL;

	} while (*++current != '@');

	return names.emplace_back(ptr, current++ - ptr);
}

//******************************************************************
 //		get_template_name
 // Parses a name with a template argument list and returns it as a string
 // In a template argument list the back reference to the names table is separately created; '0' points to the class component name with the template arguments
 // We use the same stack array to hold the names but save/restore the stack state before/after parsing the template argument list

string parsed_symbol::get_template_name() {
	uint32	names_mark	= names.size32();
	uint32	types_mark	= types.size32();
	auto	names_saver	= save(names_start, names_mark);
	auto	types_saver	= save(types_start, types_mark);

	string	name = get_literal_string();
	if (!name)
		return false;

	if (string args = get_args(false))
		name = format_string("%s<%s>", name, args);

	names.resize(names_mark);
	types.resize(types_mark);
	return name;
}

//******************************************************************
//		get_class
// Parses class as a list of parent-classes, terminated by '@' and stores the result in 'a' array.
// Each parent-class, as well as the inner element (either field/method name or class name), are represented in the mangled name by a literal name ([a-zA-Z0-9_]+ terminated by '@') or a back reference ([0-9]) or a name with template arguments ('?$' literal name followed by the template argument list).
// The class name components appear in the reverse order in the mangled name, e.g aaa@bbb@ccc@@ will be demangled to ccc::bbb::aaa
// For each of these class name components a string will be allocated in the array

bool parsed_symbol::get_class() {
	string name;

	while (*current != '@') {
		switch (*current) {
			case '\0':
				return false;

			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				name = names[names_start + *current++ - '0'];
				break;

			case '?':
				switch (*++current) {
					case '$':
						current++;
						if (name = get_template_name())
							names.push_back(name);
						break;
					case '?': {
						uint32	start	= names_start;
						uint32	num		= names.size32();

						if (demangle())
							name = format_string("`%s'", result);

						names_start = start;
						names.resize(num);
						break;
					}
					default: {
						int	num;
						if (!get_number(num))
							return false;
						name = format_string("`%i'", num);
						break;
					}
				}
				break;
			default:
				name = get_literal_string();
				break;
		}
		if (!name)
			return false;

		stack.push_back(name);
	}
	current++;
	return true;
}

//*****************************************************************
//		get_class_string
// From an array collected by get_class in stack, constructs the corresponding (allocated) string

string parsed_symbol::get_class_string(int start) {
	uint32	len = 0;
	for (int i = start; i < stack.size(); i++)
		len += 2 + strlen(stack[i]);

	string	ret(len - 2);
	char	*d = ret;

	for (int i = stack.size() - 1; i >= start; i--) {
		auto	sz = strlen(stack[i]);
		memcpy(d, stack[i], sz);
		d += sz;
		if (i > start) {
			*d++ = ':';
			*d++ = ':';
		}
	}

	return ret;
}

//*****************************************************************
//            get_class_name
// Wrapper around get_class and get_class_string.

string parsed_symbol::get_class_name() {
	uint32	mark = stack.size32();
	string	s;
	if (get_class())
		s = get_class_string(mark);
	stack.resize(mark);
	return s;
}

//*****************************************************************
//		get_calling_convention
// Returns a static string corresponding to the calling convention described by char 'ch'. Sets export to true iff the calling convention is exported
static bool get_calling_convention(char ch, const char** call_conv, const char** exported, uint32 flags) {
	*call_conv = *exported = "";

	if (!(flags & (FLAG_NO_MS_KEYWORDS | FLAG_NO_ALLOCATION_LANGUAGE))) {
		if (flags & FLAG_NO_LEADING_UNDERSCORES) {
			if (((ch - 'A') % 2) == 1)
				*exported = "dll_export ";
			switch (ch) {
				case 'A': case 'B': *call_conv = "cdecl"; break;
				case 'C': case 'D': *call_conv = "pascal"; break;
				case 'E': case 'F': *call_conv = "thiscall"; break;
				case 'G': case 'H': *call_conv = "stdcall"; break;
				case 'I': case 'J': *call_conv = "fastcall"; break;
				case 'K': case 'L': break;
				case 'M': *call_conv = "clrcall"; break;
				default: return false;//ERR("Unknown calling convention %c\n", ch); return false;
			}
		} else {
			if (((ch - 'A') % 2) == 1)
				*exported = "__dll_export ";
			switch (ch) {
				case 'A': case 'B': *call_conv = "__cdecl"; break;
				case 'C': case 'D': *call_conv = "__pascal"; break;
				case 'E': case 'F': *call_conv = "__thiscall"; break;
				case 'G': case 'H': *call_conv = "__stdcall"; break;
				case 'I': case 'J': *call_conv = "__fastcall"; break;
				case 'K': case 'L': break;
				case 'M': *call_conv = "__clrcall"; break;
				default: return false;//ERR("Unknown calling convention %c\n", ch); return false;
			}
		}
	}
	return true;
}

//******************************************************************
//         get_simple_type
// Return a string containing an allocated string for a simple data type

static const char* get_simple_type(char c) {
	switch (c) {
		case 'C': return "signed char";
		case 'D': return "char";
		case 'E': return "unsigned char";
		case 'F': return "short";
		case 'G': return "unsigned short";
		case 'H': return "int";
		case 'I': return "unsigned int";
		case 'J': return "long";
		case 'K': return "unsigned long";
		case 'M': return "float";
		case 'N': return "double";
		case 'O': return "long double";
		case 'X': return "void";
		case 'Z': return "...";
		default:  return NULL;
	}
}

//******************************************************************
//         get_extended_type
// Return a string containing an allocated string for a simple data type

static const char* get_extended_type(char c) {
	switch (c) {
		case 'D': return "__int8";
		case 'E': return "unsigned __int8";
		case 'F': return "__int16";
		case 'G': return "unsigned __int16";
		case 'H': return "__int32";
		case 'I': return "unsigned __int32";
		case 'J': return "__int64";
		case 'K': return "unsigned __int64";
		case 'L': return "__int128";
		case 'M': return "unsigned __int128";
		case 'N': return "bool";
		case 'W': return "wchar_t";
		default:  return NULL;
	}
}

//******************************************************************
//         demangle_datatype
// Attempt to demangle a C++ data type

bool parsed_symbol::demangle_datatype(datatype_t &ct, bool in_args) {
	ct.left = ct.right = 0;

	switch (char dt = *current++) {
		case '_':
			/* MS type: __int8,__int16 etc */
			ct.left = get_extended_type(*current++);
			break;
		case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'M':
		case 'N': case 'O': case 'X': case 'Z':
			/* Simple data types */
			ct.left = get_simple_type(dt);
			return true;

		case 'T': /* union */
		case 'U': /* struct */
		case 'V': /* class */
		case 'Y': /* cointerface */
			/* Class/struct/union/cointerface */
		{
			string	struct_name = get_class_name();
			if (!struct_name)
				return false;

			const char* type_name = "";
			if (!(flags & FLAG_NO_COMPLEX_TYPE)) {
				switch (dt) {
					case 'T': type_name = "union ";  break;
					case 'U': type_name = "struct "; break;
					case 'V': type_name = "class ";  break;
					case 'Y': type_name = "cointerface "; break;
				}
			}
			ct.left = format_string("%s%s", type_name, struct_name);
			break;
		}
		case '?':
			// not all the time is seems
			if (in_args) {
				int	num;
				if (!get_number(num))
					return false;
				ct.left = format_string("`template-parameter-%i'", num);
			} else {
				if (!get_modified_type(ct, '?', in_args))
					return false;
			}
			break;
		case 'A': /* reference */
		case 'B': /* volatile reference */
			if (!get_modified_type(ct, dt, in_args))
				return false;
			break;
		case 'Q': /* const pointer */
		case 'R': /* volatile pointer */
		case 'S': /* const volatile pointer */
			if (!get_modified_type(ct, in_args ? dt : 'P', in_args))
				return false;
			break;
		case 'P': /* Pointer */
			if (isdigit(*current)) {
				/* FIXME:
				 *   P6 = Function pointer
				 *   P8 = Member function pointer
				 *   others who knows.. */
				if (*current == '8') {
					current++;

					string		class_name;
					const char*	modifier;
					const char*	ptr_modif;
					if (!(class_name = get_class_name()) || !get_modifier(&modifier, &ptr_modif))
						return false;

					if (modifier)
						modifier = format_string("%s %s", modifier, ptr_modif);
					else if (ptr_modif[0])
						modifier = format_string(" %s", ptr_modif);

					const char	*call_conv, *exported;
					datatype_t	sub_ct;
					string		args;
					if (!get_calling_convention(*current++, &call_conv, &exported, flags & ~FLAG_NO_ALLOCATION_LANGUAGE)
					||	!demangle_datatype(sub_ct, false)
					||	!(args = get_args(true))
					)
						return false;

					ct.left		= format_string("%s%s (%s %s::*", sub_ct.left, sub_ct.right, call_conv, class_name);
					ct.right	= format_string(")(%s)%s", args, modifier);

				} else if (*current == '6') {
					current++;
					const char	*call_conv, *exported;
					datatype_t	sub_ct;
					string		args;
					if (!get_calling_convention(*current++, &call_conv, &exported, flags & ~FLAG_NO_ALLOCATION_LANGUAGE)
					|| !demangle_datatype(sub_ct, false)
					|| !(args = get_args(true))
					)
						return false;

					ct.left		= format_string("%s%s (%s*", sub_ct.left, sub_ct.right, call_conv);
					ct.right	= format_string(")(%s)", args);
				} else {
					return false;
				}

			} else if (!get_modified_type(ct, 'P', in_args)) {
				return false;
			}
			break;

		case 'W':
			if (*current == '4') {
				current++;
				string	enum_name = get_class_name();
				if (!enum_name)
					return false;

				if (flags & FLAG_NO_COMPLEX_TYPE)
					ct.left = enum_name;
				else
					ct.left = format_string("enum %s", enum_name);
			} else {
				return false;
			}
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			// Referring back to previously parsed type
			// left and right are pushed as two separate strings
			ct.left		= types[types_start + (dt - '0') * 2];
			ct.right	= types[types_start + (dt - '0') * 2 + 1];
			return !!ct.left;

		case '$':
			switch (*current++) {
				case '0': {
					int	num;
					if (!get_number(num))
						return false;
					ct.left = to_string(num);
					break;
				}
				case 'D': {
					int	num;
					if (!get_number(num))
						return false;
					ct.left = format_string("`template-parameter%i'", num);
					break;
				}
				case 'F': {
					int		n1, n2;
					if (!get_number(n1) || !get_number(n2))
						return false;
					ct.left = format_string("{%i,%i}", n1, n2);
					break;
				}
				case 'G': {
					int		n1, n2, n3;
					if (!get_number(n1) || !get_number(n2) || !get_number(n3))
						return false;
					ct.left = format_string("{%i,%i,%i}", n1, n2, n3);
					break;
				}
				case 'Q': {
					int	num;
					if (!get_number(num))
						return false;
					ct.left = format_string("`non-type-template-parameter%i'", num);
					break;
				}
				case '$':
					if (*current == 'B') {
						datatype_t	sub_ct;
						string		arr;
						current++;

						// multidimensional arrays
						if (*current == 'Y') {
							current++;
							int num;
							if (!get_number(num))
								return false;

							while (num--) {
								int	num2;
								get_number(num2);
								arr << '[' << num2 << ']';
							}
						}

						if (!demangle_datatype(sub_ct, false))
							return false;

						if (arr)
							ct.left = format_string("%s %s", sub_ct.left, arr);
						else
							ct.left = sub_ct.left;

						ct.right = sub_ct.right;

					} else if (*current == 'C') {
						current++;

						const char *ptr, *ptr_modif;
						if (!get_modifier(&ptr, &ptr_modif) || !demangle_datatype(ct, in_args))
							return false;
						ct.left = format_string("%s %s", ct.left, ptr);
					}
					break;
			}
			break;

		default:
			return false;//ERR("Unknown type %c\n", dt);
	}
	if (in_args) {
		// left and right are pushed as two separate strings
		types.push_back(ct.left);
		types.push_back(ct.right);
	}

	return !!ct.left;
}

//******************************************************************
//		handle_data
// Does the final parsing and handling for a variable or a field in a class

bool parsed_symbol::handle_data(const char *name) {
	const char*	access		= NULL;
	const char*	member_type	= NULL;
	const char*	modifier	= NULL;
	const char*	ptr_modif	= NULL;
	
	// 0 private static
	// 1 protected static
	// 2 public static
	// 3 private non-static
	// 4 protected non-static
	// 5 public non-static
	// 6 ?? static
	// 7 ?? static

	if (!(flags & FLAG_NO_ACCESS_SPECIFIERS)) {
		// we only print the access for static members
		switch (*current) {
			case '0': access = "private: "; break;
			case '1': access = "protected: "; break;
			case '2': access = "public: "; break;
		}
	}

	if (!(flags & FLAG_NO_MEMBER_TYPE)) {
		if (*current >= '0' && *current <= '2')
			member_type = "static ";
	}

	datatype_t	ct;

	switch (*current++) {
		case '0': case '1': case '2': case '3': case '4': case '5': {
			if (!demangle_datatype(ct, false) || !get_modifier(&modifier, &ptr_modif))
				return false;
			break;
		}
		case '6': // compiler generated static
		case '7': // compiler generated static
			if (!get_modifier(&modifier, &ptr_modif))
				return false;

			if (*current != '@') {
				string	class_name = get_class_name();
				if (!class_name)
					return false;
				ct.right = format_string("{for `%s'}", class_name);
			}
			break;

		case '8':
		case '9':
			break;

		default:
			return false;
	}


	if (flags & FLAG_NAME_ONLY) {
		result = name;

	} else {
		string_builder	b;
		b	<< access
			<< member_type
			<< ct.left		<< onlyif(ct.left, ' ')
			<< modifier		<< onlyif(modifier, ' ')
			<< ptr_modif	<< onlyif(ptr_modif, ' ')
			<< name
			<< ct.right;
		result = b;
	}

	return true;
}

//*****************************************************************
//		handle_method
// Does the final parsing and handling for a function or a method in a class

bool parsed_symbol::handle_method(string name, bool cast_op) {
	const char*		access		= NULL;
	const char*		member_type = NULL;

	// FIXME: why 2 possible letters for each option?
	// 'A' private:
	// 'B' private:
	// 'C' private: static
	// 'D' private: static
	// 'E' private: virtual
	// 'F' private: virtual
	// 'G' private: thunk
	// 'H' private: thunk
	// 'I' protected:
	// 'J' protected:
	// 'K' protected: static
	// 'L' protected: static
	// 'M' protected: virtual
	// 'N' protected: virtual
	// 'O' protected: thunk
	// 'P' protected: thunk
	// 'Q' public:
	// 'R' public:
	// 'S' public: static
	// 'T' public: static
	// 'U' public: virtual
	// 'V' public: virtual
	// 'W' public: thunk
	// 'X' public: thunk
	// 'Y'
	// 'Z'
	// "$0" private: thunk vtordisp
	// "$1" private: thunk vtordisp
	// "$2" protected: thunk vtordisp
	// "$3" protected: thunk vtordisp
	// "$4" public: thunk vtordisp
	// "$5" public: thunk vtordisp
	// "$B" vcall thunk
	// "$R" thunk vtordispex

	char	accmem		= *current++;

	if (!(flags & FLAG_NO_ACCESS_SPECIFIERS)) {
		int		access_id	= -1;
		if (accmem == '$') {
			if (*current >= '0' && *current <= '5')
				access_id = (*current - '0') / 2;
			else if (*current == 'R')
				access_id = (current[1] - '0') / 2;
			else if (*current != 'B')
				return false;

		} else if (accmem >= 'A' && accmem <= 'Z') {
			access_id = (accmem - 'A') / 8;

		} else {
			return false;
		}

		switch (access_id) {
			case 0: access = "private: "; break;
			case 1: access = "protected: "; break;
			case 2: access = "public: "; break;
		}

		if (accmem == '$' || (accmem - 'A') % 8 == 6 || (accmem - 'A') % 8 == 7)
			access = format_string("[thunk]:%s", access ? access : " ");
	}

	if (!(flags & FLAG_NO_MEMBER_TYPE)) {
		if (accmem == '$' && *current != 'B') {
			member_type = "virtual ";

		} else if (accmem <= 'X') {
			switch ((accmem - 'A') % 8) {
				case 2: case 3: member_type = "static "; break;
				case 4: case 5: case 6: case 7: member_type = "virtual "; break;
			}
		}
	}

	bool	has_args = true, has_ret = true;

	if (accmem == '$' && *current == 'B') { /* vcall thunk */
		current++;
		int	n;
		if (!get_number(n) || *current++ != 'A')
			return false;
		name		= format_string("%s{%i,{flat}}' }'", name, n);
		has_args	= false;
		has_ret		= false;

	} else if (accmem == '$' && *current == 'R') {/* vtordispex thunk */
		current += 2;
		int		n1, n2, n3, n4;
		if (!get_number(n1) || !get_number(n2) || !get_number(n3) || !get_number(n4))
			return false;
		name	= format_string("%s`vtordispex{%i,%i,%i,%i}' ", name, n1, n2, n3, n4);

	} else if (accmem == '$') { /* vtordisp thunk */
		current++;
		int		n1, n2;
		if (!get_number(n1) || !get_number(n2))
			return false;
		name	= format_string("%s`vtordisp{%i,%i}' ", name, n1, n2);

	} else if ((accmem - 'A') % 8 == 6 || (accmem - 'A') % 8 == 7) { /* a thunk */
		int	n;
		if (!get_number(n))
			return false;
		name	= format_string("%s`adjustor{%i}' ", name, n);
	}

	const char*		modifier	= NULL;
	const char*		ptr_modif	= NULL;
	if (has_args && (accmem == '$' || (accmem <= 'X' && (accmem - 'A') % 8 != 2 && (accmem - 'A') % 8 != 3))) {
		// If there is an implicit this pointer, const modifier follows
		if (!get_modifier(&modifier, &ptr_modif))
			return false;
	}

	const char	*call_conv, *exported;
	if (!get_calling_convention(*current++, &call_conv, &exported, flags))
		return false;

	datatype_t		ct_ret;

	if (has_ret) {
		// Return type, or @ if void
		if (*current == '@') {
			ct_ret.left = "void";
			ct_ret.right = NULL;
			current++;
		} else {
			if (!demangle_datatype(ct_ret, false))
				return false;
		}
		if (flags & FLAG_NO_FUNCTION_RETURNS)
			ct_ret.left = ct_ret.right = NULL;
	}

	if (cast_op) {
		name = format_string("%s%s%s", name, ct_ret.left, ct_ret.right);
		ct_ret.left = ct_ret.right = NULL;
	}

	string	args_str;

	if (has_args && !(args_str = get_args(true)))
		return false;

	// Note: '()' after 'Z' means 'throws', but we don't care here (FIXME)

	if (flags & FLAG_NAME_ONLY) {
		result = name;

	} else {
		string_builder	b;
		b	<< access
			<< member_type
			<< ct_ret.left	<< onlyif(ct_ret.left && !ct_ret.right, ' ')
			<< call_conv	<< onlyif(call_conv, ' ')
			<< exported
			<< name;

		if (args_str)
			b << '(' << args_str << ')';
		if (!(flags & FLAG_NO_THISTYPE))
			b << modifier << onlyif(modifier, ' ') << ptr_modif;
	
		b << ct_ret.right;
		result = b;
	}

	return true;
}

//******************************************************************
//         symbol_demangle
// Demangle a C++ linker symbol

bool parsed_symbol::demangle() {
	uint32		do_after = 0;

	// FIXME seems wrong as name, as it demangles a simple data type
	if (flags & FLAG_NO_ARGUMENTS) {
		datatype_t				ct;
		if (!demangle_datatype(ct, false))
			return false;
		result = format_string("%s%s", ct.left, ct.right);
		return true;
	}

	// MS mangled names always begin with '?'
	if (*current++ != '?')
		return false;

	uint32	mark = stack.size32();
	
	// Then function name or operator code
	if (*current == '?' && (current[1] != '$' || current[2] == '?')) {
		const char* function_name = NULL;

		if (current[1] == '$') {
			do_after = 6;
			current += 2;
		}

		// C++ operator code (one character, or two if the first is '_')
		switch (*++current) {
			case '0': do_after = 1; break;
			case '1': do_after = 2; break;
			case '2': function_name = "operator new"; break;
			case '3': function_name = "operator delete"; break;
			case '4': function_name = "operator="; break;
			case '5': function_name = "operator>>"; break;
			case '6': function_name = "operator<<"; break;
			case '7': function_name = "operator!"; break;
			case '8': function_name = "operator=="; break;
			case '9': function_name = "operator!="; break;
			case 'A': function_name = "operator[]"; break;
			case 'B': function_name = "operator "; do_after = 3; break;
			case 'C': function_name = "operator->"; break;
			case 'D': function_name = "operator*"; break;
			case 'E': function_name = "operator++"; break;
			case 'F': function_name = "operator--"; break;
			case 'G': function_name = "operator-"; break;
			case 'H': function_name = "operator+"; break;
			case 'I': function_name = "operator&"; break;
			case 'J': function_name = "operator->*"; break;
			case 'K': function_name = "operator/"; break;
			case 'L': function_name = "operator%"; break;
			case 'M': function_name = "operator<"; break;
			case 'N': function_name = "operator<="; break;
			case 'O': function_name = "operator>"; break;
			case 'P': function_name = "operator>="; break;
			case 'Q': function_name = "operator,"; break;
			case 'R': function_name = "operator()"; break;
			case 'S': function_name = "operator~"; break;
			case 'T': function_name = "operator^"; break;
			case 'U': function_name = "operator|"; break;
			case 'V': function_name = "operator&&"; break;
			case 'W': function_name = "operator||"; break;
			case 'X': function_name = "operator*="; break;
			case 'Y': function_name = "operator+="; break;
			case 'Z': function_name = "operator-="; break;
			case '_':
				switch (*++current) {
					case '0': function_name = "operator/="; break;
					case '1': function_name = "operator%="; break;
					case '2': function_name = "operator>>="; break;
					case '3': function_name = "operator<<="; break;
					case '4': function_name = "operator&="; break;
					case '5': function_name = "operator|="; break;
					case '6': function_name = "operator^="; break;
					case '7': function_name = "`vftable'"; break;
					case '8': function_name = "`vbtable'"; break;
					case '9': function_name = "`vcall'"; break;
					case 'A': function_name = "`typeof'"; break;
					case 'B': function_name = "`local static guard'"; break;
					case 'C': function_name = "`string'"; do_after = 4; break;
					case 'D': function_name = "`vbase destructor'"; break;
					case 'E': function_name = "`vector deleting destructor'"; break;
					case 'F': function_name = "`default constructor closure'"; break;
					case 'G': function_name = "`scalar deleting destructor'"; break;
					case 'H': function_name = "`vector constructor iterator'"; break;
					case 'I': function_name = "`vector destructor iterator'"; break;
					case 'J': function_name = "`vector vbase constructor iterator'"; break;
					case 'K': function_name = "`virtual displacement map'"; break;
					case 'L': function_name = "`eh vector constructor iterator'"; break;
					case 'M': function_name = "`eh vector destructor iterator'"; break;
					case 'N': function_name = "`eh vector vbase constructor iterator'"; break;
					case 'O': function_name = "`copy constructor closure'"; break;
					case 'R':
						flags |= FLAG_NO_FUNCTION_RETURNS;
						switch (*++current) {
							case '0': {
								current++;
								datatype_t				ct;
								demangle_datatype(ct, false);
								if (!demangle_datatype(ct, false))
									return false;

								function_name = format_string("%s%s `RTTI Type Descriptor'", ct.left, ct.right);
								current--;
								break;
							}
							case '1': {
								current++;
								int		n1, n2, n3, n4;
								if (!get_number(n1) || !get_number(n2) || !get_number(n3) || !get_number(n4))
									return false;
								current--;
								function_name = format_string("`RTTI Base Class Descriptor at (%i,%i,%i,%i)'", n1, n2, n3, n4);
								break;
							}
							case '2': function_name = "`RTTI Base Class Array'"; break;
							case '3': function_name = "`RTTI Class Hierarchy Descriptor'"; break;
							case '4': function_name = "`RTTI Complete Object Locator'"; break;
							default:
								//ERR("Unknown RTTI operator: _R%c\n", *current);
								return false;
								break;
						}
						break;
					case 'S': function_name = "`local vftable'"; break;
					case 'T': function_name = "`local vftable constructor closure'"; break;
					case 'U': function_name = "operator new[]"; break;
					case 'V': function_name = "operator delete[]"; break;
					case 'X': function_name = "`placement delete closure'"; break;
					case 'Y': function_name = "`placement delete[] closure'"; break;
					default:
						//ERR("Unknown operator: _%c\n", *current);
						return false;
				}
				break;
			default:
				// FIXME: Other operators
				//ERR("Unknown operator: %c\n", *current);
				return false;
		}
		current++;
		switch (do_after) {
			case 1: case 2:
				stack.push_back("--null--");
				break;

			case 4:
				result = (char*)function_name;
				return true;

			case 6: {
				string	args = get_args(false);
				if (args)
					function_name = format_string("%s<%s>", function_name, args);
				names.resize(0);
			}
			// fall through
			default:
				stack.push_back(function_name);
				break;
		}

	} else if (*current == '$') {
		// Strange construct, it's a name with a template argument list and that's all
		current++;
		return !!(result = get_template_name());

	} else if (*current == '?' && current[1] == '$') {
		do_after = 5;
	}

	// Either a class name, or '@' if the symbol is not a class member
	switch (*current) {
		case '@': current++; break;
		case '$': break;
		default:
			// Class the function is associated with, terminated by '@@'
			if (!get_class())
				return false;
			break;
	}

	switch (do_after) {
		case 0: default: break;
		case 1: case 2:
			// it's time to set the member name for ctor & dtor
			if (stack.size() - mark <= 1)
				return false;

			if (do_after == 1)
				stack[mark] = stack[mark + 1];
			else
				stack[mark] = "~" + stack[mark + 1];

			// ctors and dtors don't have return type
			flags |= FLAG_NO_FUNCTION_RETURNS;
			break;
		case 3:
			flags &= ~FLAG_NO_FUNCTION_RETURNS;
			break;
		case 5:
			names_start++;
			break;
	}

	// Function/Data type and access level
	string		name = get_class_string(mark);
	stack.resize(mark);
	return
		between(*current, '0', '9')						? handle_data(name)
	:	between(*current, 'A', 'Z') || *current == '$'	? handle_method(name, do_after == 3)
	:	false;
}

string demangle_vc(const char* mangled, uint32 flags) {
//	if (flags & FLAG_NAME_ONLY)
	if (flags == 0)
		flags = FLAG_NAME_ONLY | FLAG_NO_FUNCTION_RETURNS | FLAG_NO_ACCESS_SPECIFIERS | FLAG_NO_MEMBER_TYPE | FLAG_NO_ALLOCATION_LANGUAGE | FLAG_NO_COMPLEX_TYPE;


	char	buffer[1024];
	DWORD	res = UnDecorateSymbolName(mangled, buffer, sizeof(buffer), UNDNAME_NAME_ONLY);
	return buffer;

	parsed_symbol	sym(mangled, flags);
	return sym.demangle() ? sym.result : mangled;
}

extern "C"
char* __unDName(char* buffer, const char* mangled, int buflen,
                      unsigned short int flags);

string demangle_vc3(const char* mangled, uint32 flags);

struct test_demangler {
	test_demangler() {
		char	buffer[1024];
		char	buffer2[1024];

//		const char *mangled = "??$set2@$0BA@@?$set1@Upm_curve3@@@?$element_setter@PEQpm_curve3@@Y03U?$_soft_vector@$02U?$_soft_vector_fields@$02M@iso@@@iso@@@ISO@@SAIVtag@2@AEAUElement@2@@Z";
//		const char *mangled = "??$put@AEBU?$Creation@UCollision_Cone@iso@@@iso@@@?$hash_map_base@U?$hash_entry@IV?$callback@$$A6AXAEBUCreateParams@iso@@V?$crc@$0CA@@2@Varbitrary_const_ptr@2@@Z@iso@@@iso@@@iso@@IEAAPEAU?$hash_entry@IV?$callback@$$A6AXAEBUCreateParams@iso@@V?$crc@$0CA@@2@Varbitrary_const_ptr@2@@Z@iso@@@1@IAEBU?$Creation@UCollision_Cone@iso@@@1@@Z";
		const char *mangled = "?walk_heap@heap@tlsf@@QEAAXP6AXPEAX_K_N0@Z0@Z";
		DWORD	res = UnDecorateSymbolName(mangled, buffer, sizeof(buffer), UNDNAME_NAME_ONLY);

		__unDName(buffer2, mangled, sizeof(buffer2), UNDNAME_NAME_ONLY);

		string s = demangle_vc3(mangled, 0);//x3f);


	}
} _test_demangler;
