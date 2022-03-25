#ifndef AST_H
#define AST_H

#include "c-types.h"
#include "maths/graph.h"
#include "base/list.h"
#include "base/tree.h"

#undef OVERFLOW

namespace iso { namespace ast {

//-----------------------------------------------------------------------------
//	nodes
//-----------------------------------------------------------------------------

enum KIND {
	_plain,
		none = _plain,
		vararg,
		null,
		end,
		ret,
		rethrw,
		breakstmt,
		continuestmt,
		_plain_end = continuestmt,

//leaf types
	name,
	get_size,

//element
	_element,
		element = _element,
		var,
		decl,
		func, vfunc, jmp,
	_element_end = jmp,

//unary,
	_unary,
		neg = _unary,
		pos,
		bit_not,
		log_not,
		dec,
		inc,
		deref,
		ref,
		alloc_stack,
		get_type,
		refanyval,
		ckfinite,
		thrw,
		retv,
		expression,

		_unarytype,
			initobj = _unarytype,
			cast,
			load,
			box,
			newarray,
			mkrefany,
			castclass,
			isinst,
	_unary_end = isinst,

//binary,
	_binary,
		add = _binary,
		sub,
		mul,
		div,
		mod,
		bit_and,
		bit_or,
		bit_xor,
		shl,
		shr,

		assign,
		add_assign,
		sub_assign,
		mul_assign,
		div_assign,
		mod_assign,
		and_assign,
		or_assign,
		xor_assign,
		shl_assign,
		shr_assign,

		log_and,
		log_or,

		eq,
		ne,
		lt,
		gt,
		le,
		ge,
		spaceship,

		field,
		deref_field,
		comma,
		query,
		colon,

		_binarytype,
			ldelem = _binarytype,
			assignobj,
			copyobj,
	_binary_end = copyobj,

//call
	call, swtch,

//other
	literal,
	substitute,
	branch,
	whileloop,
	forloop,
	ifelse,
	basic_block,

	custom = 0x1000,
};

//inline bool check(KIND k)		{ return true; }
inline bool is_plain(KIND k)	{ return between(k, _plain, _plain_end); }
inline bool is_element(KIND k)	{ return between(k, _element, _element_end); }
inline bool is_binary(KIND k)	{ return between(k, _binary, _binary_end); }
inline bool is_unary(KIND k)	{ return between(k, _unary, _unary_end); }
inline bool is_custom(KIND k)	{ return k >= custom; }

enum FLAGS {
	ALIGNMENT	= 1,
	VOLATILE	= 1 << 8,
	READONLY	= 1 << 9,
	ASSIGN		= 1 << 10,	// var used in assignment
//	TYPED		= 1 << 11,	// node contains type

	ALLOC		= 1 << 12,	// call allocates mem for 1st param
	FIXED		= 1 << 12,	// switch has been fixed up (so can be descended down by apply_statement)
	NOLOOKUP	= 1 << 12,	// name is not a variable (it's a field, say)

	INLINE		= 1 << 13,	// inline call
	UNSIGNED	= 1 << 13,	// for integer types
	UNORDERED	= 1 << 13,	// for float compares
	OVERFLOW	= 1 << 14,

	ADDRESS		= 1 << 15,	// storing address of literal (not literal itself)
	POINTER		= ALLOC,	// literal is really a pointer to sepecified type
	LOCALMEM	= INLINE,	// literal points to local memory
};

struct node : refs<node> {
	KIND			kind;
	uint32			flags;
	const C_type	*type;

	template<class C> struct caster {
		C	&c;
		caster(C &_c) : c(_c) {}
		template<typename T> operator T*() const { return c.template cast<T>(); }
	};

	constexpr node(KIND kind, uint32 flags = 0, const C_type *type = 0) : kind(kind), flags(flags), type(type) {}
	virtual ~node()			{}
	virtual node*			flip_condition();
	virtual bool			operator==(const node &b)	const	{ return kind == b.kind; }// && flags == b.flags; }

	node*		flip_condition(bool b)				{ return b ? flip_condition() : this; }
	bool		operator!=(const node &b)	const	{ return !(*this == b); }
	bool		is_plain()					const	{ return ast::is_plain(kind); }
	bool		is_element()				const	{ return ast::is_element(kind); }
	bool		is_binary()					const	{ return ast::is_binary(kind); }
	bool		is_unary()					const	{ return ast::is_unary(kind); }
	bool		is_custom()					const	{ return ast::is_custom(kind); }
	bool		branches(bool always = false) const;

