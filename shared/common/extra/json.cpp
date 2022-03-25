#include "json.h"
#include "base/maths.h"
#include "base/algorithm.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	JSONreader
//-----------------------------------------------------------------------------

JSONreader::Token JSONreader::GetToken() {
	int	c = skip_whitespace(*this);

	switch (c) {
		case ',': return Token::COMA;
		case ':': return Token::COLON;
		case '[': return Token::ARRAY_BEGIN;
		case ']': return Token::ARRAY_END;
		case '{': return Token::OBJECT_BEGIN;
		case '}': return Token::OBJECT_END;
		case '"': {
//			Token	tok(Token::ELEMENT_STRING);
//			string&	s = (string&)tok.s;
			string_builder	b;
			for (; (c = getc()) != '"';) {
				if (c == '\\') {
					char32	c0 = get_escape(*this);
					if (between(c0, 0xd800, 0xdfff)) {
						if ((c = getc()) == '\\') {
							c = get_escape(*this);
							if (auto c2 = from_utf16_surrogates(c0, c)) {
								b << c2;
								continue;
							}
						}
						b << c0 << c;
					} else {
						b << c0;
					}
				} else {
					b.putc(c);
				}
			}
			return b.term();
//			return tok;
		}

		case 't':
		case 'f':
		case 'n': {
			buffer_accum<64>	ba;
			do
				ba << char(c);
			while (is_alphanum(c = getc()));
			put_back(c);
			if (str(ba) == "true")
				return true;
			else if (str(ba) == "false")
				return false;
			else if (str(ba) == "null")
				return Token::ELEMENT_NIL;
			break;
		}

		default: {
			bool	neg = c == '-';
			if (neg)
				c = getc();

			if (is_digit(c)) {
				uint64	i	= c - '0';
				int		e	= 0;
				if (c == '0') {
					c = getc();
				} else {
					while (is_digit(c = getc()))
						i = i * 10 + c - '0';
				}
				if (c == '.') {
					while (is_digit(c = getc())) {
						i = i * 10 + c - '0';
						e--;
					}
				}
				if (c == 'e' || c == 'E') {
					bool	nege = false;
					c = getc();
					if (c == '+' || (nege = (c == '-')))
						c = getc();

					int	d = 0;
					while (is_digit(c = getc()))
						d = d * 10 + c - '0';
					e = nege ? e - d : e + d;
				}
				put_back(c);
				if (e == 0)
					return neg ? -int64(i) : int64(i);
				else
					return (neg ? -int64(i) : int64(i)) * pow(10.0, e);
			}
			break;
		}
	}
	return Token::BAD;
}

//-----------------------------------------------------------------------------
//	JSONwriter
//-----------------------------------------------------------------------------

decltype(declval<JSONwriter::B>().begin()) JSONwriter::Start(const char *id) {
	auto	a = begin();
	a << onlyif(!first, ',') << "\r\n" << repeat('\t', indent);
	first = false;
	if (named & 1)
		a << '"' << id << "\":";
	return a;
}
void JSONwriter::Begin(const char *id, bool names) {
	Start(id);
	putc(names ? '{' : '[');
	named	= (named << 1) | int(names);
	first	= true;
	++indent;
}
void JSONwriter::End() {
	bool	was_named	= named & 1;
	named >>= 1;
	first	= false;
	--indent;
	begin() << "\r\n" << repeat('\t', indent) << (was_named ? '}' : ']');
}

void JSONwriter::Write(const char *id, const char *s) {
	if (s)
		Start(id) << '"' << s << '"';
	else
		Start(id) << "null";
}

//-----------------------------------------------------------------------------
//	JSON document
//-----------------------------------------------------------------------------

JSONval	JSONval::none;

bool JSONval::read(JSONreader &r) {
	JSONreader::Token tok = r.GetToken();
	switch (tok.type) {
		case JSONreader::Token::ARRAY_BEGIN: {
			*this = JSONval(JSONval::array());
			int	c = skip_whitespace(r);
			for (bool first = true; c != ']'; first = false) {
				if (first)
					r.put_back(c);
				else if (c != ',')
					throw("Missing ','");
				a.push_back().read(r);
				c = skip_whitespace(r);
			}
			break;
		}

		case JSONreader::Token::OBJECT_BEGIN: {
			*this = JSONval(JSONval::object());
			int	c = skip_whitespace(r);
			for (bool first = true; c != '}'; first = false) {
				if (first)
					r.put_back(c);
				else if (c != ',')
					throw("Missing ','");
				tok = r.GetToken().me();
				if (tok.type != JSONreader::Token::ELEMENT_STRING)
					throw("Expected String");
				if (skip_whitespace(r) != ':')
					throw("Expected Colon");
				o.emplace_back(tok.s, JSONval()).b.read(r);
				c = skip_whitespace(r);
			}
			sort(o, [](const pair<string,JSONval> &a, const pair<string,JSONval> &b) { return a.a < b.a; });
			break;
		}

		case JSONreader::Token::ELEMENT_NIL:	*this = JSONval(NIL); break;
		case JSONreader::Token::ELEMENT_BOOL:	*this = JSONval(!!tok.i); break;
		case JSONreader::Token::ELEMENT_INT:	*this = JSONval(int64(tok.i)); break;
		case JSONreader::Token::ELEMENT_FLOAT:	*this = JSONval(double(tok.f)); break;
		case JSONreader::Token::ELEMENT_STRING:	*this = string(tok.s); break;
		default: throw("Syntax Error");
	}
	return true;
}

bool JSONval::write(JSONwriter &w, const char *id) {
	switch (type) {
		case BOOL:		w.Write(id, b); break;
		case INT:		w.Write(id, i); break;
		case FLOAT:		w.Write(id, f); break;
		case STRING:	w.Write(id, s); break;

		case ARRAY:
			w.Begin(id, false);
			for (auto &i : a)
				i.write(w, 0);
			w.End();
			break;

		case OBJECT:
			w.Begin(id, true);
			for (auto &i : o)
				i.b.write(w, i.a);
			w.End();
			break;
	}
	return true;
}

string_accum& iso::operator<<(string_accum& a, const pair<string,JSONval>& v) {
	return a << v.a << ':';// << v.b;
}

string_accum& iso::operator<<(string_accum& a, const JSONval& v) {
	switch (v.type) {
		case JSONval::BOOL:		return a << v.b;
		case JSONval::INT:		return a << v.i;
		case JSONval::FLOAT:	return a << v.f;
		case JSONval::STRING:	return a << v.s;
		case JSONval::ARRAY:	return a << '[' << separated_list(v.a) << ']';
		case JSONval::OBJECT:	return a << '[' << separated_list(v.o) << ']';
		default:	return a << '?';
	}
}
