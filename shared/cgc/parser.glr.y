%{
#include <stdio.h>
#include <stdlib.h>

#include "cg.h"
#include "compile.h"
#include "printutils.h"
#include "errors.h"

namespace cgc {
int yyparse (CG *cg);
void yyerror(CG *cg, const char *s);
int yylex(YYSTYPE *lvalp, CG *cg);
}

using namespace cgc;
%}

%glr-parser
%param		{CG *cg}

/* Grammar semantic type: */

%union {
	int			sc_token;
	int			sc_int;
	float		sc_fval;
	int			sc_ident;
	spec		sc_specifiers;
	dtype		sc_type;
	Type		*sc_ptype;
	TypeList	*sc_typelist;
	decl		*sc_decl;
	expr		*sc_expr;
	stmt		*sc_stmt;
	attr		*sc_attr;
	Symbol		*sc_sym;
	/* Dummy place holder: */
	int	dummy;
}

%token <sc_token>	AND_SY
%token <sc_token>	ASM_SY
%token <sc_token>	ASSIGNMINUS_SY
%token <sc_token>	ASSIGNMOD_SY
%token <sc_token>	ASSIGNPLUS_SY
%token <sc_token>	ASSIGNSLASH_SY
%token <sc_token>	ASSIGNSTAR_SY
%token <sc_token>	BOOLEAN_SY
%token <sc_token>	BREAK_SY
%token <sc_token>	CASE_SY
%token <sc_fval>	CFLOATCONST_SY
%token <sc_token>	COLONCOLON_SY
%token <sc_token>	CONST_SY
%token <sc_token>	CONTINUE_SY
%token <sc_token>	DEFAULT_SY
%token <sc_token>	DISCARD_SY
%token <sc_token>	DO_SY
%token <sc_token>	EQ_SY
%token <sc_token>	ELSE_SY
%token <sc_token>	ERROR_SY
%token <sc_token>	EXTERN_SY
%token <sc_token>	FLOAT_SY
%token <sc_fval>	FLOATCONST_SY
%token <sc_fval>	FLOATHCONST_SY
%token <sc_fval>	FLOATXCONST_SY
%token <sc_token>	FOR_SY
%token <sc_token>	GE_SY
%token <sc_token>	GG_SY
%token <sc_token>	GOTO_SY
%token <sc_ident>	IDENT_SY
%token <sc_ident>	PASTING_SY
%token <sc_token>	IF_SY
%token <sc_token>	IN_SY
%token <sc_token>	INLINE_SY
%token <sc_token>	INOUT_SY
%token <sc_token>	INT_SY
%token <sc_token>	UNSIGNED_SY
%token <sc_int>		INTCONST_SY
%token <sc_int>		UINTCONST_SY
%token <sc_token>	INTERNAL_SY
%token <sc_token>	LE_SY
%token <sc_token>	LL_SY
%token <sc_token>	MINUSMINUS_SY
%token <sc_token>	NE_SY
%token <sc_token>	OR_SY
%token <sc_token>	OUT_SY
%token <sc_token>	PACKED_SY
%token <sc_token>	PLUSPLUS_SY
%token <sc_token>	RETURN_SY
%token <sc_token>	STATIC_SY
%token <sc_token>	STRCONST_SY
%token <sc_token>	STRUCT_SY
%token <sc_token>	SWITCH_SY
%token <sc_token>	TEXOBJ_SY
%token <sc_token>	THIS_SY
%token <sc_token>	TYPEDEF_SY
%token <sc_ident>	TYPEIDENT_SY
%token <sc_ident>	TEMPLATEIDENT_SY
%token <sc_token>	UNIFORM_SY
%token <sc_token>	VARYING_SY
%token <sc_token>	VOID_SY
%token <sc_token>	WHILE_SY
%token <sc_token>	SAMPLERSTATE_SY
%token <sc_token>	TECHNIQUE_SY
%token <sc_token>	PASS_SY
%token <sc_token>	VERTEXSHADER_SY
%token <sc_token>	PIXELSHADER_SY
%token <sc_token>	COMPILE_SY
%token <sc_token>	ROWMAJOR_SY
%token <sc_token>	COLMAJOR_SY
%token <sc_token>	NOINTERP_SY
%token <sc_token>	PRECISE_SY
%token <sc_token>	SHARED_SY
%token <sc_token>	GROUPSHARED_SY
%token <sc_token>	VOLATILE_SY
%token <sc_token>	REGISTER_SY
%token <sc_token>	ENUM_SY
%token <sc_token>	LOWP_SY
%token <sc_token>	MEDIUMP_SY
%token <sc_token>	HIGHP_SY
%token <sc_token>	CBUFFER_SY
%token <sc_token>	TEMPLATE_SY
%token <sc_token>	OPERATOR_SY

%token <sc_token>	FIRST_USER_TOKEN_SY	/* Must be last token declaration */

%type <dummy> compilation_unit
%type <dummy> compound_header
%type <dummy> compound_tail
%type <dummy> external_declaration
%type <dummy> function_definition
%type <dummy> struct_compound_header
%type <dummy> cbuffer_compound_header
%type <dummy> enum_declaration_list
%type <dummy> enum_declaration

%type <sc_int> function_specifier
%type <sc_int> in_out
%type <sc_int> type_domain
%type <sc_int> type_qualifier
%type <sc_int> storage_class

%type <sc_ident> operator
%type <sc_ident> identifier
%type <sc_ident> member_identifier
%type <sc_ident> scope_identifier
%type <sc_ident> semantics_identifier
%type <sc_ident> struct_identifier
%type <sc_ident> variable_identifier
%type <sc_ident> register_spec

