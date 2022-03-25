#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "symbols.h"
#include "cg.h"
#include "errors.h"
#include "printutils.h"

using namespace cgc;
#include "parser.hpp"

namespace cgc {

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Symbol Table Variables: ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

Type *UndefinedType;
Type *VoidType;
Type *CFloatType;
Type *CIntType;
Type *FloatType;
Type *HalfType;
Type *IntType;
Type *UIntType;
Type *BooleanType;
Type *StringType;
Type *VecType;

static int	baseTypeNames[TYPE_BASE_LAST_USER + 1];
static Type *baseTypes[TYPE_BASE_LAST_USER + 1];
Scope		*Scope::list = 0;

void		HAL_SetupHalfFixedTypes(int, int);

void SetScalarTypeName(int base, int name, Type *fType) {
	if (base >= 0 && base <= TYPE_BASE_LAST_USER) {
		baseTypeNames[base] = name;
		baseTypes[base]		= fType;
	}
}

Type *NewPackedArrayType(HAL *hal, Type *elType, int numels, int properties) {
	Type *t			= NewType(TYPE_CATEGORY_ARRAY | TYPE_MISC_PACKED | properties | GetBase(elType), 0);
	t->arr.eltype	= elType;
	t->arr.numels	= numels;
	t->size			= hal->GetSizeof(t);
	return t;
}

Symbol *AddSymbol(CG *cg, SourceLoc &loc, Type *type, symbolkind kind, const char *name1) {
	return AddSymbol(cg, loc, cg->current_scope, cg->AddAtom(name1), type, kind);
}

void AddSymbols(CG *cg, SourceLoc &loc, Type *type, symbolkind kind) {}

template<typename... N> void AddSymbols(CG *cg, SourceLoc &loc, Type *type, symbolkind kind, const char *name1, N ...names) {
	AddSymbol(cg, loc, cg->current_scope, cg->AddAtom(name1), type, kind);
	AddSymbols(cg, loc, type, kind, names...);
}

int InitSymbolTable(CG *cg) {
	if (!UndefinedType) {
		UndefinedType	= NewType(TYPE_BASE_UNDEFINED_TYPE | TYPE_CATEGORY_SCALAR, 0);
		VoidType		= NewType(TYPE_BASE_VOID | TYPE_CATEGORY_SCALAR, 0);
		CFloatType		= NewType(TYPE_BASE_CFLOAT | TYPE_CATEGORY_SCALAR | TYPE_QUALIFIER_CONST, 1);
		CIntType		= NewType(TYPE_BASE_CINT | TYPE_CATEGORY_SCALAR | TYPE_QUALIFIER_CONST, 1);
		FloatType		= NewType(TYPE_BASE_FLOAT | TYPE_CATEGORY_SCALAR, 1);
		HalfType		= NewType(TYPE_BASE_HALF | TYPE_CATEGORY_SCALAR, 1);
		IntType			= NewType(TYPE_BASE_INT | TYPE_CATEGORY_SCALAR, 1);
		UIntType		= NewType(TYPE_BASE_UINT | TYPE_CATEGORY_SCALAR, 1);
		BooleanType		= NewType(TYPE_BASE_BOOLEAN | TYPE_CATEGORY_SCALAR, 1);
		StringType		= NewType(TYPE_BASE_STRING | TYPE_CATEGORY_SCALAR, 1);

		VecType			= NewType(TYPE_CATEGORY_ARRAY | TYPE_MISC_PACKED, 0);
		VecType->arr.eltype	= NewType(TYPE_BASE_UNDEFINED_TYPE | TYPE_CATEGORY_NONE, 0);
		VecType->arr.numels	= -1;
	};

	Type	*CFloat1Type	= NewPackedArrayType(cg->hal, CFloatType, 1, TYPE_QUALIFIER_CONST);
	Type	*CFloat2Type	= NewPackedArrayType(cg->hal, CFloatType, 2, TYPE_QUALIFIER_CONST);
	Type	*CFloat3Type	= NewPackedArrayType(cg->hal, CFloatType, 3, TYPE_QUALIFIER_CONST);
	Type	*CFloat4Type	= NewPackedArrayType(cg->hal, CFloatType, 4, TYPE_QUALIFIER_CONST);
	Type	*CInt1Type		= NewPackedArrayType(cg->hal, CIntType, 1, TYPE_QUALIFIER_CONST);
	Type	*CInt2Type		= NewPackedArrayType(cg->hal, CIntType, 2, TYPE_QUALIFIER_CONST);
	Type	*CInt3Type		= NewPackedArrayType(cg->hal, CIntType, 3, TYPE_QUALIFIER_CONST);
	Type	*CInt4Type		= NewPackedArrayType(cg->hal, CIntType, 4, TYPE_QUALIFIER_CONST);
	Type	*Float1Type		= NewPackedArrayType(cg->hal, FloatType, 1, 0);
	Type	*Float2Type		= NewPackedArrayType(cg->hal, FloatType, 2, 0);
	Type	*Float3Type		= NewPackedArrayType(cg->hal, FloatType, 3, 0);
	Type	*Float4Type		= NewPackedArrayType(cg->hal, FloatType, 4, 0);
	Type	*Half1Type		= NewPackedArrayType(cg->hal, HalfType, 1, 0);
	Type	*Half2Type		= NewPackedArrayType(cg->hal, HalfType, 2, 0);
	Type	*Half3Type		= NewPackedArrayType(cg->hal, HalfType, 3, 0);
	Type	*Half4Type		= NewPackedArrayType(cg->hal, HalfType, 4, 0);
	Type	*Int1Type		= NewPackedArrayType(cg->hal, IntType, 1, 0);
	Type	*Int2Type		= NewPackedArrayType(cg->hal, IntType, 2, 0);
	Type	*Int3Type		= NewPackedArrayType(cg->hal, IntType, 3, 0);
	Type	*Int4Type		= NewPackedArrayType(cg->hal, IntType, 4, 0);
	Type	*UInt1Type		= NewPackedArrayType(cg->hal, UIntType, 1, 0);
	Type	*UInt2Type		= NewPackedArrayType(cg->hal, UIntType, 2, 0);
	Type	*UInt3Type		= NewPackedArrayType(cg->hal, UIntType, 3, 0);
	Type	*UInt4Type		= NewPackedArrayType(cg->hal, UIntType, 4, 0);
	Type	*Boolean1Type	= NewPackedArrayType(cg->hal, BooleanType, 1, 0);
	Type	*Boolean2Type	= NewPackedArrayType(cg->hal, BooleanType, 2, 0);
	Type	*Boolean3Type	= NewPackedArrayType(cg->hal, BooleanType, 3, 0);
	Type	*Boolean4Type	= NewPackedArrayType(cg->hal, BooleanType, 4, 0);

	SourceLoc dummyLoc;

	AddSymbol(cg, dummyLoc, cg->current_scope, VOID_SY, VoidType, TYPEDEF_S);
	AddSymbol(cg, dummyLoc, cg->current_scope, FLOAT_SY, FloatType, TYPEDEF_S);
	AddSymbol(cg, dummyLoc, cg->current_scope, INT_SY, IntType, TYPEDEF_S);
	AddSymbol(cg, dummyLoc, cg->current_scope, BOOLEAN_SY, BooleanType, TYPEDEF_S);
	
	AddSymbols(cg, dummyLoc, VecType,		TEMPLATE_S,	"vec");

	AddSymbols(cg, dummyLoc, HalfType,		TYPEDEF_S,	"half");
	AddSymbols(cg, dummyLoc, CFloatType,	TYPEDEF_S,	"cfloat");
	AddSymbols(cg, dummyLoc, CIntType,		TYPEDEF_S,	"cint");
	AddSymbols(cg, dummyLoc, UIntType,		TYPEDEF_S,	"uint");
	AddSymbols(cg, dummyLoc, CFloat1Type,	TYPEDEF_S,	"cfloat1");
	AddSymbols(cg, dummyLoc, CFloat2Type,	TYPEDEF_S,	"cfloat2");
	AddSymbols(cg, dummyLoc, CFloat3Type,	TYPEDEF_S,	"cfloat3");
	AddSymbols(cg, dummyLoc, CFloat4Type,	TYPEDEF_S,	"cfloat4");
	AddSymbols(cg, dummyLoc, CInt1Type,		TYPEDEF_S,	"cint1");
	AddSymbols(cg, dummyLoc, CInt2Type,		TYPEDEF_S,	"cint2");
	AddSymbols(cg, dummyLoc, CInt3Type,		TYPEDEF_S,	"cint3");
	AddSymbols(cg, dummyLoc, CInt4Type,		TYPEDEF_S,	"cint4");

	AddSymbols(cg, dummyLoc, Float1Type,	TYPEDEF_S,	"float1");
	AddSymbols(cg, dummyLoc, Float2Type,	TYPEDEF_S,	"float2");
	AddSymbols(cg, dummyLoc, Float3Type,	TYPEDEF_S,	"float3");
	AddSymbols(cg, dummyLoc, Float4Type,	TYPEDEF_S,	"float4");

	AddSymbols(cg, dummyLoc, Half1Type,		TYPEDEF_S,	"half1");
	AddSymbols(cg, dummyLoc, Half2Type,		TYPEDEF_S,	"half2");
	AddSymbols(cg, dummyLoc, Half3Type,		TYPEDEF_S,	"half3");
	AddSymbols(cg, dummyLoc, Half4Type,		TYPEDEF_S,	"half4");

	AddSymbols(cg, dummyLoc, Int1Type,		TYPEDEF_S,	"int1");
	AddSymbols(cg, dummyLoc, Int2Type,		TYPEDEF_S,	"int2");
	AddSymbols(cg, dummyLoc, Int3Type,		TYPEDEF_S,	"int3");
	AddSymbols(cg, dummyLoc, Int4Type,		TYPEDEF_S,	"int4");

	AddSymbols(cg, dummyLoc, UInt1Type,		TYPEDEF_S,	"uint1");
	AddSymbols(cg, dummyLoc, UInt2Type,		TYPEDEF_S,	"uint2");
	AddSymbols(cg, dummyLoc, UInt3Type,		TYPEDEF_S,	"uint3");
	AddSymbols(cg, dummyLoc, UInt4Type,		TYPEDEF_S,	"uint4");

	AddSymbols(cg, dummyLoc, Boolean1Type,	TYPEDEF_S,	"bool1");
	AddSymbols(cg, dummyLoc, Boolean2Type,	TYPEDEF_S,	"bool2");
	AddSymbols(cg, dummyLoc, Boolean3Type,	TYPEDEF_S,	"bool3");
	AddSymbols(cg, dummyLoc, Boolean4Type,	TYPEDEF_S,	"bool4");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_ANYTEX, 0),	TYPEDEF_S, "string");

	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_ANYTEX, 0), 	TYPEDEF_S,	"texture",		"Texture"	);

	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_1D, 					0),	TYPEDEF_S, "_layout1D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_2D, 					0),	TYPEDEF_S, "_layout2D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_3D, 					0),	TYPEDEF_S, "_layout3D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_CUBE,				0),	TYPEDEF_S, "_layoutCube"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_1D | TEXOBJF_ARRAY, 	0),	TYPEDEF_S, "_layout1DArray"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_2D | TEXOBJF_ARRAY, 	0),	TYPEDEF_S, "_layout2DArray"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJ_3D | TEXOBJF_ARRAY, 	0),	TYPEDEF_S, "_layout3DArray"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TYPE_MISC_INTERNAL | TEXOBJF_ARRAY | TEXOBJ_CUBE,0),	TYPEDEF_S, "_layoutCubeArray"	);

	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_ANYTEX, 			0),	TYPEDEF_S, "sampler",			"Sampler"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_1D, 				0),	TYPEDEF_S, "sampler1D",			"Sampler1D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_2D, 				0),	TYPEDEF_S, "sampler2D",			"Sampler2D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_3D, 				0),	TYPEDEF_S, "sampler3D",			"Sampler3D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_CUBE, 				0),	TYPEDEF_S, "samplerCUBE",		"SamplerCUBE"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_RECT, 				0),	TYPEDEF_S, "samplerRECT",		"SamplerRECT"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_SHADOW, 			0),	TYPEDEF_S, "sampler2DShadow",	"Sampler2DShadow"	);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_2D | TEXOBJF_ARRAY, 0),	TYPEDEF_S, "sampler2DArray",	"Sampler2DArray", "sampler2Darray"	);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_ANYTEX, 			0),	TYPEDEF_S, "samplerstate",		"SamplerState"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_SAMPLER | TEXOBJ_ANYTEX, 			0),	TYPEDEF_S, "SamplerComparisonState"	);

