#include "base/tree.h"
#include "base/algorithm.h"
#include "java.h"
#include "extra/ast.h"

using namespace iso;

template<typename T> struct C_types::type_getter<T_swap_endian<T> > : type_getter<T> {};

namespace java {

optional<bool> is_bool_lit(const ast::node *n) {
	if (const ast::lit_node *lit = n->cast()) {
		if (lit->type == C_types::get_static_type<bool>())
			return lit->v;
	}
	return optional<bool>();
}

//-----------------------------------------------------------------------------
//	Context
//-----------------------------------------------------------------------------

struct Context : ClassFile {
	C_types				ctypes;
	C_type_namespace	*ns_java, *ns_java_lang;
	const C_type		*type_object, *type_string;
	C_element			array_len;

	hash_map<uint64, const C_type*>	signature_to_type;

	Context(ClassHeader *header) : ClassFile(header) {
		ns_java				= new C_type_namespace("java");
		ctypes.add("java", ns_java);
		ns_java_lang		= new C_type_namespace("lang");
		ns_java->add_child("lang", ns_java_lang);
		type_object			= ns_java_lang->add_child("object", new C_type_struct("object"));
		type_string			= ns_java_lang->add_child("string", new C_type_struct("string"));

		array_len.init("Length", ctypes.get_type(&dynamic_array<int>::size), 0);
	}

	template<typename T> const C_type	*GetType() { return ctypes.get_type<T>(); }
};

//-----------------------------------------------------------------------------
//	Stack
//-----------------------------------------------------------------------------

struct Stack {
	enum STATE {
		STACK_UNSET,
		STACK_SET,
		STACK_VARS,
	};
	enum TYPE {
		NONE,
		INT32,
		FLOAT32,
		OBJECT,
		INT64,
		FLOAT64,
	};
	struct ENTRY {
		ast::node	*p;
		TYPE		t;
		ENTRY(ast::node *_p, TYPE _t) : p(_p), t(_t)	{}
		operator ast::node*() const		{ return p; }
		ast::node*	operator->() const	{ return p; }
		bool	is_x2()			const	{ return t >= INT64; }
	};
	dynamic_array<ENTRY>	sp;
	STATE					state;

	Stack() : state(STACK_UNSET) {}

	void			push(ast::node *p, TYPE t)	{ new(sp) ENTRY(p, t); }
	void			push(const ENTRY &e)		{ sp.push_back(e); }
	const ENTRY&	pop()						{ ISO_ASSERT(sp.size()); return sp.pop_back_retref(); }
	const ENTRY&	top() const					{ return sp.back(); }
	ENTRY&			top()						{ return sp.back(); }
	void			end()						{ sp.clear(); }
	bool			end(int i)					{ return sp.size() == i; }

	bool	compatible(const Stack &s) const {
		return sp.size() == s.sp.size();
	}
};

//-----------------------------------------------------------------------------
//	Builder
//-----------------------------------------------------------------------------

template<typename T> T	get_val(const void *p)		{ return *(packed<bigendian<T>>*)p; }
uint16					get_uint16(const void *p)	{ return get_val<uint16>(p); }

struct Builder : Stack {
	struct block : ref_ptr<ast::basicblock>	{
		uint32			offset;
		Stack			stack;
		block(ast::basicblock *_b, uint32 _offset) : ref_ptr<ast::basicblock>(_b), offset(_offset) {}
	};

	struct local_var : C_arg {
		struct use {
			ast::basicblock	*b;
			ast::node		*n;
		};
		dynamic_array<use> uses;
		local_var(const char *_id, const C_type *_type) : C_arg(_id, _type) {}
		void	add_use(ast::basicblock *b, ast::node *n) {
			use	&u = uses.push_back();
			u.b = b;
			u.n = n;
		}
	};

	Context						&ctx;
	const_memory_block			code;
	int							gen_var;
	uint32						offset;
	dynamic_array<block>		blocks;
	block						*pbb;
	ast::basicblock				*bb;
	dynamic_array<local_var*>	locals[5];

	Builder(Context &_ctx, const const_memory_block &_code) : ctx(_ctx), code(_code), gen_var(0) {
	}

	void create_blocks(dynamic_array<uint32> &dests) {
		sort(dests);
		int		id	= 0;
		block	*x	= new(blocks) block(new ast::basicblock(id++), 0);
		for (auto i = dests.begin(), e = dests.end(); i != e; ++i) {
			if (*i != x->offset)
				x = new(blocks) block(new ast::basicblock(id++), *i);
		}

		pbb		= blocks.begin();
		bb		= *pbb;
	}

	block *at_offset(uint32 target) {
		return lower_boundc(blocks, target, [](const block &a, uint32 b) {
			return a.offset < b;
		});
	}

	ref_ptr<ast::basicblock>&	entry_point() const {
		return blocks[0];
	}

	static TYPE	type_to_stack(const C_type *type) {
		if (type) {
			switch (type->type) {
				case C_type::INT:		return ((C_type_int*)type)->num_bits() > 32 ? INT64 : INT32;
				case C_type::FLOAT:		return ((C_type_float*)type)->num_bits() > 32 ? FLOAT64 : FLOAT32;
				default:				return OBJECT;
			}
		}
		return NONE;
	}
	const C_type *stack_to_type(TYPE t) {
		const C_type *types[] = {
			0,								//NONE,
			C_type_int::get<int32>(),		//INT32,
			C_type_int::get<int64>(),		//INT64,
			C_type_float::get<float>(),		//FLOAT32,
			C_type_float::get<double>(),	//FLOAT64,
			ctx.type_object,				//OBJECT,
		};
		return types[t];
	}

