#include "iso/iso_files.h"
#include "extra/text_stream.h"
#include "base/algorithm.h"

using namespace iso;

class LUAreader : public text_reader<reader_intf> {
public:
	enum TOKEN {
		TOK_EOF					= -1,
		TOK_PLUS				= '+',
		TOK_MINUS				= '-',
		TOK_ASTERIX				= '*',
		TOK_DIVIDE				= '/',
		TOK_PERCENT				= '%',
		TOK_CARET				= '^',
		TOK_HASH				= '#',
		TOK_AMPERSAND			= '&',
		TOK_TILDE				= '~',
		TOK_BAR					= '|',
		TOK_LT					= '<',
		TOK_GT					= '>',
		TOK_SET					= '=',
		TOK_OPEN_PARENTHESIS	= '(',
		TOK_CLOSE_PARENTHESIS	= ')',
		TOK_OPEN_BRACE			= '{',
		TOK_CLOSE_BRACE			= '}',
		TOK_OPEN_BRACKET		= '[',
		TOK_CLOSE_BRACKET		= ']',
		TOK_SEMICOLON			= ';',
		TOK_COLON				= ':',
		TOK_COMMA				= ',',
		TOK_DOT					= '.',

		TOK_IDENTIFIER			= 256,
		TOK_NUMBER,
		TOK_STRINGLITERAL,

		TOK_SHL,		//	<<
		TOK_SHR,		//	>>
		TOK_IDIV,		//	//
		TOK_EQ,			//	==
		TOK_NEQ,		//	~=
		TOK_LE,			//	<=
		TOK_GE,			//	>=
		TOK_LABEL,		//	::
		TOK_CONCAT,		//	..
		TOK_VARARG,		//	...

		TOK_KEYWORDS,
		TOK_AND	= TOK_KEYWORDS,
		TOK_BREAK,
		TOK_DO,
		TOK_ELSE,
		TOK_ELSEIF,
		TOK_END,
		TOK_FALSE,
		TOK_FOR,
		TOK_FUNCTION,
		TOK_GOTO,
		TOK_IF,
		TOK_IN,
		TOK_LOCAL,
		TOK_NIL,
		TOK_NOT,
		TOK_OR,
		TOK_REPEAT,
		TOK_RETURN,
		TOK_THEN,
		TOK_TRUE,
		TOK_UNTIL,
		TOK_WHILE,
	};
public:
	TOKEN		token;
	string		identifier;

	LUAreader(istream_ref _file) : text_reader<reader_intf>(_file)	{}
	LUAreader&	me()					{ return *this; }
	int			GetLineNumber()	const	{ return line_number; }
	TOKEN		GetToken();
	TOKEN		Next()					{ return token = GetToken(); }
};

LUAreader::TOKEN LUAreader::GetToken() {
	static const char* keywords[] = {
		"and",
		"break",
		"do",
		"else",
		"elseif",
		"end",
		"false",
		"for",
		"function",
		"goto",
		"if",
		"in",
		"local",
		"nil",
		"not",
		"or",
		"repeat",
		"return",
		"then",
		"true",
		"until",
		"while",
	};

	int	c;
	while ((c = skip_whitespace(*this)) == '-' && peekc() == '-')
		skip_chars(*this, ~char_set("\r\n"));

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
			identifier = read_token(*this, char_set::identifier, c);
			const char **k = find(keywords, identifier);
			return k == end(keywords)
				? TOK_IDENTIFIER
				: TOKEN(index_of(keywords, k) + TOK_KEYWORDS);
		}

		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			put_back(c);
			return TOK_NUMBER;

		case '\'':
		case '"':
			identifier = read_string(*this, c);
			return TOK_STRINGLITERAL;

		case '.':
			c = getc();
			if (c == '.') {
				c = getc();
				if (c == '.')
					return TOK_VARARG;
				put_back(c);
				return TOK_CONCAT;
			}
			put_back(c);
			return TOK_DOT;

		case '=':
			c = getc();
			if (c == '=')
				return TOK_EQ;
			put_back(c);
			return TOK_SET;

		case '<':
			c = getc();
			if (c == '=')
				return TOK_LE;
			put_back(c);
			return TOK_LT;

		case '>':
			c = getc();
			if (c == '=')
				return TOK_GE;
			put_back(c);
			return TOK_GT;

		case '~':
			c = getc();
			if (c == '=')
				return TOK_NEQ;
			put_back(c);
			return TOK_TILDE;

		case '/':
			c = getc();
			if (c == '/')
				return TOK_IDIV;
			put_back(c);
			return TOK_DIVIDE;

		case ':':
			c = getc();
			if (c == '/')
				return TOK_LABEL;
			put_back(c);
			return TOK_COLON;

		default:
			return (TOKEN)c;
	}
}


