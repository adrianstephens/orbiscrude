#include "extra/text_stream.h"
#include "extra/c-types.h"
#include "stream.h"
#include "cpp.h"
#include "extra/expression.h"
#include "extra/ast.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	C_tokeniser
//-----------------------------------------------------------------------------

class C_tokeniser {
	enum TOKEN {
		TOK_EOF					= -1,

		TOK_MINUS				= '-',
		TOK_PLUS				= '+',
		TOK_DOT					= '.',
		TOK_COLON				= ':',
		TOK_SEMICOLON			= ';',
		TOK_NOT					= '~',
		TOK_EQUALS				= '=',
		TOK_LT					= '<',
		TOK_GT					= '>',
		TOK_COMMA				= ',',
		TOK_ASTERIX				= '*',
		TOK_DIVIDE				= '/',
		TOK_MOD					= '%',
		TOK_AMPERSAND			= '&',
		TOK_OR					= '|',
		TOK_XOR					= '^',
		TOK_QUERY				= '?',
		TOK_OPEN_BRACE			= '{',
		TOK_CLOSE_BRACE			= '}',
		TOK_OPEN_BRACKET		= '[',
		TOK_CLOSE_BRACKET		= ']',
		TOK_OPEN_PARENTHESIS	= '(',
		TOK_CLOSE_PARENTHESIS	= ')',

		TOK_IDENTIFIER			= 256,
		TOK_NUMBER,
		TOK_CHARLITERAL,
		TOK_STRINGLITERAL,

		TOK_INC,
		TOK_DEC,
		TOK_MEMBER_PTR,			//.*
		TOK_PTR_MEMBER,			//->
		TOK_PTR_MEMBER_PTR,		//->*
		TOK_ELLIPSIS,
		TOK_EQUIV,
		TOK_NEQ,
		TOK_LE,
		TOK_GE,
		TOK_SHL,
		TOK_SHR,
		TOK_LOG_AND,
		TOK_LOG_OR,
		TOK_SCOPE,
		TOK_SPACESHIP,

		TOK_ADD_EQUALS,
		TOK_SUB_EQUALS,
		TOK_MUL_EQUALS,
		TOK_DIV_EQUALS,
		TOK_MOD_EQUALS,
		TOK_AND_EQUALS,
		TOK_OR_EQUALS,
		TOK_XOR_EQUALS,
		TOK_SHL_EQUALS,
		TOK_SHR_EQUALS,

		_TOK_KEYWORDS,
		TOK_VOID				= _TOK_KEYWORDS,
		TOK_BOOL,
		TOK_CHAR,
		TOK_INT,
		TOK_FLOAT,
		TOK_DOUBLE,
		TOK_LONG,
		TOK_SHORT,
		TOK_UNSIGNED,
		TOK_SIGNED,
		TOK_TYPEDEF,
		TOK_ENUM,
		TOK_STRUCT,
		TOK_UNION,
		TOK_CLASS,
		TOK_PRIVATE,
		TOK_PROTECTED,
		TOK_PUBLIC,
		TOK_STATIC,
		TOK_EXTERN,
		TOK_CONST,
		TOK_VOLATILE,
		TOK_VIRTUAL,
		TOK_OPERATOR,
		TOK_INLINE,
		TOK_TEMPLATE,
		TOK_FRIEND,
		TOK_EXPLICIT,
		TOK_NAMESPACE,
		TOK_TYPENAME,
		TOK_ATTRIBUTE,
	};

	static const char*					_keywords[];
	static hash_map<CRC32Chash, TOKEN>	keywords;

	struct TemplateArgs : dynamic_array<C_arg> {
		TemplateArgs		*prev;
		TemplateArgs(TemplateArgs *prev) : prev(prev) {}
	};

	static const C_arg* find(TemplateArgs *root, const char *id) {
		for (const TemplateArgs *p = root; p; p = p->prev) {
			for (const C_arg *i = p->begin(), *e = p->end(); i != e; ++i) {
				if (i->id == id)
					return i;
			}
		}
		return 0;
	}

	text_reader<reader_intf>	&reader;
	C_types				&types;
	TOKEN				token;
	string				identifier;
	streamptr			last_tell;
	uint32				long_bits, longlong_bits, pointer_bits, bool_bits;
	TemplateArgs		*template_args;

	int					Peek()				{ int c = skip_whitespace(reader); reader.put_back(c); return c;	}

public:
	C_tokeniser(text_reader<reader_intf> &reader, C_types &types);
	C_tokeniser(text_reader<reader_intf> &&reader, C_types &types) : C_tokeniser(reader, types) {}
	~C_tokeniser()		{ reader.r.seek(last_tell); }

	operator			TOKEN()		const	{ return token; }
	TOKEN				GetToken();
	C_tokeniser&		Next()				{ last_tell = reader.tell(); token = GetToken(); return *this;	}
	void				expect(TOKEN tok);
	void				expect(char c)		{ expect(TOKEN(c)); }
	bool				SkipHierarchy();
	int					EvaluateConstant(const C_enum *en, int n);

	void				GetAttributes(uint32 &alignment, uint32 &straddlement);
	const C_type*		ReadNumericType();
	bool				ReadElements(const char *id, dynamic_array<C_element> &elements, C_element::ACCESS access);
	bool				ReadType(const C_type *&type);
	const C_type*		ReadEmbellishedType(const C_type *type, char *name = NULL, bool allowunnamed = false);

	void				Parse();
	ast::node*			ParseExpression(const C_type_composite *scope, ast::get_variable_t get_var);
};

const char* C_tokeniser::_keywords[] = {
	"void",
	"bool",
	"char",
	"int",
	"float",
	"double",
	"long",
	"short",
	"unsigned",
	"signed",
	"typedef",
	"enum",
	"struct",
	"union",
	"class",
	"private",
	"protected",
	"public",
	"static",
	"extern",
	"const",
	"volatile",
	"virtual",
	"operator",
	"inline",
	"template",
	"friend",
	"explicit",
	"namespace",
	"typename",
	"__attribute__",
};
hash_map<CRC32Chash, C_tokeniser::TOKEN>	C_tokeniser::keywords;

C_tokeniser::C_tokeniser(text_reader<reader_intf> &reader, C_types &types) : reader(reader), types(types), template_args(0) {
	long_bits		= 64;
	longlong_bits	= 128;
	bool_bits		= 8;
	pointer_bits	= 64;

	types.add("intptr_t", types.get_static_type<intptr_t>());
	types.add("uintptr_t", types.get_static_type<uintptr_t>());
	types.add("size_t", types.get_static_type<size_t>());

	if (keywords.size() == 0) {
		for (int i = 0; i < num_elements(_keywords); i++)
			keywords[_keywords[i]] = TOKEN(_TOK_KEYWORDS + i);
	}
	Next();
}

void C_tokeniser::expect(TOKEN tok) {
	static const char* _tokens[] = {
		"identifier",
		"number",
		"character literal",
		"string literal",
		"'...'",
		"'=='",
		"'!='",
		"'<='",
		"'>='",
		"'<<'",
		"'>>'",
		"'&&'",
		"'||'",
		"'::'",
	};
	if (token != tok) {
		if (tok < 256)
			throw_accum("Expected '" << (char)tok << "'");
		else if (tok >= _TOK_KEYWORDS)
			throw_accum("Expected " << _keywords[tok - _TOK_KEYWORDS]);
		else
			throw_accum("Expected " << _tokens[tok - 256]);
	}
	Next();
}

