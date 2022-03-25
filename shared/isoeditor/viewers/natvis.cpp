#include "natvis.h"
#include "extra/identifier.h"

using namespace iso;

namespace {

using Node			= NATVIS::Node;
using DisplayNode	= NATVIS::DisplayNode;
using Expression	= NATVIS::Expression;
using Expressions	= NATVIS::Expressions;
using Item			= NATVIS::Item;
using Params		= NATVIS::Params;
using ExpandedAST	= NATVIS::ExpandedAST;

void ReadItems(dynamic_array<Item*> &items, XMLiterator &it);

C_value Evaluate(ast::noderef exp, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
	auto	r = const_fold(exp(node), get_var, get_mem);
	return r && r->kind == ast::literal ? r->cast<ast::lit_node>()->get_value(get_mem) : C_value();
}

//-----------------------------------------------------------------------------
//	Nodes
//-----------------------------------------------------------------------------

struct SimpleItem : Item {
	string		name;
	Expression	exp;
	SimpleItem(XMLiterator &it) : Item(Simple, it.data), name(it.data["Name"]), exp(it.Content()) {}
	SimpleItem(const SimpleItem *item, const Params &params) : Item(item, params), name(item->name), exp(item->exp, params) {}

	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		auto	e = exp.GetAST(node, get_var, get_mem, true);
		all.emplace_back(name, e(node));
	}
};

struct ArrayItems : Item {
	Expressions	size, value;
	Expressions	lowerbound, direction, rank;
	ArrayItems(XMLiterator &it) : Item(Array, it.data) {
		for (it.Enter(); it.Next();) {
			if (it.data.Is("Size"))
				size.emplace_back(it);
			else if(it.data.Is("ValuePointer"))
				value.emplace_back(it);
		}
	}
	ArrayItems(const ArrayItems *item, const Params &params) : Item(item, params)
		, size(item->size, params)
		, value(item->value, params)
		, lowerbound(item->lowerbound, params)
		, direction(item->direction, params)
		, rank(item->rank, params)
	{}
	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		int64	num = size.Evaluate(node, get_var, get_mem);
		auto	ptr	= value.Find(node, get_var, get_mem);
		for (int i = 0; i < num; i++)
			all.emplace_back(format_string("[%i]", i), ptr[i].substitute("this", node));
	}
};

struct IndexListItems : Item {
	Expressions	size, value;
	IndexListItems(XMLiterator &it) : Item(IndexList, it.data) {
		for (it.Enter(); it.Next();) {
			if (it.data.Is("Size"))
				size.emplace_back(it);
			else if(it.data.Is("ValueNode"))
				value.emplace_back(it);
		}
	}
	IndexListItems(const IndexListItems *item, const Params &params) : Item(item, params)
		, size(item->size, params)
		, value(item->value, params)
	{}
	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		int64	num = size.Evaluate(node, get_var, get_mem);
		auto	val	= value.Find(node, get_var, get_mem);
		for (int i = 0; i < num; i++)
			all.emplace_back(format_string("[%i]", i), val.substitute("this", node).substitute("$i", ast::noderef::make(i)));
	}
};

struct LinkedListItems : Item {
	Expressions	size, head, value, next;
	LinkedListItems(XMLiterator &it) : Item(LinkedList, it.data) {
		for (it.Enter(); it.Next();) {
			if (it.data.Is("Size"))
				size.emplace_back(it);
			else if(it.data.Is("HeadPointer"))
				head.emplace_back(it);
			else if(it.data.Is("ValueNode"))
				value.emplace_back(it);
			else if(it.data.Is("NextPointer"))
				next.emplace_back(it);
		}
	}
	LinkedListItems(const LinkedListItems *item, const Params &params) : Item(item, params)
		, size(item->size, params)
		, head(item->head, params)
		, value(item->value, params)
		, next(item->next, params)
	{}

	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		auto	s		= size.Find(node, get_var, get_mem);
		int64	num		= s ? (int64)Evaluate(s, node, get_var, get_mem) : 100;//maximum;
		auto	ptr		= head.Find(node, get_var, get_mem);
		hash_set<uint64>	seen;

		for (int i = 0; i < num; i++) {
			auto	r	= ptr(node);
			auto	r2	= const_fold(r, get_var, get_mem);
			if (!r2 || r2->kind != ast::literal || seen.check_insert(r2->cast<ast::lit_node>()->v))
				break;
			all.emplace_back(format_string("[%i]", i), value.Find(r2, get_var, get_mem)(r));
			ptr = next.Find(r2, get_var, get_mem)(ptr);
		}
	}
};