#if 0
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_1D,					0), TEMPLATE_S,	"texture1D",		"Texture1D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_2D,					0), TEMPLATE_S,	"texture2D",		"Texture2D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_3D,					0), TEMPLATE_S,	"texture3D",		"Texture3D"			);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_CUBE,					0), TYPEDEF_S,	"textureCUBE",		"TextureCUBE"		);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_RECT,					0), TYPEDEF_S,	"textureRECT",		"TextureRECT"		);

	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_ARRAY | TEXOBJ_1D,	0),	TEMPLATE_S, "Texture1DArray"	);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_ARRAY | TEXOBJ_2D,	0),	TEMPLATE_S, "Texture2DArray"	);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_ARRAY | TEXOBJ_2DMS,	0),	TEMPLATE_S, "Texture2DMSArray"	);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_ARRAY | TEXOBJ_CUBE,	0),	TEMPLATE_S, "TextureCubeArray"	);

	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_2DMS,					0),	TEMPLATE_S,	"Texture2DMS");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_BUFFER,				0),	TEMPLATE_S, "Buffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_BYTE,					0),	TEMPLATE_S, "ByteAddressBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_STRUCTURED,			0),	TEMPLATE_S, "StructuredBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_APPEND,				0),	TEMPLATE_S, "AppendStructuredBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_CONSUME,				0),	TEMPLATE_S, "ConsumeStructuredBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_INPUTPATCH,			0),	TEMPLATE_S, "InputPatch");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_OUTPUTPATCH,			0),	TEMPLATE_S, "OutputPatch");
	
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_BUFFER,				0),	TEMPLATE_S, "RWBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_BYTE,					0),	TEMPLATE_S, "RWByteAddressBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJ_STRUCTURED,			0),	TEMPLATE_S, "RWStructuredBuffer");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_RW | TEXOBJ_1D,		0),	TEMPLATE_S, "RWTexture1D");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_RW | TEXOBJ_2D,		0),	TEMPLATE_S, "RWTexture2D");
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_RW | TEXOBJ_3D,		0),	TEMPLATE_S, "RWTexture3D");

	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_RW | TEXOBJF_ARRAY | TEXOBJ_1D,0),	TEMPLATE_S, "RWTexture1DArray"	);
	AddSymbols(cg, dummyLoc, NewTexObjType(TEXOBJF_RW | TEXOBJF_ARRAY | TEXOBJ_2D,0),	TEMPLATE_S, "RWTexture2DArray"	);
