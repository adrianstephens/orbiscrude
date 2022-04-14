#include "iso_script.h"
#include "iso_files.h"
#include "iso_convert.h"
#include "base/bits.h"
#include "base/algorithm.h"
#include "extra/expression.h"

namespace ISO {

//-----------------------------------------------------------------------------
//	ScriptWriter
//-----------------------------------------------------------------------------

bool ScriptWriter::NewLine() {
	putc('\n');
	for (int i = 0; i < indent; i++)
		if (!putc('\t'))
			return false;
	return true;
}

bool ScriptWriter::PrintString(const char *s, char delimiter, int len) {
	if (!putc(delimiter))
		return false;
		
	if (s) for (const char *e, *end = len < 0 ? s + strlen(s) : s + len; s < end; s = e) {
		for (e = s; e < end && e - s < 256 && (unsigned char)*e >= ' ' && *e != '\\' && *e != delimiter; e++);
		if (!format("%.*s", e - s, s))
			return false;
		if (e < end && e - s < 256) {
			char c = *e++;
			putc('\\');
			switch (c) {
				case '\r':	c = 'r';	break;
				case '\n':	c = 'n';	break;
				case '\t':	c = 't';	break;
				default:
					if (c < ' ')
						format("x%02x", (uint8)c);
					break;
			}
			if (c >= ' ')
				putc(c);
		}
	}
	return putc(delimiter);
}

bool ScriptWriter::LegalLabel(const char *name) {
	return name && (is_alpha(name[0]) || name[0] == '_') && string_check(name + 1, char_set::identifier);
}

bool ScriptWriter::PrintLabel(tag1 id) {
	const char *s = id.get_tag();
	return LegalLabel(s) ? puts(s) : PrintString(s, '\'');
}

bool ScriptWriter::PrintLabel(tag2 id) {
	if (const char *s = id.get_tag())
		return LegalLabel(s) ? puts(s) : PrintString(s, '\'');

	char	temp[256];
	string_getter<crc32>(id.get_crc32()).get(temp, 256);
	return puts(temp);
}

template<typename T> bool ScriptWriter::DumpEnums(const TypeEnumT<T> &e) {
	bool	crcids	= e.flags & e.CRCIDS;
	T		v		= 0;
	for (int j = 0; !e[j].id.blank(); j++, v++) {
		putc(' ');
		if (!PrintLabel(e[j].id.get_tag2(crcids)))
			return false;

		if (e[j].value != v && !format(" = %i", v = e[j].value))
			return false;
	}
	return true;
}

bool ScriptWriter::DumpType(const Type *type) {
	if (type) switch (type->GetType()) {
		case INT:
			if (type == getdef<int>()) {
				return puts("int");
			} else {
				TypeInt	*i = (TypeInt*)type;
				format("int{%s%i", i->flags & TypeInt::HEX ? "x" : i->flags & TypeInt::SIGN ? "" : "u", i->num_bits());
				if (i->frac_bits())
					format(".%i", i->frac_bits());
				if (i->flags & TypeInt::ENUM) {
					if (i->num_bits() > 32)
						DumpEnums(*(TypeEnumT<uint64>*)type);
					else
						DumpEnums(*(TypeEnumT<uint32>*)type);
				}
				return putc('}');
			}

		case FLOAT:
			return type == getdef<float>()
				? puts("float")
				: format("float{%s%i.%i}", type->flags & TypeFloat::SIGN ? "" : "u", type->param1, type->param2);

		case STRING:
			return puts(type->flags & TypeString::UTF16 ? "string16" : "string");

		case VIRTUAL:
			return /* !(flags & SCRIPT_VIRTUALS) &&*/ puts("virtual");

		case COMPOSITE:	{
			TypeComposite	&comp	= *(TypeComposite*)type;
			if (puts("struct {") && Indent(+1)) {
				for (auto &e : comp) {
					if (!NewLine() || !DumpType(e.type) || !putc(' ') || !(!e.id.blank() ? PrintLabel(comp.GetID(&e)) : putc('.')))
						break;
				}
				Indent(-1);
			}
			return NewLine() && putc('}');
		}
		case ARRAY:
			return DumpType(((TypeArray*)type)->subtype)
				&& format("[%i]", ((TypeArray*)type)->count);

		case OPENARRAY:
			return DumpType(((TypeOpenArray*)type)->subtype)
				&& puts("[]");

		case REFERENCE:
			return DumpType(((TypeReference*)type)->subtype)
				&& putc('*');

		case USER:
			return PrintLabel(((TypeUser*)type)->ID());

		default:
			break;
	};
	return true;
}

bool ScriptWriter::DumpType(const ptr_machine<void> &p) {
	const Type *type	= p.GetType();
	return DumpType((p.Flags() & Value::REDIRECT) && (type && type->GetType() == REFERENCE) ? type->SubType() : type);
}

bool ScriptWriter::DumpType(const Browser &b) {
	if (b.SkipUser().GetType() == REFERENCE) {
		ptr_machine<void> p = *b;
		if (p.Flags() & Value::REDIRECT)
			return DumpType(p);
	}
	return DumpType(b.GetTypeDef());
}

template<typename T> bool ScriptWriter::DumpInt(const TypeInt *type, T v) {
	typedef uint_for_t<T>	U;
	if (type->flags & TypeInt::ENUM) {
		TypeEnumT<U>	&e		= *(TypeEnumT<U>*)type;
		bool			crcids	= e.flags & e.CRCIDS;
		if (const EnumT<U> *i = e.biggest_factor(v)) {
			if (i->value == v)
				return PrintLabel(i->id.get_tag2(crcids));

			for (char sep = '{'; i; sep = ' ', i = e.biggest_factor(v)) {
				putc(sep);
				PrintLabel(i->id.get_tag2(crcids));
				T	mult = v / i->value;
				if (mult > 1)
					format("*%i", mult);
				v -= i->value * mult;
				if (v == 0)
					break;
			}
			if (v)
				format(" %i", v);
			return putc('}');
		}
	}
	buffer_accum<256>	a;
	if (type->flags & TypeInt::HEX) {
		a << "0x" << rightjustify(type->num_bits() / 4, '0') << hex(U(v));
	} else if (type->frac_bits()) {
		a << v / float(type->frac_factor());
	} else if (type->is_signed()) {
		a << v;
	} else {
		a << U(v);
	}
	return puts(a.begin(), a.length());
}

bool ScriptWriter::DumpData(const Browser2 &_b) {
	Browser2	b = _b.SkipUser();
	switch (b.GetType()) {
		case INT: {
			TypeInt	*type	= (TypeInt*)b.GetTypeDef();
			int		nb		= type->num_bits();
			if (nb > 64) {
				puts("0x");
				for (int i = nb / 8; i--; )
					format("%02x", ((uint8*)b)[i]);
				return true;
			}
			return nb > 32 ? DumpInt(type, type->get64(b)) : DumpInt(type, type->get(b));
		}
		case FLOAT:
			begin() << formatted(b.GetFloat(), FORMAT::SHORTEST);
			return ok;

		case STRING:
			if (const char *s = b.GetString()) {
				uint16	flags = b.GetTypeDef()->flags;
				if (flags & TypeString::UNESCAPED) {
					putc('"');
					if (flags & TypeString::UTF16)
						puts(string((char16*)s));
					else
						puts(s);
					return putc('"');
				}
				return PrintString(flags & TypeString::UTF16 ? string((char16*)s).begin() : s, '"');
			}
			return puts("nil");

		case VIRTUAL: {
			if ((flags & SCRIPT_IGNORE_DEFER) || !(b.GetTypeDef()->flags & Virtual::DEFER)) {
				while (Browser2 b2 = *b) {
					if (const char	*ext = b2.External())
						return format("external \"%s\"", ext);
						
					b = b2.SkipUser();
					if (b.GetType() != VIRTUAL)
						return DumpType(b.GetTypeDef()) && DumpData(b);
				}

				if (puts("virtual {") && Indent(+1)) {
					for (int i = 0, c = b.Count(); i < c; i++) {
						Browser2 b2	= b[i];
						if (!NewLine() || (b2.GetTypeDef() && (!DumpType(b2.GetTypeDef()) || !putc(' '))))
							break;
							
						if (tag2 id = b.GetName(i)) {
							if (!PrintLabel(id) || !puts(" = "))
								break;
						}
						const char	*ext	= b2.External();
						if (!(ext ? format("external \"%s\"", ext) : DumpData(b2)))
							break;
					}
					Indent(-1);
				}
				return NewLine() && putc('}');
			}
			return puts("virtual");
		}

		case COMPOSITE:
			if (putc('{') && Indent(+1)) {
				for (int i = 0, c = b.Count(); i < c; i++) {
					//if ((root || i) && !NewLine())
					if (i && !NewLine())
						return false;

					if (tag2 id = b.GetName(i)) {
						if (!PrintLabel(id) || !puts(" = "))
							return false;
					}
					if (!DumpData(b[i]))
						return false;
				}
				Indent(-1);
			}
			return NewLine() && putc('}');

		case ARRAY:
		case OPENARRAY: {
			int			count	= b.Count();
			const Type *etype	= b[0].GetTypeDef()->SkipUser();

			if (etype && etype->GetType() == INT && (etype->flags & TypeInt::CHR)) {
				const char *s = b[0];
				if (etype->GetSize() == 2)
					return PrintString(string((char16*)s, count), '"', -1);
				else
					return PrintString(s, '"', string_len32(s, s + count));
			}

			putc('{');
			if (etype) {
				int		numperline	= etype->GetType() == INT || etype->GetType() == FLOAT ? 8 : 1;
				bool	multi_line	= count > numperline;
				if (!multi_line || Indent(+1)) {
					for (int i = 0, left = 0; i < count; i++, left--) {
						if (left == 0) {
							if (multi_line && !NewLine())
								break;
							left = numperline;
						} else {
							putc(' ');
						}
						if (!DumpData(b[i]))
							break;
					}
					if (multi_line) {
						Indent(-1);
						return NewLine() && putc('}');
					}
				}
			}
			return puts(" }");
		}
		case REFERENCE:	{
			if (ptr_machine<void> p = *b) {
				if (p.ID()) {
					if ((flags & SCRIPT_ONLYNAMES) || names.check_insert(p))
						return PrintLabel(p.ID());
				} else {
					if (names.count(p))
						return puts(format_string("_%x", intptr_t(p.get())));
				}

				if (p.Flags() & Value::CRCTYPE) {
					if (!format("<unknown type %08x> ", (uint32&)p.Header()->type))
						return false;

//				} else if (p.GetType() && !p.GetType()->SameAs(((TypeReference*)b.GetTypeDef())->subtype, MATCH_NOUSERRECURSE)) {
				} else if (p.GetType() && !b.GetTypeDef()->SubType()->SameAs(p.GetType(), MATCH_NOUSERRECURSE)) {
					if (!DumpType(p) || !putc(' '))
						return false;
				}

				if (tag2 id = p.ID()) {
					if (!PrintLabel(id) || !puts(" = "))
						return false;
				}

				if (p.Flags() & Value::REDIRECT)
					return DumpData(Browser(*(ptr<void>*)p));

				return	p.IsExternal()					? format("external \"%s\"", (const char*)p)
					:	!(p.Flags() & Value::CRCTYPE)	? DumpData(Browser(p))
					:	true;
			}

			return puts("nil");
		}

		default:
			return true;
	}
}

void CollectDefs(Browser b, hash_set_with_key<void*> &names, dynamic_array<ptr<void> > &stack) {
	if (const Type *type = b.GetTypeDef()->SkipUser()) {
		if (type->GetType() == REFERENCE) {
			b = *b;
			ptr<void>	p = b;
			if (p && p.Header()->refs > 1 && !names.check_insert(p)) {
				stack.push_back(p);
				return;
			}
		}

		if (type->ContainsReferences()) {
			for (auto i : b)
				CollectDefs(i, names, stack);
		}
	}
}

bool ScriptWriter::DumpDefs(const Browser &b) {
	dynamic_array<ptr<void> >	stack;
	dynamic_array<ptr<void> >	defs;

	for (CollectDefs(b, names, stack); !stack.empty();) {
		auto	p	= stack.pop_back_value();
		defs.push_back(p);
		CollectDefs(Browser(p), names, stack);
	}

	for (auto &p : reversed(defs)) {
		puts("define ");
		if (p.ID())
			PrintLabel(p.ID());
		else
			puts(format_string("_%x", intptr_t(p.get())));
		puts(" = ");
		DumpType(p.GetType());
		putc(' ');
		if (!DumpData(p))
			return false;
		putc('\n');
	}
	return true;
}

//-----------------------------------------------------------------------------
//	Tokeniser
//-----------------------------------------------------------------------------

Tokeniser::lines	Tokeniser::linemap;

int Tokeniser::GetLineNumber(const ptr<void> &p, const char **fn) {
	if (!linemap.empty()) {
		if (lines::iterator i = find(linemap, p)) {
			if (fn)
				*fn = *i->fn;
			return i->ln;
		}
	}
	return 0;
}

int Tokeniser::SkipWhitespace(bool newlines) {
	int	c;
	do c = getc(); while (c == ' ' || c == '\t' || (newlines && (c == '\n' || c == '\r')));
	return c;
}

Tokeniser::TOKEN Tokeniser::GetToken(int c) {

	static const char* keywords[] = {
		"nil",
		"define",
		"int",
		"float",
		"struct",
		"packed",
		"bitpacked",
		"string",
		"external",
		"function",
		"if",
		"else",
		"do",
		"while",
		"return",
		"yield",
		"break",
	};

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
				: TOKEN(index_of(keywords, k) + TOK_NIL);
		}

		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			put_back(c);
			return TOK_NUMBER;