%type <sc_decl> abstract_declaration
%type <sc_decl> abstract_declarator
%type <sc_decl> abstract_parameter_list
%type <sc_decl> declarator
%type <sc_decl> basic_declarator
%type <sc_decl> semantic_declarator
%type <sc_decl> function_decl_header
%type <sc_decl> function_definition_header
%type <sc_decl> non_empty_abstract_parameter_list
%type <sc_decl> parameter_declaration
%type <sc_decl> parameter_list
%type <sc_decl> template_param
%type <sc_decl> template_param_list
%type <sc_typelist> template_arg_list
%type <sc_typelist> non_empty_template_arg_list

%type <sc_type> abstract_declaration_specifiers
%type <sc_type> abstract_declaration_specifiers2
/*%type <sc_type> declaration_specifiers*/
%type <sc_ptype> template_decl
%type <sc_ptype> template_decl_header
%type <sc_ptype> struct_or_connector_header
%type <sc_ptype> struct_or_connector_specifier
%type <sc_ptype> enum_specifier
%type <sc_ptype> enum_header
%type <sc_ptype> untagged_enum_header
%type <sc_ptype> type_specifier
%type <sc_ptype> untagged_struct_header
%type <sc_ptype> templated_type
%type <sc_ptype> template_arg

%type <sc_sym> cbuffer_header

%type <sc_expr> actual_argument_list
%type <sc_expr> additive_expression
%type <sc_expr> AND_expression
%type <sc_expr> basic_variable
%type <sc_expr> boolean_expression_opt
%type <sc_expr> boolean_scalar_expression
%type <sc_expr> cast_expression
%type <sc_expr> conditional_expression
%type <sc_expr> constant

%type <sc_int> constant_expression

%type <sc_expr> conditional_test
%type <sc_expr> equality_expression
%type <sc_expr> exclusive_OR_expression
%type <sc_expr> expression
%type <sc_expr> expression_list
%type <sc_expr> inclusive_OR_expression
%type <sc_expr> initializer
%type <sc_expr> initializer_list
%type <sc_expr> logical_AND_expression
%type <sc_expr> logical_OR_expression
%type <sc_expr> multiplicative_expression
%type <sc_expr> non_empty_argument_list
%type <sc_expr> postfix_expression
%type <sc_expr> primary_expression
%type <sc_expr> relational_expression
%type <sc_expr> shift_expression
%type <sc_expr> unary_expression
%type <sc_expr> variable

%type <sc_stmt> annotation
%type <sc_stmt> annotation_decl_list
%type <sc_attr> attribute
%type <sc_stmt> balanced_statement
%type <sc_stmt> block_item
%type <sc_stmt> block_item_list
%type <sc_stmt> compound_statement
%type <sc_stmt> dangling_if
%type <sc_stmt> dangling_iteration
%type <sc_stmt> dangling_statement
%type <sc_stmt> declaration
%type <sc_stmt> discard_statement
%type <sc_stmt> break_statement
%type <sc_stmt> expression_statement
%type <sc_stmt> expression_statement2
%type <sc_stmt> for_expression
%type <sc_stmt> for_expression_opt
%type <sc_stmt> for_expression_init
%type <sc_stmt> if_header
%type <sc_stmt> if_statement
%type <sc_stmt> switch_statement
%type <sc_stmt> init_declarator
%type <sc_stmt> init_declarator_list
%type <sc_stmt> iteration_statement
%type <sc_stmt> return_statement
%type <sc_stmt> assembly
%type <sc_stmt> asm_statement
%type <sc_stmt> statement
%type <sc_stmt> labeled_statement
%type <sc_stmt> switch_item_list
/*%type <sc_stmt> struct_declaration
%type <sc_stmt> struct_declaration_list
*/
%type <sc_expr>	state_list
%type <sc_expr>	state
%type <sc_expr>	state_value
%type <sc_expr>	pass
%type <sc_expr>	pass_list
%type <sc_expr>	pass_item
%type <sc_expr>	pass_item_list
%type <sc_expr>	pass_state_value

/* Operator precedence rules: */

/* Don't even THINK about it! */

%%

compilation_unit:		external_declaration					
					|	compilation_unit external_declaration
;

/****************/
/* Declarations */
/****************/

external_declaration:	declaration
							{ $$ = GlobalInitStatements(cg->current_scope, $1); }
					|	cbuffer_decl ';'
							{ $$ = NULL; }
					|	TECHNIQUE_SY identifier '{' pass_list '}'
							{ DefineTechnique(cg, $2, $4, NULL); }
					|	TECHNIQUE_SY identifier annotation '{' pass_list '}'
							{ DefineTechnique(cg, $2, $5, $3); }
					|	function_definition
;

declaration:			declaration_specifiers ';'
							{ $$ = NULL; }
					|	declaration_specifiers init_declarator_list ';'
							{ $$ = $2; }
					|	ERROR_SY ';'
							{ RecordErrorPos(cg->tokenLoc); $$ = NULL; }
;

abstract_declaration:	abstract_declaration_specifiers abstract_declarator
							{ $$ = $2; }
;

declaration_specifiers:	abstract_declaration_specifiers
					|	template_decl
					|	TYPEDEF_SY abstract_declaration_specifiers
							{ cg->SetTypeMisc(TYPE_MISC_TYPEDEF); }
;