C_tokeniser::TOKEN C_tokeniser::GetToken() {
	for (;;) {
		int	c	= skip_whitespace(reader);

		switch (c) {
			case '_':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
				identifier = read_token(reader, char_set::identifier, c);
				if (TOKEN *k = keywords.check(identifier))
					return *k;
				return TOK_IDENTIFIER;

			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				reader.put_back(c);
				return TOK_NUMBER;

			case '\'':
				identifier = read_string(reader, c);
				return TOK_CHARLITERAL;

			case '.':
				c = reader.getc();
				if (c == '*')
					return TOK_MEMBER_PTR;
				if (c == '.') {
					c = reader.getc();
					if (c == '.')
						return TOK_ELLIPSIS;
					reader.put_back(c);
					c = '.';
				}
				reader.put_back(c);
				return TOK_DOT;

			case '+':
				c = reader.getc();
				if (c == '+')
					return TOK_INC;
				if (c == '=')
					return TOK_ADD_EQUALS;
				reader.put_back(c);
				return TOK_PLUS;

			case '-':
				c = reader.getc();
				if (c == '-')
					return TOK_DEC;
				if (c == '=')
					return TOK_SUB_EQUALS;
				if (c == '>') {
					c = reader.getc();
					if (c == '*')
						return TOK_PTR_MEMBER_PTR;
					reader.put_back(c);
					return TOK_PTR_MEMBER;
				}
				reader.put_back(c);
				return TOK_MINUS;

			case '=':
				c = reader.getc();
				if (c == '=')
					return TOK_EQUIV;
				reader.put_back(c);
				return TOK_EQUALS;

			case '<':
				c = reader.getc();
				if (c == '=') {
					c = reader.getc();
					if (c == '>')
						return TOK_SPACESHIP;
					reader.put_back(c);
					return TOK_LE;
				}
				if (c == '<') {
					c = reader.getc();
					if (c == '=')
						return TOK_SHL_EQUALS;
					reader.put_back(c);
					return TOK_SHL;
				}
				reader.put_back(c);
				return TOK_LT;

			case '>':
				c = reader.getc();
				if (c == '=')
					return TOK_GE;
				if (c == '>') {
					c = reader.getc();
					if (c == '=')
						return TOK_SHR_EQUALS;
					reader.put_back(c);
					return TOK_SHR;
				}
				reader.put_back(c);
				return TOK_GT;

			case '!':
				c = reader.getc();
				if (c == '=')
					return TOK_NEQ;
				reader.put_back(c);
				return TOK_NOT;

			case '&':
				c = reader.getc();
				if (c == '&')
					return TOK_LOG_AND;
				if (c == '=')
					return TOK_AND_EQUALS;
				reader.put_back(c);
				return TOK_AMPERSAND;

			case '|':
				c = reader.getc();
				if (c == '|')
					return TOK_LOG_OR;
				if (c == '=')
					return TOK_OR_EQUALS;
				reader.put_back(c);
				return TOK_OR;

			case '^':
				c = reader.getc();
				if (c == '=')
					return TOK_XOR_EQUALS;
				reader.put_back(c);
				return TOK_XOR;

			case '/':
				c = reader.getc();
				if (c == '=')
					return TOK_DIV_EQUALS;
				if (c == '/') {
					do c = reader.getc(); while (c != '\n' && c != EOF);
				} else if (c == '*') {
					c = reader.getc();
					do {
						while (c != '*' && c != EOF) c = reader.getc();
						c = reader.getc();
					} while (c != '/' && c != EOF);
				} else {
					reader.put_back(c);
					return TOK_DIVIDE;
				}
				continue;

			case '*':
				c = reader.getc();
				if (c == '=')
					return TOK_MUL_EQUALS;
				reader.put_back(c);
				return TOK_ASTERIX;

			case '%':
				c = reader.getc();
				if (c == '=')
					return TOK_MOD_EQUALS;
				reader.put_back(c);
				return TOK_MOD;

			case '"':
				identifier = read_string(reader, c);
				return TOK_STRINGLITERAL;

			case '#':
				do c = reader.getc(); while (c != '\n' && c != EOF);
				break;

			case ':':
				c = reader.getc();
				if (c == ':')
					return TOK_SCOPE;
				reader.put_back(c);
				return TOK_COLON;

			default:
				return (TOKEN)c;
		}
	}
}

bool C_tokeniser::SkipHierarchy() {
	int		stack[64], *sp = stack;
	bool	angles = token == TOK_LT;

	do {
		switch (token) {
			case TOK_EOF:
				return false;

			case TOK_LT:
				if (!angles)
					break;
			case TOK_OPEN_BRACE:
			case TOK_OPEN_BRACKET:
			case TOK_OPEN_PARENTHESIS:
				*sp++ = token;
				break;

			case TOK_GT:
				if (!angles)
					break;
				if (sp[-1] != TOK_LT)
					return false;
				--sp;
				break;

			case TOK_CLOSE_BRACE:
				if (sp[-1] != TOK_OPEN_BRACE)
					return false;
				--sp;
				break;

			case TOK_CLOSE_BRACKET:
				if (sp[-1] != TOK_OPEN_BRACKET)
					return false;
				--sp;
				break;

			case TOK_CLOSE_PARENTHESIS:
				if (sp[-1] != TOK_OPEN_PARENTHESIS)
					return false;
				--sp;
				break;

			case TOK_NUMBER:
				read_number(reader, 0);
				break;
		}
		Next();
	} while (sp > stack);
	return true;
}

void C_tokeniser::GetAttributes(uint32 &alignment, uint32 &straddlement) {
	if (token == TOK_ATTRIBUTE) {
		Next();
		expect('('); expect('(');

		for (;;) {
			int		attribute	= 0;
			int		value		= 0;
			if (token == TOK_IDENTIFIER) {
				if (identifier == "aligned")
					attribute = 1;
				if (identifier == "nostraddle")
					attribute = 2;
			}
			Next();
			if (token == TOK_OPEN_PARENTHESIS) {
				expect('(');
				value = EvaluateConstant(NULL, 0);
				expect(')');
			}

			switch (attribute) {
				case 1:	alignment		= value; break;
				case 2: straddlement	= value; break;
			}
			if (token != TOK_COMMA)
				break;
			Next();
		}

		expect(')');
	}
}

