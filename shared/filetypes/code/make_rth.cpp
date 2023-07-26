#include "pe.h"
#include "clr.h"
#include "base/array.h"
#include "base/list.h"
#include "base/hash.h"
#include "base/algorithm.h"
#include "hashes/fnv.h"
#include "maths/graph.h"
#include "stream.h"

#define TRAP(X) ISO_ASSERT(def->name != #X);

using namespace iso;
using namespace clr;

namespace clr {
auto hash(const Token &t) { return t.i * 9; }
}

constexpr ELEMENT_TYPE operator|(ELEMENT_TYPE a, ELEMENT_TYPE b)	{ return ELEMENT_TYPE(int(a) | int(b)); }
constexpr ELEMENT_TYPE& operator|=(ELEMENT_TYPE &a, ELEMENT_TYPE b)	{ return a = a | b; }

static const ELEMENT_TYPE ELEMENT_TYPE_DELEGATE = ELEMENT_TYPE(0x7f);
static const ELEMENT_TYPE ELEMENT_TYPE_REF		= ELEMENT_TYPE(0x80);
static const ELEMENT_TYPE ELEMENT_TYPE_REF1		= ELEMENT_TYPE(0x100);
static const ELEMENT_TYPE ELEMENT_TYPE_REF2		= ELEMENT_TYPE_REF | ELEMENT_TYPE_REF1;


template<typename A, typename B> uint64 namespce_name(const A &namespce, const B &name) {
	hash_stream<FNV<64>>	fnv;
	fnv.write(namespce);
	fnv.putc('.');
	fnv.write(name);
	return fnv;
}
template<typename A> uint64 namespce_name(const A &qual) {
	hash_stream<FNV<64>>	fnv;
	fnv.write(qual);
	return fnv;
}

ELEMENT_TYPE ReadTypeToken(byte_reader &r, Token &tok) {
	for (;;) {
		ELEMENT_TYPE	t = ELEMENT_TYPE(r.getc());
		switch (t) {
			case ELEMENT_TYPE_CLASS:
			case ELEMENT_TYPE_VALUETYPE:
				tok = TypeDefOrRef(CompressedUInt(r));
				return t;

			case ELEMENT_TYPE_GENERICINST: {
				auto	t	= ELEMENT_TYPE(r.getc());
				tok			= TypeDefOrRef(CompressedUInt(r));
				uint32	n	= CompressedUInt(r);
				Token	tok2;
				for (uint32 i = 0; i < n; ++i)
					ReadTypeToken(r, tok2);
				return t;
			}

			case ELEMENT_TYPE_ARRAY: {
				ReadTypeToken(r, tok);
				CompressedUInt	rank(r);
				CompressedUInt	num_sizes(r);

				for (uint32 i = num_sizes; i--;)
					(void)CompressedUInt(r);

				CompressedUInt	num_lobounds(r);
				for (uint32 i = num_lobounds; i--;)
					(void)CompressedInt(r);
				return t;
			}

			case ELEMENT_TYPE_SZARRAY:
				ReadTypeToken(r, tok);
				return t;

			case ELEMENT_TYPE_BYREF:
				return ReadTypeToken(r, tok) | ELEMENT_TYPE_REF;

			case ELEMENT_TYPE_CMOD_REQD:
			case ELEMENT_TYPE_CMOD_OPT:
				(void)CompressedUInt(r);
				// fall through
			case ELEMENT_TYPE_SYSTEMTYPE:
			case ELEMENT_TYPE_PINNED:
			case ELEMENT_TYPE_FNPTR:
			case ELEMENT_TYPE_PTR:
			case ELEMENT_TYPE_SENTINEL:
			case ELEMENT_TYPE_BOXED:
			case ELEMENT_TYPE_FIELD:
			case ELEMENT_TYPE_PROPERTY:
			case ELEMENT_TYPE_ENUM:
				break;

			case ELEMENT_TYPE_MVAR:
			case ELEMENT_TYPE_VAR: {
				uint32	n = CompressedUInt(r);
				return t;
			}

			default:
				return t;
		}
	}
}

const char *get_element_name(ELEMENT_TYPE type) {
	switch (type) {
		case ELEMENT_TYPE_BOOLEAN:		return "bool";
		case ELEMENT_TYPE_CHAR:			return "char";
		case ELEMENT_TYPE_I1:			return "signed char";
		case ELEMENT_TYPE_U1:			return "unsigned char";
		case ELEMENT_TYPE_I2:			return "short";
		case ELEMENT_TYPE_U2:			return "unsigned short";
		case ELEMENT_TYPE_R4:			return "float";
		case ELEMENT_TYPE_I:
		case ELEMENT_TYPE_I4:			return "int";
		case ELEMENT_TYPE_U:
		case ELEMENT_TYPE_U4:			return "unsigned";
		case ELEMENT_TYPE_I8:			return "__int64";
		case ELEMENT_TYPE_U8:			return "unsigned __int64";
		case ELEMENT_TYPE_R8:			return "double";
		case ELEMENT_TYPE_STRING:		return "HSTRING";
		default:						return 0;
	}
}

const ENTRY<MethodDef> *find_method(const METADATA &md, const ENTRY<TypeDef> *def, const char *name) {
	if (def) {
		for (auto &m : md.GetEntries(def, def->methods)) {
			if (m.name == name)
				return &m;
		}
	}
	return 0;
}


//-------------------------------------
//	function params
//-------------------------------------

struct param {
	enum MODE {
		MOVE,
		FROM_ADAPTED,
		TO_ADAPTED,
		RETURN,
	};
	friend MODE operator|(MODE a, MODE b) { return MODE(int(a) | int(b)); }

	ELEMENT_TYPE	element;
	Token			tok;
	const uint8		*sig;
	string			name;

	string_accum&	pass(string_accum &sa, MODE mode) const;
	bool			needs_adjust(MODE mode) const;
	bool			is_generic() const { return sig[0] == ELEMENT_TYPE_GENERICINST; }
};

// determine if return needs adjusting
bool param::needs_adjust(MODE mode) const {
	switch (element) {
		case ELEMENT_TYPE_OBJECT | ELEMENT_TYPE_REF2:
		case ELEMENT_TYPE_STRING | ELEMENT_TYPE_REF2:
			return mode == (RETURN | FROM_ADAPTED);
		case ELEMENT_TYPE_CLASS | ELEMENT_TYPE_REF2:
			return mode == (RETURN | FROM_ADAPTED) || is_generic();

		case ELEMENT_TYPE_SZARRAY | ELEMENT_TYPE_REF2: {
			byte_reader	r(sig);
			while (r.getc() != ELEMENT_TYPE_SZARRAY);
			Token			tok;
			switch (ReadTypeToken(r, tok)) {
				case ELEMENT_TYPE_OBJECT:
				case ELEMENT_TYPE_CLASS:
				case ELEMENT_TYPE_STRING:
					return true;
			}
			break;
		}
	}
	return false;
}

string_accum &param::pass(string_accum &sa, MODE mode) const {
	switch (mode) {
		case MOVE:
			switch (element) {
				case ELEMENT_TYPE_SZARRAY:		return sa << name;
//				case ELEMENT_TYPE_STRING:		return sa << "move(" << name << ")";
				case ELEMENT_TYPE_OBJECT:		return sa << "move(" << name << ")";
				case ELEMENT_TYPE_DELEGATE:		return sa << "move(" << name << ")";
			}
			// fall through
		case FROM_ADAPTED:
			switch (element) {
				case ELEMENT_TYPE_SZARRAY | ELEMENT_TYPE_REF2:	return sa << '&' << name << ".size, &" << name << ".p";
				case ELEMENT_TYPE_SZARRAY | ELEMENT_TYPE_REF:	return sa << '&' << name << "->size, &" << name << "->p";
				case ELEMENT_TYPE_SZARRAY:						return sa << name << ".size, to_abi(" << name << ".p)";
				default:
					if ((element & ELEMENT_TYPE_REF2) != ELEMENT_TYPE_REF2 && is_generic())
						return sa << "to_abi(" << name << ')';
					if (element & ELEMENT_TYPE_REF1)
						sa << '&';
					break;
			}
		case TO_ADAPTED:
			switch (element) {
				case ELEMENT_TYPE_SZARRAY | ELEMENT_TYPE_REF2:
				case ELEMENT_TYPE_SZARRAY:		return sa << "{from_abi(" << name << "), " << name << "Size}";
				default:
					break;
			}
	}
	return sa << name;
}

string_accum& pass_params(string_accum &sa, const dynamic_array<param> &params, param::MODE mode) {
	sa << '(';
	for (int i = 0, j = 0, n = params.size32(); i < n; i++) {
		auto	&p = params[wrap(i + 1, n)];
		if (p.element != ELEMENT_TYPE_VOID)
			p.pass(sa << onlyif(j++, ", "), mode);
	}
	return sa << ')';
}

string_accum& pass_params(string_accum &sa, const dynamic_array<param> &params, param *ret_param, param::MODE mode) {
	sa << '(';
	int	j = 0;
	for (auto &i : params) {
		if (&i != ret_param && i.element != ELEMENT_TYPE_VOID)
			i.pass(sa << onlyif(j++, ", "), mode);
	}
	return sa << ')';
}

string unique_param(range<const ENTRY<Param>*> &params, int i) {
	const ENTRY<Param>* p = 0;

	for (auto &j : params) {
		if (j.sequence >= i) {
			if (j.sequence == i)
				p = &j;
			break;
		}
	}

	if (!p) {
		if (i == 0)
			return "ret";
		ISO_ASSERT(0);
	}

	int	add = 0;
	for (auto &j : params) {
		if (j.sequence >= i)
			break;
		if (j.name == p->name)
			++add;
	}

	if (add)
		return p->name + to_string(add);
	return p->name;
}

dynamic_array<param> get_params(const METADATA &md, const ENTRY<MethodDef> &method) {
	byte_reader		r(method.signature);
	SIGNATURE		sig_flags	= SIGNATURE(r.getc());
	uint32			param_count	= CompressedUInt(r);
	auto			params		= md.GetEntries(&method, method.paramlist);

	dynamic_array<param>	result(param_count + 1);

	for (auto &i : params)
		ISO_ASSERT(!(i.flags & i.HasDefault));

	for (uint32 i = 0; i < param_count + 1; ++i) {
		result[i].sig		= r.p;
		result[i].element	= ReadTypeToken(r, result[i].tok);
		result[i].name		= unique_param(params, i);
	}

	return result;
}

bool params_match(const ENTRY<MethodDef> &a, const ENTRY<MethodDef> &b) {
	auto	&sa = a.signature;
	auto	&sb = b.signature;

	return *(sa + 1) == *(sb + 1);
}

int inherit_match(const METADATA &md, const ENTRY<TypeDef> *def, const ENTRY<TypeDef> *base) {
	if (!base)
		return 0;

	auto	def_methods		= md.GetEntries(def, def->methods);
	auto	base_methods	= md.GetEntries(base, base->methods);

	auto	*defi	= def_methods.begin(), *defe = def_methods.end();
	int		matches	= 0;

	for (auto &basem : md.GetEntries(base, base->methods)) {
		if (!(basem.flags & basem.Virtual))
			continue;

		while (defi < defe && !(defi->flags & defi->Virtual))
			++defi;

		if (defi == defe)
			break;

		if (defi->name != basem.name)
			return 0;

		++matches;
		++defi;
	}

	return matches;
}

const ENTRY<TypeDef> *find_exclusive(const METADATA &md, const dynamic_array<const ENTRY<TypeDef>*> &exclusives, const ENTRY<MethodDef> *&method) {
	for (auto x : exclusives) {
		for (auto &m : md.GetEntries(x, x->methods)) {
			if (m.name == method->name && params_match(m, *method)) {
				method = &m;
				return x;
			}
		}
	}
	return 0;
}

const bool has_statics(const METADATA &md, const ENTRY<TypeDef> *def) {
	for (auto &method : md.GetEntries(def, def->methods)) {
		if (method.flags & method.Static)
			return true;
	}
	return false;
}

template<typename S> string fix_method_name(const S &name) {
	if (name == "Release")
		return "release";
	return name;
}

string	get_adapted_method_name(const ENTRY<MethodDef> &method) {
	if (method.name.begins("put_") || method.name.begins("get_") || method.name.begins("add_"))
		return fix_method_name(method.name.slice(4));

	if (method.name.begins("remove_"))
		return fix_method_name(method.name.slice(7));

	return method.name;
}

//-------------------------------------
//	template params
//-------------------------------------

typedef	dynamic_array<const ENTRY<GenericParam>*>	tmpl_params_t;

tmpl_params_t get_tmpl_params(const METADATA &md, const ENTRY<TypeDef> *def) {
	tmpl_params_t	result;

	if (const char *t = str(def->name).find('`')) {
		int	n = from_string<int>(t + 1);
		result.resize(n);
		for (auto &i : md.GetTable<GenericParam>()) {
			if (i.owner.type() == TypeDef && md.GetEntry<TypeDef>(i.owner.index()) == def)
				result[i.number] = &i;
		}
	}
	return result;
}

auto template_decl_additional(const tmpl_params_t &params, int i = 1) {
	return [=](string_accum &sa) {
		int	j = i;
		if (params) {
			for (auto i : params)
				sa << onlyif(j++, ", ") << "typename " << i->name;
		}
	};
}