abstract_declaration_specifiers:
						abstract_declaration_specifiers2
							{ $$ = $1; }
					|	type_qualifier abstract_declaration_specifiers
							{ cg->SetTypeQualifiers($1); $$ = cg->type_specs; }
					|	storage_class abstract_declaration_specifiers
							{ cg->SetStorageClass($1); $$ = cg->type_specs; }
					|	type_domain abstract_declaration_specifiers
							{ cg->SetTypeDomain($1); $$ = cg->type_specs; }
					|	in_out abstract_declaration_specifiers
							{ cg->SetTypeQualifiers($1); $$ = cg->type_specs; }
					|	function_specifier abstract_declaration_specifiers
							{ cg->SetTypeMisc($1); $$ = cg->type_specs; }
					|	PACKED_SY abstract_declaration_specifiers
							{ cg->SetTypeMisc(TYPE_MISC_PACKED | TYPE_MISC_PACKED_KW); $$ = cg->type_specs; }
					|	ROWMAJOR_SY abstract_declaration_specifiers
							{ cg->SetTypeMisc(TYPE_MISC_ROWMAJOR); $$ = cg->type_specs; }
					|	COLMAJOR_SY abstract_declaration_specifiers
							{ cg->ClearTypeMisc(TYPE_MISC_ROWMAJOR); $$ = cg->type_specs; }
					|	LOWP_SY abstract_declaration_specifiers
							{ cg->SetTypeMisc(TYPE_MISC_PRECISION*1); $$ = cg->type_specs; }
					|	MEDIUMP_SY abstract_declaration_specifiers
							{ cg->SetTypeMisc(TYPE_MISC_PRECISION*2); $$ = cg->type_specs; }
					|	HIGHP_SY abstract_declaration_specifiers
							{ cg->SetTypeMisc(TYPE_MISC_PRECISION*3); $$ = cg->type_specs; }
;

abstract_declaration_specifiers2:
						type_specifier
							{ $$ = *SetDType(&cg->type_specs, $1); }
					|	abstract_declaration_specifiers2 type_qualifier
							{ cg->SetTypeQualifiers($2); $$ = cg->type_specs; }
					|	abstract_declaration_specifiers2 storage_class
							{ cg->SetStorageClass($2); $$ = cg->type_specs; }
					|	abstract_declaration_specifiers2 type_domain
							{ cg->SetTypeDomain($2); $$ = cg->type_specs; }
					|	abstract_declaration_specifiers2 in_out
							{ cg->SetTypeQualifiers($2); $$ = cg->type_specs; }
					|	abstract_declaration_specifiers2 function_specifier
							{ cg->SetTypeMisc($2); $$ = cg->type_specs; }
					|	abstract_declaration_specifiers2 PACKED_SY
							{ cg->SetTypeMisc(TYPE_MISC_PACKED | TYPE_MISC_PACKED_KW); $$ = cg->type_specs; }
;

init_declarator_list:	init_declarator
							{ $$ = $1; }
					|	init_declarator_list ',' init_declarator
							{ $$ = AddStmt($1, $3); }
;

init_declarator:		declarator
							{ $$ = Init_Declarator(cg, $1, NULL); }
					|	declarator '=' initializer
							{ $$ = Init_Declarator(cg, $1, $3); }
;

/*******************/
/* Techniques
/*******************/

pass_list:				pass
							{ $$ = InitializerList($1, NULL); }
					|	pass_list pass
							{ $$ = InitializerList($1, $2); }
;

pass:					PASS_SY identifier '{' pass_item_list '}'
							{ $$ = StateInitializer(cg, $2, $4); }
;

pass_item_list:			pass_item
							{ $$ = InitializerList($1, NULL); }
						|	pass_item_list ';' pass_item
							{ $$ = InitializerList($1, $3); }
						|	pass_item_list ';' 
							{ $$ = $1; }
;

pass_item:				identifier '=' pass_state_value
							{ $$ = StateInitializer(cg, $1, $3); }
;

pass_state_value:		state_value
						|	COMPILE_SY identifier identifier '(' ')'
							{ $$ = SymbolicConstant(cg, $3, $2); }
						|	ASM_SY '{' assembly '}'
							{ $$ = SymbolicConstant(cg, 0, 0); }
;

assembly:					{ $$ = ParseAsm(cg, cg->tokenLoc); }
;


/*******************/
/* Type Specifiers */
/*******************/

type_specifier:			INT_SY
							{ $$ = LookUpTypeSymbol(cg, INT_SY); }
					|	UNSIGNED_SY INT_SY
							{ $$ = LookUpTypeSymbol(cg, INT_SY); }
					|	FLOAT_SY
							{ $$ = LookUpTypeSymbol(cg, FLOAT_SY); }
					|	VOID_SY
							{ $$ = LookUpTypeSymbol(cg, VOID_SY); }
					|	BOOLEAN_SY
							{ $$ = LookUpTypeSymbol(cg, BOOLEAN_SY); }
					|	TEXOBJ_SY
							{ $$ = LookUpTypeSymbol(cg, TEXOBJ_SY); }
					|	enum_specifier
							{ $$ = $1; }
					|	struct_or_connector_specifier
							{ $$ = $1; }
					|	TYPEIDENT_SY
							{ $$ = LookUpTypeSymbol(cg, $1); }
					|	templated_type
							{ $$ = $1; }
					|	error
							{
								SemanticParseError(cg, cg->tokenLoc, ERROR_S_TYPE_NAME_EXPECTED, cg->GetAtomString(cg->last_token /* yychar */));
								$$ = UndefinedType;
							}
;

/*******************/
/* Type Qualifiers */
/*******************/

type_qualifier:			CONST_SY
							{ $$ = TYPE_QUALIFIER_CONST; }
;

/****************/
/* Type Domains */
/****************/

type_domain:			UNIFORM_SY
							{ $$ = TYPE_DOMAIN_UNIFORM; }
;

/*******************/
/* Storage Classes */
/*******************/

storage_class:			STATIC_SY
							{ $$ = SC_STATIC; }
					|	EXTERN_SY
							{ $$ = SC_EXTERN; }
					|	NOINTERP_SY
							{ $$ = SC_NOINTERP; }
					|	PRECISE_SY
							{ $$ = SC_PRECISE; }
					|	SHARED_SY
							{ $$ = SC_SHARED; }
					|	GROUPSHARED_SY
							{ $$ = SC_GROUPSHARED; }
					|	VOLATILE_SY
							{ $$ = SC_UNKNOWN; }
;

/**********************/
/* Function Specifier */
/**********************/

