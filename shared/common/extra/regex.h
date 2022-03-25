#ifndef REGEX_H
#define REGEX_H

#include "base/defs.h"
#include "base/array.h"
#include "base/strings.h"
#include "base/interval.h"

namespace iso {

struct PosixGroup {
	const char		*name;
	range<const interval<char16>*>	r16;
};

const PosixGroup* LookupPosixGroup(const count_string& name);

}

//	SPECIAL PATTERN CHARACTERS
//	.				not newline				any character except line terminators (LF, CR, LS, PS)
//	\t				tab						horizontal tab character (same as \u0009)
//	\n				newline					newline (line feed) character (same as \u000A)
//	\v				vertical tab			vertical tab character (same as \u000B)
//	\f				form feed				form feed character (same as \u000C)
//	\r				carriage return			carriage return character (same as \u000D)
//	\cletter		control code			control code character whose code unit value is the same as the remainder of dividing the code unit value of letter by 32
//	\xhh			ASCII character			character whose code unit value has an hex value equivalent to the two hex digits hh
//	\uhhhh			unicode character		character whose code unit value has an hex value equivalent to the four hex digits hhhh
//	\0				null					null character (same as \u0000)
//	\int			backreference			result of the submatch whose opening parenthesis is the int-th (int shall begin by a digit other than 0). See groups below for more info.
//	\d				digit					decimal digit character (same as [[:digit:]]).
//	\D				not digit				any character that is not a decimal digit character
//	\s				whitespace				whitespace character
//	\S				not whitespace			any character that is not a whitespace character
//	\w				word					alphanumeric or underscore character
//	\W				not word				any character that is not an alphanumeric or underscore character
//	\character		character				character as it is, without interpreting its special meaning within a regex expression. Needed for: ^ $ \ . * + ? ( ) [ ] { } |
//	[class]			character class			the target character is part of the class
//	[^class]		negated character class	the target character is not part of the class
//	|				separator				separates two alternative patterns or subpatterns
//
//	QUANTIFIERS
//	*				0 or more				preceding atom is matched 0 or more times
//	+				1 or more				preceding atom is matched 1 or more times
//	?				0 or 1					preceding atom is optional (matched either 0 times or once)
//	{int}			int						preceding atom is matched exactly int times
//	{int,}			int or more				preceding atom is matched int or more times
//	{min,max}		between min and max		preceding atom is matched at least min times, but not more than max
//
//	GROUPS
//	(subpattern)	Group					creates a backreference
//	(?:subpattern)	Passive group			does not create a backreference
//
//	ASSERTIONS
//	^				beginning of line		either it is the beginning of the target sequence, or follows a line terminator
//	$				end of line				either it is the end of the target sequence, or precedes a line terminator
//	\b				word boundary			the previous character is a word character and the next is a non-word character (or vice-versa)
//	\B				not a word boundary		the previous and next characters are both word characters or both are non-word characters
//	(?=subpattern)	positive lookahead		the characters following the assertion must match subpattern, but no characters are consumed
//	(?!subpattern)	negative lookahead		the characters following the assertion must not match subpattern, but no characters are consumed
//
//	CHARACTER CLASSES
//	[abc]			matches a, b or c
//	[^xyz]			matches any character except x, y and z
//	[a-z]			matches any lowercase letter (a, b, c, ... z)
//
//	[:classname:]	character class			regex traits' isctype member with the appropriate type from lookup_classname member(classname)
//	[.classname.]	collating sequence		regex traits' lookup_collatename to interpret classname
//	[=classname=]	character equivalents	regex traits' transform_primary of the result of regex_traits::lookup_collatename for classname to check for matches
//
//	[:alnum:]		alpha-numerical character
//	[:alpha:]		alphabetic character
//	[:blank:]		blank character
//	[:cntrl:]		control character
//	[:digit:]		decimal digit character
//	[:graph:]		has graphical representation
//	[:lower:]		lowercase letter
//	[:print:]		printable character
//	[:punct:]		punctuation mark character
//	[:space:]		whitespace character
//	[:upper:]		uppercase letter
//	[:xdigit:]		hexadecimal digit character
//	[:d:]			decimal digit character
//	[:w:]			word character
//	[:s:]			whitespace character
//
//	EXTENSIONS
//	\A				beginning of line
//	\z				end of line
//	\<				beginning of word
//	\>				end of word
//  (?P<name>expr)	named capture
//  (?'name'expr)	named capture
//  (?<name>expr)	named capture
//	flag edits - (?i) (?-i) (?i: )
//		^			reset flags
//		i			FoldCase
//		m			!OneLine
//		n			NeverCapture
//		s			DotNL
//		U			NonGreedy
//
//	REPLACE
//	$n				nth backreference (i.e., a copy of the nth matched group specified with parentheses in the regex pattern)
//	$&				a copy of the entire match
//	$`				the prefix (i.e., the part of the target sequence that precedes the match)
//	$´				the suffix (i.e., the part of the target sequence that follows the match)
//	$$				a single $ character

