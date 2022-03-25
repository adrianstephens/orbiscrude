#include "iso/iso_files.h"
#include "extra/text_stream.h"

using namespace iso;

struct user_void;
ISO_DEFUSER(user_void, void);

class ISO_c : public text_writer<writer_intf> {
	int			indent;
	bool		NewLine();
	void		PutName(const char *name);
public:
	void		Indent(int i)		{ indent += i; }
	void		DumpType(const ISO::Type *type, const char *name, bool _typedef = false);
	void		DumpType(const ISO::TypeUser *type, bool forward = false) {
		if (type->subtype->GetType() != ISO::COMPOSITE)
			puts("typedef ");
		if (forward)
			format("struct %s", type->id.get_tag());
		else
			DumpType(type->subtype, type->id.get_tag(), true);
		puts(";\n\n");
	}
	void		DumpISOdef(const ISO::TypeUser *type);

	ISO_c(ostream_ref _file) : text_writer<writer_intf>(_file), indent(0) {}
};

bool ISO_c::NewLine() {
	putc('\n');
	for (int i = 0; i < indent; i++)
		if (!putc('\t'))
			return false;
	return true;
}

void ISO_c::PutName(const char *name) {
	for (char c; c = *name++; ) {
		if (c == ' ')
			c = '_';
		if (is_alphanum(c) || c == '_')
			putc(c);
	}
}


void ISO_c::DumpType(const ISO::Type *type, const char *name, bool _typedef) {
	if (!type)
		puts("void");
	else switch (type->GetType()) {
		case ISO::INT: {
			ISO::TypeInt	*i = (ISO::TypeInt*)type;
			if (i->flags & ISO::TypeInt::ENUM) {
				puts("enum {");
				Indent(+1);
				ISO::Enum	*e	= (ISO::Enum*)(i + 1);
				for (int j = 0, v = 0; !e[j].id.blank(); j++, v++) {
					if (j)
						putc(',');
					NewLine();
					format("%s", (const char*)e[j].id.get_tag());
					if (e[j].value != v)
						format(" = %i", v = e[j].value);
				}
				Indent(-1);
				NewLine();
				putc('}');
			} else {
				if (!(i->flags & ISO::TypeInt::SIGN))
					puts("unsigned ");
				switch (i->num_bits()) {
					case 8:		puts("char");	break;
					case 16:	puts("short");	break;
					case 32:	puts("int");	break;
					case 64:	puts("_int64");	break;
				}
			}
			break;
		}
		case ISO::FLOAT:	{
			if (type == ISO::getdef<float>()) {
				puts("float");
			} else {
			}
			break;
		}
		case ISO::STRING:	puts("char*");	break;
		case ISO::VIRTUAL:	puts("void*");	break;
		case ISO::COMPOSITE: {
			ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)type;
			char				buffer[256];
			if (_typedef) {
				puts("struct ");
				PutName(name);
				puts(" {");
			} else {
				puts("struct {");
			}
			Indent(+1);
			int	u = 0;
			for (auto &e : comp) {
				NewLine();
				const char	*id = comp.GetID(&e).get_tag();
				if (!id) {
					sprintf(buffer, "_unnamed%i", u++);
					id = buffer;
				}
				DumpType(e.type, id);
				putc(';');
			}
			Indent(-1);
			NewLine();
			putc('}');
			if (_typedef)
				return;
			break;
		}
		case ISO::ARRAY:
			DumpType(((ISO::TypeArray*)type)->subtype, name);
			format("[%i]", ((ISO::TypeArray*)type)->count);
			return;
		case ISO::OPENARRAY:
//			DumpType(((ISO::TypeOpenArray*)type)->subtype, name);
//			puts("[]");
//			return;
		case ISO::REFERENCE: {
#if 1
			puts("iso::pointer32<");
			DumpType(((ISO::TypeReference*)type)->subtype, NULL);
			putc('>');
#else
			DumpType(((ISO::TypeReference*)type)->subtype, NULL);
			putc('*');
#endif
			break;
		}
		case ISO::USER:
			puts(((ISO::TypeUser*)type)->ID().get_tag());
			break;

		default:
			break;
	};

	if (name) {
		putc(' ');
		PutName(name);
	}
}