	template<typename T, int I> static ast::node* lit0() {
		return ast::lit_node::get<T,I>();
	}
	template<typename T> ast::node* lit0(const T &t) {
		return new ast::lit_node(ctx.GetType<T>(), t);
	}

	template<typename T> void lit(const T &t, TYPE stack_type) {
		push(lit0(t), stack_type);
	}
	void lit(const Constant *c) {
		switch (c->tag) {
			case Constant::Integer:			lit((int)c->info<Constant::Integer>().v, INT32); break;
			case Constant::Float:			lit((float)c->info<Constant::Float>().v,	FLOAT32); break;
			case Constant::String:			lit((uint16)c->info<Constant::String>().string.idx, INT32); break;
			case Constant::Class:			lit((uint16)c->info<Constant::Class>().name.idx, INT32); break;
			//case Constant::MethodHandle:	lit((int)c->info<Constant::MethodHandle>().v, INT32); break;
			case Constant::MethodType:		lit((uint16)c->info<Constant::MethodType>().descriptor.idx, INT32); break;
		}
	}
	void lit2(const Constant *c) {
		switch (c->tag) {
			case Constant::Long:			lit((int64)c->info<Constant::Long>().v, INT64); break;
			case Constant::Double:			lit((float64)c->info<Constant::Double>().v, FLOAT64); break;
		}
	}
	uint32	get_offset(const byte_reader &r) const {
		return r.p - code;
	}

	const C_element*	get_element(const C_type *parent, int i) {
		const FieldorMethod	*field	= ctx._fields[i];
		auto				name	= ctx.get_constant<Constant::Utf8>(field->name.idx);
		return parent->composite()->get(string(name->get()));
	}
	ast::element_node*	element0(ast::KIND k, const C_type *parent, int i) {
		return new ast::element_node(k, get_element(parent, i), parent);
	}
	ast::node*	ldelem0(const C_type *type) {
		const ENTRY &right	= pop();
		const ENTRY &left	= pop();
		return new ast::binary_node(ast::ldelem, left.p, right.p, type);
	}
	ast::node*	var0(const C_arg *v, uint32 _flags = 0) {
		return new ast::element_node(ast::var, (const C_element*)v, 0, _flags);
	}
	ast::node*	unary0(ast::KIND k, uint32 flags = 0) {
		return new ast::unary_node(k, pop().p, flags);
	}
	ast::node*	binary0(ast::KIND k, uint32 flags = 0) {
		const ENTRY &right	= pop();
		const ENTRY &left	= pop();
		return new ast::binary_node(k, left.p, right.p, flags);
	}
	void	unary(ast::KIND k, TYPE t, uint32 flags = 0) {
		push(unary0(k, flags), t);
	}
	void	binary(ast::KIND k, TYPE t, uint32 flags = 0) {
		push(binary0(k, flags), t);
	}
	void	cast(const C_type *type, TYPE t) {
		push(new ast::unary_node(ast::cast, pop(), type), t);
	}

	void	get_args(ast::call_node *call, C_arg *args, size_t nargs) {
		call->args.resize(nargs);
		for (size_t i = nargs; i--;)
			call->args[i] = pop();
	}

	void add_stmt(ast::node *p) {
		bb->stmts.push_back(p);
	}
	void assign(const C_arg *v) {
		add_stmt(new ast::binary_node(ast::assign, var0(v, ast::ASSIGN), pop()));
	}
	void assign(local_var *v) {
		add_stmt(new ast::binary_node(ast::assign, var0(v, ast::ASSIGN), pop()));
	}

	local_var	*get_local(int n, TYPE t) {
		if (locals[t].size() <= n)
			locals[t].resize(n + 1);

		auto &loc = locals[t][n];
		if (!loc)
			loc = new local_var(format_string("%ctemp%i", "_ifld"[t], n), stack_to_type(t));
		return loc;
	}

	void	push_local(int n, TYPE t) {
		push(var0(get_local(n, t)), t);
	}
	void	assign_local(int n, TYPE t) {
		assign(get_local(n, t));
	}

	ast::basicblock *branch_stack(block *e) {
		if (e->stack.state) {
			if (!e->stack.compatible(*this))
				ISO_TRACEF("Incompatible stack!\n");

			if (!sp.empty() && state != STACK_VARS) {
				//copy stack vars from e
				auto	i2	= e->stack.sp.begin();
				for (auto &i : sp) {
					//linked_var_node	*lvn	= (linked_var_node*)(i2++->p);
					ast::element_node	*lvn	= i2++->p->cast();
					if (*i != *lvn) {
						local_var		**lv	= find_check(locals[0], (local_var*)lvn->element);
						auto			*lvn2	= var0(*lv);
						add_stmt(new ast::binary_node(ast::assign, lvn2, i));
						i.p = lvn2;
					}
				}
				state = STACK_VARS;
			}

		} else {
			if (!sp.empty() && state != STACK_VARS) {
				//create stack vars
				for (auto &i : sp) {
					local_var	*arg	= new local_var(format_string("stack%i", gen_var++), stack_to_type(i.t));
					locals[0].push_back(arg);
					ast::node	*var	= var0(arg, ast::ASSIGN);
					add_stmt(new ast::binary_node(ast::assign, var, i));
					i.p = var;
				}
				state = STACK_VARS;
			}
			e->stack	= *this;
		}
		return *e;
	}