struct TreeItems : Item {
	Expressions	size, head, value, left, right;
	TreeItems(XMLiterator &it) : Item(Tree, it.data) {
		for (it.Enter(); it.Next();) {
			if (it.data.Is("Size"))
				size.emplace_back(it);
			else if(it.data.Is("HeadPointer"))
				head.emplace_back(it);
			else if(it.data.Is("ValueNode"))
				value.emplace_back(it);
			else if(it.data.Is("LeftPointer"))
				left.emplace_back(it);
			else if(it.data.Is("RightPointer"))
				right.emplace_back(it);
		}
	}
	TreeItems(const TreeItems *item, const Params &params) : Item(item, params)
		, size(item->size, params)
		, head(item->head, params)
		, value(item->value, params)
		, left(item->left, params)
		, right(item->right, params)
	{}
	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		auto	s		= size.Find(node, get_var, get_mem);
		int64	num		= s ? (int64)Evaluate(s, node, get_var, get_mem) : 100;//maximum;
		auto	ptr		= head.Find(node, get_var, get_mem);
		hash_set<uint64>	seen;

		for (int i = 0; i < num; i++) {
			auto	r	= ptr(node);
			auto	r2	= const_fold(r, get_var, get_mem);
			if (r2->kind != ast::literal || seen.check_insert(r2->cast<ast::lit_node>()->v))
				break;
			all.emplace_back(format_string("[%i]", i), value.Find(r2, get_var, get_mem)(r));
			ptr = left.Find(r2, get_var, get_mem)(ptr);
		}
	}
};

struct ExpandedItem : Item {
	Expression	value;
	ExpandedItem(XMLiterator &it) : Item(Expanded, it.data), value(it.Content()) {}
	ExpandedItem(const ExpandedItem *item, const Params &params) : Item(item, params), value(item->value, params) {}

	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		auto	exp = value.GetAST(node, get_var, get_mem);
		auto	r	= exp(node);
		auto	r2	= const_fold(r, get_var, get_mem);

		if (r2->kind == ast::literal) {
			const C_type*	type	= r2->type;
			if (auto ntype = natvis->Find(type)) {
				if (ntype->has_expand) {
					ntype->ExpandAST(natvis, all, r, get_var, get_mem);
					return;
				}
			}

			switch (type->type) {
				case C_type::POINTER:
					all.emplace_back("*", *r);
					break;

				case C_type::ARRAY: {
					auto	*a	= (const C_type_array*)type;
					for (int i = 0; i < a->count; i++)
						all.emplace_back(format_string("[%i]", i), r[i]);
					break;
				}
				case C_type::STRUCT:
				case C_type::UNION: {
					auto	*a	= (const C_type_struct*)type;
					for (auto &i : a->elements)
						all.emplace_back(i.id, r[&i]);
				}
			}
		}
	}
};

struct SyntheticItem : Item, NATVIS::TypeItem {

	static const auto synthetic = ast::KIND(0x1000);

	struct synthetic_node : ast::custom_node {
		ast::node		*arg;
		SyntheticItem	*synth;
		static bool check(ast::KIND k) { return k == synthetic; }
		synthetic_node(ast::node *arg, SyntheticItem *synth) : ast::custom_node(synthetic), arg(arg), synth(synth) {}
		virtual node *apply(const callback_ref<node*(node*)> &t) {
			return this;
		}

		bool	DumpValue(string_accum &sa, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var) {
			return synth->Dump(sa, arg->cast<ast::lit_node>(), natvis, get_var, get_mem);
		}
		void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
			synth->ExpandAST(natvis, all, arg, get_var, get_mem);
		}
		bool	HasChildren() const {
			return synth->has_expand && !synth->items.empty();
		}
	};

	SyntheticItem(XMLiterator &it) : Item(Synthetic, it.data), TypeItem(it.data) {
		auto	&data = it.data;
		for (it.Enter(); it.Next();) {
			if (data.Is("DisplayString")) {
				display.emplace_back(it);

			} else if (data.Is("StringView")) {
				stringview.emplace_back(it);

			} else if (data.Is("Expand")) {
				has_expand = true;
				ReadItems(items, it);
			}
		}
	}
	SyntheticItem(const SyntheticItem *item, const Params &params) : Item(item, params), NATVIS::TypeItem(*item, params) {}

	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		all.emplace_back(name, new synthetic_node(node, this));
	}
};

//-----------------------------------------------------------------------------
//	Custom Nodes
//-----------------------------------------------------------------------------

struct Var : ast::lit_node	{
	Var(const string &id) : ast::lit_node(id, C_value()) {}
};

struct CustomNode : Node {
	enum TYPE {
		Size, Loop, If, Variable, Exec, Break, Item
	};
	typedef dynamic_array<ref_ptr<Var>> Variables;