	template<typename T> const T*	cast()	const	{ return T::check(kind) ? static_cast<const T*>(this) : 0; }
	template<typename T> T*			cast()			{ return T::check(kind) ? static_cast<T*>(this) : 0; }
	caster<node>					cast()			{ return *this; }
	caster<const node>				cast()	const	{ return *this; }
};

typedef callback_ref<bool(uint64, void*, size_t)>		get_memory_t;
typedef callback_ref<ast::node*(const char*)>			get_variable_t;
typedef callback_ref<void(string_accum&, const char*)>	demangle_t;


struct custom_node : node {
	static bool check(KIND k) { return k >= custom; }
	constexpr custom_node(KIND kind, uint32 flags = 0, const C_type *type = 0) : node(kind, flags, type) {}
	virtual node *apply(const callback_ref<node*(node*)> &t) = 0;
};

struct plain_node : node {
	struct info {
		const char *name;
		int			statement:1;
	};
	const info *get_info() {
		static const info infos[] = {
			{"<?plain>",	0,	},
			{"valist",		0,	},
			{"nil",			0,	},
			{"return",		1,	},
			{"return",		1,	},
			{"rethrow",		1,	},
			{"break",		1,	},
			{"continue",	1,	},
		};
		return &infos[kind - _plain];
	}
	static bool check(KIND k) { return ast::is_plain(k); }

	plain_node(KIND kind, uint32 flags = 0) : node(kind, flags) {}
};

struct name_node : node {
	string			id;
	static bool check(KIND k) { return k == name; }

	name_node(const char *id, uint32 flags = 0) : node(name, flags), id(id) {}
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& id == ((name_node&)b).id;
	}
};

struct element_node : node {
	const C_element*	element;
	const C_type*		parent;
	static bool check(KIND k) { return ast::is_element(k); }

	element_node(KIND kind, const C_element *element, const C_type *parent = 0, uint32 flags = 0) : node(kind, flags, element->type), element(element), parent(parent) {}
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& element	== ((element_node&)b).element;
	}
};

struct decl_node : element_node {
	decl_node(const C_arg *arg, const C_type *parent = 0) : element_node(decl, (C_element*)arg, parent) {}
};

struct funcdecl_node : decl_node {
	funcdecl_node(const C_arg *func, const C_type *parent) : decl_node(func, parent) {}
	const C_arg	*get_arg(int i) const {
		C_type_function	*f = (C_type_function*)element->type;
		return &f->args[i];
	}
	const C_type *get_rettype() const {
		return ((C_type_function*)element->type)->subtype;
	}
};

struct unary_node : node {
	ref_ptr<node>	arg;
	static bool check(KIND k) { return ast::is_unary(k); }

	unary_node(KIND kind, node *arg, uint32 flags = 0) : node(kind, flags, arg ? arg->type : nullptr), arg(arg) {}
	unary_node(KIND kind, node *arg, const C_type *type, uint32 flags = 0) : node(kind, flags, type), arg(arg) {}
	node *flip_condition() {
		if (kind == log_not)
			return arg;
		return new unary_node(log_not, this);
	}
	bool operator==(const node &b) const {
		return kind == b.kind
			&& arg == ((unary_node&)b).arg;
	}
	struct info {
		const char *name;
		unsigned	precedence:4, parentheses:2, statement:1;
		// none, parentheses, angle brackets
	};
	const info *get_info() {
		static const info infos[] = {
			{"-",			13, 0, 0, },	//neg,
			{"+",			13, 0, 0, },	//pos,
			{"~",			13, 0, 0, },	//bit_not,
			{"!",			13, 0, 0, },	//log_not,
			{"--",			13, 0, 0, },	//dec,
			{"++",			13, 0, 0, },	//inc,
			{"*",			0,	1, 0, },	//deref
			{"&",			13, 0, 0, },	//ref,
			{"alloca",		0,  1, 0, },	//alloca,
			{"typeof",		13, 1, 0, },	//type,
			{"",			13, 0, 0, },	//refanyval,
			{"ckfinite",	0,  0, 0, },	//ckfinite,
			{"throw ",		1,  0, 1, },	//thrw,
			{"return ",		13, 0, 1, },	//retv,
			{"",			0,	0, 1, },	//expression,
			//with type
			//{"*static_cast",0,	1, 0, },	//deref_type
			//{"&",			13, 0, 0, },	//ref_type,
			{"initobj",		0,  2, 0, },	//initobj,
			{"static_cast",	0,  2, 0, },	//cast,
			{"*static_cast",0,  2, 0, },	//load,
			{"static_cast",	0,  2, 0, },	//box,
			{"array",		0,  2, 0, },	//newarray,
			{"mkrefany",	0,  2, 0, },	//mkrefany,
			{"static_cast",	0,  2, 0, },	//castclass,
			{"as_cast",		0,  2, 0, },	//isinst,
		};
		return &infos[kind - _unary];
	}
};

inline node *node::flip_condition() {
	return new unary_node(log_not, this);
}

struct binary_node : node {
	ref_ptr<node>	left, right;
	static bool check(KIND k) { return ast::is_binary(k); }