#endif

	Symbol *FalseSymb	= AddSymbol(cg, dummyLoc, BooleanType, CONSTANT_S, "false");
	Symbol *TrueSymb	= AddSymbol(cg, dummyLoc, BooleanType, CONSTANT_S, "true");
	FalseSymb->con.value = 0;
	TrueSymb->con.value = 1;

	SetScalarTypeName(TYPE_BASE_NO_TYPE, cg->AddAtom("***no-base-type***"), UndefinedType);
	SetScalarTypeName(TYPE_BASE_UNDEFINED_TYPE, cg->AddAtom("***undefined-base-type***"), UndefinedType);
	SetScalarTypeName(TYPE_BASE_CFLOAT, cg->AddAtom("float"), CFloatType);
	SetScalarTypeName(TYPE_BASE_CINT, cg->AddAtom("int"), CIntType);
	SetScalarTypeName(TYPE_BASE_VOID, cg->AddAtom("void"), VoidType);
	SetScalarTypeName(TYPE_BASE_FLOAT, cg->AddAtom("float"), FloatType);
	SetScalarTypeName(TYPE_BASE_HALF, cg->AddAtom("half"), HalfType);
	SetScalarTypeName(TYPE_BASE_INT, cg->AddAtom("int"), IntType);
	SetScalarTypeName(TYPE_BASE_BOOLEAN, cg->AddAtom("bool"), BooleanType);
	SetScalarTypeName(TYPE_BASE_UINT, cg->AddAtom("uint"), UIntType);
	HAL_SetupHalfFixedTypes(TYPE_BASE_HALF, 0);

	int	name = cg->AddAtom("***unknown-profile-base-type***");
	for (int i = TYPE_BASE_FIRST_USER; i <= TYPE_BASE_LAST_USER; i++)
		SetScalarTypeName(i, name, UndefinedType);

	// Add profile specific symbols and types:
	cg->hal->RegisterNames(cg);
	cg->AddAtom("<*** end hal specific atoms ***>");

	// Initialize misc. other globals:
	cg->type_specs.basetype		= UndefinedType;
	cg->type_specs.is_derived	= false;
	cg->type_specs.type			= *UndefinedType;

	return 1;
}