auto template_decl(const tmpl_params_t &params) {
	return [=](string_accum &sa) {
		if (params) {
			sa << "template<";
			int	j = 0;
			for (auto i : params)
				sa << onlyif(j++, ", ") << "typename " << i->name;
			sa << "> ";
		}
	};
}

auto template_params_additional(const tmpl_params_t &params, int i = 1) {
	return [=](string_accum &sa) {
		int	j = i;
		if (params) {
			for (auto i : params)
				sa << onlyif(j++, ", ") << i->name;
		}
	};
}

auto template_params(const tmpl_params_t &params) {
	return [=](string_accum &sa) {
		if (params) {
			sa << '<';
			int	j = 0;
			for (auto i : params)
				sa << onlyif(j++, ", ") << i->name;
			sa << "> ";
		}
	};
}

auto guid_declspec(const GUID *guid) {
	return [=](string_accum &sa) {
		if (guid)
			sa << "__declspec(uuid(\"" << *guid << "\")) ";
	};
}

//----------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------

struct Attribute {
	const ENTRY<CustomAttribute>	*i;
	Attribute(const ENTRY<CustomAttribute> *_i) : i(_i) {}
	const ENTRY<MemberRef>	*Method(const METADATA &md)	const { return md.GetEntry<MemberRef>(i->type.index()); }
	const char				*Type(const METADATA &md)	const { return md.GetEntry<TypeRef>(Method(md)->clss.index())->name; }
	const ENTRY<TypeDef>	*Parent(const METADATA &md)	const { return md.GetEntry<TypeDef>(i->parent.index()); }
	const_memory_block		Data()						const { return i->value + 2; }

	bool Is(const METADATA &md, const char *type) const {
		if (i->type.type() != MemberRef)
			return false;

		auto	*method = md.GetEntry<MemberRef>(i->type.index());
		if (method->clss.type() != TypeRef)
			return false;

		return md.GetEntry<TypeRef>(method->clss.index())->name == type;
	}
};

struct Context {
	const METADATA &md;

	enum CATEGORY {
		CAT_UNKNOWN,
		CAT_INTERFACE,
		CAT_CLASS,
		CAT_VALUE,
		CAT_ENUM,
		CAT_DELEGATE,
	};

	hash_map<uint64, const ENTRY<TypeDef>*>					name2def;
	hash_map<const ENTRY<TypeRef>*, const ENTRY<TypeDef>*>	ref2def;
	hash_map<Token, dynamic_array<Attribute>>			attributes;

	hash_set_with_key<const ENTRY<TypeDef>*>	reffed;
	hash_set_with_key<const char *>				needed0;	// just forward defs (and enums) needed
	hash_set_with_key<const char *>				needed;		// full include needed

	explicit Context(const METADATA &_md) : md(_md) {
		for (auto &i : md.GetTable<TypeDef>())
			name2def[namespce_name(i.namespce, i.name)] = &i;

		for (auto &i : md.GetTable<TypeRef>())
			ref2def[&i] = name2def[namespce_name(i.namespce, i.name)];

		for (auto &i : md.GetTable<CustomAttribute>())
			attributes[i.parent]->push_back(&i);
	}

	count_string	fix_name(const char *name) const {
		if (const char *t = str(name).find('`'))
			return  str(name).slice_to(t);
		return count_string(name);
	}

	template<typename T> count_string get_name(const T *t) const {
		return t ? fix_name(t->name) : count_string();
	}

	count_string get_name(const Token &tok) const {
		switch (tok.type()) {
			case TypeRef:
				return get_name(md.GetEntry<TypeRef>(tok.index()));
			case TypeDef:
				return get_name(md.GetEntry<TypeDef>(tok.index()));
			default:
				return count_string();
		}
	}

	template<typename T> static string qualified_name(const T *t) {
		if (t)
			return t->namespce + "." + t->name;
		return 0;
	}

	string qualified_name(const Token &tok) const {
		switch (tok.type()) {
			case TypeRef:	return qualified_name(md.GetEntry<TypeRef>(tok.index()));
			case TypeDef:	return qualified_name(md.GetEntry<TypeDef>(tok.index()));
			default:		return 0;
		}
	}

	const_memory_block get_attribute(Token tok, const char *type) const {
		for (auto &i : attributes[tok].or_default()) {
			if (i.Is(md, type))
				return i.Data();
		}
		return empty;
	}
	template<clr::TABLETYPE I> const_memory_block get_attribute(const clr::ENTRY<I> *e, const char *type) const {
		return get_attribute(md.GetIndexed(e), type);
	}

	CATEGORY	base_category(const char *namespce, const char *name) {
		if (namespce == cstr("System") || namespce == cstr("Platform")) {
			if (name == cstr("Enum"))
				return CAT_ENUM;

			if (name == cstr("ValueType"))
				return CAT_VALUE;

			if (name == cstr("MulticastDelegate"))
				return CAT_DELEGATE;
		}
		return CAT_CLASS;
	}
	CATEGORY	get_category(const ENTRY<TypeDef> *def) {
		if (def) {
			if (def->flags & def->Interface)
				return CAT_INTERFACE;

			Token base = def->extends;
			switch (base.type()) {
				case TypeRef:
					if (auto *type = md.GetEntry<TypeRef>(base.index()))
						return base_category(type->namespce, type->name);
					break;
				case TypeDef:
					if (auto *type = md.GetEntry<TypeDef>(base.index()))
						return base_category(type->namespce, type->name);
					break;
				default:
					break;
			}
		}
		return CAT_UNKNOWN;
	}

	CATEGORY	get_category(const Token &tok) {
		switch (tok.type()) {
			case TypeRef:
				if (auto *type = md.GetEntry<TypeRef>(tok.index()))
					return get_category(ref2def[type].or_default());
				break;
			case TypeDef:
				return get_category(md.GetEntry<TypeDef>(tok.index()));

			default:
				break;
		}
		return CAT_UNKNOWN;
	}

	const ENTRY<TypeDef> *GetTypeDef(Token tok) const {
		switch (tok.type()) {
			case TypeDef:
				return md.GetEntry<TypeDef>(tok.index());

			case TypeRef:
				return ref2def[md.GetEntry<TypeRef>(tok.index())].or_default();

			case TypeSpec:
				ReadTypeToken(byte_reader(md.GetEntry<TypeSpec>(tok.index())->signature).me(), tok);
				return GetTypeDef(tok);

			default:
				return 0;
		}
	}
	const ENTRY<TypeDef> *GetTypeDef(const char *name) const {
		return name2def[namespce_name(name)].or_default();
	}
	const ENTRY<TypeDef> *GetTypeDef(const count_string &name) const {
		return name2def[namespce_name(name)].or_default();
	}

	ELEMENT_TYPE get_enum_type(const ENTRY<TypeDef> *t) const {
		ELEMENT_TYPE	e = ELEMENT_TYPE_END;
		for (auto &i : md.GetEntries(t, t->fields)) {
			if (i.flags & i.Literal) {
				bool	found = false;
				Token	itok = md.GetIndexed(&i);
				for (auto &j : md.GetTable<Constant>()) {
					if (j.parent == itok) {
						if (!e)
							e = j.type;
						ISO_ASSERT(e == j.type);
						found = true;
						break;
					}
				}
				ISO_ASSERT(found);
			}
		}
		return e;
	}

	Token fix_token(Token tok) {
		if (tok.type() == TypeSpec)
			ReadTypeToken(byte_reader(md.GetEntry<TypeSpec>(tok.index())->signature).me(), tok);
		return tok;
	}
};

//----------------------------------------------------------------------------
//	Namespace
//----------------------------------------------------------------------------
/*
struct Namespace : hierarchy<Namespace> {
	string	name;
	dynamic_array<const char*>	entries;

	Namespace() {}
	Namespace(const count_string &_name) : name(_name) {}
	template<typename S> bool operator==(const S &s) const { return name == s; }

	string		full_name() const {
		if (parent && parent->name)
			return parent->full_name() + "." + name;
		return name;
	}

	void		add_entry(const char *s) {
		auto	i = lower_boundc(entries, str(s));
		if (i == entries.end() || *i != str(s))
			entries.insert(i, s);
	}

	Namespace	*get_sub(const count_string &s) {
		Namespace *found = find(children, s);
		if (found == children.end()) {
			found = new Namespace(s);
			push_back(found);
		}
		return found;
	}

	static Namespace *get(Namespace *p, const char *ns) {
		for (part_iterator i = ns; *i; ++i)
			p = p->get_sub(*i);
		return p;
	}
};
*/
//----------------------------------------------------------------------------
//	graphs
//----------------------------------------------------------------------------

template<typename T> struct clr_node : graph_edges<clr_node<T> > {
	T			t;
	int			pre_order, order;
	clr_node(T _t) : t(_t), pre_order(-1), order(0) {}
};

template<typename T> struct clr_graph {//: graph_base<clr_node<T> > {
	typedef clr_node<T>		node_t;
	dynamic_array<node_t*>		nodes;


	hash_map<T, node_t*>	node_map;

	node_t *add_node(T t) {
		node_t*	&p = node_map[t];
		if (!p)
			p = nodes.push_back(new node_t(t));
		return p;
	}

	void add_edge(node_t *n, T t) {
		if (t && t != n->t) {
			if (auto *p = node_map.check(t)) {
				for (auto &i : n->outgoing) {
					if (i->t == t)
						return;
				}
				n->add_edge(*p);
			}
		}
	}
	void add_edge (Context &ctx, node_t *n, Token tok, int flags);
	void add_edge (Context &ctx, node_t *n, byte_reader &r, int flags);
	void add_edges(Context &ctx, node_t *n, byte_reader &r, int flags);
	void add_edges(Context &ctx, node_t *n, const ENTRY<TypeDef> *def, int flags);

	dynamic_array<T> get_order();
};

template<typename T> void clr_graph<T>::add_edge(Context &ctx, clr_node<T> *n, byte_reader &r, int flags) {
	ELEMENT_TYPE	t;
	for (;;) {
		switch (t = ELEMENT_TYPE(r.getc())) {
			case ELEMENT_TYPE_CMOD_REQD: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				continue;
			}
			case ELEMENT_TYPE_CMOD_OPT: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				continue;
			}
			case ELEMENT_TYPE_SYSTEMTYPE:
			case ELEMENT_TYPE_PINNED:
			case ELEMENT_TYPE_BYREF:
			case ELEMENT_TYPE_FNPTR:
			case ELEMENT_TYPE_SZARRAY:
			case ELEMENT_TYPE_SENTINEL:
			case ELEMENT_TYPE_BOXED:
			case ELEMENT_TYPE_FIELD:
			case ELEMENT_TYPE_PROPERTY:
			case ELEMENT_TYPE_ENUM:
				continue;

			case ELEMENT_TYPE_PTR:
				// don't record ptrs as dependency
				return;

			case ELEMENT_TYPE_CLASS: {
				TypeDefOrRef	x	= TypeDefOrRef(CompressedUInt(r));
				// don't record class as dependency (unless base class)
				//if (flags & 1)
					add_edge(ctx, n, x, flags | 2);
				break;
			}
			case ELEMENT_TYPE_VALUETYPE:
				add_edge(ctx, n, TypeDefOrRef(CompressedUInt(r)), flags);
				return;

			case ELEMENT_TYPE_ARRAY: {
				add_edge(ctx, n, r, flags);
				CompressedUInt	rank(r);
				CompressedUInt	num_sizes(r);

				for (uint32 i = num_sizes; i--;)
					CompressedUInt _(r);

				CompressedUInt	num_lobounds(r);
				for (uint32 i = num_lobounds; i--;)
					CompressedInt _(r);
				return;
			}

			case ELEMENT_TYPE_GENERICINST: {
				t			= ELEMENT_TYPE(r.getc());	// ELEMENT_TYPE_CLASS or ELEMENT_TYPE_VALUETYPE
				TypeDefOrRef	tmpl	= TypeDefOrRef(CompressedUInt(r));

				// don't record classes as dependency unless base class
				//if (t == ELEMENT_TYPE_VALUETYPE || (flags & 1))
					add_edge(ctx, n, tmpl, flags | (t == ELEMENT_TYPE_CLASS ? 2 : 0));

				uint32	narg	= CompressedUInt(r);
				for (uint32 i = 0; i < narg; ++i)
					add_edge(ctx, n, r, flags);
				return;
			}

			case ELEMENT_TYPE_MVAR:
			case ELEMENT_TYPE_VAR: {
				uint32	n = CompressedUInt(r);
				return;
			}

			case ELEMENT_TYPE_END:
				return;
		}
		break;
	}
}

template<typename T> void clr_graph<T>::add_edges(Context &ctx, clr_node<T> *n, byte_reader &r, int flags) {
	SIGNATURE	sig	= SIGNATURE(r.getc());
	switch (sig & TYPE_MASK) {
		case GENERIC: {
			uint32	gen_param_count	= CompressedUInt(r);
		}
		case DEFAULT:
		case C:
		case STDCALL:
		case THISCALL:
		case FASTCALL:
		case VARARG:
		case PROPERTY: {
			uint32	param_count = CompressedUInt(r);
			for (uint32 i = 0; i < param_count + 1; ++i)
				add_edge(ctx, n, r, flags);
			break;
		}
		case FIELD:
			add_edge(ctx, n, r, flags);
			break;
	}
}