		case '\'':
			identifier = read_string(*this, c);
			return TOK_IDENTIFIER;

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
			else if (c == '<')
				return TOK_SHL;
			put_back(c);
			return TOK_LESS;

		case '>':
			c = getc();
			if (c == '=')
				return TOK_GE;
			else if (c == '>')
				return TOK_SHR;
			put_back(c);
			return TOK_GREATER;

		case '!':
			c = getc();
			if (c == '=')
				return TOK_NEQ;
			put_back(c);
			return TOK_NOT;

		case '|':
			c = getc();
			if (c == '|')
				return TOK_OR;
			put_back(c);
			return TOK_BITOR;

		case '&':
			c = getc();
			if (c == '&')
				return TOK_AND;
			put_back(c);
			return TOK_BITAND;

		case '/':
			if ((c = getc()) == '/')
				return TOK_LINECOMMENT;
			else if (c == '*')
				return TOK_COMMENT;
			put_back(c);
			return TOK_DIVIDE;

		case '"':
			identifier = read_string(*this, c);
			return TOK_STRINGLITERAL;

		default:
			return (TOKEN)c;
	}
}

bool Tokeniser::SwallowComments(TOKEN tok) {
	int	c;
	switch (tok) {
		case TOK_LINECOMMENT:
			do c = getc(); while (c != '\n' && c != '\r' && c != EOF);
			break;
			
		case TOK_COMMENT:
			c = getc();
			do {
				while (c != '*' && c != EOF) c = getc();
				c = getc();
			} while (c != '/' && c != EOF);
			break;
			
		default:
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
//	ScriptReader
//-----------------------------------------------------------------------------

struct hash_tag : tag1 {
	hash_tag(const tag &id) : tag1(id) {}
	hash_tag(const tag2 &id) : tag1(id.get_tag() ? tag1(id.get_tag()) : tag1(id.get_crc32())) {}
	friend uint32 hash(const hash_tag &h) { return h; }
};

class ScriptReader : public Tokeniser {

	hash_map<hash_tag, ptr_machine<void> >	names;

	int							flags;
	const char					*prefix;
	TOKEN						token;
	dynamic_array<ptr_machine<void> >	usings;
	ScriptReader				*first;
	filename					fn;
	streamptr					lasttell;
	bool						ifstack[16], *ifsp, inpp;
	tag							next_id;

	UserTypeArray				local_user_types;

	template<typename T> Type*	ParseEnumType(int nbits, int frac, int flags);
	template<typename T> bool	ParseEnumValue(const TypeEnumT<T> &e, void *data);

	char*			Tok2Str(TOKEN tok)const	{ static char s[2]; s[0] = tok; s[1] = 0; return s; }
	bool			KeepExternals()		const	{ return !!(flags & SCRIPT_KEEPEXTERNALS);	}
	bool			DontConvert()		const	{ return !!(flags & SCRIPT_DONTCONVERT);		}
	bool			RelativePaths()		const	{ return !!(flags & SCRIPT_RELATIVEPATHS);	}
	void			Init()						{ prefix = 0; first = this; ifsp = ifstack; inpp = false; }

public:
	ptr<ptr_string<char,32>>		pfn;

	ScriptReader(istream_ref _file, int _flags, int _line_number = 1) : Tokeniser(_file, _line_number), flags(_flags)									{ Init(); }
	ScriptReader(istream_ref _file, int _flags, const filename &_fn, int _line_number = 1) : Tokeniser(_file, _line_number), flags(_flags), fn(_fn)	{ fn.cleanup(); Init(); }
	~ScriptReader() { r.seek(lasttell); }

	ScriptReader&	SetPrefix(const char *s)	{ prefix = s; return *this; }
	ScriptReader&	Next()						{ lasttell = r.tell(); token = GetToken(); return *this; }
	operator		TOKEN()						{ return token; }
	operator		const char*()				{ return identifier; }

	void			Expect(TOKEN tok)	{
		if (token != tok)
			throw_accum("Expected " << Tok2Str(tok));
		Next();
	}

	TOKEN			GetToken();
	Browser2		GetField(const Browser2 &b);

	int				GetUnsignedInt() {
		if (token != TOK_NUMBER)
			iso_throw("Expected number");
		uint64	n = read_unsigned(*this, 0);
		Next();
		return (int)n;
	}
	int				GetInt() {
		if (!is_any(token, TOK_NUMBER, TOK_MINUS, TOK_PLUS))
			iso_throw("Expected number");
		int64	n = read_integer(*this, token == TOK_NUMBER ? 0 : (int)token);
		Next();
		return (int)n;
	}
	number			GetNumber()	{
		if (!is_any(token, TOK_NUMBER, TOK_MINUS, TOK_PLUS, TOK_DOT))
			iso_throw("Expected number");
		number	n = read_number(*this, token == TOK_NUMBER ? 0 : (int)token);
		if (!n.valid())
			iso_throw("Bad number");
		Next();
		return n;
	}
	Browser			CheckDefinition();
	ptr<void>		EvaluateExpression(const Type *type);
	bool			ParsePtr(const Type *type, void *data);
	bool			SkipHierarchy();
	bool			Parse(const Type *type, void *data);
	const Type*		ParseEmbellishments(const Type *type);
	const Type*		ParseType();
	const Type*		ParseType2();
	void			GetDefines();
	UserTypeArray&	GetUserTypes()				{ return local_user_types; }
	void			Add(ptr<void> p)			{ names[p.ID()] = p; }
};

Tokeniser::TOKEN ScriptReader::GetToken() {
	for (;;) {
		int	c = SkipWhitespace(!inpp);
		if (c != '#') {
			TOKEN tok = Tokeniser::GetToken(c);
			if (SwallowComments(tok))
				continue;
			return tok;
		}

		put_back(c);
		if (inpp)
			return (TOKEN)c;

		saver<bool>	was_inpp(inpp, true);
		for (int skip = 1, quote = 0, first = 1; skip; first = 0) {
			c		= getc();
			token	= (TOKEN)c;
			if (quote && c != EOF) {
				if (c == quote)
					quote = 0;
			} else switch (c) {
				case EOF:	iso_throw("Missing #endif"); break;
				case '"':	quote = c; break;
				case '\'':	quote = c; break;
				case '/':
					if ((c = getc()) == '/')
						SwallowComments(TOK_LINECOMMENT);
					else if (c == '*')
						SwallowComments(TOK_COMMENT);
					break;
				case '#':
					identifier = read_token(*this, char_set::identifier, c);

					if (identifier == "#ifdef") {
						if (first) {
							Next();
							if (*ifsp++ = !!CheckDefinition())
								skip = 0;
						} else {
							skip++;
						}
					} else if (identifier == "#ifndef") {
						if (first) {
							Next();
							if (*ifsp++ = !CheckDefinition())
								skip = 0;
						} else {
							skip++;
						}
					} else if (identifier == "#if") {
						if (first) {
							Next();
							if (*ifsp++ = Browser(EvaluateExpression(NULL)).GetInt() != 0)
								skip = 0;
						} else {
							skip++;
						}
					} else if (identifier == "#endif") {
						if (--skip == 0)
							ifsp--;
					} else if (skip == 1 && !ifsp[-1] && (
							identifier == "#else"
						|| (identifier == "#elif" && (Next(), Browser(EvaluateExpression(NULL)).GetInt()))
						)) {
						ifsp[-1] = true;
						skip = 0;
					}
					break;
			}
		}
		if (token != '#' && token != '\n' && token != '\r')
			return token;
	}
}

Browser ScriptReader::CheckDefinition() {
	Browser b;
	if (token == TOK_IDENTIFIER) {
		tag	id = identifier;
		b = root(id);
		if (!b) {
			b = MakeBrowser(names)[id];
			if (!b && first != this)
				b = MakeBrowser(first->names)[id];
			if (!b) {
				for (int i = 0; i < usings.size(); i++) {
					if (b = Browser(usings[i])[id])
						break;
				}
			}
		}
		Next();
		while (token == TOK_DOT) {
			Next();
			if (token == TOK_IDENTIFIER)
				b = b[identifier];
			Next();
		}
	}
	return b;
}

inline int sign(double a)	{ return a == 0 ? 0 : a < 0 ? -1 : 1; }

struct expression_context {
	struct value : ptr<void>, comparisons<value> {
		value()						{}
		value(const ptr<void> &p)	: ptr<void>(p) {}
		value(int64 i)				: ptr<void>(MakePtr(tag2(), i)) {}
		value(double f)				: ptr<void>(MakePtr(tag2(), f)) {}
		explicit value(bool b)		: ptr<void>(MakePtr(tag2(), int(b))) {}

		bool	is_float()					const	{ return GetType() && GetType()->GetType() == FLOAT; }
		bool	is_string()					const	{ return GetType() && GetType()->GetType() == STRING; }
		operator int64()					const	{ return !GetType() ? 0 : is_float() ? int64(((TypeFloat*)GetType())->get(*this)) : ((TypeInt*)GetType())->get64(*this); }
		operator double()					const	{ return is_float() ? ((TypeFloat*)GetType())->get(*this) : double(((TypeInt*)GetType())->get64(*this)); }
		operator const char*()				const	{ return is_string() ? (char*)GetType()->ReadPtr(*this) : (char*)0; }

		int64	operator!()					const	{ return is_float() ? !((TypeFloat*)GetType())->get(*this) : !((TypeInt*)GetType())->get64(*this); }
		value	operator+()					const	{ return *this; }
		value	operator-()					const	{ return is_float() ? value(-((TypeFloat*)GetType())->get(*this)) : value(-((TypeInt*)GetType())->get64(*this)); }
		bool	operator&&(int64 b)			const	{ return !!*this && !!b; }
		bool	operator||(int64 b)			const	{ return !!*this || !!b; }

		friend int		compare(const value &a, const value &b)	{
			return	a.is_string() && b.is_string()	? compare(str((const char*)a), str((const char*)b))
				:	a.is_float() || b.is_float()	? sign((double)a - (double)b)
				:	int((int64)a - (int64)b);
		}

		value	operator* (const value &b)	const	{ return is_float() || b.is_float() ? value(operator double() * (double)b) : value(operator int64() * (int64)b); }
		value	operator/ (const value &b)	const	{ return is_float() || b.is_float() ? value(operator double() / (double)b) : value(operator int64() / (int64)b); }
		value	operator% (const value &b)	const	{ return operator int64() % (int64)b; }
		value	operator+ (const value &b)	const	{ return is_float() || b.is_float() ? value(operator double() + (double)b) : value(operator int64() + (int64)b); }
		value	operator- (const value &b)	const	{ return is_float() || b.is_float() ? value(operator double() - (double)b) : value(operator int64() - (int64)b); }
//these needed by clang
		int64	operator~()					const	{ return ~operator int64(); }
		int64	operator<<(int64 b)			const	{ return operator int64() << b; }
		int64	operator>>(int64 b)			const	{ return operator int64() >> b; }
		int64	operator& (int64 b)			const	{ return operator int64() &  b; }
		int64	operator| (int64 b)			const	{ return operator int64() |  b; }
		int64	operator^ (int64 b)			const	{ return operator int64() ^  b; }
	};

	ScriptReader	&iso;
	const Type		*type;

	void	backup()	{}

	value	get_value(bool discard)	{
		if (iso == Tokeniser::TOK_IDENTIFIER) {
			if (iso.Identifier() == "defined") {
				iso.Next();
				iso.Expect(Tokeniser::TOK_OPEN_PARENTHESIS);
				Browser	b = iso.CheckDefinition();
				iso.Expect(Tokeniser::TOK_CLOSE_PARENTHESIS);
				return value(!!b);
			}
		}
//		if (skip_cur) {
//			SkipHierarchy();
//			return NULL;
//		}
		value	v;
		iso.ParsePtr(type, &v);
		return v;
	}

	expression::OP get_op() {
		expression::OP	op;
		switch ((int)(Tokeniser::TOKEN)iso) {
			case ')':					op = expression::OP_RPA; break;
			case '*':					op = expression::OP_MUL; break;
			case '/':					op = expression::OP_DIV; break;
			case '%':					op = expression::OP_MOD; break;
			case '+':					op = expression::OP_ADD; break;
			case '-':					op = expression::OP_SUB; break;
			case '^':					op = expression::OP_XOR; break;
			case '?':					op = expression::OP_QUE; break;
			case ':':					op = expression::OP_COL; break;
			case '<':					op = expression::OP_LT;	 break;
			case '>':					op = expression::OP_GT;	 break;
			case Tokeniser::TOK_SHL:	op = expression::OP_SL;	 break;
			case Tokeniser::TOK_SHR:	op = expression::OP_SR;	 break;
			case Tokeniser::TOK_EQUIV:	op = expression::OP_EQ;	 break;
			case Tokeniser::TOK_NEQ:	op = expression::OP_NE;	 break;
			case Tokeniser::TOK_LE:		op = expression::OP_LE;	 break;
			case Tokeniser::TOK_GE:		op = expression::OP_GE;	 break;
			case Tokeniser::TOK_OR:		op = expression::OP_ORO; break;
			case Tokeniser::TOK_AND:	op = expression::OP_ANA; break;
			case '(':					op = expression::OP_LPA; break;
			case '~':					op = expression::OP_COM; break;
//			case '+':					op = expression::OP_PLU; break;
//			case '-':					op = expression::OP_NEG; break;
			case '!':					op = expression::OP_NOT; break;
			default:					return expression::OP_END;
		}
		iso.Next();
		return op;
	}

	expression_context(ScriptReader &_iso, const Type *_type) : iso(_iso), type(_type) {}
};

ptr<void> ScriptReader::EvaluateExpression(const Type *type) {
	expression_context	ctx(*this, type);
	return ::expression::evaluate<expression_context::value>(ctx);
}

const Type *ScriptReader::ParseEmbellishments(const Type *type) {
	for (;;) {
		if (token == TOK_OPEN_BRACKET) {
			Next();
			if (token == TOK_CLOSE_BRACKET) {
				Next();
				type = new TypeOpenArray(type, type->GetSize(), iso::log2(type->GetAlignment()));
			} else {
				type = new TypeArray(type, GetUnsignedInt(), type->GetSize());
				Expect(TOKEN(']'));
			}
		} else if (token == TOKEN('*')) {
			Next();
			type = new TypeReference(type);
		} else if (token == TOK_FUNCTION) {
			Next();
			const Type *type2 = ParseType();
			type = new TypeFunction(type, type2);
		} else {
			return type;
		}
	}
}

template<typename T> Type *ScriptReader::ParseEnumType(int nbits, int frac, int flags) {
	TypeInt type0(nbits, frac, Type::FLAGS(flags));
	dynamic_array<EnumT<T> >	a;
	T	value = 0;
	while (token != TOK_CLOSE_BRACE) {
		if (token != TOK_IDENTIFIER)
			iso_throw("Expected identifier in enum");
		EnumT<T>	*b	= a.expand();
		b->id				= tag(identifier);
		Next();
		if (token == TOK_EQUALS) {
			Next();
			value = Browser(EvaluateExpression(&type0)).Get(T(0));
		}
		b->value = value++;
	}
	int	n = int(a.size());
	TypeEnumT<T> *type = new(n + 1) TypeEnumT<T>(nbits, Type::FLAGS(flags));
	for (int i = 0; i < n; i++)
		(*type)[i] = a[i];
	(*type)[n].id = tag1();
	return type;
}

const Type *ScriptReader::ParseType() {
	Type	*type = NULL;
	switch (token) {
		case TOK_INT: {
			Next();
			if (token == TOK_OPEN_BRACE) {
				int		flags	= TypeInt::SIGN;
				int		nbits	= 32, frac = 0;

				int	c	= SkipWhitespace();
				if (c == 'u' || c == 'U') {
					flags &= ~TypeInt::SIGN;
					c	= SkipWhitespace();
				} else if (c == 'x' || c == 'X') {
					flags |= TypeInt::HEX;
					c	= SkipWhitespace();
				}

				nbits	= (int)read_base(10, *this, c);
				c		= getc();

				if (c == TOK_DOT) {
					frac = (int)read_base(10, *this, 0);
					c = getc();
				}

				if (c == '}') {
					type = new TypeInt(nbits, frac, Type::FLAGS(flags));
				} else {
					put_back(c);
					Next();
					type = nbits > 32 ? ParseEnumType<uint64>(nbits, frac, flags) : ParseEnumType<uint32>(nbits, frac, flags);
				}
				Next();
			} else {
				type = getdef<int>();
			}
			break;
		}
		case TOK_FLOAT:
			Next();
			if (token == TOK_OPEN_BRACE) {
				int		flags	= TypeFloat::SIGN;
				int		nbits	= 32, exp = 0;

				int	c	= SkipWhitespace();
				if (c == 'u' || c == 'U') {
					flags &= ~TypeFloat::SIGN;
					c	= SkipWhitespace();
				}

				nbits	= (int)read_base(10, *this, c);

				if (peekc() == TOK_DOT) {
					getc();
					exp = (int)read_base(10, *this, 0);
				} else {
					exp = nbits < 32 ? 5 : max(iso::log2(uint32(nbits)) * 4 - 13, 8);//IEEE754 sizes
				}

				type = new TypeFloat(nbits, exp, Type::FLAGS(flags));
				Next();
				Expect(TOK_CLOSE_BRACE);

			} else {
				type = getdef<float>();
			}
			break;

		case TOK_STRING:
			Next();
			type = getdef<ptr_string<char,32>>();
			break;

		case TOK_BITPACKED:
		case TOK_PACKED:
		case TOK_STRUCT: {
			TOKEN	save = token;
			Next();
			if (token != TOK_OPEN_BRACE)
				iso_throw("Expected {");
			token = save;
		}
		fallthrough
		case TOK_OPEN_BRACE: {
			bool	bitpacked	= token == TOK_BITPACKED;
			bool	packed		= token == TOK_PACKED;
			void	*defaults	= NULL;
			TypeCompositeN<64>	builder(0);

			Next();
			while (token != TOK_CLOSE_BRACE) {
				const Type	*type	= ParseType2();
				const char	*id		= 0;
				if (token == TOK_IDENTIFIER)
					id = identifier;
				else if (token != TOK_DOT)
					iso_throw("Expected identifier");

				uint32	offset = builder.Add(type, id, packed);

				Next();
				if (defaults) {
					defaults = iso::realloc(defaults, builder.GetSize());
					memset((uint8*)defaults + offset, 0, type->GetSize());
				}

				if (token == TOK_EQUALS) {
					if (!defaults) {
						defaults = iso::malloc(builder.GetSize());
						memset(defaults, 0, builder.GetSize());
					}
					Next();
					Parse(type, (uint8*)defaults + offset);
				} else if (TypeType(type) == COMPOSITE) {
					if (const void *def = ((TypeComposite*)type)->Default()) {
						if (!defaults) {
							defaults = iso::malloc(builder.GetSize());
							memset(defaults, 0, builder.GetSize());
						}
						memcpy((uint8*)defaults + offset, def, type->GetSize());
					}
				}
			}
			Next();
			if (bitpacked) {
				int			n	= builder.Count();
				BitPacked	*c	= new(n) BitPacked(0);
				for (Element *i = builder.begin(), *e = i + n; i != e; ++i) {
					const Type *type	= i->type->SkipUser();
					uint32		bits;
					switch (TypeType(type)) {
						case INT:	bits = ((TypeInt*)type)->num_bits(); break;
						case FLOAT:	bits = ((TypeFloat*)type)->num_bits(); break;
						default:	bits = i->size * 8;
					}
					c->Add(i->type, i->id.get_tag(), bits);
				}
				c->param16 = (c->back().end() + 7) / 8;
				type	= c;
			} else {
				type	= builder.Duplicate(defaults);
			}
			iso::free(defaults);
			break;
		}
		case TOK_IDENTIFIER: {
			if ((type = local_user_types.Find(identifier)) || (type = user_types.Find((const char*)identifier))) {
//				ISO_TRACEF("parsing type:") << type << '\n';
				Next();
			} else
				return NULL;
			break;
		}
		default:
			break;
	}
	return ParseEmbellishments(type);
}

const Type* ScriptReader::ParseType2() {
	const Type	*type = ParseType();
	if (!type) {
		if (!(flags & SCRIPT_SKIPERRORS))
			iso_throw("Expected type");
		Next();
		type = ParseEmbellishments(type);
	}
	return type;
}

Browser2 ScriptReader::GetField(const Browser2 &b) {
	if (KeepExternals() && (b.External() || b.Is("Directory"))) {
		buffer_accum<512> s;
		int	state = 0;

		if (const char *p = b.External()) {
			s << p;
			state = strchr(p, ';') ? 1 : 0;
		} else {
			s << GetDirectoryPath(b);
			if (token == TOK_DOT) {
				Next();
				if (token == TOK_NUMBER)
					identifier = read_token(*this, char_set::identifier, 0);

				s << DIRECTORY_SEP;
				int	i = b.GetIndex(identifier);
				if (i >= 0)
					s << b.GetName(i);
				else
					s << identifier;
				Next();
			}
			state = 0;
		}

		for (;;) {
			if (token == TOK_DOT) {
				Next();
				if (token == TOK_NUMBER)
					identifier = read_token(*this, char_set::identifier, 0);
				s << ";.\\"[state];
				if (identifier.find('.'))
					s << '\'' << identifier << '\'';
				else
					s << identifier;
				Next();
				state = 1;

			} else if (token == TOK_OPEN_BRACKET) {
				Next();
				int i = Browser(EvaluateExpression(NULL)).GetInt();
				Expect(TOK_CLOSE_BRACKET);
				if (state == 0)
					s << ';';
				s.format("[%i]", i);
				state = 1;
			} else {
				break;
			}
		}

		ptr<void> p;
		p.CreateExternal(str(s));
		p.Header()->addref();
		return p;
	}

	bool		skip = b.IsPtr() && (b.GetPtr().Header()->flags & Value::CRCTYPE);
	if (skip & !(flags & SCRIPT_SKIPERRORS))
		iso_throw("can't browse unknown type");

	Browser2	b1 = skip ? Browser2() : b;
	for (;;) {
		if (token == TOK_DOT) {
			Next();
			if (token == TOK_NUMBER)
				identifier = read_token(*this, char_set::identifier, 0);
			if (!skip) {
				Browser2 b2 = b1[identifier];
				if (!b2) {
					Browser var = root("variables")[identifier];
					if ((!var || !(b2 = b1[var.GetString()])) && !(flags & SCRIPT_SKIPERRORS))
						throw_accum("No such field " << identifier);
				}
				b1 = b2;
			}
			Next();
		} else if (token == TOK_OPEN_BRACKET) {
			if (!skip) {
				Next();
				int	i =	Browser(EvaluateExpression(NULL)).GetInt();
				Expect(TOK_CLOSE_BRACKET);
				b1 = b1[i];
			}
		} else {
			return b1;
		}
	}
}

bool Assign(const Type *type, void *data, ptr_machine<void> p, bool dontconvert) {
	static const auto conv_flags = Conversion::ALLOW_EXTERNALS | Conversion::RECURSE | Conversion::CHECK_INSIDE | Conversion::FULL_CHECK;

	if (type) {
		if (type->GetType() == REFERENCE) {
			auto	ref = (TypeReference*)type;
			if (!dontconvert && !(p = Conversion::convert(p, ref->subtype, conv_flags | (type->Is64Bit() ? Conversion::NONE : Conversion::MEMORY32))))
				iso_throw("No such conversion");
			ref->set(data, p);
			return !!(p.Flags() & (Value::EXTERNAL | Value::HASEXTERNAL));
		}

		if (p.IsExternal()) {
			Browser2	b = FileHandler::ReadExternal(p);
			if (b.Is(type)) {
				memcpy(data, b, type->GetSize());
				int	r = _Duplicate(type, data, DUPF_DUPSTRINGS|DUPF_CHECKEXTERNALS);
				return !!(r & 2);
			}
			p = b;
		}

		if (ptr_machine<void> p2 = Conversion::convert(p, type, conv_flags)) {
			memcpy(data, p2, type->GetSize());
			_Duplicate(type, data, DUPF_DUPSTRINGS);
			return !!(p2.Flags() & (Value::EXTERNAL | Value::HASEXTERNAL));
		}

		if (!Assign(Browser(type, data), Browser(p), !dontconvert)) {
			iso_throw(" Can't assign external value to this type");
		}
		/*
		if (!Assign(Browser(type, data), p.IsExternal() ? FileHandler::ReadExternal(p) : Browser(p), !dontconvert)) {
			ptr<void> p2 = Conversion::convert(p, type, Conversion::ALLOW_EXTERNALS | Conversion::RECURSE);
			if (!p2)
				throw("Can't assign external value to this type");
			memcpy(data, p2, type->GetSize());
			_Duplicate(type, data);
		}*/
	}
	return false;
}

bool Assign2(const Type *type, void *data, Browser2 b, bool dontconvert) {
	if (b.GetType() == REFERENCE)
		return Assign(type, data, *(ptr<void>*)b, dontconvert);
		
	if (b.IsPtr())
		return Assign(type, data, b.GetPtr(), dontconvert);
		
	if (!Assign(Browser(type, data), b, !dontconvert))
		iso_throw("Can't assign external value to this type");
		
	return false;
}

bool ScriptReader::ParsePtr(const Type *type, void *data) {
	TypeReference	ref(type);
	int		line	= GetLineNumber();
	bool	ret		= Parse(&ref, data);
	if (flags & SCRIPT_LINENUMBERS)
		linemap[*(ptr<void>*)data].set(pfn, line);
	return ret;
}

bool ScriptReader::SkipHierarchy() {
	int stack[64], *sp = stack;
	switch (token) {
		case TOK_INT:
		case TOK_FLOAT:
		case TOK_STRING:
		case TOK_PACKED:
		case TOK_STRUCT:
		case TOK_ASTERIX:
		case TOK_IDENTIFIER:
			ParseType();
			fallthrough
		default:
			break;
	}
	switch (token) {
		case TOK_PLUS:
		case TOK_MINUS:
		case TOK_NUMBER:
			GetNumber();
			return true;
		case TOK_STRINGLITERAL:
			Next();
			return true;
		case TOK_IDENTIFIER:
			while (token == TOK_IDENTIFIER) {
				Next();
				if (token == TOK_OPEN_BRACKET)
					SkipHierarchy();
				if (token == TOK_DOT) {
					Next();
					if (token == TOK_NUMBER) {
						identifier = read_token(*this, char_set::identifier, 0);
						token = TOK_IDENTIFIER;
					}
				} else
					break;
			}
			return true;
		default: do {
			switch (token) {
				case TOK_OPEN_BRACE:
				case TOK_OPEN_PARENTHESIS:
				case TOK_OPEN_BRACKET:
					*sp++ = token;
					break;

				case TOK_CLOSE_BRACE:
					if (sp[-1] != TOK_OPEN_BRACE)
						return false;
					--sp;
					break;

				case TOK_CLOSE_PARENTHESIS:
					if (sp[-1] != TOK_OPEN_PARENTHESIS)
						return false;
					--sp;
					break;

				case TOK_CLOSE_BRACKET:
					if (sp[-1] != TOK_OPEN_BRACKET)
						return false;
					--sp;
					break;

				case TOK_NUMBER:
					GetNumber();
					continue;
				default:
					break;
			}
			Next();
		} while (sp > stack);
		return true;
	}
}

template<typename T> bool ScriptReader::ParseEnumValue(const TypeEnumT<T> &e, void *data) {
	if (token == TOK_OPEN_BRACE) {
		Next();
		int	value = 0;
		while (token != TOK_CLOSE_BRACE) {
			bool	found = false;
			if (token == TOK_NUMBER) {
				value += GetUnsignedInt();
				continue;
			}
			for (int j = 0; !e[j].id.blank(); j++) {
				if (e[j].id.get_tag() == tag(identifier)) {
					Next();
					int	mult = 1;
					if (token == TOK_ASTERIX) {
						Next();
						mult = GetInt();
					}
					value += e[j].value * mult;
					found = true;
					break;
				}
			}
			if (!found)
				iso_throw("Unknown enum value");
		}
		e.set(data, value);
		Next();
		return true;

	} else if (token == TOK_IDENTIFIER) {
		for (int j = 0; !e[j].id.blank(); j++) {
			if (e[j].id.get_tag() == tag(identifier)) {
				e.set(data, e[j].value);
				Next();
				return true;
			}
		}
	}
	return false;
}

bool ScriptReader::Parse(const Type *_type, void *data) {
	if (_type->GetType() == USER && (_type->flags & TypeUser::INITCALLBACK)) {
		const TypeUserCallback	*cb = (const TypeUserCallback*)_type;
		bool	ret = Parse(cb->subtype, data);
		cb->init(data, 0);
		return ret;
	}

	const Type *type = _type->SkipUser();

	switch (token) {
		case TOK_IDENTIFIER:
			if (!inpp && Peek() == '=') {
				ptr<void>	p;
				tag2		id(identifier);
				Next();
				Expect(TOK_EQUALS);

				bool	ext;
				if (type->GetType() != REFERENCE) {
					p = MakePtr(type, id);
					Add(p);
					ext = Parse(type, p);
					
				} else {
					const Type *subtype = ((TypeReference*)type)->subtype;
					if (subtype && subtype->GetType() == USER && !((TypeUser*)subtype)->subtype) {
						p	= MakePtr(subtype, id);
						Add(p);
						ext = false;
						
					} else {
						ext		= (save(next_id, id), Parse(type, &p));
						if (!p.ID()) {
							p.SetID(id);
							Add(p);
						} else if (p.ID() != id) {
							p = Duplicate(id, p);
							Add(p);
						}
						if (next_id) {
							p = Duplicate(next_id, p);
							next_id = 0;
							Add(p);
						}
//						if (p.ID() != id && p.Header()->refs > 0) {
//							p = Duplicate(id, p);
//							Add(p);
//						} else {
//							p.SetID(id);
//						}
					}
				}
				if (prefix && id.get_tag().begins(prefix))
					p.SetFlags(Value::ALWAYSMERGE);
					
				Assign(type, data, p, DontConvert());
				return ext;
				
			} else {
				tag	id = (const char*)identifier;
				if (id == "true") {
					Next();
					bool8	b = true;
					return Assign(Browser(type, data), MakeBrowser(b));
				} else if (id == "false") {
					Next();
					bool8	b = false;
					return Assign(Browser(type, data), MakeBrowser(b));
				}
				
				Browser2 b = root(id);
				if (!b) {
#if 1
					auto	t1 = names[id];
					if (t1.exists())
						b = *t1;//MakeBrowser(*t1);
					if (!b && first != this) {
						auto t2 = first->names[id];
						if (t2.exists())
							b = *t2;//MakeBrowser(*t2);
					}
#else
					b = MakeBrowser(names)[id];
					if (!b && first != this) {
						b = MakeBrowser(first->names)[id];
					}
#endif
					if (!b) {
						for (int i = 0; i < usings.size(); i++) {
							if (b = Browser(usings[i])[id])
								break;
						}
					}
				}
				if (b) {
					Next();
					return (b = GetField(b)) && Assign2(type, data, b, DontConvert());
				}
/*
				if (root.Check(id)) {
					Next();
					return Assign2(type, data, GetField(root(id)), DontConvert());
				} else {
					if (Browser2 b = MakeBrowser(names)[id]) {
						Next();
						return Assign2(type, data, GetField(b), DontConvert());
					}
					if (first != this) {
						if (Browser2 b = MakeBrowser(first->names)[id]) {
							Next();
							return Assign2(type, data, GetField(b), DontConvert());
						}
					}
				}
				*/
				if (const Type *type2 = ParseType()) {
					if (type2 == type || type2 == _type)
						return Parse(type2, data);

					ptr<void>	p;
					int			save_flags			= flags;
					ptr<void>	save_keepexternals	= root("variables")["keepexternals"];

					if (!(flags & SCRIPT_KEEPEXTERNALS) && type2->GetType() == USER && ((TypeUser*)type2)->KeepExternals()) {
						root("variables").SetMember("keepexternals", 1);
						flags |= SCRIPT_KEEPEXTERNALS;
					}

					bool	ext = ParsePtr(type2, &p);
					if (type2->SameAs(p.GetType(), MATCH_NOUSERRECURSE_RHS)) {
						p.Header()->type = type2;

					} else if (type2->SameAs(p.GetType(), MATCH_NOUSERRECURSE_RHS | MATCH_IGNORE_SIZE)) {
						ptr<void> p2 = MakePtr(type2, p.ID());
						ISO_VERIFY(Conversion::batch_convert(p, 0, p.GetType(), p2, 0, type2, 1));
						p = p2;

					} else if ((type->GetType() != REFERENCE || !type2->SameAs(((TypeReference*)type)->subtype)) && (DontConvert() || ext)) {
						p = MakePtrIndirect(p, SkipUserType(type2) == REFERENCE ? type2 : new TypeReference(type2));
					}
					
					if (ext)
						p.SetFlags(Value::HASEXTERNAL);

					if (flags != save_flags) {
						if (!DontConvert())
							p = Conversion::convert(p, ((TypeReference*)type)->subtype, Conversion::ALLOW_EXTERNALS | Conversion::RECURSE);
						flags	= save_flags;
						root("variables").SetMember("keepexternals", save_keepexternals);
						p = FileHandler::ExpandExternals(p);
					}

					Assign(type, data, p, DontConvert());
					return ext;
				}
			}
			break;

		case TOK_EXTERNAL: {
			Next();
			if (token == TOK_IDENTIFIER) {
				if (type->GetType() != REFERENCE)
					iso_throw("external must be a reference type");

				auto	ref		= (TypeReference*)type;
				auto	subtype	= ref->subtype;
				ptr_machine<void>	p;
				if (SkipUserType(subtype) == REFERENCE) {
					p	= MakePtr(subtype);
					ref->set(data, p);
					p	= ((TypeReference*)subtype->SkipUser())->get(p);
					p.CreateExternal(identifier);
				} else {
					p 	= MakePtrExternal(subtype, identifier);
					ref->set(data, p);
				}
				p.SetFlags(Value::EXTREF);
				Next();
				return true;
			}

			if (token != TOK_STRINGLITERAL || !identifier)
				iso_throw("Missing filename");

			filename	extfn;
			FileHandler::ExpandVars(identifier, extfn);
			extfn	= extfn.relative_to(fn);

			if (KeepExternals() && type->GetType() == REFERENCE) {
				auto	ref		= (TypeReference*)type;
				auto	subtype = ref->subtype;
				if (SkipUserType(subtype) == REFERENCE) {
					auto	p	= MakePtr(subtype);
					ref->set(data, p);
					((TypeReference*)subtype->SkipUser())->get(p).CreateExternal(extfn);
				} else {
					ref->set(data, MakePtrExternal(subtype, extfn));
				}
				Next();
				return true;
			}
			
			Assign2(type, data, FileHandler::ReadExternal(extfn), DontConvert());
			Next();
			return false;
		}

		case TOK_STRINGLITERAL: {
			ptr<ptr_string<char,32>>	s = ptr<ptr_string<char,32>>(0);
			const Type	*type2	= SkipUser(type);
			while (TypeType(type2) == REFERENCE)
				type2	= SkipUser(type2->SubType());
				
			if (TypeType(type2) == STRING && (type2->flags & TypeString::UNESCAPED))
				*s = identifier.begin();
			else
				*s = unescape(identifier);
			Next();
			Assign(_type, data, s, DontConvert());
			return false;
		}

		case TOK_NUMBER: case TOK_PLUS: case TOK_MINUS: case TOK_DOT: {
			number	n = GetNumber();
			if (TypeType(type) == INT) {
				((TypeInt*)type)->set(data, n);

			} else if (TypeType(type) == FLOAT) {
				((TypeFloat*)type)->set(data, n);

			} else {
				if (TypeType(type) == REFERENCE) {
					if (const Type *subtype = ((TypeReference*)type)->subtype) {
						if (const Type *subtype2 = subtype->SkipUser()) {
							if (TypeType(subtype2) == INT) {
								((TypeInt*)subtype2)->set(*(ptr<void>*)data = MakePtr(subtype), n);
								return false;
								
							} else if (TypeType(subtype2) == FLOAT) {
								((TypeFloat*)subtype2)->set(*(ptr<void>*)data = MakePtr(subtype), n);
								return false;
							}
						}
					}
				}
				if (n.type == number::FLT)
					Assign(type, data, ptr<float>(0, n), DontConvert());
				else
					Assign(type, data, ptr<int>(0, int(int64(n))), DontConvert());
			}
			return false;
		}

		case TOK_OPEN_BRACE:
			if (!type->SameAs<ptr<void> >(MATCH_IGNORE_SIZE))
				break;
			type = getdef<ptr<anything> >();
			break;
//			if (!type->SameAs<void*>() || (flags & SCRIPT_NOIMPLICITSTRUCT))
//				break;
		case TOK_INT:
		case TOK_FLOAT:
		case TOK_STRING:
		case TOK_PACKED:
		case TOK_STRUCT:
		case TOK_ASTERIX:
			if (const Type *type2 = ParseType()) {
				if (type->SameAs(type2, MATCH_MATCHNULLS))
					return Parse(type2, data);

				if (type->GetType() == REFERENCE && ((TypeReference*)type)->subtype->SameAs(type2, MATCH_MATCHNULLS)) {
					tag	id;
					if (token == TOK_IDENTIFIER && Peek() == '=') {
						id = identifier;
						Next();
						Expect(TOK_EQUALS);
					}
					auto	ref = (TypeReference*)type;
					auto	p 	= MakePtr(type2, id);
					ref->set(data, p);
					bool	ext = Parse(type2, p);
					if (prefix && tag(id).begins(prefix))
						p.SetFlags(Value::ALWAYSMERGE);
					Add(p);
					return ext;
				}
				// note 32-bit ptrs
				ptr<void>	p;
				bool	ext = type2->GetType() == REFERENCE ? Parse(type2, p = MakePtr(type2)) : ParsePtr(type2, &p);
				Assign(type, data, p, DontConvert());
				return ext;
			}
			break;

		case TOK_OPEN_PARENTHESIS:
			Next();
			Assign(type, data, EvaluateExpression(0), DontConvert());
			return false;
		default:
			break;
	}

	switch (type->GetType()) {
		case INT: {
			TypeInt	*i = (TypeInt*)type;
			if (i->flags & TypeInt::ENUM) {
				if (i->num_bits() > 32 ? ParseEnumValue(*(TypeEnumT<uint64>*)type, data) : ParseEnumValue(*(TypeEnumT<uint32>*)type, data))
					return false;
			}
			i->set(data, GetNumber());
			return false;
		}

		case FLOAT:
			((TypeFloat*)type)->set(data, GetNumber());
			return false;

		case STRING:
			if (token == TOK_STRINGLITERAL)
				type->WritePtr(data, unescape(identifier));
			else if (token == TOK_NIL)
				type->WritePtr(data, 0);
			else
				iso_throw("Bad string");
			Next();
			return false;

		case COMPOSITE: {
			Expect(TOK_OPEN_BRACE);
			TypeComposite	&comp = *(TypeComposite*)type;
			if (const void *def = comp.Default()) {
				memcpy((char*)data, def, comp.GetSize());
				_Duplicate(type, data);
			}
			int		i	= 0;
			bool	ext = false;
			while (token != TOK_CLOSE_BRACE) {
				if (token == TOK_IDENTIFIER && Peek() == '=') {
					bool	found = false;
					for (int j = 0; j < comp.count; j++) {
						if (comp[j].id.get_tag() == identifier) {
							Next();
							Expect(TOKEN('='));
							i = j;
							found = true;
							break;
						}
					}
					if (!found && (flags & SCRIPT_SKIPOLDFIELDS)) {
						Next();
						Expect(TOKEN('='));
						SkipHierarchy();
						continue;
					}
				}
				if (i >= comp.count)
					iso_throw("Expected }");
				if (Parse(comp[i].type, (char*)data + comp[i].offset))
					ext = true;
				i++;
			}
			Next();
			return ext;
		}

		case ARRAY: {
			if (token != TOK_OPEN_BRACE && (flags & SCRIPT_SKIPERRORS)) {
				SkipHierarchy();
				return false;
			}
			Expect(TOK_OPEN_BRACE);
			TypeArray	&array	= *(TypeArray*)type;
			bool			ext		= false;
			for (int i = 0, n = array.count; i < n && token != TOK_CLOSE_BRACE; i++) {
				if (Parse(array.subtype, (char*)data + array.subsize * i))
					ext = true;
			}
			Expect(TOK_CLOSE_BRACE);
			return ext;
		}

		case OPENARRAY:
			*(void_ptr32*)data	= NULL;

			if (token == TOK_NIL) {
				Next();
				return false;

			} else {
				Expect(TOK_OPEN_BRACE);

				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				bool	ext		= false;
				int		count	= 0, size = 0;
				void	*p		= NULL;

				while (token != TOK_CLOSE_BRACE) {
					if (array.subsize && count >= size) {
						size	= count ? count * 2 : 16;
						p		= iso::realloc(p, size * array.subsize);
						memset((char*)p + array.subsize * count, 0, array.subsize * (size - count));
					}
					if (Parse(array.subtype, (char*)p + array.subsize * count))
						ext = true;
					count++;
				}
				Next();

				if (count) {
					OpenArrayHead	*h = type->Is64Bit()
						? OpenArrayAlloc<32>::Create(array.subsize, array.SubAlignment(), count, count)
						: OpenArrayAlloc<32>::Create(array.subsize, array.SubAlignment(), count, count);
					memcpy(GetData(h), p, array.subsize * count);
					type->WritePtr(data, GetData(h));
					iso::free(p);
				}
				return ext;
			}

		case REFERENCE:
			type->WritePtr(data, 0);
			if (token == TOK_NIL) {
				Next();

			} else {
				auto	ref 	= (TypeReference*)type;
				auto	reftype	= ref->subtype;
				ptr<void>	&p	= *(ptr<void>*)data;

				if (!reftype && (!(flags & SCRIPT_SKIPERRORS) || !(flags & SCRIPT_NOIMPLICITSTRUCT))) {
					if (flags & SCRIPT_SKIPERRORS) {
						if (token == TOK_IDENTIFIER && !MakeBrowser(names)[identifier]) {
							ptr<void>	p;
							Next();
							return (save(flags, flags | SCRIPT_NOIMPLICITSTRUCT), ParsePtr(ParseEmbellishments(reftype), &p));
						}
						do
							Next();
						while (token == TOK_IDENTIFIER && !MakeBrowser(names)[identifier]);
						SkipHierarchy();
						return false;
					}
					if (token == TOK_IDENTIFIER)
						throw_accum("Can't find " << identifier);
					iso_throw("Syntax");
				}
				p = MakePtr(reftype, next_id);
				if (next_id) {
					next_id = 0;
					Add(p);
				}
				if (Parse(reftype, p)) {
					p.SetFlags(Value::HASEXTERNAL);
					return true;
				}
			}
			return false;

		case FUNCTION:
			Expect(TOK_OPEN_BRACE);
			Expect(TOK_CLOSE_BRACE);
			return false;

		default:
			if (flags & SCRIPT_SKIPERRORS) {
				while (token == TOK_IDENTIFIER && !MakeBrowser(names)[identifier])
					Next();
				SkipHierarchy();
			}
	}
	return false;
}

void ScriptReader::GetDefines() {
	Next();
	while (token == TOK_DEFINE) {
		Next();
		if (token == TOK_STRINGLITERAL) {
			filename	dir(identifier);
			Next();
			tag	id	= identifier;
//			root.Add(GetDirectory(id, dir));
			first->Add(GetDirectory(id, fn.dir().relative(dir)));
			Next();

		} else if (token == TOK_EXTERNAL) {
			Next();
			if (token != TOK_STRINGLITERAL)
				iso_throw("Missing filename");
			filename		fnh = FileHandler::FindAbsolute(identifier);
			if (fnh.blank()) {
				if (flags & SCRIPT_SKIPERRORS) {
					Next();
					continue;
				}
				throw_accum("Cannot include " << identifier);
			}
			FileHandler::Include	inc(fnh);
			FileHandler::AddToCache(fnh, ISO_NULL);

			FileInput			file(fnh);
			ScriptReader	include(file, flags, fnh);
			include.Add(GetDirectory("local", fnh.dir()));
			include.Add(ptr<string>("filename", (const char*)fnh));
			include.Add(MakePtr("includer", &names));

			include.first = first;
			try {
				include.GetDefines();
				if (include.token != EOF)
					iso_throw("extra text past last define");
			} catch (const char *error) {
				throw_accum(error << " at line " << include.GetLineNumber() << " in " << identifier);
			}
			Next();

		} else if (token == TOK_IDENTIFIER && Peek() == TOK_DOT) {
			string		id		= identifier;
			bool		vars	= id == "variables";
			Browser2	b		= root().Check(id) ? root() : MakeBrowser(names);
			for (;;) {
				Next();
				if (!b.Check(id)) {
					if (!b.Is<anything>())
						iso_throw("Can't create a value");
						
					anything	*a = b;
					if (token == TOK_EQUALS) {
						ptr<void>	p;
						Next();
						Parse(getdef<ptr<void> >(), &p);
						p.SetID(id);
						a->Append(p);
						if (vars)
							flags = ScriptFlags();
						break;
					}
					
					a->Append(ptr<anything>(id));
				}
				
				b = b[id];
				if (token == TOK_EQUALS) {
					Next();
					Parse(b.GetTypeDef(), b);
					break;
				}
				if (token != TOK_DOT) {
					usings.push_back(b);
					break;
				}
				Expect(TOK_DOT);
				if (token != TOK_IDENTIFIER)
					iso_throw("Syntax");
				id = identifier;
			}

		} else {
			const Type	*type	= NULL;
			if (Peek() != TOK_EQUALS)
				type	= ParseType2();
			if (token != TOK_IDENTIFIER)
				iso_throw("Expected identifier");
				
			tag			id		= identifier;
			Next();
			if (token == TOK_EQUALS) {
				ptr<void>	p;
				Next();
				ParsePtr(type, &p);
				p.SetID(id);
				first->Add(p);
			} else if (TypeUser *user = local_user_types.Find((const char*)id)) {
				if (user->subtype && !user->subtype->SameAs(type))
					throw_accum("Type " << id << " differs from previous definition");
				else
					user->subtype = type;

			} else if (TypeUser *user = user_types.Find((const char*)id)) {
				if (user->subtype && !type->SameAs(user->subtype, MATCH_MATCHNULL_RHS)) {
					if (user->flags & TypeUser::FROMFILE) {
						user->subtype = type;
					} else if (flags & SCRIPT_ALLOWBUILTINDIFF) {

					} else {
						throw_accum("Type " << id << " differs from built-in");
					}
				} else {
					user->subtype = type;
				}
			} else {
				user = new TypeUserSave(id, type, TypeUser::FROMFILE | TypeUser::WRITETOBIN);
				if (!type || (type->Dodgy()))
					user->flags |= Type::TYPE_DODGY;
			}
//			user_types.Add(new TypeUserSave(id, type));	//automatic in constructor
		}
	}
}

//-----------------------------------------------------------------------------
//	stubs
//-----------------------------------------------------------------------------

uint32 ScriptFlags() {
	Browser	b = root("variables");
	return	(b["keepexternals"].GetInt()	? SCRIPT_KEEPEXTERNALS		: 0)
		|	(b["dontconvert"].GetInt()		? SCRIPT_DONTCONVERT		: 0)
		|	(b["skipoldfields"].GetInt()	? SCRIPT_SKIPOLDFIELDS		: 0)
		|	(b["skiperrors"].GetInt()		? SCRIPT_SKIPERRORS			: 0)
		|	(b["linenumbers"].GetInt()		? SCRIPT_LINENUMBERS		: 0)
		|	(b["relativepaths"].GetInt()	? SCRIPT_RELATIVEPATHS		: 0)
		|	(b["allowbuiltindiff"].GetInt()	? SCRIPT_ALLOWBUILTINDIFF	: 0);
}

bool ScriptRead(ptr<void> &p, tag id, const char *fn, istream_ref file, int flags, const char *prefix) {
	if (file.exists()) {
		ScriptReader	token(file, flags, fn);
		token.SetPrefix(prefix);
		try {
			if (fn && fn[0]) {
				token.Add(GetDirectory("local", filename(fn).dir()));
				token.Add(token.pfn = ptr<ptr_string<char,32>>("_filename", fn));
			} else {
				token.Add(GetDirectory("local", "."));
			}
			
			token.Add(ptr<int>("filedepth", FileHandler::Include::Depth()));
			token.GetDefines();
			
			if (token.ParsePtr(NULL, &p))
				p.SetFlags(Value::HASEXTERNAL);
				
			if (p && (!(flags & SCRIPT_KEEPID) || !p.ID()))
				p.SetID(id);
				
			if (!(flags & SCRIPT_NOCHECKEND) && token != EOF)
				iso_throw("extra text past end of value");
				
			return true;
			
		} catch (const char *error) {
			throw_accum(error << " at line " << token.GetLineNumber());
		}
	}
	return false;
}

bool ScriptRead(const Browser &b, istream_ref file, int flags, const char *prefix) {
	if (b.GetType() != REFERENCE)
		return ScriptReader(file, flags).Next().Parse(b.GetTypeDef(), b);

	tag2		id	= b.GetName();
	bool		ret = ScriptReader(file, flags).Next().Parse(b.GetTypeDef(), b);
	ptr<void>	*p 	= b;
	if (!p->ID())
		p->SetID(id);
	return ret;
}

const Type *ScriptReadType(istream_ref file) {
	return ScriptReader(file, 0).Next().ParseType();
}

const Type *ScriptReadType(const char *s) {
	return ScriptReader(memory_reader(s).me(), 0).Next().ParseType();
}

TypeUser *ScriptReadType(tag id, istream_ref file) {
	return new TypeUserSave(id, ScriptReadType(file));
}

bool ScriptReadDefines(istream_ref file, UserTypeArray &types) {
	if (file.exists()) {
		ScriptReader	include(file, 0);
		try {
			include.GetDefines();
			types.append(include.GetUserTypes());
			return true;
		} catch (const char *error) {
			throw_accum(error << " at line " << include.GetLineNumber());
		}
	}
	return false;
}

bool ScriptReadDefines(const filename &fn, UserTypeArray &types) {
	if (exists(fn)) {
		FileHandler::Include	inc(fn);
		FileInput				file(fn);
		ScriptReader		include(file, 0, fn);
		try {
			include.GetDefines();
			types.append(include.GetUserTypes());
			return true;
		} catch (const char *error) {
			throw_accum(error << " at line " << include.GetLineNumber() << " in " << fn);
		}
	}
	return false;
}

ScriptReadContext::ScriptReadContext(istream_ref stream, int flags) {
	r = new ScriptReader(stream, flags);
}

ScriptReadContext::~ScriptReadContext() {
	ScriptReader *token = static_cast<ScriptReader*>(r);
	delete token;
}

const char* ScriptReadContext::next_symbol() {
	ScriptReader *token = static_cast<ScriptReader*>(r);
	for(;;) {
		if (token->Next() == ScriptReader::TOK_NUMBER)
			token->GetNumber();
		switch (*token) {
			case ScriptReader::TOK_EOF:				return NULL;
			case ScriptReader::TOK_IDENTIFIER:		return token->Identifier();
			case ScriptReader::TOK_STRINGLITERAL:	return unescape(token->Identifier());
			default:								break;
		}
	}
}

TypeScript::TypeScript(tag id, const char *script) : type(ScriptReadType(id, memory_reader(script).me())) {}

fixed_string<256> TypeScript::write() const {
	fixed_string<256>	s;
	memory_writer		m(memory_block(s.begin(), 256));
	ScriptWriter(m).DumpType(type);
	m.putc(0);
	return s;
}

}//namespace iso