static void unlinkScope(Scope *scope) {
	if (scope->next)
		scope->next->prev = scope->prev;
	if (scope->prev)
		scope->prev->next = scope->next;
	else
		Scope::list = scope->next;
}

Scope::Scope(MemoryPool *_pool) {
	memset(this, 0, sizeof(*this));
	pool	= _pool;
	pid		= PID_NONE_ID;
	if ((next = list))
		list->prev = this;
	list = this;
}

Scope *NewScopeInPool(MemoryPool *pool) {
	Scope *s = new(pool) Scope(pool);
	mem_AddCleanup(pool, (MemoryPoolCleanup*)unlinkScope, s);
	return s;
}

// Allocate a new symbol node;
Symbol *NewSymbol(SourceLoc &loc, Scope *scope, int name, Type *type, symbolkind kind) {
	Symbol *s	= new(scope->pool) Symbol;
	memset(s, 0, sizeof(*s));

	s->name		= name;
	s->storage	= SC_UNKNOWN;
	s->type		= type;
	s->loc		= loc;
	s->kind		= kind;
	return s;
}

static void lAddToTree(CG *cg, Symbol **fSymbols, Symbol *fSymb) {
	if (Symbol *lSymb = *fSymbols) {
		int	frev = cg->GetReversedAtom(fSymb->name);
		while (lSymb) {
			int	lrev = cg->GetReversedAtom(lSymb->name);
			if (lrev == frev) {
				InternalError(cg, fSymb->loc, 9999, "symbol \"%s\" already in table", cg->GetAtomString(fSymb->name));
				break;
			} else {
				if (lrev > frev) {
					if (lSymb->left) {
						lSymb = lSymb->left;
					} else {
						lSymb->left = fSymb;
						break;
					}
				} else {
					if (lSymb->right) {
						lSymb = lSymb->right;
					} else {
						lSymb->right = fSymb;
						break;
					}
				}
			}
		}
	} else {
		*fSymbols = fSymb;
	}
}

void RemoveSymbol(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *fSymb) {
	Symbol **fSymbols 	= &fScope->symbols;
	Symbol *lSymb 		= *fSymbols, *parent = 0;
	bool 	left;

	if (lSymb) {
		int	frev = cg->GetReversedAtom(fSymb->name);
		while (lSymb) {
			int	lrev = cg->GetReversedAtom(lSymb->name);
			if (lrev == frev) {
				if (!parent) {
					if (*fSymbols = lSymb->left) {
						for (parent = lSymb->left; parent->right; parent = parent->right);
						parent->right = lSymb->right;
					} else {
						*fSymbols = lSymb->right;
					}
				} else if (left) {
					if (parent->left = lSymb->left) {
						for (parent = lSymb->left; parent->right; parent = parent->right);
						parent->right = lSymb->right;
					} else {
						parent->left = lSymb->right;
					}
				} else {
					if (parent->right = lSymb->right) {
						for (parent = lSymb->right; parent->left; parent = parent->left);
						parent->left = lSymb->left;
					} else {
						parent->right = lSymb->left;
					}
				}
				//delete(fScope->pool) lSymb;
				return;
			} else {
				parent	= lSymb;
				left	= lrev > frev;
				lSymb	= left ? lSymb->left : lSymb->right;
			}
		}
	}
	InternalError(cg, loc, 9999, "symbol \"%s\" not in table", cg->GetAtomString(fSymb->name));
}