namespace iso { namespace re2 {

class Regexp;
class Prog;

//-----------------------------------------------------------------------------
//	regex
//-----------------------------------------------------------------------------

enum empty_flags {
	word_boundary 		= 1 << 0,		// \b - word boundary
	nonword_boundary	= 1 << 1,		// \B - not \b
	begin_text 			= 1 << 2,		// \A - beginning of text
	end_text 			= 1 << 3,		// \z - end of text
	begin_line 			= 1 << 4,		// ^  - beginning of line
	end_line 			= 1 << 5,		// $  - end of line
	begin_word 			= 1 << 6,		// \< - beginning of word
	end_word 			= 1 << 7,		// \> - end of word
};

enum match_flags {
	match_default		= 0,		// Default					Default matching behavior
	match_not_bol		= 1 << 0,	// Not Beginning-Of-Line	The first character is not considered a beginning of line ("^" does not match)
	match_not_eol		= 1 << 1,	// Not End-Of-Line			The last character is not considered an end of line ("$" does not match)
	match_not_bow		= 1 << 2,	// Not Beginning-Of-Word	The escape sequence "\b" does not match as a beginning-of-word
	match_not_eow		= 1 << 3,	// Not End-Of-Word			The escape sequence "\b" does not match as an end-of-word
	match_any			= 1 << 4,	// Any match				Any match is acceptable if more than one match is possible
	match_not_null		= 1 << 5,	// Not null					Empty sequences do not match
	match_continuous	= 1 << 6,	// Continuous				The expression must match a sub-sequence that begins at the first character. Sub-sequences must begin at the first character to match
	match_prev_avail	= 1 << 7,	// Previous Available		One or more characters exist before the first one. (match_not_bol and match_not_bow are ignored)
	format_default		= 0,		// Default formatting		Uses the standard formatting rules to replace matches (those used by ECMAScript's replace method)
//	format_sed			= 1 << 8,	// sed formatting			Uses the same rules as the sed utility in POSIX to replace matches
	format_no_copy		= 1 << 9,	// No copy					The sections in the target sequence that do not match the regular expression are not copied when replacing matches
	format_first_only	= 1 << 10,	// First only				Only the first occurrence of a regular expression is replaced

	// Encoding
	_encoding		= 1 << 16,
	encoding_ascii	= _encoding * 0,
	encoding_utf8	= _encoding * 1,
	encoding_wchar	= _encoding * 2,
};
inline match_flags	operator|(match_flags a, match_flags b)		{ return match_flags((int)a | (int)b); }
inline match_flags&	operator|=(match_flags &a, match_flags b)	{ return a = match_flags((int)a | (int)b); }

enum syntax_flags {
	ECMAScript		= 0,		// ECMAScript grammar
	basic,						// Basic POSIX grammar
	extended,					// Extended POSIX grammar
	awk,						// Awk POSIX grammar
	grep,						// Grep POSIX grammar
	egrep,						// Egrep POSIX grammar
	literal,					// just text