template<typename T> void clr_graph<T>::add_edges(Context &ctx, clr_node<T> *n, const ENTRY<TypeDef> *def, int flags) {
	add_edge(ctx, n, def->extends, 1);

	for (auto &i : ctx.md.GetTable<InterfaceImpl>()) {
		if (ctx.md.GetEntry(i.clss) == def)
			add_edge(ctx, n, i.interfce, flags | 1);
	}

	for (auto &i : ctx.md.GetEntries(def, def->fields))
		add_edges(ctx, n, byte_reader(i.signature).me(), flags);

	for (auto &i : ctx.md.GetEntries(def, def->methods))
		add_edges(ctx, n, byte_reader(i.signature).me(), flags);
}

template<typename T> void visit(dynamic_array<T> &L, clr_node<T> *n) {
    //mark n temporarily
	n->order = -2;

	for (auto m : n->outgoing) {
		if (m->order == -2) {
			m->pre_order = L.size32();
			L.push_back(m->t);
		} else if (m->order < 0) {
			visit(L, m);
		}
	}

	//mark n permanently
	n->order = L.size32();
    L.push_back(n->t);
}

template<typename T> dynamic_array<T> clr_graph<T>::get_order() {
	dynamic_array<T>	L;
	for (auto i : nodes)
		i->order = -1;
	for (auto i : nodes) {
		if (i->order == -1)
			visit(L, i) ;
	}
	return L;
}

//----------------------------------------------------------------------------
// specific graphs
//----------------------------------------------------------------------------

typedef clr_graph<const ENTRY<TypeDef>*>	TypeGraph;
typedef clr_graph<const char*>				NameSpaceGraph;

template<>	void clr_graph<const ENTRY<TypeDef>*>::add_edge(Context &ctx, node_t *n, Token tok, int flags) {
	switch (tok.type()) {
		case TypeDef:
			if (auto *def = ctx.md.GetEntry<TypeDef>(tok.index())) {
				if (!(flags & 2) || (flags & 1)) {
					add_edge(n, def);
					if (flags & 1)
						ctx.needed.insert(def->namespce);
					else
						ctx.needed0.insert(def->namespce);
				} else {
					ctx.reffed.insert(def);
				}
			}
			break;

		case TypeRef:
			if (auto *ref = ctx.md.GetEntry<TypeRef>(tok.index())) {
				if (auto def = ctx.ref2def[ref].or_default()) {
					if (!(flags & 2) || (flags & 1)) {
						add_edge(n, def);
						if (flags & 1)
							ctx.needed.insert(def->namespce);
						else
							ctx.needed0.insert(def->namespce);
					} else {
						ctx.reffed.insert(def);
					}
				}
			}
			break;

		case TypeSpec:
			add_edge(ctx, n, byte_reader(ctx.md.GetEntry<TypeSpec>(tok.index())->signature).me(), flags);
			break;
	}
}

template<>	void clr_graph<const char*>::add_edge(Context &ctx, node_t *n, Token tok, int flags) {
	if ((flags & 2) && !(flags & 1))
		return;

	switch (tok.type()) {
		case TypeDef:
			if (auto *p = ctx.md.GetEntry<TypeDef>(tok.index()))
				add_edge(n, p->namespce);
			break;

		case TypeRef:
			if (auto *p = ctx.md.GetEntry<TypeRef>(tok.index()))
				add_edge(n, p->namespce);
			break;

		case TypeSpec:
			add_edge(ctx, n, byte_reader(ctx.md.GetEntry<TypeSpec>(tok.index())->signature).me(), flags);
			break;
	}
}

//----------------------------------------------------------------------------
// struct CustomAttributeContainer
//----------------------------------------------------------------------------

struct CustomAttributeContainer {
	const METADATA							&md;
	range<const ENTRY<CustomAttribute>*>	attrs;
	const char								*name;
	Token									parent;

	typedef Attribute	element;

	bool ok(const ENTRY<CustomAttribute> *attr) const {
		if (attr->parent.type() != TypeDef)
			return false;

		if (parent && parent != attr->parent)
			return false;

		if (attr->type.type() != MemberRef)
			return false;

		auto	*method = md.GetEntry<MemberRef>(attr->type.index());
		if (method->clss.type() != TypeRef)
			return false;

		return !name || md.GetEntry<TypeRef>(method->clss.index())->name == name;
	}

	struct iterator {
		const CustomAttributeContainer	*c;
		const ENTRY<CustomAttribute>	*i;
		iterator(const CustomAttributeContainer *_c, const ENTRY<CustomAttribute> *_i) : c(_c), i(_i) {}
		iterator	operator++() {
			while (i != c->attrs.end() && !c->ok(++i)) {}
			return *this;
		}
		element		operator*() const {
			return i;
		}
		friend bool operator!=(const iterator &a, const iterator &b) { return a.i != b.i; }
	};

	CustomAttributeContainer(const METADATA &_md, const char *_name) : md(_md), attrs(md.GetTable<clr::CustomAttribute>()), name(_name), parent(0) {}
	CustomAttributeContainer(const METADATA &_md, Token _parent) : md(_md), attrs(md.GetTable<clr::CustomAttribute>()), name(0), parent(_parent) {}

	iterator	begin() const {
		auto	i = attrs.begin();
		while (i != attrs.end() && !ok(i))
			++i;
		return iterator(this, i);
	}

	iterator	end() const {
		return iterator(this, attrs.end());
	}
};

struct SemanticFunctions {
	const ENTRY<MethodDef>	*Setter, *Getter, *Other, *AddOn, *RemoveOn, *Fire;
	SemanticFunctions(const METADATA &md, Token	tok, hash_set<const ENTRY<MethodDef>*> &done_method) {
		clear(*this);

		for (auto &i : md.GetTable<MethodSemantics>()) {
			if (i.association == tok) {
				auto	*method =  md.GetEntry(i.method);
				(&Setter)[lowest_set_index(i.flags)] = method;
				done_method.insert(method);
			}
		}
	}
};

//----------------------------------------------------------------------------
// AttributeElement
//----------------------------------------------------------------------------

struct AttributeElement {
	ELEMENT_TYPE	type;
	union {
		uint64			val;
		count_string	str;
	};

	bool is_string() const {
		return	type == ELEMENT_TYPE_SYSTEMTYPE
			||	type == ELEMENT_TYPE_ENUM
			||	type == ELEMENT_TYPE_STRING;
	}
	template<typename R> bool read(ELEMENT_TYPE sig, R &data) {
		type = sig;
		switch (sig) {
			case ELEMENT_TYPE_SYSTEMTYPE:
			case ELEMENT_TYPE_ENUM:
			case ELEMENT_TYPE_STRING:	 str = SerString(data); break;
			case ELEMENT_TYPE_BOOLEAN:	 val = data.template get<bool8>(); break;
			case ELEMENT_TYPE_CHAR:		 val = data.template get<wchar_t>(); break;
			case ELEMENT_TYPE_I1:		 val = data.template get<int8>(); break;
			case ELEMENT_TYPE_U1:		 val = data.template get<uint8>(); break;
			case ELEMENT_TYPE_I2:		 val = data.template get<int16>(); break;
			case ELEMENT_TYPE_U2:		 val = data.template get<uint16>(); break;
			case ELEMENT_TYPE_I4:		 val = data.template get<int32>(); break;
			case ELEMENT_TYPE_U4:		 val = data.template get<uint32>(); break;
			case ELEMENT_TYPE_I8:		 val = data.template get<int64>(); break;
			case ELEMENT_TYPE_U8:		 val = data.template get<uint64>(); break;
			case ELEMENT_TYPE_R4:		 val = data.template get<float>(); break;
			case ELEMENT_TYPE_R8:		 val = data.template get<double>(); break;
			case ELEMENT_TYPE_I:		 val = data.template get<int>(); break;
			case ELEMENT_TYPE_U:		 val = data.template get<unsigned>(); break;
			default: return false;
		}
		return true;
	}
	template<typename S, typename R> bool read(S &sig, R &data) {
		return read(ELEMENT_TYPE(sig.getc()), data);
	}
	template<typename S, typename R> bool read(const Context &ctx, S &sig, R &data) {
		ELEMENT_TYPE	s = ELEMENT_TYPE(sig.getc());
		switch (s) {
			case ELEMENT_TYPE_CLASS:
			case ELEMENT_TYPE_VALUETYPE: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(sig));
				const ENTRY<TypeDef>	*def = 0;
				switch (x.type()) {
					case TypeRef: {
						const ENTRY<TypeRef>	*ref = ctx.md.GetEntry<TypeRef>(x.index());
						if (ref->namespce == "System" || ref->namespce == "Platform") {
							if (ref->name == "Type")
								return read(ELEMENT_TYPE_SYSTEMTYPE, data);
						}

						def = ctx.ref2def[ref].or_default();
						break;
					}
					case TypeDef:
						def = ctx.md.GetEntry<TypeDef>(x.index());
						break;
				}
				if (def && def->extends.type() == TypeRef && ctx.md.GetEntry<TypeRef>(def->extends.index())->name == "Enum")
					return read(ELEMENT_TYPE_I, data);

				return false;
			}
		}
		return read(s, data);
	}

	template<typename R> AttributeElement(ELEMENT_TYPE sig, R &r)		: str(0) { read(sig, data); }
	template<typename S, typename R> AttributeElement(S &sig, R &data)	: str(0) { read(sig, data); }
	template<typename S, typename R> AttributeElement(const Context &ctx, S &sig, R &data) : str(0) { read(ctx, sig, data); }
};

string_accum &operator<<(string_accum &sa, const AttributeElement &e) {
	if (e.is_string())
		return sa << e.str;
	return sa << e.val;
}

//----------------------------------------------------------------------------
// struct HeaderWriter
//----------------------------------------------------------------------------

struct Method {
	const ENTRY<TypeDef>	*type;
	const ENTRY<MethodDef>	*method;
	Method(const ENTRY<TypeDef> *_type, const ENTRY<MethodDef> *_method) : type(_type), method(_method) {}
};

struct Methods {
	hash_map<string, dynamic_array<Method>>	methods;

	void add(const ENTRY<TypeDef> *type, const ENTRY<MethodDef> &method) {

		if (method.flags & ENTRY<MethodDef>::SpecialName) {
			auto	&array = methods[get_adapted_method_name(method)].put();
			for (auto &i : array) {
				if (i.type == type)
					return;
			}
			array.emplace_back(type, &method);
			return;
		}

		methods[method.name]->emplace_back(type, &method);
	}

	void add(const METADATA &md, const ENTRY<TypeDef> *def) {
		if (def && (def->flags & def->Interface)) {
			if (const char *t = str(def->name).find('`'))
				return;

			for (auto &m : md.GetEntries(def, def->methods)) {
				if (m.name[0] != '.') {
					add(def, m);
					//methods[m.name].emplace_back(def, &m);
				}
			}
		}
	}

	dynamic_array<Method> get_overloads() {
		dynamic_array<Method> methods2;
		/*
		hash_map<string, dynamic_array<Method>>	properties;

		for (auto &i : methods) {
			auto	front = i.front();
			if (front.method->flags & ENTRY<MethodDef>::SpecialName) {
				methods.remove(&i);
				properties[get_adapted_method_name(*front.method)].push_back(front);
			}
		}
		for (auto &i : properties) {
			if (i.size() > 1) {
				i.erase(i.begin() + 1, i.end());
				methods2.append(i);
			}
		}
		*/
		for (auto &i : methods) {
			if (i.size() < 2)
				continue;

			if (i.front().method->flags & ENTRY<MethodDef>::SpecialName) {
				i.erase(i.begin() + 1, i.end());
				methods2.append(i);
				continue;
			}

			auto end = i.end();
			for (auto j = i.begin(); j != end; ++j)
				end = remove(j + 1, end, [j](Method &k) { return params_match(*j->method, *k.method); });

			auto	j		= i.begin();
			auto	type	= j++->type;
			bool	overloads = false;

			while (j != end)
				overloads = j++->type != type;

			if (!overloads) {
				//ISO_TRACEF("adding extra override: ") << type->name << "::" << i.front().method->name << '\n';
				i.erase(i.begin() + 1, i.end());
			}
			//if (overloads)
				methods2.append(i);
		}

		return methods2;
	}
};

struct HeaderWriter : Context {

	enum FLAGS {
		USE_MASK		= 7,
			USE_BASE	= 1,
			USE_RETURN	= 2,
			USE_FIELD	= 3,
			USE_PARAM	= 4,
			USE_TEMP	= 5,
		RAW				= 0,
		ADAPTER			= 8,
		UNADAPTER		= 16,
		TEMPLATE		= 32,
		POINTER			= 64,
		NOSTRIP			= 128,	// do not strip any namespaces
		QUALIFY			= 256,	// must qualify with at least one namespace
	};

	friend FLAGS usage(FLAGS f)					{ return (FLAGS)(f & USE_MASK); }
	friend FLAGS operator|(FLAGS a, FLAGS b)	{ return FLAGS(int(a) | int(b)); }
	friend FLAGS operator-(FLAGS a, FLAGS b)	{ return FLAGS(int(a) & ~int(b)); }

	const char*		raw_prefix;
	bool			properties, events, indent_ns, try_block;