void AddToScope(CG *cg, Scope *fScope, Symbol *fSymb) {
	lAddToTree(cg, &fScope->symbols, fSymb);
}

// Add a variable, type, or function name to a scope.
Symbol *AddSymbol(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType, symbolkind kind) {
	Symbol *lSymb = NewSymbol(loc, fScope, atom, fType, kind);
	lAddToTree(cg, &fScope->symbols, lSymb);
	return lSymb;
}

Symbol *AddMemberSymbol(CG *cg, SourceLoc &loc, Scope *fScope, int atom, Type *fType) {
	Symbol *lSymb = AddSymbol(cg, loc, fScope, atom, fType, VARIABLE_S);
	if (fScope->symbols != lSymb) {
		Symbol	*mSymb = fScope->symbols;
		while (mSymb->next)
			mSymb = mSymb->next;
		mSymb->next = lSymb;
	}
	return lSymb;
}

// Add a symbol to fScope that is different from every other symbol.  Useful for compiler generated temporaries.
Symbol *UniqueSymbol(CG *cg, Scope *fScope, Type *fType, symbolkind kind) {
	static int			nextTmp = 0;
	static SourceLoc	tmpLoc;
	char				buf[256];
	sprintf(buf, "__TMP%d", nextTmp++);
	return AddSymbol(cg, tmpLoc, fScope, cg->AddAtom(buf), fType, kind);
}


// Add a tag name to a scope.
Symbol *AddTag(CG *cg, SourceLoc &loc, Scope *fScope, int atom, int category) {
	Type *pType = NewType(category, 0);
	pType->str.unqualifiedtype 	= pType;
	Symbol *lSymb = NewSymbol(loc, fScope, atom, pType, TAG_S);
	lAddToTree(cg, &fScope->tags, lSymb);
	return lSymb;
}

//***************************************** Type Functions ***********************************

//Type::Type() { memset(this, 0, sizeof(Type)); }

Type *NewType(int _properties, int _size) {
	return new Type2(_properties, _size);
}

Type *DupType(Type *t) {
	return new Type(*t);
}

Type *NewTexObjType(int dims, int properties) {
	Type *t = NewType(properties | TYPE_BASE_TEXOBJ | TYPE_CATEGORY_TEXOBJ, 0);
	t->tex.dims = dims;
	return t;
}

bool IsVector(const Type *t, int *len) {
	if (IsArray(t) && (t->properties & TYPE_MISC_PACKED) && !IsArray(t->arr.eltype)) {
		if (len)
			*len = t->arr.numels;
		return true;
	}
	return false;
}

bool IsMatrix(const Type *t, int *len, int *len2) {
	if (IsArray(t) && (t->properties & TYPE_MISC_PACKED) && IsVector(t->arr.eltype, len)) {
		if (len2)
			*len2 = t->arr.numels;
		return true;
	}
	return false;
}

bool IsSameUnqualifiedType(const Type *a, const Type *b) {
	const int UnQMask = TYPE_BASE_MASK | TYPE_CATEGORY_MASK ; // 020122 // | TYPE_DOMAIN_MASK;

	if (a == b)
		return true;
	
	if (a && b && (a->properties & UnQMask) == (b->properties & UnQMask)) {
		switch (a->properties & TYPE_CATEGORY_MASK) {
		case TYPE_CATEGORY_SCALAR:
			return true;
		case TYPE_CATEGORY_ARRAY:
			return a->arr.numels == b->arr.numels && IsSameUnqualifiedType(a->arr.eltype, b->arr.eltype);
		case TYPE_CATEGORY_FUNCTION:
			break;
		case TYPE_CATEGORY_STRUCT:
			return a->str.unqualifiedtype == b->str.unqualifiedtype;
		case TYPE_CATEGORY_TEXOBJ:
			return a->tex.dims == b->tex.dims || (b->tex.dims & 15) == 0 || (a->tex.dims & 15) == 0;
		default:
			break;
		}
	}
	return false;
}

/************************************ Symbol Semantic Functions ******************************/

Symbol *LookUpLocalSymbol(CG *cg, Scope *fScope, int atom) {
	int	ratom = cg->GetReversedAtom(atom);
	int rname;
	for (Symbol *lSymb = fScope->symbols; lSymb; lSymb = rname > ratom ? lSymb->left : lSymb->right) {
		rname = cg->GetReversedAtom(lSymb->name);
		if (rname == ratom)
			return lSymb;
	}
	return NULL;
}

//Lookup a symbol in a local tree by the : semantic name.
// Note:  The tree is not ordered for this lookup so the next field is used
// This only works for structs and other scopes that maintain this list.
Symbol *LookUpLocalSymbolBySemanticName(Scope *fScope, int atom) {
	if (fScope) {
		for (Symbol *lSymb = fScope->symbols; lSymb; lSymb = lSymb->next) {
			if (lSymb->kind == VARIABLE_S && lSymb->var.semantics == atom)
				return lSymb;
		}
	}
	return NULL;
}