	TYPE	type;
	CustomNode(TYPE type, XMLreader::Data &data) : Node(data), type(type) {}
	CustomNode(const CustomNode *c, const Params &params) : Node(*c, params), type(c->type) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		return false;
	}
};

struct CustomNodes : dynamic_array<CustomNode*>	{
	CustomNodes(XMLiterator &it);
	CustomNodes(const CustomNodes &nodes, const Params &params);

	bool	Expand(dynamic_array<ExpandedAST> &all, CustomNode::Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		size_t	nv	= variables.size();
		bool	ret	= false;
		for (auto &i : *this) {
			if (ret = i->condition.Test(node, get_var, get_mem) && i->Expand(all, variables, node, get_var, get_mem))
				break;
		}
		variables.resize(nv);
		return ret;
	}

};

struct CustomListItems : Item {
	CustomNodes	nodes;
	CustomListItems(XMLiterator &it) : Item(CustomList, it.data), nodes(it) {}
	CustomListItems(const CustomListItems *item, const Params &params) : Item(item, params), nodes(item->nodes, params) {}
	virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem);
};

// Custom Nodes

struct CustomSize	: CustomNode {
	Expression	val;
	CustomSize(XMLiterator &it) : CustomNode(Size, it.data), val(it.Content()) {}
	CustomSize(const CustomSize *c, const Params &params) : CustomNode(c, params), val(c->val, params) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		variables[0]->set_value(val.Evaluate(node, get_var, get_mem));
		if (variables[0]->v > 0x100000)
			variables[0]->v = 0;
		return variables[0]->type && variables[0]->v <= all.size();
	}
};

struct CustomLoop	: CustomNode, CustomNodes {
	CustomLoop(XMLiterator &it) : CustomNode(Loop, it.data), CustomNodes(it) {}
	CustomLoop(const CustomLoop *c, const Params &params) : CustomNode(c, params), CustomNodes(*c, params) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		while (!CustomNodes::Expand(all, variables, node, get_var, get_mem))
			;
		return variables[0]->type && variables[0]->v == all.size();
	}
};

struct CustomIf		: CustomNode, CustomNodes {
	CustomIf	*next;
	CustomIf(XMLiterator &it) : CustomNode(If, it.data), CustomNodes(it), next(0) {}
	CustomIf(const CustomIf *c, const Params &params) : CustomNode(c, params), CustomNodes(*c, params) {
		if (c->next)
			next = new CustomIf(next, params);
	}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		return CustomNodes::Expand(all, variables, node, get_var, get_mem)
			|| (next && next->condition.Test(node, get_var, get_mem) && next->Expand(all, variables, node, get_var, get_mem));
	}
};


struct CustomVariable : CustomNode, Var {
	Expression		init;
	CustomVariable(XMLiterator &it) : CustomNode(Variable, it.data), Var(it.data.Find("Name")), init(it.data.Find("InitialValue")) { addref(); }
	CustomVariable(const CustomVariable *c, const Params &params) : CustomNode(c, params), Var(*c), init(c->init, params) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		set_value(init.Evaluate(node, get_var, get_mem));
		variables.push_back(this);
		return false;
	}
};

struct CustomExec	: CustomNode {
	Expression	exec;
	CustomExec(XMLiterator &it) : CustomNode(Exec, it.data), exec(it.Content()) {}
	CustomExec(const CustomExec *c, const Params &params) : CustomNode(c, params), exec(c->exec, params) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		exec.Evaluate(node, get_var, get_mem);
		return false;
	}
};

struct CustomBreak	: CustomNode {
	CustomBreak(XMLiterator &it) : CustomNode(Break, it.data) {}
	CustomBreak(const CustomBreak *c, const Params &params) : CustomNode(c, params) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		return true;
	}
};

struct CustomItem	: CustomNode {
	Expression	item;
	CustomItem(XMLiterator &it) : CustomNode(Item, it.data), item(it.Content()) {}
	CustomItem(const CustomItem *c, const Params &params) : CustomNode(c, params), item(c->item, params) {}
	virtual bool	Expand(dynamic_array<ExpandedAST> &all, Variables &variables, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
		auto	val = item.GetAST(node, get_var, get_mem).substitute("this", node);
		val	= const_fold(val, get_var, get_mem);
		all.emplace_back(format_string("[%i]", all.size()), val);
		return variables[0]->type && variables[0]->v == all.size();
	}
};