function_specifier:		INLINE_SY
							{ $$ = TYPE_MISC_INLINE; }
					|	INTERNAL_SY
							{ $$ = TYPE_MISC_INTERNAL; }
;

/**********/
/* In Out */
/**********/

in_out:					IN_SY
							{ $$ = TYPE_QUALIFIER_IN; }
					|	OUT_SY
							{ $$ = TYPE_QUALIFIER_OUT; }
					|	INOUT_SY
							{ $$ = TYPE_QUALIFIER_INOUT; }
;

/****************/
/* Struct Types */
/****************/

struct_or_connector_specifier:
						struct_or_connector_header struct_compound_header struct_declaration_list '}'
							{ $$ = SetStructMembers(cg, $1, cg->PopScope()); }
					|	untagged_struct_header struct_compound_header struct_declaration_list '}'
							{ $$ = SetStructMembers(cg, $1, cg->PopScope()); }
					|	struct_or_connector_header
							{ $$ = $1; }
;

struct_compound_header:	compound_header
							{ cg->current_scope->flags |= Scope::is_struct; $$ = $1; }
;

struct_or_connector_header:
						STRUCT_SY struct_identifier
							{ $$ = StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, $2); }
					|	STRUCT_SY struct_identifier ':' semantics_identifier
							{ $$ = StructHeader(cg, cg->tokenLoc, cg->current_scope, $4, $2); }
					|	STRUCT_SY struct_identifier ':' TYPEIDENT_SY
							{ $$ = AddStructBase(StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, $2), LookUpTypeSymbol(cg, $4)); }
;

struct_identifier:		identifier
					|	TYPEIDENT_SY
;

untagged_struct_header:	STRUCT_SY
							{ $$ = StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, 0); }
;

struct_declaration_list:
						struct_declaration
					|	struct_declaration_list struct_declaration
;

struct_declaration:		declaration
					|	function_definition
;

/****************/
/* Cbuffer	*/
/****************/
cbuffer_decl:			cbuffer_header cbuffer_compound_header struct_declaration_list '}'
							{ SetConstantBuffer(cg, cg->tokenLoc, $1, cg->PopScope()); }

cbuffer_compound_header: compound_header
							{ cg->current_scope->flags |= Scope::is_struct | Scope::is_cbuffer; $$ = $1; }
;

cbuffer_header:			CBUFFER_SY struct_identifier
							{ $$ = ConstantBuffer(cg, cg->tokenLoc, cg->current_scope, $2, 0); }
					|	CBUFFER_SY struct_identifier ':' register_spec
							{ $$ = ConstantBuffer(cg, cg->tokenLoc, cg->current_scope, $2, $4); }

;

/****************/
/* Template	*/
/****************/
template_decl:			template_decl_header '{' { cg->PushScope($1->str.members); cg->current_scope->flags |= Scope::is_struct; } struct_declaration_list '}'
							{ cg->PopScope(); $$ = $1; }
					|	template_decl_header
;

template_decl_header:	TEMPLATE_SY '<'
							{ cg->current_scope->formal++; }
						template_param_list '>' STRUCT_SY struct_identifier 
							{ cg->current_scope->formal--; $$ = TemplateHeader(cg, cg->tokenLoc, cg->current_scope, $7, $4); }


template_param_list:	template_param
							{ $$ = $1; }
					|	template_param_list ',' template_param
							{ $$ = AddDecl($1, $3); }
;

template_param:			TYPEDEF_SY		identifier
							{ $$ = NewDeclNode(cg->tokenLoc, $2, 0); }
					|	abstract_declaration
							{ $$ = $1; }
;

/****************/
/* Templated Types */
/****************/

templated_type:			TEMPLATEIDENT_SY
							{ $$ = InstantiateTemplate(cg, cg->tokenLoc, cg->current_scope, LookUpTypeSymbol(cg, $1), 0); }
					|	TEMPLATEIDENT_SY '<' template_arg_list '>'
							{ $$ = InstantiateTemplate(cg, cg->tokenLoc, cg->current_scope, LookUpTypeSymbol(cg, $1), $3); }

template_arg_list:	/* empty */
							{ $$ = NULL; }
					|	non_empty_template_arg_list
;

non_empty_template_arg_list:
						template_arg
							{ $$ = AddtoTypeList(cg, NULL, $1); }
					|	non_empty_template_arg_list ',' template_arg
							{ $$ = AddtoTypeList(cg, $1, $3); }
;

template_arg:			type_specifier
					|	constant_expression
							{ $$ = IntToType(cg, $1); }

/****************/
/* Enum Types */
/****************/

enum_specifier:			enum_header '{' {SetDType(&cg->type_specs, $1);} enum_declaration_list '}'
					|	untagged_enum_header '{' {SetDType(&cg->type_specs, $1);} enum_declaration_list '}'
					|	enum_header
;

enum_header:			ENUM_SY struct_identifier
							{ $$ = EnumHeader(cg, cg->tokenLoc, cg->current_scope, $2); }
;

untagged_enum_header:	ENUM_SY
							{ $$ = EnumHeader(cg, cg->tokenLoc, cg->current_scope, 0); }
;

enum_declaration_list:	enum_declaration
					|	enum_declaration_list ',' enum_declaration
;

enum_declaration:		identifier
							{ EnumAdd(cg, cg->tokenLoc, cg->current_scope, cg->type_specs.basetype, $1, 0); }
					|	identifier '=' INTCONST_SY
							{ EnumAdd(cg, cg->tokenLoc, cg->current_scope, cg->type_specs.basetype, $1, $3); }
;

/***************/
/* Annotations */
/***************/

annotation:				'<' { cg->PushScope(); } annotation_decl_list '>'
							{ $$ = $3; cg->PopScope(); }
;

annotation_decl_list:	/* empty */
							{ $$ = 0; }
					|	annotation_decl_list declaration
;

/***************/
/* Attributes */
/***************/