	binary_node(KIND kind, node *left, node *right, uint32 flags = 0) : node(kind, flags, right ? right->type : nullptr), left(left), right(right) {}
	binary_node(KIND kind, node *left, node *right, const C_type *type, uint32 flags = 0) : node(kind, flags, type), left(left), right(right) {}

	struct info {
		enum NOT_MODE : uint8 { NOT, OP, FLIP, RECURSE};
		const char *name;
		unsigned	precedence:4;
		int			spaces:1;
		int			statement:1;
		NOT_MODE	not_mode:2;
		KIND		not_kind:8;
	};

	static const info *get_info(KIND kind) {
		static const info infos[] = {
			{"+",	10,	1, 0, info::NOT,		log_not,	},	//add,
			{"-",	10,	1, 0, info::NOT,		log_not,	},	//sub,
			{"*",	11,	1, 0, info::NOT,		log_not,	},	//mul,
			{"/",	11,	1, 0, info::NOT,		log_not,	},	//div,
			{"%",	11,	1, 0, info::NOT,		log_not,	},	//mod,
			{"&",	6,	1, 0, info::NOT,		log_not,	},	//bit_and,
			{"|",	4,	1, 0, info::NOT,		log_not,	},	//bit_or,
			{"^",	5,	1, 0, info::NOT,		log_not,	},	//bit_xor,
			{"<<",	9,	1, 0, info::NOT,		log_not,	},	//shl,
			{">>",	9,	1, 0, info::NOT,		log_not,	},	//shr,
			{"=",	1,	1, 1, info::NOT,		log_not,	},	//assign,
			{"+=",	1,	1, 1, info::NOT,		log_not,	},	//add_assign,
			{"-=",	1,	1, 1, info::NOT,		log_not,	},	//sub_assign,
			{"*=",	1,	1, 1, info::NOT,		log_not,	},	//mul_assign,
			{"/=",	1,	1, 1, info::NOT,		log_not,	},	//div_assign,
			{"%=",	1,	1, 1, info::NOT,		log_not,	},	//mod_assign,
			{"&=",	1,	1, 1, info::NOT,		log_not,	},	//and_assign,
			{"|=",	1,	1, 1, info::NOT,		log_not,	},	//or_assign,
			{"^=",	1,	1, 1, info::NOT,		log_not,	},	//xor_assign,
			{"<<=",	1,	1, 1, info::NOT,		log_not,	},	//shl_assign,
			{">>=",	1,	1, 1, info::NOT,		log_not,	},	//shr_assign,
			{"&&",	3,	1, 0, info::RECURSE,	log_or,		},	//log_and,
			{"||",	2,	1, 0, info::RECURSE,	log_and,	},	//log_or,
			{"==",	7,	1, 0, info::OP,			ne,			},	//eq,
			{"!=",	7,	1, 0, info::OP,			eq,			},	//ne,
			{"<",	8,	1, 0, info::OP,			ge,			},	//lt,
			{">",	8,	1, 0, info::OP,			le,			},	//gt,
			{"<=",	8,	1, 0, info::OP,			gt,			},	//le,
			{">=",	8,	1, 0, info::OP,			lt,			},	//ge,
			{"<=>",	8,	1, 0, info::NOT,		log_not,	},	//spaceship,
			{"->",	12,	0, 0, info::NOT,		log_not,	},	//field,
			{"->*",	12,	0, 0, info::NOT,		log_not,	},	//deref_field,
			{",",	0,	1, 0, info::NOT,		log_not,	},	//comma,
			{"?",	1,	1, 0, info::NOT,		log_not,	},	//query
			{":",	1,	1, 0, info::NOT,		log_not,	},	//colon
			{"[",	12,	1, 0, info::NOT,		log_not,	},	//ldelem
		};
		return &infos[kind - _binary];
	}
	const info *get_info() {
		return get_info(kind);
	}

	node *flip_condition() {
		const info *info = get_info();
		switch (info::NOT_MODE(info->not_mode&3)) {
			default:			return new unary_node(log_not, this);
			case info::OP:		return new binary_node(info->not_kind, left, right);
			case info::FLIP:	return new binary_node(info->not_kind, right, left);
			case info::RECURSE:	return new binary_node(info->not_kind, right->flip_condition(), left->flip_condition());
		}
	}
	bool operator==(const node &b) const {
		return kind		== b.kind
			&& left		== ((binary_node&)b).left
			&& right	== ((binary_node&)b).right;
	}
};

struct lit_node : node {
	uint64	v;
	string	id;
	static bool check(KIND k) { return k == literal; }

	lit_node(const C_value &value, uint32 flags = 0)							: node(literal, flags, value.type), v(value.v) {}
	lit_node(const C_type *type, uint64 v, uint32 flags = 0)					: node(literal, flags, type), v(v) {}
	lit_node(const char *id, const C_value &value, uint32 flags = 0)			: node(literal, flags, value.type), v(value.v), id(id) {}
	lit_node(const char *id, const C_type *type, uint64 v, uint32 flags = 0)	: node(literal, flags, type), v(v), id(id) {}

