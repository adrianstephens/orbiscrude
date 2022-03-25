#include "base/tree.h"
#include "base/algorithm.h"
#include "clr.h"
#include "cil.h"
#include "extra/ast.h"

using namespace iso;

namespace clr {

struct operator_name {
	const char *name;
	ast::KIND kind;
	bool operator==(const char *p) const { return str(name) == p; }
};
//unary
static operator_name unary_operator_names[] = {
	{"op_UnaryNegation",				ast::neg		},// - (unary)
	{"op_UnaryPlus",					ast::pos		},// + (unary)
	{"op_OnesComplement",				ast::bit_not	},// ~
	{"op_LogicalNot",					ast::log_not	},// !
	{"op_Decrement",					ast::dec		},// Similar to --1
	{"op_Increment",					ast::inc		},// Similar to ++1
//	{"op_True2",						ast::none		},// Not defined
//	{"op_False2",						ast::none		},// Not defined
	{"op_AddressOf",					ast::ref		},// & (unary)
	{"op_PointerDereference",			ast::deref		},// * (unary)
};
//binary
static operator_name binary_operator_names[] = {
	{"op_Addition",						ast::add		},// + (binary)
	{"op_Subtraction",					ast::sub		},// - (binary)
	{"op_Multiply",						ast::mul		},// * (binary)
	{"op_Division",						ast::div		},// /
	{"op_Modulus",						ast::mod		},// %
	{"op_BitwiseAnd",					ast::bit_and	},// & (binary)
	{"op_BitwiseOr",					ast::bit_or		},// |
	{"op_ExclusiveOr",					ast::bit_xor	},// ^
	{"op_LeftShift",					ast::shl		},// <<
	{"op_RightShift",					ast::shr		},// >>
//	{"op_SignedRightShift",				ast::none		},// Not defined
//	{"op_UnsignedRightShift",			ast::none		},// Not defined

	{"op_AdditionAssignment",			ast::add_assign	},// +=
	{"op_SubtractionAssignment",		ast::sub_assign	},// -=
	{"op_MultiplicationAssignment",		ast::mul_assign	},// *=
	{"op_DivisionAssignment",			ast::div_assign	},// /=
	{"op_ModulusAssignment",			ast::mod_assign	},// %=
	{"op_BitwiseAndAssignment",			ast::and_assign	},// &=
	{"op_BitwiseOrAssignment",			ast::or_assign	},// |=
	{"op_ExclusiveOrAssignment",		ast::xor_assign	},// ^=
	{"op_LeftShiftAssignment",			ast::shl_assign	},// <<=
	{"op_RightShiftAssignment",			ast::shr_assign	},// >>=
//	{"op_UnsignedRightShiftAssignment",	ast::none		},// Not defined

	{"op_Assign",						ast::assign		},// Not defined (= is not the same)
	{"op_LogicalAnd",					ast::log_and	},// &&
	{"op_LogicalOr",					ast::log_or		},// ||

	{"op_Equality",						ast::eq			},// ==
	{"op_GreaterThan",					ast::gt			},// >
	{"op_LessThan",						ast::lt			},// <
	{"op_Inequality",					ast::ne			},// !=
	{"op_GreaterThanOrEqual",			ast::ge			},// >=
	{"op_LessThanOrEqual",				ast::le			},// <=

	{"op_MemberSelection",				ast::field		},// ->
	{"op_PointerToMemberSelection",		ast::deref_field},// ->*
	{"op_Comma",						ast::comma		},// ,
};

string_param special_name(const clr::ENTRY<MethodDef> &method, const C_type_function *func, const char *type_name) {
	if (method.flags & clr::ENTRY<MethodDef>::SpecialName) {
		if (method.name == ".dtor")
			return string("~") + type_name;
		if (method.name == ".ctor")
			return type_name;
		if (method.name.begins("op_")) {
			if (auto i = find_check(unary_operator_names, method.name))
				return cstr("operator") + ast::unary_node(i->kind, 0).get_info()->name;
			if (auto i = find_check(binary_operator_names, method.name))
				return cstr("operator") + ast::binary_node(i->kind, 0, 0).get_info()->name;
		}
	}
	return method.name;
}

C_type::FLAGS	function_flags(const clr::ENTRY<MethodDef> &i) {
	return (i.name == ".dtor" || i.name == ".ctor"	? C_type_function::NORETURN : C_type::NONE)
		| (i.flags & i.Virtual	? C_type_function::VIRTUAL : C_type::NONE)
		| (i.flags & i.Abstract	? C_type_function::ABSTRACT : C_type::NONE);
};

ast::node *fix_type(ast::node *n, const C_type *type) {
	if (type && n->kind == ast::literal) {
		ast::lit_node	*lit		= (ast::lit_node*)n;
		C_type::TYPE	lit_type	= lit->type->type;

		switch (type->type) {
			case C_type::INT:
				if (lit_type == C_type::INT)
					lit->type = type;
				break;
			case C_type::FLOAT:
				if (lit_type == C_type::FLOAT)
					lit->type = type;
				break;
			case C_type::POINTER:
				if (lit_type == C_type::INT || lit_type == C_type::POINTER)
					lit->type = type;
				break;
		}
	}
	return n;
}

bool check_type(const C_type *type, const char *id, C_type_namespace *ns) {
	if (type) {
		if (auto c = type->composite())
			return c->id == id && c->parent == ns;
	}
	return false;
}

bool is_value_type(const C_type *type) {
	const C_type *base;
	while (type->type == C_type::STRUCT && (base = ((const C_type_struct*)type)->get_base()))
		type = base;

	return type->type != C_type::STRUCT || ((const C_type_struct*)type)->id == "ValueType";
}

enum bool3 { false3 = 1, true3 = 2};
bool3 is_bool_lit(const ast::node *n) {
	if (const ast::lit_node *lit = n->cast()) {
		if (lit->type == C_types::get_static_type<bool>())
			return lit->v ? true3 : false3;
	}
	return (bool3)false;
}

struct clr_string {
	const void *p;
	clr_string(const void *_p) : p(_p) {}
	operator uint64()			{ return (uint64)p; }
	operator count_string16()	{
		byte_reader		r(p);
		CompressedUInt	u(r);
		return count_string16((const char16*)r.p, (u - 1) / 2);
	}
};

//-----------------------------------------------------------------------------
//	Context
//-----------------------------------------------------------------------------

struct Context {
	METADATA			*meta;
	C_types				ctypes;
	C_type_namespace	*ns_system;
	const C_type		*type_typedbyref, *type_object, *type_string, *type_uint8ptr, *type_dynamic_array;
	C_element			array_len;

	hash_map_with_key<uint64, const C_type*>	signature_to_type;

	Context(METADATA *_meta) : meta(_meta) {
		ns_system			= new C_type_namespace("System");
		ctypes.add("System", ns_system);
		type_typedbyref		= ns_system->add_child("typedbyref",	new C_type_struct("typedbyref"	));
		type_object			= ns_system->add_child("object",		new C_type_struct("object"		));
		type_string			= ns_system->add_child("string",		new C_type_custom("string",	[](string_accum &sa, const void *p)->string_accum& {
			return sa << '"' << escaped((count_string16)*(clr_string*)p) << '"';
		}));
		//type_dynamic_array	= ns_system->add_child("dynamic_array",	new C_type_template(new C_type_struct("dynamic_array", C_type::TEMPLATED), 1));
		type_dynamic_array	= ns_system->add_child("dynamic_array",	new C_type_template(new C_type_struct("dynamic_array"), 1));
		type_uint8ptr		= ctypes.add(C_type_pointer(C_type_int::get<uint8>(), 32, false));

		array_len.init("Length", ctypes.get_type(&dynamic_array<int>::size), 0);
	}
	ast::node*			TakeAddress(ast::node *arg) {
		return new ast::unary_node(ast::ref, arg, type_uint8ptr);
	}

	template<typename T> const C_type	*GetType() { return ctypes.get_type<T>(); }
	const C_type*		GetType(const ENTRY<TypeDef> *x, const char **name = 0);
	const C_type*		GetType(const ENTRY<TypeRef> *x, const char **name = 0);
	const C_type*		GetType(const Token &tok, const char **name = 0);
	const C_type*		ReadType(byte_reader &r);
	const C_type*		ReadSignature(byte_reader &r, const C_type *parent, ENTRY<Param> *params = 0, C_type::FLAGS flags = C_type::NONE);
	const C_type*		ReadSignature(const Signature &s, const C_type *parent, ENTRY<Param> *params = 0, C_type::FLAGS flags = C_type::NONE);
	const C_element*	GetElement(const Token &tok, const C_type *&parent);

	count_string16		GetUserString(uint32 i)	{
		void			*p	= meta->GetHeap(HEAP_UserString, i);
		byte_reader		r(p);
		CompressedUInt	u(r);
		return count_string16((const char16*)r.p, (u - 1) / 2);
	}
	count_string16		GetUserString(const Token &tok)	{
		ISO_ASSERT(tok.type() == StringHeap);
		return GetUserString(tok.index());
	}

