#include "xml.h"
#include <stdarg.h>

using namespace iso;

static const struct {
	const char *name;
	uint16	c;
} XMLentities[] = {
/*	"lt",		'<',
	"gt",		'>',
	"amp",		'&',
	"quot",		'"',
	"ntilde",	'~',
	"apos",		'\'',
	"nbsp",		' ',
*/
	{"quot",	0x0022},	{"amp",		0x0026},	{"apos",	0x0027},	{"lt",		0x003C},
	{"gt",		0x003E},	{"nbsp",	0x00A0},	{"iexcl",	0x00A1},	{"cent",	0x00A2},
	{"pound",	0x00A3},	{"curren",	0x00A4},	{"yen",		0x00A5},	{"brvbar",	0x00A6},
	{"sect",	0x00A7},	{"uml",		0x00A8},	{"copy",	0x00A9},	{"ordf",	0x00AA},
	{"laquo",	0x00AB},	{"not",		0x00AC},	{"shy",		0x00AD},	{"reg",		0x00AE},
	{"macr",	0x00AF},	{"deg",		0x00B0},	{"plusmn",	0x00B1},	{"sup2",	0x00B2},
	{"sup3",	0x00B3},	{"acute",	0x00B4},	{"micro",	0x00B5},	{"para",	0x00B6},
	{"middot",	0x00B7},	{"cedil",	0x00B8},	{"sup1",	0x00B9},	{"ordm",	0x00BA},
	{"raquo",	0x00BB},	{"frac14",	0x00BC},	{"frac12",	0x00BD},	{"frac34",	0x00BE},
	{"iquest",	0x00BF},	{"Agrave",	0x00C0},	{"Aacute",	0x00C1},	{"Acirc",	0x00C2},
	{"Atilde",	0x00C3},	{"Auml",	0x00C4},	{"Aring",	0x00C5},	{"AElig",	0x00C6},
	{"Ccedil",	0x00C7},	{"Egrave",	0x00C8},	{"Eacute",	0x00C9},	{"Ecirc",	0x00CA},
	{"Euml",	0x00CB},	{"Igrave",	0x00CC},	{"Iacute",	0x00CD},	{"Icirc",	0x00CE},
	{"Iuml",	0x00CF},	{"ETH",		0x00D0},	{"Ntilde",	0x00D1},	{"Ograve",	0x00D2},
	{"Oacute",	0x00D3},	{"Ocirc",	0x00D4},	{"Otilde",	0x00D5},	{"Ouml",	0x00D6},
	{"times",	0x00D7},	{"Oslash",	0x00D8},	{"Ugrave",	0x00D9},	{"Uacute",	0x00DA},
	{"Ucirc",	0x00DB},	{"Uuml",	0x00DC},	{"Yacute",	0x00DD},	{"THORN",	0x00DE},
	{"szlig",	0x00DF},	{"agrave",	0x00E0},	{"aacute",	0x00E1},	{"acirc",	0x00E2},
	{"atilde",	0x00E3},	{"auml",	0x00E4},	{"aring",	0x00E5},	{"aelig",	0x00E6},
	{"ccedil",	0x00E7},	{"egrave",	0x00E8},	{"eacute",	0x00E9},	{"ecirc",	0x00EA},
	{"euml",	0x00EB},	{"igrave",	0x00EC},	{"iacute",	0x00ED},	{"icirc",	0x00EE},
	{"iuml",	0x00EF},	{"eth",		0x00F0},	{"ntilde",	0x00F1},	{"ograve",	0x00F2},
	{"oacute",	0x00F3},	{"ocirc",	0x00F4},	{"otilde",	0x00F5},	{"ouml",	0x00F6},
	{"divide",	0x00F7},	{"oslash",	0x00F8},	{"ugrave",	0x00F9},	{"uacute",	0x00FA},
	{"ucirc",	0x00FB},	{"uuml",	0x00FC},	{"yacute",	0x00FD},	{"thorn",	0x00FE},
	{"yuml",	0x00FF},	{"OElig",	0x0152},	{"oelig",	0x0153},	{"Scaron",	0x0160},
	{"scaron",	0x0161},	{"Yuml",	0x0178},	{"fnof",	0x0192},	{"circ",	0x02C6},
	{"tilde",	0x02DC},	{"Alpha",	0x0391},	{"Beta",	0x0392},	{"Gamma",	0x0393},
	{"Delta",	0x0394},	{"Epsilon",	0x0395},	{"Zeta",	0x0396},	{"Eta",		0x0397},
	{"Theta",	0x0398},	{"Iota",	0x0399},	{"Kappa",	0x039A},	{"Lambda",	0x039B},
	{"Mu",		0x039C},	{"Nu",		0x039D},	{"Xi",		0x039E},	{"Omicron",	0x039F},
	{"Pi",		0x03A0},	{"Rho",		0x03A1},	{"Sigma",	0x03A3},	{"Tau",		0x03A4},
	{"Upsilon",	0x03A5},	{"Phi",		0x03A6},	{"Chi",		0x03A7},	{"Psi",		0x03A8},
	{"Omega",	0x03A9},	{"alpha",	0x03B1},	{"beta",	0x03B2},	{"gamma",	0x03B3},
	{"delta",	0x03B4},	{"epsilon",	0x03B5},	{"zeta",	0x03B6},	{"eta",		0x03B7},
	{"theta",	0x03B8},	{"iota",	0x03B9},	{"kappa",	0x03BA},	{"lambda",	0x03BB},
	{"mu",		0x03BC},	{"nu",		0x03BD},	{"xi",		0x03BE},	{"omicron",	0x03BF},
	{"pi",		0x03C0},	{"rho",		0x03C1},	{"sigmaf",	0x03C2},	{"sigma",	0x03C3},
	{"tau",		0x03C4},	{"upsilon",	0x03C5},	{"phi",		0x03C6},	{"chi",		0x03C7},
	{"psi",		0x03C8},	{"omega",	0x03C9},	{"thetasym",0x03D1},	{"upsih",	0x03D2},
	{"piv",		0x03D6},	{"ensp",	0x2002},	{"emsp",	0x2003},	{"thinsp",	0x2009},
	{"zwnj",	0x200C},	{"zwj",		0x200D},	{"lrm",		0x200E},	{"rlm",		0x200F},
	{"ndash",	0x2013},	{"mdash",	0x2014},	{"lsquo",	0x2018},	{"rsquo",	0x2019},
	{"sbquo",	0x201A},	{"ldquo",	0x201C},	{"rdquo",	0x201D},	{"bdquo",	0x201E},
	{"dagger",	0x2020},	{"Dagger",	0x2021},	{"bull",	0x2022},	{"hellip",	0x2026},
	{"permil",	0x2030},	{"prime",	0x2032},	{"Prime",	0x2033},	{"lsaquo",	0x2039},
	{"rsaquo",	0x203A},	{"oline",	0x203E},	{"frasl",	0x2044},	{"euro",	0x20AC},
	{"image",	0x2111},	{"weierp",	0x2118},	{"real",	0x211C},	{"trade",	0x2122},
	{"alefsym",	0x2135},	{"larr",	0x2190},	{"uarr",	0x2191},	{"rarr",	0x2192},
	{"darr",	0x2193},	{"harr",	0x2194},	{"crarr",	0x21B5},	{"lArr",	0x21D0},
	{"uArr",	0x21D1},	{"rArr",	0x21D2},	{"dArr",	0x21D3},	{"hArr",	0x21D4},
	{"forall",	0x2200},	{"part",	0x2202},	{"exist",	0x2203},	{"empty",	0x2205},
	{"nabla",	0x2207},	{"isin",	0x2208},	{"notin",	0x2209},	{"ni",		0x220B},
	{"prod",	0x220F},	{"sum",		0x2211},	{"minus",	0x2212},	{"lowast",	0x2217},
	{"radic",	0x221A},	{"prop",	0x221D},	{"infin",	0x221E},	{"ang",		0x2220},
	{"and",		0x2227},	{"or",		0x2228},	{"cap",		0x2229},	{"cup",		0x222A},
	{"int",		0x222B},	{"there4",	0x2234},	{"sim",		0x223C},	{"cong",	0x2245},
	{"asymp",	0x2248},	{"ne",		0x2260},	{"equiv",	0x2261},	{"le",		0x2264},
	{"ge",		0x2265},	{"sub",		0x2282},	{"sup",		0x2283},	{"nsub",	0x2284},
	{"sube",	0x2286},	{"supe",	0x2287},	{"oplus",	0x2295},	{"otimes",	0x2297},
	{"perp",	0x22A5},	{"sdot",	0x22C5},	{"vellip",	0x22EE},	{"lceil",	0x2308},
	{"rceil",	0x2309},	{"lfloor",	0x230A},	{"rfloor",	0x230B},	{"lang",	0x2329},
	{"rang",	0x232A},	{"loz",		0x25CA},	{"spades",	0x2660},	{"clubs",	0x2663},
	{"hearts",	0x2665},	{"diams",	0x2666},
};