	template<typename T> uint32 get_target(const void *p) {
		return (const uint8*)p + get_val<T>((const uint8*)p + 1) - code;
	}
	void branch(uint32 target) {
		add_stmt(new ast::branch_node(
			branch_stack(at_offset(target))
		));
	}
	void branch(const local_var *loc) {
//		add_stmt(new ast::branch_node(
//			branch_stack(at_offset(target))
//		));
	}
	void condbranch(uint32 target, ast::node *cond) {
		add_stmt(new ast::branch_node(
			branch_stack(pbb + 1),
			branch_stack(at_offset(target)),
			cond
		));
	}
};

//-----------------------------------------------------------------------------
//	MakeAST
//-----------------------------------------------------------------------------

ref_ptr<ast::basicblock> MakeAST(Context &ctx, const C_arg &this_arg, ast::funcdecl_node *func, const Code *code) {
	Builder		builder(ctx, const_memory_block(code->code.begin(), code->code.size()));

	// find branch destinations
	dynamic_array<uint32>	dests;
	for (const uint8 *p = builder.code; p < builder.code.end(); ) {
		uint32		offset	= builder.get_offset(p);
		OPCODE		op		= (OPCODE)p[0];
		PARAMS		params	= get_info(op).params;
		uint32		len		= get_len(p);

		switch (params) {
			default:
				p += len;
				break;

			case BRANCH2:
				dests.push_back(offset + *(packed<int16be>*)(p + 1));
				break;

			case BRANCH4:
				dests.push_back(offset + *(packed<int32be>*)(p + 1));
				break;

			case TABLESWITCH: {
				TableSwitch		*t = (TableSwitch*)align(p, 4);
				for (int i = 0, n = t->high - t->low; i <= n; ++i)
					dests.push_back(offset + t->offsets[i]);
				dests.push_back(offset + t->def);
				break;
			}
			case LOOKUPSWITCH: {
				LookupSwitch	*t = (LookupSwitch*)align(p, 4);
				for (int i = 0, n = t->count; i < n; ++i)
					dests.push_back(offset + t->pairs[i].offset);
				dests.push_back(offset + t->def);
				break;
			}
		}
	}

	// create basic blocks
	builder.create_blocks(dests);
//	builder.create_locals(code->max_locals);

	uint32	flags	= 0;
	uint32	len;
	for (const uint8 *p = builder.code; p < builder.code.end(); p += len) {
		len		= get_len(p);

		OPCODE		op		= (OPCODE)p[0];

		switch (op) {
			case op_nop:		break;
			case op_aconst_null:	builder.push(nullptr, Stack::OBJECT); break;
			case op_iconst_m1:		builder.push(builder.lit0<int32, -1>(), Stack::INT32); break;
			case op_iconst_0:		builder.push(builder.lit0<int32,  0>(), Stack::INT32); break;
			case op_iconst_1:		builder.push(builder.lit0<int32,  1>(), Stack::INT32); break;
			case op_iconst_2:		builder.push(builder.lit0<int32,  2>(), Stack::INT32); break;
			case op_iconst_3:		builder.push(builder.lit0<int32,  3>(), Stack::INT32); break;
			case op_iconst_4:		builder.push(builder.lit0<int32,  4>(), Stack::INT32); break;
			case op_iconst_5:		builder.push(builder.lit0<int32,  5>(), Stack::INT32); break;
			case op_lconst_0:		builder.push(builder.lit0<int64,  0>(), Stack::INT64); break;
			case op_lconst_1:		builder.push(builder.lit0<int64,  1>(), Stack::INT64); break;
			case op_fconst_0:		builder.push(builder.lit0<float32,0>(), Stack::FLOAT32); break;
			case op_fconst_1:		builder.push(builder.lit0<float32,1>(), Stack::FLOAT32); break;
			case op_fconst_2:		builder.push(builder.lit0<float32,2>(), Stack::FLOAT32); break;
			case op_dconst_0:		builder.push(builder.lit0<float64,0>(), Stack::FLOAT64); break;
			case op_dconst_1:		builder.push(builder.lit0<float64,1>(), Stack::FLOAT64); break;
			case op_bipush:			builder.lit(p[1], Stack::INT32); break;
			case op_sipush:			builder.lit((p[1] << 8) | p[2], Stack::INT32); break;
			case op_ldc:			builder.lit(ctx.get_constant(p[1])); break;
			case op_ldc_w:			builder.lit(ctx.get_constant((p[1] << 8) | p[2])); break;
			case op_ldc2_w:			builder.lit2(ctx.get_constant((p[1] << 8) | p[2])); break;

			case op_iload:			builder.push_local(p[1], Stack::INT32); break;
			case op_lload:			builder.push_local(p[1], Stack::INT64); break;
			case op_fload:			builder.push_local(p[1], Stack::FLOAT32); break;
			case op_dload:			builder.push_local(p[1], Stack::FLOAT64); break;
			case op_aload:			builder.push_local(p[1], Stack::OBJECT); break;
			case op_iload_0:		builder.push_local(0, Stack::INT32); break;
			case op_iload_1:		builder.push_local(1, Stack::INT32); break;
			case op_iload_2:		builder.push_local(2, Stack::INT32); break;
			case op_iload_3:		builder.push_local(3, Stack::INT32); break;
			case op_lload_0:		builder.push_local(0, Stack::INT64); break;
			case op_lload_1:		builder.push_local(1, Stack::INT64); break;
			case op_lload_2:		builder.push_local(2, Stack::INT64); break;
			case op_lload_3:		builder.push_local(3, Stack::INT64); break;
			case op_fload_0:		builder.push_local(0, Stack::FLOAT32); break;
			case op_fload_1:		builder.push_local(1, Stack::FLOAT32); break;
			case op_fload_2:		builder.push_local(2, Stack::FLOAT32); break;
			case op_fload_3:		builder.push_local(3, Stack::FLOAT32); break;
			case op_dload_0:		builder.push_local(0, Stack::FLOAT64); break;
			case op_dload_1:		builder.push_local(1, Stack::FLOAT64); break;
			case op_dload_2:		builder.push_local(2, Stack::FLOAT64); break;
			case op_dload_3:		builder.push_local(3, Stack::FLOAT64); break;
			case op_aload_0:		builder.push_local(0, Stack::OBJECT); break;
			case op_aload_1:		builder.push_local(1, Stack::OBJECT); break;
			case op_aload_2:		builder.push_local(2, Stack::OBJECT); break;
			case op_aload_3:		builder.push_local(3, Stack::OBJECT); break;
			case op_iaload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int32>(),	flags), Stack::INT32); flags = 0; break;
			case op_laload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int64>(),	flags), Stack::INT32); flags = 0; break;
			case op_faload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<float32>(),	flags), Stack::INT32); flags = 0; break;
			case op_daload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<float64>(),	flags), Stack::INT32); flags = 0; break;
			case op_aaload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.type_object,		flags), Stack::INT32); flags = 0; break;
			case op_baload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int8>(),	flags), Stack::INT32); flags = 0; break;
			case op_caload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<char>(),	flags), Stack::INT32); flags = 0; break;
			case op_saload:			builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int16>(),	flags), Stack::INT32); flags = 0; break;

			case op_istore:			builder.assign_local(p[1], Stack::INT32); break;
			case op_lstore:			builder.assign_local(p[1], Stack::INT64); break;
			case op_fstore:			builder.assign_local(p[1], Stack::FLOAT32); break;
			case op_dstore:			builder.assign_local(p[1], Stack::FLOAT64); break;
			case op_astore:			builder.assign_local(p[1], Stack::OBJECT); break;
			case op_istore_0:		builder.assign_local(0, Stack::INT32); break;
			case op_istore_1:		builder.assign_local(1, Stack::INT32); break;
			case op_istore_2:		builder.assign_local(2, Stack::INT32); break;
			case op_istore_3:		builder.assign_local(3, Stack::INT32); break;
			case op_lstore_0:		builder.assign_local(0, Stack::INT64); break;
			case op_lstore_1:		builder.assign_local(1, Stack::INT64); break;
			case op_lstore_2:		builder.assign_local(2, Stack::INT64); break;
			case op_lstore_3:		builder.assign_local(3, Stack::INT64); break;
			case op_fstore_0:		builder.assign_local(0, Stack::FLOAT32); break;
			case op_fstore_1:		builder.assign_local(1, Stack::FLOAT32); break;
			case op_fstore_2:		builder.assign_local(2, Stack::FLOAT32); break;
			case op_fstore_3:		builder.assign_local(3, Stack::FLOAT32); break;
			case op_dstore_0:		builder.assign_local(0, Stack::FLOAT64); break;
			case op_dstore_1:		builder.assign_local(1, Stack::FLOAT64); break;
			case op_dstore_2:		builder.assign_local(2, Stack::FLOAT64); break;
			case op_dstore_3:		builder.assign_local(3, Stack::FLOAT64); break;
			case op_astore_0:		builder.assign_local(0, Stack::OBJECT); break;
			case op_astore_1:		builder.assign_local(1, Stack::OBJECT); break;
			case op_astore_2:		builder.assign_local(2, Stack::OBJECT); break;
			case op_astore_3:		builder.assign_local(3, Stack::OBJECT); break;
			case op_iastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int32>(),		flags), p)); flags = 0; break; }
			case op_lastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int64>(),		flags), p)); flags = 0; break; }
			case op_fastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<float32>(),	flags), p)); flags = 0; break; }
			case op_dastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<float64>(),	flags), p)); flags = 0; break; }
			case op_aastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.type_object,			flags), p)); flags = 0; break; }
			case op_bastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int8>(),		flags), p)); flags = 0; break; }
			case op_castore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<char>(),		flags), p)); flags = 0; break; }
			case op_sastore:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int16>(),		flags), p)); flags = 0; break; }

			case op_pop:			builder.pop(); break;
			case op_pop2:			if (!builder.pop().is_x2()) builder.pop(); break;
			case op_dup:			builder.push(builder.top()); break;
			case op_dup_x1:			builder.sp.insert(builder.sp.end() - 2, builder.top()); break;
			case op_dup_x2:			builder.sp.insert(builder.sp.end() - (builder.sp.end()[-2].is_x2() ? 2 : 3), builder.top()); break;
			case op_dup2:
				if (builder.top().is_x2()) {
					builder.push(builder.top());
				} else {
					builder.push(builder.sp.end()[-2]);
					builder.push(builder.sp.end()[-2]);
				}
				break;
			case op_dup2_x1:
				if (builder.top().is_x2()) {
					builder.sp.insert(builder.sp.end() - 2, builder.top());
				} else {
					builder.sp.insert(builder.sp.end() - 3, builder.sp.end()[-2]);
					builder.sp.insert(builder.sp.end() - 3, builder.top());
				}
				break;
			case op_dup2_x2:
				if (builder.top().is_x2()) {
					if (builder.sp.end()[-2].is_x2()) {
						//form4
						builder.sp.insert(builder.sp.end() - 2, builder.top());
					} else {
						//form2
						builder.sp.insert(builder.sp.end() - 3, builder.top());
					}
				} else {
					if (builder.sp.end()[-3].is_x2()) {
						//form3
						builder.sp.insert(builder.sp.end() - 3, builder.sp.end()[-2]);
						builder.sp.insert(builder.sp.end() - 3, builder.top());
					} else {
						//form1
						builder.sp.insert(builder.sp.end() - 4, builder.sp.end()[-2]);
						builder.sp.insert(builder.sp.end() - 4, builder.top());
					}
				}
				break;

			case op_swap:
				swap(builder.top(), builder.sp.end()[-2]);
				break;

			case op_iadd:			builder.binary(ast::add, Stack::INT32); break;
			case op_ladd:			builder.binary(ast::add, Stack::INT64); break;
			case op_fadd:			builder.binary(ast::add, Stack::FLOAT32); break;
			case op_dadd:			builder.binary(ast::add, Stack::FLOAT64); break;
			case op_isub:			builder.binary(ast::sub, Stack::INT32); break;
			case op_lsub:			builder.binary(ast::sub, Stack::INT64); break;
			case op_fsub:			builder.binary(ast::sub, Stack::FLOAT32); break;
			case op_dsub:			builder.binary(ast::sub, Stack::FLOAT64); break;
			case op_imul:			builder.binary(ast::mul, Stack::INT32); break;
			case op_lmul:			builder.binary(ast::mul, Stack::INT64); break;
			case op_fmul:			builder.binary(ast::mul, Stack::FLOAT32); break;
			case op_dmul:			builder.binary(ast::mul, Stack::FLOAT64); break;
			case op_idiv:			builder.binary(ast::div, Stack::INT32); break;
			case op_ldiv:			builder.binary(ast::div, Stack::INT64); break;
			case op_fdiv:			builder.binary(ast::div, Stack::FLOAT32); break;
			case op_ddiv:			builder.binary(ast::div, Stack::FLOAT64); break;
			case op_irem:			builder.binary(ast::mod, Stack::INT32); break;
			case op_lrem:			builder.binary(ast::mod, Stack::INT64); break;
			case op_frem:			builder.binary(ast::mod, Stack::FLOAT32); break;
			case op_drem:			builder.binary(ast::mod, Stack::FLOAT64); break;
			case op_ineg:			builder.unary(ast::neg, Stack::INT32); break;
			case op_lneg:			builder.unary(ast::neg, Stack::INT64); break;
			case op_fneg:			builder.unary(ast::neg, Stack::FLOAT32); break;
			case op_dneg:			builder.unary(ast::neg, Stack::FLOAT64); break;
			case op_ishl:			builder.binary(ast::shl, Stack::INT32); break;
			case op_lshl:			builder.binary(ast::shl, Stack::INT64); break;
			case op_ishr:			builder.binary(ast::shr, Stack::INT32); break;
			case op_lshr:			builder.binary(ast::shr, Stack::INT64); break;
			case op_iushr:			builder.binary(ast::shr, Stack::INT32, ast::UNSIGNED); break;
			case op_lushr:			builder.binary(ast::shr, Stack::INT64, ast::UNSIGNED); break;
			case op_iand:			builder.binary(ast::bit_and, Stack::INT32); break;
			case op_land:			builder.binary(ast::bit_and, Stack::INT64); break;
			case op_ior:			builder.binary(ast::bit_or, Stack::INT32); break;
			case op_lor:			builder.binary(ast::bit_or, Stack::INT64); break;
			case op_ixor:			builder.binary(ast::bit_xor, Stack::INT32); break;
			case op_lxor:			builder.binary(ast::bit_xor, Stack::INT64); break;
			case op_iinc:			builder.add_stmt(new ast::binary_node(ast::add_assign, builder.var0(builder.get_local(p[1], Stack::INT32), ast::ASSIGN), builder.lit0(int(int8(p[2]))))); break;

			case op_i2l:			builder.cast(ctx.GetType<int64>(),		Stack::INT64); break;
			case op_i2f:			builder.cast(ctx.GetType<float32>(),	Stack::FLOAT32); break;
			case op_i2d:			builder.cast(ctx.GetType<float64>(),	Stack::FLOAT64); break;
			case op_l2i:			builder.cast(ctx.GetType<int32>(),		Stack::INT32); break;
			case op_l2f:			builder.cast(ctx.GetType<float32>(),	Stack::FLOAT32); break;
			case op_l2d:			builder.cast(ctx.GetType<float64>(),	Stack::FLOAT64); break;
			case op_f2i:			builder.cast(ctx.GetType<int32>(),		Stack::INT32); break;
			case op_f2l:			builder.cast(ctx.GetType<int64>(),		Stack::INT64); break;
			case op_f2d:			builder.cast(ctx.GetType<float64>(),	Stack::FLOAT64); break;
			case op_d2i:			builder.cast(ctx.GetType<int32>(),		Stack::INT32); break;
			case op_d2l:			builder.cast(ctx.GetType<int64>(),		Stack::INT64); break;
			case op_d2f:			builder.cast(ctx.GetType<float32>(),	Stack::FLOAT32); break;
			case op_i2b:			builder.cast(ctx.GetType<int8>(),		Stack::INT32); break;
			case op_i2c:			builder.cast(ctx.GetType<char>(),		Stack::INT32); break;
			case op_i2s:			builder.cast(ctx.GetType<int16>(),		Stack::INT32); break;

			case op_lcmp:			builder.binary(ast::spaceship, Stack::INT32); break;
			case op_fcmpl:			builder.binary(ast::spaceship, Stack::INT32); break;
			case op_fcmpg:			builder.binary(ast::spaceship, Stack::INT32); break;
			case op_dcmpl:			builder.binary(ast::spaceship, Stack::INT32); break;
			case op_dcmpg:			builder.binary(ast::spaceship, Stack::INT32); break;

			case op_ifeq:			builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::eq, builder.pop(), builder.lit0<int, 0>())); break;
			case op_ifne:			builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::ne, builder.pop(), builder.lit0<int, 0>())); break;
			case op_iflt:			builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::lt, builder.pop(), builder.lit0<int, 0>())); break;
			case op_ifge:			builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::ge, builder.pop(), builder.lit0<int, 0>())); break;
			case op_ifgt:			builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::gt, builder.pop(), builder.lit0<int, 0>())); break;
			case op_ifle:			builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::le, builder.pop(), builder.lit0<int, 0>())); break;
			case op_if_icmpeq:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::eq)); break;
			case op_if_icmpne:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::ne)); break;
			case op_if_icmplt:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::lt)); break;
			case op_if_icmpge:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::ge)); break;
			case op_if_icmpgt:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::gt)); break;
			case op_if_icmple:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::le)); break;
			case op_if_acmpeq:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::eq)); break;
			case op_if_acmpne:		builder.condbranch(builder.get_target<int16>(p), builder.binary0(ast::ne)); break;
			case op_goto:			builder.branch(builder.get_target<int16>(p)); break;
			case op_jsr:			builder.lit((p + len) - builder.code, Stack::INT32); builder.branch(builder.get_target<int16>(p)); break;
			case op_ret:			builder.branch(builder.get_local(p[0], Stack::INT32)); break;
			case op_tableswitch: {
				TableSwitch			*t		= (TableSwitch*)align(p, 4);
				ast::switch_node	*s		= new ast::switch_node(builder.pop());
				uint32				offset	= p - builder.code;
				for (int i = 0, n = t->high - t->low; i <= n; ++i)
					s->targets.push_back(builder.branch_stack(builder.at_offset(offset + t->offsets[i])));
				s->targets.push_back(builder.pbb[1]);//offset + t->def;
				builder.add_stmt(s);
				break;
			}
			case op_lookupswitch: {
				LookupSwitch	*t		= (LookupSwitch*)align(p, 4);
				uint32			offset	= p - builder.code;
				auto			val		= builder.pop();
				for (int i = 0, n = t->count; i < n; ++i)
					builder.condbranch(offset + t->pairs[i].offset, new ast::binary_node(ast::eq, val, builder.lit0(t->pairs[i].val)));
				break;
			}
			case op_ireturn:
			case op_lreturn:
			case op_freturn:
			case op_dreturn:
			case op_areturn:	builder.add_stmt(new ast::unary_node(ast::retv, builder.pop())); builder.sp.clear(); break;
			case op_return:		builder.add_stmt(new ast::node(ast::ret)); builder.sp.clear(); break;

			case op_getstatic: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				builder.push(builder.var0(e), builder.type_to_stack(e->type));
				break;
			}
			case op_putstatic: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				builder.add_stmt(new ast::binary_node(ast::assign, builder.var0(e), builder.pop()));
				break;
			}
			case op_getfield: {
				auto	*e	= new ast::element_node(ast::element, builder.get_element(this_arg.type, get_uint16(p + 1)), this_arg.type);
				builder.push(new ast::binary_node(ast::field, builder.pop(), e, flags), builder.type_to_stack(e->element->type));
				break;
			}
			case op_putfield: {
				auto		*e		= new ast::element_node(ast::element, builder.get_element(this_arg.type, get_uint16(p + 1)), this_arg.type);
				ast::node	*val	= builder.pop();
				builder.add_stmt(new ast::binary_node(ast::assign, new ast::binary_node(ast::field, builder.pop(), e, flags), val));
				break;
			}
			case op_invokevirtual: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, this_arg.type));

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype) {
					builder.push(call, builder.type_to_stack(func->subtype));
				} else {
					builder.add_stmt(call);
				}
				break;
			}
			case op_invokespecial: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, this_arg.type));

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype) {
					builder.push(call, builder.type_to_stack(func->subtype));
				} else {
					builder.add_stmt(call);
				}
				break;
			}
			case op_invokestatic: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, this_arg.type));

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype) {
					builder.push(call, builder.type_to_stack(func->subtype));
				} else {
					builder.add_stmt(call);
				}
				break;
			}
			case op_invokeinterface: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, this_arg.type));

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype) {
					builder.push(call, builder.type_to_stack(func->subtype));
				} else {
					builder.add_stmt(call);
				}
				break;
			}
			case op_invokedynamic: {
				auto	*e	= builder.get_element(this_arg.type, get_uint16(p + 1));
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, this_arg.type));

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype) {
					builder.push(call, builder.type_to_stack(func->subtype));
				} else {
					builder.add_stmt(call);
				}
				break;
			}
			case op_new: {
				auto	*c	= ctx.get_constant(get_uint16(p + 1));
				//builder.push(new ast::call_node(new ast::element_node(ast::element, e, parent), ast::ALLOC));
				break;
			}
			case op_newarray:
				break;
			case op_anewarray:
				break;
			case op_arraylength: {
				const C_element			*e		= &ctx.array_len;
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, 0));
				builder.get_args(call, func->args, func->num_args());
				builder.push(call, Stack::INT32);
				break;
			}
			case op_athrow:
				break;
			case op_checkcast:
				break;
			case op_instanceof:
				break;
			case op_monitorenter:
				break;
			case op_monitorexit:
				break;

			case op_wide: {
				uint16	i = (p[1] << 8) + p[2];
				switch (p[1]) {
					case op_iload:		builder.push_local(i, Stack::INT32); break;
					case op_lload:		builder.push_local(i, Stack::INT64); break;
					case op_fload:		builder.push_local(i, Stack::FLOAT32); break;
					case op_dload:		builder.push_local(i, Stack::FLOAT64); break;
					case op_aload:		builder.push_local(i, Stack::OBJECT); break;
					case op_istore:		builder.assign_local(i, Stack::INT32); break;
					case op_lstore:		builder.assign_local(i, Stack::INT64); break;
					case op_fstore:		builder.assign_local(i, Stack::FLOAT32); break;
					case op_dstore:		builder.assign_local(i, Stack::FLOAT64); break;
					case op_astore:		builder.assign_local(i, Stack::OBJECT); break;
					case op_ret:		builder.branch(builder.get_local(i, Stack::INT32)); break;
					case op_iinc:		builder.add_stmt(new ast::binary_node(ast::add_assign, builder.var0(builder.get_local(i, Stack::INT32), ast::ASSIGN), builder.lit0(int(int16((p[3] << 8) + p[4]))))); break;
				}
				break;
			}

			case op_multianewarray:
				break;

			case op_ifnull:		builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::eq, builder.pop(), builder.lit0<int, 0>())); break;
			case op_ifnonnull:	builder.condbranch(builder.get_target<int16>(p), new ast::binary_node(ast::ne, builder.pop(), builder.lit0<int, 0>())); break;

			case op_goto_w:		builder.branch(builder.get_target<int32>(p)); break;
			case op_jsr_w:		builder.lit((p + len) - builder.code, Stack::INT32); builder.branch(builder.get_target<int32>(p)); break;

			case op_breakpoint:
				break;
			case op_impdep1:
				break;
			case op_impdep2:
				break;

		}
	}

	// Flow Analysis

	//remove any redundant blocks (that just jump to other ones)

	ast::basicblock	**redirection = alloc_auto(ast::basicblock*, builder.blocks.size32());
	for (auto &i : builder.blocks) {
		ast::basicblock	*b		= i;
		int				id		= b->id;
		if (b->stmts.size() == 1 && b->stmts[0]->kind == ast::branch && !((ast::branch_node*)get(b->stmts[0]))->cond) {
			ast::basicblock	*b2	= b->stmts.back()->cast<ast::branch_node>()->dest0;
			b->stmts.clear();
			b	= b2;
		}
		redirection[id] = b;
	}

	for (auto &i : builder.blocks) {
		ast::basicblock	*b		= i;
		int				id		= b->id;
		while (b != redirection[b->id])
			b = redirection[b->id];
		redirection[id] = b;
	}

	// create edges

	for (auto &i : builder.blocks) {
		ast::basicblock	*b = i;
		if (b->stmts) {
			switch (b->stmts.back()->kind) {
				case ast::swtch: {
					ast::switch_node	*a = (ast::switch_node*)get(b->stmts.back());
					for (auto &j : a->targets)
						b->add_edge(j = redirection[j->id]);
					break;
				}
				case ast::branch: {
					ast::branch_node	*a = (ast::branch_node*)get(b->stmts.back());
					if (a->cond)
						b->add_edge(a->dest1 = redirection[a->dest1->id]);
					b->add_edge(a->dest0 = redirection[a->dest0->id]);
					break;
				}
			}
		}
	}

	// make dominator tree

	ast::DominatorTree	dt(builder.entry_point());