	struct Interfaces : dynamic_array<Token> {
		bool	has_default;
		Interfaces() : has_default(false) {}
	};
	hash_map<const ENTRY<TypeDef>*, Interfaces>	interfaces;
	const char*		current_ns;
	int				depth;
	int				lines;

	HeaderWriter(const METADATA &_md, const char *_raw_prefix = "_") : Context(_md), raw_prefix(_raw_prefix), properties(true), events(true), indent_ns(false), try_block(true), current_ns(""), depth(0), lines(0)	{}

	void get_all_interfaces(hash_set_with_key<Token> &found, const ENTRY<TypeDef> *def) {
		if (def) {
			get_all_interfaces(found, GetTypeDef(def->extends));

			for (auto &i : *interfaces[def]) {
				if (!found.check_insert(i))
					get_all_interfaces(found, GetTypeDef(i));
			}
		}
	}

	void get_all_methods(Methods &methods, const ENTRY<TypeDef> *def) {
		hash_set_with_key<Token> interfaces;
		get_all_interfaces(interfaces, def);
		for (auto &i : interfaces)
			methods.add(md, GetTypeDef(i));
	}

	string_accum&	newline(string_accum &sa)	{ ++lines; return sa << '\n' << repeat('\t', depth); }
	string_accum&	open(string_accum &sa)		{ ++depth; lines = 0; return sa << '{'; }
	string_accum&	close(string_accum &sa)		{ ISO_ASSERT(depth); --depth; if (lines) newline(sa); return sa << '}'; }
	void			set_namespace(string_accum &sa, const char *ns);

	string	full_name(const ENTRY<TypeDef> *def, const char *namespce, const char *name, FLAGS flags);
	string	full_name(const ENTRY<TypeDef> *def, const ENTRY<TypeDef> *type, FLAGS flags = RAW) {
		return type ? full_name(def, type->namespce, type->name, flags) : 0;
	}
	string	full_name(const ENTRY<TypeDef> *def, const ENTRY<TypeRef> *type, FLAGS flags = RAW) {
		return type ? full_name(def, type->namespce, type->name, flags) : 0;
	}
	string	full_name(const ENTRY<TypeDef> *def, const Token &tok, const tmpl_params_t &tmpl_params, FLAGS flags);
	string	signature_type(const ENTRY<TypeDef> *def, byte_reader &r, FLAGS flags, const tmpl_params_t &tmpl_params) {
		string_builder	s;
		write_signature_type(s, def, 0, r, flags, tmpl_params);
		return s;
	}
	string	factory_getter(const ENTRY<TypeDef> *def, const ENTRY<TypeDef> *exclusive) {
		return "get_activation_factory<" + full_name(def, def) << ", " + full_name(def, exclusive) + ">()";
	}

	string	disambiguate(const ENTRY<TypeDef> *root, const ENTRY<TypeDef> *type);

	string_accum&	write_template(string_accum &sa, const char *tname, const ENTRY<TypeDef> *def, const tmpl_params_t &tmpl_params) {
		return newline(sa) << "template<" << template_decl_additional(tmpl_params, 0) << "> struct " << tname << '<' << full_name(def, def) << template_params(tmpl_params) << "> ";
	}

	bool			get_props_events(const ENTRY<TypeDef> *def, range<const ENTRY<Property>*> &properties, range<const ENTRY<clr::Event>*> &events);
	void			write_property(string_accum &sa, const ENTRY<TypeDef> *def, const SemanticFunctions &sf, const tmpl_params_t &tmpl_params, const char *getter);

	bool			used_name(const ENTRY<TypeDef> *def, const char *namespce, const char *name);
	bool			inherits(Token clss, Token tok);
	bool			has_immediate_interface(const ENTRY<TypeDef> *base, const ENTRY<TypeDef> *interface) const;
	bool			has_interface(const ENTRY<TypeDef> *base, const ENTRY<TypeDef> *interface) const;
	bool			has_interface(Token base, const ENTRY<TypeDef> *interface)	const { return has_interface(GetTypeDef(base), interface); }
	bool			has_interface(Token base, Token interface)					const { return has_interface(base, GetTypeDef(interface)); }
	bool			check_for_adaptors(dynamic_array<param> &params);
	param*			get_return(dynamic_array<param> &params);

	ELEMENT_TYPE	get_class_type(const char *&prefix, const char *&suffix, Token tok, FLAGS flags);

	ELEMENT_TYPE	write_signature_type(string_accum &sa, const ENTRY<TypeDef> *def, const char *name, byte_reader &r, FLAGS flags, const tmpl_params_t &tmpl_params);
	void			write_type_ref(string_accum &sa, const ENTRY<TypeDef> *def);
	void			write_type_ref(string_accum &sa, const ENTRY<TypeRef> *ref);
	void			write_valuetype(string_accum &sa, const ENTRY<TypeDef> *def);
	void			write_enum(string_accum &sa, const ENTRY<TypeDef> *def);

	string_accum&	write_field(string_accum &sa, const ENTRY<TypeDef> *def, const ENTRY<Field> &f, const tmpl_params_t &tmpl_params);
	string_accum&	write_method_params(string_accum &sa, const ENTRY<TypeDef> *def, const tmpl_params_t &tmpl_params, const dynamic_array<param> &params, FLAGS flags);
	string_accum&	write_method_params(string_accum &sa, const ENTRY<TypeDef> *def, const tmpl_params_t &tmpl_params, const dynamic_array<param> &params, param *ret_param, FLAGS flags);
	string_accum&	write_method(string_accum &sa, const ENTRY<TypeDef> *def, const ENTRY<MethodDef> &method, const tmpl_params_t &tmpl_params, bool abstract);
	string_accum&	write_adapted_method_body(string_accum &sa, const char *getter, const ENTRY<TypeDef> *def, const ENTRY<MethodDef> &method, const tmpl_params_t &tmpl_params, const dynamic_array<param> &params, param *ret_param, bool return_this = false);
	string_accum&	write_overloaded_method(string_accum &sa, const ENTRY<TypeDef> *def, const ENTRY<TypeDef> *def2, const ENTRY<MethodDef> &method, const tmpl_params_t &tmpl_params);

	void			write_statics(string_accum &sa, const ENTRY<TypeDef> *def, const dynamic_array<const ENTRY<TypeDef>*> &exclusives, const dynamic_array<string> &activators, const ENTRY<TypeDef> *composer);
	void			write_adapter(string_accum &sa, const ENTRY<TypeDef> *def);
	void			write_unadapter(string_accum &sa, const ENTRY<TypeDef> *def);
};

//-------------------------------------
// writers
//-------------------------------------

void HeaderWriter::set_namespace(string_accum &sa, const char *ns) {
	parts<'.'>::iterator n0	= current_ns;
	parts<'.'>::iterator n1	= ns;

	while (*n0 && *n0 == *n1) {
		++n0;
		++n1;
	}

	int	back = 0;
	while (*n0) {
		++back;
		++n0;
	}

	if (indent_ns) {
		while (back--)
			close(sa);
	} else if (back) {
		sa << '\n' << repeat('}', back);
	}

	int	j = 0;
	while (*n1) {
		if (indent_ns) {
			open(newline(sa) << "namespace " << *n1 << " ");
		} else {
			sa << (j++ ? ' ' : '\n') << "namespace " << *n1 << " {";
		}
		++n1;
	}
	current_ns = ns;
}

string_accum& HeaderWriter::write_field(string_accum &sa, const ENTRY<TypeDef> *def, const ENTRY<Field> &f, const tmpl_params_t &tmpl_params) {
	byte_reader	r(f.signature);
	SIGNATURE	sig_flags	= SIGNATURE(r.getc());

	if (f.flags & f.Literal) {
		bool	found	= false;
		Token	itok	= md.GetIndexed(&f);
		for (auto &j : md.GetTable<Constant>()) {
			if (j.parent == itok) {
				sa << "static const ";
				write_signature_type(sa, def, f.name, r, ADAPTER|USE_FIELD, 0);
				sa << " = " << read_bytes<int64>(j.value, j.value.size32()) << ";\n";
				found = true;
				break;
			}
		}
		ISO_ASSERT(found);

	} else {
		if (f.flags & f.Static)
			sa << "static ";
		write_signature_type(sa, def, f.name, r, ADAPTER|USE_FIELD, tmpl_params);
	}
	return sa;
}

string_accum &HeaderWriter::write_method_params(string_accum &sa, const ENTRY<TypeDef> *def, const tmpl_params_t &tmpl_params, const dynamic_array<param> &params, FLAGS flags) {
	int	n = params.size32();
	for (int i = 1; i < n; i++)
		write_signature_type(sa << onlyif(i > 1, ", "), def, params[i].name, byte_reader(params[i].sig).me(), flags | USE_PARAM, tmpl_params);
	if (params[0].element != ELEMENT_TYPE_VOID)
		write_signature_type(sa << onlyif(n > 1, ", "), def, cstr("*") + params[0].name, byte_reader(params[0].sig).me(), flags | POINTER, tmpl_params);
	return sa;
}

string_accum &HeaderWriter::write_method_params(string_accum &sa, const ENTRY<TypeDef> *def, const tmpl_params_t &tmpl_params, const dynamic_array<param> &params, param *ret_param, FLAGS flags) {
	int	j = 0;
	for (auto &i : params) {
		if (&i != ret_param && i.element != ELEMENT_TYPE_VOID)
			write_signature_type(sa << onlyif(j++, ", "), def, i.name, byte_reader(i.sig).me(), ADAPTER | USE_PARAM, tmpl_params);
	}
	return sa;
}

string_accum& HeaderWriter::write_method(string_accum &sa, const ENTRY<TypeDef> *def, const ENTRY<MethodDef> &method, const tmpl_params_t &tmpl_params, bool abstract) {
	if (method.flags & method.Static)
		sa << "static ";

	if (method.flags & method.Virtual)
		sa << "virtual ";

	if (method.name == ".dtor") {
		sa << "//~" << get_name(def);

	} else if (method.name == ".ctor") {
		sa << "//" << get_name(def);

	} else {
		sa << "STDMETHODIMP " << raw_prefix;
		if (auto overload = get_attribute(&method, "OverloadAttribute"))
			sa << SerString(byte_reader(overload).me());
		else
			sa << method.name;
	}

	write_method_params(sa << '(', def, tmpl_params, get_params(md, method), RAW) << ')';

	if (abstract)
		sa << " = 0";
	return sa << ';';
}

string_accum &HeaderWriter::write_adapted_method_body(string_accum &sa, const char *getter, const ENTRY<TypeDef> *def, const ENTRY<MethodDef> &method, const tmpl_params_t &tmpl_params, const dynamic_array<param> &params, param *ret_param, bool return_this) {
	// params
	write_method_params(sa << '(', def, tmpl_params, params, ret_param, ADAPTER | USE_PARAM) << ") { ";

	// return temp
	if (ret_param->element != ELEMENT_TYPE_VOID) {
		write_signature_type(sa, def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_TEMP, tmpl_params);
		sa << ' ' << ret_param->name << "; ";
	}

	// pass to original function
	sa << "hrcheck(" << getter << "->" << raw_prefix;
	if (auto overload = get_attribute(&method, "OverloadAttribute"))
		sa << SerString(byte_reader(overload).me());
	else
		sa << method.name;

	pass_params(sa, params, param::FROM_ADAPTED) << ')';

	if (ret_param->element != ELEMENT_TYPE_VOID) {
		if (ret_param->needs_adjust(param::TO_ADAPTED | param::RETURN))
			sa << "; return from_abi(" << ret_param->name << ')';
		else
			sa << "; return " << ret_param->name;

	} else if (return_this)
		sa << "; return this";

	return sa << "; }";
}

string_accum& HeaderWriter::write_overloaded_method(string_accum &sa, const ENTRY<TypeDef> *def, const ENTRY<TypeDef> *def2, const ENTRY<MethodDef> &method, const tmpl_params_t &tmpl_params) {
	auto	params		= get_params(md, method);
	param	*ret_param	= get_return(params);

	if (method.flags & method.Static)
		sa << "static ";

	write_signature_type(sa, def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN | QUALIFY, tmpl_params);

	sa << ' ' << get_adapted_method_name(method);
	write_method_params(sa << '(', def, tmpl_params, params, ret_param,  ADAPTER | USE_PARAM | QUALIFY) << ") { ";

	if (ret_param->element != ELEMENT_TYPE_VOID)
		sa << "return ";

	if (method.flags & method.Static)
		sa << full_name(def, def2) << "::";
	else
		sa << "get<" << full_name(def, def2) << ">()->";

	return pass_params(sa << get_adapted_method_name(method), params, ret_param, param::MOVE) << "; }";
}


void HeaderWriter::write_type_ref(string_accum &sa, const ENTRY<TypeDef> *def) {
	auto	cat = get_category(def);

	if (tmpl_params_t tmpl_params = get_tmpl_params(md, def)) {
		sa << template_decl(tmpl_params) << "struct " << get_name(def);

	} else if (cat == CAT_ENUM) {
		sa << "enum class " << def->name << " : " << get_element_name(get_enum_type(def));

	} else {
		sa << "struct " << def->name;
	}

	sa << ';';
}