void ISO_c::DumpISOdef(const ISO::TypeUser *type) {
	if (type->subtype->GetType() == ISO::COMPOSITE) {
		ISO::TypeComposite	*comp = (ISO::TypeComposite*)type->subtype.get();
		int					count = comp->Count();
		format("ISO_DEFUSERCOMP(%s, %i) {\n", (const char*)type->ID().get_tag(), count);
		for (int i = 0; i < count; i++)
			format("\tISO_SETFIELD(%i, %s);\n", i, (const char*)comp->GetID(i).get_tag());
		puts("}};\n\n");
	} else {
		format("ISO_DEFUSER(%s, ", (const char*)type->ID().get_tag());
		DumpType(type->subtype, NULL);
		puts(");\n\n");
	}
}

//-----------------------------------------------------------------------------
//	C_tokeniser
//-----------------------------------------------------------------------------

namespace {
class C_tokeniser : public text_mode_reader<istream_ref> {
	enum TOKEN {
		TOK_EOF					= -1,

		TOK_MINUS				= '-',
		TOK_PLUS				= '+',
		TOK_DOT					= '.',
		TOK_COLON				= ':',
		TOK_SEMICOLON			= ';',
		TOK_NOT					= '~',
		TOK_EQUALS				= '=',
		TOK_LESS				= '<',
		TOK_GREATER				= '>',
		TOK_COMMA				= ',',
		TOK_ASTERIX				= '*',
		TOK_DIVIDE				= '/',
		TOK_AMPERSAND			= '&',
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

		TOK_ELLIPSIS,
		TOK_EQUIV,
		TOK_NEQ,
		TOK_LE,
		TOK_GE,
		TOK_SHIFTLEFT,
		TOK_SHIFTRIGHT,
		TOK_SCOPE,

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
	};
	TOKEN		token;
	string		identifier;

	int			Peek()					{ int c = skip_whitespace(*this); put_back(c); return c;	}

	ISO::TypeUser		*MakeUser(tag id, ISO::Type *type) {
		return new ISO::TypeUserSave(id, type, ISO::TypeUser::FROMFILE);
	}

public:
	C_tokeniser(istream_ref file, int line_number = 1) : text_mode_reader<istream_ref>(file, line_number) { Next(); }

	operator			TOKEN()			const	{ return token; }
	int					GetLineNumber()	const	{ return line_number; }

	TOKEN				GetToken();
	C_tokeniser&		Next()					{ token = GetToken(); return *this;	}
	void				Expect(char tok);
	bool				ReadFloat(float &value);
	bool				SkipHierarchy();
	int					EvaluateConstant(const ISO::Enum *en, int n);
	ISO::Type*			ReadNumericType();
	ISO::Type*			ReadType();
	ISO::Type*			ReadEmbellishedType(ISO::Type *type, char *name = NULL, bool allowunnamed = false, bool refisptr = true);
	void				Parse();
};

void C_tokeniser::Expect(char tok) {
	if (token != tok)
		throw_accum("Expected " << tok);
	Next();
}

C_tokeniser::TOKEN C_tokeniser::GetToken() {
	static const char* keywords[] = {
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
	};

	for (;;) {
		int	c	= skip_whitespace(*this);

		switch (c) {
			case '_':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z': {
				identifier = read_string(*this, c);
				for (int i = 0; i < num_elements(keywords); i++) {
					if (strcmp(identifier, keywords[i]) == 0)
						return TOKEN(_TOK_KEYWORDS + i);
				}
				return TOK_IDENTIFIER;
			}

			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				put_back(c);
				return TOK_NUMBER;

			case '\'':
				identifier = read_string(*this, c);
				return TOK_CHARLITERAL;

			case '.':
				c = getc();
				if (c == '.')
					return TOK_ELLIPSIS;
				put_back(c);
				return TOK_DOT;

			case '=':
				c = getc();
				if (c == '=')
					return TOK_EQUIV;
				put_back(c);
				return TOK_EQUALS;

			case '<':
				c = getc();
				if (c == '=')
					return TOK_LE;
				put_back(c);
				return TOK_LESS;

			case '>':
				c = getc();
				if (c == '=')
					return TOK_GE;
				put_back(c);
				return TOK_GREATER;

			case '!':
				c = getc();
				if (c == '=')
					return TOK_NEQ;
				put_back(c);
				return TOK_NOT;

			case '/':
				c = getc();
				if (c == '/') {
					do c = getc(); while (c != '\n' && c != EOF);
				} else if (c == '*') {
					c = getc();
					do {
						while (c != '*' && c != EOF) c = getc();
						c = getc();
					} while (c != '/' && c != EOF);
				} else {
					put_back(c);
					return TOK_DIVIDE;
				}
				continue;

			case '"':
				identifier = read_string(*this, c);
				return TOK_STRINGLITERAL;

			case '#':
				do c = getc(); while (c != '\n' && c != EOF);
				break;

			default:
				return (TOKEN)c;
		}
	}
}