CustomNodes::CustomNodes(XMLiterator &it)  {
	auto		&data = it.data;
	CustomIf	*lastif = 0;
	for (it.Enter(); it.Next();) {
		if (lastif) {
			if (data.Is("ElseIf")) {
				lastif->push_back(new CustomIf(it));
				continue;
			}
			if (data.Is("Else")) {
				lastif->push_back(new CustomIf(it));
				lastif = 0;
				continue;
			}
			lastif = 0;
		}
		if (data.Is("Size"))
			push_back(new CustomSize(it));
		else if (data.Is("Loop"))
			push_back(new CustomLoop(it));
		else if (data.Is("Variable"))
			push_back(new CustomVariable(it));
		else if (data.Is("Exec"))
			push_back(new CustomExec(it));
		else if (data.Is("Break"))
			push_back(new CustomBreak(it));
		else if (data.Is("Item"))
			push_back(new CustomItem(it));
		else if (data.Is("If"))
			push_back(lastif = new CustomIf(it));
	}
}

CustomNodes::CustomNodes(const CustomNodes &nodes, const Params &params) {
	for (auto i : nodes) {
		switch (i->type) {
			case CustomNode::Size:		push_back(new CustomSize		((CustomSize	*)i, params)); break;
			case CustomNode::Loop:		push_back(new CustomLoop		((CustomLoop	*)i, params)); break;
			case CustomNode::If:		push_back(new CustomIf			((CustomIf		*)i, params)); break;
			case CustomNode::Variable:	push_back(new CustomVariable	((CustomVariable*)i, params)); break;
			case CustomNode::Exec:		push_back(new CustomExec		((CustomExec	*)i, params)); break;
			case CustomNode::Break:		push_back(new CustomBreak		((CustomBreak	*)i, params)); break;
			case CustomNode::Item:		push_back(new CustomItem		((CustomItem	*)i, params)); break;
		}
	}
}

void CustomListItems::Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
	CustomNode::Variables variables;
	variables.push_back(new Var("Size"));
	nodes.Expand(all, variables, node,
		[&variables, get_var](const char* id) -> ast::node* {
			for (auto &i : reversed(variables))
				if (i->id == id)
					return i;
			return get_var(id);
		},
		get_mem
	);
}

//-----------------------------------------------------------------------------
//	ReadItems
//-----------------------------------------------------------------------------

void ReadItems(dynamic_array<Item*> &items, XMLiterator &it) {
	auto	&data = it.data;
	for (it.Enter(); it.Next();) {
		if (data.Is("Item"))
			items.push_back(new SimpleItem(it));
		else if (data.Is("ArrayItems"))
			items.push_back(new ArrayItems(it));
		else if (data.Is("IndexListItems"))
			items.push_back(new IndexListItems(it));
		else if (data.Is("LinkedListItems"))
			items.push_back(new LinkedListItems(it));
		else if (data.Is("TreeItems"))
			items.push_back(new TreeItems(it));
		else if (data.Is("CustomListItems"))
			items.push_back(new CustomListItems(it));
		else if (data.Is("ExpandedItem"))
			items.push_back(new ExpandedItem(it));
		else if (data.Is("Synthetic"))
			items.push_back(new SyntheticItem(it));
	}
}

}

//-----------------------------------------------------------------------------
//	NATVIS
//-----------------------------------------------------------------------------

template<> const char *field_names<NATVIS::Priority>::s[] = {
	"Low", "MediumLow", "Medium", "MediumHigh", "High"
};

NATVIS::Type::Type(const XMLreader::Data &data) : NodeIncExc(data), TypeItem(data)
	, inheritable(data.Get("Inheritable", true))
	, priority(data.Get("Priority", Medium))
{}


string substitute(string_scan s, const Params &params) {
	string_builder	b;
	const char		*start = s.getp();

	while (s.scan("$T")) {
		b.merge(start, s.getp());
		int	i = s.move(2).get<int>();
		b << params[i - 1];
		start = s.getp();
	}
	b << s.remainder();
	return b;
}

NATVIS::Expression::Expression(const Expression &exp, const Params &params) : s(substitute(exp.s, params)) {}

ast::noderef NATVIS::Expression::GetAST(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem, bool formatted) {
	if (!ast && s)
		ast = ReadFormattedExpression(s, ctypes, GetScope(NotPointer(node->type)), get_var);

	if (!formatted && ast) {
		if (auto fmt = ast->cast<formatted_node>())
			return fmt->arg;
	}
	return ast;
}

ast::noderef NATVIS::Expressions::Find(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
	for (auto &i : *this) {
		if (i.condition.Test(node, get_var, get_mem))
			return i.GetAST(node, get_var, get_mem);
	}
	return nullptr;
}

