#ifndef HAL_H
#define HAL_H

#include "support.h"

namespace cgc {

struct HAL;
struct Profile;

// Profile and connector IDs for non-programs and non-connectors:

#define PID_NONE_ID			0
#define CID_NONE_ID			0
#define CID_INVALID_ID		1 // Marks connector as invalid to prevent multiple errors

// Connector capabilities bits returned by GetConnectorUses:

enum HAL_CONNECTOR {
	CONNECTOR_IS_USELESS	= 0x0000,
	CONNECTOR_IS_INPUT		= 0x0001,
	CONNECTOR_IS_OUTPUT		= 0x0002,
};
enum HAL_REG {
	REG_NONE				= 0x0000,
	REG_ALLOC				= 0x0001,
	REG_RESERVED			= 0x0002,
	REG_HIDDEN				= 0x0004,
	REG_WRITE_REQUIRED		= 0x0008,
	REG_INPUT				= 0x0010,
	REG_OUTPUT				= 0x0020,
};
enum HAL_CAPS {
	CAPS_INLINE_ALL_FUNCTIONS,
	CAPS_RESTRICT_RETURNS,
	CAPS_DECONSTRUCT_MATRICES,
	CAPS_DECONSTRUCT_VECTORS,
	CAPS_LATE_BINDINGS,
	CAPS_INDEXED_ARRAYS,
	CAPS_DONT_FLATTEN_IF_STATEMENTS,
	CAPS_NO_OPTIMISATION,
	CAPS_FX,
};

struct Profile {
	Profile*	next;
	HAL*		(*CreateHAL)(CG*);
	int			name;
	int			id;
};

struct ProfileRegistration {
	static ProfileRegistration *&head() {
		static ProfileRegistration *head = 0;
		return head;
	}
	ProfileRegistration	*next;
	HAL*				(*create)(CG*);
	const char			*name;
	int					id;

	ProfileRegistration(HAL* (*create)(CG*), const char *name, int id) : create(create), name(name), id(id) {
		ProfileRegistration	*&link = head(); next = link; link = this;
	}
};

// Hal version of connector register description:
struct ConnectorRegisters {
	const char*	sname;
	int			name;
	int			base;
	int			regno;
	int			size;
	int			properties;
};

struct ConnectorDescriptor {
	const char*	sname;
	int			name;
	int			cid;
	int			properties;
	int			numregs;
	ConnectorRegisters *registers;
};

// Hal version of "semantics"	descriptions:
enum SemanticProperties {
	SEM_IN = 1, SEM_OUT = 2, SEM_UNIFORM = 4, SEM_VARYING = 8, SEM_HIDDEN = 16,
	SEM_EXCLUSIVE = 32, SEM_REQUIRED = 64
};

struct SemanticsDescriptor {
	const char*	sname;
	int			base;
	int			size;
	int			regno;
	int			numregs;
	int			reggroup;
	int			properties;
};

struct HAL {
	// Function members:
	virtual void	RegisterNames			(CG *cg);
	virtual bool	GetCapsBit				(int bit);
	virtual int		GetConnectorID			(int bit);
	virtual int		GetConnectorUses		(int cid, int pid);
	virtual int		GetFloatSuffixBase		(SourceLoc &loc, int suffix);
	virtual int		GetSizeof				(Type *t);
	virtual int		GetAlignment			(Type *t);
	virtual int		GetInternalFunction		(Symbol *s, int *group);

	virtual bool	CheckDeclarators		(CG *cg, SourceLoc &loc, const dtype *fDtype);
	virtual bool	CheckDefinition			(CG *cg, SourceLoc &loc, int name, const Type *t);
	virtual bool	CheckStatement			(CG *cg, SourceLoc &loc, stmt *s);

	virtual bool	IsNumericBase			(int base);
	virtual bool	IsIntegralBase			(int base);
	virtual bool	IsTexobjBase			(int base);
	virtual bool	IsValidRuntimeBase		(int base);
	virtual bool	IsValidScalarCast		(int toBase, int fromBase, bool Explicit);
	virtual bool	IsValidOperator			(SourceLoc &loc, int name, int op, int subop);

	virtual expr*	FoldInternalFunction	(CG *cg, int group, int index, expr *args);
	virtual int		GetBinOpBase			(int lop, int lbase, int rbase, int llen, int rlen);
	virtual bool	ConvertConstant			(CG *cg, const scalar_constant *val, int fbase, int tbase, expr **e);

	virtual bool	BindUniformUnbound		(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind);
	virtual bool	BindUniformPragma		(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind, const Binding *pragma);
	virtual bool	BindVaryingSemantic		(CG *cg, SourceLoc &loc, Symbol *s, int semantic, Binding *bind, bool is_out_val);
	virtual bool	BindVaryingPragma		(CG *cg, SourceLoc &loc, Symbol *s, Binding *bind, const Binding *pragma, bool is_out_val);
	virtual bool	BindVaryingUnbound		(CG *cg, SourceLoc &loc, Symbol *s, int name, int semantic, Binding *bind, bool is_out_val);

	virtual int		GenerateCode			(CG *cg, SourceLoc &loc, Scope *s, Symbol *program);

	// Profile specific data members:

	const char		*vendor;
	const char		*version;
	int				profileName;
	int				pid;

	HAL() : vendor("None"), version("0.0"), profileName(0), pid(0)	{}
	virtual ~HAL() {}

	template<typename T> static HAL* create(CG *cg)		{ return new T; }
	template<typename T> static HAL* create_cg(CG *cg)	{ return new T(cg); }
};

ConnectorDescriptor *LookupConnectorHAL(ConnectorDescriptor *connectors, int cid, int num);
void SetSymbolConnectorBindingHAL(Binding *fBind, ConnectorRegisters *fConn);

} //namespace cgc

#endif // HAL_H
