#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "generic_hal.h"
#include "cg.h"
#include "compile.h"
#include "printutils.h"
#include "errors.h"

using namespace cgc;

// Vendor and version strings:

#define VENDOR_STRING_GENERIC        "NVIDIA Corporation"
#define VERSION_STRING_GENERIC       "1.0.1"

// Generic connectors:

#define CID_GENERIC_IN_NAME          "generic_in"
#define CID_GENERIC_IN_ID            8

#define CID_GENERIC_OUT_NAME         "generic_out"
#define CID_GENERIC_OUT_ID           9

// Generic profile:

#define PROFILE_GENERIC_NAME         "generic"
#define PROFILE_GENERIC_ID           5


enum regAppToVertex_generic {
    REG_AP2V_ATTR0, REG_AP2V_ATTR1, REG_AP2V_ATTR2, REG_AP2V_ATTR3,
    REG_AP2V_ATTR4, REG_AP2V_ATTR5, REG_AP2V_ATTR6, REG_AP2V_ATTR7,
    REG_AP2V_ATTR8, REG_AP2V_ATTR9, REG_AP2V_ATTR10, REG_AP2V_ATTR11,
    REG_AP2V_ATTR12, REG_AP2V_ATTR13, REG_AP2V_ATTR14, REG_AP2V_ATTR15,
};

enum regVertexToFrag_generic {
    REG_V2FR_HPOS,
    REG_V2FR_COL0, REG_V2FR_COL1,
    REG_V2FR_TEX0, REG_V2FR_TEX1, REG_V2FR_TEX2, REG_V2FR_TEX3,
    REG_V2FR_FOGC, REG_V2FR_PSIZ,
    REG_V2FR_WPOS,
};

#define NUMELS(x) (sizeof(x) / sizeof((x)[0]))

// Static data
#define FLT TYPE_BASE_FLOAT