//Lookup a symbol in a local tree by the lname in the semantic binding structure.
// Note:  The tree is not ordered for this lookup so the next field is used
// This only works for structs and other scopes that maintain this list.
Symbol *LookUpLocalSymbolByBindingName(Scope *fScope, int atom) {
	if (fScope) {
		for (Symbol *lSymb = fScope->symbols; lSymb; lSymb = lSymb->next) {
			if (lSymb->kind == VARIABLE_S && lSymb->var.bind && lSymb->var.bind->lname == atom)
				return lSymb;
		}
	}
	return NULL;
}

Symbol *LookUpLocalTag(CG *cg, Scope *fScope, int atom) {
	int	ratom = cg->GetReversedAtom(atom);
	int	rname;
	for (Symbol *lSymb = fScope->tags; lSymb; lSymb = rname > ratom ? lSymb->left : lSymb->right) {
		rname = cg->GetReversedAtom(lSymb->name);
		if (rname == ratom)
			return lSymb;
	}
	return NULL;
}

Symbol *LookUpSymbol(CG *cg, Scope *fScope, int atom) {
	while (fScope) {
		if (Symbol *lSymb = LookUpLocalSymbol(cg, fScope, atom))
			return lSymb;
		fScope = fScope->parent;
	}
	return NULL;
}

Symbol *LookUpTag(CG *cg, Scope *fScope, int atom) {
	while (fScope) {
		if (Symbol *lSymb = LookUpLocalTag(cg, fScope, atom))
			return lSymb;
		fScope = fScope->parent;
	}
	return NULL;
}

Type *LookUpTypeSymbol(CG *cg, int atom) {
	Symbol *lSymb = LookUpSymbol(cg, cg->current_scope, atom);
	if (lSymb) {
		if (!IsTypedef(lSymb) && !IsTemplate(lSymb)) {
			InternalError(cg, cg->tokenLoc, ERROR_S_NAME_NOT_A_TYPE, cg->GetAtomString(atom));
			return UndefinedType;
		}
		Type *lType = lSymb->type;
		return lType ? lType : UndefinedType;
	} else {
		InternalError(cg, cg->tokenLoc, ERROR_S_TYPE_NAME_NOT_FOUND, cg->GetAtomString(atom));
		return UndefinedType;
	}
}

// Scalar: len = 0.
// Vector: len >= 1 and len2 = 0
// Matrix: len >= 1 and len2 >= 1
//
// len = 1 means "float f[1]" not "float f"
// Vector and matrix types are PACKED.
Type *GetStandardType(HAL *hal, int tbase, int tlen, int tlen2) {
	if (tbase >= 0 && tbase <= TYPE_BASE_LAST_USER) {
		Type *lType = baseTypes[tbase];
		if (tlen > 0) {
			// Put these in a table, too!!! XYZZY !!!
			Type *nType = NewType(TYPE_CATEGORY_ARRAY | TYPE_MISC_PACKED | tbase, 0);
			nType->arr.eltype = lType;
			nType->arr.numels = tlen;
			nType->size = hal->GetSizeof(nType);
			lType = nType;
			if (tlen2 > 0) {
				// Put these in a table, too!!! XYZZY !!!
				nType = NewType(TYPE_CATEGORY_ARRAY | TYPE_MISC_PACKED | tbase, 0);
				nType->arr.eltype = lType;
				nType->arr.numels = tlen2;
				nType->size = hal->GetSizeof(nType);
				return nType;
			}
		}
		return lType;
	}
	return 0;
}

// Assign offsets to members for use by code generators.
void SetStructMemberOffsets(HAL *hal, Type *fType) {
	int		addr = 0;
	for (Symbol *s = fType->str.members->symbols; s; s = s->next) {
		int	alignment	= hal->GetAlignment(s->type);
		int	size		= hal->GetSizeof(s->type);
		addr			= ((addr + alignment - 1) / alignment) * alignment;
		s->var.addr		= addr;
		addr			+= size;
	}
	fType->size = ((addr + 3) / 4) * 4;
}

// Set the member tree of a structure.
Type *SetStructMembers(CG *cg, Type *fType, Scope *members) {
	if (fType) {
		if (fType->str.members) {
//			SemanticError(cg, cg->tokenLoc, ERROR_SSD_STRUCT_ALREADY_DEFINED, cg->GetAtomString(fType->str.tag), cg->GetAtomString(fType->str.loc.file), fType->str.loc.line);
			SemanticError(cg, cg->tokenLoc, ERROR_SSD_STRUCT_ALREADY_DEFINED, cg->GetAtomString(fType->str.tag), cg->GetAtomString(cg->tokenLoc.file), cg->tokenLoc.line);
		} else {
			fType->str.members	= members;
//			fType->str.loc		= loc;
			SetStructMemberOffsets(cg->hal, fType);
			if (int tag = fType->str.tag) {
				Symbol *lSymb = LookUpLocalSymbol(cg, cg->current_scope, tag);
				if (!lSymb)
					lSymb = DefineTypedef(cg, cg->tokenLoc, cg->current_scope, tag, fType);
				else if (IsCategory(fType, TYPE_CATEGORY_STRUCT) && !IsCategory(lSymb->type, TYPE_CATEGORY_STRUCT))
					SemanticError(cg, cg->tokenLoc, ERROR_S_NAME_ALREADY_DEFINED,  cg->GetAtomString(tag));
			}
		}
	}
	return fType;
}