attribute:				'[' identifier ']'
							{ $$ = Attribute($2, 0, 0, 0); }
					 |	'[' identifier '(' INTCONST_SY ')' ']'
							{ $$ = Attribute($2, 1, $4, 0); }
					 |	'[' identifier '(' INTCONST_SY ',' INTCONST_SY ')' ']'
							{ $$ = Attribute($2, 2, $4, $6); }
					 |	'[' identifier '(' INTCONST_SY ',' INTCONST_SY  ',' INTCONST_SY ')' ']'
							{ $$ = Attribute($2, 2, $4, $6); }
;

/***************/
/* Declarators */
/***************/

declarator:				semantic_declarator
					|	semantic_declarator annotation
;

semantic_declarator:	basic_declarator
							{ $$ = Declarator(cg, $1, 0, 0); }
					|	basic_declarator ':' semantics_identifier
							{ $$ = Declarator(cg, $1, $3, 0); }
					|	basic_declarator ':' register_spec
							{ $$ = Declarator(cg, $1, 0, $3); }
					|	basic_declarator ':' semantics_identifier ':' register_spec
							{ $$ = Declarator(cg, $1, $3, $5); }
;

register_spec:			REGISTER_SY '(' identifier ')'
							{ $$ = $3; }
					|	REGISTER_SY '(' identifier ',' identifier ')'
							{ $$ = $5; }
;

basic_declarator:		identifier
							{ $$ = NewDeclNode(cg->tokenLoc, $1, &cg->type_specs); }
					|	basic_declarator '[' constant_expression ']'
							{ $$ = Array_Declarator(cg, $1, $3, 0); }
					|	basic_declarator '[' ']'
							{ $$ = Array_Declarator(cg, $1, 0, 1); }
					|	function_decl_header parameter_list ')'
							{ $$ = SetFunTypeParams(cg, $1, $2, $2); }
					|	function_decl_header abstract_parameter_list ')'
							{ $$ = SetFunTypeParams(cg, $1, $2, NULL); }
;

function_decl_header:	basic_declarator '('
							{ $$ = FunctionDeclHeader(cg, $1->loc, cg->current_scope, $1); }
					|	OPERATOR_SY operator '('
							{ $$ = FunctionDeclHeader(cg, cg->tokenLoc, cg->current_scope, NewDeclNode(cg->tokenLoc, GetOperatorName(cg, $2), &cg->type_specs)); }
;

operator:				'+'			{ $$ = POS_OP;		}
					|	'-' 		{ $$ = NEG_OP;		}
					|	'!' 		{ $$ = BNOT_OP;		}
					|	'~' 		{ $$ = NOT_OP;		}
					|	'*' 		{ $$ = MUL_OP;		}
					|	'/' 		{ $$ = DIV_OP;		}
					|	'%'			{ $$ = MOD_OP;		}
					|	GG_SY 		{ $$ = SHR_OP;		}
					|	'<' 		{ $$ = LT_OP;		}
					|	'>' 		{ $$ = GT_OP;		}
					|	LE_SY 		{ $$ = LE_OP;		}
					|	GE_SY 		{ $$ = GE_OP;		}
					|	EQ_SY 		{ $$ = EQ_OP;		}
					|	NE_SY		{ $$ = NE_OP;		}
					|	'&' 		{ $$ = AND_OP;		}
					|	'^' 		{ $$ = XOR_OP;		}
					|	'|' 		{ $$ = OR_OP;		}
					|	AND_SY 		{ $$ = BAND_OP;		}
					|	OR_SY		{ $$ = BOR_OP;		}
					|	'(' ')'		{ $$ = FUN_CALL_OP;		}
					|	'[' ']'		{ $$ = ARRAY_INDEX_OP;	}
;

abstract_declarator:	/* empty */
							{ $$ = NewDeclNode(cg->tokenLoc, 0, &cg->type_specs); }
					|	abstract_declarator '[' constant_expression ']'
							{ $$ = Array_Declarator(cg, $1, $3, 0); }
					|	abstract_declarator '[' ']'
							{ $$ = Array_Declarator(cg, $1, 0 , 1); }
/***
 *** This rule causes a major shift reduce conflict with:
 ***
 ***	primary_expression	:;=	type_specifier '(' expression_list ')'
 ***
 *** Cannot be easily factored.	Would force: "( expr -list )" to be merged with "( abstract-param-list )"
 ***
 *** Matches other shading languages' syntax.
 *** Will disallow abstract literal function parameter declarations should we ever defide to
 ***	support function parameters in the future.
 ***
					|	abstract_declarator '(' abstract_parameter_list ')'
***/
;

parameter_list:			parameter_declaration
							{ $$ = $1; }
					|	parameter_list ',' parameter_declaration
							{ $$ = AddDecl($1, $3); }
;

parameter_declaration:	attribute parameter_declaration
							{ $$ = AddDeclAttribute($2, $1); }
					|	declaration_specifiers declarator
							{ $$ = Param_Init_Declarator(cg, $2, NULL); }
					|	declaration_specifiers declarator '=' initializer
							{ $$ = Param_Init_Declarator(cg, $2, $4); }
;

abstract_parameter_list:/* empty */
							{ $$ = NULL; }
					|	non_empty_abstract_parameter_list
;

non_empty_abstract_parameter_list:
						abstract_declaration
							{
								if (IsVoid(&$1->type.type))
								cg->current_scope->flags |= Scope::has_void_param;
								$$ = $1;
							}
					|	non_empty_abstract_parameter_list ',' abstract_declaration
							{
								if ((cg->current_scope->flags & Scope::has_void_param) || IsVoid(&$1->type.type))
								SemanticError(cg, cg->tokenLoc, ERROR___VOID_NOT_ONLY_PARAM);
								$$ = AddDecl($1, $3);
							}
;

/******************/
/* Initialization */
/******************/

