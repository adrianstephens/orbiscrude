#include "yaml.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	YAMLreader
//-----------------------------------------------------------------------------

char_set	c_printable				= char_set(0x9) | 0xA | 0xD | char_set(0x20, 0x7E);// | 0x85;// | [0xA0-0xD7FF] | [0xE000-0xFFFD] | [0x10000-0x10FFFF];
char_set	c_indicator				= char_set("-?:,[]{}#&*!|>\'\"%@`");
char16		b_line_separator		= char16(0x2028);
char16		b_paragraph_separator	= char16(0x2029);
char_set	b_char					= char_set("\n\r\x85");
char_set	nb_char					= c_printable - b_char;
char_set	s_white					= char_set(" \t");
char_set	ns_char					= nb_char - s_white;
char_set	ns_dec_digit			= char_set('0', '9');
char_set	ns_hex_digit			= ns_dec_digit | char_set('A', 'F') | char_set('a', 'f');
char_set	ns_ascii_letter			= char_set('A', 'Z') | char_set('a', 'z');
char_set	ns_word_char			= ns_dec_digit | ns_ascii_letter | '-';
char_set	ns_uri_char				= ns_word_char | char_set(";/?:@&=+$,_.!~*'()[]");// | '%' ns_hex_digit ns_hex_digit;
char_set	ns_tag_char				= ns_uri_char - '!';

YAMLreader::YAMLreader(const delegate &d, istream_ref _file, int line_number) : text_reader<istream_ref, 2>(_file, line_number), context(KEY), d(d) {
	tags[""]	= "!";
	tags["!"]	= "tag:yaml.org,2002:";
}

int YAMLreader::get_start() {
	int			c;
	while ((c = skip_chars(*this, b_char | ' ')) == '#')
		skip_chars(*this, ~char_set('\n'));

	int	col = col_number - 1;
	switch (c) {
		case '-':
			if (peekc() == '-') {
				put_back(c);
				return 0;
			}
			break;

		case '.':
			if (peekc() == '.') {
				put_back(c);
				return 0;
			}
			break;
	}
	put_back(c);
	return col;
}

string YAMLreader::get_tag() {
	int	c	= getc();
	if (c == '<')
		return get_token0(~char_set('>'));

	put_back(c);
	string	tag		= get_token(ns_tag_char);
	c		= getc();
	if (c != '!') {	//primary
		return tags[""] + tag;
	} else {
		string	tag2 = get_token(ns_uri_char);
		return tags[tag] + tag2;
	}
}

void *YAMLreader::get_map(const char *map_tag, const char *tag, const void *map_key, const void *key, int indentation, const char *anchor) {
	string		str_tag, str_anchor;
	Begin(map_tag, map_key, MAP);
	int		i = 0;
	do {
		int	c = getc();
		if (c != ':')
			throw("Expected ':' after mapping key");

		context = BLOCK;
		void	*val = get_value(tag, key, indentation);

		if (anchor)
			anchors[anchor] = val;

		Entry(key, val);
		++i;

		context = KEY;
		if (get_start() != indentation)
			break;

		anchor	= tag = 0;
		while ((c = skip_chars(*this, ' ')) == '&' || c == '!') {
			if (c == '&')	// define anchor
				anchor = str_anchor = get_token(ns_char);
			else 			// tag
				tag = str_tag = get_tag();
		}
		put_back(c);

	} while (key = get_value(0, 0, indentation));
	return End(MAP);
}


void *YAMLreader::get_block(const char *map_tag, const char *tag, const void *map_key, int indentation, const char *anchor) {
	int		c = getc();
	if (c == '-') {
		string		str_tag, str_anchor;
		c	= getc();
		if (c == ' ') {
			// block seq
			Begin(map_tag, map_key, SEQ);
			int		i = 0;
			do {
				c = skip_chars(*this, ' ');
				put_back(c);
				int			col = col_number;

				void	*val = get_value(tag, 0, indentation);
				c = skip_chars(*this, ' ');
				if (c == ':') {
					put_back(c);
					val = get_map(tag, 0, 0, val, col, 0);
				}

				if (anchor)
					anchors[anchor] = val;

				Entry(0, val);
				++i;

				if (get_start() != indentation)
					return End(SEQ);

				anchor	= tag = 0;
				while ((c = skip_chars(*this, ' ')) == '&' || c == '!') {
					if (c == '&')	// define anchor
						anchor = str_anchor = get_token(ns_char);
					else 			// tag
						tag = str_tag = get_tag();
				}

			} while (c == '-');
			put_back(c);
			return End(SEQ);
		}
	}
	// block map
	put_back(c);

	void	*val;
	context = KEY;

	//unity hack
	if (c == 's') {
		auto	value = get_string(indentation);
		if (value == "stripped") {
			c = skip_chars(*this, ' ');
			put_back(c);
			value = get_string(indentation);
		}
		val = Value(0, 0, value);
	} else {
		val = get_value(0, 0, indentation);
	}

//	if (val == "stripped")
//		val = get_value(0, 0, indentation);

	return get_map(map_tag, tag, map_key, val, indentation, anchor);
}

