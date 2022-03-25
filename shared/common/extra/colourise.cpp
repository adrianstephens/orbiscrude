#include "colourise.h"
#include "base/algorithm.h"

using namespace iso;

//<character set>	 (\ansi | \mac | \pc | \pca)? \ansicpgN?
//		RTFblock	rtf(b, "rtf1\\ansi\\fdeff0\\deftab", tabs);
//	RTFblock	r(b, "colortbl;");
//	b << "\\red0\\green0\\blue0;";
//	for (const win::Colour	*i = colours, *e = i + num_cols; i < e; ++i)
//		b.format("\\red%i\\green%i\\blue%i;", i->r, i->g, i->b);
//	b << "\\red128\\green128\\blue128;";
//	for (const win::Colour	*i = colours, *e = i + num_cols; i < e; ++i)
//		b.format("\\red%i\\green%i\\blue%i;", (i->r + 255) / 2,(i->g + 255) / 2,(i->b + 255) / 2);
//						b << "\\line";

//	ucN
//	uN

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

string_accum &SyntaxColourer::RTFcolours(string_accum &b) const {
	uint32		num_cols	= num_tables + KEYWORDS;

	RTFblock	r(b, "colortbl;");
	b << "\\red0\\green0\\blue0;";
	for (const win::Colour	*i = colours, *e = i + num_cols; i < e; ++i)
		b.format("\\red%i\\green%i\\blue%i;", i->r, i->g, i->b);
	b << "\\red128\\green128\\blue128;";
	for (const win::Colour	*i = colours, *e = i + num_cols; i < e; ++i)
		b.format("\\red%i\\green%i\\blue%i;", (i->r + 255) / 2,(i->g + 255) / 2,(i->b + 255) / 2);
	return b;
}

string SyntaxColourer::RTF(const memory_block &text, int *active, size_t num_active) {
	string_builder	b;
	RTFblock	rtf(b, "rtf1\\ansi\\deff0\\deftab", tabstop);
	RTFcolours(b);

	size_t		line		= 1;
	bool		used		= !active;

	string_scan	s(text, text.end());
	while (s.remaining()) {
		//const char *start	= s.getp();
		const char *start;
		TYPE	t			= GetNext(s.skip_whitespace(), &start);
		b.format("{\\cf%i ", ColourIndex(t, used));

		for (const char *i = start, *e = s.getp(); i < e; ++i) {
			switch (*i) {
				case '{': case '}': case '\\':	// escape these
					b << '\\';
					break;
				case '\n': {
					++line;
					if (s.skip_whitespace().check("#line"))
						s >> line;
					b << "\\line";
					if (active) {
						bool	used0 = used;
						if (used = num_active && line == *active) {
							++active;
							--num_active;
						}
						if (used != used0)
							b.format("}{\\cf%i ", ColourIndex(t, used));
					}
					break;
				}
			}
			b << *i;
		}
		//b.add(start, s.getp() - start);
		b << "}";
	}
	return b;
}

void SyntaxColourer::Process(win::RichEditControl &text, int *active, size_t num_active) const {
	text.SetSelection(win::CharRange::all());
	win::RichEditControl::Paragraph().Tabs(tabstop).Set(text);

	string	buffer = text.GetText(win::CharRange::all());
	if (!buffer)
		return;

	for (char *p = buffer; *p; ++p) {
		if (*p == 0x0d)
			*p = '\n';
	}

	uint32		prev_s	= 0;
	TYPE		prev_t	= NONE;
	size_t		line	= 1;
	bool		used	= !active;

	string_scan	s(buffer);
	while (s.remaining()) {
		uint32	prev_e	= s.getp() - buffer;
		char	c		= s.getc();
		if (c == '\n') {
			++line;
			if (s.skip_whitespace().check("#line"))
				s >> line;
			if (active) {
				if (used = num_active && line == *active) {
					++active;
					--num_active;
				}
			}
		}
		if (is_whitespace(c))
			continue;

		const char *start;
		TYPE	next_t	= GetNext(s.move(-1), &start);
		uint32	next_s	= start - buffer;

		if (next_t != prev_t || next_s != prev_e) {
			text.SetSelection(win::CharRange(prev_s, prev_e));
			text.SetFormat(win::CharFormat().Colour(Colour(prev_t, used)));
			prev_s	= next_s;
			prev_t	= next_t;
		}
	}

	text.SetSelection(win::CharRange(prev_s, s.getp() - buffer));
	text.SetFormat(win::CharFormat().Colour(Colour(prev_t, used)));
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