enum {
	XML_ENTITIES_CONTENT= 3,
	XML_ENTITIES_ATTRIB	= 4,
	XML_ENTITIES_ALL	= num_elements(XMLentities),
};

//-----------------------------------------------------------------------------
//	XMLreader
//-----------------------------------------------------------------------------

cstring XMLreader::Data::Find(const char *name) const {
	for (int i = 1, n = values.size32(); i < n; i += 2) {
		if (str(values[i]) == name)
			return values[i + 1];
	}
	return 0;
}

cstring XMLreader::Data::Find(const ichar *name) const {
	for (int i = 1, n = values.size32(); i < n; i += 2) {
		if (str(values[i]) == name)
			return values[i + 1];
	}
	return 0;
}


template<typename C> bool match_wild(C *wild, const char *s, dynamic_array<count_string> &matches) {
	matches.clear();
	while (char c = *wild++) {
		if (c == '*') {
			c = *wild;
			auto	start = s;
			while (*s && *s != c)
				++s;
			matches.emplace_back(start, s);
		} else if (*s == c) {
			++s;
		} else {
			return false;
		}
	}
	return *s == 0;
}

cstring XMLreader::Data::FindWild(const char *name, dynamic_array<count_string> &matches) const {
	for (int i = 1, n = values.size32(); i < n; i += 2) {
		if (string_scan(values[i]).match_wild(name, matches))
			return values[i + 1];
	}
	return 0;
}