int C_tokeniser::EvaluateConstant(const C_enum *en, int n) {
	struct context {
		C_tokeniser	&tok;
		void	backup()	{}

		int		get_value(bool discard)	{
			if (tok.token == TOK_NUMBER) {
				int	v = read_integer(tok.reader, 0);
				tok.Next();
				return v;
			}
			//FAIL!
			return 0;
		}

		expression::OP get_op() {
			expression::OP	op;
			switch (tok) {
				case ')':	op = expression::OP_RPA; break;
				case '*':	op = expression::OP_MUL; break;
				case '/':	op = expression::OP_DIV; break;
				case '%':	op = expression::OP_MOD; break;
				case '+':	op = expression::OP_ADD; break;
				case '-':	op = expression::OP_SUB; break;
				case '^':	op = expression::OP_XOR; break;
				case '?':	op = expression::OP_QUE; break;
				case ':':	op = expression::OP_COL; break;
				case '<':	op = expression::OP_LT;	 break;
				case '>':	op = expression::OP_GT;	 break;
				case TOK_SHL:		op = expression::OP_SL;	 break;
				case TOK_SHR:		op = expression::OP_SR;	 break;
				case TOK_EQUIV:		op = expression::OP_EQ;	 break;
				case TOK_NEQ:		op = expression::OP_NE;	 break;
				case TOK_LE:		op = expression::OP_LE;	 break;
				case TOK_GE:		op = expression::OP_GE;	 break;
				case TOK_LOG_OR:	op = expression::OP_ORO; break;
				case TOK_LOG_AND:	op = expression::OP_ANA; break;
				case '(':	op = expression::OP_LPA; break;
				case '~':	op = expression::OP_COM; break;
	//			case '+':	op = expression::OP_PLU; break;
	//			case '-':	op = expression::OP_NEG; break;
				case '!':	op = expression::OP_NOT; break;
				default:	return expression::OP_END;
			}
			tok.Next();
			return op;
		}

		context(C_tokeniser	&_tok) : tok(_tok) {}
	};
	context	ctx(*this);
	return expression::evaluate<int>(ctx);
}

const C_type *C_tokeniser::ReadNumericType() {
	enum {
		SIGNED		= 1 << 0,
		UNSIGNED	= 1 << 1,
		SHORT		= 1 << 2,
		LONG		= 1 << 3,
		LONGLONG	= 1 << 4,
	};
	int	flags		 = 0;

	if (token == TOK_UNSIGNED) {
		flags |= UNSIGNED;
		Next();
	} else if (token == TOK_SIGNED) {
		flags |= SIGNED;
		Next();
	}

	if (token == TOK_SHORT) {
		flags |= SHORT;
		Next();
	} else if (token == TOK_LONG) {
		Next();
		if (token == TOK_LONG) {
			flags |= LONGLONG;
			Next();
		} else {
			flags |= LONG;
		}
	}

	if (token == TOK_FLOAT) {
		Next();
		return flags & SHORT ? C_type_float::get<16,5,true>() : flags & (LONG|LONGLONG) ? C_type_float::get<double>() : C_type_float::get<float>();
	}

	if (token == TOK_DOUBLE) {
		Next();
		return flags & (LONG|LONGLONG) ? C_type_float::get<80,15,true>() : C_type_float::get<double>();
	}

	int				nbits		= 32;
	C_type::FLAGS	int_flags	= flags & UNSIGNED ? C_type::NONE : C_type_int::SIGN;

	if (token == TOK_CHAR) {
		int_flags	|= C_type_int::CHAR;
		nbits		= 8;
		Next();

	} else if (token == TOK_INT) {
		Next();

	} else if (!flags) {
		return NULL;
	}

	if (flags & (SHORT | LONG | LONGLONG))
		nbits = flags & SHORT ? 16 : flags & LONG ? long_bits : longlong_bits;

	return C_type_int::get(nbits, int_flags);
}

bool C_tokeniser::ReadElements(const char *id, dynamic_array<C_element> &elements, C_element::ACCESS access) {
	char				name[64];
	const C_type		*type;

	if (token == TOK_COLON) {	//inherit
		C_element::ACCESS	inherit_access = access;
		for (;;) {
			Next();

			switch (token) {
				case TOK_PUBLIC:	inherit_access = C_element::PUBLIC; Next(); break;
				case TOK_PRIVATE:	inherit_access = C_element::PRIVATE; Next(); break;
				case TOK_PROTECTED:	inherit_access = C_element::PROTECTED; Next(); break;
			}

			if (token != TOK_IDENTIFIER)
				throw("Bad inheritance");

			if (const C_type *sub = types.lookup(identifier)) {
				if (sub->type == C_type::TEMPLATE) {
					string	id = identifier;
					Next();
					expect('<');
					ReadType(type);
					expect('>');
					sub = types.instantiate((C_type_template*)sub, id, type);
				}
				if (sub->type == C_type::STRUCT) {
					elements.push_back(C_element(0, sub, inherit_access));
					Next();
					if (token == TOK_COMMA)
						continue;
					break;
				}
			}
		}
	}

	if (token == TOK_OPEN_BRACE) {
		Next();
		while (token != TOK_CLOSE_BRACE) {

			if (token == TOK_PUBLIC || token == TOK_PRIVATE || token == TOK_PROTECTED) {
				access	= token == TOK_PUBLIC	? C_element::PUBLIC
						: token == TOK_PRIVATE	? C_element::PRIVATE
						: C_element::PROTECTED;
				Next();
				expect(':');
				continue;
			}

			if (token == TOK_EXPLICIT) {
				Next();
				continue;

			} else if (token == TOK_FRIEND) {
				Next();
				ReadType(type);

			} else if (token == TOK_TYPEDEF) {
				Next();
				if (!ReadType(type))
					throw("Bad typedef");

				for (;;) {
					const C_type	*type2	= ReadEmbellishedType(type, name);

					if (token == TOK_OPEN_PARENTHESIS) {
						SkipHierarchy();	// don't add functions
					} else {
						types.add(name, type2);
					}

					if (token != TOK_COMMA)
						break;

					Next();
				}

			} else if (token == TOK_IDENTIFIER && id && strcmp(identifier, id) == 0) {
				//constructor
				Next();
				if (token != TOK_OPEN_PARENTHESIS)
					throw("Missing (");
				SkipHierarchy();

				if (token == TOK_COLON) {
					do {
						Next();
						if (token != TOK_IDENTIFIER)
							throw("Bad initialiser");
						Next();
						if (token != TOK_OPEN_PARENTHESIS)
							throw("Missing (");
						SkipHierarchy();
					} while (token == TOK_COMMA);

					if (token != TOK_OPEN_BRACE)
						throw("Missing {");

				}
				if (token == TOK_OPEN_BRACE) {
					SkipHierarchy();
					continue;
				}
			} else if (token != TOK_SEMICOLON) {

				bool	virtual_function = token == TOK_VIRTUAL;

				if (virtual_function)
					Next();

				if (token == TOK_NOT) {
					//destructor
					Next();
					if (token != TOK_IDENTIFIER || strcmp(identifier, id) != 0)
						throw("Bad destructor");
					Next();
					expect('(');
					if (token != TOK_CLOSE_PARENTHESIS)
						throw("Destructor cannot have arguments");
					Next();
					if (token == TOK_OPEN_BRACE) {
						SkipHierarchy();
						continue;
					}
				} else if (token == TOK_OPERATOR) {
					//conversion operator
					Next();
					if (!ReadType(type))
						throw("Bad conversion operator");
					while (token == TOK_ASTERIX || token == TOK_AMPERSAND)
						Next();
					expect('(');
					if (token != TOK_CLOSE_PARENTHESIS)
						throw("Conversion operator cannot have arguments");
					Next();
					if (token == TOK_OPEN_BRACE) {
						SkipHierarchy();
						continue;
					}
				} else {
					if (!ReadType(type))
						throw("Bad struct");

					if (token == TOK_SEMICOLON) {
						if (type->type == C_type::STRUCT && !((C_type_struct*)type)->id)
							elements.push_back(C_element(0, type, access));

					} else {
						if (token == TOK_OPEN_PARENTHESIS) {	//probably just a macro - ignore it
							SkipHierarchy();
							continue;
						}
						if (token == TOK_OPERATOR) {
						}

						for (const C_type *type2 = ReadEmbellishedType(type, name);;) {
							elements.push_back(C_element((const char*)name, type2, access));
							if (token != TOK_COMMA)
								break;

							Next();
							type2	= ReadEmbellishedType(type, name);
						}
					}
				}
			}

			expect(';');
		}
		Next();
	}
	return true;
}