	bool operator==(const node &b) const {
		return node::operator==(b)
			&& type		== ((lit_node&)b).type
			&& v		== ((lit_node&)b).v;
	}
	C_value	get_value(get_memory_t get_memory) const {
		const C_type	*type2	= type;
		uint64			v2		= v;

		if ((flags & ADDRESS) && type->type != C_type::ARRAY) {
			size_t	size = type->size();
			ISO_ASSERT(size <= sizeof(v2));
			v2 = 0;
			if (flags & LOCALMEM)
				memcpy(&v2, (void*)v, size);
			else if (get_memory)
				get_memory(v, &v2, size);
		}

		if (auto type3 = IsReference(type2)) {
			if (get_memory)
				get_memory(v2, &v2, 8);
			type2 = type3;
		}
		return C_value(type2, v2);
	}
	void	set_value(const C_value &val) {
		v		= val.v;
		type	= val.type;
		flags	&= ~ADDRESS;
	}

	template<typename T, int I> static lit_node *get() {
		static lit_node	lit(C_types::get_static_type<T>(), T(I));
		return &lit;
	}
};

struct call_node : node {
	ref_ptr<node>					func;
	dynamic_array<ref_ptr<node> >	args;
	static bool check(KIND k) { return k == call; }

	call_node(node *func, uint32 flags = 0) : node(call, flags), func(func) {}
	void	add(node *n)			{ args.push_back(n); }
	template<typename...P> void	add(node *n, P&&...p)	{ add(n); add(p...); }
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& func		== ((call_node&)b).func
			&& args		== ((call_node&)b).args;
	}
};

struct substitute_node : binary_node {
	string	id;
	substitute_node(const char *id, node *left, node *right, uint32 flags = 0) : binary_node(substitute, left, right, flags), id(id) {}
};

//-----------------------------------------------------------------------------
//	noderef helper
//-----------------------------------------------------------------------------

struct noderef : ref_ptr<node> {
	template<typename T> static noderef make(T t)	{ return new lit_node(C_value(t)); }

	noderef(node *p = nullptr)	: ref_ptr<node>(p) {}

	noderef operator-	() const	{ return new unary_node(neg, p); }
	noderef operator+	() const	{ return new unary_node(pos, p); }
	noderef operator~	() const	{ return new unary_node(bit_not, p); }
//	noderef operator!	() const	{ return new unary_node(log_not, p); }
	noderef operator--	() const	{ return new unary_node(dec, p); }
	noderef operator++	() const	{ return new unary_node(inc, p); }
	noderef operator*	() const	{ return new unary_node(deref, p, p->type->subtype()); }
	noderef operator&	() const	{ return new unary_node(ref, p); }

	noderef operator+	(const noderef &b) const	{ return new binary_node(add, p, b); }
	noderef operator-	(const noderef &b) const	{ return new binary_node(sub, p, b); }
	noderef operator*	(const noderef &b) const	{ return new binary_node(mul, p, b); }
	noderef operator/	(const noderef &b) const	{ return new binary_node(div, p, b); }
	noderef operator%	(const noderef &b) const	{ return new binary_node(mod, p, b); }
	noderef operator&	(const noderef &b) const	{ return new binary_node(bit_and, p, b); }
	noderef operator|	(const noderef &b) const	{ return new binary_node(bit_or, p, b); }
	noderef operator^	(const noderef &b) const	{ return new binary_node(bit_xor, p, b); }
	noderef operator&&	(const noderef &b) const	{ return new binary_node(log_and, p, b); }
	noderef operator||	(const noderef &b) const	{ return new binary_node(log_or, p, b); }
	noderef operator<<	(const noderef &b) const	{ return new binary_node(shl, p, b); }
	noderef operator>>	(const noderef &b) const	{ return new binary_node(shr, p, b); }
	noderef operator==	(const noderef &b) const	{ return new binary_node(eq, p, b); }
	noderef operator!=	(const noderef &b) const	{ return new binary_node(ne, p, b); }
	noderef operator<	(const noderef &b) const	{ return new binary_node(lt, p, b); }
	noderef operator>	(const noderef &b) const	{ return new binary_node(gt, p, b); }
	noderef operator<=	(const noderef &b) const	{ return new binary_node(le, p, b); }
	noderef operator>=	(const noderef &b) const	{ return new binary_node(ge, p, b); }

	noderef operator[]	(const noderef &b)	const	{ return new binary_node(ldelem, *this, b); }
	noderef operator[]	(int i)				const	{ return new binary_node(ldelem, *this, make(i)); }
	noderef operator[]	(const char *id)	const	{ return new binary_node(field, *this, new name_node(id, NOLOOKUP)); }
	noderef operator[]	(const C_element *e)const	{ return new binary_node(field, *this, new element_node(element, e)); }

	noderef substitute(const char *id, node *b)	const { return new substitute_node(id, b, *this); }