cstring XMLreader::Data::FindWild(const ichar *name, dynamic_array<count_string> &matches) const {
	for (int i = 1, n = values.size32(); i < n; i += 2) {
		if (string_scan(values[i]).match_wild(name, matches))
			return values[i + 1];
	}
	return 0;
}

int XMLreader::GetEscape() {
	int	v = 0;
	int	c = getc();
	if (c == '#') {
		c = getc();
		if (c == 'x') {
			while ((c = getc()) != EOF && c != ';') {
				if (!is_hex(c))
					throw("Expected hex digit");
				v = v * 16 + from_digit(c);
			}
		} else {
			do {
				if (!is_digit(c))
					throw("Expected decimal digit");
				v = v * 10 + c - '0';
			} while ((c = getc()) != EOF && c != ';');

		}
	} else {
		size_t	entity_start = value.size();
		while (c != EOF && c != ';') {
			value.push_back(c);
			c = getc();
		}

		count_string	name(value + entity_start, value.size() - entity_start);

		for (int i = 0; i < XML_ENTITIES_ALL; i++) {
			if (name == XMLentities[i].name) {
				v = XMLentities[i].c;
				break;
			}
		}
		if (!v) {
			auto	entity = custom_entities[name].get();
			if (entity.exists()) {
				value.resize(entity_start);
				for (const char *p = get(entity); *p; p++)
					value.push_back(*p);
				return value.pop_back_retref();
			}
			if (flags.test(SKIPUNKNOWNENTITIES))
				v = '?';
			else
				throw("Unrecognised entity name");
		}

		value.resize(entity_start);
	}
	return v;
}