bool C_tokeniser::ReadType(const C_type *&type) {
	while (token == TOK_STATIC || token == TOK_CONST || token == TOK_VOLATILE || token == TOK_INLINE || token == TOK_EXTERN)
		Next();

	switch (token) {
		case TOK_STRUCT:
		case TOK_UNION:
		case TOK_CLASS: {
			string		id;
			bool		un	= token == TOK_UNION;

			Next();
			uint32		alignment	= 0, straddlement = 0;
			GetAttributes(alignment, straddlement);

			if (token == TOK_IDENTIFIER) {
				id		= identifier;
				Next();
			}

			dynamic_array<C_element> elements;
			if (ReadElements(id, elements, C_element::PUBLIC)) {
				C_type_composite	*comp	= new C_type_composite(un ? C_type::UNION : C_type::STRUCT, C_type::NONE, move(id));
				int					n		= int(elements.size());

				size_t	offset = 0;
				for (int i = 0; i < n; i++) {
					if (straddlement) {
						if (offset % straddlement + elements[i].type->size() > straddlement)
							offset = align(offset, straddlement);
					}
					C_element *e = comp->add(elements[i].id, elements[i].type, offset, C_element::ACCESS(elements[i].access));
					if (!un)
						offset	+= elements[i].type->size();
				}
				comp->_size = offset;

				if (comp->id)
					types.add(comp->id, comp);
				type	= comp;
				return true;
			}

			return false;
		}

		case TOK_ENUM: {
			string		id;
			Next();
			if (token == TOK_IDENTIFIER) {
				id		= identifier;
				Next();
			}

			if (token == TOK_OPEN_BRACE) {
				Next();
				dynamic_array<C_enum>	a;
				int		value	= 0;

				for (;;) {
					if (token == TOK_CLOSE_BRACE)
						break;

					if (token != TOK_IDENTIFIER)
						throw("Bad enum");

					string	id	= identifier;

					Next();
					if (token == TOK_EQUALS) {
						Next();
						value = EvaluateConstant(a, int(a.size()));
					}

					a.emplace_back(id, value++);

					if (token != TOK_COMMA)
						break;
					Next();
				}
				expect('}');

				uint32	n = uint32(a.size());
				C_type_enum *enm = new C_type_enum(32, n);
				for (uint32 i = 0; i < n; i++)
					(*enm)[i] = a[i];
				if (id)
					types.add(id, enm);
				type	= enm;
				return true;
			}
		}

		case TOK_VOID:
			Next();
			type	= 0;
			return true;

		case TOK_BOOL:
			Next();
			type = bool_bits == 8 ? C_type_bool::get<8>() : C_type_bool::get<32>();//new C_type_bool(bool_bits);
			return true;

		case TOK_TEMPLATE: {
			Next();
			if (token != TOK_LT)
				throw("Missing template arguments");

			TemplateArgs	args(template_args);
			auto	saver	= save(template_args, &args);

			Next();
			for (int i = 0;;) {
				const C_type *arg_type = 0;
				if (token == TOK_TYPENAME || token == TOK_CLASS) {
					arg_type	= new C_type_templateparam(i++);
					Next();
				} else {
					if (!ReadType(arg_type))
						throw("bad template argument");
				}

				char	arg_name[64];
				arg_type = ReadEmbellishedType(arg_type, arg_name);
				args.emplace_back(arg_name, arg_type);

				if (token != TOK_COMMA)
					break;
				Next();
			}
			expect('>');

			if (token == TOK_CLASS || token == TOK_STRUCT || token == TOK_UNION) {
				bool		un	= token == TOK_UNION;

				Next();
				if (token != TOK_IDENTIFIER)
					throw("missing template name");

				string		id	= identifier;
				Next();

				dynamic_array<C_element> elements;
				if (ReadElements(id, elements, C_element::PUBLIC)) {
					C_type_composite	*comp	= new C_type_composite(un ? C_type::UNION : C_type::STRUCT, C_type::NONE, move(id));
					for (int i = 0, n = int(elements.size()); i < n; i++)
						comp->add(elements[i].id, elements[i].type, 0, C_element::ACCESS(elements[i].access));

					C_type_template		*temp	= new C_type_template(comp, 0);
					for (int i = 0, n = int(args.size()); i < n; i++)
						temp->add(args[i].id, args[i].type);

					types.add(comp->id, temp);
					type	= temp;
					return true;
				}
				return false;

			} else {
				//templated function
				do Next(); while (token != TOK_OPEN_PARENTHESIS);
				SkipHierarchy();
				if (token == TOK_CONST)
					Next();
				if (token == TOK_OPEN_BRACE)
					SkipHierarchy();
				else
					expect(';');
			}
			return ReadType(type);
		}

		case TOK_IDENTIFIER: {
			if (const C_type *p = types.lookup(identifier)) {
				Next();

				if (p->type == C_type::TEMPLATE) {
					expect('<');
					ReadType(type);
					expect('>');
					p = types.instantiate((C_type_template*)p, identifier, type);
				}

				type = p;
				return true;

			} else if (const C_arg *arg = find(template_args, identifier)) {
				Next();
				type = arg->type;
				return true;
			}

			if (Peek() == '(') {
				Next();
				Next();
				if (!ReadType(type))
					return false;
				type = ReadEmbellishedType(type, NULL, true);
				expect(')');
				return true;
			}

			while (Peek() == ':') {
				Next();
				expect(TOK_SCOPE);
			}
			if (const C_type *p = types.lookup(identifier)) {
				Next();
				type = p;
				return true;
			}
			types.add(identifier, 0);
			Next();
			return false;
		}

		default:
			return !!(type = ReadNumericType());
	}
}