//-----------------------------------------------------------------------------
//	LUA
//-----------------------------------------------------------------------------

class LUAFileHandler : public FileHandler {
	ISO_ptr<void>	Read(LUAreader &lua);
	ISO_ptr<void>	ReadValue(tag id, LUAreader &lua);

	const char*		GetExt() override { return "lua"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		if (file.getc() == '-' && file.getc() == '-')
			return CHECK_PROBABLE;
		return CHECK_NO_OPINION;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		LUAreader	lua(file);
		ISO_ptr<anything>	p(id);
		try {
			lua.Next();
			while (lua.token != LUAreader::TOK_EOF) {
				p->Append(Read(lua));
			}
			return p;

		} catch (const char *error) {
			throw_accum(error << " at line " << lua.GetLineNumber());
			return ISO_NULL;
		}
	}
} lua;


ISO_ptr<void> LUAFileHandler::Read(LUAreader &lua) {
	switch (lua.token) {
		case LUAreader::TOK_IDENTIFIER: {
			string	name = lua.identifier;
			while (lua.Next() == '.') {
				lua.Next();
				name = name + "." + lua.identifier;
			}
			if (lua.token != '=')
				iso_throw("Expected =");

			lua.Next();
			auto p = ReadValue(name, lua);
			while (lua.Next() == LUAreader::TOK_OR) {
				lua.Next();
				auto q = ReadValue(name, lua);
				if (!p)
					p = q;
			}
			return p;
		}
		default:
			iso_throw("Expected identifier");
			break;
	}
}

ISO_ptr<void> LUAFileHandler::ReadValue(tag id, LUAreader &lua) {
	switch (lua.token) {
		case LUAreader::TOK_IDENTIFIER: {
			int	c;
			while ((c = skip_whitespace(lua)) == '.')
				lua.Next();
			lua.put_back(c);
			return ISO_NULL;
		}

		case LUAreader::TOK_OPEN_BRACE: {
			ISO_ptr<anything>	p(id);
			while (lua.Next() != LUAreader::TOK_CLOSE_BRACE) {
				string	name;
				switch (lua.token) {
					case LUAreader::TOK_IDENTIFIER:
						name = lua.identifier;
						break;
					case LUAreader::TOK_OPEN_BRACKET: {
						if (lua.Next() != LUAreader::TOK_NUMBER)
							iso_throw("Expected number");
						number	n = read_number(lua, 0);
						if (lua.Next() != ']')
							iso_throw("Expected ]");
						name = to_string((int64)n);
						break;
					}
				}

				if (name) {
					if (lua.Next() != '=')
						iso_throw("Expected =");
					lua.Next();
				}

				p->Append(ReadValue(name, lua));

				if (!is_any(lua.Next(), LUAreader::TOK_COMMA, LUAreader::TOK_SEMICOLON))
					break;
			}
			return p;
		}

		case LUAreader::TOK_TRUE:
		case LUAreader::TOK_FALSE:
			return ISO_ptr<bool>(id, lua.token == LUAreader::TOK_TRUE);

		case LUAreader::TOK_DOT:
		case LUAreader::TOK_PLUS:
		case LUAreader::TOK_MINUS:
		case LUAreader::TOK_NUMBER: {
			number	n = read_number(lua, lua.token == LUAreader::TOK_NUMBER ? 0 : (int)lua.token);
			if (!n.valid())
				iso_throw("Bad number");
			if (n.isfloat())
				return ISO_ptr<float64>(id, n);
			else
				return ISO_ptr<int64>(id, n);
		}

		case LUAreader::TOK_STRINGLITERAL:
			return ISO_ptr<string>(id, lua.identifier);

	}
	return ISO_NULL;
}
