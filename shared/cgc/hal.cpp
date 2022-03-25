#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hal.h"
#include "cg.h"
#include "errors.h"

namespace cgc {

ConnectorDescriptor *LookupConnectorHAL(ConnectorDescriptor *connectors, int cid, int num) {
	for (int i = 0; i < num; i++) {
		if (cid == connectors[i].cid)
			return &connectors[i];
	}
	return NULL;
}

void SetSymbolConnectorBindingHAL(Binding *fBind, ConnectorRegisters *fConn) {
	fBind->properties = BIND_IS_BOUND;
	fBind->kind = BK_CONNECTOR;
	if (fConn->properties & REG_WRITE_REQUIRED)
		fBind->properties |= BIND_WRITE_REQUIRED;
	// fBind->conn.gname set elsewhere
	// fBind->conn.lname set elsewhere
	fBind->size = fConn->size;
	fBind->rname = fConn->name;
	fBind->regno = fConn->regno;
}

//-----------------------------------------------------------------------------
// default implementations
//-----------------------------------------------------------------------------

void	HAL::RegisterNames(CG *cg)													{}
bool	HAL::GetCapsBit(int bit)													{ return false; }
int		HAL::GetConnectorID(int bit)												{ return CONNECTOR_IS_USELESS; }
int		HAL::GetConnectorUses(int cid, int pid)										{ return 0; }
bool	HAL::CheckDefinition(CG *cg, SourceLoc &loc, int name, const Type *t)		{ return true; }
bool	HAL::CheckStatement(CG *cg, SourceLoc &loc, stmt *s)						{ return true; }
int		HAL::GetInternalFunction(Symbol *s, int *group)								{ return 0; }
bool	HAL::IsTexobjBase(int base)													{ return false; }
bool	HAL::IsValidOperator(SourceLoc &loc, int name, int op, int subop)			{ return true; }
expr*	HAL::FoldInternalFunction(CG *cg, int group, int index, expr *args)			{ return 0; }

bool	HAL::BindUniformUnbound(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind) {
	return false;
}
bool	HAL::BindUniformPragma(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind, const Binding *pragma) {
	return false;
}
bool	HAL::BindVaryingSemantic(CG *cg, SourceLoc &loc, Symbol *s, int semantic, Binding *bind, bool is_out_val) {
	return false;
}
bool	HAL::BindVaryingPragma(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind, const Binding *pragma, bool is_out_val) {
	return false;
}
bool	HAL::BindVaryingUnbound(CG *cg, SourceLoc &loc, Symbol *s, int name, int semantic, Binding *bind, bool is_out_val) {
	return false;
}

// Check for profile-specific limitations of floating point suffixes and return the base for this suffix.
int HAL::GetFloatSuffixBase(SourceLoc &loc, int suffix) {
	switch (suffix) {
		case ' ':
			return TYPE_BASE_CFLOAT;
		case 'f':
			return TYPE_BASE_FLOAT;
		case 'h':
			return TYPE_BASE_HALF;
		default:
			return TYPE_BASE_UNDEFINED_TYPE;
	}
}

//Return a profile specific size for this scalar, vector, or matrix type. Used for defining struct member offsets for use by code generator.
int HAL::GetSizeof(Type *fType) {
	int size, alignment, len, len2;

	if (fType) {
		int	category = GetCategory(fType);
		switch (category) {
			case TYPE_CATEGORY_SCALAR:
			case TYPE_CATEGORY_STRUCT:
				size = fType->size;
				break;
			case TYPE_CATEGORY_ARRAY:
				if (IsVector(fType, &len)) {
					size		= len;
				} else if (IsMatrix(fType, &len, &len2)) {
					size		= (len2 > len ? len : len2) * 4;
				} else {
					size		= GetSizeof(fType->arr.eltype);
					alignment	= GetAlignment(fType->arr.eltype);
					size		= ((size + alignment - 1)/alignment)*alignment*fType->arr.numels;
				}
				break;
			case TYPE_CATEGORY_FUNCTION:
			default:
				size = 0;
				break;
		}
	} else {
		size = 0;
	}
	return size;
}

// Return a profile specific alignment for this type. Used for defining struct member offsets for use by code generator.
int HAL::GetAlignment(Type *fType) {
	int alignment;

	if (fType) {
		if (IsTexobjBase(GetBase(fType))) {
			alignment = 4;
		} else {
			int category = GetCategory(fType);
			switch (category) {
				case TYPE_CATEGORY_SCALAR:
					alignment = 4;
					break;
				case TYPE_CATEGORY_STRUCT:
				case TYPE_CATEGORY_ARRAY:
					alignment = 4;
					break;
				case TYPE_CATEGORY_FUNCTION:
				default:
					alignment = 1;
					break;
			}
		}
	} else {
		alignment = 1;
	}
	return alignment;
}

// Check for profile-specific limitations of declarators.
bool HAL::CheckDeclarators(CG *cg, SourceLoc &loc, const dtype *fDtype) {
	int numdims = 0;
	const Type *lType;

	lType = (Type*)&fDtype->type;
	while (GetCategory(lType) == TYPE_CATEGORY_ARRAY) {
		if (lType->arr.numels == 0 && numdims > 0) {
			SemanticError(cg, loc, ERROR___LOW_DIM_UNSPECIFIED);
			return false;
		}
		if (lType->arr.numels > 4 && IsPacked(lType)) {
			SemanticError(cg, loc, ERROR___PACKED_DIM_EXCEEDS_4);
			return false;
		}
		lType = lType->arr.eltype;
		numdims++;
	}
	if (numdims > 3) {
		SemanticError(cg, loc, ERROR___NUM_DIMS_EXCEEDS_3);
		return false;
	}
	return 1;
}

// Is it valid to typecast a scalar from fromBase to toBase?.
bool HAL::IsValidScalarCast(int toBase, int fromBase, bool Explicit) {
	switch (toBase) {
		case TYPE_BASE_BOOLEAN:
		case TYPE_BASE_FLOAT:
		case TYPE_BASE_HALF:
		case TYPE_BASE_INT:
		case TYPE_BASE_UINT:
			switch (fromBase) {
				case TYPE_BASE_CFLOAT:
				case TYPE_BASE_CINT:
				case TYPE_BASE_FLOAT:
				case TYPE_BASE_HALF:
				case TYPE_BASE_INT:
				case TYPE_BASE_UINT:
					return true;
				case TYPE_BASE_BOOLEAN:
					return (toBase == TYPE_BASE_BOOLEAN) || Explicit;
				default:
					return false;
			}
			break;
		case TYPE_BASE_CFLOAT:
		case TYPE_BASE_CINT:
		default:
			return false;
	}
}

//Is fBase a numeric type?
bool HAL::IsNumericBase(int fBase) {
	switch (fBase) {
		case TYPE_BASE_CFLOAT:
		case TYPE_BASE_CINT:
		case TYPE_BASE_UINT:
		case TYPE_BASE_FLOAT:
		case TYPE_BASE_HALF:
		case TYPE_BASE_INT:
			return true;
		default:
			return false;
	}
}

// Is fBase an integral type?
bool HAL::IsIntegralBase(int fBase) {
	switch (fBase) {
		case TYPE_BASE_CINT:
		case TYPE_BASE_INT:
		case TYPE_BASE_UINT:
			return true;
		default:
			return false;
	}
}

// Are runtime variables with a base of fBase supported? In other words, can a non-const variable of this base be declared in this profile?
bool HAL::IsValidRuntimeBase(int fBase) {
	switch (fBase) {
		case TYPE_BASE_FLOAT:
			return true;
		default:
			return false;
	}
}

// Return the base type for this binary operation.
int HAL::GetBinOpBase(int lop, int lbase, int rbase, int llen, int rlen) {
	switch (lop) {
		case VECTOR_V_OP:
		case MATRIX_M_OP:
		case MUL_OP:
		case DIV_OP:
		case MOD_OP:
		case ADD_OP:
		case SUB_OP:
		case SHL_OP:
		case SHR_OP:
		case LT_OP:
		case GT_OP:
		case LE_OP:
		case GE_OP:
		case EQ_OP:
		case NE_OP:
		case AND_OP:
		case XOR_OP:
		case OR_OP:
		case COND_OP:
			if (lbase == rbase)
				return lbase;
			if (lbase == TYPE_BASE_FLOAT || rbase == TYPE_BASE_FLOAT)
				return TYPE_BASE_FLOAT;
			if (lbase == TYPE_BASE_CFLOAT || rbase == TYPE_BASE_CFLOAT)
				return lbase == TYPE_BASE_INT || rbase == TYPE_BASE_INT ? TYPE_BASE_FLOAT : TYPE_BASE_CFLOAT;
			return TYPE_BASE_INT;

		default:
			return TYPE_BASE_NO_TYPE;
	}
}

// Convert a numeric scalar constant from one base type to another.
bool HAL::ConvertConstant(CG *cg, const scalar_constant *fval, int fbase, int tbase, expr **fexpr) {
	switch (fbase) {
		case TYPE_BASE_CFLOAT:
		case TYPE_BASE_FLOAT:
			switch (tbase) {
				case TYPE_BASE_CFLOAT:
				case TYPE_BASE_FLOAT:
					*fexpr	= NewFConstNode(cg, FCONST_OP, fval->f, tbase);
					return true;
				case TYPE_BASE_CINT:
				case TYPE_BASE_INT:
					*fexpr	= NewIConstNode(cg, ICONST_OP, (int) fval->f, tbase);
					return true;
				default:
					return false;
			}
		case TYPE_BASE_CINT:
		case TYPE_BASE_INT:
			switch (tbase) {
				case TYPE_BASE_CFLOAT:
				case TYPE_BASE_FLOAT:
					*fexpr = NewFConstNode(cg, FCONST_OP, (float) fval->i, tbase);
					return true;
				case TYPE_BASE_CINT:
				case TYPE_BASE_INT:
					*fexpr = NewIConstNode(cg, ICONST_OP, fval->i, tbase);
					return true;
				default:
					return false;
			}
		default:
			return false;
	}
}

int HAL::GenerateCode(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program) {
	InternalError(cg, cg->tokenLoc, ERROR_S_NO_CODE_GENERATOR, cg->GetAtomString(cg->hal->profileName));
	return 1;
}

} //namespace cgc
