#ifndef DEMANGLE_H
#define DEMANGLE_H

#include "base/defs.h"
#include "base/strings.h"

enum DEMANGLE_FLAGS {
	FLAG_RESTRICT	= 1<<0,
	FLAG_VOLATILE	= 1<<1,
	FLAG_CONST		= 1<<2,
	FLAG_STATIC		= 1<<3,

	FLAG_EXTERNC	= 1<<8,
	FLAG_BUILTIN	= 1<<9,
	FLAG_SUBST		= 1<<10,

	FLAG_CV			= FLAG_RESTRICT | FLAG_VOLATILE | FLAG_CONST,
};

enum DEMANGLE_OPERATOR {
	OP_nw,	// new
	OP_na,	// new[]
	OP_dl,	// delete
	OP_da,	// delete[]
	OP_ps,	// + (unary)
	OP_ng,	// - (unary)
	OP_ad,	// & (unary)
	OP_de,	// * (unary)
	OP_co,	// ~
	OP_pl,	// +
	OP_mi,	// -
	OP_ml,	// *
	OP_dv,	// /
	OP_rm,	// %
	OP_an,	// &
	OP_or,	// |
	OP_eo,	// ^
	OP_aS,	// =
	OP_pL,	// +=
	OP_mI,	// -=
	OP_mL,	// *=
	OP_dV,	// /=
	OP_rM,	// %=
	OP_aN,	// &=
	OP_oR,	// |=
	OP_eO,	// ^=
	OP_ls,	// <<
	OP_rs,	// >>
	OP_lS,	// <<=
	OP_rS,	// >>=
	OP_eq,	// ==
	OP_ne,	// !=
	OP_lt,	// <
	OP_gt,	// >
	OP_le,	// <=
	OP_ge,	// >=
	OP_nt,	// !
	OP_aa,	// &&
	OP_oo,	// ||
	OP_pp,	// ++ (postfix in <expression> context)
	OP_mm,	// -- (postfix in <expression> context)
	OP_cm,	// ,
	OP_pm,	// ->*
	OP_pt,	// ->
	OP_cl,	// ()
	OP_ix,	// []
	OP_qu,	// ?

	OP_st,	// sizeof (a type)
	OP_sz,	// sizeof (an expression)
	OP_at,	// alignof (a type)
	OP_az,	// alignof (an expression)
	OP_cv,	// <type>		// (cast)
//	OP_v0	// <digit> <source-name>		// vendor extended operator

	OP_Ut,	// unnamed type

	OP_C1,	// complete object constructor
	OP_C2,	// base object constructor
	OP_C3,	// complete object allocating constructor
	OP_D0,	// deleting destructor
	OP_D1,	// complete object destructor
	OP_D2,	// base object destructor

//	OP_Dt,	// <expression> E  # decltype of an id-expression or class member access (C++0x)
//	OP_DT,	// <expression> E  # decltype of an expression (C++0x)
	OP_gs,	//
	OP_ti,	// typeid (type)
	OP_te,	// typeid (expression)
	OP_dc,	// dynamic_cast<type> (expression)
	OP_sc,	// static_cast<type> (expression)
	OP_rc,	// reinterpret_cast<type> (expression)
	OP_dt,	// expr.name
	OP_ds,	// expr.*expr
	OP_sp,	// pack expansion
	OP_tw,	// throw expression
	OP_tr,	// throw with no operand (rethrow)
	OP_sr,	// T::x
};

enum DR_TYPE {
	DR_none,	DR_name, DR_list, DR_local, DR_nested, DR_function, DR_rettype, DR_template_param, DR_template_arg,
	DR_array,	DR_pointer, DR_reference, DR_pointer2member, DR_rvalue_ref, DR_complex, DR_imaginary,
	DR_operator,DR_literal, DR_qualifier,
	DR_string,
	DR_guard_variable, DR_virtual_table, DR_vtt_struc, DR_typeinfo_struc, DR_typeinfo_name, DR_thunk, DR_thunk_v,
};

class Demangler {
public:
	struct Record {
		DR_TYPE		type;
		Record		*child;
		int			flags;
		union {
			int			value;
			const char	*name;
			Record		*child2;
		};
		Record(DR_TYPE _type = DR_none, Record *_child = 0, int _flags = 0) : type(_type), child(_child), flags(_flags) {}
		~Record();
	};

protected:
	Record *root;
public:
	Demangler() : root(0)	{}

	operator Record*() const	{ return root; }
};

iso::string demangle(const char *m);
iso::string enmangle(const char *m);

#endif// DEMANGLE_H