	friend noderef _not(const noderef &a)			{ return new unary_node(log_not, a.p); }

	template<typename...P> noderef operator()(P&&...p)	const {
		auto	f = new call_node(*this, INLINE);
		f->add(forward<P>(p)...);
		return f;
	}
};

//-----------------------------------------------------------------------------
//	basicblock
//-----------------------------------------------------------------------------

struct basicblock : inherit_refs<basicblock, node>, graph_edges<basicblock> {
	int								id;
	dynamic_array<ref_ptr<node> >	stmts;
	static bool check(KIND k) { return k == basic_block; }

	basicblock(int id) : inherit_refs<basicblock, node>(basic_block), id(id) {}

	void	add(node *p) {
		stmts.push_back(p);
	}

	ref_ptr<node> *last_assignment(const C_arg *v) {
		for (auto &i : reversed(stmts)) {
			if (i->kind == assign) {
				node *n = ((binary_node*)get(i))->left;
				if (n->kind == var && ((element_node*)n)->element == v)
					return &i;
			}
		}
		return 0;
	}
	ref_ptr<node> remove_last_assignment(const C_arg *v) {
		ref_ptr<node> *p = last_assignment(v);
		if (p) {
			ref_ptr<node>	n = *p;
			stmts.erase(p);
			return n;
		}
		return nullptr;
	}

	basicblock	*jump() const;

	template<typename T> void apply(T t);
	template<typename T> void apply_stmts(T t);
};

struct switch_node : node {
	ref_ptr<node>						arg;
	dynamic_array<ref_ptr<basicblock> >	targets;
	static bool check(KIND k) { return k == swtch; }

	switch_node(node *arg) : node(swtch), arg(arg) {}
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& arg		== ((switch_node&)b).arg
			&& targets	== ((switch_node&)b).targets;
	}
};

struct branch_node : node {
	ref_ptr<basicblock>	dest0, dest1;
	ref_ptr<node>		cond;
	static bool check(KIND k) { return k == branch; }

	branch_node(basicblock *dest) : node(branch), dest0(dest) {}
	branch_node(basicblock *dest0, basicblock *dest1, node *cond) : node(branch), dest0(dest0), dest1(dest1), cond(cond) {}
	void	set(basicblock *_dest0, basicblock *_dest1, node *_cond)	{ dest0 = _dest0; dest1 = _dest1; cond = _cond; }
	void	set(basicblock *dest)										{ dest0 = dest; dest1 = 0; cond = 0; }
	bool	has(basicblock *dest) const									{ return dest0 == dest || dest1 == dest; }
	bool operator==(const node &b) const {
		return kind		== b.kind
			&& dest0	== ((branch_node&)b).dest0
			&& dest1	== ((branch_node&)b).dest1
			&& cond		== ((branch_node&)b).cond;
	}
};

struct ifelse_node : node {
	ref_ptr<node>		cond;
	ref_ptr<basicblock>	blockt, blockf;
	static bool check(KIND k) { return k == ifelse; }

	ifelse_node(node *cond, basicblock *blockt, basicblock *blockf) : node(ifelse), cond(cond), blockt(blockt), blockf(blockf) {}
	bool	has(basicblock *dest) const									{ return blockt == dest || blockf == dest; }
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& cond		== ((ifelse_node&)b).cond
			&& blockt	== ((ifelse_node&)b).blockt
			&& blockf	== ((ifelse_node&)b).blockf;
	}
};

struct while_node : node {
	ref_ptr<node>		cond;
	ref_ptr<basicblock>	block;
	static bool check(KIND k) { return k == whileloop; }

	while_node(node *cond, basicblock *block) : node(whileloop), cond(cond), block(block) {}
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& cond		== ((while_node&)b).cond
			&& block	== ((while_node&)b).block;
	}
};

struct for_node : node {
	ref_ptr<node>		init, cond, update;
	ref_ptr<basicblock>	block;
	static bool check(KIND k) { return k == forloop; }

	for_node(node *_init, node *_cond, node *_update, basicblock *_block) : node(forloop), init(_init), cond(_cond), update(_update), block(_block) {}
	bool operator==(const node &b) const {
		return node::operator==(b)
			&& init		== ((for_node&)b).init
			&& cond		== ((for_node&)b).cond
			&& update	== ((for_node&)b).update
			&& block	== ((for_node&)b).block;
	}
};

//-----------------------------------------------------------------------------
//	apply functions
//-----------------------------------------------------------------------------

