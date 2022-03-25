#include "base/tree.h"
#include "base/algorithm.h"
#include "extra/ast.h"

using namespace iso;

namespace iso {
string			demangle_vc(const char* mangled, uint32 flags);
string_accum&	demangle_vc(string_accum &a, const char* mangled, uint32 flags);
}

//-----------------------------------------------------------------------------
//	Dumper
//-----------------------------------------------------------------------------

fixed_string<1024> demangle_name(const char *s) {
	fixed_string<1024>	r;
	if (const char *q = string_find(s, '?')) {
		demangle_vc(lvalue(fixed_accum(r)), q, 0x7f);
		s = r;
	}
	char	*d = r;
	for (;;) {
		const char *e;
		if (s[0] == '<') {
			e = string_find(++s, '>');
			memcpy(d, s, e - s);
			d += e - s;
			++e;
		} else {
			e = string_find(s, char_set(".:"));
			if (!e)
				e = s + strlen(s);
			memcpy(d, s, e - s);
			d += e - s;
		}
		if (!(*d++ = *e++))
			break;
		if (*e == ':')
			*d++ = *e++;
		s = e;
	}
	return r;
}

bool Dumper::DumpCPP(string_accum &a, const ast::node *p, int precedence) {
	bool	statement = false;
	if (p->is_plain()) {
		auto			*i	= ((ast::plain_node*)p)->get_info();
		a << i->name;
		return i->statement;

	} else if (p->is_unary()) {
		ast::unary_node	*b	= (ast::unary_node*)p;
		auto			*i	= b->get_info();
		a << i->name;

		if (p->type) {
			if (p->kind == ast::deref)
				a << "static_cast";
			a << '<';
			DumpType(a, p->type);
			if (p->kind == ast::load || p->kind == ast::deref)
				a << '*';
			a << '>';
		}

		DumpCPP(a << parentheses(i->parentheses || i->precedence < precedence), b->arg, i->precedence);
		return i->statement;

	} else if (p->is_binary()) {
		ast::binary_node	*b	= (ast::binary_node*)p;
		auto				*i	= b->get_info();

		switch (p->kind) {
  			case ast::ldelem: {
				ast::binary_node	*b = (ast::binary_node*)p;
				DumpCPP(a, b->left);
				a << '[';
				DumpCPP(a, b->right);
				a << ']';
				return false;
			}
			case ast::assignobj: {
				ast::binary_node	*b = (ast::binary_node*)p;
				DumpCPP(a, b->left);
				a << " = ";
				DumpCPP(a, b->right);
				return true;
			}
			case ast::copyobj: {
				ast::binary_node	*b = (ast::binary_node*)p;
				a << "copyobj(";
				DumpCPP(a, b->left);
				a << ", ";
				DumpCPP(a, b->right);
				a << ")";
				return true;
			}
			case ast::field:
				if (is_this(b->left)) {
					DumpCPP(a, b->right, precedence);
				} else if (b->left->kind == ast::deref) {
					DumpCPP(a, ((ast::unary_node*)get(b->left))->arg);
					a << "->";
					save(use_scope, false), DumpCPP(a, b->right, precedence);
				} else {
					DumpCPP(a, b->left);
					a << ".";
					save(use_scope, false), DumpCPP(a, b->right, precedence);
				}
				return false;

			default: {
				DumpCPP(a << parentheses(i->precedence < precedence), b->left, i->precedence),
				DumpCPP(i->spaces ? (a << ' ' << i->name << ' ') : (a << i->name), b->right, i->precedence);
				return i->statement;
			}
		}

	} else switch (p->kind) {
		case ast::literal: {
			ast::lit_node	*b = (ast::lit_node*)p;
			DumpData(a, &b->v, b->type);
			break;
		}

		case ast::name:
			a << demangle_name(((ast::name_node*)p)->id);
			break;

		case ast::get_size:
			a << "sizeof(" << p->type << ')';
			break;

		//element
		case ast::element:
		case ast::var:
		case ast::func:
		case ast::vfunc: {
			const ast::element_node	*b = p->cast();
			if (use_scope && b->parent && b->parent != scope)
				a << ((C_type_composite*)b->parent)->id << "::";
			a << b->element->id;
			break;
		}

		case ast::decl: {
			ast::element_node	*b = (ast::element_node*)p;
			DumpType(a, b->element->type, b->element->id);
			return true;
		}
		case ast::jmp:
			a << "jump " << ((ast::element_node*)p)->element->id;
			return true;

		case ast::call: {
			ast::call_node	*c = (ast::call_node*)p;

			if (c->func->kind == ast::element) {
				ast::element_node	*en = (ast::element_node*)get(c->func);
				const C_element		*e	= en->element;

				if (c->flags & ast::ALLOC) {
					a << "new ";
					DumpType(a, en->parent);
					DumpParams(a, make_rangec(c->args));
					break;
				}

				if (e->id[0] == '.') {
					if (e->id == ".ctor") {
						if (c->flags & ast::ALLOC) {
							a << "new ";
							DumpType(a, en->parent);
							DumpParams(a, make_rangec(c->args));
						} else {
							ast::node	*dest = c->args[0];
							if (dest->kind == ast::ref) {
								DumpCPP(a, ((ast::unary_node*)dest)->arg);
								a << " = ";
							}
							DumpType(a, en->parent);
							DumpParams(a, sub_range(c->args, 1));
						}
						break;
					}

				}
				if (((C_type_function*)e->type)->has_this()) {
					if (!is_this(c->args[0])) {
						if (c->args[0]->kind == ast::ref) {
							DumpCPP(a, c->args[0]->cast<ast::unary_node>()->arg);
							a << '.';
						} else {
							DumpCPP(a, c->args[0]);
							a << "->";
						}
					}
					save(use_scope, false),  DumpCPP(a, c->func);
					DumpParams(a, sub_range(c->args, 1));
					break;
				}
			}

			DumpCPP(a, c->func);
			DumpParams(a, make_rangec(c->args));
			break;
		}

		case ast::basic_block: {
			ast::basicblock	*bb = (ast::basicblock*)p;
			string	&name = named[bb];
			newline(a);
			if (name) {
				a << "goto " << name;
				return true;
			} else {
				if (/*true || */bb->shared()) {
					name = format_string("block%i", bb->id);
					a << name << ":";
				}
				for (auto i : bb->stmts) {
					newline(a);
					if (DumpCPP(a, i))
						a << ';';
				}
			}
			break;
		}

		case ast::branch: {
			ast::branch_node	*b = (ast::branch_node*)p;
			if (b->cond) {
				DumpCPP(a << "if " << parentheses(), b->cond);
				scoped(a << ' '), DumpCPP(a, b->dest1);
				scoped(a << " else"), DumpCPP(a, b->dest0);
			} else {
				DumpCPP(a, b->dest0);
			}
			break;
		}
		case ast::swtch: {
			ast::switch_node	*s = (ast::switch_node*)p;
			DumpCPP(a << "switch " << parentheses(), s->arg);
			auto			sa		= scoped(a << ' ');
			int				n		= s->targets.size32();
			const C_type	*type	= s->arg->type;
			for (int i = 0; i < n - 1; ++i) {
				if (s->targets[i]) {
					newline(a) << "case ";
					if (type)
						DumpData(a, &i, type);
					else
						a << i;
					scoped(a << ": "), DumpCPP(a, s->targets[i]);
				}
			}

			scoped(newline(a) << "default: "), DumpCPP(a, s->targets[n - 1]);
			break;
		}
		case ast::whileloop: {
			ast::while_node	*b = (ast::while_node*)p;
			DumpCPP(a << "while " << parentheses(), b->cond);
			scoped(a << ' '), DumpCPP(a, b->block);
			break;
		}

		case ast::forloop: {
			ast::for_node	*b = (ast::for_node*)p;
			a << "for (";
			if (b->init)
				DumpCPP(a, b->init);
			a << "; ";
			if (b->cond)
				DumpCPP(a, b->cond);
			a << "; ";
			if (b->update)
				DumpCPP(a, b->update);
			scoped(a << ") "), DumpCPP(a, b->block);
			break;
		}

		case ast::ifelse: {
			ast::ifelse_node	*b = (ast::ifelse_node*)p;
			DumpCPP(a << "if " << parentheses(), b->cond);
			scoped(a << ' '), DumpCPP(a, b->blockt);
			if (b->blockf)
				scoped(a << " else"), DumpCPP(a, b->blockf);
			break;
		}

		default:
			a << "<unknown>";
			break;
	}
	return false;
}