	clr_string			GetUserString0(const Token &tok)	{
		ISO_ASSERT(tok.type() == StringHeap);
		return meta->GetHeap(HEAP_UserString, tok.index());
	}
};

C_element::ACCESS get_access(clr::ACCESS a) {
	switch (a) {
		case clr::ACCESS_CompilerControlled:
		case clr::ACCESS_Private:
			return C_element::PRIVATE;
		case clr::ACCESS_FamANDAssem:
		case clr::ACCESS_Family:
			return C_element::PROTECTED;
		default:
			return C_element::PUBLIC;
	}
};

const C_type *Context::GetType(const ENTRY<TypeDef> *p, const char **name) {
	if (name)
		*name = p->name;
	auto	t	= signature_to_type[uintptr_t(p)];
	if (t.exists())
		return t;

	t = nullptr;

	static recursion_checker<const ENTRY<TypeDef>*>	recursion;
	auto	rec_check	= recursion.check(p);
	ISO_ASSERT(rec_check);

	const C_type	*ns		= 0;
	if (p->namespce && !(ns = ctypes.lookup(p->namespce)))
		ns = ctypes.add(p->namespce, new C_type_namespace(p->namespce));

	const C_type	*base	= p->extends ? GetType(p->extends) : 0;
	if (check_type(base, "Enum", ns_system)) {
		C_type_enum		*e = new C_type_enum(32);
		t	= e;
		for (auto &i : meta->GetEntries(p, p->fields)) {
			if (i.flags & i.Literal) {
				bool	found	= false;
				Token	itok	= meta->GetIndexed(&i);
				for (auto &j : meta->GetTable<Constant>()) {
					if (j.parent == itok) {
						new(*e) C_enum(i.name, *j.value);
						found = true;
						break;
					}
				}
				ISO_ASSERT(found);
			} else {
				ISO_ASSERT(i.name == "value__");
				C_type_int	*it	= (C_type_int*)ReadSignature(i.signature, nullptr);
				ISO_ASSERT(it->type == C_type::INT);
				e->set_size(it->num_bits(), it->sign());
			}
		}
		return e;
	}

	C_type_struct	*comp	= new C_type_struct(p->name);
	t				= comp;
	comp->parent	= ns;

	if (base)
		comp->add(0, base);

	for (auto &i : meta->GetTable<InterfaceImpl>()) {
		if (meta->GetEntry(i.clss) == p)
			comp->add(0, GetType(i.interfce));
	}

	for (auto &i : meta->GetTable<NestedClass>()) {
		if (meta->GetEntry(i.enclosing_class) == p) {
			const char		*sub_name	= 0;
			const C_type	*sub		= GetType(i.nested_class, &sub_name);
			comp->add_child(sub_name, sub);
		}
	}

	for (auto &i : meta->GetEntries(p, p->fields)) {
		if (i.flags & i.Literal) {
			bool	found	= false;
			Token	itok	= meta->GetIndexed(&i);
			for (auto &j : meta->GetTable<Constant>()) {
				if (j.parent == itok) {
					comp->add_child(i.name, ReadSignature(i.signature, comp));
					found = true;
					break;
				}
			}
			ISO_ASSERT(found);

		} else {
			comp->add(
				i.name.begins("<backing_store>") ? (const char*)("_" + i.name.slice(15)) : i.name,
				ReadSignature(i.signature, comp),
				get_access(i.access()),
				!!(i.flags & i.Static)
			);
		}
	}

	for (auto &i : meta->GetEntries(p, p->methods)) {
		//ISO_TRACEF("name=") << i.name << '\n';
		if (i.name == ".cctor")	// don't add static (class) constructor
			continue;

		C_type_function	*func = (C_type_function*)ReadSignature(i.signature, comp, meta->GetEntry(i.paramlist), function_flags(i));
		//if (func->has_this())
		//	func->args[0].type = ctypes.add(C_type_pointer(comp, 32, false));

		C_element		*e = comp->add(
			//i.name,
			special_name(i, func, comp->id),
			func,
			get_access(i.access()),
			true
		);
	}
	return comp;
}

const C_type *Context::GetType(const ENTRY<TypeRef> *p, const char **name) {
	if (name)
		*name = p->name;
	auto	t = signature_to_type[uintptr_t(p)];
	if (t.exists())
		return t;

	ISO_TRACEF("Adding ") << p->namespce << "::" << p->name << "\n";

	C_type_struct	*comp = new C_type_struct(p->name);

	const C_type *ns = 0;
	if (p->namespce && !(ns = ctypes.lookup(p->namespce)))
		ns = ctypes.add(p->namespce, new C_type_namespace(p->namespce));
	comp->parent = ns;

	t = comp;
	return comp;
}

const C_type *Context::GetType(const Token &tok, const char **name) {
	switch (tok.type()) {
		case TypeRef:
			return GetType(meta->GetEntry<TypeRef>(tok.index()), name);
		case TypeDef:
			return GetType(meta->GetEntry<TypeDef>(tok.index()), name);
		case TypeSpec:
			return ReadType(unconst(byte_reader(meta->GetEntry<TypeSpec>(tok.index())->signature)));
		default:
			return 0;
	}
}

const C_type *Context::ReadType(byte_reader &r) {
	uint32	flags;
	for (;;) {
		switch (r.getc()) {
			case ELEMENT_TYPE_CMOD_REQD: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				continue;
			}
			case ELEMENT_TYPE_CMOD_OPT: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				continue;
			}
			case ELEMENT_TYPE_SYSTEMTYPE:
				continue;

			case ELEMENT_TYPE_PINNED:
				continue;

			case ELEMENT_TYPE_BYREF:
				return ctypes.add(C_type_pointer(ReadType(r), 32, true, true));

			case ELEMENT_TYPE_CLASS:
				return ctypes.add(C_type_pointer(GetType(TypeDefOrRef(CompressedUInt(r))), 32, true, true));

			case ELEMENT_TYPE_VALUETYPE:
				return GetType(TypeDefOrRef(CompressedUInt(r)));

			case ELEMENT_TYPE_TYPEDBYREF:	return type_typedbyref;
			case ELEMENT_TYPE_OBJECT:		return type_object;
			case ELEMENT_TYPE_STRING:		return type_string;
			case ELEMENT_TYPE_INTERNAL:		return 0;

			case ELEMENT_TYPE_VOID:			return 0;
			case ELEMENT_TYPE_BOOLEAN:		return C_types::get_static_type<bool>();
			case ELEMENT_TYPE_CHAR:			return C_type_int::get<char>();
			case ELEMENT_TYPE_I1:			return C_type_int::get<int8>();
			case ELEMENT_TYPE_U1:			return C_type_int::get<uint8>();
			case ELEMENT_TYPE_I2:			return C_type_int::get<int16>();
			case ELEMENT_TYPE_U2:			return C_type_int::get<uint16>();
			case ELEMENT_TYPE_I4:			return C_type_int::get<int32>();
			case ELEMENT_TYPE_U4:			return C_type_int::get<uint32>();
			case ELEMENT_TYPE_I8:			return C_type_int::get<int64>();
			case ELEMENT_TYPE_U8:			return C_type_int::get<uint64>();
			case ELEMENT_TYPE_R4:			return C_type_float::get<float>();
			case ELEMENT_TYPE_R8:			return C_type_float::get<double>();
			case ELEMENT_TYPE_I:			return C_type_int::get<int>();
			case ELEMENT_TYPE_U:			return C_type_int::get<unsigned int>();
				break;

			case ELEMENT_TYPE_ARRAY: {
				const C_type	*t = ReadType(r);
				CompressedUInt	rank(r);
				CompressedUInt	num_sizes(r);

				for (uint32 i = num_sizes; i--;)
					t = ctypes.add(C_type_array(t, CompressedUInt(r)));

				for (uint32 i = rank - num_sizes; i--;)
					t = ctypes.add(C_type_array(t, 0));

				CompressedUInt	num_lobounds(r);
				for (uint32 i = num_lobounds; i--;)
					CompressedInt	lobound(r);
				return t;
			}
			case ELEMENT_TYPE_FNPTR:
				return ReadSignature(r, nullptr);

			case ELEMENT_TYPE_GENERICINST: {
				ELEMENT_TYPE	t2		= ELEMENT_TYPE(r.getc());	// ELEMENT_TYPE_CLASS or ELEMENT_TYPE_VALUETYPE
				const char		*name;
				const C_type	*t		= GetType(TypeDefOrRef(CompressedUInt(r)), &name);
				uint32			n		= CompressedUInt(r);

#if 1
				C_type_template	temp(t, n);
				for (uint32 i = 0; i < n; ++i)
					temp.args[i].type = ReadType(r);

				return ctypes.add(temp);
#else
				
				const C_type	**args	= alloc_auto(const C_type*, n);
				bool			instantiate	= false;
				for (uint32 i = 0; i < n; ++i) {
					args[i] = ReadType(r);
					instantiate = instantiate || (args[i]->type != C_type::TEMPLATE_PARAM);
				}
				return instantiate ? ctypes.instantiate(t, name, args) : t;
#endif

			}
			case ELEMENT_TYPE_MVAR:
			case ELEMENT_TYPE_VAR: {
				uint32	n = CompressedUInt(r);
				return ctypes.add(C_type_templateparam(n));
			}
			case ELEMENT_TYPE_PTR:
				return ctypes.add(C_type_pointer(ReadType(r), 32, false));

			case ELEMENT_TYPE_SZARRAY:
				return ctypes.instantiate(type_dynamic_array, "dynamic_array", ReadType(r));

			case ELEMENT_TYPE_SENTINEL:	flags |= 1; continue;
			case ELEMENT_TYPE_BOXED:	flags |= 2; continue;
			case ELEMENT_TYPE_FIELD:	flags |= 4; continue;
			case ELEMENT_TYPE_PROPERTY:	flags |= 8; continue;
			case ELEMENT_TYPE_ENUM:		flags |= 16; continue;

			case ELEMENT_TYPE_END:
				return 0;
		}
	}
}

