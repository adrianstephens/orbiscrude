#ifndef ScriptH
#define ScriptH

#include "iso.h"
#include "stream.h"
#include "filename.h"
#include "extra/text_stream.h"

namespace ISO {

//-----------------------------------------------------------------------------
//	Tokeniser
//-----------------------------------------------------------------------------

class Tokeniser : public text_mode_reader<reader_intf> {
public:
	struct line {
		ptr<ptr_string<char,32>>	fn;
		int							ln;
		void set(ptr<ptr_string<char,32>> &_fn, int _ln) { fn = _fn; ln = _ln; }
	};
	typedef map<ptr<void>, line>	lines;
	static lines					linemap;

	static int GetLineNumber(const ptr<void> &p, const char **fn = 0);

	enum TOKEN {
		TOK_EOF					= -1,

		TOK_SEMICOLON			= ';',
		TOK_MINUS				= '-',
		TOK_PLUS				= '+',
		TOK_DOT					= '.',
		TOK_NOT					= '!',
		TOK_EQUALS				= '=',
		TOK_LESS				= '<',
		TOK_GREATER				= '>',
		TOK_COMMA				= ',',
		TOK_ASTERIX				= '*',
		TOK_DIVIDE				= '/',
		TOK_OPEN_BRACE			= '{',
		TOK_CLOSE_BRACE			= '}',
		TOK_OPEN_BRACKET		= '[',
		TOK_CLOSE_BRACKET		= ']',
		TOK_OPEN_PARENTHESIS	= '(',
		TOK_CLOSE_PARENTHESIS	= ')',

		TOK_IDENTIFIER			= 256,
		TOK_NUMBER,
		TOK_STRINGLITERAL,
		TOK_COMMENT,
		TOK_LINECOMMENT,

		TOK_ELLIPSIS,
		TOK_EQUIV,
		TOK_NEQ,
		TOK_LE,
		TOK_GE,
		TOK_SHL,
		TOK_SHR,
		TOK_OR,
		TOK_BITOR,
		TOK_AND,
		TOK_BITAND,

		TOK_KEYWORDS,
		TOK_NIL		= TOK_KEYWORDS,
		TOK_DEFINE,
		TOK_INT,
		TOK_FLOAT,
		TOK_STRUCT,
		TOK_PACKED,
		TOK_BITPACKED,
		TOK_STRING,
		TOK_EXTERNAL,
		TOK_FUNCTION,
		TOK_IF,
		TOK_ELSE,
		TOK_DO,
		TOK_WHILE,
		TOK_RETURN,
		TOK_YIELD,
		TOK_BREAK,
	};

	string				identifier;
	Tokeniser(istream_ref file, int _line_number = 1) : text_mode_reader<reader_intf>(file, _line_number) {}

	int					SkipWhitespace(bool newlines = true);

	int					GetLineNumber()	const	{ return line_number;		}
	const string&		Identifier()	const	{ return identifier;		}
	int					Peek()					{ int c = SkipWhitespace(); put_back(c); return c;	}
	TOKEN				GetToken(int c);
	bool				SwallowComments(TOKEN tok);
};
//-----------------------------------------------------------------------------
//	ScriptWriter
//-----------------------------------------------------------------------------

enum {
	SCRIPT_ONLYNAMES		= 1 << 0,
	SCRIPT_VIRTUALS		= 1 << 1,
	SCRIPT_DEFS			= 1 << 2,
};

class ScriptWriter : public text_mode_writer<writer_intf> {
	hash_set_with_key<void*>	names;
	uint32						flags;
	uint32						indent;
	uint32						maxdepth;

	bool		PrintString(const char *s, char delimiter, int len = -1);
	bool		NewLine();
	bool		Indent(int i) {
		if (maxdepth && indent + i > maxdepth)
			return false;
		indent += i;
		return true;
	}
	template<typename T> bool DumpEnums(const TypeEnumT<T> &e);
	template<typename T> bool DumpInt(const TypeInt *type, T v);

public:
	static bool	LegalLabel(const char *name);

	bool		DumpType(const Type *type);
	bool		DumpType(const Browser &b);
	bool		DumpType(const ptr_machine<void> &p);
	bool		DumpData(const Browser2 &_b);
	bool		DumpDefs(const Browser &_b);
	bool		PrintLabel(tag1 id);
	bool		PrintLabel(tag2 id);

	ScriptWriter	&SetFlags(uint32 s)		{ flags |=  s; return *this; }
	ScriptWriter	&ClearFlags(uint32 c)	{ flags &= ~c; return *this; }
	ScriptWriter	&SetMaxDepth(uint32 c)	{ maxdepth = c; return *this; }

	ScriptWriter(ostream_ref file, text_mode mode = text_mode::ASCII) : text_mode_writer<writer_intf>(file, mode), flags(0), indent(0) {}
	ScriptWriter(ostream_ref file, const Browser &b, text_mode mode = text_mode::ASCII) : text_mode_writer<writer_intf>(file, mode), flags(0), indent(0), maxdepth(0) {
		DumpType(b.GetTypeDef());
		putc(' ');
		DumpData(b);
	}
};

//-----------------------------------------------------------------------------
//	ScriptReader
//-----------------------------------------------------------------------------

enum {
	SCRIPT_KEEPEXTERNALS		= 1 << 0,
	SCRIPT_DONTCONVERT		= 1 << 1,
	SCRIPT_SKIPOLDFIELDS		= 1 << 2,
	SCRIPT_SKIPERRORS		= 1 << 3,
	SCRIPT_NOIMPLICITSTRUCT	= 1 << 4,
	SCRIPT_NOCHECKEND		= 1 << 5,
	SCRIPT_LINENUMBERS		= 1 << 6,
	SCRIPT_RELATIVEPATHS		= 1 << 7,
	SCRIPT_ALLOWBUILTINDIFF	= 1 << 8,
	SCRIPT_KEEPID			= 1 << 9,
};

uint32			ScriptFlags();
bool			ScriptRead(ptr<void> &p, tag id, const char *fn, istream_ref file, int flags, const char *prefix = "_");
bool			ScriptRead(const Browser &b, istream_ref file, int flags, const char *prefix = "_");

const Type*		ScriptReadType(istream_ref file);
const Type*		ScriptReadType(const char *s);
TypeUser*		ScriptReadType(tag id, istream_ref file);
bool			ScriptReadDefines(istream_ref file, UserTypeArray &types = user_types);
bool			ScriptReadDefines(const filename &fn, UserTypeArray &types = user_types);

inline ptr<void>	ScriptRead(tag id, const char *fn, istream_ref file, int flags, const char *prefix = "_") {
	ptr<void> p;
	ScriptRead(p, id, fn, file, flags, prefix);
	return p;
}

#ifdef USE_ISTREAM
inline ptr<void>	ScriptRead(tag id, const char *fn, istream_ref file, int flags, const char *prefix = "_") {
	return ScriptRead(id, fn, file, flags, prefix);
}
#endif

struct ScriptReadContext {
	Tokeniser *r;
	ScriptReadContext(istream_ref stream, int flags = 0);
	~ScriptReadContext();

	const char*		next_symbol();
};

struct TypeScript {
	const TypeUser	*type;
	TypeScript(tag id, const char *script);
	TypeScript(const TypeUser *_type) : type(_type) {}
	const Type*			operator&()	const { return type; }
	fixed_string<256>	write()		const;
};

}//namespace ISO

#endif// ScriptH

