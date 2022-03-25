#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "memory.h"
#include "scanner.h"
#include "binding.h"
#include "cpp.h"		// to get MacroSymbol def
#include <string.h>

namespace cgc {

struct HAL;

#define MAX_ARRAY_DIMENSIONS 3

// Type propertes:

enum {
	TYPE_BASE_MASK				= 0x0000000f,
	TYPE_BASE_SHIFT				= 0,
	TYPE_BASE_BITS				= 4,
	TYPE_BASE_NO_TYPE			= 0, // e.g. struct or connector
	TYPE_BASE_UNDEFINED_TYPE,
	TYPE_BASE_CFLOAT,
	TYPE_BASE_CINT,
	TYPE_BASE_VOID,
	TYPE_BASE_FLOAT,
	TYPE_BASE_INT,
	TYPE_BASE_BOOLEAN,
	TYPE_BASE_UINT,
	TYPE_BASE_TEXOBJ,
	TYPE_BASE_STRING,
	TYPE_BASE_HALF,
	TYPE_BASE_FIRST_USER,
	TYPE_BASE_LAST_USER			= 0x0000000f,

	TYPE_CATEGORY_MASK			= 0x000000f0,
	TYPE_CATEGORY_SHIFT			= 4,
	TYPE_CATEGORY_NONE			= 0x00000000,
	TYPE_CATEGORY_SCALAR		= 0x00000010,
	TYPE_CATEGORY_ARRAY			= 0x00000020,
	TYPE_CATEGORY_FUNCTION		= 0x00000030,
	TYPE_CATEGORY_STRUCT		= 0x00000040,
	TYPE_CATEGORY_TEXOBJ		= 0x00000050,
	TYPE_CATEGORY_ENUM			= 0x00000060,

	TYPE_DOMAIN_MASK			= 0x00000f00,
	TYPE_DOMAIN_SHIFT			= 8,
	TYPE_DOMAIN_UNKNOWN			= 0x00000000,
	TYPE_DOMAIN_UNIFORM			= 0x00000100,
	TYPE_DOMAIN_VARYING			= 0x00000200,

	TYPE_QUALIFIER_MASK			= 0x0000f000,
	TYPE_QUALIFIER_NONE			= 0x00000000,
	TYPE_QUALIFIER_CONST		= 0x00001000,
	TYPE_QUALIFIER_IN			= 0x00002000,
	TYPE_QUALIFIER_OUT			= 0x00004000,
	TYPE_QUALIFIER_INOUT		= TYPE_QUALIFIER_IN | TYPE_QUALIFIER_OUT,

	// ??? Should these be called "declarator bits"???
	TYPE_MISC_MASK				= 0x7ff00000,
	TYPE_MISC_TYPEDEF			= 0x00100000,
	//TYPE_MISC_UNSIGNED		= 0x00200000,
	TYPE_MISC_ABSTRACT_PARAMS	= 0x00400000,	// Type is function declared with abstract parameters
	//TYPE_MISC_VOID			= 0x00800000,	// Type is void
	TYPE_MISC_INLINE			= 0x01000000,	// "static inline" function attribute
	TYPE_MISC_INTERNAL			= 0x02000000,	// "__internal" function attribute
	TYPE_MISC_PACKED			= 0x04000000,	// For vector types like float3
	TYPE_MISC_PACKED_KW			= 0x08000000,	// Actual "packed" keyword used
	TYPE_MISC_ROWMAJOR			= 0x10000000,
	TYPE_MISC_PRECISION			= 0x20000000,	// precision for glsl: 0=def, 1=low, 2=med, 3=high
	TYPE_MISC_MARKED			= 0x80000000,	// Temp value for printing types, etc.
};

enum {
	TEXOBJ_ANYTEX	= 0,
	TEXOBJ_1D,
	TEXOBJ_2D,
	TEXOBJ_2DMS,
	TEXOBJ_3D,
	TEXOBJ_CUBE,
	TEXOBJ_RECT,
	TEXOBJ_SHADOW,

	TEXOBJ_BUFFER,
	TEXOBJ_BYTE,
	TEXOBJ_STRUCTURED,
	TEXOBJ_APPEND,
	TEXOBJ_CONSUME,
	TEXOBJ_INPUTPATCH,
	TEXOBJ_OUTPUTPATCH,

