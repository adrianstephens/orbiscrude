#ifndef BINDING_H
#define BINDING_H

#include "scanner.h"

namespace cgc {

struct MemoryPool;

enum BindingKinds {
	BK_NONE,
	BK_CONNECTOR,
	BK_TEXUNIT,
	BK_REGARRAY,
	BK_CONSTANT,
	BK_DEFAULT,
	BK_SEMANTIC,
	BK_CONSTBUFFER,
};

// Properties bits:

#define BIND_IS_BOUND           0x0001
#define BIND_HIDDEN             0x0002
#define BIND_UNIFORM            0x0004
#define BIND_VARYING            0x0008
#define BIND_INPUT              0x0010
#define BIND_OUTPUT             0x0020
#define BIND_WRITE_REQUIRED     0x0040
#define BIND_WAS_WRITTEN        0x0080

struct Binding {
	int					properties;	// Properties
	int					gname;		// Global name
	int					lname;		// Local name
//	int					base;		// type base
	int					size;		// num of elements
	BindingKinds		kind;
	int					rname;		// HW Register name or semantic name
	int					regno;		// or unit or semantic index

	struct RegArray {
		int				count;	// Number of registers allocated
		const Binding	*parent;
	};
	struct ConstDefault {
		float			*val;	// Values
	};
	
	union {
		RegArray		reg;
		ConstDefault	constdef;
	};

	Binding(int gname = 0, int lname = 0, BindingKinds _kind = BK_NONE);
};

struct BindingList {
	BindingList		*next;
	Binding			*binding;
	BindingList(Binding *_binding) : next(0), binding(_binding) {}
};

struct BindingTree {
	BindingTree		*nextc;		// Next connector name in a list
	BindingTree		*nextm;		// Next member in this connector
	SourceLoc		loc;		// Source location of #pragma bind
	Binding			binding;	// Binding info.
	BindingTree(SourceLoc &_loc, int _gname = 0, int _lname = 0, BindingKinds _kind = BK_NONE)
		: binding(_gname, _lname, _kind), nextc(0), nextm(0), loc(_loc)
	{ loc.line = 0;}
};

struct UniformSemantic {
	UniformSemantic *next;
	int				gname;
	int				vname;
	int				semantic;

	UniformSemantic(int _gname, int _vname, int _semantic) : next(0), gname(_gname), vname(_vname), semantic(_semantic) {}
};

Binding			*DupBinding(MemoryPool *pool, Binding *fBind);
Binding			*NewConstDefaultBinding(MemoryPool *pool, int gname, int sname, int count, int rname, int regno, float *fval);

BindingTree		*NewConnectorBindingTree(MemoryPool *pool, SourceLoc &loc, int cname, int mname, int rname);
BindingTree		*NewRegArrayBindingTree(MemoryPool *pool, SourceLoc &loc, int pname, int aname, int rname, int regno, int count);
BindingTree		*NewTexunitBindingTree(MemoryPool *pool, SourceLoc &loc, int pname, int aname, int unitno);
BindingTree		*NewConstDefaultBindingTree(MemoryPool *pool, SourceLoc &loc, BindingKinds kind, int pname, int aname, int count, float *fval);
BindingTree		*LookupBinding(BindingTree *bindings, int gname, int lname);
void 			AddBinding(BindingList **bindings, BindingList *b);
void			AddBinding(BindingTree **bindings, BindingTree *b);

} //namespace cgc

#endif // BINDING_H