bool C_tokeniser::SkipHierarchy() {
	int		stack[64], *sp = stack;
	bool	angles = token == TOK_LESS;

	do {
		switch (token) {
			case TOK_EOF:
				return false;

			case TOK_LESS:
				if (!angles)
					break;
			case TOK_OPEN_BRACE:
			case TOK_OPEN_BRACKET:
			case TOK_OPEN_PARENTHESIS:
				*sp++ = token;
				break;

			case TOK_GREATER:
				if (!angles)
					break;
				if (sp[-1] != TOK_LESS)
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
				read_number(*this, 0);
				break;

			default:
				break;
		}
		Next();
	} while (sp > stack);
	return true;
}

int C_tokeniser::EvaluateConstant(const ISO::Enum *en, int n) {
	Next();
	int	value = token == TOK_NUMBER ? read_integer(*this, 0) : 0;
	Next();
	return value;
}

ISO::Type *C_tokeniser::ReadNumericType() {
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
	} else if (token == TOK_UNSIGNED) {
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
		return ISO::getdef<float>();
	}

	if (token == TOK_DOUBLE) {
		Next();
		return ISO::getdef<double>();
	}

	if (token == TOK_CHAR) {
		if (flags & (SHORT | LONG | LONGLONG))
			throw("Syntax error");
		Next();
		return new ISO::TypeInt(8, 0, flags & SIGNED ? ISO::TypeInt::SIGN : ISO::TypeInt::NONE);
	}

	if (token == TOK_INT) {
		Next();
	} else if (!flags) {
		return NULL;
	}

	int	nbits = flags & SHORT ? 16 : flags & LONG ? 32 : flags & LONGLONG ? 64 : 32;

	if (nbits == 32 && !(flags & UNSIGNED))
		return ISO::getdef<int>();

	return new ISO::TypeInt(nbits, 0, flags & UNSIGNED ? ISO::TypeInt::NONE : ISO::TypeInt::SIGN);
}

