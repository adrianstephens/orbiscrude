#ifndef COLOURISE_H
#define COLOURISE_H

#include "base/defs.h"
#include "windows/window.h"
#include "extra/regex.h"

namespace iso {

struct RTFblock {
	string_accum &b;
	RTFblock(string_accum &_b) : b(_b)								{ b << "{\\"; }
	RTFblock(string_accum &_b, const char *name) : b(_b)			{ b << "{\\" << name << ' '; }
	RTFblock(string_accum &_b, const char *name, int value) : b(_b)	{ b << "{\\" << name << value << ' '; }
	~RTFblock()		{ b << '}'; }
	operator string_accum&() const { return b; }
	void		Escape(const char *i, const char *e);
};

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

	SyntaxColourer() {}
	SyntaxColourer(uint32 num_tables, const win::Colour *colours, int tabstop, win::Font font) : num_tables(num_tables), colours(colours), font(font), tabstop(tabstop) {}

	virtual	TYPE	GetNext(string_scan &s, const char **start) const = 0;

	int				ColourIndex(int t, bool enabled = true) const {
		return enabled ? t + 1 : t + num_tables + KEYWORDS + 2;
	}
	win::Colour		Colour(int t, bool enabled = true) const {
		win::Colour	col = t ? colours[t - 1] : colour::black;
		if (!enabled) {
			col.r = (col.r + 255) / 2;
			col.g = (col.g + 255) / 2;
			col.b = (col.b + 255) / 2;
		}
		return col;
	}
	string_accum&	RTFcolours(string_accum &b) const;
	string			RTF(const memory_block &text, int *active, size_t num_active);
	void			Process(win::RichEditControl &text, int *active, size_t num_active) const;

};

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

} // namespace iso
#endif	//COLOURISE_H
