#ifndef NATVIS_H
#define NATVIS_H

#include "filename.h"
#include "extra/xml.h"
#include "extra/ast.h"

namespace iso {

extern		C_types ctypes, user_ctypes;

struct NATVIS {
	typedef range<string*>	Params;

	enum Priority { Low, MediumLow, Medium, MediumHigh, High	};

	struct ExpandedAST {
		string			id;
		ast::noderef	ast;
		ExpandedAST(const char *id, ast::node *ast) : id(id), ast(ast) {}
	};

	class Expression {
		string			s;
		ast::noderef	ast;
	public:
		Expression() {}
		Expression(const count_string &s)	: s(s) {}
		Expression(const char *s)			: s(s) {}
		Expression(const Expression &exp, const Params &params);

		ast::noderef	GetAST(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem, bool formatted = false);

		C_value Evaluate(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
			auto	exp	= GetAST(node, get_var, get_mem);
			auto	r	= const_fold(exp(node), get_var, get_mem);
			return r && r->kind == ast::literal ? r->cast<ast::lit_node>()->get_value(get_mem) : C_value();
		}
		bool Test(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {
			if (!s)
				return true;
			auto	exp	= GetAST(node, get_var, get_mem);
			auto	r	= const_fold(exp(node), get_var, get_mem);
			return r && r->kind == ast::literal && r->cast<ast::lit_node>()->get_value(get_mem);
		}
	};
	struct Node {
		Expression	condition;
		bool		optional;
		Node(const XMLreader::Data &data) : condition(data.Find("Condition")), optional(data["Optional"]) {}
		Node(const Node &node, const Params &params) : condition(node.condition, params), optional(node.optional) {}
	};
	struct ExpressionNode : Node, Expression {
		ExpressionNode(XMLiterator &it) : Node(it.data), Expression(it.Content()) {}
		ExpressionNode(const ExpressionNode &node, const Params &params) : Node(node, params), Expression(node, params) {}
	};
	struct Expressions : dynamic_array<ExpressionNode> {
		C_value			Evaluate(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem);
		ast::noderef	Find(ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem);
		Expressions() {}
		Expressions(const Expressions &exps, const Params &params);
	};
	struct NodeIncExc : Node {
		string	inc, exc;
		NodeIncExc(const XMLreader::Data &data) : Node(data)
			, inc(data["IncludeView"])
			, exc(data["ExcludeView"]) {}
		NodeIncExc(const NodeIncExc &node, const Params &params) : Node(node, params), inc(node.inc), exc(node.exc) {}
	};

	struct Item : NodeIncExc {
		enum TYPE { Simple, Array, IndexList, LinkedList, Tree, CustomList, Expanded, Synthetic };
		TYPE	type;
		Item(TYPE type, const XMLreader::Data &data) : NodeIncExc(data), type(type) {}
		Item(const Item *item, const Params &params) : NodeIncExc(*item, params), type(item->type) {}
		virtual ~Item()	{};
		virtual void	Expand(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) {}
	};

	struct DisplayNode : Node {
		string	s;
		dynamic_array<pair<string, Expression>>	entries;
		DisplayNode(XMLiterator &it) : Node(it.data), s(it.Content()) { }
		DisplayNode(const DisplayNode &node, const Params &params);
	};

	struct TypeItem {
		string		name;
		bool		has_expand;

		dynamic_array<DisplayNode>		display;
		dynamic_array<ExpressionNode>	stringview;
		dynamic_array<Item*>			items;

		TypeItem(const XMLreader::Data &data) : name(data["Name"]), has_expand(false) {}
		TypeItem(const TypeItem &type, const Params &params);
		bool	Dump(string_accum &sa, ast::lit_node *lit, NATVIS *natvis, ast::get_variable_t get_var, ast::get_memory_t get_mem);
		void	ExpandAST(NATVIS *natvis, dynamic_array<ExpandedAST> &all, ast::node *node, ast::get_variable_t get_var, ast::get_memory_t get_mem) const;
	};

	struct Type : NodeIncExc, TypeItem {
		bool			inheritable;
		Priority		priority;
		string			version_name;
		interval<int>	version;

		Type(const XMLreader::Data &data);
		Type(const Type &type, const Params &params) : NodeIncExc(type, params), TypeItem(type, params), inheritable(type.inheritable) {}
		bool operator<(const char *s) const {
			if (auto w	= name.find('*'))
				return memcmp(name, s, w - name) < 0;
			return name < s;
		}
		bool operator<=(const char *s) const {
			if (auto w	= name.find('*'))
				return memcmp(name, s, w - name) <= 0;
			return name <= s;
		}
		bool operator<(const Type &b) const {
			return priority > b.priority || (priority == b.priority && name < b.name);
		}
	};

	struct HResult : Node {
		string		name;
		HRESULT		hr;
		string		description;
		HResult(const XMLreader::Data &data) : Node(data), name(data["Name"]) {}
		bool operator<(const HResult &b) const {
			return hr < b.hr;
		};
	};

	struct UIVisualizer : Node {
		GUID		service;
		int			id;
		UIVisualizer(const XMLreader::Data &data) : Node(data), service(data["Service"]), id(data["Id"]) {}
	};

	struct CustomVisualizer : Node {
		CustomVisualizer(const XMLreader::Data &data) : Node(data) {}
	};

	dynamic_array<Type>				types;
	dynamic_array<HResult>			hresults;
	dynamic_array<UIVisualizer>		uivisualizers;
	dynamic_array<CustomVisualizer>	customvisualizers;

	hash_map<const C_type*, Type*>	map;

	void				Read(istream_ref in);

	Type*		Find(const C_type *type, bool root = true);
	const Type*	Find(const C_type *type) const;
};

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


void DumpValue(string_accum &sa, uint64 addr, const C_type *type, uint32 flags, FORMAT::FLAGS format, ast::get_memory_t get_mem);
bool DumpValue(string_accum &sa, ast::node *node, FORMAT::FLAGS format, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var);
dynamic_array<NATVIS::ExpandedAST> Expand(ast::noderef node, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var);
bool HasChildren(const C_type *type, const NATVIS *natvis);
bool HasChildren(ast::node *node, const NATVIS *natvis, ast::get_variable_t get_var, ast::get_memory_t get_mem);
ast::node* ReadFormattedExpression(const char *s, C_types &types, const C_type_composite *scope, ast::get_variable_t get_var);

}// namespace iso
#endif //NATVIS_H