void *YAMLreader::get_node(const void *key, int indentation) {
	string		tag, anchor;
	int			c;
	while ((c = skip_chars(*this, ' ')) == '&' || c == '!') {
		if (c == '&')	// define anchor
			anchor	= get_token(ns_char);
		else 			// tag
			tag		= get_tag();
	}

	put_back(c);
	void	*p = get_value(tag, key, indentation);
	if (anchor)
		anchors[anchor] = p;
	return p;
}

string YAMLreader::get_string(int indentation) {
	string	value;
	if (context == FLOW) {
		value = get_token(nb_char - char_set(",}]:#\t"));
	} else {
		value = get_token(nb_char - char_set(":#\t"));
		for (;;) {
			int	c = getc();
			if (c != '\n') {
				put_back(c);
				break;
			}
			if (get_start() <= indentation)
				break;
			value += get_token(nb_char - char_set(":#\t"));
		}
	}

	return value;
}


void *YAMLreader::get_value(const char *tag, const void *key, int indentation) {
	for (;;) {
		int	c = skip_chars(*this, ' ');
		switch (c) {
			case '#':
				skip_chars(*this, ~char_set('\n'));
			case '\n':
				while ((c = skip_chars(*this, b_char | ' ')) == '#')
					skip_chars(*this, ~char_set('\n'));

				put_back(c);
				//if (context == KEY)
				//	continue;

				if (col_number + int(c == '-') > indentation)
					return get_block(tag, 0, key, col_number, 0);

				return 0;

			case '@':
			case '`':		// reserved
				throw("reserved");

			case '*':		// use anchor
				return anchors[get_token(ns_char)];

			case '?':		// mapping key
				c	= getc();
				if (c == ' ')
					return get_node(0, indentation);

				//plain
				break;

			case '\'':		// single-quoted scalar
				return Value(tag, key, get_token0(~char_set("'")));

			case '"': {		// double-quoted scalar
				string_builder	b;
				while ((c = getc()) != '"') {
					if (c == '\\')
						c = get_escape(*this);
					b.putc(c);
				}
				return Value(tag, key, b);
			}

			case '|': {	// literal block scalar
				string	value;
				for (;;) {
					int	n = get_leading_spaces();
					if (n < indentation)
						return value;

					read_line(value);
				}
				return Value(tag, key, value);
			}

			case '>': {	// folded block scalar
				string	value;
				for (;;) {
					int	n = get_leading_spaces();
					if (n < indentation)
						return value;

					read_line(value);
				}
				return Value(tag, key, value);
			}

			case '[': {	// sequence start
				Begin(tag, key, SEQ);
				auto	s = save(context, FLOW);
				while ((c = skip_whitespace(*this)) != ']') {
					put_back(c);
					int		ind = 0;
					void	*val = get_node(0, ind);
					Entry(0, val);
					if ((c = getc()) != ',')
						put_back(c);
				}
				return End(SEQ);
			}

			case '{': {	// mapping start
				Begin(tag, key, MAP);
				auto	s = save(context, FLOW);
				while ((c = skip_whitespace(*this)) != '}') {
					put_back(c);
					int		ind		= 0;
					void	*key	= get_node(0, ind);
					if (getc() != ':')
						throw("Expected ':' after mapping key");
					void	*val	= get_node(key, ind);
					Entry(key, val);
					if ((c = getc()) != ',')
						put_back(c);
				}
				return End(SEQ);
			}

			default:
				put_back(c);
				break;
		}
		break;
	}

	//plain style
	return Value(tag, key, get_string(indentation));
}

void YAMLreader::Read() {
	bool	doc = false;
	for (;;) {
		int			c;
		while ((c = skip_chars(*this, b_char | ' ')) == '#')
			skip_chars(*this, ~char_set('\n'));

		switch (c) {
			case '%': {	// directive
				string	dir = get_token(ns_char);
				if (dir == "YAML") {
					version = get_token(ns_char);
					continue;

				} else if (dir == "TAG") {
					c = skip_chars(*this, ' ');
					if (c != '!')
						throw("TAG must start with '!'");

					string	tag		= get_token(ns_tag_char);
					c = getc();

					if (tag == "") {
						if (c == '!')
							tag = "!";
					} else if (c != '!') {
						throw("TAG must end with '!'");
					}
					tags[tag] = get_token(ns_uri_char, ns_tag_char);
					continue;
				}
			}

			case '-': {
				if ((c = getc()) == '-') {
					if (getc() == '-') {
						if (doc)
							Entry(0, End(DOC));
						Begin(0, 0, DOC);
						doc	= true;
						c = skip_chars(*this, ' ');
						if (c == '\n')
							continue;
						break;
					}
					throw("Syntax");
				}
				put_back(c);
				c = '-';
				break;
			}

			case '.':	// stream end
				if (c = getc() == '.') {
					if (c = getc() == '.') {
						skip_chars(*this, ~char_set('\n'));
						if (doc) {
							Entry(0, End(DOC));
							doc = false;
						}
						continue;
					}
				}
				break;

			default:
				break;
		}

		if (c == -1)
			break;

		put_back(c);
		if (!doc) {
			Begin(0, 0, DOC);
			doc = true;
		}
	#if 1
		string		tag, anchor;
		while ((c = skip_chars(*this, ' ')) == '&' || c == '!') {
			if (c == '&')	// define anchor
				anchor	= get_token(ns_char);
			else			// tag
				tag		= get_tag();
		}
		if (c != '\n')
			put_back(c);
		void	*p = get_block(0, tag, 0, 1, anchor);
	#else
		void	*p = get_node(0);
	#endif
		Entry(0, p);
	}
	if (doc)
		Entry(0, End(DOC));
}