const C_type *Context::ReadSignature(byte_reader &r, const C_type *parent, ENTRY<Param> *params, C_type::FLAGS flags) {
	SIGNATURE	sflags = SIGNATURE(r.getc());

	switch (sflags & TYPE_MASK) {
		case GENERIC: {
			uint32	gen_param_count	= CompressedUInt(r);
		}
		case DEFAULT:
		case C:
		case STDCALL:
		case THISCALL:
		case FASTCALL:
		case VARARG: {
			uint32	param_count = CompressedUInt(r);
			// return type
			C_type_function	fn(ReadType(r), (sflags & HASTHIS) && !(sflags & EXPLICITTHIS) ? parent : nullptr);
			// params
			for (uint32 i = 0; i < param_count; ++i)
				fn.add(params ? (const char*)params++->name : 0, ReadType(r));
			fn.flags |= flags;
			return ctypes.add(fn);
		}
		case FIELD:
			return ReadType(r);

		case PROPERTY: {
			uint32	param_count = CompressedUInt(r);
			// type
			C_type_function	fn(ReadType(r));
			// params
			for (uint32 i = 0; i < param_count; ++i)
				fn.add(params ? (const char*)params++->name : 0, ReadType(r));
			fn.flags |= flags;
			return ctypes.add(fn);
		}

		case GENRICINST: {
			uint32	gen_param_count = CompressedUInt(r);
			// params
			C_type_composite	c(C_type::NAMESPACE);
			for (uint32 i = 0; i < gen_param_count; ++i)
				c.add(0, ReadType(r), 0);
			return ctypes.add(c);
		}
	}
	return 0;
}

const C_type *Context::ReadSignature(const Signature &s, const C_type *parent, ENTRY<Param> *params, C_type::FLAGS flags) {
	uint64	key = uintptr_t((const char*)s.begin() - meta->heaps[0]) ^ (uint64(flags) << 56);
	if (parent)
		key ^= crc32(parent->tag());
	auto	type = signature_to_type[key];
	if (!type.exists()) {
		ISO_ASSERT(key != 0x92a07);
		C_type	dummy(C_type::UNKNOWN);
		type = &dummy;
		type = ReadSignature(unconst(byte_reader(s)), parent, params, flags);
	} else if (type->type == C_type::UNKNOWN) {
		type = ReadSignature(unconst(byte_reader(s)), parent, params, flags);
	}
	return type;
}

const C_element *Context::GetElement(const Token &tok, const C_type *&parent) {
	switch (tok.type()) {
		case Field: {
			auto			*p		= meta->GetEntry<Field>(tok.index());
			//ISO_ASSERT(p->name != "get_width");
			parent					= GetType(meta->GetOwner(p, &clr::ENTRY<clr::TypeDef>::fields));
			const C_type	*type	= ReadSignature(p->signature, parent);
			return parent->composite()->get(p->name);
		}
		case MethodDef: {
			auto			*p		= meta->GetEntry<MethodDef>(tok.index());
			//ISO_ASSERT(p->name != "get_width");
			parent					= GetType(meta->GetOwner(p, &clr::ENTRY<clr::TypeDef>::methods));
			const C_type	*type	= ReadSignature(p->signature, parent, 0, function_flags(*p));
			auto	c	= parent->skip_template()->composite();
			return c->get(special_name(*p, (C_type_function*)type, c->id), type);
			//return c->get(p->name, type);
		}
		case MemberRef: {
			auto			*p		= meta->GetEntry<MemberRef>(tok.index());
			//ISO_ASSERT(p->name != "get_width");
			parent					= GetType(p->clss);
			const C_type	*type	= ReadSignature(p->signature, parent);
			string			id		= p->name;

			if (auto c = parent->skip_template()->composite()) {
				auto	e	= c->get(id, type);
				if (!e)
					e = unconst(c)->add(id, type, 0);
				return e;
			} else {
				return new C_element(id, type);	//LEAK!
			}
		}
		case MethodSpec: {
			auto	*p = meta->GetEntry<MethodSpec>(tok.index());
			if (const C_element *e = GetElement(p->method, parent)) {
				byte_reader r(p->instantiation);
				SIGNATURE	flags = SIGNATURE(r.getc());
				ISO_ASSERT(flags == GENRICINST);

				uint32			n = CompressedUInt(r);

#if 1
				C_type_template	temp(e->type, n);
				for (uint32 i = 0; i < n; ++i)
					temp.args[i].type = ReadType(r);

				return new C_element(e->id, ctypes.add(temp));
#else
				const C_type	**c = alloc_auto(const C_type*, n);
				for (uint32 i = 0; i < n; ++i)
					c[i] = ReadType(r);

				string	id = e->id;
				if (uint64 needed = ctypes.inferable_template_args(e->type) ^ bits(n)) {
					auto	b	= build(id);
					b << '<';
					for (int j = 0; needed; needed = clear_lowest(needed))
						DumpType(b << onlyif(j++, ','), c[lowest_set_index(needed)], 0, 0);
					b << '>';
				}
				return new C_element(id, ctypes.instantiate(e->type, e->id, c));
#endif
			}
			return 0;
		}
		case StandAloneSig: {
			parent = nullptr;
			const C_type *type = ReadSignature(unconst(byte_reader(meta->GetEntry<StandAloneSig>(tok.index())->signature)), parent);
			return new C_element(0, type);
		}
		default:
			return 0;
	}
}

//-----------------------------------------------------------------------------
//	Stack
//-----------------------------------------------------------------------------

struct Stack {
	enum TYPE {
		NONE,
		INT32,
		INT64,
		NATIVE_INT,
		FLOAT,
		PTR,
		OBJECT,
		USERVALUE,
	};
	enum STATE {
		STACK_UNSET,
		STACK_SET,
		STACK_VARS,
	};
	enum {
		SIDE_EFFECTS	= 1 << 0,
	};
	struct ENTRY {
		ast::node	*p;
		TYPE		t:8;
		uint32		flags:24;
		uint32		offset;
		ENTRY(ast::node *p, TYPE t, uint32 offset, uint32 flags) : p(p), t(t), flags(flags), offset(offset) {}
		operator ast::node*() const		{ return p; }
		ast::node*	operator->() const	{ return p; }
	};
	dynamic_array<ENTRY>	sp;
	STATE					state;

	Stack() : state(STACK_UNSET) {}

	void			push(ast::node *p, TYPE t, uint32 offset = 0, uint32 flags = 0)	{ sp.emplace_back(p, t, offset, flags); }
	void			push(const ENTRY &e)	{ sp.push_back(e); }
	const ENTRY&	pop()					{ ISO_ASSERT(sp.size()); return sp.pop_back_retref(); }
	const ENTRY&	top() const				{ return sp.back(); }
	ENTRY&			top()					{ return sp.back(); }
	void			end()					{ sp.clear(); }
	bool			end(int i)				{ return sp.size() == i; }

	bool	compatible(const Stack &s) const {
		if (sp.size() != s.sp.size())
			return false;
		for (auto i0 = sp.begin(), i1 = s.sp.begin(), e0 = sp.end(); i0 != e0; ++i0, ++i1) {
			if (i0->t != i1->t)
				return false;
		}
		return true;
	}
	int		first_unequal(const Stack &s) const {
		for (auto i0 = sp.begin(), i1 = s.sp.begin(), e0 = sp.end(); i0 != e0; ++i0, ++i1) {
			if (*i0->p != *i1->p)
				return e0 - i0;
		}
		return 0;
	}
};

static_assert(sizeof(Stack::ENTRY) == 16, "ugh");
//-----------------------------------------------------------------------------
//	Builder
//-----------------------------------------------------------------------------

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
	dynamic_array<local_var*>	locals;

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

	void	create_locals(Token tok) {
		if (tok) {
			ISO_ASSERT(tok.type() == StandAloneSig);
			byte_reader	r(ctx.meta->GetEntry<StandAloneSig>(tok.index())->signature);
			SIGNATURE	flags = SIGNATURE(r.getc());
			ISO_ASSERT((flags & TYPE_MASK) == LOCAL_SIG);

			uint32	n = CompressedUInt(r);
			locals.resize(n);
			for (int i = 0; i < n; ++i)
				locals[i] = new local_var(format_string("temp%i", i), ctx.ReadType(r));

		} else {
			locals.clear();
		}
	}

	block *at_offset(uint32 target) {
		return lower_boundc(blocks, target, [](const block &a, uint32 b) {
			return a.offset < b;
		});
	}

	local_var *is_local(const ast::node *n) const {
		if (const ast::element_node *en = n->cast()) {
			if (local_var *const *lv = find_check(locals, (local_var*)en->element))
				return *lv;
		}
		return 0;
	}


	ref_ptr<ast::basicblock>&	entry_point() const {
		return blocks[0];
	}

	static TYPE	type_to_stack(const C_type *type) {
		if (type) {
			switch (type->type) {
				case C_type::INT:		return ((C_type_int*)type)->num_bits() > 32 ? INT64 : INT32;
				case C_type::FLOAT:		return FLOAT;
				case C_type::STRUCT:	return OBJECT;
				case C_type::ARRAY:		return OBJECT;
				case C_type::POINTER:	return ((C_type_pointer*)type)->managed() ? OBJECT : PTR;
				default:				return PTR;
			}
		}
		return PTR;
	}
	const C_type *stack_to_type(TYPE t) {
		const C_type *types[] = {
			0,								//NONE,
			C_type_int::get<int32>(),		//INT32,
			C_type_int::get<int64>(),		//INT64,
			C_type_int::get<int>(),			//NATIVE_INT,
			C_type_float::get<double>(),	//FLOAT,
			ctx.type_uint8ptr,				//PTR,
			ctx.type_object,				//OBJECT,
			0,	//USERVALUE,
		};
		return types[t];
	}

	void	push(ast::node *p, TYPE t, uint32 flags = 0)	{ Stack::push(p, t, offset, flags); }
	void	push(const ENTRY &e)							{ Stack::push(e.p, e.t, offset, e.flags); }

	uint32	get_offset(const byte_reader &r) const {
		return r.p - code;
	}
	bool	is_float()		const	{
		return top().t == FLOAT;
	}

	ENTRY	fix_ptr(ENTRY e) {
		if (e.t == PTR && e.p->kind == ast::var)
			e.p = new ast::unary_node(ast::cast, e.p, ctx.type_uint8ptr);
		return e;
	}

	ast::element_node*	element0(ast::KIND k, Token tok) {
		const C_type		*parent;
		const C_element		*e = ctx.GetElement(tok, parent);
		return new ast::element_node(k, e, parent);
	}

	ast::node*	binary_cmp0(ast::KIND k, uint32 flags = 0) {
		const ENTRY &right	= pop();
		const ENTRY &left	= pop();
		return new ast::binary_node(k, left.p, right.p, flags);
	}
	ast::node*	ldelem0(const C_type *type) {
		const ENTRY &right	= pop();
		const ENTRY &left	= pop();
		return new ast::binary_node(ast::ldelem, left.p, right.p, type);
	}
	ast::node*	var0(const C_arg *v, uint32 _flags = 0) {
		return new ast::element_node(ast::var, (const C_element*)v, 0, _flags);
	}