void HeaderWriter::write_type_ref(string_accum &sa, const ENTRY<TypeRef> *ref) {
	if (const ENTRY<TypeDef> *def = ref2def[ref]) {
		write_type_ref(sa, def);

	} else {
		ISO_TRACEF("No def for ") << ref->name << '\n';
		sa << "struct " << ref->name << ';';
	}
}

void HeaderWriter::write_valuetype(string_accum &sa, const ENTRY<TypeDef> *def) {
	tmpl_params_t tmpl_params = get_tmpl_params(md, def);

	open(newline(sa) << template_decl(tmpl_params) << "struct " << get_name(def) << " ");

	for (auto &i : md.GetEntries(def, def->fields))
		write_field(newline(sa), def, i, tmpl_params) << ';';

	close(sa) << ';';
}

void HeaderWriter::write_enum(string_accum &sa, const ENTRY<TypeDef> *def) {
	ELEMENT_TYPE e	= get_enum_type(def);
	bool		sign = IsSigned(e);
	uint32		size = (uint32)GetSize(e);
	open(newline(sa) << "enum class " << def->name << " : " << get_element_name(e) << " ");

	for (auto &i : md.GetEntries(def, def->fields)) {
		if (i.flags & i.Literal) {
			bool	found	= false;
			Token	itok	= md.GetIndexed(&i);
			for (auto &j : md.GetTable<Constant>()) {
				if (j.parent == itok) {
					newline(sa) << i.name << " = ";
					if (sign)
						sa << read_bytes<int64>(j.value, size);
					else
						sa << "0x" << hex(uint64(read_bytes<uint64>(j.value, size)));
					sa << ',';
					found = true;
					break;
				}
			}
			ISO_ASSERT(found);
		}
	}
	close(sa) << ';';
}

//-------------------------------------
// params
//-------------------------------------

bool HeaderWriter::check_for_adaptors(dynamic_array<param> &params) {
	int		adapted = 0;
	for (auto &i : params) {
		switch (i.element) {
			default:
				if (i.element & ELEMENT_TYPE_REF)
					++adapted;
				break;

			case ELEMENT_TYPE_CLASS:
				if (get_category(i.tok) != CAT_DELEGATE)
					break;
				i.element = ELEMENT_TYPE_DELEGATE;
			case ELEMENT_TYPE_STRING:
				++adapted;
				break;

			case ELEMENT_TYPE_SZARRAY:
				++adapted;
				break;
		}
	}
	return adapted > 0;
}

param *HeaderWriter::get_return(dynamic_array<param> &params) {
	check_for_adaptors(params);

	param	*ret = params.begin();
	if (ret->element == ELEMENT_TYPE_VOID) {
		for (auto &i : reversed(params)) {
			if (i.element & ELEMENT_TYPE_REF) {
				i.element |= ELEMENT_TYPE_REF2;
				return &i;
			}
		}
	} else {
		ret->element |= ELEMENT_TYPE_REF2;
	}
	return ret;
}

bool HeaderWriter::inherits(Token clss, Token tok) {
	if (tok == clss)
		return true;

	if (auto def = GetTypeDef(clss)) {
		if (tok == fix_token(def->extends))
			return true;

		for (auto &i : interfaces[def].get()) {
			if (inherits(fix_token(i), tok))
				return true;
		}
	}

	return false;
}

bool HeaderWriter::has_immediate_interface(const ENTRY<TypeDef> *def, const ENTRY<TypeDef> *interface) const {
	for (auto &i : interfaces[def]) {
		if (GetTypeDef(i) == interface)
			return true;
	}
	return false;
}

bool HeaderWriter::has_interface(const ENTRY<TypeDef> *base, const ENTRY<TypeDef> *interface) const {
	if (base) {

		if (base == interface)
			return true;

		if (base->extends && has_interface(base->extends, interface))
			return true;

		for (auto &i : interfaces[base]) {
			if (has_interface(i, interface))
				return true;
		}
	}
	return false;
}

//-------------------------------------
// names
//-------------------------------------

bool HeaderWriter::used_name(const ENTRY<TypeDef> *def, const char *namespce, const char *name) {
	if (!def)
		return false;

	if (def->name == name && def->namespce != namespce)
		return true;

	for (auto &i : md.GetEntries(def, def->methods)) {
		if ((i.name.begins("put_") || i.name.begins("get_")) ? (i.name.slice(4) == name) : (i.name == name))
			return true;
		for (auto &p : md.GetEntries(&i, i.paramlist)) {
			if (p.name == name)
				return true;
		}
	}

	for (auto &i : interfaces[def].get()) {
		if (used_name(GetTypeDef(i), namespce, name))
			return true;
	}

	return used_name(GetTypeDef(def->extends), namespce, name);
}


string HeaderWriter::full_name(const ENTRY<TypeDef> *def, const char *namespce, const char *name, FLAGS flags) {
	count_string	name2;
	if (const char *t = str(name).find('`'))
		name2 = str(name, t);
	else
		name2 = count_string(name);

	if (namespce == cstr("System"))
		namespce = "Platform";

	if (!current_ns || (flags & NOSTRIP))
		return replace(namespce, ".", "::") + "::" + name2;

	parts<'.'>::iterator	n0		= current_ns;
	parts<'.'>::iterator	n1		= namespce;

	while (*n0 && *n0 == *n1) {
		++n0;
		++n1;
	}
	if (!*n1) {
		if ((flags & QUALIFY) || used_name(def, namespce, name)) {
			const char *dot = str(namespce).rfind('.');
			return str(dot ? dot + 1 : namespce) + "::" + name2;
		}
		return name2;
	}
#if 1
//	ISO_ASSERT(str(name) != "DeviceInformation");
	for (bool restart = true; restart; ) {
		restart = false;
		for (auto i = n0; *i; ++i) {
			string	ns = i.full() + "." + *n1;
			if (restart = name2def.check(namespce_name(ns))) {
				--n1;
				break;
			}
		}
	}
#else
	while (*n0) {
		string	ns = str(n0.s, n0.n) + "." + *n1;
		if (name2def.check(namespce_name(ns))) {
			string	ns2 = str(n1.s == namespce ? "." : "") + namespce;
			return replace(ns2, ".", "::") + "::" + name2;
		}
		++n0;
	}
#endif
	return replace(str(n1.p, n1.e), ".", "::") + "::" + name2;
}

string HeaderWriter::full_name(const ENTRY<TypeDef> *def, const Token &tok, const tmpl_params_t &tmpl_params, FLAGS flags) {
	switch (tok.type()) {
		case TypeRef:
			return full_name(def, md.GetEntry<TypeRef>(tok.index()), flags);
		case TypeDef:
			return full_name(def, md.GetEntry<TypeDef>(tok.index()), flags);
		case TypeSpec:
			return signature_type(def, byte_reader(md.GetEntry<TypeSpec>(tok.index())->signature).me(), flags, tmpl_params);
		default:
			return 0;
	}
}


string HeaderWriter::disambiguate(const ENTRY<TypeDef> *root, const ENTRY<TypeDef> *type) {
	if (!root)
		return 0;

	if (type == root)
		return get_name(root);

	const ENTRY<TypeDef>	*def	= GetTypeDef(root->extends);
	string					name	= disambiguate(def, type);

	for (auto &i : interfaces[root].get()) {
		auto	idef = GetTypeDef(i);
		if (string iname = disambiguate(idef, type)) {
			if (name)
				return get_name(def) + "::" + name;
			def		= idef;
			name	= iname;
		}
	}
	return name;
}

//-------------------------------------
// HeaderWriter::write_signature_type
//-------------------------------------

ELEMENT_TYPE HeaderWriter::get_class_type(const char *&prefix, const char *&suffix, Token tok, FLAGS flags) {
	ELEMENT_TYPE	t = get_category(tok) == CAT_DELEGATE ? ELEMENT_TYPE_DELEGATE : ELEMENT_TYPE_CLASS;
	prefix = suffix = 0;

	if (flags & ADAPTER) {
		switch (usage(flags)) {
			case USE_TEMP:
				suffix = "*";
				break;
			case USE_PARAM:
				prefix = t == ELEMENT_TYPE_DELEGATE ? "handler_ref<" : flags & TEMPLATE ? "ptr<" : "pptr<";
				suffix = ">";
				break;
			case USE_BASE:
				if (!(flags & TEMPLATE))
					break;
			default:
				prefix = "ptr<";
				suffix = ">";
				break;
		}
	} else {
		if (usage(flags) != USE_BASE || (flags & TEMPLATE))
			suffix = "*";
	}

	return t;
}

ELEMENT_TYPE HeaderWriter::write_signature_type(string_accum &sa, const ENTRY<TypeDef> *def, const char *name, byte_reader &r, FLAGS flags, const tmpl_params_t &tmpl_params) {
	ELEMENT_TYPE	t;
	for (;;) {
		switch (t = ELEMENT_TYPE(r.getc())) {
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

			case ELEMENT_TYPE_BYREF: {
				if (usage(flags) != USE_PARAM)
					continue;
				//const char *prefix = (flags & ADAPTER) && !(flags & (TEMPLATE | POINTER)) ? "&" : "*";
				const char *prefix = "*";
				return ELEMENT_TYPE(write_signature_type(sa, def, str(prefix) + name, r, flags | POINTER, tmpl_params) | ELEMENT_TYPE_REF);
			}
			case ELEMENT_TYPE_CLASS: {
				Token	tok		= TypeDefOrRef(CompressedUInt(r));

				const char *prefix, *suffix;
				t	= get_class_type(prefix, suffix, tok, flags);
				sa << prefix << full_name(def, tok, tmpl_params, flags) << suffix;
				break;
			}

			case ELEMENT_TYPE_VALUETYPE: {
				TypeDefOrRef	type	= TypeDefOrRef(CompressedUInt(r));
//				if (usage(flags) == USE_PARAM && !(flags & (TEMPLATE | POINTER)) && get_category(type) != CAT_ENUM)
				if (usage(flags) == USE_PARAM && (flags & ADAPTER) && !(flags & (TEMPLATE | POINTER)) && get_category(type) != CAT_ENUM)
					sa << "const " << full_name(def, type, tmpl_params, flags) << '&';
				else
					sa << full_name(def, type, tmpl_params, flags);
				break;
			}

			case ELEMENT_TYPE_OBJECT:
				if (flags & ADAPTER) {
					switch (usage(flags)) {
						case USE_TEMP:
							sa << "IInspectable*";
							break;
						case USE_PARAM:
							if (flags & POINTER)
								sa << "IInspectable*";
							else if (flags & TEMPLATE)
								sa << "object";
							else
								sa << "object_ref";
							break;
						case USE_BASE:
							if (!(flags & TEMPLATE))
								break;
						default:
							sa << "object";
							break;
					}
				} else {
					if (usage(flags) != USE_BASE || (flags & TEMPLATE))
						sa << "IInspectable*";
					else
						sa << "IInspectable";
				}
//				sa << ifelse((flags & ADAPTER) && usage(flags) == USE_PARAM && !(flags & (TEMPLATE|POINTER)), "object", "IInspectable*");
//				sa << ifelse((flags & ADAPTER) && usage(flags) != USE_TEMP && !(flags & POINTER), "object", "IInspectable*");
				break;

			case ELEMENT_TYPE_STRING:
				if (flags & ADAPTER) {
					switch (usage(flags)) {
						case USE_FIELD:
							sa << "hstring";
							break;
						case USE_RETURN:
							sa << "hstring";
							break;
						case USE_PARAM:
							if (flags & (TEMPLATE | POINTER))
								sa << "hstring";
							else
								sa << "hstring_ref";
							break;
						default:
							sa << "HSTRING";
							break;
					}
				} else {
					sa << "HSTRING";
				}
				break;

			case ELEMENT_TYPE_TYPEDBYREF:	sa << "typedbyref";			break;
			case ELEMENT_TYPE_INTERNAL:		sa << "<internal>";			break;
			case ELEMENT_TYPE_VOID:			sa << "void";				break;
			case ELEMENT_TYPE_BOOLEAN:		sa << "bool";				break;
			case ELEMENT_TYPE_CHAR:			sa << "char";				break;
			case ELEMENT_TYPE_I1:			sa << "char";				break;
			case ELEMENT_TYPE_U1:			sa << "unsigned char";		break;
			case ELEMENT_TYPE_I2:			sa << "short";				break;
			case ELEMENT_TYPE_U2:			sa << "unsigned short";		break;
			case ELEMENT_TYPE_I4:			sa << "int";				break;
			case ELEMENT_TYPE_U4:			sa << "unsigned";			break;
			case ELEMENT_TYPE_I8:			sa << "__int64";			break;
			case ELEMENT_TYPE_U8:			sa << "unsigned __int64";	break;
			case ELEMENT_TYPE_R4:			sa << "float";				break;
			case ELEMENT_TYPE_R8:			sa << "double";				break;
			case ELEMENT_TYPE_I:			sa << "int";				break;
			case ELEMENT_TYPE_U:			sa << "unsigned";			break;

			case ELEMENT_TYPE_ARRAY: {
				write_signature_type(sa, def, name, r, flags, tmpl_params);
				CompressedUInt	rank(r);
				CompressedUInt	num_sizes(r);

				for (uint32 i = num_sizes; i--;)
					sa << '[' << (uint32)CompressedUInt(r) << ']';

				for (uint32 i = rank - num_sizes; i--;)
					sa << "[]";

				CompressedUInt	num_lobounds(r);
				for (uint32 i = num_lobounds; i--;)
					CompressedInt	lobound(r);
				return t;
			}
			case ELEMENT_TYPE_FNPTR:
				write_signature_type(sa, def, name, r, flags, tmpl_params);
				return t;

			case ELEMENT_TYPE_GENERICINST: {
				t				= ELEMENT_TYPE(r.getc());	// ELEMENT_TYPE_CLASS or ELEMENT_TYPE_VALUETYPE
				Token	tok		= TypeDefOrRef(CompressedUInt(r));

				const char *prefix = 0, *suffix = 0;
				if (t == ELEMENT_TYPE_CLASS)
					t = get_class_type(prefix, suffix, tok, flags);

				sa << prefix << full_name(def, tok, tmpl_params, flags) << '<';

				flags = flags | TEMPLATE;
				//if (t == ELEMENT_TYPE_DELEGATE)
				//	flags = flags - ADAPTER;

				for (uint32 i = 0, n = CompressedUInt(r); i < n; ++i)
					write_signature_type(sa << onlyif(i, ", "), def, 0, r, flags | TEMPLATE, tmpl_params);

				sa << '>' << suffix;
				break;
			}

			case ELEMENT_TYPE_MVAR:
			case ELEMENT_TYPE_VAR: {
				uint32	n = CompressedUInt(r);
				sa << tmpl_params[n]->name;
				if (usage(flags) == USE_TEMP || (flags & UNADAPTER))
					sa << 'R';
				break;
			}

			case ELEMENT_TYPE_PTR:
				write_signature_type(sa, def, cstr("*") + name, r, flags, tmpl_params);
				return t;

			case ELEMENT_TYPE_SZARRAY:
				if (flags & ADAPTER) {
					bool	param = usage(flags) == USE_PARAM && !(flags & (TEMPLATE | POINTER));
					if (param)
						sa << "const ";
					write_signature_type(sa << "szarray<", def, 0, r, flags | TEMPLATE, tmpl_params);
					sa << ">";
					if (param)
						sa << '&';
					break;
				} else {
					write_signature_type(sa << "unsigned " << name << "Size, ", def, cstr("*") + name, r, flags | POINTER, tmpl_params);
				}
				return t;

			case ELEMENT_TYPE_SENTINEL:
			case ELEMENT_TYPE_BOXED:
			case ELEMENT_TYPE_FIELD:
			case ELEMENT_TYPE_PROPERTY:
			case ELEMENT_TYPE_ENUM:
				continue;

			case ELEMENT_TYPE_END:
				return t;
		}
		break;
	}

	if (name)
		sa << ' ' << name;
	return t;
}