const C_type *C_tokeniser::ReadEmbellishedType(const C_type *type, char *name, bool allowunnamed) {
	const C_type	*type2	= type;

	while (token == TOK_ASTERIX || token == TOK_AMPERSAND || token == TOK_CONST || token == TOK_VOLATILE) {
		bool	ref = token == TOK_AMPERSAND;
		if (ref || token == TOK_ASTERIX) {
			Next();
			if (token == TOK_SHORT) {
				type2 = types.add(C_type_pointer(type2, 16, ref));
				Next();
			} else if (token == TOK_LONG) {
				Next();
				if (token == TOK_LONG) {
					type2 = types.add(C_type_pointer(type2, longlong_bits, ref));
					Next();
				} else {
					type2 = types.add(C_type_pointer(type2, long_bits, ref));
				}
			} else {
				type2 = types.add(C_type_pointer(type2, pointer_bits, ref));
			}
		}
	}

//	if (token == TOK_LT)
//		SkipHierarchy();

	if (token != TOK_IDENTIFIER) {
		if (token == TOK_OPEN_PARENTHESIS) {
			Next();
			while (token == TOK_ASTERIX || token == TOK_AMPERSAND)
				Next();
			type2 = ReadEmbellishedType(type2, name, true);
			expect(')');
		} else if (name) {
			name[0] = 0;
		} else if (!allowunnamed) {
			throw("Bad name");
		}
	} else if (name) {
		strcpy(name, identifier);
		Next();

	} else if (!allowunnamed) {
		throw("Bad name");
	}

	if (token == TOK_OPEN_BRACKET) {
		dynamic_array<int>	dims;
		while (token == TOK_OPEN_BRACKET) {
			Next();
			int	i = EvaluateConstant(NULL, 0);
			if (token != TOK_CLOSE_BRACKET)
				throw("Bad array size");
			dims.push_back(i);
			Next();
		}
		for (int *p = dims.end(); p-- > dims.begin();)
			type2 = types.add(C_type_array(type2, *p));
	}

	if (token == TOK_COLON) {
		if (type2->type != C_type::INT)
			throw("Bitfields can only be used with integer types");
		Next();
		C_type_int	t(EvaluateConstant(NULL, 0), false);
		t.flags = type2->flags | C_type::PACKED;
		type2 = types.add(t);
	}

	if (token == TOK_OPEN_PARENTHESIS) {
		//function
		Next();
		C_type_function	*fn	= new C_type_function(type2);
		if (token != TOK_CLOSE_PARENTHESIS) for (;;) {
			const C_type *arg_type = 0;
			if (!ReadType(arg_type))
				throw("bad function argument");

			char	arg_name[64];
			arg_type	= ReadEmbellishedType(arg_type, arg_name, true);

			fn->args.push_back(C_arg(arg_name, arg_type));
			if (token != TOK_COMMA)
				break;
			Next();
		}
		expect(TOK_CLOSE_PARENTHESIS);
		return fn;
	}

	return type2;
}

void C_tokeniser::Parse() {
	for (;;) {
		const C_type		*type;
		if (token == TOK_NAMESPACE) {
			Next();
			if (token == TOK_IDENTIFIER)
				Next();
			expect(TOK_OPEN_BRACE);
			Parse();
			expect(TOK_CLOSE_BRACE);

		} else if (token == TOK_TYPEDEF) {
			Next();
			if (ReadType(type)) for (;;) {
				char	name[64];
				const C_type	*type2	= ReadEmbellishedType(type, name);
				types.add(name, type2);
				if (token != TOK_COMMA)
					break;
				Next();
			}
			expect(';');

		} else  if (token == TOK_EXTERN) {
			Next();
			if (token == TOK_STRINGLITERAL) {
				Next();
				if (token == TOK_OPEN_BRACE) {
					Next();
					Parse();
					expect(TOK_CLOSE_BRACE);
					continue;
				}
			}

		} else if (token == TOK_CLOSE_BRACE || token == TOK_EOF) {
			return;

		} else if (ReadType(type)) {
			while (token != TOK_EOF && token != TOK_SEMICOLON) switch (token) {
				case TOK_ASTERIX:
				case TOK_AMPERSAND:
				case TOK_CONST:
				case TOK_VOLATILE:
				case TOK_LT:
				case TOK_OPEN_PARENTHESIS:
				case TOK_OPEN_BRACKET:
				case TOK_IDENTIFIER: {
					char	name[256];
					ReadEmbellishedType(type, name);
					if (token == TOK_OPEN_PARENTHESIS)
						SkipHierarchy();	// skip function defs
					break;
				}

				case TOK_EQUALS:
					Next();
					while (token != TOK_SEMICOLON && token != TOK_COMMA)
						SkipHierarchy();
					break;

				default:
					if (!SkipHierarchy())
						Next();
					break;
			}
			Next();
		} else {
			return;
		}
	}
}

bool iso::ReadCType(text_reader<reader_intf> &reader, C_types &types, const C_type *&type) {
	return C_tokeniser(reader, types).ReadType(type);
}

bool iso::ReadCType(istream_ref file, C_types &types, const C_type *&type) {
	return C_tokeniser(text_reader<reader_intf>(file), types).ReadType(type);
}

const C_type *iso::ReadCType(istream_ref file, C_types &types, C_type *type, char *name) {
	return C_tokeniser(text_reader<reader_intf>(file), types).ReadEmbellishedType(type, name, true);
}

const C_type *iso::ReadCType(istream_ref file, C_types &types, char *name) {
	text_reader<reader_intf>	reader(file);
	C_tokeniser				tok(reader, types);
	const C_type			*type;
	return tok.ReadType(type) ? tok.ReadEmbellishedType(type, name, true) : 0;
}

void iso::ReadCTypes(istream_ref file, C_types &types) {
	C_tokeniser(text_reader<reader_intf>(file), types).Parse();
}