C_value NATVIS::Expressions::Evaluate(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
	for (auto &i : *this) {
		if (i.condition.Test(node, get_var, get_mem)) {
			auto	exp = i.GetAST(node, get_var, get_mem);
			auto	r	= const_fold(exp(node), get_var, get_mem);
			if (r && r->kind == ast::literal)
				return r->cast<ast::lit_node>()->get_value(get_mem);
		}
	}
	return C_value();
}

NATVIS::Expressions::Expressions(const Expressions &exps, const Params &params) {
	for (auto &i : exps)
		emplace_back(i, params);
}

NATVIS::DisplayNode::DisplayNode(const DisplayNode &node, const Params &params) : Node(node, params) {
	string_scan		ss = node.s;
	string_builder	b;
	while (ss.remaining()) {
		char	c = ss.getc();
		if (c == '{' && !ss.check('{')) {
			auto	start	= ss.getp();
			for (int depth = 1; depth;) {
				c = ss.getc();
				if (c == '}')
					--depth;
				if (c == '{')
					++depth;
			}
			entries.emplace_back(move(b), substitute(string_scan(start, ss.getp() - 1), params));
		} else {
			if (c == '}')
				ss.check('}');
			b << c;
		}
	}
	s = b;
}

NATVIS::TypeItem::TypeItem(const TypeItem &item, const Params &params) : name(item.name), has_expand(item.has_expand) {
	for (auto &i : item.display)
		display.emplace_back(i, params);

	for (auto &i : item.stringview)
		stringview.emplace_back(i, params);

	for (auto i : item.items) {
		switch (i->type) {
			case Item::Simple:		items.push_back(new SimpleItem(		(SimpleItem*		)i, params));	break;
			case Item::Array:		items.push_back(new ArrayItems(		(ArrayItems*		)i, params));	break;
			case Item::IndexList:	items.push_back(new IndexListItems(	(IndexListItems*	)i, params));	break;
			case Item::LinkedList:	items.push_back(new LinkedListItems((LinkedListItems*	)i, params));	break;
			case Item::Tree:		items.push_back(new TreeItems(		(TreeItems*			)i, params));	break;
			case Item::CustomList:	items.push_back(new CustomListItems((CustomListItems*	)i, params));	break;
			case Item::Expanded:	items.push_back(new ExpandedItem(	(ExpandedItem*		)i, params));	break;
			case Item::Synthetic:	items.push_back(new SyntheticItem(	(SyntheticItem*		)i, params));	break;
		}
	}
}

bool NATVIS::TypeItem::Dump(string_accum &sa, ast::lit_node *lit, NATVIS *natvis, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
	for (auto &i : display) {
		if (i.condition.Test(lit, get_var, get_mem)) {
			bool	ok = false;
			for (auto &j : i.entries) {
				auto	e = j.b.GetAST(lit, get_var, get_mem, true);
				if (auto r = const_fold(e(lit), get_var, get_mem)) {
					FORMAT::FLAGS	format = FORMAT::NONE;
					if (auto fmt = r->cast<formatted_node>()) {
						format = fmt->format;
						r		= fmt->arg;
					}
					if (auto lit = r->cast<ast::lit_node>()) {
						DumpValue(sa << j.a, lit, format, natvis, get_mem, get_var);
						ok = true;
					} else {
						ISO_TRACEF("error\n");
					}
				}
			}
			if (ok) {
				sa << i.s;
				return true;
			}
		}
	}
	return false;
}

void NATVIS::TypeItem::ExpandAST(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) const {
	for (auto &i : items) {
		if (i->condition.Test(node, get_var, get_mem))
			i->Expand(natvis, all, node, get_var, get_mem);
	}
}

void skip_to(string_scan &s, char end) {
	while (int c = s.getc()) {
		if (c == end)
			return;

		if (end == '"' || end == '\'') {
			if (c == '\\')
				s.getc();
		} else
			switch (c) {
			case '<':	skip_to(s, '>'); break;
			case '[':	skip_to(s, ']'); break;
			case '{':	skip_to(s, '}'); break;
			case '(':	skip_to(s, ')'); break;
			//case ']': case '}': case ')': throw(mismatch_exception(s.getp()));
			case '"': case '\'': skip_to(s, c); break;
		}
	}
	s.move(-1);
}

bool WildcardMatch(string_scan &&s, const char *wild, dynamic_array<string> &params) {
	params.clear();

	while (char c = *(wild = skip_whitespace(wild))++) {
		s.skip_whitespace();
		if (c == '*') {
			auto	start = s.getp();
			skip_to(s, *wild++);
			params.emplace_back(start, s.getp() - 1);

		} else if (s.getc() != c) {
			return false;
		}
	}
	return s.remaining() == 0;
}