char *XMLreader::FixEscapes(char *s) {
	if (!s)
		return s;

	char	*p, *d, c;
	for (p = d = s; c = *p++; ) {
		if (c == '&') {
			uint32	v = 0;
			if ((c = *p++) == '#') {
				if (*p == 'x') {
					++p;
					while (is_hex(c = *p)) {
						v = v * 16 + from_digit(c);
						++p;
					}
				} else {
					int	v = 0;
					while (is_digit(c = *p)) {
						v = v * 10 + c - '0';
						++p;
					}
				}
				if (c != ';')
					--p;

			} else {
				char	*e = s;;
				while ((c = *e) && c != ';')
					++e;
				count_string	name(s, e - s);
				for (int i = 0; i < XML_ENTITIES_ALL; i++) {
					if (name == XMLentities[i].name) {
						v = XMLentities[i].c;
						break;
					}
				}
			}
			d += put_char(v, d);

		} else {
			*d++ = c;
		}
	}
	*d = 0;
	return s;
}

bool XMLreader::GetName(int c) {
	if (flags.test(ALLOWBADNAMES)) {
		while (!is_whitespace(c) && c != '=') {
			value.push_back(c);
			c = getc();
		}
	} else {
		if (!is_alpha(c) && c != '_' && c != ':') {
			if (flags.test(SKIPBADNAMES)) {
				while (!is_whitespace(c) && c != '>' && c != '/' && c != '?')
					c = getc();
				return false;
			}
			throw("Names must start with an alphabetic character or _ or :");
		}

		while (is_alphanum(c) || c == '_' || c == ':' || c == '-' || c == '.') {
			value.push_back(c);
			c = getc();
		}
	}
	put_back(c);
	return true;
}

XMLreader::TagType XMLreader::ReadNext(Data &data) {
	bool	directive	= false;
	bool	whitespace	= true;
	value.clear();
	data.values.clear();

	int		c;
	while ((c = getc()) != EOF && c != '<') {
		if (c == '&') {
			c = GetEscape();
			whitespace = false;
		} else if (flags.test(GIVEWHITESPACE) || (whitespace && c != ' ' && c != '\n' && c != '\r' && c != '\t')) {
			whitespace = false;
		}
		value.push_back(c);
	}

	if (value.size() && !whitespace) {
		data.values.push_back(value.begin());
		data.values.push_back(value.end());
		value.push_back(0);
		put_back(c);
		return TAG_CONTENT;
	}

	if (c == EOF)
		return TAG_EOF;

	switch (c = getc()) {
		case '!':	// declarative statement
			c = getc();
			if (c == '-') {								// comment
				if (getc() != '-') {
//					if (!flags.test(RELAXED))
						throw("Expected '-' after '<!-'");
				}
				for (int stage = 0;;) {
					c = getc();
					value.push_back(c);
					if (c == '-' && stage == 0)
						stage = 1;
					else if (c == '-' && stage == 1)
						stage = 2;
					else if (c == '>' && stage == 2)
						break;
					else
						stage = 0;
				}
				data.values.push_back(value.begin());
				data.values.push_back(value.end() - 3);
				return TAG_COMMENT;

			} else {
				if (c == '[') {
					static const char cdata[] = "[CDATA[";
					const char	*p;
					for (p = cdata; *p && c == *p; ++p)
						c = getc();

					if (*p == 0) {							// CDATA
						value.clear();
						value.push_back(c);
						for (int stage = 0;;) {
							c = getc();
							value.push_back(c);
							if (c == ']' && stage == 0)
								stage = 1;
							else if (c == ']' && stage == 1)
								stage = 2;
							else if (c == '>' && stage == 2)
								break;
							else
								stage = 0;
						}
						data.values.push_back(value.begin());
						data.values.push_back(value.end() - 3);
						return TAG_CONTENT;
					} else {
						do c = getc(); while (c != '>');
					}
				} else {
					value.clear();
					GetName(c);
					value.push_back(0);

					data.values.push_back(nullptr);

					c = skip_whitespace(*this);

					while (c != EOF && c != '>') {
						int	start = c, end, count = 1;
						switch (start) {
							case '(':	end = ')'; break;
							case '[':	end = ']'; break;
							case '"':	end = '"'; c = getc(); start = 0; break;
							default:	end = ' '; start = 0; break;
						}

						size_t	value_start	= value.size();
						do {
							value.push_back(c);
							c = getc();
							if (c == EOF || c == '>')
								break;
							if (c == start)
								count++;
						} while (c != end || --count);

						if (start)
							value.push_back(c);
						value.push_back(0);

						if (c != '>')
							c = skip_whitespace(*this);
						data.values.push_back((char*)value_start);
					}

					for (auto &i : data.values)
						i = value + intptr_t(i);

					return TAG_DECL;
				}
			}
			break;

		case '/': {
			size_t	entity_start = value.size();
			GetName(getc());
			value.push_back(0);
			if (skip_whitespace(*this) != '>' && !flags.test(RELAXED))
				throw("Expected '>' after closing entity");

			data.values.push_back(value + entity_start);
			return TAG_END;
		}

		case '?':	// processing directive
			directive	= true;
			c			= getc();
		default: {
			size_t		entity_start = value.size(), name_start, value_start;

			if (!GetName(c)) {
				value.push_back('<');
				value.push_back(c);
				break;
			}

			value.push_back(0);
			c = skip_whitespace(*this);

			data.values.push_back((char*)entity_start);

			while (c != '>' && c != '/' && c != '?') {
				name_start	= value.size();

				if (c == '<') {
					while ((c = getc()) != '>' && c != EOF);
					break;
				}

				if (!GetName(c)) {
					c = getc();
					continue;
				}

				value.push_back(0);
				value_start	= value.size();

				c = skip_whitespace(*this);
				if (c == '=') {
					c = skip_whitespace(*this);
					if (c == '"' || c == '\'') {
						for (char q = c; (c = getc()) != q; value.push_back(c)) {
							if (c == '&')
								c = GetEscape();
							else if (c == '<')
								throw("Illegal character '<' in element tag");
						}
					} else if (flags.test(UNQUOTEDATTRIBS)) {
						for (;;) {
							value.push_back(c);
							c = getc();
							if (c == '>' || c == ' ')
								break;
							if (c == '&')
								c = GetEscape();
							else if (c == '<')
								throw("Illegal character '<' in element tag");
						}
						put_back(c);
					} else {
						throw("Attribute values must be enclosed in quotes");
					}
					c = skip_whitespace(*this);
				} else if (!flags.test(NOEQUALATTRIBS)) {
					throw("Expected '=' after attribute name");
				}

				value.push_back(0);

				data.values.push_back((char*)name_start);
				data.values.push_back((char*)value_start);
			}

			for (auto &i : data.values)
				i = value + intptr_t(i);

			if (c == '/') {
				if (getc() != '>' && !flags.test(RELAXED))
					throw("Expected '>' after '/' in element tag");
				return TAG_BEGINEND;
			} else if (directive && c == '?') {
				if (getc() != '>' && !flags.test(RELAXED))
					throw("Expected '>' after '?' in directive tag");
				return TAG_DIR;
			} else {
				return TAG_BEGIN;
			}
		}
	}
	return TAG_ERROR;
}