#if 0
	Loops	loops(dt);
	for (auto &i : loops) {
		auto				*h		= i->header();
		ast::basicblock		*n		= unconst(h->node);
		auto				&a		= n->stmts.back();
		if (a->kind == ast::branch && ((ast::branch_node*)get(a))->cond) {
			ISO_ASSERT(a->kind == ast::branch && ((ast::branch_node*)get(a))->cond);
			ast::branch_node	*b		= (ast::branch_node*)get(a);
			ast::while_node		*w;
			if (i->contains(dt[(ast::basicblock*)get(b->dest1)])) {
				w			= new ast::while_node(b->cond, b->dest1);
			} else {
				w			= new ast::while_node(b->cond->flip_condition(), b->dest0);
				b->dest0	= b->dest1;
			}
			n->stmts.insert(&a, w);
			b->dest1	= 0;
			b->cond		= 0;

			for (auto j : i->latches()) {
				ast::basicblock	*n		= unconst(j->node);
				auto			&a		= n->stmts.back();
				ISO_ASSERT(a->kind == ast::branch && !((ast::branch_node*)get(a))->cond);
				n->stmts.erase(&a);
			}
		}
	}
#endif

//	builder.count_local_use(dt.info_root);
//	builder.remove_redundant_locals();

#if 1
	for (auto &b : builder.blocks) {
		for (auto *i = b->stmts.begin(), *e = b->stmts.end(); i != e; ++i) {
			if ((*i)->kind == ast::assign) {
				ast::binary_node *a = (*i)->cast();
				if (a->left == a->right) {
					b->stmts.erase(i);
					--i;
				}
			}
		}
	}