ast::node* C_tokeniser::ParseExpression(const C_type_composite *scope, ast::get_variable_t get_var) {
	if (token == TOK_EOF)
		return nullptr;

	// Operator stack
	struct Operator	{
		ast::KIND	kind:8;
		uint8		prec;
	} ops[32], *opsp = ops;

	// Value stack
	ref_ptr<ast::node>	vals[32], *vsp = vals;

	*opsp	= {ast::end, uint8(0)};
	bool	need_bin	= false;

	for (;; Next()) {
		ast::KIND	kind	= ast::none;
		uint8		prec	= 0;
		ast::node	*postval= 0;

		if (need_bin) {
			switch (token) {
				case TOK_DEC:			kind = ast::dec; prec = 0xff;	break;
				case TOK_INC:			kind = ast::inc; prec = 0xff;	break;

				case TOK_OPEN_PARENTHESIS:
					kind		= ast::call;
					prec		= 0xff;
					need_bin	= false;
					break;

				case TOK_CLOSE_PARENTHESIS: prec = 1;			break;
				default:
				case TOK_EOF:			break;

				case TOK_MINUS:			kind = ast::sub;		break;
				case TOK_PLUS:			kind = ast::add;		break;
				case TOK_COMMA:			kind = ast::comma;		break;
				case TOK_EQUALS:		kind = ast::assign;		break;
				case TOK_ADD_EQUALS:	kind = ast::add_assign;	break;
				case TOK_SUB_EQUALS:	kind = ast::sub_assign;	break;
				case TOK_MUL_EQUALS:	kind = ast::mul_assign;	break;
				case TOK_DIV_EQUALS:	kind = ast::div_assign;	break;
				case TOK_MOD_EQUALS:	kind = ast::mod_assign;	break;
				case TOK_AND_EQUALS:	kind = ast::and_assign;	break;
				case TOK_OR_EQUALS:		kind = ast::or_assign;	break;
				case TOK_XOR_EQUALS:	kind = ast::xor_assign;	break;
				case TOK_SHL_EQUALS:	kind = ast::shl_assign;	break;
				case TOK_SHR_EQUALS:	kind = ast::shr_assign;	break;
				case TOK_QUERY:			kind = ast::query;		break;
				case TOK_COLON:			kind = ast::colon;		break;
				case TOK_LOG_OR:		kind = ast::log_or;		break;
				case TOK_LOG_AND:		kind = ast::log_and;	break;
				case TOK_OR:			kind = ast::bit_or;		break;
				case TOK_XOR:			kind = ast::bit_xor;	break;
				case TOK_AMPERSAND:		kind = ast::bit_and;	break;
				case TOK_EQUIV:			kind = ast::eq;			break;
				case TOK_NEQ:			kind = ast::ne;			break;
				case TOK_LT:			kind = ast::lt;			break;
				case TOK_GT:			kind = ast::gt;			break;
				case TOK_LE:			kind = ast::le;			break;
				case TOK_GE:			kind = ast::ge;			break;
				case TOK_SPACESHIP:		kind = ast::spaceship;	break;
				case TOK_SHL:			kind = ast::shl;		break;
				case TOK_SHR:			kind = ast::shr;		break;
				case TOK_ASTERIX:		kind = ast::mul;		break;
				case TOK_DIVIDE:		kind = ast::div;		break;
				case TOK_MOD:			kind = ast::mod;		break;
				case TOK_DOT:			kind = ast::field;		break;
				case TOK_PTR_MEMBER:	kind = ast::field;		break;
				case TOK_PTR_MEMBER_PTR:kind = ast::deref_field;break;
				case TOK_OPEN_BRACKET:	kind = ast::ldelem;		break;
				case TOK_CLOSE_BRACKET:	kind = ast::ldelem; prec = 1; break;
			}
			if (kind && !prec) {
				prec		= ast::binary_node::get_info(kind)->precedence * 8;
				need_bin	= false;
			}

		} else {
			switch (token) {
				case TOK_NUMBER:
					*vsp++		= new ast::lit_node(read_number(reader, 0));
					need_bin	= true;
					continue;

				case TOK_CHARLITERAL:
					*vsp++		= new ast::lit_node(identifier[0]);
					need_bin	= true;
					continue;

				case TOK_STRINGLITERAL:
					break;

				case TOK_MINUS:			kind = ast::neg;	break;
				case TOK_PLUS:			kind = ast::pos;	break;
				case TOK_DEC:			kind = ast::dec;	break;
				case TOK_INC:			kind = ast::inc;	break;
				case TOK_AMPERSAND:		kind = ast::ref;	break;
				case TOK_ASTERIX:		kind = ast::deref;	break;

				case TOK_OPEN_PARENTHESIS:	prec = 0xff;	break;

				default: {
					const C_type *type;
					if (ReadType(type)) {
						if (token != TOK_OPEN_PARENTHESIS) {
							ISO_ASSERT(!opsp->kind);
							type = ReadEmbellishedType(type, 0, true);
							ISO_ASSERT(token == TOK_CLOSE_PARENTHESIS);
							--opsp;
						}
						kind	= ast::cast;
						postval	= new ast::unary_node(ast::cast, nullptr, type);
						break;
					}
					if (token >= 128 || !is_alphanum(reader.peekc())) {
						iso_throw("Operator in incorrect context");
						break;
					}

					identifier	= read_token(reader, char_set::identifier, token);
				}
				//fall through

				case TOK_IDENTIFIER: {
					if (opsp->kind == ast::field) {
						if (auto s = GetScope(vsp[-1]->type)) {
							if (auto e = s->get(identifier)) {
								*vsp++		= new ast::element_node(ast::element, e);
								need_bin	= true;
								continue;
							}
						}
						*vsp++		= new ast::name_node(identifier, ast::NOLOOKUP);
						need_bin	= true;
						continue;
					}

					if (scope) {
						if (auto *e = scope->get(identifier)) {
							ast::noderef	t(new ast::name_node("this"));
							t			= t[e];
							*vsp++		= t;
							need_bin	= true;
							continue;
						}
					}

					// get fully qualified name (including template parameters)
					char	c = reader.getc();
					for (;;) {
						while (c == ':' && reader.peekc() == ':') {
							reader.getc();
							identifier << c << read_token(reader, char_set::identifier, c);
							c = reader.getc();
						}

						if (c != '<')
							break;
						identifier << c << read_to(reader, '>');
						c = reader.getc();
					}

					reader.put_back(c);

					if (!(postval = get_var ? get_var(identifier) : nullptr)) {
						if (auto type = types.lookup(identifier))
							postval	= new ast::unary_node(ast::cast, nullptr, type);
						else
							postval = new ast::name_node(identifier);
					}

					if (postval->kind == ast::cast) {
						Next();
						if (token != TOK_OPEN_PARENTHESIS) {
							ISO_ASSERT(!opsp->kind);
							postval->type = ReadEmbellishedType(postval->type, 0, true);
							ISO_ASSERT(token == TOK_CLOSE_PARENTHESIS);
							--opsp;
						}
						kind = ast::cast;
						break;
					}

					*vsp++		= postval;
					need_bin	= true;
					continue;
				}

			}
			prec	= 0xff;
		}

		while (prec <= opsp->prec) {
			auto stackk = (opsp--)->kind;

			if (stackk == ast::end) {
				return vals[0].detach();

			} else if (stackk == ast::none) {
				if (prec != 1)
					iso_throw("Missing ')'");
				break;

			} else if (stackk == ast::call) {
				if (prec != 1)
					iso_throw("Missing ')'");
				auto	&v2 = *--vsp;
				auto	f	= new ast::call_node(vsp[-1]);
				f->add(v2);
				vsp[-1] = f;
				break;

			} else if (stackk == ast::cast) {
				auto	&v2 = *--vsp;
				vsp[-1]->cast<ast::unary_node>()->arg = v2;

			} else if (is_binary(stackk)) {
				auto	&v2 = *--vsp;
				auto	&v1 = vsp[-1];
				v1 = new ast::binary_node(stackk, v1, v2, nullptr);

				if (stackk == ast::ldelem) {
					if (kind != ast::ldelem)
						iso_throw("Missing ']'");
					break;
				}

			} else {
				vsp[-1] = new ast::unary_node(stackk, vsp[-1], nullptr);
			}
		}

		if (postval)
			*vsp++	= postval;

		if (prec > 1) {
			opsp++;
			opsp->kind	= kind;
			opsp->prec	= prec == 0xff ? 1 : prec;
		}

	}
}

ast::node* iso::ReadCExpression(text_reader<reader_intf> &reader, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var) {
	return C_tokeniser(reader, types).ParseExpression(scope, get_var);
}

ast::node* iso::ReadCExpression(istream_ref file, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var) {
	return C_tokeniser(text_reader<reader_intf>(file), types).ParseExpression(scope, get_var);
}
/*
namespace iso {
string			demangle_vc(const char* mangled, uint32 flags);
string_accum&	demangle_vc(string_accum &a, const char* mangled, uint32 flags);
}
*/
//-----------------------------------------------------------------------------
//	Dumper
//-----------------------------------------------------------------------------
/*
fixed_string<1024> demangle_name(const char *s) {
	fixed_string<1024>	r;
	if (const char *q = string_find(s, '?')) {
		demangle_vc(rvalue(fixed_accum(r)), q, 0x7f);
		s = r;
	}
	char	*d = r;
	for (;;) {
		const char *e;
		if (s[0] == '<') {
			e = string_find(++s, '>');
			memcpy(d, s, e - s);
			d += e - s;
			++e;
		} else {
			e = string_find(s, char_set(".:"));
			if (!e)
				e = s + strlen(s);
			memcpy(d, s, e - s);
			d += e - s;
		}
		if (!(*d++ = *e++))
			break;
		if (*e == ':')
			*d++ = *e++;
		s = e;
	}
	return r;
}
*/


