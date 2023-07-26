#ifndef UNICODE_TABLES_H
#define UNICODE_TABLES_H

#include "base/strings.h"
#include "base/interval.h"

namespace iso {

//-----------------------------------------------------------------------------
//	unicode
//-----------------------------------------------------------------------------

namespace unicode {

enum {
	TAB		= 0x0009,
	LF		= 0x000A,
	VT		= 0x000B,
	FF		= 0x000C,
	CR		= 0x000D,
	SP		= 0x0020,
	NBSP	= 0x00A0,
	LS		= 0x2028,	//Line separator
	PS		= 0x2029,	//Paragraph separator
	ZWNJ	= 0x200C,
	ZWJ		= 0x200D,
	BOM		= 0xFEFF,
	Runemax = 0x10FFFF,
};

enum Category : uint8 {
	Lu,	//Letter, Uppercase
	Ll,	//Letter, Lowercase
	Lt,	//Letter, Titlecase
	Mn,	//Mark, Non-Spacing
	Mc,	//Mark, Spacing Combining
	Me,	//Mark, Enclosing
	Nd,	//Number, Decimal Digit
	Nl,	//Number, Letter
	No,	//Number, Other
	Zs,	//Separator, Space
	Zl,	//Separator, Line
	Zp,	//Separator, Paragraph
	Cc,	//Other, Control
	Cf,	//Other, Format
	Cs,	//Other, Surrogate
	Co,	//Other, Private Use
	Cn,	//Other, Not Assigned (no characters in the file have this property)

	Lm,	//Letter, Modifier
	Lo,	//Letter, Other
	Pc,	//Punctuation, Connector
	Pd,	//Punctuation, Dash
	Ps,	//Punctuation, Open
	Pe,	//Punctuation, Close
	Pi,	//Punctuation, Initial quote (may behave like Ps or Pe depending on usage)
	Pf,	//Punctuation, Final quote (may behave like Ps or Pe depending on usage)
	Po,	//Punctuation, Other
	Sm,	//Symbol, Math
	Sc,	//Symbol, Currency
	Sk,	//Symbol, Modifier
	So,	//Symbol, Other
};

struct Group {
	range<const interval<char16>*>	r16;
	range<const interval<char32>*>	r32;
};

enum DeltaEnum {
	EvenOdd		= 1,
	EvenOddSkip = 1 << 30,
};
struct CaseFold {
	char32	lo, hi;
	int32	delta;	// or DeltaEnum
};

static constexpr Category char_categories[] = {
//	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f
	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	// 0
	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	Cc,	// 1
	Zs,	Po,	Po,	Po,	Sc,	Po,	Po,	Po,	Ps,	Pe,	Po,	Sm,	Po,	Pd,	Po,	Po,	// 2
	Nd,	Nd,	Nd,	Nd,	Nd,	Nd,	Nd,	Nd,	Nd,	Nd,	Po,	Po,	Sm,	Sm,	Sm,	Po,	// 3
	Po,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	// 4
	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Lu,	Ps,	Po,	Pe,	Sk,	Pc,	// 5
	Sk,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	// 6
	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ll,	Ps,	Sm,	Pe,	Sm,	Cc	// 7
};
	
constexpr Category	category(char c) { return char_categories[c]; }
Category			category(int code);
bool				is_category(Category cat, char32 code);

const CaseFold*		LookupCaseFold(char32 r);
char32				ApplyFold(const CaseFold *f, char32 r);
char32				CycleFoldRune(char32 r);
const Group*		LookupGroup(const count_string& name);
const interval<char32>*	LookupBlock(const count_string& name);
const char*			GetBlock(const char32 code);

} } // namespace iso::unicode

#endif // UNICODE_TABLES_H