//	ast::node*	var0(local_var *v, uint32 _flags = 0) {
//		return new linked_var_node(v, bb, _flags);
//	}

	void	unary(ast::KIND k, uint32 flags = 0)	{
		const ENTRY &arg	= pop();
		push(new ast::unary_node(k, arg.p, flags), arg.t);
	}
	void	binary(ast::KIND k, uint32 flags = 0) {
		ENTRY right	= fix_ptr(pop());
		ENTRY left	= fix_ptr(pop());
		push(new ast::binary_node(k, left.p, right.p, flags), (TYPE)max(left.t, right.t));
	}
	void	binary_cmp(ast::KIND k, uint32 flags = 0)	{
		push(binary_cmp0(k, flags), INT32);
	}
	void	ldelem(const C_type *type, TYPE stack_type) {
		push(ldelem0(type), stack_type);
	}
	void	cast(const C_type *type, TYPE stack_type) {
		TYPE	top_type = top().t;
		if (top_type == stack_type)
			return;
		if (top_type == NATIVE_INT && (stack_type == INT32 || stack_type == INT64)) {
			top().t = stack_type;
			return;
		}
		push(new ast::unary_node(ast::cast, pop(), type), stack_type);
	}
	void	var(local_var *v) {
		push(var0(v), type_to_stack(v->type));
	}
	void	var(const C_arg *v) {
		push(var0(v), type_to_stack(v->type));
	}

	void	get_args(ast::call_node *call, C_arg *args, size_t nargs) {
		call->args.resize(nargs);
		for (size_t i = nargs; i--;)
			call->args[i] = fix_type(pop(), args[i].type);
	}

	template<typename T> ast::node* lit0(const T &t) {
		return new ast::lit_node(C_value(t));
	}

	template<typename T> void lit(const T &t, TYPE stack_type) {
		push(lit0(t), stack_type);
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

	ast::basicblock *branch_stack(block *e) {
		if (e->stack.state) {
			if (!e->stack.compatible(*this)) {
				ISO_TRACEF("Incompatible stack!\n");

			} else if (!sp.empty() && state != STACK_VARS) {
				//copy stack vars from e
				auto	i2	= e->stack.sp.begin();
				for (auto &i : sp) {
					//linked_var_node	*lvn	= (linked_var_node*)(i2++->p);
					ast::element_node	*lvn	= i2++->p->cast();
					if (*i != *lvn) {
						local_var		**lv	= find_check(locals, (local_var*)lvn->element);
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
					locals.push_back(arg);
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

	template<typename T> const uint32 read_branch(byte_reader &r) {
		T	t = r.get<T>();
		return r.p + t - code;
	}

	void branch(uint32 target) {
		add_stmt(new ast::branch_node(
			branch_stack(at_offset(target))
		));
	}

	template<typename T> void condbranch(byte_reader &r, ast::node *cond) {
		uint32	target1	= read_branch<T>(r);
		add_stmt(new ast::branch_node(
			branch_stack(pbb + 1),
			branch_stack(at_offset(target1)),
			cond
		));
	}

	bool check_end(byte_reader &r) {
		offset		= get_offset(r);

		if (pbb != blocks.end() - 1 && offset == pbb[1].offset) {
			if (bb->stmts.empty() || !bb->stmts.back()->branches())
				bb->stmts.push_back(new ast::branch_node(branch_stack(pbb + 1)));

			else if (bb->stmts.back()->branches(true))
				sp = pbb[1].stack.sp;

			bb		= *++pbb;
			state	= STACK_SET;

		} else if (bb && !bb->stmts.empty() && bb->stmts.back()->branches(true)) {
			// dead code?
			if (pbb == blocks.end() - 1)
				return false;

			bb		= *++pbb;
			sp		= pbb->stack.sp;
			offset	= pbb->offset;
			r		= (const uint8*)code + offset;
			state	= STACK_SET;
		}
		return r.p < code.end();
	}

	void	count_local_use(postorder<ast::DominatorTree::Info> &&po);
	void	remove_redundant_locals();
};


void Builder::count_local_use(postorder<ast::DominatorTree::Info> &&po) {
	for (auto &i : locals) {
		if (i)
			i->uses.clear();
	}
	for (auto i : po) {
		ast::basicblock	*bb	= unconst(i->node);
		bb->apply([this, bb](ref_ptr<ast::node> &r) {
			if (r->kind == ast::assign) {
				if (auto *lv = is_local(r->cast<ast::binary_node>()->left))
					lv->add_use(bb, r);
			}
			if (r->kind == ast::var) {
				if (auto *lv = is_local(r))
					lv->add_use(bb, r);
			}
		});
	}
}

void Builder::remove_redundant_locals() {
	for (auto &i : locals) {
		if (i) {
			int		assigns	= 0, reads = 0;
			Builder::local_var::use	*last_assign	= 0;
			for (auto &u : i->uses) {
				if (u.n->kind == ast::assign) {
					--reads;
					if (!last_assign || *last_assign->n != *u.n) {
						last_assign = &u;
						++assigns;
					}
				} else {
					++reads;
				}
			}

			if (assigns == 1) {
				ast::node			*n		= last_assign->n;
				ref_ptr<ast::node>	s		= n->cast<ast::binary_node>()->right;

				if (reads == 1 || s->kind == ast::var) {
					const C_arg			*v	= i;
					ref_ptr<ast::node>	ass	= last_assign->b->remove_last_assignment(v);

					ISO_ASSERT(ass == n);

					for (auto &b : blocks) {
						b->apply([v, s](ref_ptr<ast::node> &r) {
							if (r->kind == ast::var) {
								if (r->cast<ast::element_node>()->element == v)
									r = s;
							}
						});
					}
					i = 0;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	MakeAST
//-----------------------------------------------------------------------------

ref_ptr<ast::basicblock> MakeAST(Context &ctx, ast::funcdecl_node *func, const ILMETHOD *ilmethod) {
	Builder		builder(ctx, ilmethod->GetCode());

	// find branch destinations
	dynamic_array<uint32>	dests;
	for (byte_reader r(builder.code); r.p < builder.code.end(); ) {
		uint32		offset	= builder.get_offset(r);
		cil::OP		op		= (cil::OP)r.getc();
		if (op != cil::PREFIX1) {
			cil::FLAGS	flags	= cil::get_flags(op);
			uint32		len		= cil::param_len(flags.params());

			switch (flags.flow()) {
				default:
					r.skip(len);
					break;

				case cil::FLOW_BRANCH:
					switch (flags.params()) {
						case cil::METHOD:
							//dests.push_back(make_pair(offset, ~0));
							break;
						case cil::TARGET_8:
							dests.push_back(builder.read_branch<int8>(r));
							break;
						case cil::TARGET_32:
							dests.push_back(builder.read_branch<int32>(r));
							break;
					}
					// in case dead code left in here?
					//dests.push_back(builder.get_offset(r));
					break;

				case cil::FLOW_COND_BRANCH:
					switch (flags.params()) {
						case cil::TARGET_8:
							dests.push_back(builder.read_branch<int8>(r));
							break;
						case cil::TARGET_32:
							dests.push_back(builder.read_branch<int32>(r));
							break;
						case cil::SWITCH: {
							uint32		n		= r.get<uint32le>();
							uint32		here	= builder.get_offset(r) + n * 4;
							for (int i = 0; i < n; i++)
								dests.push_back(here + r.get<int32le>());
							dests.push_back(here);
							break;
						}
					}
					dests.push_back(builder.get_offset(r));
					break;

/*				case cil::FLOW_RETURN:
					bb = new(bbs) ast::basicblock(builder.get_offset(r));
					break;
				case cil::FLOW_THROW:
					bb = bb->next = new(bbs) ast::basicblock(builder.get_offset(r));
					break;
*/
			}
		} else {
			cil::OP_FE	op		= (cil::OP_FE)r.getc();
			cil::FLAGS	flags	= cil::get_flags(op);
			uint32		len		= cil::param_len(flags.params());
			r.skip(len);
		}
	}

	// create basic blocks
	builder.create_blocks(dests);
	builder.create_locals(ilmethod->GetLocalVarSig());

	uint32	flags		= 0;
	for (byte_reader r(builder.code); builder.check_end(r); ) {

		switch (r.getc()) {
			case cil::NOP:			break;
			case cil::BREAK:		break;
			case cil::LDARG_0:		builder.var(func->get_arg(0)); break;
			case cil::LDARG_1:		builder.var(func->get_arg(1)); break;
			case cil::LDARG_2:		builder.var(func->get_arg(2)); break;
			case cil::LDARG_3:		builder.var(func->get_arg(3)); break;
			case cil::LDLOC_0:		builder.var(builder.locals[0]); break;
			case cil::LDLOC_1:		builder.var(builder.locals[1]); break;
			case cil::LDLOC_2:		builder.var(builder.locals[2]); break;
			case cil::LDLOC_3:		builder.var(builder.locals[3]); break;
			case cil::STLOC_0:		builder.assign(builder.locals[0]); break;
			case cil::STLOC_1:		builder.assign(builder.locals[1]); break;
			case cil::STLOC_2:		builder.assign(builder.locals[2]); break;
			case cil::STLOC_3:		builder.assign(builder.locals[3]); break;
			case cil::LDARG_S:		builder.var(func->get_arg(r.getc())); break;
			case cil::LDARGA_S:		builder.push(ctx.TakeAddress(builder.var0(func->get_arg(r.getc()))), Stack::PTR); break;
			case cil::STARG_S:		builder.assign(func->get_arg(r.getc())); break;
			case cil::LDLOC_S:		builder.var(builder.locals[r.getc()]); break;
			case cil::LDLOCA_S:		builder.push(ctx.TakeAddress(builder.var0(builder.locals[r.getc()])), Stack::PTR); break;
			case cil::STLOC_S:		builder.assign(builder.locals[r.getc()]); break;
			case cil::LDNULL:		builder.push(new ast::node(ast::null), Stack::OBJECT); break;
			case cil::LDC_I4_M1:	builder.lit(-1, Stack::INT32); break;
			case cil::LDC_I4_0:		builder.lit(0, Stack::INT32); break;
			case cil::LDC_I4_1:		builder.lit(1, Stack::INT32); break;
			case cil::LDC_I4_2:		builder.lit(2, Stack::INT32); break;
			case cil::LDC_I4_3:		builder.lit(3, Stack::INT32); break;
			case cil::LDC_I4_4:		builder.lit(4, Stack::INT32); break;
			case cil::LDC_I4_5:		builder.lit(5, Stack::INT32); break;
			case cil::LDC_I4_6:		builder.lit(6, Stack::INT32); break;
			case cil::LDC_I4_7:		builder.lit(7, Stack::INT32); break;
			case cil::LDC_I4_8:		builder.lit(8, Stack::INT32); break;
			case cil::LDC_I4_S:		builder.lit(r.getc(), Stack::INT32); break;
			case cil::LDC_I4:		builder.lit(r.get<int32>(), Stack::INT32); break;
			case cil::LDC_I8:		builder.lit(r.get<int64>(), Stack::INT64); break;
			case cil::LDC_R4:		builder.lit(r.get<float>(), Stack::FLOAT); break;
			case cil::LDC_R8:		builder.lit(r.get<double>(), Stack::FLOAT); break;
			case cil::DUP:
				if (builder.top().flags & Stack::SIDE_EFFECTS) {
					auto	*loc = new Builder::local_var(format_string("sidefx%i", builder.offset), builder.top()->type);
					builder.locals.push_back(loc);
					builder.assign(loc);
					builder.var(loc);
				}
				builder.push(builder.top());
				break;
			case cil::POP:			builder.pop(); break;
			case cil::JMP: {
				Token					tok		= r.get();
				builder.add_stmt(builder.element0(ast::jmp, tok));
				break;
			}
			case cil::CALL: {
				Token					tok		= r.get();
				const C_type			*parent	= 0;
				const C_element			*e		= ctx.GetElement(tok, parent);
				const C_type_function	*func	= (const C_type_function*)e->type->skip_template();
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, parent));

				builder.get_args(call, func->args, func->num_args());

				auto	stack_type = builder.type_to_stack(func->subtype);

				if (tok.type() == MethodDef) {
					auto	*method		= ctx.meta->GetEntry<MethodDef>(tok.index());
					if (method->flags & clr::ENTRY<MethodDef>::SpecialName) {
						if (method->name == ".ctor") {
							builder.add_stmt(call);
							break;
						}

						if (method->name.begins("op_")) {
							if (auto i = find_check(unary_operator_names, method->name)) {
								builder.push(new ast::unary_node(i->kind, call->args[0]), stack_type);
								break;
							}

							if (auto i = find_check(binary_operator_names, method->name)) {
								builder.push(new ast::binary_node(i->kind, call->args[0], call->args[1]), stack_type);
								break;
							}

							if (method->name == "op_Implicit") {
								builder.push(call->args[0], stack_type);
								break;
							}

							if (method->name == "op_Explicit") {
								builder.push(new ast::unary_node(ast::cast, call->args[0], func->subtype), stack_type);
								break;
							}
						}

					}
				}
				if (func->subtype) {
					builder.push(call, stack_type);

				} else {
					builder.add_stmt(call);//new ast::unary_node(ast::expression, call));
				}
				break;
			}
			case cil::CALLI: {
				Token					tok		= r.get();
				const C_type			*parent	= 0;
				const C_element			*e		= ctx.GetElement(tok, parent);
				const C_type_function	*func	= (const C_type_function*)e->type;
				ast::call_node			*call	= new ast::call_node(builder.pop());

				if (e->id == ".ctor") {
					ISO_TRACEF("ctor\n");
				}

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype)
					builder.push(call, builder.type_to_stack(func->subtype));
				else
					builder.add_stmt(call);//new ast::unary_node(ast::expression, call));
				break;
			}
			case cil::RET:
				if (const C_type *ret = func->get_rettype())
					builder.add_stmt(new ast::unary_node(ast::retv, builder.pop()));
				else
					builder.add_stmt(new ast::node(ast::ret));
				builder.end(0);
				break;

			case cil::BR_S:			builder.branch(builder.read_branch<int8>(r)); break;
			case cil::BRFALSE_S:	builder.condbranch<int8>(r, builder.pop()->flip_condition()); break;
			case cil::BRTRUE_S:		builder.condbranch<int8>(r, builder.pop()); break;
			case cil::BEQ_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::eq)); break;
			case cil::BGE_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::lt, builder.is_float() ? ast::UNORDERED : 0)->flip_condition()); break;
			case cil::BGT_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::gt)); break;
			case cil::BLE_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::gt, builder.is_float() ? ast::UNORDERED : 0)->flip_condition()); break;
			case cil::BLT_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::lt)); break;
			case cil::BNE_UN_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::eq)->flip_condition()); break;
			case cil::BGE_UN_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::lt, builder.is_float() ? 0 : ast::UNSIGNED)->flip_condition()); break;
			case cil::BGT_UN_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::gt, ast::UNORDERED)); break;
			case cil::BLE_UN_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::gt, builder.is_float() ? 0 : ast::UNSIGNED)->flip_condition()); break;
			case cil::BLT_UN_S:		builder.condbranch<int8>(r, builder.binary_cmp0(ast::lt, ast::UNORDERED)); break;

			case cil::BR:			builder.branch(builder.read_branch<int32>(r)); break;
			case cil::BRFALSE:		builder.condbranch<int32>(r, builder.pop()->flip_condition()); break;
			case cil::BRTRUE:		builder.condbranch<int32>(r, builder.pop()); break;
			case cil::BEQ:			builder.condbranch<int32>(r, builder.binary_cmp0(ast::eq)); break;
			case cil::BGE:			builder.condbranch<int32>(r, builder.binary_cmp0(ast::lt, builder.is_float() ? ast::UNORDERED : 0)->flip_condition()); break;
			case cil::BGT:			builder.condbranch<int32>(r, builder.binary_cmp0(ast::gt)); break;
			case cil::BLE:			builder.condbranch<int32>(r, builder.binary_cmp0(ast::gt, builder.is_float() ? ast::UNORDERED : 0)->flip_condition()); break;
			case cil::BLT:			builder.condbranch<int32>(r, builder.binary_cmp0(ast::lt)); break;
			case cil::BNE_UN:		builder.condbranch<int32>(r, builder.binary_cmp0(ast::eq)->flip_condition()); break;
			case cil::BGE_UN:		builder.condbranch<int32>(r, builder.binary_cmp0(ast::lt, builder.is_float() ? 0 : ast::UNSIGNED)->flip_condition()); break;
			case cil::BGT_UN:		builder.condbranch<int32>(r, builder.binary_cmp0(ast::gt, ast::UNORDERED)); break;
			case cil::BLE_UN:		builder.condbranch<int32>(r, builder.binary_cmp0(ast::gt, builder.is_float() ? 0 : ast::UNSIGNED)->flip_condition()); break;
			case cil::BLT_UN:		builder.condbranch<int32>(r, builder.binary_cmp0(ast::lt, ast::UNORDERED)); break;

			case cil::SWITCHOP: {
				ast::switch_node	*s		= new ast::switch_node(builder.pop());
				uint32				n		= r.get<uint32le>();
				uint32				offset	= builder.offset + n * 4 + 5;
				for (int i = 0; i < n; i++)
					s->targets.push_back(builder.branch_stack(builder.at_offset(offset + r.get<int32le>())));
				s->targets.push_back(builder.pbb[1]);
				builder.add_stmt(s);
				break;
			}
			case cil::LDIND_I1:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int8>(),	flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_U1:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<uint8>(),	flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_I2:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int16>(),	flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_U2:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<uint16>(),	flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_I4:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int32>(),	flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_U4:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<uint32>(),	flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_I8:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int64>(),	flags), Stack::INT64); flags = 0; break;
			case cil::LDIND_I:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int>(),		flags), Stack::INT32); flags = 0; break;
			case cil::LDIND_R4:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<float>(),	flags), Stack::FLOAT); flags = 0; break;
			case cil::LDIND_R8:		builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<double>(),	flags), Stack::FLOAT); flags = 0; break;
			case cil::LDIND_REF:	builder.push(new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<uint8>(),	flags), Stack::PTR); flags = 0; break;

			case cil::STIND_REF:	{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<uint8>(),	flags), p)); flags = 0; break; }
			case cil::STIND_I1:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int8>(),	flags), p)); flags = 0; break; }
			case cil::STIND_I2:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int16>(),	flags), p)); flags = 0; break; }
			case cil::STIND_I4:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int32>(),	flags), p)); flags = 0; break; }
			case cil::STIND_I8:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int64>(),	flags), p)); flags = 0; break; }
			case cil::STIND_R4:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<float>(),	flags), p)); flags = 0; break; }
			case cil::STIND_R8:		{ ast::node *p = builder.pop(); builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<double>(),flags), p)); flags = 0; break; }

			case cil::ADD:			builder.binary(ast::add); break;
			case cil::SUB:			builder.binary(ast::sub); break;
			case cil::MUL:			builder.binary(ast::mul); break;
			case cil::DIV:			builder.binary(ast::div); break;
			case cil::DIV_UN:		builder.binary(ast::div, ast::UNSIGNED); break;
			case cil::REM:			builder.binary(ast::mod); break;
			case cil::REM_UN:		builder.binary(ast::mod, ast::UNSIGNED); break;
			case cil::AND:			builder.binary(ast::bit_and); break;
			case cil::OR:			builder.binary(ast::bit_or); break;
			case cil::XOR:			builder.binary(ast::bit_xor); break;
			case cil::SHL:			builder.binary(ast::shl); break;
			case cil::SHR:			builder.binary(ast::shr); break;
			case cil::SHR_UN:		builder.binary(ast::shr, ast::UNSIGNED); break;
			case cil::NEG:			builder.unary(ast::neg); break;
			case cil::NOT:			builder.unary(ast::bit_not); break;

			case cil::CONV_I1:		builder.cast(C_type_int::get<int8>(), Stack::INT32); break;
			case cil::CONV_I2:		builder.cast(C_type_int::get<int16>(), Stack::INT32); break;
			case cil::CONV_I4:		builder.cast(C_type_int::get<int32>(), Stack::INT32); break;
			case cil::CONV_I8:		builder.cast(C_type_int::get<int64>(), Stack::INT64); break;
			case cil::CONV_R4:		builder.cast(C_type_float::get<float>(), Stack::FLOAT); break;
			case cil::CONV_R8:		builder.cast(C_type_float::get<double>(), Stack::FLOAT); break;
			case cil::CONV_U4:		builder.cast(C_type_int::get<uint32>(), Stack::INT32); break;
			case cil::CONV_U8:		builder.cast(C_type_int::get<uint64>(), Stack::INT64); break;

			case cil::CALLVIRT: {
				Token					tok		= r.get();
				const C_type			*parent	= 0;
				const C_element			*e		= ctx.GetElement(tok, parent);
				const C_type_function	*func	= (const C_type_function*)e->type->skip_template();
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, parent));

				if (e->id == ".ctor") {
					ISO_TRACEF("ctor\n");
				}

				builder.get_args(call, func->args, func->num_args());

				if (func->subtype)
					builder.push(call, builder.type_to_stack(func->subtype));
				else
					builder.add_stmt(call);//new ast::unary_node(ast::expression, call));
				break;
			}
			case cil::CPOBJ: {
				ast::node				*srce	= builder.pop(), *dest = builder.pop();
				builder.add_stmt(new ast::binary_node(ast::copyobj, dest, srce, ctx.GetType(r.get<Token>())));
				break;
			}
			case cil::LDOBJ:
				builder.push(new ast::unary_node(ast::load, builder.pop(), ctx.GetType(r.get<Token>()), flags), Stack::USERVALUE);
				flags = 0;
				break;
			case cil::LDSTR:
				builder.push(new ast::lit_node(ctx.type_string, ctx.GetUserString0(r.get<Token>())), Stack::OBJECT);
				break;
			case cil::NEWOBJ: {
				Token					tok		= r.get();
				const C_type			*parent	= 0;
				const C_element			*e		= ctx.GetElement(tok, parent);
				const C_type_function	*func	= (const C_type_function*)e->type->skip_template();
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, parent), ast::ALLOC);

				builder.get_args(call, func->args + 1, int(func->num_args()) - 1);
				builder.push(call, is_value_type(parent) ? Stack::PTR : Stack::OBJECT);
				break;
			}
			case cil::CASTCLASS:		builder.push(new ast::unary_node(ast::castclass, builder.pop(), ctx.GetType(r.get<Token>())), Stack::OBJECT); break;
			case cil::ISINST:			builder.push(new ast::unary_node(ast::isinst, builder.pop(), ctx.GetType(r.get<Token>())), Stack::OBJECT); break;
			case cil::CONV_R_UN:		builder.cast(C_type_float::get<float>(), Stack::FLOAT); break;
			case cil::UNBOX:			builder.push(new ast::unary_node(ast::castclass, builder.pop(), ctx.GetType(r.get<Token>())), Stack::PTR); break;
			case cil::THROW:			builder.add_stmt(new ast::unary_node(ast::thrw, builder.pop())); break;
			case cil::LDFLD: {
				Token					tok		= r.get();
				auto					*e		= builder.element0(ast::element, tok);
				ast::node				*r		= builder.pop();
				//if (r->kind == ast::ref)
				//	r = ((ast::unary_node*)r)->arg;
				builder.push(new ast::binary_node(ast::field, r, e, flags), builder.type_to_stack(e->element->type));
				flags = 0;
				break;
			}
			case cil::LDFLDA: {
				Token					tok		= r.get();
				builder.push(ctx.TakeAddress(new ast::binary_node(ast::field, builder.pop(), builder.element0(ast::element, tok))), Stack::PTR);
				break;
			}
			case cil::STFLD: {
				Token					tok		= r.get();
				auto					*e		= builder.element0(ast::element, tok);
				ast::node				*val	= fix_type(builder.pop(), e->element->type);
				ast::node				*r		= builder.pop();
				//if (r->kind == ast::ref)
				//	r = ((ast::unary_node*)r)->arg;
				builder.add_stmt(new ast::binary_node(ast::assign, new ast::binary_node(ast::field, r, e, flags), val));
				flags = 0;
				break;
			}
			case cil::LDSFLD: {
				Token					tok		= r.get();
				auto					*e		= builder.element0(ast::element, tok);
				builder.push(e, builder.type_to_stack(e->element->type));
				break;
			}
			case cil::LDSFLDA: {
				Token					tok		= r.get();
				builder.push(ctx.TakeAddress(builder.element0(ast::element, tok)), Stack::PTR);
				break;
			}
			case cil::STSFLD: {
				Token					tok		= r.get();
				builder.add_stmt(new ast::binary_node(ast::assign, builder.element0(ast::element, tok), builder.pop()));
				break;
			}
			case cil::STOBJ: {
				ast::node		*srce	= builder.pop();
				builder.add_stmt(new ast::binary_node(ast::assignobj, builder.pop(), srce, ctx.GetType(r.get<Token>()), flags));
				break;
			}
			case cil::CONV_OVF_I1_UN:	builder.cast(ctx.GetType<int8>(),	Stack::INT32); break;
			case cil::CONV_OVF_I2_UN:	builder.cast(ctx.GetType<int16>(),	Stack::INT32); break;
			case cil::CONV_OVF_I4_UN:	builder.cast(ctx.GetType<int32>(),	Stack::INT32); break;
			case cil::CONV_OVF_I8_UN:	builder.cast(ctx.GetType<int64>(),	Stack::INT64); break;
			case cil::CONV_OVF_U1_UN:	builder.cast(ctx.GetType<uint8>(),	Stack::INT32); break;
			case cil::CONV_OVF_U2_UN:	builder.cast(ctx.GetType<uint16>(),	Stack::INT32); break;
			case cil::CONV_OVF_U4_UN:	builder.cast(ctx.GetType<uint32>(),	Stack::INT32); break;
			case cil::CONV_OVF_U8_UN:	builder.cast(ctx.GetType<uint64>(),	Stack::INT64); break;
			case cil::CONV_OVF_I_UN:	builder.cast(ctx.GetType<int32>(),	Stack::INT32); break;
			case cil::CONV_OVF_U_UN:	builder.cast(ctx.GetType<uint32>(),	Stack::INT32); break;

			case cil::BOX:				builder.push(new ast::unary_node(ast::box, builder.pop(), ctx.GetType(r.get<Token>())), Stack::OBJECT); break;
			case cil::NEWARR:			builder.push(new ast::unary_node(ast::newarray, builder.pop(), ctx.GetType(r.get<Token>())), Stack::OBJECT, Stack::SIDE_EFFECTS); break;

			case cil::LDLEN: {
				const C_element			*e		= &ctx.array_len;
				const C_type_function	*func	= (const C_type_function*)e->type->skip_template();
				ast::call_node			*call	= new ast::call_node(new ast::element_node(ast::element, e, 0));
				builder.get_args(call, func->args, func->num_args());
				builder.push(call, Stack::NATIVE_INT);
				break;
			}

			case cil::LDELEMA:			builder.push(ctx.TakeAddress(builder.ldelem0(ctx.GetType(r.get<Token>()))), Stack::NATIVE_INT); break;
			case cil::LDELEM_I1:		builder.ldelem(ctx.GetType<int8>(),		Stack::INT32); break;
			case cil::LDELEM_U1:		builder.ldelem(ctx.GetType<uint8>(),	Stack::INT32); break;
			case cil::LDELEM_I2:		builder.ldelem(ctx.GetType<int16>(),	Stack::INT32); break;
			case cil::LDELEM_U2:		builder.ldelem(ctx.GetType<uint16>(),	Stack::INT32); break;
			case cil::LDELEM_I4:		builder.ldelem(ctx.GetType<int32>(),	Stack::INT32); break;
			case cil::LDELEM_U4:		builder.ldelem(ctx.GetType<uint32>(),	Stack::INT32); break;
			case cil::LDELEM_I8:		builder.ldelem(ctx.GetType<int64>(),	Stack::INT64); break;
			case cil::LDELEM_I:			builder.ldelem(ctx.GetType<int>(),		Stack::INT32); break;
			case cil::LDELEM_R4:		builder.ldelem(ctx.GetType<float>(),	Stack::FLOAT); break;
			case cil::LDELEM_R8:		builder.ldelem(ctx.GetType<double>(),	Stack::FLOAT); break;
			case cil::LDELEM_REF:		builder.ldelem(ctx.GetType<void*>(),	Stack::PTR); break;

			case cil::STELEM_I:			builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<int>()),		builder.pop())); break;
			case cil::STELEM_I1:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<int8>()),	builder.pop())); break;
			case cil::STELEM_I2:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<int16>()),	builder.pop())); break;
			case cil::STELEM_I4:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<int32>()),	builder.pop())); break;
			case cil::STELEM_I8:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<int64>()),	builder.pop())); break;
			case cil::STELEM_R4:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<float>()),	builder.pop())); break;
			case cil::STELEM_R8:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<double>()),	builder.pop())); break;
			case cil::STELEM_REF:		builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType<void*>()),	builder.pop())); break;
			case cil::LDELEM:			builder.ldelem(ctx.GetType(r.get<Token>()), Stack::INT32); break;
			case cil::STELEM:			builder.add_stmt(new ast::binary_node(ast::assign, builder.ldelem0(ctx.GetType(r.get<Token>())), builder.pop())); break;

			case cil::UNBOX_ANY:		builder.push(new ast::unary_node(ast::load, builder.pop(), ctx.GetType(r.get<Token>()), flags), Stack::INT32); flags = 0; break;

			case cil::CONV_OVF_I1:		builder.cast(C_type_int::get<int8>(),	Stack::INT32); break;
			case cil::CONV_OVF_U1:		builder.cast(C_type_int::get<int16>(),	Stack::INT32); break;
			case cil::CONV_OVF_I2:		builder.cast(C_type_int::get<int32>(),	Stack::INT32); break;
			case cil::CONV_OVF_U2:		builder.cast(C_type_int::get<int64>(),	Stack::INT64); break;
			case cil::CONV_OVF_I4:		builder.cast(C_type_int::get<uint8>(),	Stack::INT32); break;
			case cil::CONV_OVF_U4:		builder.cast(C_type_int::get<uint16>(),	Stack::INT32); break;
			case cil::CONV_OVF_I8:		builder.cast(C_type_int::get<uint32>(),	Stack::INT32); break;
			case cil::CONV_OVF_U8:		builder.cast(C_type_int::get<uint64>(),	Stack::INT64); break;

			case cil::REFANYVAL:		builder.push(new ast::unary_node(ast::refanyval, builder.pop()), Stack::PTR); break;
			case cil::CKFINITE:			builder.unary(ast::ckfinite); break;
			case cil::MKREFANY:			builder.push(new ast::unary_node(ast::mkrefany, builder.pop(), ctx.GetType(r.get<Token>())), Stack::OBJECT); break;