initializer:			expression
							{ $$ = Initializer(cg, $1); }
					|	'{' initializer_list '}'
							{ $$ = Initializer(cg, $2); }
					|	'{' initializer_list ',' '}'
							{ $$ = Initializer(cg, $2); }
					|	SAMPLERSTATE_SY '{' state_list '}'
							{ $$ = $3;}
;

initializer_list:		initializer
							{ $$ = InitializerList($1, NULL); }
					|	initializer_list ',' initializer
							{ $$ = InitializerList($1, $3); }
;

state_list:				state ';'
							{ $$ = InitializerList($1, NULL); }
					|	state_list state ';'
							{ $$ = InitializerList($1, $2); }
;

state:					identifier '=' state_value
							{ $$ = StateInitializer(cg, $1, $3); }
					|	TYPEIDENT_SY '=' state_value
							{ $$ = StateInitializer(cg, $1, $3); }
;

state_value:			identifier
							{ $$ = SymbolicConstant(cg, $1, 0); }
					|	constant
					|	'<' additive_expression '>'
							{ $$ = $2; }
;
/***************/
/* EXPRESSIONS */
/***************/

/************/
/* Variable */
/************/

variable:				basic_variable
							{ $$ = $1; }
					|	scope_identifier COLONCOLON_SY basic_variable
							{ $$ = $3; }
;

basic_variable:			variable_identifier
							{ $$ = BasicVariable(cg, $1); }
;

/**********************/
/* Primary Expression */
/**********************/

primary_expression:		variable
					|	constant
					|	'(' expression ')'
							{ $$ = $2; }
					|	type_specifier '(' expression_list ')'
							{ $$ = NewConstructor(cg, cg->tokenLoc, $1, $3); }
;

/*********************/
/* Postfix Operators */
/*********************/

postfix_expression:		primary_expression
					|	postfix_expression PLUSPLUS_SY
							{ $$ =	NewUnopNode(cg, POSTINC_OP, $1); }
					|	postfix_expression MINUSMINUS_SY
							{ $$ =	NewUnopNode(cg, POSTDEC_OP, $1); }
					|	postfix_expression '.' member_identifier
							{ $$ = NewMemberSelectorOrSwizzleOrWriteMaskOperator(cg, cg->tokenLoc, $1, $3); }
					|	postfix_expression '[' expression ']'
							{ $$ = NewIndexOperator(cg, cg->tokenLoc, $1, $3); }
					|	postfix_expression '(' actual_argument_list ')'
							{ $$ = NewFunctionCallOperator(cg, cg->tokenLoc, $1, $3); }
;

actual_argument_list:	/* empty */
							{ $$ = NULL; }
					|	non_empty_argument_list
;

non_empty_argument_list: expression
							{ $$ = ArgumentList(cg, NULL, $1); }
					|	non_empty_argument_list ',' expression
							{ $$ = ArgumentList(cg, $1, $3); }
;

expression_list:		expression
							{ $$ = ExpressionList(cg, NULL, $1); }
					|	expression_list ',' expression
							{ $$ = ExpressionList(cg, $1, $3); }
;

/*******************/
/* Unary Operators */
/*******************/

unary_expression:		postfix_expression
					|	PLUSPLUS_SY unary_expression
							{ $$ =	NewUnopNode(cg, PREINC_OP, $2); }
					|	MINUSMINUS_SY unary_expression
							{ $$ =	NewUnopNode(cg, PREDEC_OP, $2); }
					|	'+' unary_expression
							{ $$ = NewUnaryOperator(cg, cg->tokenLoc, POS_OP, '+', $2, 0); }
					|	'-' unary_expression
							{ $$ = NewUnaryOperator(cg, cg->tokenLoc, NEG_OP, '-', $2, 0); }
					|	'!' unary_expression
							{ $$ = NewUnaryOperator(cg, cg->tokenLoc, BNOT_OP, '!', $2, 0); }
					|	'~' unary_expression
							{ $$ = NewUnaryOperator(cg, cg->tokenLoc, NOT_OP, '~', $2, 1); }
;

/*****************/
/* Cast Operator */
/*****************/

cast_expression:		unary_expression
/* *** reduce/reduce conflict: (var-ident) (type-ident) ***
					|	'(' type_name ')' cast_expression
*/
					|	'(' abstract_declaration ')' cast_expression
							{ $$ = NewCastOperator(cg, cg->tokenLoc, $4, GetTypePointer(cg, $2->loc, &$2->type)); }
;

/****************************/
/* Multiplicative Operators */
/****************************/

multiplicative_expression: cast_expression
					|	multiplicative_expression '*' cast_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, MUL_OP, '*', $1, $3, 0); }
					|	multiplicative_expression '/' cast_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, DIV_OP, '/', $1, $3, 0); }
					|	multiplicative_expression '%' cast_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, MOD_OP, '%', $1, $3, 1); }
;

/**********************/
/* Addative Operators */
/**********************/

additive_expression:	multiplicative_expression
					|	additive_expression '+' multiplicative_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, ADD_OP, '+', $1, $3, 0); }
					|	additive_expression '-' multiplicative_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, SUB_OP, '-', $1, $3, 0); }
;

/***************************/
/* Bitwise Shift Operators */
/***************************/

shift_expression:		additive_expression
					|	shift_expression LL_SY additive_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, SHL_OP, LL_SY, $1, $3, 1); }
					|	shift_expression GG_SY additive_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, SHR_OP, GG_SY, $1, $3, 1); }
;

/************************/
/* Relational Operators */
/************************/