XMLreader::TagType XMLreader::ReadNext(Data &data, TagType type) {
	TagType type2;
	while ((type2 = ReadNext(data)) != type) {
		switch (type2) {
			case TAG_BEGINEND:
				if (type == TAG_BEGIN)
					return type2;
				break;
			case TAG_BEGIN:
				type2 = ReadNext(data, TAG_END);
				if (type2 == TAG_END)
					break;
			case TAG_END:
			case TAG_ERROR:
			case TAG_EOF:
				return type2;
			default:
				break;
		}
	}
	return type2;
}

float XMLreader::CheckVersion() {
	float		version = -1;
	char		test[6];
	for (int i = 0; i < 5; i++)
		test[i] = getc();
	test[5] = 0;
	if (cstr(test) == "<?xml") try {
		r.seek(0);
		mode.detect(r, backup);
		Data	data;
		if ((save(flags, flags.set(SKIPBADNAMES)), ReadNext(data)) == TAG_DIR && data.Is("xml"))
			data.Read("version", version);
	} catch (...) {
		return -1;
	}
	return version;
}

//-----------------------------------------------------------------------------
//	XMLwriter
//-----------------------------------------------------------------------------

void XMLwriter::EscapeString(const char *name, int num_entities) {
	while (name) {
		const char	*p = NULL, *e;
		int			entity = -1;
		for (p = name; *p >= ' '; p++);
		if (*p == 0)
			p = NULL;
		for (int i = 0; i < num_entities; i++) {
			if ((e = strchr(name, XMLentities[i].c)) && (!p || e < p)) {
				p = e;
				entity = i;
			}
		}
		if (p) {
			puts(name, int(p - name));
			if (*p < 32)
				puts(format_string("&#x%02X;", *p));
			else
				puts(format_string("&%s;", XMLentities[entity].name));
			name	= p + 1;
		} else {
			puts(name);
			name	= NULL;
		}
	}
}