//-------------------------------------
// properties
//-------------------------------------

bool HeaderWriter::get_props_events(const ENTRY<TypeDef> *def, range<const ENTRY<Property>*> &p, range<const ENTRY<clr::Event>*> &e) {
	p = empty;
	e = empty;

	if (properties || events) {
		auto	index = md.GetIndexed(def);

		if (properties) {
			for (auto &i : md.GetTable<PropertyMap>()) {
				if (i.parent == index) {
					p = md.GetEntries(&i, i.property_list);
					break;
				}
			}
		}
		if (events) {
			for (auto &i : md.GetTable<EventMap>()) {
				if (i.parent == index) {
					e = md.GetEntries(&i, i.event_list);
					break;
				}
			}
		}

		return !p.empty() || !e.empty();
	}
	return false;
}

void HeaderWriter::write_property(string_accum &sa, const ENTRY<TypeDef> *def, const SemanticFunctions &sf, const tmpl_params_t &tmpl_params, const char *getter) {
	if (sf.Getter) {
		auto	params		= get_params(md, *sf.Getter);
		param	*ret_param	= get_return(params);
		string	ret_type	= signature_type(def, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN, tmpl_params);

		write_adapted_method_body(newline(sa) << ret_type << " get", getter, def, *sf.Getter, tmpl_params, params, ret_param);

		newline(sa) << ret_type << " operator()() { return get(); }";
		newline(sa) << "operator " << ifelse(ret_param->element == (ELEMENT_TYPE_STRING | ELEMENT_TYPE_REF2), "hstring_ref", ret_type) << " () { return get(); }";

		if (ret_param->element == (ELEMENT_TYPE_CLASS | ELEMENT_TYPE_REF2))
			newline(sa) << ret_type << " operator->() { return get(); }";

	}
	if (sf.Setter) {
		auto	params		= get_params(md, *sf.Setter);
		param	*ret_param	= get_return(params);

		write_adapted_method_body(newline(sa) << "void put", getter, def, *sf.Setter, tmpl_params, params, ret_param);

		pass_params(write_method_params(newline(sa) << "void operator=(",  def, tmpl_params, params, ret_param, ADAPTER | USE_PARAM) << ") { put", params, param::MOVE) << "; }";
		pass_params(write_method_params(newline(sa) << "void operator()(", def, tmpl_params, params, ret_param, ADAPTER | USE_PARAM) << ") { put", params, param::MOVE) << "; }";
	}

	if (sf.AddOn) {
		auto	params		= get_params(md, *sf.AddOn);
		param	*ret_param	= get_return(params);
		write_signature_type(newline(sa), def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN, tmpl_params);
		write_adapted_method_body(sa << " operator+=", getter, def, *sf.AddOn, tmpl_params, params, ret_param);
	}
	if (sf.RemoveOn) {
		auto	params		= get_params(md, *sf.RemoveOn);
		param	*ret_param	= get_return(params);
		write_signature_type(newline(sa), def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN, tmpl_params);
		write_adapted_method_body(sa << " operator-=", getter, def, *sf.RemoveOn, tmpl_params, params, ret_param);
	}
}

//-------------------------------------
// HeaderWriter::write_statics
//-------------------------------------

void HeaderWriter::write_statics(string_accum &sa, const ENTRY<TypeDef> *def, const dynamic_array<const ENTRY<TypeDef>*> &exclusives, const dynamic_array<string> &activators, const ENTRY<TypeDef> *composer) {
	count_string	name		= get_name(def);
	string			full		= full_name(def, def);
	CATEGORY		cat			= get_category(def);
	tmpl_params_t	tmpl_params = get_tmpl_params(md, def);

	range<const ENTRY<Property>*>	prop_range;
	range<const ENTRY<clr::Event>*> event_range;

	char	targ[2]	= "X";
	bool	got_pe	= false;
	for (auto exclusive : exclusives) {
		if (!has_interface(def, exclusive))
			got_pe |= get_props_events(exclusive, prop_range, event_range);
	}

	open(newline(sa) << "template<typename> struct " << name << "_statics ");

	hash_set<const ENTRY<MethodDef>*>	done_method;

	// properties

	if (got_pe) {
		for (auto exclusive : exclusives) {
			if (has_interface(def, exclusive))
				continue;

			string	getter = factory_getter(def, exclusive);

			get_props_events(exclusive, prop_range, event_range);

			for (auto &prop : prop_range) {
				open(newline(sa) << "static struct _" << prop.name << " : property ");
				write_property(sa, def, SemanticFunctions(md, md.GetIndexed(&prop), done_method), tmpl_params, getter);
				close(sa) << ' ' << fix_method_name(prop.name) << ';';
			}

			for (auto &event : event_range) {
				open(newline(sa) << "static struct _" << event.name << " : property ");
				write_property(sa, def, SemanticFunctions(md, md.GetIndexed(&event), done_method), tmpl_params, getter);
				close(sa) << ' ' << fix_method_name(event.name) << ';';
			}
		}
	}

	// methods

	for (auto &method : md.GetEntries(def, def->methods)) {
		if (method.name[0] == '.' || !(method.flags & method.Static))
			continue;

		auto	pmethod = &method;
		if (const ENTRY<TypeDef> *exclusive = find_exclusive(md, exclusives, pmethod)) {
			if (done_method.count(pmethod))
				continue;

			auto	params		= get_params(md, method);
			param	*ret_param	= get_return(params);
			write_signature_type(newline(sa) << "static ", def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN, tmpl_params);
			write_adapted_method_body(sa << ' ' << get_adapted_method_name(method), factory_getter(def, exclusive), def, method, tmpl_params, params, ret_param);

		} else {
			auto	params		= get_params(md, method);
			param	*ret_param	= get_return(params);
			write_signature_type(newline(sa) << "static ", def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN, tmpl_params);
			write_method_params(sa << ' ' << get_adapted_method_name(method) << '(', def, tmpl_params, params, ret_param, ADAPTER | USE_PARAM) << ");";
		}
	}

	for (auto &i : activators) {
		if (i == ".") {
			if (activators.size() == 1)
				continue;
			//default activator
			newline(sa) << "static " << name << " *activate() { " << name << " *t; get_activation_factory<" << name << ">()->ActivateInstance((IInspectable**)&t); return t; }";

		} else {
			//	template<> struct ref_new<X> : ptr<X> {
			//		ref_new(A a, B b, ...) { get_activation_factory<X, F>()->Create(a, b, ..., &t); }
			//	};
			const ENTRY<TypeDef> *activator = name2def[namespce_name(i)];

			for (auto &method : md.GetEntries(activator, activator->methods)) {
				if (method.name == ".cctor")	// don't add static (class) constructor
					continue;

				newline(sa) << "static " << name << " *activate(";
				auto	params = get_params(md, method);
				int		n		= params.size32();

				//params

				for (int i = 1; i < n; i++)
					write_signature_type(sa << onlyif(i > 1, ", "), def, params[i].name, byte_reader(params[i].sig).me(), ADAPTER | USE_PARAM, 0);

				sa << ") { " << name << " *" << params[0].name << "; ";
				sa << "hrcheck(get_activation_factory<" << name << ", " << full_name(def, activator) << ">()->" << raw_prefix << method.name << '(';

				// pass to activator function

				for (uint32 i = 1; i < n; ++i)
					params[i].pass(sa << onlyif(i > 1, ", "), param::FROM_ADAPTED);

				sa << ", &" << params[0].name << ")); return " << params[0].name << "; }";

			}
		}
	}

	if (composer) {
		for (auto &method : md.GetEntries(composer, composer->methods)) {
			if (method.name == ".cctor")	// don't add static (class) constructor
				continue;

			newline(sa) << "static " << name << " *activate(";
			auto	params = get_params(md, method);
			int		n		= params.size32();

			//params

			for (int i = 1, j = 0; i < n; i++)
				write_signature_type(sa << onlyif(j++, ", "), def, params[i].name, byte_reader(params[i].sig).me(), ADAPTER | USE_PARAM, 0);

			sa << ") { " << name << " *" << params[0].name << "; ";
			sa << "hrcheck(get_activation_factory<" << name << ", " << full_name(def, composer) << ">()->" << raw_prefix << method.name << '(';

			// pass to activator function

			for (int i = 1, j = 0; i < n; ++i) {
				if (j++)
					sa << ", ";
				params[i].pass(sa, param::TO_ADAPTED);
			}

			sa << ", &" << params[0].name << ")); return " << params[0].name << "; }";
		}
	}

	//need a default constructor...
	//if (got_pe)
	//	newline(sa) << name << "_statics() {}";

	close(sa) << ';';

	// static defs of properties
	if (got_pe) {
		for (auto exclusive : exclusives) {
			if (has_interface(def, exclusive))
				continue;

			get_props_events(exclusive, prop_range, event_range);

			for (auto &prop : prop_range)
				newline(sa) << "template<typename X> typename " << name << "_statics<X>::_" << prop.name << ' ' << name << "_statics<X>::" << prop.name << ";";
			for (auto &event : event_range)
				newline(sa) << "template<typename X> typename " << name << "_statics<X>::_" << event.name << ' ' << name << "_statics<X>::" << event.name << ";";
		}
	}

	sa << '\n';
}

//-------------------------------------
// HeaderWriter::write_adapter
//-------------------------------------