#endif

	// find loops and if/else

	for (auto i : make_postorder(dt.info_root)) {
		ast::basicblock	*header	= unconst(i->node);
		ast::node		*last	= header->stmts.back();

		if (auto loop = ast::Loop(dt, header)) {
			ref_ptr<ast::node>	init, update;
			if (loop.single_latch()) {
				if (const ast::element_node *i = loop.get_induction_var()) {
					const C_arg	*var		= i->element;
					if (init = loop.get_init(var)) {
						if (Builder::local_var **lv = find_check(builder.locals[0], var)) {
							//linked_var_node	*lvn = (linked_var_node*)get(((ast::binary_node*)get(init))->left);
							//lvn->bb = header;
							bool	used_outside = false;
							for (auto &u : (*lv)->uses) {
								if (used_outside = !loop.is_inside(dt, u.b))
									break;
							}

							if (!used_outside) {
								((ast::binary_node*)get(init))->left = new ast::decl_node(var);
								builder.locals[0].erase(lv);
							}
						}
						update	= loop.get_update(var);
					}
				}
			}
			dt.convert_loop(header, loop.back_edges, init, update);

		} else if (last->kind == ast::branch && ((ast::branch_node*)last)->cond) {
			dt.convert_ifelse(header);

		} else if (last->kind == ast::swtch) {
			dt.convert_switch(header);

		} else if (last->kind == ast::ret) {
			dt.convert_return(header);
		}
	}


	// turn gotos into break/continue

	for (auto &i : builder.blocks) {
		if (i->incoming.size() > 1) {
			const ast::DominatorTree::Info	*common = 0;
			for (auto &j : i->outgoing) {
				auto	*ji = dt.get_info(j);
				common = common
					? dt.nearest_common_dominator(common, ji)
					: ji;
			}
		}
	}


	// peephole optimisations:

	// check for ++/--
	for (auto &i : builder.blocks) {
		for_each(i->stmts, [](ref_ptr<ast::node> &r) {
			if (r->kind == ast::assign) {
				ast::binary_node	*assign	= r->cast();
				ast::KIND			k		= assign->right->kind;
				if (k == ast::add || k == ast::sub) {
					ast::binary_node *exp	= assign->right->cast<ast::binary_node>();
					if (ast::lit_node *lit = exp->right->cast()) {
						if (lit->type->type == C_type::INT && abs((int64)lit->v) == 1) {
							if (exp->left == assign->left) {
								r = new ast::unary_node(((int64)lit->v < 0) ^ (k == ast::sub) ? ast::dec : ast::inc, assign->left);
							}
						}
					}
				}
			}
		});
	}

	// check for if/else -> ternary op
	for (auto &i : builder.blocks) {
		for_each(i->stmts, [&i](ref_ptr<ast::node> &r) {
			if (ast::ifelse_node *n = r->cast()) {
				ast::basicblock	*blockt = n->blockt;
				ast::basicblock	*blockf = n->blockf;
				if (blockt && blockt->stmts.size() == 1 && blockf && blockf->stmts.size() == 1 && assign_to_same(blockt->stmts[0], blockf->stmts[0])) {
					ast::binary_node	*bt = blockt->stmts[0]->cast();
					ast::binary_node	*bf = blockf->stmts[0]->cast();
					r	= new ast::binary_node(ast::assign, bt->left, new ast::binary_node(ast::query, n->cond, new ast::binary_node(ast::colon, bt->right, bf->right)));
					blockf->stmts.clear();
					blockt->stmts.clear();
					i->remove_edge(blockf);
					i->remove_edge(blockt);
				}
			}
		});
	}