void XMLwriter::ElementBegin(const char *name) {
	if (opentag) {
		puts(">", 1);
		indent++;
	}
	if (indent < 0)
		indent = 0;
	else
		NewLine();

	puts("<", 1);
	puts(name);
	opentag			= true;
	closetagnewline = false;
}

void XMLwriter::Attribute(const char *name, const char *value) {
	puts(format_string(" %s=\"", name));
	EscapeString(value, XML_ENTITIES_ATTRIB);
	puts("\"", 1);
}

void XMLwriter::ElementEnd(const char *name) {
	if (opentag) {
		puts(name[0] == '?' ? "?>" : "/>");
		opentag	= false;
	} else {
		indent--;
		if (closetagnewline)
			NewLine();
		puts(format_string("</%s>", name));
	}
	closetagnewline = true;
}

void XMLwriter::ElementContent(const char *text) {
	if (!text)
		return;

	if (opentag) {
		puts(">", 1);
		opentag	= false;
		indent++;
	}
	EscapeString(text, XML_ENTITIES_CONTENT);
}

void XMLwriter::ElementContentVerbatim(const char *text, size_t len) {
	if (!text)
		return;

	if (opentag) {
		puts(">", 1);
		opentag	= false;
		indent++;
	}
	puts(text, len);
}

void XMLwriter::CDATA(const void *data, int len) {
	if (opentag) {
		puts(">", 1);
		opentag	= false;
		indent++;
	}
	NewLine();
	puts("<![CDATA[", 9);
	puts((char*)data, len);
	puts("]]>", 3);
	closetagnewline = true;
}

void XMLwriter::Comment(const char *comment) {
	puts("<!--");
	puts(comment);
	puts("-->");
}

void XMLwriter::NewLine() {
	if (format) {
		putc('\r');
		putc('\n');
		for (int i = 0; i < indent; i++)
			putc('\t');
	}
}

//-----------------------------------------------------------------------------
//	XML DOM
//-----------------------------------------------------------------------------

XMLDOM::XMLDOM(istream_ref file) : XMLDOM0(new XMLDOM_node("xml")) {
	if (!file)
		return;

	XMLreader::Data	data;
	XMLreader		xml(file);
	XMLDOM_node		*stack[128], **sp = stack;

	*sp = *this;

	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND: {
				XMLDOM_node	*node = new XMLDOM_node(data.Name());
				(*sp)->Append(node);
				for (auto &i : data.Attributes())
					node->Append(i.name, i.value);

				if (tag == XMLreader::TAG_BEGIN)
					*++sp = node;
				break;
			}
			case XMLreader::TAG_END:
				sp--;
				break;

			case XMLreader::TAG_CONTENT:
				(*sp)->Set(data.Content().begin());
				break;

			default:
				break;
		}
	}
}

const char*	XMLDOM_node::GetAttr(const char *s) const {
	for (auto &i : attrs) {
		if (istr(s) == i.name)
			return i.value;
	}
	return nullptr;
}

XMLDOM_node *XMLDOM_node::GetNode(const char *s) const {
	bool	wild = s[0] == '*';
	if (wild) {
		if (const char *colon = strchr(s, ':'))
			s = colon + 1;
	}
	for (auto &i : *this) {
		const char *name = i->name;
		if (wild) {
			if (const char *colon = str(name).find(':'))
				name = colon + 1;
		}
		if (istr(s) == name)
			return i;
	}
	return nullptr;
}

bool XMLDOM0::_write(XMLwriter &xml, XMLDOM_node *node) {
	for (auto &i : node->attrs)
		xml.Attribute(i.name, i.value);

	for (XMLDOM_node *child : *node) {
		xml.ElementBegin(child->Name());
		_write(xml, child);
		xml.ElementEnd(child->Name());
	}
	xml.ElementContent(node->Value());
	return true;
}
