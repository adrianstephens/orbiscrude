#include "colourise.h"
#include "base/algorithm.h"

using namespace iso;

/*
struct RTFblock {
	string_accum &b;
	RTFblock(string_accum &b)								: b(b)	{ b << "{\\"; }
	RTFblock(string_accum &b, const char *name)				: b(b)	{ b << "{\\" << name << ' '; }
	RTFblock(string_accum &b, const char *name, int value)	: b(b)	{ b << "{\\" << name << value << ' '; }
	~RTFblock()		{ b << '}'; }
	operator string_accum&() const { return b; }
	void		Escape(const char *i, const char *e);
};

void RTFblock::Escape(const char *i, const char *e) {
	for (; i < e; ++i) {
		switch (*i) {
			case '{': case '}': case '\\':	// escape these
				b << '\\';
				break;
		}
		b << *i;
	}
}
*/

void SyntaxColourer::Process(win::RichEditControl& text, range<const int*> active) const {
	text.SetSelection(win::CharRange::all());
	win::RichEditControl::Paragraph().Tabs(tabstop).Set(text);

	string	buffer = text.GetText(win::CharRange::all());
	replace(buffer, 0x0d, '\n');

	RichEditColourer	col(text);
	Process(col, buffer.data(), active);
	text.SetSelection(win::CharRange::begin());
}

SyntaxColourer::TYPE SyntaxColourerSS::GetNext(string_scan &s, const char **start) const {
	while (s.skip_whitespace().remaining()) {
		*start = s.getp();
		char	c	= s.getc();
		switch (c) {
			case '/':
				if (s.peekc() == '/') {
					s.scan('\n');
					return COMMENT;
				} else if (s.peekc() == '*') {
					while (s.scan('*') && s.getc() == '*' && s.getc() != '/');
					return COMMENT;
				}
				break;

			case '"':
			case '\'':
				if (s.scan(c))
					s.move(1);
				return STRING;

			case '#':
				s.get_token(tokspec);
				return PREPROCESS;

			default:
				if (is_alphanum(c)) {
					count_string	token = s.move(-1).get_token(tokspec);
					while (is_digit(token.end()[-1]))
						token = token.slice(0, -1);
					for (int i = 0; i < num_tables; i++) {
						if (find(keywords[i], token) != end(keywords[i]))
							return TYPE(i + KEYWORDS);
					}
				}
				break;
		}
	}
	return NONE;
}

SyntaxColourer::TYPE SyntaxColourerRE::GetNext(string_scan &s, const char **start) const {
	using namespace re2;

	dynamic_array<count_string>		matches;
	auto	flags	= match_default;
	if (int id = search(s.remainder(), matches, match_prev_avail)) {
		*start = matches[0].begin();
		s.move(matches[0].end() - s.getp());
		return (TYPE)id;
	}
	*start = s.getp();
	s.move(uint32(s.remaining()));
	return NONE;
}