string_accum &DumpType1(string_accum &sa, const C_type *type, const char *name, int indent, bool _typedef = false) {
	if (!type) {
		sa << "void";

	} else if (type == C_types::get_static_type<bool>()) {
		sa << "bool";

	} else switch (type->type) {
		case C_type::INT: {
			C_type_int	*i = (C_type_int*)type;
			if (i->scale() != 1) {
				sa << onlyif(!i->sign(), 'u') << "norm" << i->num_bits();
				break;
			}
			if (!i->sign())
				sa << "unsigned ";
			switch (i->num_bits()) {
				case 8:		sa << "char";	break;
				case 16:	sa << "short";	break;
				case 32:	sa << "int";	break;
				case 64:	sa << "_int64";	break;
			}
			break;
		}
		case C_type::FLOAT:	{
			C_type_float	*i = (C_type_float*)type;
			switch (i->num_bits()) {
				case 16:	sa << "short float";	break;
				case 32:	sa << "float";	break;
				case 64:	sa << "double";	break;
			}
			break;
		}
		case C_type::TEMPLATE: {
			C_type_template	*t = (C_type_template*)type;
			const C_type	*s = t->subtype;
			const char *id	= name;
			switch (s->type) {
				case C_type::UNION:
				case C_type::STRUCT:
					id = s->composite()->id;
					break;
			}
			sa << id << '<';
			for (auto *i = t->args.begin(), *e = t->args.end(); i != e; ++i) {
				sa << i->id;
				if (i != e - 1)
					sa << ", ";
			}
			sa << '>';
			break;
		}
		case C_type::UNION:
		case C_type::STRUCT: {
			auto	*s = type->composite();
			const char *kind = type->type == C_type::UNION ? "union" : type->type == C_type::STRUCT ? "struct" : "namespace";

			if (_typedef) {
				if (s->id && s->id != name) {
					sa << "typedef " << kind << ' ' << s->id;
				} else {
					sa << kind << ' ' << name;
					name = 0;
				}
			} else {
				if (s->id) {
					sa << s->id;
					break;
				}
				sa << kind;
			}
			auto	*i	= s->elements.begin(), *e = s->elements.end();
			const char	*sep	= " : ";
			while (i != e && !i->id) {
				C_type_struct	*s = (C_type_struct*)i++->type;
				sa << sep << s->id;
				sep = ", ";
			}
			sa << " {\n";
			C_element::ACCESS	access = C_element::PUBLIC;
			static const char *access_names[] = {"public", "protected", "private"};
			while (i != e) {
				if (access != i->access) {
					access = (C_element::ACCESS)i->access;
					sa << repeat('\t', indent) << access_names[access] << ":\n";
				}
				sa << repeat('\t', indent + 1);
				bool	fn = i->type->type == C_type::FUNCTION;
				if (i->is_static() && (!fn || !(i->type->flags & C_type_function::HASTHIS)))
					sa << "static ";
				DumpType1(sa, i->type, i->id, indent + 1, _typedef && fn) << ";\n";
				++i;
			}
			sa << repeat('\t', indent) << "}";
			break;
		}
		case C_type::ARRAY:
			DumpType1(sa, type->subtype(), name, indent);
			return sa << '[' << ((C_type_array*)type)->count << ']';

		case C_type::POINTER: {
			C_type_pointer	*p = (C_type_pointer*)type;
			DumpType1(sa, type->subtype(), 0, indent) << (p->reference() ? '&' : '*');
			break;
		}
		case C_type::FUNCTION: {
			C_type_function	*f = (C_type_function*)type;
			if (_typedef && (f->flags & C_type_function::VIRTUAL))
				sa << "virtual ";

			if (!(f->flags & C_type_function::NORETURN)) {
				DumpType1(sa, f->subtype, 0, indent);
				if (name)
					sa << ' ';
			}
			if (name)
				sa << name;
			sa << '(';
			for (auto *i = f->args.begin() + int(f->has_this()), *e = f->args.end(); i != e; ++i) {
				DumpType1(sa, i->type, i->id, indent);
				if (i != e - 1)
					sa << ", ";
			}
			sa << ')';
			if (_typedef && (f->flags & C_type_function::ABSTRACT))
				sa << "=0";
			return sa;
		}

		case C_type::TEMPLATE_PARAM:
			return sa << 'T' << int(type->flags & C_type::USER_FLAGS);

		case C_type::CUSTOM:
			sa << ((C_type_custom*)type)->id;
			break;
	}

	if (name)
		sa << ' ' << name;
	return sa;
}

string_accum &iso::DumpType(string_accum &sa, const C_type *type, string_param &&name, int indent, bool _typedef) {
	return DumpType1(sa, type, name, indent, _typedef);
}

template<typename F> string_accum&	DumpParams(F&& f, string_accum &a, const range<ref_ptr<ast::node>*> &args) {
	return a << parentheses() << comma_list(args, forward<F>(f));
}