Type *InstantiateType(CG *cg, Type *in, TypeList *types) {
	if (!in)
		return types->p;

	switch (GetCategory(in)) {
		case TYPE_CATEGORY_NONE:
		case TYPE_CATEGORY_SCALAR:
			if (GetBase(in) == TYPE_BASE_UNDEFINED_TYPE) {
				Type	*out = (*types)[in->typname.index];
				if (~out->properties & in->properties & TYPE_QUALIFIER_MASK) {
					out = DupType(out);
					out->properties |= in->properties & TYPE_QUALIFIER_MASK;
				}
				return out;
			}
			break;

		case TYPE_CATEGORY_ARRAY: {
			Type	*eltype = InstantiateType(cg, in->arr.eltype, types);
			if (eltype != in->arr.eltype || in->arr.numels < 0) {
				Type	*out = DupType(in);
				out->arr.eltype	= eltype;
				if (in->arr.numels < 0) {
					Type	*n = (*types)[-in->arr.numels];
					out->arr.numels = n->typname.index;
				}
				out->size	= cg->hal->GetSizeof(out);
				return out;
			}
			break;
		}

		case TYPE_CATEGORY_FUNCTION: {
			Type		*out	= in;
			TypeList	*list	= 0;
			if (in->fun.rettype) {
				Type	*rettype = InstantiateType(cg, in->fun.rettype, types);
				if (rettype != in->fun.rettype) {
					out = DupType(in);
					out->fun.rettype = rettype;
				}
			}

			for (TypeList *p = in->fun.paramtypes; p; p = p->next) {
				Type	*t = InstantiateType(cg, p->p, types);
				if (t != p->p) {
					if (out == in)
						out = DupType(in);
					if (!list) {
						for (TypeList *p2 = in->fun.paramtypes; p2 != p; p2 = p2->next) {
							TypeList *node = new TypeList(p2->p);
							if (list)
								list->next = node;
							else
								out->fun.paramtypes = list = node;
						}
					}
					TypeList *node = new TypeList(t);
					if (list)
						list->next = node;
					else
						out->fun.paramtypes = list = node;
				}
			}
			return out;
		}
		case TYPE_CATEGORY_STRUCT:
			break;
	}

	return in;
}

Type *InstantiateTemplate(CG *cg, SourceLoc &loc, Scope *fScope, Type *tmplt, TypeList *types) {
	if (!IsStruct(tmplt))
		return InstantiateType(cg, tmplt, types);
	
	Type	*inst	= DupType(tmplt);
	inst->str.members	= NewScopeInPool(tmplt->str.members->pool);
	
	char	name[1024];
	strcpy(name, cg->GetAtomString(tmplt->str.tag));
	if (types) {
		const char *sep = "<";
		for (auto i : *types) {
			strcat(name, sep);
			sep = ", ";
			FormatTypeStringRT(cg, name + strlen(name), 0, 0, 0, i, true);
		}
		strcat(name, ">");
	} else {
		strcat(name, "<>");
	}
	inst->str.tag = cg->AddAtom(name);

	TypeList	def(0);
	if (!types) {
		def.p = GetStandardType(cg->hal, TYPE_BASE_FLOAT, 4, 0);
		types = &def;
	}

	Symbol	**list = &inst->str.members->symbols;
	for (Symbol *s = tmplt->str.members->symbols; s; s = s->next) {
		Symbol	*si = new(fScope->pool) Symbol(*s);
		si->left	= si->right = si->next = 0;
		si->type	= InstantiateType(cg, s->type, types);
		lAddToTree(cg, &inst->str.members->symbols, si);
		*list		= si;
		list		= &si->next;
	}

	return inst;
}

// Set the members of constant buffer
void SetConstantBuffer(CG *cg, SourceLoc &loc, Symbol *sym, Scope *members) {
	Type *type = sym->type;
	if (Scope *existing = type->str.members) {
		Symbol *s = existing->symbols;
		while (s->next)
			s = s->next;
		s->next = members->symbols;
		for (s = members->symbols; s; s = s->next) {
			RemoveSymbol(cg, loc, members, s);
			lAddToTree(cg, &existing->symbols, s);
		}
	} else {
		type->str.members		= members;
//		type->str.loc			= loc;

		sym->next				= cg->global_scope->params;
		cg->global_scope->params= sym;
	}
	SetStructMemberOffsets(cg->hal, type);
}

// Add a parameter to a function's formal parameter list, or a member to a struct or connector's member list.
void AddParameter(Scope *fScope, Symbol *param) {
	if (Symbol *lSymb = fScope->params) {
		while (lSymb->next)
			lSymb = lSymb->next;
		lSymb->next = param;
	} else {
		fScope->params = param;
	}
}

// Create type to encode an int for templates
Type *IntToType(CG *cg, int i) {
	Type *t			= NewType(TYPE_CATEGORY_NONE | TYPE_BASE_CINT, 0);
	t->typname.index = i;
	return t;
}

////////////////////////////////// Various Support Functions: /////////////////////////////////