//-----------------------------------------------------------------------------
//	YAMLwriter
//-----------------------------------------------------------------------------

void YAMLwriter::NewLine() {
	file.write("\r\n");
	for (int i = 0; i < indent; i++)
		file.putc('\t');
}

void YAMLwriter::Start(const char *id) {
	if (!first)
		file.putc(',');
	first = false;
	NewLine();
	if (named & 1) {
		file.putc('"');
		file.write(id);
		file.putc('"');
		file.putc(':');
	}
}
void YAMLwriter::Begin(const char *id, bool names) {
	Start(id);
	file.putc(names ? '{' : '[');
	named	= (named << 1) | int(names);
	first	= true;
	++indent;
}
void YAMLwriter::End() {
	char e	= named & 1 ? '}' : ']';
	named >>= 1;
	first	= false;
	--indent;
	NewLine();
	file.putc(e);
}
void YAMLwriter::Write(const char *id, bool b) {
	Start(id);
	file.write(b ? "true" : "false");
}
void YAMLwriter::Write(const char *id, int i) {
	Start(id);
	file.write((const char*)to_string(i));
}
void YAMLwriter::Write(const char *id, float f) {
	Start(id);
	file.write((const char*)to_string(f));
}
void YAMLwriter::Write(const char *id, const char *s) {
	Start(id);
	if (s) {
		file.putc('"');
		file.write(s);
		file.putc('"');
	} else {
		file.write("null");
	}
}

//-----------------------------------------------------------------------------
//	YAML document
//-----------------------------------------------------------------------------
void YAML::kill(YAMLval *p) {
	if (p) switch (p->type) {
		case YAMLval::ARRAY: {
			for (auto &i : *(YAMLarray*)p)
				kill(i);
			break;
		}
		case YAMLval::OBJECT: {
			for (auto &i : *(YAMLobject*)p)
				kill(i.b);
			break;
		}
	}
}

size_t YAML0::get_count() const {
	if (p) switch (p->type) {
		case YAMLval::ARRAY:	return ((YAMLarray*)p)->size();
		case YAMLval::OBJECT:	return ((YAMLobject*)p)->size();
	}
	return 0;
}

YAMLval *YAML0::GetNode(const char *s) const {
	if (p && p->type == YAMLval::OBJECT) {
		for (auto &i : *(YAMLobject*)p) {
			if (i.a == s)
				return i.b;
		}
	}
	return nullptr;
}

YAMLval *YAML0::GetNode(int i) const {
	if (p) switch (p->type) {
		case YAMLval::ARRAY: {
			YAMLarray	*a = (YAMLarray*)p;
			return i < a->size() ? (*a)[i] : 0;
		}
		case YAMLval::OBJECT: {
			YAMLobject	*a = (YAMLobject*)p;
			return i < a->size() ? (*a)[i].b : 0;
		}
	}
	return 0;
}

bool YAML0::write(YAMLwriter &w, const char *id, YAMLval *p) {
	if (!p) {
		w.Write(id, 0);

	} else switch (p->type) {
		case YAMLval::BOOL:		w.Write(id, *(YAMLelement<bool>*)p);	break;
		case YAMLval::INT:		w.Write(id, *(YAMLelement<int>*)p);		break;
		case YAMLval::FLOAT:	w.Write(id, *(YAMLelement<float>*)p);	break;
		case YAMLval::STRING:	w.Write(id, *(YAMLelement<string>*)p);	break;

		case YAMLval::ARRAY: {
			w.Begin(id, false);
			for (auto &i : *(YAMLarray*)p)
				write(w, 0, i);
			w.End();
			break;
		}
		case YAMLval::OBJECT: {
			w.Begin(id, true);
			for (auto &i : *(YAMLobject*)p)
				write(w, i.a, i.b);
			w.End();
			break;
		}
	}
	return true;
}
