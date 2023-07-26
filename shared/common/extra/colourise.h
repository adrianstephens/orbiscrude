#ifndef COLOURISE_H
#define COLOURISE_H

#include "base/defs.h"
#include "windows/window.h"
#include "extra/regex.h"

namespace iso {

struct SyntaxColourer {
	uint32				num_tables;
	const win::Colour	*colours;
	win::Font			font;
	int					tabstop;

	enum TYPE {
		NONE,
		COMMENT,
		STRING,
		PREPROCESS,
		KEYWORDS,
		USER,
	};

	struct Token {
		const SyntaxColourer	*me;
		uint32	i;
		bool	enabled;
		operator win::Colour()	const { return me->Colour(i, enabled); }
		operator int()			const { return me->ColourIndex(i, enabled); }
	};

	SyntaxColourer() {}
	SyntaxColourer(uint32 num_tables, const win::Colour *colours, int tabstop, win::Font font) : num_tables(num_tables), colours(colours), font(font), tabstop(tabstop) {}

	virtual	TYPE	GetNext(string_scan &s, const char **start) const = 0;

	auto			Colours()	const { return make_range_n(colours, num_tables + KEYWORDS); }

	int				ColourIndex(uint32 t, bool enabled = true) const {
		return enabled ? t + 1 : t + num_tables + KEYWORDS + 2;
	}
	win::Colour		Colour(uint32 t, bool enabled = true) const {
		win::Colour	col = t ? colours[t - 1] : colour::black;
		if (!enabled) {
			col.r = (col.r + 255) / 2;
			col.g = (col.g + 255) / 2;
			col.b = (col.b + 255) / 2;
		}
		return col;
	}
	Token			MakeToken(uint32 t, bool enabled = true) const {
		return {this, t, enabled};
	}

	template<typename C> void	Process(C &sink, const_memory_block buffer, range<const int*> active) const;
	void			Process(win::RichEditControl& text, range<const int*> active) const;
};

template<typename C> void SyntaxColourer::Process(C &sink, const_memory_block buffer, range<const int*> active) const {
	TYPE		prev_t	= NONE;
	size_t		line	= 1;
	bool		used	= !active;

	sink.Begin(this, buffer);

	string_scan	s(buffer, buffer.end());
	while (s.remaining()) {
		auto	prev_e	= s.getp();
		char	c		= s.getc();
		if (c == '\n') {
			++line;
			if (s.skip_whitespace().check("#line"))
				s >> line;
			if (!active.empty()) {
				if (used = line == active.front())
					active.pop_front();
			}
		}
		if (is_whitespace(c))
			continue;

		const char *next_s;
		TYPE	next_t	= GetNext(s.move(-1), &next_s);

		if (next_t != prev_t || next_s != prev_e) {
			sink.AddColourTo(prev_e, MakeToken(prev_t, used));
			if (next_s != prev_e)
				sink.AddColourTo(next_s, MakeToken(NONE, used));
			prev_t	= next_t;
		}
	}

	sink.AddColourTo(s.getp(), MakeToken(prev_t, used));
	sink.End();

}

struct SyntaxColourerSS : SyntaxColourer {
	range<const char**>	*keywords;
	char_set			tokspec;

	SyntaxColourerSS()	{}
	SyntaxColourerSS(uint32 num_tables, range<const char**> *keywords, const win::Colour *colours, int tabstop, win::Font font) : SyntaxColourer(num_tables, colours, tabstop, font), keywords(keywords), tokspec(char_set::alphanum + '_') {}
	TYPE			GetNext(string_scan &s, const char **start) const;
};

struct SyntaxColourerRE : SyntaxColourer, re2::regex_set {
	SyntaxColourerRE() {}
	SyntaxColourerRE(uint32 num_tables, const char **re, const win::Colour *colours, int tabstop, win::Font font) : SyntaxColourer(num_tables, colours, tabstop, font) {
		add("/\\*.*?\\*/|//.*?$");			// comment
		add("\".*?[^\\\\]\"|'.*?[^\\\\]'");	// string
		add("^\\s*#[a-z]+");				// preprocess
		/*
		const char *num_fp	= "\\b[-+]?(0\\.|[1-9][0-9]*\\.?)[0-9]+([eE][-+]?[0-9]+)?\\b";
		const char *num_int	= "\\b[-+]?("
			"0[0-7]*"				//oct
			"|[1-9][0-9]*"			//dec
			"|0[xX][0-9a-fA-F]+"	//hex
			"|0[bB][01]+"			//bin
			")[uU]?(l|L|ll|LL)?[uU]?\\b";
		add(string("(") + num_int + ")|(" + num_fp + ")");
		*/
		for (int i = 0; i < num_tables; i++)
			add(re[i]);
	}

	TYPE	GetNext(string_scan &s, const char **start) const;
};


struct RichEditColourer {
	win::RichEditControl	text;
	const char	*buffer, *prev;

	RichEditColourer(win::RichEditControl text) : text(text) {}

	void	Begin(const SyntaxColourer *col, const char* p) {
		buffer = prev = p;
	}
	void	End() {}

	void	AddColourTo(const char *i, win::Colour c) {
		text.SetSelection(win::CharRange(prev - buffer, i - buffer));
		text.SetFormat(win::CharFormat().Colour(c));
		prev = i;
	}
};

struct RetainColourer : dynamic_array<pair<uint32, win::Colour>> {
	const char	*buffer;
	void	Begin(const SyntaxColourer *col, const char* p) {
		buffer = p;
	}
	void	End() {}
	void	AddColourTo(const char *i, win::Colour c) {
		emplace_back(i - buffer, c);
	}
};

struct RTFColourer {
	string_builder	b;
	const char		*buffer, *prev;

	void	Begin(const SyntaxColourer *col, const char* p) {
		buffer = prev = p;
		b << "{\\rtf1\\ansi\\deff0\\deftab" << col->tabstop << ' ';
		
		b << "{\\colortbl;";
		b << "\\red0\\green0\\blue0;";
		for (auto &i : col->Colours())
			b.format("\\red%i\\green%i\\blue%i;", i.r, i.g, i.b);
		b << "\\red128\\green128\\blue128;";
		for (auto &i : col->Colours())
			b.format("\\red%i\\green%i\\blue%i;", (i.r + 255) / 2, (i.g + 255) / 2, (i.b + 255) / 2);
		b << '}';
	}

	void	End() {
		b << '}';
	}

	void	AddColourTo(const char *i, int c) {
		b.format("{\\cf%i ", c);
		while (prev < i) {
			switch (*prev) {
				case '{': case '}': case '\\':	// escape these
					b << '\\';
					break;
			}
			b << *prev++;
		}
		b << "}";
	}
};


} // namespace iso
#endif	//COLOURISE_H