// Build a swizzle mask out of the letters in an identifier.
int GetSwizzleOrWriteMask(CG *cg, SourceLoc &loc, int atom, bool *FIsLValue, int *flen) {
	const char *s		= cg->GetAtomString(atom), *t = s;
	int		len			= 0, mask = 0, bits = 0, groups = 0;
	bool	LIsLValue	= true;
	int		bit, group;

	while (*s) {
		char	ch = *s++;
		switch (ch) {
			case 'x':
				bit = 0;
				group = 1;
				break;
			case 'y':
				bit = 1;
				group = 1;
				break;
			case 'z':
				bit = 2;
				group = 1;
				break;
			case 'w':
				bit = 3;
				group = 1;
				break;
			case 'r':
				bit = 0;
				group = 2;
				break;
			case 'g':
				bit = 1;
				group = 2;
				break;
			case 'b':
				bit = 2;
				group = 2;
				break;
			case 'a':
				bit = 3;
				group = 2;
				break;
			default:
				SemanticError(cg, loc, ERROR_CS_INVALID_SWIZZLE_CHAR, ch, t);
				return mask;
				break;
		}
		mask |= bit << len*2;
		bit = 1 << bit;
		if (bits & bit)
			LIsLValue = false;
		bits |= bit;
		if (groups && groups != group) {
			SemanticError(cg, loc, ERROR_CS_INVALID_SWIZZLE_CHAR, ch, t);
			return mask;
		}
		groups |= group;
		len++;
	}
	if (len > 4)
		SemanticError(cg, loc, ERROR_S_SWIZZLE_TOO_LONG, t);
	if (FIsLValue)
		*FIsLValue = LIsLValue;
	if (flen)
		*flen = len;
	return mask;
}

// Build a matrix swizzle mask out of the letters in an identifier.
int GetMatrixSwizzleOrWriteMask(CG *cg, SourceLoc &loc, int atom, bool *FIsLValue, int *flen) {
	const char *s = cg->GetAtomString(atom), *t = s;
	int		len = 0, mask = 0, bits = 0, Error = 0;
	int		bit;
	bool	LIsLValue = true;
	char	lch;
	if (s[0] == '_' && s[1] != '\0') {
		int	base = s[1] != 'm';
		while (*s) {
			char ch = lch = *s++;
			if (ch == '_') {
				if (base == 0) {
					if (*s++ != 'm') {
						Error = 1;
						break;
					}
				}
				lch = *s++;
				ch = lch - base;
				if (ch >= '0' && ch <= '3') {
					bit = (ch - '0') << 2;
					lch = *s++;
					ch = lch - base;
					if (ch >= '0' && ch <= '3') {
						bit = bit | (ch - '0');
						mask |= bit << len*4;
						bit = 1 << bit;
						if (bit & bits)
							LIsLValue = 0;
						bits |= bit;
						len++;
					} else {
						Error = 1;
						break;
					}
				} else {
					Error = 1;
					break;
				}
			} else {
				Error = 1;
				break;
			}
		}
	} else {
		lch = *s;
		Error = 1;
	}
	if (Error)
		SemanticError(cg, loc, ERROR_CS_INVALID_SWIZZLE_CHAR, lch, t);
	if (len > 4)
		SemanticError(cg, loc, ERROR_S_SWIZZLE_TOO_LONG, t);
	if (FIsLValue)
		*FIsLValue = LIsLValue;
	if (flen)
		*flen = len;
	return mask;
}

// Return a pointer to a string representation of a base type name.
const char *GetBaseTypeNameString(CG *cg, int base) {
	if (base >= 0 && base <= TYPE_BASE_LAST_USER) {
		return cg->GetAtomString(baseTypeNames[base]);
	} else {
		return "*** bad base value ***";
	}
}

// Clear the tempptr for all symbols in this tree.
static void ClearSymbolTempptr(Symbol *fSymb) {
	while (fSymb) {
		fSymb->tempptr = NULL;
		ClearSymbolTempptr(fSymb->left);
		fSymb = fSymb->right;
	}
}

// Walk a list of scopes and clear tempptr field for all symbols.
static void ClearSymbolTempptrList(Scope *fScope) {
	while (fScope) {
		ClearSymbolTempptr(fScope->symbols);
		fScope = fScope->next;
	}
}

void ClearAllSymbolTempptr(void) {
	ClearSymbolTempptrList(Scope::list);
}


// Clear the tempptr2 for all symbols in this tree.
static void ClearSymbolTempptr2(Symbol *fSymb) {
	while (fSymb) {
		fSymb->tempptr2 = NULL;
		ClearSymbolTempptr2(fSymb->left);
		fSymb = fSymb->right;
	}
}

// Walk a list of scopes and clear tempptr field for all symbols.
static void ClearSymbolTempptr2List(Scope *fScope) {
	while (fScope) {
		ClearSymbolTempptr2(fScope->symbols);
		fScope = fScope->next;
	}
}

void ClearAllSymbolTempptr2(void) {
	ClearSymbolTempptr2List(Scope::list);
}

void AddToSymbolList(SymbolList **plist, Symbol *s) {
	SymbolList	*node = new SymbolList(s);
	if (SymbolList *list = *plist) {
		while (list->next)
			list = list->next;
		list->next = node;
	} else {
		*plist = node;
	}
}

} //namespace cgc