template<typename T> void apply(T &&t, ref_ptr<node> &r) {
	if (node *p = r) {
		if (p->is_unary()) {
			auto	*b	= (unary_node*)p;
			apply(t, b->arg);

		} else if (p->is_binary()) {
			auto	*b	= (binary_node*)p;
			apply(t, b->left);
			apply(t, b->right);

		} else switch (p->kind) {
			case call: {
				auto	*b = (call_node*)p;
				apply(t, b->func);
				for (auto &i : b->args)
					apply(t, i);
				break;
			}
			case branch: {
				auto	*b = (branch_node*)p;
				apply(t, b->cond);
				break;
			}
			case swtch: {
				auto	*b = (switch_node*)p;
				apply(t, b->arg);
				break;
			}
			case whileloop: {
				auto	*b = (while_node*)p;
				apply(t, b->cond);
				break;
			}
			case forloop: {
				auto	*b = (for_node*)p;
				apply(t, b->init);
				apply(t, b->cond);
				apply(t, b->update);
				break;
			}
			case ifelse: {
				auto	*b = (ifelse_node*)p;
				apply(t, b->cond);
				break;
			}
			default:
				break;
		}

		t(r);
	}
}

node *apply_preserve(const callback_ref<node*(node*)> &t, node *p);

inline noderef substitute_params(node *r) {
	return apply_preserve([](node *r)->node* {
		return r;
	}, r);
}

template<typename T> void apply_stmts(T &&t, ref_ptr<node> &r) {
	if (node *p = r) {
		switch (p->kind) {
			case swtch: {
				if (p->flags & FIXED) {
					auto	*b = (switch_node*)p;
					for (auto &i : b->targets)
						apply_stmts(t, i);
				}
				break;
			}
			case whileloop: {
				auto	*b = (while_node*)p;
				apply_stmts(t, b->block);
				break;
			}
			case forloop: {
				auto	*b = (for_node*)p;
				apply_stmts(t, b->block);
				break;
			}
			case ifelse: {
				auto	*b = (ifelse_node*)p;
				apply_stmts(t, b->blockf);
				apply_stmts(t, b->blockt);
				break;
			}
			default:
				break;
		}

		t(r);
	}
}

noderef	const_fold(node *r, get_variable_t get_var, get_memory_t get_mem);

inline noderef	const_fold_this(node *r, get_variable_t get_var, get_memory_t get_mem) {
	return get_var
		? const_fold(r,
			[get_var, get_mem](const char *id) -> node* {
				if (auto *node = get_var(id))
					return node;

				if (noderef node = get_var("this")) {
					auto	type = node->type;
					if (type->type == C_type::POINTER)
						type = type->subtype();

					if (auto c = type->composite()) {
						if (auto e = c->get(id))
							return node[e];
					}
				}
				return nullptr;
			},
			get_mem
		)
		: const_fold(r, iso::none, get_mem);
}

template<typename T> void basicblock::apply(T t) {
	for (auto &i : stmts)
		ast::apply(t, i);
}

template<typename T> void basicblock::apply_stmts(T t) {
	for (auto &i : stmts)
		ast::apply_stmts(t, i);
}

//-----------------------------------------------------------------------------
//	graphs
//-----------------------------------------------------------------------------

struct Substitutor {
	const C_arg *v;
	node		*s;
	Substitutor(const C_arg *_v, node *_s) : v(_v), s(_s) {}

	void operator()(ref_ptr<node> &r) {
		if (r->kind == var) {
			auto	*b = (element_node*)get(r);
			if (b->element == v)
				r = s;
		}
	}
};

inline bool assign_to_same(const node *a, const node *b) {
	return a->kind == assign && b->kind == assign && ((binary_node*)a)->left == ((binary_node*)b)->left;
}

class DominatorTree : public iso::DominatorTree<basicblock>	{
	typedef iso::DominatorTree<basicblock>	B;
public:
	DominatorTree(basicblock *b) : B(b) {}

	static basicblock	*make_break_block();
	static basicblock	*make_continue_block();
	static ifelse_node	*make_if(node *cond, basicblock *blockt);

	static void remove_block(const basicblock *b);
	void		break_branch(basicblock *from, const basicblock *to) const;

	void		convert_loop(basicblock *header, hash_set_with_key<const basicblock*> &back, node *init = 0, node *update = 0);
	void		convert_ifelse(basicblock *header);
	void		convert_switch(basicblock *header);
	void		convert_return(basicblock *header);
};

struct Loop {
	basicblock		*header;
	basicblock		*preheader;
	hash_set_with_key<const basicblock*> back_edges;

	Loop(const DominatorTree &dt, basicblock *header) : header(header), back_edges(dt.back_edges2(header)) {
		preheader	= unconst(dt.idom(header));
	}

	operator bool() const {
		return !back_edges.empty();
	}
	bool		single_latch() const {
		return back_edges.size() == 1;
	}

	basicblock	*latch() const {
		return unconst(*back_edges.begin());
	}

	bool		is_inside(const DominatorTree &dt, const basicblock *b) const {
		return dt.dominates(header, b) && dt.dominates(b, latch());
	}