	icase			= 1 << 3,	// Case insensitive			Regular expressions match without regard to case
	nosubs			= 1 << 4,	// No sub-expressions		Sub-expressions are not considered to be marked
	optimize		= 1 << 5,	// Optimize matching		Matching efficiency is preferred over efficiency constructing regex objects
	collate			= 1 << 6,	// Locale sensitiveness		Character ranges, like "[a-b]", are affected by locale
	oneline			= 1 << 7,	// Treat ^ and $ as only matching at beginning and end of text, not around embedded newlines
	nongreedy 		= 1 << 8,	// Repetition operators are non-greedy by default

	syntax_default	= ECMAScript,
};
inline syntax_flags	operator|(syntax_flags a, syntax_flags b)	{ return syntax_flags((int)a | (int)b); }

class regex {
	Regexp			*re;
	mutable Prog	*prog;
	bool	check_prog(match_flags flags) const;
	static Regexp*	init(const char *begin, const char *end, syntax_flags syntax);
	static Regexp*	init(const char16 *begin, const char16 *end, syntax_flags syntax);
	static Regexp*	literal(range<const char*> chars);
	static Regexp*	empty(empty_flags e);
public:
	regex(const string_param& pattern, syntax_flags syntax = syntax_default) : re(init(pattern.begin(), pattern.end(), syntax)), prog(0) {}
	regex(regex&& b) : re(b.re), prog(b.prog) { b.re = 0; b.prog = 0; }
	regex(const char_set &set, syntax_flags syntax = syntax_default);
	regex(Regexp *re = nullptr) : re(re), prog(0)	{}
	template<int N> regex(const char (&text)[N]) : re(literal(make_range_n(text, N - 1))), prog(0) {}
	regex(empty_flags e) : re(empty(e)), prog(0) {}
	~regex();

	regex& operator=(regex&& b) {
		swap(*this, b);
		return *this;
	}

	explicit operator bool() const { return !!re; }

	bool	match(const count_string& text, match_flags flags = match_default)	const;
	bool	search(const count_string& text, match_flags flags = match_default)	const;
	bool	match(const count_string& text, dynamic_array<count_string> &matches, match_flags flags = match_default)	const;
	bool	search(const count_string& text, dynamic_array<count_string> &matches, match_flags flags = match_default)	const;
	string	replace(const count_string& text, const char *repl, match_flags flags = match_default)	const;

	friend regex operator+(const regex &a, const regex &b);
	friend regex operator|(const regex &a, const regex &b);
	friend regex operator*(const regex &a);
	friend regex operator+(const regex &a);
	friend regex operator*(const regex &a, int n);
	
	friend regex group(const regex &a);
	friend regex maybe(const regex &a);
	friend regex between(const regex &a, int min, int max);
	friend regex repeat(const regex &a);

	friend void swap(regex &a, regex &b) { using iso::swap; swap(a.re, b.re); swap(a.prog, b.prog); }
};

//-----------------------------------------------------------------------------
//	regex_set
//-----------------------------------------------------------------------------

class regex_set {
	syntax_flags	syntax;
	mutable Prog	*prog;
	dynamic_array<Regexp*> res;
	bool	check_prog(match_flags flags) const;
public:
	regex_set(syntax_flags syntax = syntax_default) : syntax(syntax), prog(0) {}
	regex_set(regex_set &&b) = default;
	~regex_set();

	int32	add(Regexp *re, int32 match_id);
	int32	add(Regexp *re)	{ return add(re, res.size32() + 1); }
	int32	add(const string_param& pattern, int32 match_id);
	int32	add(const string_param& pattern);
	int32	match(const count_string& text, match_flags flags = match_default)	const;
	int32	search(const count_string& text, match_flags flags = match_default) const;
	int32	match(const count_string& text, dynamic_array<count_string> &matches, match_flags flags = match_default)	const;
	int32	search(const count_string& text, dynamic_array<count_string> &matches, match_flags flags = match_default)	const;
	string	replace(const count_string& text, const range<const char**> &repl, match_flags flags = match_default)		const;
};

} } // namespace iso::re2

inline auto operator"" _re(const char *p, size_t len) {
	return iso::re2::regex(p);
}


#endif // REGEX_H