//	dt.recreate(builder.entry_point());
//	builder.count_local_use(dt.info_root);
//	builder.remove_redundant_locals();

	for (auto &i : builder.blocks) {
		i->apply([](ref_ptr<ast::node> &r) {
			if (r->kind == ast::query) {

				//optimise ternary:
				ast::binary_node *q		= r->cast();
				ast::binary_node *v		= q->right->cast();
				optional<bool>	a		= is_bool_lit(v->left);
				optional<bool>	b		= is_bool_lit(v->right);
				if (!a.exists()) {
					if (b.exists()) {
						if (b) {
							// a ? b : true    ->  !a || b
							r = new ast::binary_node(ast::log_or, q->left->flip_condition(), v->right);
						} else {
							// a ? b : false   ->  a && b
							r = new ast::binary_node(ast::log_and, q->left, v->right);
						}
					}
				} else if (a) {
					// a ? true:false  ->  a
					// a ? true : b    ->  a || b
					r = b.exists() && !b ? q->left : new ast::binary_node(ast::log_or, q->left, v->right);
				} else {
					// a ? false:true  ->  !a
					// a ? false : b   ->  !a && b
					r = b.exists() && b ? q->left->flip_condition() : new ast::binary_node(ast::log_and, q->left->flip_condition(), v->right);
				}
			}
		});
	}

	for (auto &i : builder.blocks)
		ISO_TRACEF("block %i has %i incoming edges\n", i->id, i->incoming.size());

	// put in local decls
	{
		auto	r	= builder.entry_point();
		auto	j	= r->stmts.begin();
		for (auto &i : builder.locals[0]) {
			if (i) {
				j = r->stmts.insert(j, new ast::decl_node(i));
				++j;
			}
		}
	}
	return builder.entry_point();
}