//			case cil::LDTOKEN:			builder.push(new tok_node(ast::tok, r.get()), Stack::NATIVE_INT); break;
			case cil::LDTOKEN:			builder.lit(r.get<uint32>(), Stack::INT32); break;

			case cil::CONV_U2:			builder.cast(C_type_int::get<uint16>(),	Stack::INT32); break;
			case cil::CONV_U1:			builder.cast(C_type_int::get<uint8>(),	Stack::INT32); break;
			case cil::CONV_I:			builder.cast(C_type_int::get<int32>(),	Stack::INT32); break;
			case cil::CONV_OVF_I:		builder.cast(C_type_int::get<int32>(),	Stack::INT32); break;
			case cil::CONV_OVF_U:		builder.cast(C_type_int::get<uint32>(),	Stack::INT32); break;

			case cil::ADD_OVF:			builder.binary(ast::add, ast::OVERFLOW); break;
			case cil::ADD_OVF_UN:		builder.binary(ast::add, ast::OVERFLOW | ast::UNSIGNED); break;
			case cil::MUL_OVF:			builder.binary(ast::mul, ast::OVERFLOW); break;
			case cil::MUL_OVF_UN:		builder.binary(ast::mul, ast::OVERFLOW | ast::UNSIGNED); break;
			case cil::SUB_OVF:			builder.binary(ast::sub, ast::OVERFLOW); break;
			case cil::SUB_OVF_UN:		builder.binary(ast::sub, ast::OVERFLOW | ast::UNSIGNED); break;

			case cil::ENDFINALLY:		builder.add_stmt(new ast::node(ast::end));		break;
			case cil::LEAVE:			builder.branch(builder.read_branch<int32>(r));	break;
			case cil::LEAVE_S:			builder.branch(builder.read_branch<int8>(r));	break;

			case cil::STIND_I: {
				ast::node *p = builder.pop();
				builder.add_stmt(new ast::binary_node(ast::assign, new ast::unary_node(ast::deref, builder.pop(), ctx.GetType<int>(), flags), p));
				flags = 0;
				break;
			}
			case cil::CONV_U:			builder.cast(C_type_int::get<uint32>(), Stack::INT32); break;

			case cil::PREFIX1:
				switch (r.getc()) {
					case cil::ARGLIST:		builder.push(new ast::node(ast::vararg), Stack::NATIVE_INT); break;
					case cil::CEQ:			builder.binary_cmp(ast::eq); break;
					case cil::CGT:			builder.binary_cmp(ast::gt); break;
					case cil::CGT_UN:		builder.binary_cmp(ast::gt, ast::UNORDERED); break;
					case cil::CLT:			builder.binary(ast::lt); break;
					case cil::CLT_UN:		builder.binary_cmp(ast::lt, ast::UNORDERED); break;

					case cil::LDFTN:		builder.push(builder.element0(ast::func, r.get()), Stack::NATIVE_INT); break;
					case cil::LDVIRTFTN:	builder.push(builder.element0(ast::vfunc, r.get()), Stack::NATIVE_INT); break;

					case cil::LDARG:		builder.var(func->get_arg(r.get<uint16>())); break;
					case cil::LDARGA:		builder.push(ctx.TakeAddress(builder.var0(func->get_arg(r.get<uint16>()))), Stack::PTR); break;
					case cil::STARG:		builder.assign(func->get_arg(r.get<uint16>())); break;
					case cil::LDLOC:		builder.var(builder.locals[r.get<uint16>()]); break;
					case cil::LDLOCA:		builder.push(ctx.TakeAddress(builder.var0(builder.locals[r.get<uint16>()])), Stack::PTR); break;
					case cil::STLOC:		builder.assign(builder.locals[r.get<uint16>()]); break;
					case cil::LOCALLOC:		builder.push(new ast::unary_node(ast::alloc_stack, builder.pop()), Stack::NATIVE_INT); break;
					case cil::ENDFILTER:	break;
					case cil::UNALIGNED_:	flags |= r.getc(); break;
					case cil::VOLATILE_:	flags |= ast::VOLATILE; break;
					case cil::TAIL_:		break;
					case cil::INITOBJ:		builder.add_stmt(new ast::unary_node(ast::initobj, builder.pop(), ctx.GetType(r.get<Token>()))); break;
					case cil::CONSTRAINED_:	r.get<Token>(); break;
					case cil::CPBLK: {
						ast::node		*size	= builder.pop(), *srce = builder.pop(), *dest = builder.pop();
						ast::call_node	*call	= new ast::call_node(new ast::name_node("copymem"), flags);
						call->add(dest);
						call->add(srce);
						call->add(size);
						flags = 0;
						break;
					}
					case cil::INITBLK: {
						ast::node		*size	= builder.pop(), *value = builder.pop(), *dest = builder.pop();
						ast::call_node	*call	= new ast::call_node(new ast::name_node("initmem"), flags);
						call->add(dest);
						call->add(value);
						call->add(size);
						flags = 0;
						break;
					}
					case cil::NO_:			r.getc(); break;
					case cil::RETHROW:		builder.add_stmt(new ast::node(ast::rethrw)); break;
					case cil::SIZEOF:		builder.push(new ast::node(ast::get_size, 0, ctx.GetType(r.get<Token>())), Stack::NATIVE_INT); break;
					case cil::REFANYTYPE:	builder.push(new ast::unary_node(ast::get_type, builder.pop()), Stack::NATIVE_INT); break;
					case cil::READONLY_:	flags |= ast::READONLY; break;
				}
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

	builder.count_local_use(dt.info_root);
	builder.remove_redundant_locals();

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
						if (Builder::local_var **lv = find_check(builder.locals, var)) {
							//linked_var_node	*lvn = (linked_var_node*)get(((ast::binary_node*)get(init))->left);
							//lvn->bb = header;
							bool	used_outside = false;
							for (auto &u : (*lv)->uses) {
								if (used_outside = !loop.is_inside(dt, u.b))
									break;
							}

							if (!used_outside) {
								((ast::binary_node*)get(init))->left = new ast::decl_node(var);
								builder.locals.erase(lv);
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
		for_each(i->stmts, [&i, &dt](ref_ptr<ast::node> &r) {
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
	builder.count_local_use(dt.info_root);
	builder.remove_redundant_locals();

	for (auto &i : builder.blocks) {
		i->apply([](ref_ptr<ast::node> &r) {
			if (r->kind == ast::query) {

				//optimise ternary:
				ast::binary_node *q		= r->cast();
				ast::binary_node *v		= q->right->cast();
				bool3			a		= is_bool_lit(v->left);
				bool3			b		= is_bool_lit(v->right);
				switch (a) {
					case true3:
						// a ? true:false  ->  a
						// a ? true : b    ->  a || b
						r = b == false3 ? q->left : new ast::binary_node(ast::log_or, q->left, v->right);
						break;
					case false3:
						// a ? false:true  ->  !a
						// a ? false : b   ->  !a && b
						r = b == true3 ? q->left->flip_condition() : new ast::binary_node(ast::log_and, q->left->flip_condition(), v->right);
						break;
					default:
						switch (b) {
							case true3:
								// a ? b : true    ->  !a || b
								r = new ast::binary_node(ast::log_or, q->left->flip_condition(), v->right);
								break;
							case false3:
								// a ? b : false   ->  a && b
								r = new ast::binary_node(ast::log_and, q->left, v->right);
								break;
						}
						break;
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
		for (auto &i : builder.locals) {
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

void Decompile(clr::Context &ctx, Dumper &dumper, string_accum &a, const ENTRY<MethodDef> *method) {
	const C_type		*func	= ctx.ReadSignature(method->signature, dumper.scope, ctx.meta->GetEntry(method->paramlist), function_flags(*method));
	DumpType(a, func, dumper.scope ? dumper.scope->id + "::" + special_name(*method, (C_type_function*)func, dumper.scope->id) : method->name, dumper.depth, true);

	if (method->IsCIL() && method->code.data) {
		C_arg				*arg	= new C_arg(method->name, func);
		ast::funcdecl_node	*decl	= new ast::funcdecl_node(arg, dumper.scope);
		auto				p		= MakeAST(ctx, decl, method->code.data);
		dumper.scoped(a << ' '), dumper.DumpCPP(a << ' ', p);
	}
}

void Decompile(clr::Context &ctx, Dumper &dumper, string_accum &a, const ENTRY<TypeDef> *type) {
	const C_type	*ctype = ctx.GetType(type);
	DumpType(a, ctype, type->name, 0, true) << ";\n";

	for (auto &i : ctx.meta->GetEntries(type, type->methods)) {
		ISO_TRACEF("Decompile ") << i.name << '\n';
		dumper.newline(a);
		dumper.newline(a);
		Decompile(ctx, dumper, a, &i);
	}
}

void DecompileCS(clr::Context &ctx, Dumper &dumper, string_accum &a, const ENTRY<TypeDef> *type) {
	dumper.newline(a);
	const C_type_composite	*s = dumper.scope;

	a << ifelse(is_value_type(s), "struct", "class") << ' ' << s->id;

	auto	*i	= s->elements.begin(), *e = s->elements.end();

	// bases classes

	const char	*sep	= " : ";
	while (i != e && !i->id) {
		DumpTypeCS(a << sep, i++->type);
		sep = ", ";
	}

	{
		static const char *access_names[] = {"public", "protected", "private"};
		auto	scope = dumper.scoped(a << ' ');

		// fields

		while (i != e) {
			if (i->type->type != C_type::FUNCTION) {
				dumper.newline(a) << access_names[i->access] << ' ';
				DumpTypeCS(a, i->type) << ' ' << i->id << ';';
			}
			++i;
		}

		// properties e.g.	public static Color red { get { return new Color(1, 0, 0, 1); } }

		range<clr::ENTRY<clr::Property>*>	props;
		for (auto &i : ctx.meta->GetTable<PropertyMap>()) {
			if (ctx.meta->GetEntry(i.parent) == type) {
				props = ctx.meta->GetEntries(&i, i.property_list);
				break;
			}
		}

		hash_set<Indexed<MethodDef>>	done;

		for (auto &p : props) {
			Token		itok		= ctx.meta->GetIndexed(&p);
			ACCESS		access;
			bool		external	= true;
			C_type_function	*f		= nullptr;

			for (auto &i : ctx.meta->GetTable<MethodSemantics>()) {
				if (i.association == itok) {
					auto	method	= ctx.meta->GetEntry(i.method);
					access		= method->access();
					external	= !method->IsCIL() || !method->code.data;
					f			= (C_type_function*)ctx.ReadSignature(method->signature, s, ctx.meta->GetEntry(method->paramlist), function_flags(*method));
					break;
				}   
			}

			if (!f)
				continue;

			dumper.newline(a) << onlyif(external, "extern ") << access_names[get_access(access)] << ' ' << onlyif(!(f->flags & C_type_function::HASTHIS), "static ");

			auto	scope = dumper.scoped(DumpTypeCS(a, f->subtype) << ' ' << p.name << ' ');
			for (auto &i : ctx.meta->GetTable<MethodSemantics>()) {
				if (i.association == itok) {
					auto	method	= ctx.meta->GetEntry(i.method);
					if (i.flags & i.Getter) {
						dumper.newline(a) << "get";
					} else if (i.flags & i.Setter) {
						dumper.newline(a) << "set";
					}
					done.insert(i.method);

					if (method->IsCIL() && method->code.data) {
						auto				func	= ctx.ReadSignature(method->signature, s, ctx.meta->GetEntry(method->paramlist), function_flags(*method));
						C_arg				*arg	= new C_arg(method->name, func);
						ast::funcdecl_node	*decl	= new ast::funcdecl_node(arg, s);
						auto				ast		= MakeAST(ctx, decl, method->code.data);
						dumper.named.clear();
						dumper.scoped(a << ' '), dumper.DumpCS(a << ' ', ast);
					} else {
						a << ';';
					}
				}   
			}
		}

		// methods

		for (auto &i : ctx.meta->GetEntries(type, type->methods)) {
			if (done.check(ctx.meta->GetIndexed(&i)))
				continue;

			ISO_TRACEF("Decompile ") << i.name << '\n';
			auto	func	= ctx.ReadSignature(i.signature, s, ctx.meta->GetEntry(i.paramlist), function_flags(i));
			auto	f		= (C_type_function*)func;

			dumper.newline(a) << access_names[get_access(i.access())] << ' ';
				
			bool	external	= !i.IsCIL() || !i.code.data;

			a	<< onlyif(!(f->flags & C_type_function::HASTHIS), "static ")
				<< onlyif(external, "extern ");

			if (i.flags & clr::ENTRY<MethodDef>::SpecialName && i.name == "op_Implicit") {
				DumpTypeCS(a << "implicit operator ", f->subtype);

			} else {
				if (!(f->flags & C_type_function::NORETURN))
					DumpTypeCS(a, f->subtype);

				a << ' ' << special_name(i, f, s->id);
			}

			a << '(';
			for (auto *i = f->args.begin() + int(f->has_this()), *e = f->args.end(); i != e; ++i) {
				DumpTypeCS(a, i->type) << ' ' << i->id;
				if (i != e - 1)
					a << ", ";
			}
			a << ')';

			if (external) {
				a << ';';
			} else {
				C_arg				*arg	= new C_arg(i.name, func);
				ast::funcdecl_node	*decl	= new ast::funcdecl_node(arg, s);
				auto				p		= MakeAST(ctx, decl, i.code.data);
				dumper.named.clear();
				dumper.scoped(a << ' '), dumper.DumpCS(a << ' ', p);
			}
		}
	}
}

} // namespace clr

string Decompile(const clr::ENTRY<clr::MethodDef> *method, clr::METADATA* meta) {
	string_builder		a;
	clr::Context		ctx(meta);
	auto				type = meta->GetOwner(method, &clr::ENTRY<clr::TypeDef>::methods);
	Dumper				dumper(ctx.GetType(type));
	clr::Decompile(ctx, dumper, a, method);
	return a;
}

string Decompile(const clr::ENTRY<clr::TypeDef> *type, clr::METADATA* meta) {
	string_builder		a;
	clr::Context		ctx(meta);
	Dumper				dumper(ctx.GetType(type));
	a << "namespace " << type->namespce << " {\n";
	clr::Decompile(ctx, dumper, a, type);
	a << "\n\n} // namespace " << type->namespce;
	return a;
}

string DecompileCS(const clr::ENTRY<clr::TypeDef> *type, clr::METADATA* meta) {
	string_builder		a;
	clr::Context		ctx(meta);
	const C_type		*ctype = ctx.GetType(type);
	if (ctype->type == C_type::STRUCT) {
		Dumper				dumper(ctype);
		dumper.scoped(a << "namespace " << type->namespce << ' '), clr::DecompileCS(ctx, dumper, a, type);
	}
	return a;
}

string DecompileCS(clr::METADATA* meta) {
	string_builder		a;
	clr::Context		ctx(meta);
	Dumper				dumper(0);

	for (auto &i : meta->GetTable<clr::TypeDef>()) {
		bool	nested = false;
		for (auto &j : meta->GetTable<clr::NestedClass>()) {
			if (nested = j.nested_class == meta->GetIndex(&i))
				break;
		}
		if (!nested) {
			ISO_TRACEF("** Decompile Type ") << i.name << " **\n";
			const C_type		*ctype = ctx.GetType(&i);
			if (ctype->type == C_type::STRUCT) {
				dumper.SetScope(ctype);
				dumper.scoped(a << "namespace " << i.namespce << ' '), clr::DecompileCS(ctx, dumper, a, &i);
			}
		}
	}

	return a;
}

string Decompile(clr::METADATA* meta) {
	string_builder		a;
	clr::Context		ctx(meta);
	Dumper				dumper(0);

	for (auto &i : meta->GetTable<clr::TypeDef>()) {
		bool	nested = false;
		for (auto &j : meta->GetTable<clr::NestedClass>()) {
			if (nested = j.nested_class == meta->GetIndex(&i))
				break;
		}
		if (!nested) {
			ISO_TRACEF("** Decompile Type ") << i.name << " **\n";
			dumper.SetScope(ctx.GetType(&i));
			clr::Decompile(ctx, dumper, a, &i);
		}
	}

	return a;
}


