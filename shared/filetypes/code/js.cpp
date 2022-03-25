#include "iso/iso_files.h"
#include "extra/text_stream.h"
#include "extra/unicode_tables.h"

//-----------------------------------------------------------------------------
//	JavaScript
//-----------------------------------------------------------------------------

using namespace iso;

class javascript_tokeniser : public text_mode_reader<istream_ref> {
	static const char* keywords[];
	static const char* future_keywords[];

	enum TOKEN {
		TOK_EOF					= -1,

		TOK_QUERY				= '?',
		TOK_LOGICAL_NOT			= '!',
		TOK_MOD					= '%',
		TOK_MINUS				= '-',
		TOK_PLUS				= '+',
		TOK_DOT					= '.',
		TOK_COLON				= ':',
		TOK_SEMICOLON			= ';',
		TOK_NOT					= '~',
		TOK_EQUALS				= '=',
		TOK_LESS				= '<',
		TOK_GREATER				= '>',
		TOK_COMMA				= ',',
		TOK_MUL					= '*',
		TOK_DIV					= '/',
		TOK_AND					= '&',
		TOK_OR					= '|',
		TOK_XOR					= '^',
		TOK_OPEN_BRACE			= '{',
		TOK_CLOSE_BRACE			= '}',
		TOK_OPEN_BRACKET		= '[',
		TOK_CLOSE_BRACKET		= ']',
		TOK_OPEN_PARENTHESIS	= '(',
		TOK_CLOSE_PARENTHESIS	= ')',

		TOK_IDENTIFIER			= 256,
		TOK_NUMBER,
		TOK_STRINGLITERAL,
		TOK_REGEXP,

		TOK_EQ,
		TOK_EQQ,
		TOK_NEQ,
		TOK_NEQQ,
		TOK_LE,
		TOK_GE,
		TOK_LSHIFT,
		TOK_RSHIFT,
		TOK_ARSHIFT,
		TOK_INC,
		TOK_DEC,
		TOK_LOGICAL_AND,
		TOK_LOGICAL_OR,
		TOK_PLUS_EQ,
		TOK_MINUS_EQ,
		TOK_MUL_EQ,
		TOK_MOD_EQ,
		TOK_DIV_EQ,
		TOK_AND_EQ,
		TOK_OR_EQ,
		TOK_XOR_EQ,
		TOK_LSHIFT_EQ,
		TOK_RSHIFT_EQ,
		TOK_ARSHIFT_EQ,

		_TOK_KEYWORDS,

		TOK_BREAK				= _TOK_KEYWORDS,
		TOK_DO,
		TOK_INSTANCEOF,
		TOK_TYPEOF,
		TOK_CASE,
		TOK_ELSE,
		TOK_NEW,
		TOK_VAR,
		TOK_CATCH,
		TOK_FINALLY,
		TOK_RETURN,
		TOK_VOID,
		TOK_CONTINUE,
		TOK_FOR,
		TOK_SWITCH,
		TOK_WHILE,
		TOK_DEBUGGER,
		TOK_FUNCTION,
		TOK_THIS,
		TOK_WITH,
		TOK_DEFAULT,
		TOK_IF,
		TOK_THROW,
		TOK_DELETE,
		TOK_IN,
		TOK_TRY,

		TOK_NULL,
		TOK_FALSE,
		TOK_TRUE,

		TOK_FUTUREKEYWORD,
	};
	bool		strict;
	string		identifier;

	TOKEN		GetToken();
};

const char* javascript_tokeniser::keywords[] = {
	"break",	"do",		"instanceof","typeof",	"case",		"else",		"new",		"var",
	"catch",	"finally",	"return",	"void",		"continue",	"for",		"switch",	"while",
	"debugger",	"function",	"this",		"with",		"default",	"if",		"throw",	"delete",
	"in",		"try",
	"null",
	"false",	"true",
};
const char* javascript_tokeniser::future_keywords[] = {
	"class",	"enum",		"extends",	"super",	"const",	"export",	"import",
	"implements","let",		"private",	"public",	"yield",	"interface","package",	"protected","static",
};