	TEXOBJF_ARRAY	= 32,
	TEXOBJF_SAMPLER	= 64,
	TEXOBJF_RW		= 128,
};

enum symbolkind {
	VARIABLE_S,
	TYPEDEF_S,
	TEMPLATE_S,
	FUNCTION_S,
	CONSTANT_S,
	TAG_S,
	MACRO_S,
	TECHNIQUE_S,
};

enum StorageClass {
	SC_UNKNOWN,
	SC_AUTO,
	SC_STATIC,
	SC_EXTERN,
	SC_NOINTERP,
	SC_PRECISE,
	SC_SHARED,
	SC_GROUPSHARED,
};

enum {
	SYMB_IS_PARAMETER			= 0x000001,	// Symbol is a formal parameter
	SYMB_IS_DEFINED				= 0x000002,	// Symbol is defined.	Currently only used for functions.
	SYMB_IS_BUILTIN				= 0x000004,	// Symbol is a built-in function.
	SYMB_IS_INLINE_FUNCTION		= 0x000008,	// Symbol is a function that will be inlined.
	SYMB_IS_CONNECTOR_REGISTER	= 0x000010,	// Symbol is a connector hw register
	SYMB_CONNECTOR_CAN_READ		= 0x000020,	// Symbol is a readable connector hw register
	SYMB_CONNECTOR_CAN_WRITE	= 0x000040,	// Symbol is a writable connector hw register
	SYMB_NEEDS_BINDING			= 0x000080,	// Symbol is a non-static global and has not yet been bound
};

// Typedefs for things defined in "support.h":

struct stmt;
struct expr;
struct Scope;
struct Type;

typedef List<Type> TypeList;

struct Type {
	int			properties;
	int			size;
	union {
		struct {
			Type		*eltype;
			int			numels;
		} arr;

		struct {						// for structs and connectors and templates
			Type		*unqualifiedtype;
			Type		*base;
			Scope		*members;
			int			tag;			// struct or connector tag
			int			semantics;
//			char		*allocated;		// set if corresponding register has been bound
//			int			csize;
		} str;

		struct {
			Type		*rettype;
			TypeList	*paramtypes;
		} fun;

		struct {
			Type		*eltype;
			int			dims;	//1D, 2D, 3D, Cube, rect
		} tex;

		struct {
			int			tag;
			Scope		*members;
		} enm;

		struct {						// template parameter
			int			index;
		} typname;
	};

};

struct Type2 : Type {
	Type2(int _properties, int _size) {
		memset(this, 0, sizeof(*this));
		properties	= _properties;
		size		= _size;
	}

};

static inline int 	GetBase(const Type *t)				{ return t ? t->properties & TYPE_BASE_MASK			: TYPE_BASE_NO_TYPE;}
static inline int 	GetCategory(const Type *t)			{ return t ? t->properties & TYPE_CATEGORY_MASK		: TYPE_CATEGORY_NONE;}
static inline int 	GetDomain(const Type *t)			{ return t ? t->properties & TYPE_DOMAIN_MASK		: TYPE_DOMAIN_UNKNOWN;}
static inline int 	GetQualifiers(const Type *t)		{ return t ? t->properties & TYPE_QUALIFIER_MASK	: TYPE_QUALIFIER_NONE;}
static inline int 	GetQuadRegSize(const Type *t)		{ return t ? (t->size + 3) >> 2 : 1;}
static inline bool 	IsOut(const Type *t) 				{ return t && (t->properties & TYPE_QUALIFIER_OUT); }
static inline bool 	IsConst(const Type *t) 				{ return t && (t->properties & TYPE_QUALIFIER_CONST); }

static inline void 	SetCategory(Type *t, int category)	{ t->properties = (t->properties & ~TYPE_CATEGORY_MASK)	| category; }
static inline void 	SetDomain(Type *t, int domain)		{ t->properties = (t->properties & ~TYPE_DOMAIN_MASK)	| domain; }
	
static inline bool 	IsCategory(const Type *t, int category)	{ return GetCategory(t) == category; }
static inline bool 	IsScalar(const Type *t)				{ return GetCategory(t) == TYPE_CATEGORY_SCALAR; }
static inline bool 	IsEnum(const Type *t)				{ return GetCategory(t) == TYPE_CATEGORY_ENUM; }
static inline bool 	IsStruct(const Type *t)				{ return GetCategory(t) == TYPE_CATEGORY_STRUCT; }
static inline bool 	IsArray(const Type *t)				{ return GetCategory(t) == TYPE_CATEGORY_ARRAY; }
static inline bool 	IsUnsizedArray(const Type *t)		{ return IsArray(t) && t->arr.numels == 0; }
	
static inline bool 	IsTypeBase(const Type *t, int base)	{ return GetBase(t) == base; }
static inline bool 	IsVoid(const Type *t)				{ return GetBase(t) == TYPE_BASE_VOID; }
static inline bool 	IsBoolean(const Type *t)			{ return GetBase(t) == TYPE_BASE_BOOLEAN; }
static inline bool 	IsPacked(const Type *t)				{ return t && (t->properties & TYPE_MISC_PACKED); }

bool	IsVector(const Type *t, int *len);
bool	IsMatrix(const Type *t, int *len, int *len2);
bool	IsSameUnqualifiedType(const Type *a, const Type *b);

struct Symbol {
	Symbol		*left, *right, *next;
	int			name;
	Type		*type;
	SourceLoc	loc;
	symbolkind	kind;
	int			properties;
	StorageClass storage;
	int			flags;		// Temp for various semantic uses
	void		*tempptr;	// DAG rewrite temp: expr*, for SSA expr. that writes this sym
	void		*tempptr2; 	// DAG rewrite temp: dagnode* to DOP_UNIFORM, for input var