	const element_node*	get_induction_var() const {
		auto		&last	= header->stmts.back();
//		ISO_ASSERT(last->kind == ast::branch);
		if (last->kind != ast::branch)
			return 0;

		if (node *cond = last->cast<branch_node>()->cond) {

			if (binary_node	*b = cond->cast()) {
				if (b->left->kind == ast::var)
					return (ast::element_node*)get(b->left);
			}

			if (call_node	*b = cond->cast()) {
				if (b->args.size()) {
					if (unary_node *u = b->args[0]->cast()) {
						if (u->kind == ref && u->arg->kind == var)
							return u->arg->cast();
					}
				}
			}
		}
		return 0;
	}

	ref_ptr<node>	get_init(const C_arg *v) const {
		return preheader->remove_last_assignment(v);
	}
	ref_ptr<node>	get_update(const C_arg *v) const {
		return latch()->remove_last_assignment(v);
	}
};

} // namespace :ast

// namespace iso
#if 0
template<class N> struct LoopBase {
	typedef N	node_t;

	LoopBase					*parent;
	dynamic_array<LoopBase*>	sub_loops;	// Loops contained entirely within this one.
	dynamic_array<const N*>		blocks;		// The list of blocks in this loop. First entry is the header node.
	small_set<const N*, 8>		block_set;

	explicit LoopBase(const N *n) : parent(0) {
		blocks.push_back(n);
		block_set.insert(n);
	}
	LoopBase() : parent(0) {}
	~LoopBase() {
		for (auto i : sub_loops)
			delete i;
	}

	const N	*header() const {
		return blocks.front();
	}
	uint32	depth() const {
		uint32 d = 1;
		for (const Loop *i = parent; i; i = i->parent)
			++d;
		return d;
	}
	bool	contains(const N *n) const {
		return block_set.count(n);
	}
	bool	contains(const LoopBase *loop) const {
		return loop == this || (loop && contains(loop->parent));
	}
	void	reserve(uint32 num_blocks, uint32 num_subloops) {
		sub_loops.reserve(num_subloops);
		blocks.reserve(num_blocks);
	}
	void	add_block(N *n) {
		blocks.push_back(n);
		block_set.insert(n);
	}
	void	fixup() {
		reverse(blocks.begin() + 1, blocks.end());
		reverse(sub_loops.begin(), sub_loops.end());
	}

	//---------- tests ---------

	// can the block branch out of this loop
	bool exits_loop(const N *n) const {
		for (auto &i : outgoing(n)) {
			if (!contains(i))
				return true;
		}
		return false;
	}

	// if the given loop's header has exactly one unique predecessor outside the loop, return it
	N *predecessor() const {
		N *out	= 0;
		for (auto i : header()->preds) {
			if (!contains(i)) {	// if the block is not in the loop...
				if (out && out != i)
					return 0;	// multiple predecessors outside the loop
				out = i;
			}
		}
		return out;
	}
	N *preheader() const {
		N *out = predecessor();
		return out && outgoing(out).size() == 1 ? out : 0;
	}

	// if there is a single latch block for this loop, return it (A latch block is a block that contains a branch back to the header)
	N *latch() const {
		N *out = 0;
		for (auto i : header()->preds) {
			if (contains(i)) {
				if (out)
					return 0;
				out = i;
			}
		}
		return out;
	}

	/// Return all latch blocks of this loop
	auto latches() const {
		return filter(header()->preds, [this](N *n) { return contains(n); });
	}
};

template<class N, class L> struct LoopsBase : public dynamic_array<L*> {
	typedef typename DominatorTree<N>::Info	I;
	hash_map<I*, L*>	loop_map;

	LoopsBase(const DominatorTree<N> &dt);
};

template<class N, class L> LoopsBase<N, L>::LoopsBase(const DominatorTree<N> &dt) {
	// Postorder traversal of the dominator tree.
	for (auto header : make_postorder(dt.info_root)) {

		// Check each predecessor of the potential loop header.
		dynamic_array<I*>	work_list;
		for (auto i : header->preds) {
			// If header dominates i, this is a new loop. Collect the backedges.
			if (dt.dominates(header, i))
				work_list.push_back(i);
		}

		// Perform a backward CFG traversal to discover and map blocks in this loop.
		if (!work_list.empty()) {
			L		*loop			= new L(header);
			uint32	num_blocks		= 0;
			uint32	num_subloops	= 0;

			while (!work_list.empty()) {
				I	*pred	= work_list.pop_back_retref();

				if (L *sub_loop = loop_map.lookup(pred)) {
					// This is a discovered block. Find its outermost discovered loop.
					while (L *parent = sub_loop->parent())
						sub_loop = parent;

					if (sub_loop != loop) {
						// Discover a subloop of this loop.
						sub_loop->parent(loop);
						num_subloops++;
						num_blocks += int(sub_loop->blocks.capacity());

						// Continue traversal along predecessors that are not loop-back edges from within this subloop tree itself. Note that a predecessor may directly reach another subloop that is not yet discovered to be a subloop of this loop, which we must traverse.
						for (auto i : sub_loop->header()->preds) {
							if (loop_map.lookup(i) != sub_loop)
								work_list.push_back(i);
						}
					}

				} else {
					// This is an undiscovered block. Map it to the current loop.
					loop_map[pred] = loop;
					++num_blocks;
					if (pred != loop->header())
						work_list.append(pred->preds);
				}
			}
			loop->reserve(num_subloops, num_blocks);
		}
	}
	// Perform a single forward CFG traversal to populate block and subloop vectors for all loops
	for (const N *n : make_postorder(dt.info_root->node)) {
		I *i		= dt.info_map.lookup(n);
		L *sub_loop = loop_map.lookup(i);
		if (sub_loop && i == sub_loop->header()) {
			// We reach this point once per subloop after processing all the blocks in the subloop.
			if (sub_loop->parent())
				sub_loop->parent()->sub_loops.push_back(sub_loop);
			else
				push_back(sub_loop);

			sub_loop->fixup();
			sub_loop = sub_loop->parent();
		}
		while (sub_loop) {
			sub_loop->add_block(i);
			sub_loop = sub_loop->parent();
		}
	}
}