relational_expression:	shift_expression
					|	relational_expression '<' shift_expression
							{ $$ = NewBinaryComparisonOperator(cg, cg->tokenLoc, LT_OP, '<', $1, $3); }
					|	relational_expression '>' shift_expression
							{ $$ = NewBinaryComparisonOperator(cg, cg->tokenLoc, GT_OP, '>', $1, $3); }
					|	relational_expression LE_SY shift_expression
							{ $$ = NewBinaryComparisonOperator(cg, cg->tokenLoc, LE_OP, LE_SY, $1, $3); }
					|	relational_expression GE_SY shift_expression
							{ $$ = NewBinaryComparisonOperator(cg, cg->tokenLoc, GE_OP, GE_SY, $1, $3); }
;

/**********************/
/* Equality Operators */
/**********************/

equality_expression:	relational_expression
					|	equality_expression EQ_SY relational_expression
							{ $$ = NewBinaryComparisonOperator(cg, cg->tokenLoc, EQ_OP, EQ_SY, $1, $3); }
					|	equality_expression NE_SY relational_expression
							{ $$ = NewBinaryComparisonOperator(cg, cg->tokenLoc, NE_OP, NE_SY, $1, $3); }
;

/************************/
/* Bitwise AND Operator */
/************************/

AND_expression:			equality_expression
					|	AND_expression '&' equality_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, AND_OP, '&', $1, $3, 1); }
;

/*********************************/
/* Bitwise Exclusive OR Operator */
/*********************************/

exclusive_OR_expression: AND_expression
					|	exclusive_OR_expression '^' AND_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, XOR_OP, '^', $1, $3, 1); }
;

/*********************************/
/* Bitwise Inclusive OR Operator */
/*********************************/

inclusive_OR_expression: exclusive_OR_expression
					|	inclusive_OR_expression '|' exclusive_OR_expression
							{ $$ = NewBinaryOperator(cg, cg->tokenLoc, OR_OP, '|', $1, $3, 1); }
;

/************************/
/* Logical AND Operator */
/************************/

logical_AND_expression:	inclusive_OR_expression
					|	logical_AND_expression AND_SY inclusive_OR_expression
							{ $$ = NewBinaryBooleanOperator(cg, cg->tokenLoc, BAND_OP, AND_SY, $1, $3); }
;

/***********************/
/* Logical OR Operator */
/***********************/

logical_OR_expression:	logical_AND_expression
					|	logical_OR_expression OR_SY logical_AND_expression
							{ $$ = NewBinaryBooleanOperator(cg, cg->tokenLoc, BOR_OP, OR_SY, $1, $3); }
;

/************************/
/* Conditional Operator */
/************************/

conditional_expression:	logical_OR_expression
					|	conditional_test '?' expression ':' conditional_expression
							{ $$ = NewConditionalOperator(cg, cg->tokenLoc, $1, $3, $5); }
;

conditional_test:		logical_OR_expression
							{	int len; $$ = CheckBooleanExpr(cg, cg->tokenLoc, $1, &len); }
;

/***********************/
/* Assignment operator */
/***********************/

expression:				conditional_expression
/***
					|	basic_variable '=' expression
							{ $$ =	NewBinopNode(cg, ASSIGN_OP, $1, $3); }
***/
;

/***********************/
/* Function Definition */
/***********************/

function_definition:	function_definition_header block_item_list '}'
							{ DefineFunction(cg, $1, $2); cg->PopScope(); }
					|	function_definition_header '}'
							{ DefineFunction(cg, $1, NULL); cg->PopScope(); }
;

function_definition_header:
						attribute function_definition_header
							{ $$ = AddDeclAttribute($2, $1); }
					|	declaration_specifiers declarator '{'
							{ $$ = Function_Definition_Header(cg, $2); }
;

/*************/
/* Statement */
/*************/

statement:				attribute statement
							{ $$ = AddStmtAttribute($2, $1); }
					|	balanced_statement
					|	dangling_statement
;

balanced_statement:		compound_statement
					|	discard_statement
					|	expression_statement
					|	iteration_statement
					|	if_statement
					|	switch_statement
					|	break_statement
					|	return_statement
					|	asm_statement
;

dangling_statement:		dangling_if
					|	dangling_iteration
;

/**********************/
/* Assembly Statement */
/**********************/

asm_statement:			ASM_SY '{' assembly '}'
							{ $$ = $3; }
;
/*********************/
/* Discard Statement */
/*********************/

discard_statement:		DISCARD_SY ';'
							{ $$ =	NewDiscardStmt(cg->tokenLoc, NewUnopSubNode(cg, KILL_OP, SUBOP_V(0, TYPE_BASE_BOOLEAN), NULL)); }
					|	DISCARD_SY expression ';'
							{
								int	len;
								expr *e = CheckBooleanExpr(cg, cg->tokenLoc, $2, &len);
								$$ = NewDiscardStmt(cg->tokenLoc, NewUnopSubNode(cg, KILL_OP, SUBOP_V(len, TYPE_BASE_BOOLEAN), e));
							}
;
/****************/
/* Break Statement */
/****************/
break_statement:		BREAK_SY ';'
							{ $$ =	NewBreakStmt(cg->tokenLoc); }


/****************/
/* If Statement */
/****************/

if_statement:			if_header balanced_statement ELSE_SY balanced_statement
							{ $$ =	SetThenElseStmts($1, $2, $4); }
;

dangling_if:			if_header statement
							{ $$ =	SetThenElseStmts($1, $2, NULL); }
					|	if_header balanced_statement ELSE_SY dangling_statement
							{ $$ =	SetThenElseStmts($1, $2, $4); }
;

if_header:				IF_SY '(' boolean_scalar_expression ')'
							{ $$ =	NewIfStmt(cg->tokenLoc, $3, NULL, NULL); ; }
;

/****************/
/* Switch Statement */
/****************/

switch_statement:		SWITCH_SY	'(' expression ')' compound_header switch_item_list compound_tail
							{ $$ =	NewSwitchStmt(cg->tokenLoc, $3, $6, cg->popped_scope); }
;

labeled_statement:		CASE_SY constant_expression ':' statement
							{ $$ = $4; }
					|	DEFAULT_SY ':' statement
							{ $$ = $3; }