	union {
		struct {
			Scope		*locals;
			Symbol		*params;
			stmt		*statements;
			Symbol		*overload;	// List of overloaded versions of this function
			int			flags;		// Used when resolving overloaded reference
			short		group;		// Built-in function group
			short		index;		// Built-in function index
			int			semantics;
		} fun;
		
		struct {
			int			addr;		// Address or member offset
			int			semantics;
			Binding		*bind;
			expr		*init;		// For initialized non-static globals
		} var;
		
		struct {
			int			value;		// Constant value: 0 = false, 1 = true
		} con;
		
		MacroSymbol mac;
	};
	
	friend NextIterator<Symbol> begin(Symbol *s)	{ return s; }
	friend NextIterator<Symbol> end(Symbol *s)		{ return nullptr; }
};

static inline bool IsTypedef(const Symbol *s)	{ return s && s->kind == TYPEDEF_S;}
static inline bool IsTemplate(const Symbol *s)	{ return s && s->kind == TEMPLATE_S;}
static inline bool IsFunction(const Symbol *s)	{ return s && s->kind == FUNCTION_S;}
static inline bool IsInline(const Symbol *s)	{ return IsFunction(s) && (s->properties & SYMB_IS_INLINE_FUNCTION);}

typedef List<Symbol> SymbolList;

// Symbol table is a simple binary tree.
struct Scope {
	enum {
		has_void_param	= 1 << 0,
		has_return		= 1 << 1,
		is_struct		= 1 << 2,
		is_cbuffer		= 1 << 3,
		has_semantics	= 1 << 4,
	};
	static Scope *list;
	Scope		*next, *prev;		// doubly-linked list of all scopes

	Scope		*parent;
	Scope		*func_scope;		// Points to base scope of enclosing function
	MemoryPool	*pool;				// pool used for allocation in this scope
	Symbol		*symbols;
	Symbol		*tags;
	Symbol		*params;
	Type		*rettype;
	int			level;				// 0 = super globals, 1 = globals, etc.
	int			funindex;			// Identifies which function contains scope
	int			formal;				// > 0 when parsing formal parameters.
	int			pid;				// Program type id
	int			flags;

// Only used at global scope (level 1):
	stmt		*init_stmts;		// Global initialization statements
	
	Scope(MemoryPool *pool);
};

extern Type *UndefinedType;
extern Type *VoidType;
extern Type *BooleanType;

int		InitSymbolTable(CG *cg);

void	SetScalarTypeName(int base, int name, Type *fType);
Scope*	NewScopeInPool(MemoryPool *);
Symbol*	NewSymbol(SourceLoc &loc, Scope *fScope, int name, Type *fType, symbolkind kind);
Symbol*	AddSymbol(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType, symbolkind kind);
Symbol*	AddMemberSymbol(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType);
void	RemoveSymbol(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *lSymb);
Symbol*	UniqueSymbol(CG *cg, Scope *fScope, Type *fType, symbolkind kind);
Symbol*	AddTag(CG *cg, SourceLoc &loc, Scope *fScope, int atom, int category);
Symbol*	LookUpLocalSymbol(CG *cg, Scope *fScope, int atom);
Symbol*	LookUpLocalSymbolBySemanticName(Scope *fScope, int atom);
Symbol*	LookUpLocalSymbolByBindingName(Scope *fScope, int atom);
Symbol*	LookUpLocalTag(CG *cg, Scope *fScope, int atom);
Symbol*	LookUpSymbol(CG *cg, Scope *fScope, int atom);
Symbol*	LookUpTag(CG *cg, Scope *fScope, int atom);
void	AddToSymbolList(SymbolList **lList, Symbol *fSymb);
void	AddToScope(CG *cg, Scope *fScope, Symbol *fSymb);
void	AddParameter(Scope *fScope, Symbol *param);
void	ClearAllSymbolTempptr(void);
void	ClearAllSymbolTempptr2(void);

Type*	LookUpTypeSymbol(CG *cg, int atom);
Type*	NewType(int _properties, int _size);
Type*	DupType(Type *fType);
Type*	NewTexObjType(int dims, int properties);
Type*	GetStandardType(HAL *hal, int tbase, int tlen, int tlen2);
void	SetStructMemberOffsets(HAL *hal, Type *fType);
Type*	SetStructMembers(CG *cg, Type *fType, Scope *members);
void	SetConstantBuffer(CG *cg, SourceLoc &loc, Symbol *sym, Scope *members);
Type	*InstantiateTemplate(CG *cg, SourceLoc &loc, Scope *fScope, Type *tmplt, TypeList *types);
Type	*IntToType(CG *cg, int i);

int		GetSwizzleOrWriteMask(CG *cg, SourceLoc &loc, int atom, bool *FIsLValue, int *flen);
int		GetMatrixSwizzleOrWriteMask(CG *cg, SourceLoc &loc, int atom, bool *FIsLValue, int *flen);
const char *GetBaseTypeNameString(CG *cg, int base);

} //namespace cgc

#endif // !SYMBOLS_H