void HeaderWriter::write_adapter(string_accum &sa, const ENTRY<TypeDef> *def) {
	count_string	name		= get_name(def);
	CATEGORY		cat			= get_category(def);
	tmpl_params_t	tmpl_params = get_tmpl_params(md, def);
	char			targ[2]		= "X";

	range<const ENTRY<Property>*>	prop_range;
	range<const ENTRY<clr::Event>*> event_range;
	bool			got_pe		= get_props_events(def, prop_range, event_range);

	if (got_pe) {
		for (bool restart = true; restart;) {
			restart = false;

			for (auto &i : prop_range) {
				if (restart = (i.name == targ))
					break;
			}
			if (!restart) {
				for (auto &i : event_range) {
					if (restart = (i.name == targ))
						break;
				}
			}
			if (restart) {
				if (targ[0]++ == 'Z')
					targ[0] = 'A';
			}
		}
	}

	newline(sa) << "template<typename " << targ << template_decl_additional(tmpl_params);
	open(sa << "> struct " << name << "_adaptor : " << targ << " ");

	for (auto &i : tmpl_params)
		newline(sa) << "typedef to_abi_t<" << i->name << "> " << i->name << "R;";

	hash_set<const ENTRY<MethodDef>*>	done_method;

	// properties

	if (got_pe) {
		open(newline(sa) << "union ");
		//string	getter = "((" + name + "_adaptor*)this)->get()";
		string	getter = "enc(&" + name + "_adaptor::";

		for (auto &prop : prop_range) {
			open(newline(sa) << "struct : property ");
			auto	pname = fix_method_name(prop.name);
			write_property(sa, def, SemanticFunctions(md, md.GetIndexed(&prop), done_method), tmpl_params, getter + pname + ")");
			close(sa) << " " << pname << ';';
		}

		for (auto &event : event_range) {
			open(newline(sa) << "struct : property ");
			auto	pname = fix_method_name(event.name);
			write_property(sa, def, SemanticFunctions(md, md.GetIndexed(&event), done_method), tmpl_params, getter + pname + ")");
			close(sa) << " " << pname << ';';
		}

		close(sa) << ';';
	}

	// methods

	string	getter = targ + cstr("::get()");
	for (auto &method : md.GetEntries(def, def->methods)) {
		if (method.name[0] == '.' || (method.flags & method.Static))	// don't add static (class) constructor, etc
			continue;

		if (done_method.count(&method))
			continue;

//		ISO_ASSERT(method.name != "GetInspectableArray");

		auto	params		= get_params(md, method);
		param	*ret_param	= get_return(params);

		write_signature_type(newline(sa), def, 0, byte_reader(ret_param->sig).me(), ADAPTER | USE_RETURN, tmpl_params);
		write_adapted_method_body(sa << ' ' << get_adapted_method_name(method), getter, def, method, tmpl_params, params, ret_param);
	}

	//need a default constructor...
	if (got_pe)
		newline(sa) << name << "_adaptor() {}";

	close(sa) << ';';
}

//-------------------------------------
// HeaderWriter::write_unadapter
//-------------------------------------

void HeaderWriter::write_unadapter(string_accum &sa, const ENTRY<TypeDef> *def) {
	count_string	name		= get_name(def);
	tmpl_params_t	tmpl_params	= get_tmpl_params(md, def);
	const char		*targ		= "X";

	newline(sa) << "template<typename " << targ << template_decl_additional(tmpl_params);
	open(sa << "> struct " << name << "_unadaptor : " << targ << " ");

	for (auto &i : tmpl_params)
		newline(sa) << "typedef to_abi_t<" << i->name << "> " << i->name << "R;";

	// unadapters

	for (auto &method : md.GetEntries(def, def->methods)) {
		if (method.name[0] == '.' || method.name == "Invoke" || (method.flags & method.Static))
			continue;

		auto	params		= get_params(md, method);
		param	*ret_param	= get_return(params);

		newline(sa) << "STDMETHODIMP " << raw_prefix;
		if (auto overload = get_attribute(&method, "OverloadAttribute"))
			sa << SerString(byte_reader(overload).me());
		else
			sa << method.name;

		write_method_params(sa << '(', def, tmpl_params, params, UNADAPTER | USE_PARAM) << ") { ";

		if (try_block)
			sa << "return hrtry([&, this] { ";

		if (properties && method.name.begins("get_")) {
			sa << "get_prop(" << ret_param->name << ", " << get_adapted_method_name(method) << ')';

		} else if (properties && method.name.begins("put_")) {
			sa << "put_prop(" << params[1].name << ", " << get_adapted_method_name(method) << ')';

		} else {
//			ISO_ASSERT(method.name!="First");

			if (ret_param->element != ELEMENT_TYPE_VOID)
				sa << '*' << ret_param->name << " = ";

			bool	to_abi = ret_param->needs_adjust(param::FROM_ADAPTED | param::RETURN);

			if (to_abi)
				sa << "to_abi(";

			sa << targ << "::get()->";
			const char *close = 0;

			if (properties) {
				if (method.name.begins("put_")) {
					sa << get_adapted_method_name(method) << '=';
				} else if (method.name.begins("get_")) {
					sa << get_adapted_method_name(method);
				} else if (method.name.begins("add_")) {
					sa << method.name.slice(4) << "+=";
				} else if (method.name.begins("remove_")) {
					sa << method.name.slice(7) << "-=";
				} else {
					close = ")";
					sa << method.name << '(';
				}
			} else {
				close = ")";
				sa << get_adapted_method_name(method) << "(";
			}

			// pass to user's function

			sa << comma_list(make_rangec(params), [&](string_accum &sa, param &i) {
				if (i.element == ELEMENT_TYPE_VOID || &i == ret_param)
					return false;

				if (i.element == ELEMENT_TYPE_SZARRAY) {
					byte_reader	r(i.sig);
					ELEMENT_TYPE	t = ELEMENT_TYPE(r.getc());
					write_signature_type(sa << "{(", def, 0, r, RAW, tmpl_params);
					sa << "*)" <<i.name << ", " << i.name << "Size}";
				} else {
					sa << i.name;
				}
				return true;
			});

			sa << close;

			if (ret_param->element == (ELEMENT_TYPE_SZARRAY | ELEMENT_TYPE_REF2))
				sa << ".detach(" << ret_param->name << "Size)";

			if (to_abi)
				sa << ')';

		}

		if (try_block)
			sa << "; }); }";
		else
			sa << "; return S_OK; }";
	}
	close(sa) << ';';
}


//-----------------------------------------------------------------------------
//	main
//-----------------------------------------------------------------------------

bool check_namespace(const dynamic_array<const char*> &namespaces, string_base<const char*> namespce) {
	bool	visible = namespaces.empty();

	for (auto &f : namespaces) {
		if (f[0] == '-') {
			if (visible && namespce.begins(f + 1))
				visible = false;

		} else if (f[0] == '*') {
			if (!visible && namespce.begins(f + 1))
				visible = true;

		} else if (namespce == f) {
			return true;
		}
	}
	return visible;
}