NATVIS::Type *NATVIS::Find(const C_type *type, bool root) {
	if (!type)
		return nullptr;

	auto	entry = map[type];
	if (entry.exists())
		return entry;

	string	name;
	DumpType(build(name), type, none, 0);

	dynamic_array<string>	tparams;
	for (auto i = lower_boundc(types, name), e = types.end(); i != e && *i <= name; ++i) {
		if ((root || i->inheritable) && WildcardMatch(name, i->name, tparams))
			return entry = new Type(*i, tparams);
	}

	if (type->type == C_type::STRUCT) {
		for (auto &e : ((C_type_struct*)type)->elements) {
			if (!e.id) {
				if (auto ntype = Find(e.type, false))
					return entry = ntype;
			}
		}
	}

	return entry = nullptr;
}

const NATVIS::Type *NATVIS::Find(const C_type *type) const {
	if (type) {
		auto	entry = map[type];
		if (entry.exists())
			return entry;

		string	name;
		DumpType(build(name), type, none, 0);

		dynamic_array<string>	params;
		for (auto i = lower_boundc(types, name), e = types.end(); i != e && *i <= name; ++i) {
			if (WildcardMatch(name, i->name, params))
				return i;
		}
	}
	return nullptr;
}

void NATVIS::Read(istream_ref in) {
	XMLreader::Data	data;
	XMLreader				xml(in);

	XMLiterator it(xml, data);
	if (!it.Next() || !data.Is("AutoVisualizer"))
		return;

	for (it.Enter(); it.Next();) {
		if (data.Is("Type")) {
			auto	&type = types.emplace_back(data);

			for (it.Enter(); it.Next();) {
				if (data.Is("Version")) {
					type.version_name = data["Name"];
					type.version.a = data["Min"];
					type.version.b = data["Max"];

				} else if (data.Is("DisplayString")) {
					type.display.emplace_back(it);

				} else if (data.Is("StringView")) {
					type.stringview.emplace_back(it);

				} else if (data.Is("Expand")) {
					type.has_expand = true;
					ReadItems(type.items, it);
				}
			}

		} else if (data.Is("HResult")) {
			auto	&hresult = hresults.emplace_back(data);

			for (it.Enter(); it.Next();) {
				if (data.Is("HRValue"))
					from_string(it.Content(), hresult.hr);
				else if (data.Is("HRDescription"))
					hresult.description	= it.Content();
			}

		} else if (data.Is("UIVisualizer")) {
			uivisualizers.emplace_back(data);

		} else if (data.Is("CustomVisualizer")) {
			customvisualizers.emplace_back(data);
		}
	}

	sort(types);
	sort(hresults);
}

template<typename C> alloc_string<C> GetString(uint64 addr, ast::get_memory_t get_mem, uint64 len) {
	alloc_string<C>	s(len + 1);
	get_mem(addr, s, len * sizeof(C));
	return move(s);
}
template<typename C> uint64 GetStringLength(uint64 addr, ast::get_memory_t get_mem) {
	C		buffer[0x100];
	uint64	len		= 0x100;
	uint64	addr1	= addr;

	while (len == 0x100) {
		get_mem(addr1, buffer, sizeof(buffer));
		len		= string_len(make_range<C>(buffer));
		addr1	+= sizeof(buffer);
	}
	return (addr1 - addr) / sizeof(C) + len - 0x100;
}
template<typename C> alloc_string<C> GetString(uint64 addr, ast::get_memory_t get_mem) {
	return GetString<C>(addr, get_mem, GetStringLength<C>(addr, get_mem));
}

void iso::DumpValue(string_accum &sa, uint64 addr, const C_type *type, uint32 flags, FORMAT::FLAGS format, ast::get_memory_t get_mem) {
	bool	ref = !!(flags & ast::ADDRESS);
	if (auto rtype = IsReference(type)) {
		if (ref)
			get_mem(addr, &addr, sizeof(addr));
		ref		= true;
		type	= rtype;
	}

	uint32	array = type->type == C_type::ARRAY ? ((C_type_array*)type)->count : 0;

	if (type->type == C_type::POINTER) {
		if (int size = IsString(type)) {
			if (ref)
				get_mem(addr, &addr, sizeof(addr));

			if (addr) {
				switch (size) {
					case 1:	sa << GetString<char>(addr, get_mem); return;
					case 2:	sa << GetString<char16>(addr, get_mem); return;
					case 4:	sa << GetString<char32>(addr, get_mem); return;
				}
			}
		}
	} else if (array) {
		if (int size = IsString(type)) {
			if (ref)
				get_mem(addr, &addr, sizeof(addr));
			switch (size) {
				case 1:	sa << GetString<char>(addr, get_mem, array); return;
				case 2:	sa << GetString<char16>(addr, get_mem, array); return;
				case 4:	sa << GetString<char32>(addr, get_mem, array); return;
			}
		}
	}

	if (ref || array) {
		if (flags & ast::LOCALMEM) {
			DumpData(sa, (void*)addr, type, 0, 0, format);
		} else {
			size_t	size	= type->size();
			malloc_block	temp(size);
			if (get_mem(addr, temp, size))
				DumpData(sa, temp, type, 0, 0, format);
		}
	} else {
		DumpData(sa, &addr, type, 0, 0, format);
	}
}