bool Dumper::DumpCPP(string_accum &a, const ast::node *p, int precedence) {
	bool	statement = false;

	if (p->is_plain()) {
		auto			*i	= ((ast::plain_node*)p)->get_info();
		a << i->name;
		return i->statement;

	} else if (p->is_unary()) {
		ast::unary_node	*b	= (ast::unary_node*)p;

		if (b->kind == ast::deref && IsReference(b->arg->type)) {
			DumpCPP(a, b->arg);
			return false;

		} else {
			auto	*i	= b->get_info();
			a << i->name;

			if (i->parentheses == 2) {
				DumpType(a << '<', b->type, none, depth) << '>';

			} else if (b->kind == ast::deref || b->kind == ast::load) {
				if (b->type && b->type != b->arg->type->subtype())
					DumpType(a << "static_cast<", b->type, none, depth) << "*>";

			} else if (b->type && b->type != b->arg->type) {
				//DumpType(a << '(', b->type) << ')';
			}
			DumpCPP(a << parentheses(i->parentheses || i->precedence < precedence), b->arg, i->precedence);
			return i->statement;
		}

	} else if (p->is_binary()) {
		ast::binary_node	*b	= (ast::binary_node*)p;
		auto				*i	= b->get_info();

		switch (p->kind) {
			case ast::ldelem: {
				ast::binary_node	*b = (ast::binary_node*)p;
				DumpCPP(a, b->left);
				DumpCPP(a << square_brackets(), b->right);
				return false;
			}
			case ast::assignobj: {
				ast::binary_node	*b = (ast::binary_node*)p;
				DumpCPP(a, b->left);
				a << " = ";
				DumpCPP(a, b->right);
				return true;
			}
			case ast::copyobj: {
				ast::binary_node	*b = (ast::binary_node*)p;
				DumpCPP(a << "copyobj(", b->left);
				DumpCPP(a << ", ", b->right);
				a << ")";
				return true;
			}
			case ast::field: {
				ast::node	*left = b->left;
				if (left->kind == ast::ref) {
					left = ((ast::unary_node*)left)->arg;
				}
				if (is_this(left)) {
					DumpCPP(a, b->right, precedence);
				} else if (left->kind == ast::deref) {
					DumpCPP(a, ((ast::unary_node*)get(left))->arg);
					save(use_scope, false), DumpCPP(a << "->", b->right, precedence);
				} else {
					DumpCPP(a, left);
					save(use_scope, false), DumpCPP(a << ".", b->right, precedence);
				}
				return false;
			}

			default: {
				DumpCPP(a << parentheses(i->precedence < precedence), b->left, i->precedence),
					DumpCPP(i->spaces ? (a << ' ' << i->name << ' ') : (a << i->name), b->right, i->precedence);
				return i->statement;
			}
		}

	} else switch (p->kind) {
		case ast::literal: {
			ast::lit_node	*lit = (ast::lit_node*)p;
			if (lit->id)
				a << lit->id;
			else {
				if (lit->flags & ast::ADDRESS) {
					if (get_mem) {
						uint64	addr	= lit->v;
						size_t	size	= lit->type->size();
						malloc_block	temp(size);
						get_mem(addr, temp, size);
						DumpData(a, temp, lit->type);
					} else {
						DumpType(a << "*(", lit->type, none, depth) << "*)0x" << hex(lit->v);
					}
				} else if (lit->type->type == C_type::ARRAY) {
					if (get_mem) {
						uint64	addr	= lit->v;
						size_t	size	= lit->type->size();
						malloc_block	temp(size);
						get_mem(addr, temp, size);
						DumpData(a, temp, lit->type);
					} else {
						DumpType(a << "((", lit->type, "(&)", depth) << ")0x" << hex(lit->v) << ')';
					}
				} else {
					DumpData(a, &lit->v, lit->type);
				}
			}
			break;
		}

		case ast::name:
			if (demangle)
				demangle(a, ((ast::name_node*)p)->id);
			else
				a << ((ast::name_node*)p)->id;
			break;

		case ast::get_size:
			a << "sizeof(" << p->type << ')';
			break;

			//element
		case ast::element:
		case ast::var:
		case ast::func:
		case ast::vfunc: {
			const ast::element_node	*b = p->cast();
			if (use_scope && b->parent && b->parent != scope)
				a << b->parent->composite()->id << "::";
			a << b->element->id;
			break;
		}

		case ast::decl: {
			ast::element_node	*b = (ast::element_node*)p;
			DumpType(a, b->element->type, b->element->id, depth);
			return true;
		}
		case ast::jmp:
			a << "jump " << ((ast::element_node*)p)->element->id;
			return true;

		case ast::call: {
			ast::call_node	*c = (ast::call_node*)p;
			auto	DumpParams	= [this](string_accum &a, const range<ref_ptr<ast::node>*> &args) {
				return a << parentheses() << comma_list(args, [this](string_accum &a, ast::node *i) { DumpCPP(a, i); });
			};

			if (c->func->kind == ast::element) {
				ast::element_node	*en = (ast::element_node*)get(c->func);
				const C_element		*e	= en->element;

				if (c->flags & ast::ALLOC) {
					a << "new ";
					DumpType(a, en->parent, none, depth);
					DumpParams(a, make_rangec(c->args));
					break;
				}

				if (e->id[0] == '.') {
					if (e->id == ".ctor") {
						if (c->flags & ast::ALLOC) {
							DumpType(a << "new ", en->parent, none, depth);
							DumpParams(a, make_rangec(c->args));
						} else {
							ast::node	*dest = c->args[0];
							if (dest->kind == ast::ref) {
								DumpCPP(a, ((ast::unary_node*)dest)->arg);
								a << " = ";
							}
							DumpType(a, en->parent, none, depth);
							DumpParams(a, slice(c->args, 1));
						}
						break;
					}

				}
				if (((C_type_function*)e->type)->has_this()) {
					if (!is_this(c->args[0])) {
						if (c->args[0]->kind == ast::ref) {
							DumpCPP(a, c->args[0]->cast<ast::unary_node>()->arg);
							a << '.';
						} else {
							DumpCPP(a, c->args[0]);
							a << "->";
						}
					}
					save(use_scope, false),  DumpCPP(a, c->func);
					DumpParams(a, slice(c->args, 1));
					break;
				}
			}

			DumpCPP(a, c->func);
			DumpParams(a, make_rangec(c->args));
			break;
		}

		case ast::basic_block: {
			ast::basicblock	*bb = (ast::basicblock*)p;
			string	&name = named[bb];
			newline(a);
			if (name) {
				a << "goto " << name;
				return true;
			} else {
				if (/*true || */bb->shared()) {
					name = format_string("block%i", bb->id);
					a << name << ":";
				}
				for (auto i : bb->stmts) {
					newline(a);
					if (DumpCPP(a, i))
						a << ';';
				}
			}
			break;
		}

		case ast::branch: {
			ast::branch_node	*b = (ast::branch_node*)p;
			if (b->cond) {
				DumpCPP(a << "if " << parentheses(), b->cond);
				scoped(a << ' '), DumpCPP(a, b->dest1);
				scoped(a << " else"), DumpCPP(a, b->dest0);
			} else {
				DumpCPP(a, b->dest0);
			}
			break;
		}
		case ast::swtch: {
			ast::switch_node	*s = (ast::switch_node*)p;
			DumpCPP(a << "switch " << parentheses(), s->arg);
			auto			sa		= scoped(a << ' ');
			int				n		= s->targets.size32();
			const C_type	*type	= s->arg->type;
			for (int i = 0; i < n - 1; ++i) {
				if (s->targets[i]) {
					newline(a) << "case ";
					if (type)
						DumpData(a, &i, type);
					else
						a << i;
					scoped(a << ": "), DumpCPP(a, s->targets[i]);
				}
			}

			scoped(newline(a) << "default: "), DumpCPP(a, s->targets[n - 1]);
			break;
		}
		case ast::whileloop: {
			ast::while_node	*b = (ast::while_node*)p;
			DumpCPP(a << "while " << parentheses(), b->cond);
			scoped(a << ' '), DumpCPP(a, b->block);
			break;
		}

		case ast::forloop: {
			ast::for_node	*b = (ast::for_node*)p;
			a << "for (";
			if (b->init)
				DumpCPP(a, b->init);
			a << "; ";
			if (b->cond)
				DumpCPP(a, b->cond);
			a << "; ";
			if (b->update)
				DumpCPP(a, b->update);
			scoped(a << ") "), DumpCPP(a, b->block);
			break;
		}

		case ast::ifelse: {
			ast::ifelse_node	*b = (ast::ifelse_node*)p;
			DumpCPP(a << "if " << parentheses(), b->cond);
			scoped(a << ' '), DumpCPP(a, b->blockt);
			if (b->blockf)
				scoped(a << " else "), DumpCPP(a, b->blockf);
			break;
		}

		default:
			a << "<unknown>";
			break;
	}
	return false;
}

