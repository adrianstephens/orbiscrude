#include "ast.h"

namespace iso { namespace ast {

node *apply_preserve(const callback_ref<node*(node*)> &t, node *p) {
	if (p) {
		if (p->is_unary()) {
			auto	*b	= (unary_node*)p;
			node	*x	= apply_preserve(t, b->arg);
			if (x != b->arg)
				p = new unary_node(p->kind, x, p->type, p->flags);

		} else if (p->is_binary()) {
			auto	*b	= (binary_node*)p;
			node	*x	= apply_preserve(t, b->left);
			node	*y	= apply_preserve(t, b->right);
			if (x != b->left || y != b->right)
				p = new binary_node(p->kind, x, y, b->type, p->flags);

		} else if (p->is_custom()) {
			p = p->cast<custom_node>()->apply(t);

		} else switch (p->kind) {
			case call: {
				auto	*b		= (call_node*)p;
				size_t	num		= b->args.size();
				node**	temp	= alloc_auto(node*, num);
				bool	mod		= false;
				for (auto &i : b->args) {
					auto	x = apply_preserve(t, i);
					mod = mod || (x != i);
					*temp++ = x;
				}

				temp -= num;
				if (p->flags & INLINE) {
					return apply_preserve([&t, temp](ast::node *r) {
						if (r->kind == ast::name && !(r->flags & ast::NOLOOKUP) && r->cast<ast::name_node>()->id == "this")
							r = temp[0];
						return apply_preserve(t, r);
					}, b->func);

				} else {
					auto func = apply_preserve(t, b->func);
					if (mod || (func != b->func)) {
						p = new call_node(func, p->flags);
						b->args = make_range_n(temp, num);
					}
				}

				break;
			}

			case substitute: {
				auto	*b	= (substitute_node*)p;
				node	*x	= apply_preserve(t, b->left);
				return apply_preserve([&t, b, x](ast::node *r) {
					if (r->kind == ast::name && !(r->flags & ast::NOLOOKUP) && r->cast<ast::name_node>()->id == b->id)
						r = x;
					return apply_preserve(t, r);
				}, b->right);
			}

			default:
				break;
		}

		p = t(p);
	}
	return p;
}

noderef const_fold(node *r, get_variable_t get_var, get_memory_t get_mem) {
	return apply_preserve([get_var, get_mem](node *r)->node* {
		if (r->kind == name) {
			if (!(r->flags & NOLOOKUP) && get_var) {
				if (auto *v = get_var(r->cast<name_node>()->id))
					return v;
			}

		} else if (r->is_unary()) {
			auto	*b	= r->cast<unary_node>();
			if (b->kind == pos) {
				return b->arg;

			} else if (b->arg->kind == literal) {
				auto	arg	= b->arg->cast<lit_node>();
				switch (b->kind) {
					case ref:
						if (arg->flags & ADDRESS)
							return new lit_node(arg->type, arg->v, (arg->flags & LOCALMEM) | POINTER);
						return nullptr;

					case deref:
						if (arg->flags & POINTER)
							return new lit_node(arg->type, arg->v, (arg->flags & LOCALMEM) | ADDRESS);
						if (arg->type->type == C_type::POINTER) {
							C_value	x = arg->get_value(get_mem);
							return new lit_node(b->type ? b->type : x.type->subtype(), x.v, (arg->flags & LOCALMEM) | ADDRESS);
						}
						return nullptr;

					case cast:
						if (b->type->type != C_type::POINTER)
							return new lit_node(arg->get_value(get_mem).cast(b->type));

						//fall through
					case castclass: {
						uint32	flags = arg->flags & (ADDRESS | LOCALMEM);
						if (arg->type->type != C_type::POINTER)
							flags &= ~ADDRESS;
						return new lit_node(b->type, arg->v, flags);
					}

					case neg:		return new lit_node(-arg->get_value(get_mem));
					case bit_not:	return new lit_node(~arg->get_value(get_mem));
					case log_not:	return new lit_node(!arg->get_value(get_mem));
				}
			}

		} else if (r->is_binary()) {
			auto	*b	= r->cast<binary_node>();
			if (b->left->kind == literal) {
				auto	left	= b->left->cast<lit_node>();
				switch (b->kind) {
					case field: {
						uint64	data	= left->v;
						auto	type	= left->type;
						if (type->type == C_type::POINTER) {
							type = type->subtype();
							get_mem(data, &data, 8);	//read pointer
						}

						if (b->right->kind == name) {
							int		shift;
							if (type = GetField((void*&)data, type, b->right->cast<name_node>()->id, shift))
								return new lit_node(type, data, ADDRESS | (left->flags & LOCALMEM));

						} else if (b->right->kind == element) {
							auto	e = b->right->cast<element_node>()->element;
							return new lit_node(e->type, data + e->offset, ADDRESS | (left->flags & LOCALMEM));
						}
						return r;
					}

					case ldelem:
						if (b->right->kind == literal) {
							auto	val		= left->get_value(get_mem);
							int64	i		= b->right->cast<lit_node>()->get_value(get_mem);

							if (left->flags & POINTER) {
								return new lit_node(left->type, val.v + i * left->type->size(), ADDRESS | (left->flags & LOCALMEM));

							} else {
								void	*data	= (void*)val.v;
								int		shift;
								if (auto type = Index(data, left->type, (int64)b->right->cast<lit_node>()->get_value(get_mem), shift))
									return new lit_node(type, (uint64)data, ADDRESS | (left->flags & LOCALMEM));
							}
						}
						return r;

					case assign:
						ISO_ASSERT(b->right->kind == ast::literal);
						left->set_value(b->right->cast<lit_node>()->get_value(get_mem));
						return left;
				}

				C_value		x		= left->get_value(get_mem);
				switch (b->kind) {
					case log_and:	return x ? b->right : new lit_node(false);
					case log_or:	return x ? new lit_node(true) : b->right;
					case comma:	return b->right;
					case query:
						if (b->right->kind == colon) {
							b	= b->right->cast<binary_node>();
							return x ? b->left : b->right;
						}
						break;
					default: if (b->right->kind == literal) {
						C_value		y = b->right->cast<lit_node>()->get_value(get_mem);
						switch (b->kind) {
							case add:		return new lit_node(x + y);
							case sub:		return new lit_node(x - y);
							case mul:		return new lit_node(x * y);
							case div:		return new lit_node(x / y);
							case mod:		return new lit_node(x % y);
							case bit_and:	return new lit_node(x & y);
							case bit_or:	return new lit_node(x | y);
							case bit_xor:	return new lit_node(x ^ y);
							case shl:		return new lit_node((int64)x << (int64)y);
							case shr:		return new lit_node((int64)x >> (int64)y);
							case eq:		return new lit_node(compare(x, y) == 0);
							case ne:		return new lit_node(compare(x, y) != 0);
							case lt:		return new lit_node(compare(x, y) <  0);
							case gt:		return new lit_node(compare(x, y) >  0);
							case le:		return new lit_node(compare(x, y) <= 0);
							case ge:		return new lit_node(compare(x, y) >= 0);
							case spaceship:	return new lit_node(compare(x, y));
						}
					}
				}
			}
		}
		return r;

	}, r);
}

bool node::branches(bool always) const {
	switch (kind) {
		case branch:
			return !always || !cast<branch_node>()->cond;
		case jmp:
		case swtch:
		case ret:
		case retv:
		case thrw:
		case end:
		case rethrw:
			return true;
		default:
			return false;
	}
}

basicblock	*basicblock::jump() const {
	auto *b = stmts.back()->cast<branch_node>();
	return b ? b->dest0 : nullptr;
}

void DominatorTree::remove_block(const basicblock *b) {
	for (auto i : incoming(*b)) {
		if (i->stmts.back()->kind == branch) {
			branch_node *cb = (branch_node*)get(i->stmts.back());
			if (cb->dest0 == b) {
				cb->dest0 = 0;
			} else if (cb->dest1 == b) {
				cb->dest1 = 0;
			}
			if (!cb->dest0 && !cb->dest1) {
				unconst(i)->stmts.pop_back();
				if (i->stmts.empty())
					remove_block(i);
			}
		}
	}
}

void DominatorTree::break_branch(basicblock *from, const basicblock *to) const {
	auto	*from_info = get_info(from);
	for (auto i : incoming(*to)) {
		if (dominates(from_info, get_info(i))) {
//		if (dominates(from_info, i->idom)) {
//		if (i->idom == from_info) {
			//ISO_ASSERT(b->stmts.back()->kind == branch);
			if (i->stmts.back()->kind == branch) {
				branch_node	*bra = (branch_node*)get(i->stmts.back());
				//ISO_ASSERT(!bra->cond && bra->dest0 == to);
				if (!bra->cond || bra->dest0 != to)
					break;
				i->remove_edge(bra->dest0);
				unconst(i)->stmts.pop_back();
				if (i->stmts.empty())
					remove_block(i);
				break;
			}
		}
	}
}

basicblock *DominatorTree::make_break_block()	{
	basicblock	*b = new basicblock(-1);
	b->add(new node(breakstmt));
	return b;
}

basicblock *DominatorTree::make_continue_block()	{
	basicblock	*b = new basicblock(-2);
	b->add(new node(continuestmt));
	return b;
}

ifelse_node *DominatorTree::make_if(node *cond, basicblock *blockt) {
	// check for &&
	if (blockt->stmts.front()->kind == ifelse && blockt->stmts.size() == 1) {
		ifelse_node *ifelse	= (ifelse_node*)get(blockt->stmts.front());
		ifelse->cond	= new binary_node(log_and, cond, ifelse->cond);
		return ifelse;
	}
	return new ifelse_node(cond, blockt, 0);
}

void DominatorTree::convert_loop(basicblock *header, hash_set_with_key<const basicblock*> &back, node *init, node *update) {
	auto		&last	= header->stmts.back();
//	ISO_ASSERT(last->kind == ast::branch);
	if (last->kind != branch)
		return;

	branch_node	*b		= (branch_node*)get(last);
	node		*cond	= b->cond;
	basicblock	*blockf	= b->dest0;
	basicblock	*blockt	= b->dest1;	// in loop

	bool	in_loop	= false;
	for (auto &j : back) {
		if (in_loop = dominates(blockt, j))
			break;
	}
	if (!in_loop) {
		swap(blockf, blockt);
		cond	= cond->flip_condition();
	}

	node	*newnode;
	if (init) {
		newnode = new for_node(init, cond, update, blockt);
	} else {
		newnode = new while_node(cond, blockt);
	}
	header->stmts.insert(&last, newnode);
	b->set(blockf);

	for (auto &j : back) {
		if (j->stmts.back()->kind == branch) {
			branch_node	*bra	= (branch_node*)get(j->stmts.back());
			if (bra->cond) {
				bool	which	= bra->dest1 == header;
				auto	jm		= unconst(j);
				jm->stmts.insert(jm->stmts.end() - 1,
					new ifelse_node(which ? get(bra->cond) : bra->cond->flip_condition(), make_continue_block(), 0)
				);
				bra->set(which ? bra->dest0 : bra->dest1);

			} else {
				if (back.size() == 1)
					unconst(j)->stmts.pop_back();
				else
					bra->dest0	= make_continue_block();

			}

			unconst(j)->remove_edge(header);
		}
	}
}

void DominatorTree::convert_ifelse(basicblock *header) {
	auto		&last	= header->stmts.back();
	branch_node	*b		= (branch_node*)get(last);
	node		*cond	= b->cond;
	basicblock	*blockf	= b->dest0;
	basicblock	*blockt	= b->dest1;

	if (!blockt || !blockf) {
		if (!blockt) {
			blockt	= blockf;
			cond	= cond->flip_condition();
		}
		last = make_if(cond, blockt);
		header->remove_edge(blockt);
		return;

	}

	if (ifelse_node *i = blockt->stmts.front()->cast<ifelse_node>()) {
		if (i->has(blockf) || (!i->blockf && blockt->jump() == blockf)) {
//			i->cond	= new binary_node(log_and, cond->flip_condition(i->blockt == blockf), i->cond);
			i->cond	= new binary_node(log_and, cond, i->cond->flip_condition(i->blockt != blockf));
			last	= i;
			//header->stmts.append(slice(blockt->stmts, 1));
			header->remove_edge(blockf);
			header->merge(blockt);
			return;
		}
	}

	if (ifelse_node *i = blockf->stmts.front()->cast<ifelse_node>()) {
		if (i->has(blockt) || (!i->blockf && blockf->jump() == blockt)) {
			i->cond	= new binary_node(log_or, cond, i->cond->flip_condition(i->blockt != blockt));
			last	= i;
			header->stmts.append(slice(blockf->stmts, 1));
			header->remove_edge(blockt);
			header->merge(blockf);
			return;
		}
	}

	auto	frontt	= dominance_frontier(blockt);
	auto	frontf	= dominance_frontier(blockf);
	auto	joins	= frontt & frontf;

	if (!joins.empty()) {
		auto	*join	= *joins.begin();
		if (join == blockt || join == blockf) {
			if (join == blockt) {
				swap(blockf, blockt);
				cond	= cond->flip_condition();
			}
			header->stmts.insert(&last, make_if(cond, blockt));
			b->set(blockf);
			break_branch(blockt, blockf);

		} else {
			break_branch(blockt, join);
			break_branch(blockf, join);

			header->stmts.insert(&last, new ifelse_node(cond, blockt, blockf));
			header->add_edge(unconst(join));
			b->set(unconst(join));

			header->remove_edge(blockf);
		}

	} else {
		bool	noelse;
		if (noelse = frontf.count(blockt)) {
			swap(blockf, blockt);
			cond	= cond->flip_condition();

		} else if (!(noelse = frontt.count(blockf))) {
			if (noelse = blockf->stmts.back()->branches()) {
				swap(blockf, blockt);
				cond	= cond->flip_condition();
			} else {
				noelse	= blockt->stmts.back()->branches();
			}
		}

		if (noelse) {
			header->stmts.insert(&last, make_if(cond, blockt));
			b->set(blockf);
			break_branch(blockt, blockf);

		} else {
			last = new ifelse_node(cond, blockt, blockf);
			header->remove_edge(blockf);
		}

	}
	header->remove_edge(blockt);
}

void DominatorTree::convert_switch(basicblock *header) {
	auto		&last	= header->stmts.back();
	switch_node	*b		= (switch_node*)get(last);
	auto		front	= dominance_frontier(b->targets.front());
	front.insert(b->targets.front());

	for (auto &i : slice(b->targets, 1)) {
		auto	df = dominance_frontier(i);
		df.insert(i);
		front &= df;
		if (front.empty())
			break;
	}

	if (!front.empty()) {
		auto	*join	= *front.begin();
		header->add(unconst(join));

		for (auto &i : incoming(*join)) {
			if (dominates(header, i)) {
				auto	&s = i->stmts.back();
				if (s->kind == branch && ((branch_node*)get(s))->dest0 == join) {
					unconst(s) = new node(breakstmt);
				}
			}
		}
		for (auto &i : b->targets) {
			if (join == i) {
				auto	deflt = b->targets.back();
				if (deflt->stmts.size() == 1 && deflt->stmts[0]->kind == breakstmt) {
					//use default
					i = 0;
				} else {
					i = make_break_block();
				}
			} else {
				//break_branch(dt, i, join);
				for (auto &j : dominance_frontier(i)) {
					auto	&s = j->stmts.back();
					if (s->kind == branch) {
						branch_node	*b = (branch_node*)unconst(get(s));
						if (b->dest0 == join)
							unconst(s) = new node(breakstmt);
					}

				}
			}
		}
		b->flags |= FIXED;
	}
}

void DominatorTree::convert_return(basicblock *header) {
	if (dominance_frontier(header).empty())
		header->stmts.pop_back();
}

} }