ISO::Type *C_tokeniser::ReadType() {
	while (token == TOK_STATIC || token == TOK_CONST || token == TOK_VOLATILE || token == TOK_INLINE || token == TOK_EXTERN)
		Next();

	switch (token) {
		case TOK_STRUCT:
		case TOK_CLASS: {
			static_array<ISO::Element,64>	elements;
			bool		packed	= false;
			uint32		offset	= 0;
			char		name[64];
			tag			id;

			Next();
			if (token == TOK_IDENTIFIER) {
				id		= identifier;
				Next();

				if (token == TOK_COLON) {
					for (;;) {
						Next();
						if (token == TOK_PUBLIC || token == TOK_PRIVATE || token == TOK_PROTECTED)
							Next();

						if (token != TOK_IDENTIFIER)
							throw("Bad inheritance");

						if (ISO::Type *type = ISO::user_types.Find((const char*)identifier))
							offset	= elements.expand()->set(tag(), type, offset);

						Next();
						if (token == TOK_COMMA)
							continue;

					}
				}
			}

			if (token == TOK_OPEN_BRACE) {
				Next();
				while (token != TOK_CLOSE_BRACE) {

					if (token == TOK_PUBLIC || token == TOK_PRIVATE || token == TOK_PROTECTED) {
						Next();
						Expect(':');
						continue;
					}

					if (token == TOK_EXPLICIT) {
						Next();
						continue;

					} else if (token == TOK_FRIEND) {
						Next();
						ISO::Type *type = ReadType();

					} else if (token == TOK_TYPEDEF) {
						Next();
						ISO::Type *type = ReadType();
						if (!type)
							throw("Bad typedef");

						for (;;) {
							ISO::Type	*type2	= ReadEmbellishedType(type, name);

							if (token == TOK_OPEN_PARENTHESIS) {
								SkipHierarchy();	// don't add functions
							} else {
								MakeUser(name, type2);
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
							Expect('(');
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
							ISO::Type	*type = ReadType();
							if (!type)
								throw("Bad conversion operator");
							while (token == TOK_ASTERIX || token == TOK_AMPERSAND)
								Next();
							Expect('(');
							if (token != TOK_CLOSE_PARENTHESIS)
								throw("Conversion operator cannot have arguments");
							Next();
							if (token == TOK_OPEN_BRACE) {
								SkipHierarchy();
								continue;
							}
						} else {
							ISO::Type *type = ReadType();
							if (!type)
								throw("Bad struct");

							if (token == TOK_SEMICOLON) {
								if (TypeType(type) == ISO::COMPOSITE)
									offset	= elements.expand()->set(tag(), type, offset);

							} else {
								if (token == TOK_OPEN_PARENTHESIS) {	//probably just a macro - ignore it
									SkipHierarchy();
									continue;
								}
								if (token == TOK_OPERATOR) {
								}

								for (ISO::Type *type2 = ReadEmbellishedType(type, name);;) {
									if (!packed) {
										if (uint32 a = type->GetAlignment())
											offset = align(offset, a);
									}

									offset	= elements.expand()->set(tag(name), type2, offset);
									if (token != TOK_COMMA)
										break;

									Next();
									type2	= ReadEmbellishedType(type, name);
								}
							}
						}
					}

					Expect(';');
				}
				Next();
				int	n = int(elements.size());
				ISO::TypeComposite	*comp	= new(n) ISO::TypeComposite(n);
				for (int i = 0; i < n; i++)
					(*comp)[i] = elements[i];
				if (id)
					return MakeUser(id, comp);
				return comp;
			} else {
//				Expect(';');
				int	n = int(elements.size());
				ISO::TypeComposite	*comp	= new(n) ISO::TypeComposite(n);
				if (id)
					return MakeUser(id, comp);
				return comp;
			}
		}
/*
		case TOK_UNION: {
			Next();
			LX_type		*type	= NULL;
			LXT_user	*user	= NULL;
			char		name[64];

			if (token == TOK_IDENTIFIER) {
				user	= new LXT_user(identifier, NULL, &scope);
				Next();
			}

			if (token == TOK_OPEN_BRACE) {
				Next();
				while (token != TOK_CLOSE_BRACE) {
					LX_type *type1 = ReadType();
					if (!type1)
						throw("Bad union");

					if (token != TOK_SEMICOLON) {
						if (token == TOK_OPEN_PARENTHESIS) {	//probably just a macro - ignore it
							SkipHierarchy();
							continue;
						}
						LX_type	*type2 = ReadEmbellishedType(type1, name);
						if (!type)
							type = type2;
						else
							type = new LXT_union(type, type2);
					}
					Expect(';');
				}
			}
			Next();
			if (user) {
				user->subtype	= type;
				type			= user;
				scope.Add(user);
			}
			return type;
		}

*/
		case TOK_ENUM: {
			tag			id;
			Next();
			if (token == TOK_IDENTIFIER) {
				id		= identifier;
				Next();
			}

			if (token == TOK_OPEN_BRACE) {
				Next();
				dynamic_array<ISO::Enum>	a;
				int		value	= 0;

				for (;;) {
					if (token == TOK_CLOSE_BRACE)
						break;

					if (token != TOK_IDENTIFIER)
						throw("Bad enum");

					ISO::Enum	*b = a.expand();
					b->id		= tag(identifier);
					Next();

					if (token == TOK_EQUALS)
						value = EvaluateConstant(a, int(a.size()));
					b->value = value++;

					if (token != TOK_COMMA)
						break;
					Next();
				}
				Expect('}');

				uint32	n = uint32(a.size());
				ISO::TypeEnum *enm = new(n) ISO::TypeEnum(32);
				for (uint32 i = 0; i < n; i++)
					(*enm)[i] = a[i];
				//(*enm)[n].id = 0;
				if (id)
					return MakeUser(id, enm);
				return enm;
			}
		}

		case TOK_VOID:
			Next();
			return ISO::getdef<user_void>();

		case TOK_BOOL:
			Next();
			return ISO::getdef<bool8>();

		case TOK_TEMPLATE:
			Next();
			if (token != TOK_LESS)
				throw("Missing template arguments");
			SkipHierarchy();
			if (token == TOK_CLASS) {
				do Next(); while (token != TOK_OPEN_BRACE);
				SkipHierarchy();
				Expect(';');
			} else {
				do Next(); while (token != TOK_OPEN_PARENTHESIS);
				SkipHierarchy();
				if (token == TOK_CONST)
					Next();
				if (token == TOK_OPEN_BRACE)
					SkipHierarchy();
				else
					Expect(';');
			}
			return ReadType();

		case TOK_IDENTIFIER: {
			if (ISO::Type *type = ISO::user_types.Find((const char*)identifier)) {
				Next();
//				if (token != TOK_SCOPE)
					return type;
/*
				Next();
				if (token != TOK_IDENTIFIER)
					throw("Expected identifier after ::");
				entry = Find(identifier);
				if (entry && entry->EntryType() == LXE_USERTYPE) {
					Next();
					return (LXT_user*)entry;
				}
*/			}
			if (Peek() == '(') {
				Next();
				Next();
				ISO::Type	*type = ReadEmbellishedType(ReadType(), NULL, true);
				Expect(')');
				return type;
			} else if (Peek() == '<') {
				Next();
				if (strcmp(identifier, "ISO_ptr") == 0) {
					Next();
					ISO::Type	*type = ReadEmbellishedType(ReadType());
					Expect('>');
					type = new ISO::TypeReference(type);
					return type;
				} else if (strcmp(identifier, "ISO_openarray") == 0) {
					Next();
					ISO::Type	*type = ReadEmbellishedType(ReadType());
					Expect('>');
					type = new ISO::TypeOpenArray(type, type->GetSize());
					return type;
				}
			}

			while (Peek() == ':') {
				Next();
				if (token != TOK_SCOPE)
					throw("Expected ::");
				Next();
			}
			ISO::TypeUser	*user = MakeUser(identifier, NULL);
			Next();
			return user;
		}

		default:
			return ReadNumericType();
	}
}

bool C_tokeniser::ReadFloat(float &value) {
	if (is_any(token, TOK_NUMBER, TOK_MINUS, TOK_PLUS, TOK_DOT)) {
		number	n = read_number(*this, token == TOK_NUMBER ? 0 : (int)token);
		if (n.valid()) {
			Next();
			value = n;
		}
	}
	return false;
}

ISO::Type *C_tokeniser::ReadEmbellishedType(ISO::Type *type, char *name, bool allowunnamed, bool refisptr) {
	ISO::Type	*type2	= type;

	while (token == TOK_ASTERIX || token == TOK_AMPERSAND || token == TOK_CONST || token == TOK_VOLATILE) {
		if (token == TOK_ASTERIX || (token == TOK_AMPERSAND && refisptr)) {
			if (type2->GetType() == ISO::INT && type2->GetSize() == 1)
				type2 = ISO::getdef<string>();
			else
				type2 = new ISO::TypeReference(type2);
		}
		Next();
	}

	if (token == TOK_LESS)
		SkipHierarchy();

	if (token != TOK_IDENTIFIER) {
		if (token == TOK_OPEN_PARENTHESIS) {
			Next();
			while (token == TOK_ASTERIX || token == TOK_AMPERSAND)
				Next();
			type2 = ReadEmbellishedType(type2, name, true, refisptr);
			Expect(')');
		} else if (name) {
			name[0] = 0;
		} else if (!allowunnamed) {
			throw("Bad name");
		}
	} else if (name) {
		strcpy(name, identifier);
		Next();
	} else {
		throw("Bad name");
	}

	while (token == TOK_OPEN_BRACKET) {
		int	i = EvaluateConstant(NULL, 0);
		if (token != TOK_CLOSE_BRACKET)
			throw("Bad array size");
		Next();
		type2 = new ISO::TypeArray(type2, i, type2->GetSize());
	}

	return type2;
}

void C_tokeniser::Parse() {
	for (;;) {
		if (token == TOK_NAMESPACE) {
			Next();
			if (token == TOK_IDENTIFIER)
				Next();
			Expect(TOK_OPEN_BRACE);
			Parse();
			Expect(TOK_CLOSE_BRACE);

		} else if (token == TOK_TYPEDEF) {
			char			name[64];
			Next();
			ISO::Type		*type1	= ReadType();
			ISO::TypeUser	*user;
			for (;;) {
				ISO::Type	*type2	= ReadEmbellishedType(type1, name);
				user				= MakeUser(name, type2);
				if (token != TOK_COMMA)
					break;
				Next();
			}
			Expect(';');

		} else  if (token == TOK_EXTERN) {
			Next();
			if (token == TOK_STRINGLITERAL) {
				Next();
				if (token == TOK_OPEN_BRACE) {
					Next();
					Parse();
					Expect(TOK_CLOSE_BRACE);
					continue;
				}
			}

		} else if (token == TOK_CLOSE_BRACE || token == TOK_EOF) {
			return;

		}

		if (ISO::Type *type = ReadType()) {
			while (token != TOK_EOF && token != TOK_SEMICOLON) switch (token) {
				case TOK_ASTERIX:
				case TOK_AMPERSAND:
				case TOK_CONST:
				case TOK_VOLATILE:
				case TOK_LESS:
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
	};
}
}	// anonymous namespace

//-----------------------------------------------------------------------------
//	HFileHandler
//-----------------------------------------------------------------------------

class HFileHandler : public FileHandler {
	const char*		GetExt() override { return "h";	}
	const char*		GetDescription() override { return "C header file";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} h;

ISO_ptr<void> HFileHandler::Read(tag id, istream_ref file) {
	C_tokeniser	c(file);
	try {
		c.Parse();
	} catch (const char *error) {
		throw_accum(error << " at line " << c.GetLineNumber());
	}
	return ISO_ptr<int>(0);
//	return ISO_ptr<void>().Create(c.ReadType(), id);
}

bool HFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_c	c(file);
//	c.DumpType(p.GetType(), p.ID());
//	c.puts(";\n\n");
	int	isodefs = ISO::root("variables")["isodefs"].GetInt();
	for (auto &i : ISO::user_types) {
		if ((i->flags & ISO::TypeUser::FROMFILE) && i->subtype) {
			if (isodefs == 0 || isodefs == 2)
				c.DumpType(i);
			if (isodefs)
				c.DumpISOdef(i);
		}
	}
	return true;
}
//-----------------------------------------------------------------------------
//	CFileHandler
//-----------------------------------------------------------------------------

#include "filetypes/bin.h"

class CFileHandler : public FileHandler {
	const char*		GetExt() override { return "c";	}
	const char*		GetDescription() override { return "C file";	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} c;

bool CFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	memory_block	m = GetRawData(p);
	if (!m)
		return false;

	text_writer<writer_intf>	text(file);
	text << "const unsigned char " << p.ID() << "[] = {\n";

	uint8	*data = m;
	for (uint32 size = m.size32(), n; size; data += n, size -= n) {
		n = min(size, 16);
		for (int i = 0; i < n; i++)
			text << "0x" << hex(data[i]) << ", ";

		text << '\n';
	}

	text << "};\n";
	return true;
}

//-----------------------------------------------------------------------------
//	WriteCHeader
//-----------------------------------------------------------------------------

struct DependentTypes {
	enum STATE {
		NONE, STARTED, DUMPING, FORWARD, PROCESSING_FORWARD, DUMPED,
	};
	typedef const ISO::Type *type;
	typedef	dynamic_array<type>	dependencies[2];

	struct Depends {
		type				t;
		dependencies		d;
		STATE				state;
		Depends() : state(NONE)	{}
	};

	map<tag1, Depends>		types;

	void	Add(Depends &dep, type t, bool ref);
	bool	OrderedDump(ISO_c &c, Depends &dep, type t);

	Depends	&GetRecord(type t) {
		return types[t->GetType() == ISO::USER ? ((const ISO::TypeUser*)t)->id : tag1()];
	}
	void Add(type t) {
		Depends	&dep = GetRecord(t);
		if (dep.state == NONE) {
			dep.state	= STARTED;
			dep.t		= t;
			Add(dep, t->GetType() == ISO::USER ? t->SubType() : t, false);
		}
	}

	bool Dump(ISO_c &c) {
		for (auto &i : types) {
			if (!OrderedDump(c, i, i.t))
				return false;
		}
		return true;
	}
};

void DependentTypes::Add(Depends &dep, type t, bool ref) {
	switch (t->GetType()) {
		case ISO::COMPOSITE: {
			const ISO::TypeComposite *comp = (ISO::TypeComposite*)t;
			for (auto &i : *comp)
				Add(dep, i.type, ref);
			break;
		}
		case ISO::ARRAY:
		case ISO::OPENARRAY:
			Add(dep, t->SubType(), ref);
			break;

		case ISO::REFERENCE:
			Add(dep, t->SubType(), true);
			break;

		case ISO::USER: {
			const ISO::TypeUser	*user1	= (const ISO::TypeUser*)t;
			Depends				&dep1	=  types[user1->id];
			if (dep1.state == NONE) {
				dep1.state	= STARTED;
				dep1.t		= t;
				Add(dep1, user1->subtype, ref);
			}
			if (ref && t == dep.t)
				break;
			dep.d[ref].push_back(t);
			break;
		}
		default:
			break;
	}
}
bool DependentTypes::OrderedDump(ISO_c &c, Depends &dep, type t) {
	if (dep.state == DUMPED)
		return true;

	if (dep.state == STARTED)
		dep.state = DUMPING;
	else if (dep.state == FORWARD)
		dep.state = PROCESSING_FORWARD;
	else
		return false;

	for (auto i : dep.d[0]) {
		const ISO::TypeUser	*user1	= (const ISO::TypeUser*)i;
		Depends				&dep1	=  types[user1->id];
		if (!OrderedDump(c, dep1, i))
			return false;
	}

	for (auto i : dep.d[1]) {
		const ISO::TypeUser	*user1	= (const ISO::TypeUser*)i;
		Depends				&dep1	=  types[user1->id];
		if (!OrderedDump(c, dep1, i)) {
			if (dep1.state == DUMPING)
				c.DumpType(user1, true);

			if (dep1.state != FORWARD)
				return false;
		}
	}

	if (t->GetType() == ISO::USER) {
		const ISO::TypeUser	*user = (const ISO::TypeUser*)t;
		if (!user->id.get_tag().find('<'))
			c.DumpType(user);
	}
	dep.state = DUMPED;
	return true;
}

void WriteCHeader(const ISO::Type *type, ostream_ref file) {
	DependentTypes	deps;
	deps.Add(type);

	ISO_c	c(file);
	deps.Dump(c);
}

void WriteCHeader(const ISO::Type **types, uint32 num, ostream_ref file) {
	DependentTypes	deps;

	for (int i = num; i--;)
		deps.Add(*types++);

	ISO_c	c(file);
	deps.Dump(c);

	for (auto &i : deps.types) {
		if (i.t->GetType() == ISO::USER)
		c.DumpISOdef((const ISO::TypeUser*)i.t);
	}

}

void WriteCHeader(ISO_ptr<void> p, ostream_ref file) {
	ISO_c	c(file);
	if (p.IsType<anything>(ISO::MATCH_MATCHNULLS)) {
		ISO::Browser2	b(p);
		c.format("struct layout_%s {\n", (const char*)p.ID().get_tag());
		for (int i = 0, n = b.Count(); i < n; i++) {
			c.puts("\t");
			c.DumpType(b[i].GetTypeDef(), b.GetName(i).get_tag());
			c.puts(";\n");
		}
		c.puts("};\n");
	} else {
		c.DumpType(p.GetType(), p.ID().get_tag());
		c.puts(";\n\n");
	}
}