//-----------------------------------------------------------------------------
//	Decompile
//-----------------------------------------------------------------------------

void Decompile(Context &ctx, string_accum &a, const Constant *type, const FieldorMethod *method) {
	/*
	if (method->IsCIL() && method->code.data) {
		const C_type		*ctype	= type ? ctx.GetType(type) : 0;
		const C_type		*func	= ctx.ReadSignature(method->signature, ctx.meta->GetEntry(method->paramlist));
		C_arg				*arg	= new C_arg(method->name, func);
		ast::funcdecl_node	*decl	= new ast::funcdecl_node(arg, ctype);
		C_arg				this_arg = C_arg("this", ctype);
		auto				p		= MakeAST(ctx, this_arg, decl, method->code.data);

		Dumper			dumper(&ctx, ctype, a);
		dumper.startline();
		dumper.DumpType(func, type ? type->name + "::" + special_name(*method, type->name) : method->name, true);
		dumper.scoped(), dumper.DumpCPP(p);
	}
	*/
}

void Decompile(Context &ctx, string_accum &a, const Constant *type) {
	/*
	indenter		sa(a << "namespace " << type->namespce << " {\n");
	const C_type	*ctype = ctx.GetType(type);
	DumpType(sa.startline(), ctype, type->name, 0, true) << ";\n";

	for (auto &i : ctx.meta->GetEntries(type, type->methods)) {
		ISO_TRACEF("Decompile ") << i.name << '\n';
		sa << '\n';
		Decompile(ctx, sa, type, &i);
	}
	sa << "\n\n} // namespace " << type->namespce;
	*/
}

} // namespace java

string Decompile(const java::FieldorMethod *method, java::ClassHeader* header) {
	string_builder		a;
	java::Context		ctx(header);
	java::Decompile(ctx, a, 0, method);
	return a;
}

string Decompile(java::ClassHeader* header) {
	string_builder		a;
	java::Context		ctx(header);
	java::Decompile(ctx, a, ctx.get_constant(ctx._info->this_class.idx));
	return a;
}