// These define all the input connector registers for this profile
// you can have numltiple names that refer to the same register number
static ConnectorRegisters inputCRegs_generic[] = {
	{ "ATTR0",  0, FLT, REG_AP2V_ATTR0,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR1",  0, FLT, REG_AP2V_ATTR1,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR2",  0, FLT, REG_AP2V_ATTR2,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR3",  0, FLT, REG_AP2V_ATTR3,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR4",  0, FLT, REG_AP2V_ATTR4,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR5",  0, FLT, REG_AP2V_ATTR5,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR6",  0, FLT, REG_AP2V_ATTR6,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR7",  0, FLT, REG_AP2V_ATTR7,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR8",  0, FLT, REG_AP2V_ATTR8,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR9",  0, FLT, REG_AP2V_ATTR9,  4, REG_ALLOC | REG_INPUT, },
	{ "ATTR10", 0, FLT, REG_AP2V_ATTR10, 4, REG_ALLOC | REG_INPUT, },
	{ "ATTR11", 0, FLT, REG_AP2V_ATTR11, 4, REG_ALLOC | REG_INPUT, },
	{ "ATTR12", 0, FLT, REG_AP2V_ATTR12, 4, REG_ALLOC | REG_INPUT, },
	{ "ATTR13", 0, FLT, REG_AP2V_ATTR13, 4, REG_ALLOC | REG_INPUT, },
	{ "ATTR14", 0, FLT, REG_AP2V_ATTR14, 4, REG_ALLOC | REG_INPUT, },
	{ "ATTR15", 0, FLT, REG_AP2V_ATTR15, 4, REG_ALLOC | REG_INPUT, },
};

static ConnectorRegisters outputCRegs_generic[] = {
// These are output register names
	{ "HPOS",  0, FLT, REG_V2FR_HPOS,  4, REG_RESERVED | REG_OUTPUT | REG_WRITE_REQUIRED, },
	{ "COL0",  0, FLT, REG_V2FR_COL0,  4, REG_RESERVED | REG_OUTPUT, },
	{ "COL1",  0, FLT, REG_V2FR_COL1,  4, REG_RESERVED | REG_OUTPUT, },
	{ "TEX0",  0, FLT, REG_V2FR_TEX0,  4, REG_ALLOC | REG_OUTPUT, },
	{ "TEX1",  0, FLT, REG_V2FR_TEX1,  4, REG_ALLOC | REG_OUTPUT, },
	{ "TEX2",  0, FLT, REG_V2FR_TEX2,  4, REG_ALLOC | REG_OUTPUT, },
	{ "TEX3",  0, FLT, REG_V2FR_TEX3,  4, REG_ALLOC | REG_OUTPUT, },
	{ "FOGC",  0, FLT, REG_V2FR_FOGC,  1, REG_RESERVED | REG_OUTPUT, },
	{ "PSIZ",  0, FLT, REG_V2FR_PSIZ,  1, REG_RESERVED | REG_OUTPUT, },
};


// Semantics:
enum { AP2V_GROUP = 0, V2FR_GROUP = 1, };

static SemanticsDescriptor Semantics_generic[] = {
// These are semantics that can be attached to varying variables and
// parameters.  They usually correspond to the input and output registers
// defined above, but don't have to.  You can add multiple names for the
// the same thing as aliases
	// Varying input semantics:
	{ "ATTRIB",   FLT, 4, REG_AP2V_ATTR0, 16, AP2V_GROUP, SEM_IN | SEM_VARYING, },
	// Varying output semantics:
	{ "POSITION", FLT, 4, REG_V2FR_HPOS, 1, V2FR_GROUP, SEM_OUT | SEM_VARYING, },
	{ "FOG",      FLT, 1, REG_V2FR_FOGC, 0, V2FR_GROUP, SEM_OUT | SEM_VARYING, },
	{ "COLOR",    FLT, 4, REG_V2FR_COL0, 2, V2FR_GROUP, SEM_OUT | SEM_VARYING, },
	{ "PSIZE",    FLT, 1, REG_V2FR_PSIZ, 0, V2FR_GROUP, SEM_OUT | SEM_VARYING, },
	{ "TEXCOORD", FLT, 4, REG_V2FR_TEX0, 4, V2FR_GROUP, SEM_OUT | SEM_VARYING, },
};


// These are the connector types which refer to the register names above
static ConnectorDescriptor connectors_generic[] = {
	{ CID_GENERIC_IN_NAME,  0, CID_GENERIC_IN_ID,  CONNECTOR_IS_INPUT,  NUMELS(inputCRegs_generic),  inputCRegs_generic  },

	{ CID_GENERIC_OUT_NAME, 0, CID_GENERIC_OUT_ID, CONNECTOR_IS_OUTPUT, NUMELS(outputCRegs_generic), outputCRegs_generic },
};


///////////////////////////////////////////////////////////////////////////////
/////////////////////////// Generic output Program ////////////////////////////
///////////////////////////////////////////////////////////////////////////////

HAL_generic::HAL_generic() {
	vendor	= VENDOR_STRING_GENERIC;
	version = VERSION_STRING_GENERIC;
}


extern "C" {
ProfileRegistration	generic_reg(HAL::create<HAL_generic>, PROFILE_GENERIC_NAME, PROFILE_GENERIC_ID);
}

void HAL_generic::RegisterNames(CG *cg) {
	// Add atoms for connectors and connector registers.
	for (int i = 0; i < NUMELS(connectors_generic); i++) {
		ConnectorDescriptor * conn = &connectors_generic[i];
		conn->name = cg->AddAtom(conn->sname);
		for (int j = 0; j < conn->numregs; j++)
			conn->registers[j].name = cg->AddAtom(conn->registers[j].sname);
	}
}

int HAL_generic::GetConnectorID(int name) {
	for (int i = 0; i < NUMELS(connectors_generic); i++)
		if (name == connectors_generic[i].name)
			return connectors_generic[i].cid;

	return 0;
}

int HAL_generic::GetConnectorUses(int cid, int pid) {
	const ConnectorDescriptor *conn = LookupConnectorHAL(connectors_generic, cid, NUMELS(connectors_generic));
	return conn ? conn->properties : 0;
}

bool HAL_generic::GetCapsBit(int bit) {
	switch (bit) {
		case CAPS_INDEXED_ARRAYS:
		case CAPS_DONT_FLATTEN_IF_STATEMENTS:
			return true;
		default:
			return false;
	}
}

int HAL_generic::GetInternalFunction(Symbol *s, int *group) {
	return 1;
}

bool HAL_generic::BindUniformUnbound(CG *cg, SourceLoc &loc, Symbol *s,  Binding *bind) {
	bind->properties |= BIND_IS_BOUND;
	return true;
}

bool HAL_generic::BindUniformPragma(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind, const Binding *pragma) {
	*bind	= *pragma;
	bind->properties |= BIND_IS_BOUND | BIND_UNIFORM;
	return true;
}

bool HAL_generic::BindVaryingSemantic(CG *cg, SourceLoc &loc, Symbol *fSymb, int semantic, Binding *bind, bool is_out_val) {
	int					index;
	char				root[128];

	const char			*pname		= cg->GetAtomString(semantic);
	bool				has_suffix	= HasNumericSuffix(pname, root, 128, &index);
	SemanticsDescriptor *semantics	= Semantics_generic;

	for (int i = 0; i < NUMELS(Semantics_generic); i++, semantics++) {
		const char *match = semantics->numregs > 0 ? root : pname;
		if (!strcmp(match, semantics->sname)) {
			if (semantics->numregs > 0) {
				if (index >= semantics->numregs) {
					SemanticError(cg, loc, ERROR_S_SEMANTICS_INDEX_TOO_BIG, pname);
					return false;
				}
			} else {
				index = 0;
			}

			// Found a match.  See if the type is compatible:

			Type	*lType = fSymb->type;
			int		len;
			if (IsScalar(lType)) {
				len = 1;
			} else if (!IsVector(lType, &len)) {
				SemanticError(cg, loc, ERROR_S_SEM_VAR_NOT_SCALAR_VECTOR,  cg->GetAtomString(fSymb->name));
				return false;
			}
			if (GetBase(lType) != TYPE_BASE_FLOAT)
				return false;

			if (semantics->properties & SEM_VARYING) {
				bind->kind = BK_SEMANTIC;
				bind->rname = semantic;
				bind->regno = semantics->regno + index;
				fSymb->properties |= SYMB_IS_CONNECTOR_REGISTER;
			} else {
				bind->kind = BK_SEMANTIC;
				bind->rname = semantic;
				bind->regno = 0;
			}
			bind->properties |= BIND_IS_BOUND | BIND_VARYING;
			if (semantics->properties & SEM_HIDDEN)
				bind->properties |= BIND_HIDDEN;
			fSymb->properties |= SYMB_CONNECTOR_CAN_READ; // Obsolete
			fSymb->properties |= SYMB_CONNECTOR_CAN_WRITE; // Obsolete
			if (semantics->properties & SEM_IN)
				bind->properties |= BIND_INPUT;
			if (semantics->properties & SEM_OUT)
				bind->properties |= BIND_OUTPUT;
			if (semantics->properties & SEM_REQUIRED)
				bind->properties |= BIND_WRITE_REQUIRED;
			// bind->none,gname set elsewhere
			// bind->none.lname set elsewhere
			bind->size = semantics->size;
			return true;
		}
	}
	return false;
}

static void PrintFunctions(CG *cg, Symbol *symb) {
	Symbol *fSymb;
	if (symb) {
		PrintFunctions(cg, symb->left);
		if (IsFunction(symb)) {
			fSymb = symb;
			while (fSymb) {
				PrintFunction(cg, fSymb);
				BPrintFunction(cg, fSymb);
				fSymb = fSymb->fun.overload;
			}
		}
		PrintFunctions(cg, symb->right);
	}
}

int HAL_generic::GenerateCode(CG *cg, SourceLoc &loc, Scope *fScope, Symbol *program) {
	if (fScope)
		PrintFunctions(cg, fScope->symbols);
	return 0;
}