bool iso::DumpValue(string_accum &sa, ast::node *node, FORMAT::FLAGS format, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var) {
	if (auto f = node->cast<formatted_node>()) {
		format	= f->format;
		node	= f->arg;
	}

	if (auto lit = node->cast<ast::lit_node>()) {
		if (natvis) {
			if (auto *type = natvis->Find(NotPointer(lit->type))) {
				if (type->Dump(sa, lit, natvis, get_var, get_mem))
					return true;
			}
		}
		DumpValue(sa, lit->v, lit->type, lit->flags, format, get_mem);
		return true;
	}

	if (auto syn = node->cast<SyntheticItem::synthetic_node>())
		return syn->DumpValue(sa, natvis, get_mem, get_var);

	return false;
}

dynamic_array<NATVIS::ExpandedAST> iso::Expand(ast::noderef node, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var) {
	dynamic_array<NATVIS::ExpandedAST> exp;

	if (auto syn = node->cast<SyntheticItem::synthetic_node>()) {
		syn->Expand(natvis, exp, get_var, get_mem);
		return exp;
	}

	const C_type*	type	= node->type;
	if (!type) {
		node = const_fold(node, get_var, get_mem);
		type = node->type;
	}

	if (auto rtype = IsReference(type))
		type = rtype;

	bool	got = false;
	if (natvis) {
		if (auto *ntype = natvis->Find(type)) {
			if (got = ntype->has_expand)
				ntype->ExpandAST(natvis, exp, node, get_var, get_mem);
		}
	}

	if (!got && type) {
		switch (type->type) {
			case C_type::POINTER:
				exp.emplace_back("*", *node);
				break;

			case C_type::ARRAY: {
				auto	*a	= (const C_type_array*)type;
				for (int i = 0; i < a->count; i++)
					exp.emplace_back(format_string("[%i]", i), node[i]);
				break;
			}
			case C_type::STRUCT:
			case C_type::UNION: {
				auto	*a	= (const C_type_composite*)type;
				for (auto &i : a->elements)
					exp.emplace_back(i.id, node[&i]);
			}
		}
	}
	return exp;
}

bool iso::HasChildren(const C_type *type, const NATVIS *natvis) {
	if (type) {
		if (auto *ntype = natvis ? natvis->Find(type) : nullptr) {
			if (ntype->has_expand)
				return !ntype->items.empty();
		}
		type = NotReference(type);
		return type && is_any(type->type, C_type::STRUCT, C_type::UNION, C_type::ARRAY, C_type::POINTER);
	}
	return false;
}

bool iso::HasChildren(ast::node *node, const NATVIS *natvis, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
	if (auto syn = node->cast<SyntheticItem::synthetic_node>())
		return syn->HasChildren();

	return HasChildren(const_fold(node, get_var, get_mem)->type, natvis);
}

struct formatted_node : ast::custom_node {
	static const auto formatted = ast::KIND(ast::custom + 1);
	ast::noderef	arg;
	FORMAT::FLAGS	format;
	static bool check(ast::KIND k) { return k == formatted; }
	formatted_node(ast::node *arg, FORMAT::FLAGS format) : ast::custom_node(formatted), arg(arg), format(format) {}
	virtual node *apply(const callback_ref<node*(node*)> &t) {
		node	*p = apply_preserve(t, arg);
		if (p == arg)
			return this;
		return new formatted_node(p, format);
	}
};

struct castarray_node : ast::custom_node {
	static const auto castarray = ast::KIND(ast::custom + 2);
	ast::noderef	left, right;

	struct caster : ast::unary_node {
		C_type_array	array;
		caster(ast::node *arg, int n) : ast::unary_node(ast::cast, arg, &array), array(arg->type->subtype(), n) {};
	};

	castarray_node(ast::node *left, ast::node *right) : ast::custom_node(castarray), left(left), right(right) {}
	virtual node *apply(const callback_ref<node*(node*)> &t) {
		node	*p = apply_preserve(t, right);
		if (auto lit = p->cast<ast::lit_node>())
			return new caster(apply_preserve(t, left), (uint32)(int64)lit->get_value(none));
		return this;
	}
};