;

switch_item_list:		labeled_statement
					|	switch_item_list labeled_statement
							{ $$ = AddStmt($1, $2); }
;

/**********************/
/* Compound Statement */
/**********************/

compound_statement:		compound_header block_item_list compound_tail
							{ $$ =	NewBlockStmt(cg->tokenLoc, $2, cg->popped_scope); }
					|	compound_header compound_tail
							{ $$ = NULL; }
;

compound_header:		'{'
							{ cg->PushScope(); cg->current_scope->funindex = cg->func_index; }
;

compound_tail:			'}'
							{
								if (cg->opts & cgclib::DUMP_PARSETREE)
									PrintScopeDeclarations(cg);
								cg->PopScope();
							}
;

block_item_list:		block_item
					|	block_item_list block_item
							{ $$ = AddStmt($1, $2); }
;

block_item:				declaration
					|	statement
							{ $$ = CheckStmt($1); }
;

/************************/
/* Expression Stetement */
/************************/

expression_statement:	expression_statement2 ';'
					|	';'
							{ $$ = NULL; }
;

expression_statement2:	postfix_expression /* basic_variable */ '=' expression
							{ $$ = NewSimpleAssignmentStmt(cg, cg->tokenLoc, $1, $3, 0); }
					|	expression
							{ $$ =	NewExprStmt(cg->tokenLoc, $1); }
					|	postfix_expression ASSIGNMINUS_SY expression
							{ $$ = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNMINUS_OP, $1, $3); }
					|	postfix_expression ASSIGNMOD_SY expression
							{ $$ = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNMOD_OP, $1, $3); }
					|	postfix_expression ASSIGNPLUS_SY expression
							{ $$ = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNPLUS_OP, $1, $3); }
					|	postfix_expression ASSIGNSLASH_SY expression
							{ $$ = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNSLASH_OP, $1, $3); }
					|	postfix_expression ASSIGNSTAR_SY expression
							{ $$ = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNSTAR_OP, $1, $3); }
;

/***********************/
/* Iteration Statement */
/***********************/

iteration_statement:	WHILE_SY '(' boolean_scalar_expression ')' balanced_statement
							{ $$ =	NewWhileStmt(cg->tokenLoc, WHILE_STMT, $3, $5); }
					|	DO_SY statement WHILE_SY '(' boolean_scalar_expression ')' ';'
							{ $$ =	NewWhileStmt(cg->tokenLoc, DO_STMT, $5, $2); }
					|	FOR_SY '(' for_expression_init ';' boolean_expression_opt ';' for_expression_opt ')' balanced_statement
							{ $$ =	NewForStmt(cg->tokenLoc, $3, $5, $7, $9); }
;

dangling_iteration:		WHILE_SY '(' boolean_scalar_expression ')' dangling_statement
							{ $$ =	NewWhileStmt(cg->tokenLoc, WHILE_STMT, $3, $5); }
					|	FOR_SY '(' for_expression_init ';' boolean_expression_opt ';' for_expression_opt ')' dangling_statement
							{ $$ =	NewForStmt(cg->tokenLoc, $3, $5, $7, $9); }
;

boolean_scalar_expression:
						expression
							{	$$ = CheckBooleanExpr(cg, cg->tokenLoc, $1, NULL); }
;

for_expression_opt:		for_expression
					|	/* empty */
							{ $$ = NULL; }
;

for_expression:			expression_statement2
					|	for_expression ',' expression_statement2
							{
								if (stmt *lstmt = $1) {
									while (lstmt->next)
										lstmt = lstmt->next;
									lstmt->next = $3;
									$$ = $1;
								} else {
									$$ = $3;
								}
						}
;
for_expression_init:	declaration_specifiers init_declarator_list
							{ $$ = $2; }
					|	for_expression_opt
;

boolean_expression_opt:	boolean_scalar_expression
					|	/* empty */
							{ $$ = NULL; }
;

/*******************/
/*Return Statement */
/*******************/

return_statement:		RETURN_SY expression ';'
							{ $$ =	NewReturnStmt(cg, cg->tokenLoc, cg->current_scope, $2); }
					|	RETURN_SY ';'
							{ $$ =	NewReturnStmt(cg, cg->tokenLoc, cg->current_scope, NULL); }
;

/*********/
/* Misc. */
/*********/

member_identifier:		identifier
;

scope_identifier:		identifier
;

semantics_identifier:	identifier
;

variable_identifier:	identifier
;

identifier:	IDENT_SY
;

constant:				INTCONST_SY /* Temporary! */
							{ $$ =	NewIConstNode(cg, ICONST_OP, $1, TYPE_BASE_CINT); }
					|	UINTCONST_SY /* Temporary! */
							{ $$ =	NewIConstNode(cg, ICONST_OP, $1, TYPE_BASE_CINT); }
					|	CFLOATCONST_SY /* Temporary! */
							{ int base = cg->GetFloatSuffixBase(' '); $$ = NewFConstNode(cg, FCONST_OP, $1, base); }
					|	FLOATCONST_SY /* Temporary! */
							{ int base = cg->GetFloatSuffixBase('f'); $$ = NewFConstNode(cg, FCONST_OP, $1, base); }
					|	FLOATHCONST_SY /* Temporary! */
							{ int base = cg->GetFloatSuffixBase('h'); $$ = NewFConstNode(cg, FCONST_OP, $1, base); }
					|	FLOATXCONST_SY /* Temporary! */
							{int base = cg->GetFloatSuffixBase('x'); $$ = NewFConstNode(cg, FCONST_OP, $1, base); }
					|	STRCONST_SY /* Temporary! */
							{ $$ =	NewIConstNode(cg, ICONST_OP, $1, TYPE_BASE_STRING); }
;

constant_expression:	expression
							{ $$ = GetConstant(cg, $1, 0); }
;

%%