void MakeMDH(const METADATA &md, string_accum &sa0, string_accum &sa, const dynamic_array<const char*> &namespaces) {
	const char *winrt = "iso_winrt";

	sa << "#pragma once\n";
	sa << "// generated by isopod tools\n";
	sa << "// namespaces:\n";
	for (auto i : namespaces)
		sa << "// " << i << '\n';
	sa << '\n';
	sa << "#include \"" << (namespaces[0] + int(!is_alpha(namespaces[0][0]))) << ".0.h\"\n";

	//----------------------------------------------------------------------------
	// make ref->def assoc & collect namespaces
	//----------------------------------------------------------------------------

	HeaderWriter	hw(md);

	hash_set_with_key<const char*>	all_ns;
	for (auto &i : md.GetTable<clr::TypeDef>())
		all_ns.insert(i.namespce);

	for (auto &ns : all_ns) {
		for (parts<'.'>::iterator i = ns; *i; ++i)
			hw.name2def[namespce_name(i.full())].put();	// create dummy entry for namespace
	}

	//----------------------------------------------------------------------------
	// collect types to output
	//----------------------------------------------------------------------------

	TypeGraph	g;

	for (auto &i : md.GetTable<clr::TypeDef>()) {
		if (check_namespace(namespaces, i.namespce) && i.namespce != "" && i.namespce != "System" && is_alpha(i.name[0]))
			g.add_node(&i);
	}

	//----------------------------------------------------------------------------
	// collect interfaces
	//----------------------------------------------------------------------------

	hash_set<const ENTRY<TypeDef>*>	overridable;

	for (auto &i : md.GetTable<InterfaceImpl>()) {
		auto	*def = md.GetEntry(i.clss);
		if (g.node_map.check(def)) {

			auto &interfaces = hw.interfaces[def].put();
			interfaces.push_back(i.interfce);

			if (auto da = hw.get_attribute(&i, "DefaultAttribute")) {
				interfaces.has_default = true;
				swap(interfaces.front(), interfaces.back());
			}

			if (auto ov = hw.get_attribute(&i, "OverridableAttribute"))
				overridable.insert(hw.GetTypeDef(i.interfce));
		}
	}

	for (auto i : g.node_map) {
		auto	*def	= i->t;
		bool	intf	= !!(def->flags & def->Interface);

		auto&	interfaces		= hw.interfaces[def].put();
		bool	has_default		= interfaces.has_default;
		Token	extends			= def->extends;
		auto	end				= interfaces.end();

		if (intf) {
			//interfaces shouldn't extend (?)
			ISO_ASSERT(!extends);
		} else {
			// remove redundant interfaces
			end = remove(interfaces, [&](Token i) { return hw.has_interface(extends, i); });
		}

		// remove redundant interfaces (but not default, if any)
		for (auto i = interfaces.begin(); i < end; ++i)
			end = remove(interfaces.begin() + int(has_default), end, [&](Token j) { return *i != j && hw.has_interface(*i, j); });
		interfaces.erase(end, interfaces.end());

		if (!has_default) {
			Token*	best_interface	= 0;
			int		best_match		= -1;

			for (auto &i : interfaces) {
				int		match = inherit_match(md, def, hw.GetTypeDef(i));
				if (match > best_match) {
					best_match		= match;
					best_interface	= &i;
				}
			}

			// move default interface to front
			if (best_interface)
				swap(*best_interface, interfaces[0]);
		}
	}

	//----------------------------------------------------------------------------
	// find namespace order
	//----------------------------------------------------------------------------

//	hash_set_with_key<const char*>	all_ns;
//	for (auto &i : md.GetTable<clr::TypeDef>())
//		all_ns.insert(i.namespce);
//	hw.namespaces = all_ns;

	NameSpaceGraph	g_ns;
	for (auto i : g.nodes)
		g_ns.add_node(i->t->namespce);

//	for (auto &type : md.GetTable<clr::TypeDef>())
//		g_ns.add_node(type.namespce);
//	for (auto &type : md.GetTable<clr::TypeRef>())
//		g_ns.add_node(type.namespce);

	for (auto i : g.nodes)
		g_ns.add_edges(hw, g_ns.node_map[i->t->namespce], i->t, 0);

	auto	Lns = g_ns.get_order();

	//----------------------------------------------------------------------------
	// find type order
	//----------------------------------------------------------------------------

	for (auto i : g.nodes)
		i->order = g_ns.node_map[i->t->namespce]->order;

	// sort types based on their namespace order
	sort(g.nodes, [](const TypeGraph::node_t *a, const TypeGraph::node_t *b) {
		return a->order < b->order;
	});

	for (auto &i : g.nodes)
		g.add_edges(hw, i, i->t, 0);

	//----------------------------------------------------------------------------
	// collect activators
	//----------------------------------------------------------------------------

	hash_map<const ENTRY<TypeDef>*, dynamic_array<string> > activators;

	for (auto a : CustomAttributeContainer(md, "ActivatableAttribute")) {
		auto	*def	= a.Parent(md);
		if (!g.node_map.check(def))
			continue;

		byte_reader		sig(a.Method(md)->signature + 1);
		byte_reader		data(a.Data());

		uint32			param_count	= CompressedUInt(sig);
		if (param_count < 2)
			continue;

		sig.getc();	// return type

		AttributeElement	e	= AttributeElement(hw, sig, data);
		if (e.type == ELEMENT_TYPE_SYSTEMTYPE) {
			activators[def]->push_back(e.str);

			g.add_edge(g.node_map[def], hw.name2def[namespce_name(e.str)]);

		} else if (e.type == ELEMENT_TYPE_U4 && e.read(hw, sig, data)) {
			if (e.type == ELEMENT_TYPE_STRING && e.str == "Windows.Foundation.UniversalApiContract")
				activators[def]->push_back(".");
		}
	}

	hash_map<const ENTRY<TypeDef>*, const ENTRY<TypeDef>*>		composable;

	for (auto a : CustomAttributeContainer(md, "ComposableAttribute")) {
		auto	*def	= a.Parent(md);
		if (!g.node_map.check(def))
			continue;

		byte_reader		sig(a.Method(md)->signature + 1);
		byte_reader		data(a.Data());

		uint32			param_count	= CompressedUInt(sig);
		if (param_count < 2)
			continue;
		sig.getc();	// return type

		AttributeElement	e0	= AttributeElement(hw, sig, data);	// type
		AttributeElement	e1	= AttributeElement(hw, sig, data);	// public?
		if (e1.val == 2) {
			auto composer = hw.GetTypeDef(e0.str);
			if (md.GetEntries(composer, composer->methods).size())
				composable[def] = composer;
			//ISO_TRACEF("Composable: ") << def->name << " with " << e0 << '\n';
		}
	}

	//----------------------------------------------------------------------------
	// collect exclusive-to attributes
	//----------------------------------------------------------------------------

	hash_map<const ENTRY<TypeDef>*, dynamic_array<const ENTRY<TypeDef>*> > exclusives;

	for (auto a : CustomAttributeContainer(md, "ExclusiveToAttribute")) {
		auto	*def	= a.Parent(md);

		byte_reader		sig(a.Method(md)->signature + 1);
		byte_reader		data(a.Data());
		uint32			param_count	= CompressedUInt(sig);
		sig.getc();	// return type

		AttributeElement	e	= AttributeElement(hw, sig, data);
		if (e.type == ELEMENT_TYPE_SYSTEMTYPE) {
			const ENTRY<TypeDef> *parent = hw.name2def[namespce_name(e.str)];
			ISO_ASSERT(parent);
			if (auto p = g.node_map.check(parent)) {
				if (!hw.has_immediate_interface(parent, def)) {
					exclusives[parent]->push_back(def);
					g.add_edge(*p, def);
				}
			}
		}
	}

	//----------------------------------------------------------------------------
	// write out forward defs
	//----------------------------------------------------------------------------

	hw.needed0 &= ~hw.needed;

	const char *controls	= 0;
	const char *primitives	= 0;
	for (auto &i : md.GetTable<clr::TypeDef>()) {
		if (!controls && i.namespce == "Windows.UI.Xaml.Controls") {
			controls = i.namespce;
			if (primitives)
				break;
		}
		if (!primitives && i.namespce == "Windows.UI.Xaml.Controls.Primitives") {
			primitives = i.namespce;
			if (controls)
				break;
		}
	}

	if (hw.needed0.remove(primitives))
		hw.needed0.insert(controls);
	else if (hw.needed.remove(primitives))
		hw.needed.insert(controls);

	for (auto &i : hw.needed0) {
		if (!check_namespace(namespaces, i))
			sa << "#include \"" << i << ".0.h\"\n";
	}
	for (auto &i : hw.needed) {
		if (!check_namespace(namespaces, i))
			sa << "#include \"" << i << ".h\"\n";
	}

	dynamic_array<const ENTRY<TypeDef>*>	ordered_refs = hw.reffed;
	sort(ordered_refs, [](const ENTRY<TypeDef> *a, const ENTRY<TypeDef> *b) { return a->namespce < b->namespce; });
	sa << "\nnamespace " << winrt << " {\n\n// forward types\n";

	for (auto t : ordered_refs) {
		if (!hw.needed.count(t->namespce) && !hw.needed0.count(t->namespce)) {
			hw.set_namespace(sa, t->namespce);
			hw.write_type_ref(hw.newline(sa), t);
		}
	}
	hw.set_namespace(sa, "");


	auto	ordered_defs = g.get_order();

	//-----------------------------
	// defs
	//-----------------------------

	sa << "\n\n// defs\n";
	for (auto def : ordered_defs) {
		tmpl_params_t	tmpl_params = get_tmpl_params(md, def);
		hw.write_template(sa, "def", def, tmpl_params);

		switch (hw.get_category(def)) {
			case hw.CAT_INTERFACE: {
				if (def->visibility() == def->Public || overridable.count(def))
					sa << ": overridable_type<";
				else
					sa << ": interface_type<";

				int	j = 0;
				for (auto &i : hw.interfaces[def].get())
					sa << onlyif(j++, ", ") << hw.full_name(def, i, tmpl_params, hw.ADAPTER | hw.USE_BASE);
				sa << '>';
				break;
			}

			case hw.CAT_VALUE: {
				sa << ": value_type<";
				int	j = 0;
				for (auto &i : md.GetEntries(def, def->fields)) {
					if (i.flags & (i.Literal | i.Static))
						continue;

					byte_reader	r(i.signature);
					SIGNATURE	sig_flags = SIGNATURE(r.getc());
					hw.write_signature_type(sa << onlyif(j++, ", "), def, 0, r, hw.USE_FIELD, tmpl_params);
				}
				sa << '>';
				break;
			}

			case hw.CAT_CLASS: {
				sa << ": class_type<";
				Token	extends			= def->extends;
				auto	interfaces		= hw.interfaces[def].or_default();

				sa << hw.full_name(def, extends, tmpl_params, hw.ADAPTER | hw.USE_BASE);

				if (!interfaces) {
					sa << ", Platform::Object";
				} else {
					for (auto &i : interfaces)
						sa << ", " << hw.full_name(def, i, tmpl_params, hw.ADAPTER | hw.USE_BASE);
				}
				sa << '>';
				break;
			}
			case hw.CAT_DELEGATE: {
				sa << ": delegate_type";
				break;
			}
			case hw.CAT_ENUM: {
				ELEMENT_TYPE e	= hw.get_enum_type(def);
				sa << ": enum_type<" << get_element_name(e) << '>';
				break;
			}
		}

		if (auto comp = composable[def].or_default()) {
			sa << ", composer_type<" << hw.full_name(def, comp) << '>';
		} else {
			auto	acts		= activators[def].or_default();
			if (acts->size() && (acts->size() != 1 || (*acts)[0] != "."))
				sa << ", custom_activators";
		}


		hw.open(sa << ' ');
#if 0
		hw.newline(sa) << "static constexpr auto ";
		if (tmpl_params) {
			sa << "name{" << "make_constexpr_string(L\"" << def->namespce << "." << def->name << "<\")";
			for (auto i : tmpl_params)
				sa << " + name<" << i->name << ">::value";
			sa << " + L\">\"";
		} else {
			sa << "&name{" << "L\"" << def->namespce << "." << def->name << '"';
		}
		sa << "};";
#endif

		hw.close(sa) << ';';

	}
#if 0
	sa << "\n\n// names\n";
	for (auto def : ordered_defs) {
		tmpl_params_t	tmpl_params = get_tmpl_params(md, def);

		hw.write_template(sa, "name", def, tmpl_params) << "{ static constexpr auto ";
		if (tmpl_params) {
			sa << "value{" << "make_constexpr_string(L\"" << def->namespce << "." << def->name << "<\")";
			for (auto i : tmpl_params)
				sa << " + def<" << i->name << ">::name";
			sa << " + L\">\"";
		} else {
			sa << "&value{" << "L\"" << def->namespce << "." << def->name << '"';
		}
		sa << "}; };";
	}
#endif
	sa << "\n\n// uuids\n";
	for (auto def : ordered_defs) {
		if (const GUID *guid = hw.get_attribute(def, "GuidAttribute")) {
			tmpl_params_t	tmpl_params = get_tmpl_params(md, def);

#if 1
			hw.open(hw.write_template(sa, tmpl_params ? "uuid_gen" : "uuid", def, tmpl_params));
			sa << " define_guid(0x" << hex(guid->Data1) << ", 0x" << hex(guid->Data2) << ", 0x" << hex(guid->Data3);
			for (auto i : guid->Data4)
				sa << ", 0x" << hex(i);
			sa << ");";
#else
			hw.open(hw.write_template(sa, "uuid", def, tmpl_params));
			if (tmpl_params) {
				sa << " generate_guid((generate_pinterface_guid" << template_params(tmpl_params) << '(' << CODE_GUID(*guid) << ")));";
			} else {
				sa << " define_guid(" << "0x" << hex(guid->Data1) << ", 0x" << hex(guid->Data2) << ", 0x" << hex(guid->Data3);
				for (auto i : guid->Data4)
					sa << ", 0x" << hex(i);
				sa << ");";
//				sa << "static constexpr GUID value = " << CODE_GUID(*guid) << ';';
			}
#endif
			hw.close(sa) << ';';
		}
	}

	//----------------------------------------------------------------------------
	// types
	//----------------------------------------------------------------------------

	sa << "\n\n// types\n";
	for (auto def : ordered_defs) {
		auto			cat			= hw.get_category(def);
		if (cat == hw.CAT_VALUE || cat == hw.CAT_ENUM)
			continue;

		count_string	name		= hw.get_name(def);
		string			full_name	= hw.full_name(def, def, hw.NOSTRIP);
		tmpl_params_t	tmpl_params = get_tmpl_params(md, def);

		hw.set_namespace(sa, def->namespce);
		hw.newline(sa) << "\n// " << name;

		switch (cat) {
			case hw.CAT_INTERFACE: {

				//-----------------------------
				// can we just inherit?
				//-----------------------------
				if (md.GetEntries(def, def->methods).empty()) {
					hw.newline(sa) << template_decl(tmpl_params) << "struct " << name << " : IInspectable, generate<" << name << template_params(tmpl_params) << "> {};";
					break;
				}

				bool	can_override	= def->visibility() == def->Public || overridable.count(def);
				bool	has_adaptors	= true;
				if (auto e2 = hw.get_attribute(def, "ExclusiveToAttribute")) {
					const ENTRY<TypeDef> *parent = hw.name2def[namespce_name(SerString(byte_reader(e2).me()))];
					has_adaptors = hw.has_interface(parent, def);
				}

				//-----------------------------
				// raw
				//-----------------------------
				hw.newline(sa) << template_decl(tmpl_params) << "struct " << name << onlyif(has_adaptors, "_raw") << " : IInspectable ";
				hw.open(sa);
				for (auto &method : md.GetEntries(def, def->methods)) {
					if (method.name[0] != '.' && !(method.flags & method.Static))
						hw.write_method(hw.newline(sa), def, method, tmpl_params, can_override);
				}
				hw.close(sa) << ';';

				if (!has_adaptors)
					break;

				//-----------------------------
				// adapter
				//-----------------------------

				hw.write_adapter(sa, def);

				hw.newline(sa) << "template<typename X" << template_decl_additional(tmpl_params) << "> struct adapt<" << name << template_params(tmpl_params) << ", X> : "
					<< full_name << "_adaptor<X" << template_params_additional(tmpl_params) << "> ";
				sa << "{ typedef adapt " << name << "; };";

				//-----------------------------
				// unadapter
				//-----------------------------
				if (can_override) {
					hw.write_unadapter(sa, def);

					hw.newline(sa) << "template<typename X" << template_decl_additional(tmpl_params)
						<< "> struct unadapt<" << name << template_params(tmpl_params) << ", X> : "
						<< full_name << "_unadaptor<X" << template_params_additional(tmpl_params)
						<< "> {};";
				}

				//-----------------------------
				// full type
				//-----------------------------
				hw.newline(sa) << template_decl(tmpl_params) << "struct " << name << " : " << name << "_raw";

				if (tmpl_params) {
					sa << '<';
					int	j = 0;
					for (auto i : tmpl_params)
						sa << onlyif(j++, ", ") << "to_abi_t<" << i->name << '>';
					sa << "> ";
				}

				//<< template_params(tmpl_params)
				sa	<< ", generate<" << name << template_params(tmpl_params) << "> {};";

				break;
			}

			case hw.CAT_CLASS: {
				//-----------------------------
				// statics
				//-----------------------------

				auto	composer	= composable[def].or_default();
				auto	acts		= activators[def].or_default();
				bool	has_acts	= composer || (acts->size() && (acts->size() != 1 || (*acts)[0] != "."));
				bool	statics		= has_statics(md, def) || has_acts;

				if (statics)
					hw.write_statics(sa, def, exclusives[def], acts, composer);

				hw.newline(sa) << "template<typename X" << template_decl_additional(tmpl_params) << "> struct statics<" << name << template_params(tmpl_params) << ", X> : X";

				if (statics)
					sa << ", " << full_name << "_statics<void>";
				hw.open(sa << ' ');

				if (has_acts)
					hw.newline(sa) << "using " << full_name << "_statics<void>::activate;";

				hw.newline(sa) << "typedef typename X::root_type " << name << ';';
				hw.close(sa) << ';';

				//-----------------------------
				// full type (with overloads)
				//-----------------------------

				hw.newline(sa) << template_decl(tmpl_params) << "struct " << name << " : " << "generate<" << name << template_params(tmpl_params) << "> ";
				hw.open(sa);

				Methods	methods;
				hw.get_all_methods(methods, def);
#if 0
				for (auto &i :  methods.get_overloads())
					hw.write_overloaded_method(hw.newline(sa), def, i.type, *i.method, tmpl_params);
#else

				const ENTRY<TypeDef>	*prev_type = 0;
				string					prev_name;
				for (auto &i :  methods.get_overloads()) {
					auto name = get_adapted_method_name(*i.method);
					if (prev_type != i.type || prev_name != name) {
						//hw.newline(sa) << "using " << hw.disambiguate(def, i.type) << "::" << name << ';';
						hw.newline(sa) << "using " << i.type->name << "::" << name << ';';
						prev_type = i.type;
						prev_name = name;
					}
				}
#endif

				hw.close(sa) << ';';
				break;
			}

			case hw.CAT_DELEGATE:
				hw.open(hw.newline(sa) << template_decl(tmpl_params) << "struct " << name << " : IUnknown ");
				for (auto &method : md.GetEntries(def, def->methods)) {
					if (method.name[0] != '.' && !(method.flags & method.Static))
						hw.write_method(hw.newline(sa), def, method, tmpl_params, true);
				}
				hw.close(sa) << ';';
				break;
		}

	}

	hw.set_namespace(sa, "");

	sa << "\n} // namespace " << winrt << '\n';

	//----------------------------------------------------------------------------
	// 0 header
	//----------------------------------------------------------------------------

	sa0 << "#pragma once\n";
	sa0 << "// generated by isopod tools\n";
	sa0 << '\n';

	sa0 << "#include \"pre_include.h\"\n";
	sa0 << "\nnamespace " << winrt << " {\n\n// forward types\n";

//	ordered_refs = make_transformed(g.node_map, [](const clr_node<const ENTRY<TypeDef>*> *a) { return a->t; });
//	sort(ordered_refs, [](const ENTRY<TypeDef> *a, const ENTRY<TypeDef> *b) { return a->namespce < b->namespce; });

	for (auto t : ordered_defs) {
		hw.set_namespace(sa0, t->namespce);

		switch (hw.get_category(t)) {
			case hw.CAT_ENUM:
				hw.write_enum(sa0, t);
				break;

			case hw.CAT_VALUE:
				hw.write_valuetype(sa0, t);
				break;

			default:
				hw.write_type_ref(hw.newline(sa0), t);
				break;
		}
	}

	hw.set_namespace(sa0, "");
	sa0 << "\n} // namespace " << winrt << '\n';
}