class Loop : public LoopBase<DominatorTree<ast::basicblock>::Info> {
	typedef LoopBase<DominatorTree<ast::basicblock>::Info>	B;
public:
	explicit Loop(const node_t *n) : B(n) {}

	Loop	*parent()			{ return (Loop*)B::parent; }
	void	parent(Loop *p)		{ B::parent = p; }

	ast::node *getCanonicalInductionVariable() const {
		const node_t *h		= header();
		if (h->preds.size() != 2)
			return 0;

		const node_t *incoming = h->preds[0], *backedge = h->preds[1];

		if (contains(incoming)) {
			if (contains(backedge))
				return 0;
			swap(incoming, backedge);
		} else if (!contains(backedge)) {
			return 0;
		}

		// Loop over all of the PHI nodes, looking for a canonical indvar.
		for (auto i : h->node->stmts) {
			if (i->kind == ast::assign) {
				ast::binary_node	*a = (ast::binary_node*)get(i);
				if (a->left->kind == ast::var && a->right->kind == ast::literal) {
					return i;
				}
			}
		#if 0
			if (ConstantInt *CI = dyn_cast<ConstantInt>(i->getIncomingValueForBlock(incoming)))
				if (CI->isNullValue())
					if (Instruction *Inc = dyn_cast<Instruction>(i->getIncomingValueForBlock(backedge)))
						if (Inc->getOpcode() == Instruction::Add && Inc->getOperand(0) == i)
							if (ConstantInt *CI = dyn_cast<ConstantInt>(Inc->getOperand(1)))
								if (CI->equalsInt(1))
								return i;
		#endif
		}
		return 0;
	}
};

class Loops : public LoopsBase<ast::basicblock, Loop> {
public:
	Loops(const DominatorTree<ast::basicblock> &dt) : LoopsBase<ast::basicblock, Loop>(dt) {}
};
#endif


struct Dumper : indenter {
	const C_type_composite			*scope;
	ast::get_memory_t				get_mem;
	ast::demangle_t					demangle;
	map<ast::basicblock*, string>	named;
	int								precedence;
	bool							use_scope;
	uint32							flags;

	struct scoper {
		indenter		&i;
		string_accum	&a;
		scoper(indenter &i, string_accum &a) : i(i), a(i.open(a))	{}
		~scoper()		{ i.close(a); }
	};

	Dumper(const C_type *scope, int depth = 0, uint32 flags = 0) : indenter(depth, flags)
		, scope(GetScope(scope))
		, use_scope(true)
	{}

	void	SetScope(const C_type* t) {
		scope = GetScope(t);
	}

	scoper	scoped(string_accum	&a)	{ return scoper(*this, a); }

	static bool	is_this(ast::node *node) {
		return node->kind == ast::var && ((ast::element_node*)node)->element->id == "this";
	}

	bool	DumpCPP(string_accum &a, const ast::node *p, int precedence = 0);
	bool	DumpCS(string_accum &a, const ast::node *p, int precedence = 0);

//	string_accum&	DumpType(string_accum &a, const C_type *type, const char *name = 0, bool _typedef = false);
//	string_accum&	DumpParams(string_accum &a, const range<ref_ptr<ast::node>*> &args) {
//		return a << parentheses() << comma_list(args, [&](string_accum &a, ast::node *i) { DumpCPP(a, i); });
//	}
};

//from c-header.cpp
ast::node*	ReadCExpression(text_reader<reader_intf> &reader, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var);
ast::node*	ReadCExpression(istream_ref file, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var);

//from csharp.cpp
ast::node*	ReadCSExpression(text_reader<reader_intf> &reader, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var);
ast::node*	ReadCSExpression(istream_ref file, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var);

} // namespace iso

#endif // AST_H