ast::node* iso::ReadFormattedExpression(const char *s, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var) {
	memory_reader				mi(s);
	text_reader<reader_intf>	r(mi);

	auto	node	= ReadCExpression(r, ctypes, scope, get_var);
	if (r.getc() == ',') {
		FORMAT::FLAGS f = FORMAT::NONE;
		switch (char c = r.getc()) {
			case 'd':
				break;

			case 'o':
				f |= FORMAT::OCT;
				break;

			case 'x': case 'h':
				f |= FORMAT::HEX | FORMAT::CFORMAT * (r.getc() != 'b');
				break;
			case 'X': case 'H':
				f |= FORMAT::HEX | FORMAT::UPPER | FORMAT::CFORMAT * (r.getc() != 'b');
				break;
			case 'b':
				f |= FORMAT::BIN | FORMAT::CFORMAT * (r.getc() != 'b');
				break;

			case 'e':
				f |= FORMAT::SCIENTIFIC;
				break;
			case 'g':
				f |= FORMAT::SHORTEST;
				break;
			case 'c':
				break;
			case 's':
				break;

			case '[':
				node = new castarray_node(node, ReadCExpression(r, ctypes, scope, get_var));
				break;
			default:
				if (is_digit(c)) {
					node = new castarray_node(node, new ast::lit_node(read_number(r, c)));
					break;
				}
				break;
		}
		if (f)
			node = new formatted_node(node, f);
	}
	return node;
}


/*
Specifier	Format																																		Original Watch Value												Value Displayed
d			decimal integer																																0x00000066															102
o			unsigned octal integer																														0x00000066															000000000146
x/h			hexadecimal integer																															102																	0xcccccccc
X/H			hexadecimal integer																															102																	0xCCCCCCCC
xb/hb		hexadecimal integer (without leading 0x)																									102																	cccccccc
Xb,Hb		hexadecimal integer (without leading 0x)																									102																	CCCCCCCC
b			unsigned binary integer																														25																	0b00000000000000000000000000011001
bb			unsigned binary integer(without leading 0b)																									25																	00000000000000000000000000011001

e			scientific notation																															25000000															2.500000e+07
g			shorter of scientific or floating point																										25000000															2.5e+07
c			single character																															0x0065, c															101 'e'
s			const char* string (with quotation marks)																									<location> "hello world"											"hello world"
sb			const char* string (no quotation marks)																										<location> "hello world"											hello world
s8			UTF-8 string																																<location> "This is a UTF-8 coffee cu								"This is a UTF-8 coffee cup"
s8b			UTF-8 string (no quotation marks)																											<location> "hello world"											hello world
su			Unicode (UTF-16 encoding) string (with quotation marks)																						<location> L"hello world"											L"hello world"u"hello world"
sub			Unicode (UTF-16 encoding) string (no quotation marks)																						<location> L"hello world"											hello world
bstr		BSTR binary string (with quotation marks)																									<location> L"hello world"											L"hello world"
env			Environment block (double-null terminated string)																							<location> L"=::=::\\"												L"=::=::\\\0=C:=C:\\windows\\system32\0ALLUSERSPROFILE=...
s32			UTF-32 string (with quotation marks)																										<location> U"hello world"											U"hello world"
s32b		UTF-32 string (no quotation marks)																											<location> U"hello world"											hello world
en			enum																																		Saturday(6)															Saturday
hv			Pointer type - indicates that the pointer value being inspected is the result of the heap allocation of an array, for example, new int[3].	<location>{<first member>}											<location>{<first member>, <second member>, ...}
na			Suppresses the memory address of a pointer to an object.																					<location>, {member=value...}										{member=value...}
nd			Displays only the base class information, ignoring derived classes																			(Shape*) square includes base class and derived class information	Displays only base class information
hr			HRESULT or Win32 error code. This specifier is no longer needed for HRESULTs as the debugger decodes them automatically.					S_OK																S_OK
wc			Window class flag																															0x0010																WC_DEFAULTCHAR
wm			Windows message numbers																														16																	WM_CLOSE
nr			Suppress "Raw View" item
nvo			Show "Raw View" item for numeric values only
!			raw format, ignoring any data type views customizations																						<customized representation>											4

Specifier		Format													Original Watch Value	Value Displayed
n				Decimal or hexadecimal integer							pBuffer,[32]
																		pBuffer,[0x20]			Displays pBuffer as a 32 element array.
[exp]			A valid C++ expression that evaluates to an integer.	pBuffer,[bufferSize]	Displays pBuffer as an array of bufferSize elements.
expand(n)		A valid C++ expression that evaluates to an integer		pBuffer, expand(2)		Displays the third element of pBuffer

*/