//	7.5 Tokens
//	IdentifierName Punctuator NumericLiteral StringLiteral
javascript_tokeniser::TOKEN javascript_tokeniser::GetToken() {
	bool	line_terminator = false;
	for (;;) switch (int c = getc()) {
		case unicode::TAB:
		case unicode::VT:
		case unicode::FF:
		case unicode::SP:
		case unicode::NBSP:
		case unicode::BOM:
			line_terminator = false;
			continue;

		case unicode::LF:
		case unicode::CR:
		case unicode::LS:
		case unicode::PS:
			line_terminator = true;
			continue;

//	7.6 Identifier Names and Identifiers
//	IdentifierName:				IdentifierStart IdentifierName IdentifierPart
//	IdentifierStart:			UnicodeLetter $ _ \ UnicodeEscapeSequence
//	IdentifierPart:				IdentifierStart UnicodeCombiningMark UnicodeDigit UnicodeConnectorPunctuation <ZWNJ> <ZWJ>
//	UnicodeLetter:				any character in the Unicode categories --Uppercase letter (Lu), --Lowercase letter (Ll), --Titlecase letter (Lt), --Modifier letter (Lm), --Other letter (Lo), or --Letter number (Nl).
//	UnicodeCombiningMark:		any character in the Unicode categories --Non-spacing mark (Mn) or --Combining spacing mark (Mc)
//	UnicodeDigit:				any character in the Unicode category --Decimal number (Nd)
//	UnicodeConnectorPunctuation:any character in the Unicode category --Connector punctuation (Pc)
//	UnicodeEscapeSequence:		u HexDigit HexDigit HexDigit HexDigit
		case '\\':case '_': case '$':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
		case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
		case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
		case 'y': case 'z':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
		case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
		case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z': {
			auto	b = build(identifier);
			do {
				if (c == '\\') {
					if ((c = getc()) == 'u') {
						c = 0;
						for (int i = 0; i < 4; i++) {
							int	c2 = getc();
							if (!is_hex(c2)) {
								put_back(c2);
								break;
							}
							c = c * 16 + from_digit(c2);
						}
					}
				}
				b << char32(c);
				c = getc();
			} while (c == '\\' || c == '_' || c == '$' || is_any(unicode::category(c), unicode::Ll, unicode::Lu, unicode::Nd, unicode::Pc));

			put_back(c);

//	7.6.1 Reserved Words
//	ReservedWord:				Keyword FutureReservedWord FutureReservedWordStrict NullLiteral BooleanLiteral
			for (int i = 0; i < num_elements(keywords); i++) {
				if (identifier == keywords[i])
					return TOKEN(_TOK_KEYWORDS + i);
			}
			for (int i = 0, n = strict ? int(num_elements(future_keywords)) : 7; i < n; i++) {
				if (identifier == future_keywords[i])
					return TOK_FUTUREKEYWORD;
			}
			return TOK_IDENTIFIER;
		}

//	7.8 Literals

//	NumericLiteral:				DecimalLiteral HexIntegerLiteral
//	DecimalLiteral:				DecimalIntegerLiteral . DecimalDigitsopt ExponentPartopt . DecimalDigits ExponentPartopt DecimalIntegerLiteral ExponentPartopt
//	DecimalIntegerLiteral:		0 NonZeroDigit DecimalDigitsopt
//	DecimalDigits:				DecimalDigit DecimalDigits DecimalDigit
//	DecimalDigit:				one of	0 1 2 3 4 5 6 7 8 9
//	NonZeroDigit:				one of	1 2 3 4 5 6 7 8 9
//	HexDigit:					one of	0 1 2 3 4 5 6 7 8 9 a b c d e f A B C D E F
//	ExponentPart:				ExponentIndicator SignedInteger
//	ExponentIndicator:			one of	e E
//	SignedInteger:				DecimalDigits + DecimalDigits - DecimalDigits
//	HexIntegerLiteral:			0x HexDigit 0X HexDigit HexIntegerLiteral HexDigit
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			put_back(c);
			return TOK_NUMBER;

//	StringLiteral:				" DoubleStringCharactersopt " ' SingleStringCharactersopt '
//	DoubleStringCharacters:		DoubleStringCharacter DoubleStringCharactersopt
//	SingleStringCharacters:		SingleStringCharacter SingleStringCharactersopt
//	DoubleStringCharacter:		SourceCharacter but not one of " or \ or LineTerminator \ EscapeSequence LineContinuation
//	SingleStringCharacter:		SourceCharacter but not one of ' or \ or LineTerminator \ EscapeSequence LineContinuation
//	LineContinuation:			\ LineTerminatorSequence
//	EscapeSequence:				CharacterEscapeSequence 0 [lookahead ? DecimalDigit] HexEscapeSequence UnicodeEscapeSequence
//	CharacterEscapeSequence:	SingleEscapeCharacter NonEscapeCharacter
//	SingleEscapeCharacter:		one of	' " \ b f n r t v
//	NonEscapeCharacter:			SourceCharacter but not one of EscapeCharacter or LineTerminator
//	EscapeCharacter:			SingleEscapeCharacter DecimalDigit x u
//	HexEscapeSequence:			x HexDigit HexDigit
//	UnicodeEscapeSequence:		u HexDigit HexDigit HexDigit HexDigit
		case '\'':	case '"': {
			auto	b	= build(identifier);
			int		c2	= getc();
			while (c2 != EOF && c2 != c) {
				if (c2 == '\\')
					c2 = get_escape(*this);
				b.putc(c2);
				c2		= getc();
			}
			return TOK_STRINGLITERAL;
		}

//	7.7 Punctuators
//	Punctuator:		{	}	(	)	[	]	.	;	,	<	>	<=	>=	==	!=	===
//					!==	+	-	*	%	++	--	<<	>>	>>>	&	|	^	!	~	&&
//					||	?	:	=	+=	-=	*=	%=	<<=	>>=	>>>=	&=	|=	^=
		case '=':
			c = getc();
			if (c == '=') {
				c = getc();
				if (c == '=')
					return TOK_EQQ;
				put_back(c);
				return TOK_EQ;
			}
			put_back(c);
			return TOK_EQUALS;

		case '<':
			c = getc();
			if (c == '<') {
				c = getc();
				if (c == '=')
					return TOK_LSHIFT_EQ;
				put_back(c);
				return TOK_LSHIFT;
			}
			if (c == '=')
				return TOK_LE;
			put_back(c);
			return TOK_LESS;

		case '>':
			c = getc();
			if (c == '>') {
				c = getc();
				if (c == '>') {
					c = getc();
					if (c == '=')
						return TOK_ARSHIFT_EQ;
					put_back(c);
					return TOK_ARSHIFT;
				}
				if (c == '=')
					return TOK_RSHIFT_EQ;
				put_back(c);
				return TOK_RSHIFT;
			}
			if (c == '=')
				return TOK_GE;
			put_back(c);
			return TOK_GREATER;

		case '!':
			c = getc();
			if (c == '=') {
				c = getc();
				if (c == '=')
					return TOK_NEQQ;
				put_back(c);
				return TOK_NEQ;
			}
			put_back(c);
			return TOK_NOT;

		case '&':
			c = getc();
			if (c == '&')
				return TOK_LOGICAL_AND;
			if (c == '=')
				return TOK_AND_EQ;
			put_back(c);
			return TOK_NOT;

		case '|':
			c = getc();
			if (c == '|')
				return TOK_LOGICAL_OR;
			if (c == '=')
				return TOK_OR_EQ;
			put_back(c);
			return TOK_OR;

		case '^':
			c = getc();
			if (c == '=')
				return TOK_XOR_EQ;
			put_back(c);
			return TOK_XOR;

		case '+':
			c = getc();
			if (c == '=')
				return TOK_PLUS_EQ;
			put_back(c);
			return TOK_PLUS;

		case '-':
			c = getc();
			if (c == '=')
				return TOK_MINUS_EQ;
			put_back(c);
			return TOK_MINUS;

		case '*':
			c = getc();
			if (c == '=')
				return TOK_PLUS_EQ;
			put_back(c);
			return TOK_PLUS;

		case '%':
			c = getc();
			if (c == '=')
				return TOK_MOD_EQ;
			put_back(c);
			return TOK_MOD;

//	DivPunctuator:						one of	/	/=
//	RegularExpressionLiteral:			/ RegularExpressionBody / RegularExpressionFlags
//	RegularExpressionBody:				RegularExpressionFirstChar RegularExpressionChars
//	RegularExpressionChars:				[empty] RegularExpressionChars RegularExpressionChar
//	RegularExpressionFirstChar:			RegularExpressionNonTerminator but not one of * or \ or / or [ RegularExpressionBackslashSequence RegularExpressionClass
//	RegularExpressionChar:				RegularExpressionNonTerminator but not one of \ or / or [ RegularExpressionBackslashSequence RegularExpressionClass
//	RegularExpressionBackslashSequence:	\ RegularExpressionNonTerminator
//	RegularExpressionNonTerminator:		SourceCharacter but not LineTerminator
//	RegularExpressionClass:				[ RegularExpressionClassChars ]
//	RegularExpressionClassChars:		[empty] RegularExpressionClassChars RegularExpressionClassChar
//	RegularExpressionClassChar:			RegularExpressionNonTerminator but not one of ] or \ RegularExpressionBackslashSequence
//	RegularExpressionFlags:				[empty] RegularExpressionFlags IdentifierPart

//	7.4 Comments
//	MultiLineComment:					/* MultiLineCommentCharsopt */
//	MultiLineCommentChars:				MultiLineNotAsteriskChar MultiLineCommentCharsopt * PostAsteriskCommentCharsopt
//	PostAsteriskCommentChars:			MultiLineNotForwardSlashOrAsteriskChar MultiLineCommentCharsopt * PostAsteriskCommentCharsopt
//	MultiLineNotAsteriskChar:			SourceCharacter but not *
//	MultiLineNotForwardSlashOrAsteriskChar:	SourceCharacter but not one of / or *
//	SingleLineComment:					// SingleLineCommentCharsopt
//	SingleLineCommentChars:				SingleLineCommentChar SingleLineCommentCharsopt
//	SingleLineCommentChar:				SourceCharacter but not LineTerminator
		case '/':
			c = getc();
			if (c == '/') {
				do c = getc(); while (c != '\n' && c != EOF);
			} else if (c == '*') {
				do {
					do c = getc(); while (c != '*' && c != EOF);
				} while ((c = getc()) != '/' && c != EOF);
			} else {
				if (c == '=')
					return TOK_DIV_EQ;
				put_back(c);
				return TOK_DIV;
				//return TOK_REGEXP;
			}
			continue;

		default:
			return (TOKEN)c;
	}

}
/*
11.1 Primary Expressions
PrimaryExpression:			this
							Identifier
							Literal
							ArrayLiteral
							ObjectLiteral
							( Expression )

11.1.4 Array Initialiser
ArrayLiteral:				[ Elisionopt ]
							[ ElementList ]
							[ ElementList , Elisionopt ]
ElementList:				Elision	AssignmentExpression
							ElementList , Elision AssignmentExpression
Elision :					,
							Elision ,

11.1.5 Object Initialiser
ObjectLiteral:				{ }
							{ PropertyNameAndValueList }
							{ PropertyNameAndValueList , }
PropertyNameAndValueList:	PropertyAssignment
							PropertyNameAndValueList , PropertyAssignment
PropertyAssignment:			PropertyName : AssignmentExpression
							get PropertyName ( ) { FunctionBody }
							set PropertyName ( PropertySetParameterList ) { FunctionBody }
PropertyName:				IdentifierName
							StringLiteral
							NumericLiteral
PropertySetParameterList:	Identifier

11.2 Left-Hand-Side			Expressions
MemberExpression:			PrimaryExpression
							FunctionExpression
							MemberExpression [ Expression ]
							MemberExpression . IdentifierName
							new MemberExpression Arguments
NewExpression:				MemberExpression
							new NewExpression
CallExpression:				MemberExpression Arguments
							CallExpression Arguments
							CallExpression [ Expression ]
							CallExpression . IdentifierName
Arguments:					( )
							( ArgumentList )
ArgumentList:				AssignmentExpression
							ArgumentList , AssignmentExpression
LeftHandSideExpression:		NewExpression
							CallExpression

11.3 Postfix Expressions
PostfixExpression:			LeftHandSideExpression
							LeftHandSideExpression [no LineTerminator here] ++
							LeftHandSideExpression [no LineTerminator here] --

11.4 Unary Operators
UnaryExpression:			PostfixExpression
							delete UnaryExpression
							void UnaryExpression
							typeof UnaryExpression
							++ UnaryExpression
							-- UnaryExpression
							+ UnaryExpression
							- UnaryExpression
							~ UnaryExpression
							! UnaryExpression

11.5 Multiplicative Operators
MultiplicativeExpression:	UnaryExpression
							MultiplicativeExpression * UnaryExpression
							MultiplicativeExpression / UnaryExpression
							MultiplicativeExpression % UnaryExpression

11.7 Bitwise Shift Operators
ShiftExpression:			AdditiveExpression
							ShiftExpression << AdditiveExpression
							ShiftExpression >> AdditiveExpression
							ShiftExpression >>> AdditiveExpression

11.8 Relational Operators
RelationalExpression:		ShiftExpression
							RelationalExpression < ShiftExpression
							RelationalExpression > ShiftExpression
							RelationalExpression <= ShiftExpression
							RelationalExpression >= ShiftExpression
							RelationalExpression instanceof ShiftExpression
							RelationalExpression in ShiftExpression
RelationalExpressionNoIn:	ShiftExpression
							RelationalExpressionNoIn < ShiftExpression
							RelationalExpressionNoIn > ShiftExpression
							RelationalExpressionNoIn <= ShiftExpression
							RelationalExpressionNoIn >= ShiftExpression
							RelationalExpressionNoIn instanceof ShiftExpression

11.10 Binary Bitwise Operators
BitwiseANDExpression:		EqualityExpression
							BitwiseANDExpression & EqualityExpression
BitwiseANDExpressionNoIn:	EqualityExpressionNoIn
							BitwiseANDExpressionNoIn & EqualityExpressionNoIn
BitwiseXORExpression:		BitwiseANDExpression
							BitwiseXORExpression ^ BitwiseANDExpression
BitwiseXORExpressionNoIn:	BitwiseANDExpressionNoIn
							BitwiseXORExpressionNoIn ^ BitwiseANDExpressionNoIn
BitwiseORExpression:		BitwiseXORExpression
							BitwiseORExpression | BitwiseXORExpression
BitwiseORExpressionNoIn:	BitwiseXORExpressionNoIn
							BitwiseORExpressionNoIn | BitwiseXORExpressionNoIn

11.13 Assignment Operators
AssignmentExpression:		ConditionalExpression
							LeftHandSideExpression = AssignmentExpression
							LeftHandSideExpression AssignmentOperator AssignmentExpression
AssignmentExpressionNoIn:	ConditionalExpressionNoIn
							LeftHandSideExpression = AssignmentExpressionNoIn
							LeftHandSideExpression AssignmentOperator AssignmentExpressionNoIn
AssignmentOperator : one of *=	/=	%=	+=	-=	<<=	>>=	>>>=	&=	^=	|=

11.14 Comma Operator ( , )
Expression:					AssignmentExpression
							Expression , AssignmentExpression
ExpressionNoIn:				AssignmentExpressionNoIn
							ExpressionNoIn , AssignmentExpressionNoIn

12 Statements
Statement:					Block
							VariableStatement
							EmptyStatement
							ExpressionStatement
							IfStatement
							IterationStatement
							ContinueStatement
							BreakStatement
							ReturnStatement
							WithStatement
							LabelledStatement
							SwitchStatement
							ThrowStatement
							TryStatement
							DebuggerStatement
12.1 Block
Block:						{ StatementListopt }
StatementList:				Statement
							StatementList Statement

12.2 Variable Statement
VariableStatement:			var VariableDeclarationList ;
VariableDeclarationList:	VariableDeclaration
							VariableDeclarationList , VariableDeclaration
VariableDeclarationListNoIn:VariableDeclarationNoIn
							VariableDeclarationListNoIn , VariableDeclarationNoIn
VariableDeclaration:		Identifier Initialiseropt
VariableDeclarationNoIn:	Identifier InitialiserNoInopt
Initialiser:				= AssignmentExpression
InitialiserNoIn:			= AssignmentExpressionNoIn

12.3 Empty Statement
EmptyStatement:				;

12.4 Expression Statement
ExpressionStatement:		[lookahead notin {{, function}] Expression ;

12.5 The if Statement
IfStatement:				if ( Expression ) Statement else Statement if ( Expression ) Statement

12.6 Iteration Statements
IterationStatement:			do Statement while ( Expression );
							while ( Expression ) Statement
							for (ExpressionNoInopt; Expressionopt ; Expressionopt ) Statement
							for ( var VariableDeclarationListNoIn; Expressionopt ; Expressionopt ) Statement
							for ( LeftHandSideExpression in Expression ) Statement
							for ( var VariableDeclarationNoIn in Expression ) Statement

12.7 The continue Statement
ContinueStatement:			continue ;
							continue [no LineTerminator here] Identifier;

12.8 The break Statement
BreakStatement:				break ;
							break [no LineTerminator here] Identifier ;

12.9 The return Statement
ReturnStatement:			return ;
							return [no LineTerminator here] Expression ;

12.10 The with Statement
WithStatement:				with ( Expression ) Statement

12.11 The switch Statement
SwitchStatement:			switch ( Expression ) CaseBlock
CaseBlock:					{ CaseClausesopt }
							{ CaseClausesopt DefaultClause CaseClausesopt }
CaseClauses:				CaseClause
							CaseClauses CaseClause
CaseClause:					case Expression : StatementListopt
DefaultClause:				default : StatementListopt

12.13 The throw Statement
ThrowStatement:				throw [no LineTerminator here] Expression ;

12.14 The try Statement
TryStatement:				try Block Catch
							try Block Finally
							try Block Catch Finally
Catch:						catch ( Identifier ) Block
Finally:					finally Block

12.15 The debugger statement
DebuggerStatement:			debugger ;

13 Function Definition
FunctionDeclaration:		function Identifier ( FormalParameterListopt ) { FunctionBody }
FunctionExpression:			function Identifieropt ( FormalParameterListopt ) { FunctionBody }
FormalParameterList:		Identifier
							FormalParameterList , Identifier
FunctionBody:				SourceElementsopt

14 Program
Program:					SourceElementsopt
SourceElements:				SourceElement
							SourceElements SourceElement
SourceElement:				Statement
							FunctionDeclaration
*/
//-----------------------------------------------------------------------------
//	JSFileHandler
//-----------------------------------------------------------------------------
/*
class JSFileHandler : public FileHandler {
	const char*		GetExt() override { return "js";	}
	const char*		GetDescription() override { return "javascript file";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} js;

*/

