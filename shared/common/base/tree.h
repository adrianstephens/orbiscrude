#ifndef TREE_H
#define TREE_H

#include "defs.h"
#include "interval.h"

namespace iso {

template<typename N, typename P, bool PARENT>	struct treenode_base;
template<typename N, bool PARENT>				struct _tree_traverser;
template<typename N, bool PARENT = N::PARENT>	using tree_traverser = typename _tree_traverser<N, PARENT>::type;

//-----------------------------------------------------------------------------
// treenode_base/tree_base (no parent)
//-----------------------------------------------------------------------------

template<typename N, typename P> struct treenode_base<N, P, false> {
	static const bool PARENT = false;
	P		child[2]	= { nullptr, nullptr };

	template<typename N2> void	attach(N2 n, bool right) { child[right] = n; }
	N*		make_root()					{ return static_cast<N*>(this); }
	N*		other(const N *n)	const	{ return child[child[0] == n]; }
	bool	side(const N *n)	const	{ return child[1] == n; }

	N*		last(bool right) const {
		auto	n = unconst(static_cast<const N*>(this));
		while (N *c = n->child[right])
			n = c;
		return n;
	}

	N*		rotate(bool right) {
		N	*n	= static_cast<N*>(this);
		N	*p	= child[!right];
		n->attach(p->child[right], !right);
		p->attach(n, right);
		return p;
	}

	N*		rotate2(bool right) {
		N	*n	= static_cast<N*>(this);
		n->attach(child[!right]->rotate(!right), !right);
		return n->rotate(right);
	}

	N*		remove() {
		N	*n0 = child[0], *n1 = child[1];
		if (!n1)
			return n0;
		if (!n0)
			return n1;

		N	*n = n1, *p = nullptr;
		while (N *c = n->child[false]) {
			p	= n;
			n	= c;
		}
		if (p) {
			p->attach(n->child[true], false);
			n->attach(n1, true);
		}
		n->attach(n0, false);
		return n;
	}

	size_t	subtree_size() const {
		size_t	size = 1;
		if (N *n = child[false])
			size += n->subtree_size();
		if (N *n = child[true])
			size += n->subtree_size();
		return size;
	}

	N*		subtree_clone() const {
		N	*n	= new N(*static_cast<const N*>(this));
		if (N *c = child[false])
			n->attach(c->subtree_clone(), false);
		if (N *c = child[true])
			n->attach(c->subtree_clone(), true);
		return n;
	}

	void	subtree_delete() {
		for (N *i = static_cast<N*>(this), *n; i; i = n) {
			if (n = i->child[0]) {
				i->child[0] = n->child[1];
				n->child[1] = i;
			} else {
				n = i->child[1];
				delete i;
			}
		}
	}

	void	swap_contents(N *r) {
		swap(*r, *static_cast<N*>(this));				//swap everything
		swap(*static_cast<treenode_base*>(r), *this);	//swap back pointers, etc
	}

	template<typename I> static N* remove(I &i) {
		N	*n = i.node();
		if (n) {
			if (N *p = i.parent_node())
				p->attach(n->remove(), p->side(n));
			else 
				return n->remove();
		}
		return (N*)1;
	}

	friend bool validate(const treenode_base *n, int depth) {
		return !n || (n->valid() && depth && validate(n->child[0], depth - 1) && validate(n->child[1], depth - 1));
	}
	template<typename C> friend bool validate(const treenode_base *n, const C &comp, int depth) {
		return !n || (n->valid() && n->valid_order(comp) && depth && validate(n->child[0], comp, depth - 1) && validate(n->child[1], comp, depth - 1));
	}

	constexpr bool	valid()			const {	return true; }
	constexpr bool	valid_root()	const {	return true; }
	template<typename C> constexpr bool	valid_order(const C &comp)	const {
		return	!(child[0] && comp(static_cast<const N*>(this)->key(), child[0]->key()))
			&&	!(child[1] && comp(child[1]->key(), static_cast<const N*>(this)->key()));
	}
};

template<typename N> class tree_stackptr {
protected:
	N		**sp;

	tree_stackptr(N **stack) : sp(stack)	{}

	template<typename K, typename C> void scan_lower_bound(const K &k, const C &comp) {
		N		**best	= --sp;
		bool	right;
		for (N *n = *sp; n; n = n->child[right]) {
			push(n);
			if (!(right = comp(n->key(), k)))
				best = sp;
		}
		sp = best;
	}
	template<typename K, typename C> void scan_upper_bound(const K &k, const C &comp) {
		N		**best	= --sp;
		bool	right;
		for (N *n = *sp; n; n = n->child[right]) {
			push(n);
			if (!(right = !comp(k, n->key())))
				best = sp;
		}
		sp = best;
	}
	template<typename K, typename C> int scan_insertion(const K &k, const C &comp) {
		N		**best	= nullptr;
		bool	right	= false;
		for (N *n = *--sp; n; n = n->child[right]) {
			push(n);
			if (right = !comp(k, n->key()))
				best = sp;
		}
		if (best && !comp(best[-1]->key(), k)) {
			sp = best;
			return 0;
		}
		return right ? 1 : -1;
	}

	void	last(N *p, bool right) {
		while (p) {
			push(p);
			p = p->child[right];
		}
	}

public:
	typedef N	node_type;

	void	push(N *n) {
		*sp++ = n;
	}
};

template<typename N, int D=32> class tree_stack : public tree_stackptr<N> {
	typedef tree_stackptr<N> B;
	N		*stack[D];
public:
	using	B::sp;

	tree_stack(N *p = nullptr)		noexcept : B(stack) { *sp = p; sp += !!p; }
	tree_stack(const tree_stack &b)	noexcept : B(stack) {
		for (N *const *i = b.stack, **e = b.sp; i != e; ++i)
			B::push(*i);
	}
	tree_stack& operator=(const tree_stack &b)	noexcept {
		sp = stack;
		for (N *const *i = b.stack, **e = b.sp; i != e; ++i)
			B::push(*i);
		return *this;
	}

	void	clear()		noexcept		{ sp = stack; }

	bool	operator==(const tree_stack &b) const	{ return node() == b.node(); }
	bool	operator!=(const tree_stack &b) const	{ return node() != b.node(); }

	N*		node()				const	{ return sp > stack ? sp[-1] : nullptr; }
	bool	side(const N *n)	const	{ return sp < stack + 1 || sp[-1]->side(n); }
	N*		pop()						{ return sp > stack ? *--sp : nullptr; }

	N*		parent_node()		const	{ return sp > stack + 1 ? sp[-2] : nullptr; }
	N*		parent_pop()				{ --sp; return node(); }	//equiv to p = p->parent

	int		depth()				const	{ return sp - stack; }
	int		depth(N **p)		const	{ return p - stack; }

	N*		operator[](int i)	const	{ return stack[i]; }
	N*&		operator[](int i)			{ return stack[i]; }

	void	last(bool right) {
		if (sp > stack)
			for (auto p = sp[-1]; p = p->child[right];)
				push(p);
	}
	void	next(bool right) {
		if (N *p = node()) {
			if (N *n = p->child[right]) {
				B::last(n, !right);

			} else {
				N	*c;
				do {
					c = p;
					p = parent_pop();
				} while (p && c == p->child[right]);
			}
		}
	}


	N*		move_to_leaf(bool right) {
	#if 1
		typedef noref_t<decltype(sp[0]->child[0])>	N2;
		auto	p = node();
		if (auto n = p->child[right]) {
			auto	s	= exchange(p->child[!right], N2(nullptr));
			N		*r	= n;
			N		*pp = parent_node();

			if (!n->child[!right]) {
				p->attach(r->child[right], right);
				r->attach(s, !right);
				r->child[right] = n;	//copy colour
				r->attach(p, right);
				sp[-1]	= r;
				push(p);

			} else {
				auto	sp0 = sp;
				push(n);
				last(!right);
				r	= node();
				p->attach(r->child[right], right);
				r->attach(n, right);
				r->attach(s, !right);
				N		*rp = parent_node();
				rp->attach(p, rp->side(r));

				sp0[-1]	= r;
				sp[-1]	= p;
			}

			if (!pp)
				return r;	// new root

			pp->attach(r, pp->side(p));
		}
	#else
		N	*p	= node();
		if (auto n = p->child[right]) {
			B::last(n, !right);
			p->swap_contents(node());
		}
	#endif
		return nullptr;	// no new root
	}

	template<typename K, typename C> void scan_lower_bound(const K &k, const C &comp) noexcept {
		if (sp > stack)
			B::scan_lower_bound(k, comp);
	}
	template<typename K, typename C> void scan_upper_bound(const K &k, const C &comp) noexcept {
		if (sp > stack)
			B::scan_upper_bound(k, comp);
	}
	template<typename K, typename C> int scan_insertion(const K &k, const C &comp) noexcept {
		return sp > stack ? B::scan_insertion(k, comp) : -1;
	}
};

template<typename N> struct _tree_traverser<N, false> : T_type<tree_stack<N>> {};

//-----------------------------------------------------------------------------
// treenode_base/tree_base (with parent)
//-----------------------------------------------------------------------------

template<typename N, typename P> struct treenode_base<N, P, true> : treenode_base<N, P, false> {
	static const bool PARENT = true;
	typedef	treenode_base<N, P, false>	B;
	using	B::child;
	P		parent	= nullptr;

	N*		make_root() {
		parent = nullptr;
		return static_cast<N*>(this);
	}
	template<typename N2>	void attach(N2 n, bool right) {
		if (child[right] = n)
			n->parent = static_cast<N*>(this);
	}
	N*		sibling() const {
		return parent->other(static_cast<const N*>(this));
	}
	void	subtree_delete() {
		N	*n = static_cast<N*>(this), *c;
		while (n) {
			while (c = n->child[1])
				n = c->last(false);

			do {
				c = n;
				n = n->parent;
				delete c;
			} while (n && n->child[1] == c);
		}
	}

	void	swap_contents(N *r) {
		swap(*r, *static_cast<N*>(this));				//swap everything
		swap(*static_cast<treenode_base*>(r), *this);	//swap back pointers, etc
	}

	constexpr bool	valid()			const { return (!child[0] || child[0]->parent == this) && (!child[1] || child[1]->parent == this); }
	constexpr bool	valid_root()	const { return !parent; }
};

template<typename N> class tree_pointer {
	N		*p;
public:
	typedef N	node_type;

	tree_pointer(N *p) : p(p)			{}
	void	clear()						{ p = nullptr; }
	bool	operator==(const tree_pointer &b) const	{ return p == b.p; }
	bool	operator!=(const tree_pointer &b) const	{ return p != b.p; }

	N*		node()				const	{ return p; }
	int		side(const N *n)	const	{ return !p || p->side(n); }
	N*		pop()						{ return p ? exchange(p, p->parent) : p; }

	N*		parent_node()		const	{ return p->parent; }
	N*		parent_pop()				{ return p = p->parent; }
	
	void	push(N *n)					{ p = n; }

	void	last(bool right) {
		if (p)
			p = p->last(right);
	}

	void	next(bool right) {
		if (N *n = p->child[right]) {
			p = n->last(!right);

		} else {
			N	*c;
			n	= p;
			do {
				c = n;
				n = n->parent;
			} while (n && c == n->child[right]);
			p = n;
		}
	}

	N*	move_to_leaf(bool right) {
	#if 1
		typedef noref_t<decltype(p->child[0])>	N2;
		if (auto n = p->child[right]) {
			auto	s	= exchange(p->child[!right], N2(nullptr));
			N		*r	= n;
			N		*pp = p->parent;

			if (!n->child[!right]) {
				p->attach(r->child[right], right);
				r->attach(s, !right);
				r->child[right] = n;	//get colour
				r->attach(p, right);

			} else {
				r	= n->last(!right);
				p->attach(r->child[right], right);
				r->attach(n, right);
				r->attach(s, !right);
				N	*rp = r->parent;
				rp->attach(p, rp->side(r));
			}

			if (!pp) {
				r->parent = nullptr;
				return r;	// new root
			}
			pp->attach(r, pp->side(p));
		}
	#else
		N	*r	= p;
		if (auto n = r->child[right]) {
			p = n->last(!right);
			r->swap_contents(p);
		}
	#endif
		return nullptr;	// no new root
	}

	template<typename K, typename C> void scan_lower_bound(const K &k, const C &comp) {
		N		*best	= nullptr;
		bool	right;
		for (N *n = p; n; n = n->child[right]) {
			if (!(right = comp(n->key(), k)))
				best = n;
		}
		p = best;
	}
	template<typename K, typename C> void scan_upper_bound(const K &k, const C &comp) {
		N		*best	= nullptr;
		bool	right;
		for (N *n = p; n; n = n->child[right]) {
			if (!(right = !comp(k, n->key())))
				best = n;
		}
		p = best;
	}
	template<typename K, typename C> int scan_insertion(const K &k, const C &comp) {
		N		*best	= nullptr;
		N		*last	= nullptr;
		bool	right;
		for (N *n = p; n; n = n->child[right]) {
			last = n;
			if (right = !comp(k, n->key()))
				best = n;
		}
		if (best && !comp(best->key(), k)) {
			p = best;
			return 0;
		}
		p = last;
		return right ? 1 : -1;;
	}
};

template<typename N> struct _tree_traverser<N, true> : T_type<tree_pointer<N>> {};

//-----------------------------------------------------------------------------
// tree_base
//-----------------------------------------------------------------------------

template<typename N> class tree_iterator : public tree_traverser<N> {
	typedef tree_traverser<N>	I;
public:
	tree_iterator(N *p) : I(p)				{}
	explicit operator bool()		const	{ return !!I::node(); }
	auto&			operator*()		const	{ return *I::node(); }
	auto			operator->()	const	{ return I::node(); }
	auto&			operator++()			{ I::next(true); return *this;	}
	auto			operator++(int)			{ auto i = *this; I::next(true); return i; }
	auto&			operator--()			{ I::next(false); return *this;	}
	auto			operator--(int)			{ auto i = *this; I::next(false); return i; }
};

template<typename N, bool PARENT = N::PARENT> class tree_base {
protected:
	N*		_root	= nullptr;
	
	void	set_root(N* n)				{ _root = n ? n->make_root() : n; }

	void	assign(const tree_base &b)	{ if (b._root) _root = b._root->subtree_clone(); }
	void	assign(tree_base &&b)		{ _root = exchange(b._root, nullptr); }

public:
	typedef N	node_type;
	typedef tree_iterator<N>		iterator;
	typedef tree_iterator<const N>	const_iterator;

	tree_base()						{}
	tree_base(const tree_base &b)	{ assign(b); }
	tree_base(tree_base &&b)		{ assign(move(b)); }

	bool			empty()	const	{ return !_root; }
	size_t			size()	const	{ return _root ? _root->subtree_size() : 0; }
	void			deleteall()		{ if (_root) _root->subtree_delete(); _root = nullptr; }
	explicit operator bool() const	{ return !empty(); }

	const_iterator	root()	const	{ return _root; }
	const_iterator	begin()	const	{ auto i = root(); i.last(false); return i; }
	const_iterator	end()	const	{ return nullptr; }

	iterator		root()			{ return _root; }
	iterator		begin()			{ auto i = root(); i.last(false); return i; }
	iterator		end()			{ return nullptr; }

	// unbalanced remove
#if 0
	N*		remove(const tree_traverser<N, PARENT> &i) {
		N	*n = i.node();
		if (n) {
			if (N *p = i.parent_node())
				p->attach(n->remove(), p->side(n));
			else 
				set_root(n->remove());
		}
		return n;
	}
#else
	N*	remove(tree_traverser<N, PARENT> &i) {
		N	*n	= i.node();
		N	*r	= N::remove(i);
		if (r != (N*)1)
			set_root(r);
		return n;
	}
	N*	remove(tree_traverser<N, PARENT> &&i) {
		return remove(i);
	}
#endif

	// unbalanced insert n before i
	void	insert(N *i, N *n) {
		if (!i)
			set_root(n);
		else if (N *p = i->child[false])
			p->last(true)->attach(n, true);
		else
			i->attach(n, false);
	}

	// unbalanced insert (after any equal values)
	template<typename C> void	insert(N *n, const C &comp) {
		bool	right	= false;
		N		*last	= nullptr;
		auto&&	k		= n->key();
		for (N *p = _root; p; p = p->child[right]) {
			last	= p;
			right	= !comp(k, p->key());
		}
		if (last)
			last->attach(n, right);
		else
			set_root(n);
	}

	friend bool validate(const tree_base &t, int depth = 64) {
		return !t._root || (t._root->valid_root() && validate(t._root, depth));
	}
	template<typename C> friend bool validate(const tree_base &t, const C &comp, int depth = 64) {
		return !t._root || (t._root->valid_root() && validate(t._root, comp, depth));
	}
};


template<typename N> class tree_iterator2 : public tree_traverser<N> {
	typedef tree_traverser<N>	I;
public:
	tree_iterator2(N *p)	: I(p)			{}
	explicit operator bool()		const	{ return !!I::node(); }
	auto&			operator*()		const	{ return I::node()->value(); }
	auto			operator->()	const	{ return &I::node()->value(); }
	auto&			operator++()			{ I::next(true); return *this;	}
	auto			operator++(int)			{ auto i = *this; I::next(true); return i; }
	auto&			operator--()			{ I::next(false); return *this;	}
	auto			operator--(int)			{ auto i = *this; I::next(false); return i; }
	decltype(auto)	key()			const	{ return I::node()->key(); }
};

template<typename N, bool PARENT = N::PARENT> class tree_base2 : public tree_base<N, PARENT> {
public:
	typedef tree_iterator2<N>		iterator;
	typedef tree_iterator2<const N>	const_iterator;

	const_iterator	root()	const	{ return _root; }
	const_iterator	begin()	const	{ auto i = root(); i.last(false); return i; }
	const_iterator	end()	const	{ return nullptr; }

	iterator		root()			{ return _root; }
	iterator		begin()			{ auto i = root(); i.last(false); return i; }
	iterator		end()			{ return nullptr; }

	const tree_base<N, PARENT>&	with_keys()	const	{ return *this; }
	tree_base<N, PARENT>&		with_keys()			{ return *this; }
};


//-----------------------------------------------------------------------------
// unbalanced tree
//-----------------------------------------------------------------------------

template<typename T, bool PARENT = T::PARENT> struct e_treenode : public treenode_base<e_treenode<T, PARENT>, e_treenode<T, PARENT>*, PARENT> {
	T&			key()			{ return *static_cast<T*>(this);	}
	const T&	key()	const	{ return *static_cast<const T*>(this);	}
	T&			value()			{ return *static_cast<T*>(this);	}
	const T&	value()	const	{ return *static_cast<const T*>(this);	}
};

template<typename T, typename C = less> class e_tree : public tree_base2<e_treenode<T, T::PARENT>>, inheritable<C> {
	typedef	e_treenode<T, T::PARENT>	N;
	typedef	tree_base2<N>				B;
	decltype(auto)	comp()	const	{ return this->get_inherited(); }

public:
	e_tree(const C &c = C()) : inheritable<C>(c)	{}

	void			insert(N *n)	{ B::insert(n, comp()); }

	// using auto prevents NVO on clang
	template<typename K> const_iterator	lower_bound(const K &k)	const	{ auto i = root(); i.scan_lower_bound(k, comp()); return i; }
	template<typename K> const_iterator	upper_bound(const K &k)	const	{ auto i = root(); i.scan_upper_bound(k, comp()); return i; }
	template<typename K> const_iterator	find(const K &k)		const	{ auto i = lower_bound(k); return i && !comp()(k, *i) ? i : nullptr; }
	template<typename K> iterator		lower_bound(const K &k)			{ auto i = root(); i.scan_lower_bound(k, comp()); return i; }
	template<typename K> iterator		upper_bound(const K &k)			{ auto i = root(); i.scan_upper_bound(k, comp()); return i; }
	template<typename K> iterator		find(const K &k)				{ auto i = lower_bound(k); return i && !comp()(k, *i) ? i : nullptr; }

	//friends
	template<typename K> friend const_iterator	lower_boundc(const e_tree &t, K &&k)	{ return t.lower_bound(k); }
	template<typename K> friend const_iterator	upper_boundc(const e_tree &t, K &&k)	{ return t.upper_bound(k); }
	template<typename K> friend const_iterator	find(const e_tree &t, K &&k)			{ return t.find(k); }
	template<typename K> friend iterator		lower_boundc(e_tree &t, K &&k)			{ return t.lower_bound(k); }
	template<typename K> friend iterator		upper_boundc(e_tree &t, K &&k)			{ return t.upper_bound(k); }
	template<typename K> friend iterator		find(e_tree &t, K &&k)					{ return t.find(k); }

	friend bool validate(const e_tree &t, int depth = 64) {
		return validate(t, t.comp(), depth);
	}

};

//-----------------------------------------------------------------------------
// red black tree
//-----------------------------------------------------------------------------

enum rbcol { BLACK, RED };
template<typename N> using rbptr = pointer_pair<N, rbcol, 2, 2>;

#if 0
template<typename N, bool PARENT> struct rbnode_base_old : treenode_base<N, rbptr<N>, PARENT> {
	typedef treenode_base<N, rbptr<N>, PARENT>	B;
	using	B::child;

	void			set(rbcol c)				{ child[0].b = c; }
	rbcol			col()	const				{ return child[0].b; }
	static rbcol	col(const rbnode_base_old *n)	{ return n ? n->col() : BLACK; }
	friend rbcol	col(const rbnode_base_old *n)	{ return n ? n->col() : BLACK; }

	N	*rotate(bool right) {
		N	*p	= B::rotate(right);
		set(RED);
		p->set(BLACK);
		return p;
	}

	N	*insert(tree_traverser<N, PARENT> &i, bool right) {
		N	*n = static_cast<N*>(this);
		n->set(RED);

		while (N *p = i.pop()) {
			p->attach(n, right);
			bool	next	= i.side(p);

			if (col(n) == RED) {
				if (col(p->child[!right]) == RED) {
					p->set(RED);
					p->child[0]->set(BLACK);
					p->child[1]->set(BLACK);

				} else if (col(n->child[right]) == RED) {
					p = p->rotate(!right);

				} else if (col(n->child[!right]) == RED) {
					p = p->rotate2(!right);
				}
			}
			right	= next;
			n		= p;
		}

		n->set(BLACK);
		return n;
	}

	N	*remove_child_balance(bool right, bool *done) {
		if (N *s = child[!right]) {
			auto	my_col	= col();
			if (col(s->child[0]) == BLACK && col(s->child[1]) == BLACK) {
				if (my_col == RED)
					*done = true;

				set(BLACK);
				s->set(RED);
			} else {
				auto	p	= col(s->child[!right]) == RED ? rotate(right) : rotate2(right);
				p->set(my_col);
				p->child[0]->set(BLACK);
				p->child[1]->set(BLACK);
				*done = true;
				return p;
			}
		}
		return static_cast<N*>(this);
	}

	static N* remove(tree_traverser<N, PARENT> &i) {
		N		*p		= i.pop();
		if (!p)
			return (N*)1;

		if (p->child[0] && p->child[1]) {
			//	move_to_leaf
			i.push(p);
			i.push(p->child[false]);
			i.last(true);

			N	*r	= i.pop();
			swap(*r, *p);														//swap everything
			swap(*static_cast<rbnode_base_old*>(r), *static_cast<rbnode_base_old*>(p));	//swap back pointers, etc
			p = r;
		}

		bool	right	= i.side(p);
		bool	done	= col(p) == RED;
		N		*n		= p->child[!p->child[0]];

		if (!done && col(n) == RED) {
			n->set(BLACK);
			done = true;
		}

		while (N *p = i.pop()) {
			p->attach(n, right);
			if (done)
				return (N*)1;

			bool	prev	= right;
			right			= i.side(p);

			if (col(p->child[!prev]) == RED) {
				n = p->rotate(prev);
				//n->child[prev] = p->remove_child_balance(prev, &done);
				n->attach(p->remove_child_balance(prev, &done), prev);
			} else {
				n = p->remove_child_balance(prev, &done);
			}
		}

		// new root
		if (n)
			n->set(BLACK);
		return n;
	}

	constexpr bool	valid()	const {
		return B::valid() && (col() == BLACK || (col(child[0]) == BLACK && col(child[1]) == BLACK));
	}
	friend int validate(const rbnode_base_old *n, int depth = 0) {
		if (!n)
			return 0;
		if (depth > 64 || !n->valid())
			return -1;
		int	len0	= validate((N*)n->child[0], depth + 1);
		int	len1	= validate((N*)n->child[1], depth + 1);
		return len0 < 0 || len0 != len1 ? -1 : len0 + int(n->col() == BLACK);
	}
	template<typename C> friend int validate(const rbnode_base_old *n, const C &comp, int depth = 0) {
		if (!n)
			return 0;
		if (depth > 64 || !n->valid() || !n->valid_order(comp))
			return -1;
		int	len0	= validate((N*)n->child[0], comp, depth + 1);
		int	len1	= validate((N*)n->child[1], comp, depth + 1);
		return len0 < 0 || len0 != len1 ? -1 : len0 + int(n->col() == BLACK);
	}
};
#endif

//-----------------------------------------------------------------------------
// red black tree 2 - colours in pointers
//-----------------------------------------------------------------------------

template<typename N, bool PARENT> struct rbnode_base : treenode_base<N, rbptr<N>, PARENT> {
	typedef treenode_base<N, rbptr<N>, PARENT>	B;
	using	B::child;

	N*	rotate(bool right) {
		N	*n	= static_cast<N*>(this);
		auto p	= child[!right];
		n->attach(p->child[right], !right);
		p->attach(rbptr<N>{n, RED}, right);
		return p;
	}

	N*	insert(tree_traverser<N, PARENT> &i, bool right) {
		rbptr<N>	n	= {static_cast<N*>(this), RED};

		while (N *p = i.pop()) {
			p->attach(n, right);
			bool	prev	= right;
			right			= i.side(p);

			if (n.b == RED) {
				if (p->child[!prev].b == RED) {
					p->child[0].b = BLACK;
					p->child[1].b = BLACK;
					n	= {p, RED};

				} else if (n->child[prev].b == RED) {
					n	= {p->rotate(!prev), BLACK};

				} else if (n->child[!prev].b == RED) {
					n	= {p->rotate2(!prev), BLACK};

				} else {
					n	= {p, i.node() ? i.node()->child[right].b : BLACK};
				}
			} else {
				n	= {p, i.node() ? i.node()->child[right].b : BLACK};
			}
		}
		return n;
	}

	rbptr<N> remove_child_balance(bool right, rbcol c, bool *done) {
		if (N *s = child[!right]) {
			if (s->child[0].b == BLACK && s->child[1].b == BLACK) {
				child[!right].b = RED;
				if (c == RED) {
					c		= BLACK;
					*done	= true;
				}

			} else {
				auto	p	= s->child[!right].b == RED ? rotate(right) : rotate2(right);
				p->child[0].b = BLACK;
				p->child[1].b = BLACK;
				*done = true;
				return {p, c};
			}
		}
		return {static_cast<N*>(this), c};
	}

	static N* remove(tree_traverser<N, PARENT> &i) {
		N		*p		= i.node();
		N		*root	= (N*)1;
		if (!p)
			return root;

		if (p->child[0] && p->child[1]) {
			if (p = i.move_to_leaf(false))
				root = p;
		}

		p	= i.pop();

		auto	n		= p->child[!p->child[0]];
		N		*g		= i.node();
		bool	right	= g && g->side(p);
		bool	done	= g && g->child[right].b == RED;

		if (!done && n.b == RED) {
			n.b		= BLACK;
			done	= true;
		}

		while (N *p = i.pop()) {
			p->attach(n, right);
			if (done)
				return root;

			bool	prev	= right;
			right			= i.side(p);

			if (p->child[!prev].b == RED) {
				n	= {p->rotate(prev), BLACK};
				n->attach(p->remove_child_balance(prev, RED, &done), prev);

			} else {
				rbcol	pcol	= i.node() ? i.node()->child[right].b : BLACK;	//get p's colour
				n	= p->remove_child_balance(prev, pcol, &done);
			}
		}
		return n;
	}

	constexpr bool	valid(rbcol c)	const {
		return B::valid() && (c == BLACK || (child[0].b == BLACK && child[1].b == BLACK));
	}
	friend int validate(const rbptr<N> &n, int depth) {
		if (!n)
			return 0;
		if (!depth || !n->valid(n.b))
			return -1;
		int	len0	= validate(n->child[0], depth - 1);
		int	len1	= validate(n->child[1], depth - 1);
		return len0 < 0 || len0 != len1 ? -1 : len0 + int(n.b == BLACK);
	}
	template<typename C> friend bool validate(const rbnode_base *n, const C &comp, int depth) {
		if (!n->B::valid() || !n->valid_order(comp))
			return false;
		int	len0	= validate(n->child[0], comp, depth - 1);
		int	len1	= validate(n->child[1], comp, depth - 1);
		return len0 == len1;
	}
	template<typename C> friend int validate(const rbptr<N> &n, const C &comp, int depth) {
		if (!n)
			return n.b == RED ? -1 : 0;
		if (!depth || !n->valid(n.b) || !n->valid_order(comp))
			return -1;
		int	len0	= validate(n->child[0], comp, depth - 1);
		int	len1	= validate(n->child[1], comp, depth - 1);
		return len0 < 0 || len0 != len1 ? -1 : len0 + int(n.b == BLACK);
	}
};

//-----------------------------------------------------------------------------
// rbtree_base0
//-----------------------------------------------------------------------------

template<typename N, bool PARENT = N::PARENT> class rbtree_base0 : public tree_base2<N, PARENT> {
protected:
	typedef tree_base<N, PARENT>		B;
	typedef tree_traverser<N, PARENT>	I;

	//requires i has no 'right' child
	void	insert_side(I &&i, N *n, bool right) {
		B::set_root(n->insert(i, right));
	}

	// insert n before i
	void	insert(I &&i, N *n) {
		N	*p	= i.node();
		p		= p ? (N*)p->child[false] : B::_root;
		if (p) {
			i.push(p);
			i.last(true);
			B::set_root(n->insert(i, true));
		} else {
			B::set_root(n->insert(i, false));
		}
	}
};


template<typename N, typename C = less, bool PARENT = N::PARENT> class rbtree_base : public rbtree_base0<N, PARENT>, inheritable<C> {
	typedef rbtree_base0<N, PARENT>	B;
protected:
	decltype(auto)	comp()	const		{ return this->get_inherited(); }
	rbtree_base(const C &comp = C()) : inheritable<C>(comp)	{}

	friend bool validate(rbtree_base &t, int depth = 64) { return validate(t, t.comp(), depth); }
public:
	// using auto prevents NVO on clang
	template<typename K> const_iterator	lower_bound(const K &k)	const	{ auto i = B::root(); i.scan_lower_bound(k, comp()); return i; }
	template<typename K> const_iterator	upper_bound(const K &k)	const	{ auto i = B::root(); i.scan_upper_bound(k, comp()); return i; }
	template<typename K> const_iterator	find(const K &k)		const	{ auto i = lower_bound(k); return i && !comp()(k, i.key()) ? i : nullptr; }
	template<typename K> iterator		lower_bound(const K &k)			{ auto i = B::root(); i.scan_lower_bound(k, comp()); return i; }
	template<typename K> iterator		upper_bound(const K &k)			{ auto i = B::root(); i.scan_upper_bound(k, comp()); return i; }
	template<typename K> iterator		find(const K &k)				{ auto i = lower_bound(k); return i && !comp()(k, i.key()) ? i : nullptr; }
};

//-----------------------------------------------------------------------------
// e_rbtree
//-----------------------------------------------------------------------------

#if 0
template<typename T, bool PARENT> struct e_rbnode_old : rbnode_base_old<T, PARENT> {
	T&			key()			{ return *static_cast<T*>(this);	}
	const T&	key()	const	{ return *static_cast<const T*>(this);	}
	T&			value()			{ return *static_cast<T*>(this);	}
	const T&	value()	const	{ return *static_cast<const T*>(this);	}
};

#endif

template<typename T, bool PARENT> struct e_rbnode : rbnode_base<T, PARENT> {
	T&			key()			{ return *static_cast<T*>(this);	}
	const T&	key()	const	{ return *static_cast<const T*>(this);	}
	T&			value()			{ return *static_cast<T*>(this);	}
	const T&	value()	const	{ return *static_cast<const T*>(this);	}
};

template<typename T, typename C = less> class e_rbtree : public rbtree_base<T, C> {
	typedef rbtree_base<T, C>	B;
	typedef tree_traverser<T>	I;

public:
	using B::insert;

	e_rbtree(const C &comp = C()) : B(comp)	{}

	void	insert(T *n) {
		auto	i = root();
		i.scan_upper_bound(n->key(), comp());
		return insert(move(i), n);
	}

	//friends (using auto prevents NVO on clang)
	template<typename K> friend const_iterator	lower_boundc(const e_rbtree &t, K &&k)	{ return t.lower_bound(k); }
	template<typename K> friend const_iterator	upper_boundc(const e_rbtree &t, K &&k)	{ return t.upper_bound(k); }
	template<typename K> friend const_iterator	find(const e_rbtree &t, K &&k)			{ return t.find(k); }
	template<typename K> friend iterator		lower_boundc(e_rbtree &t, K &&k)		{ return t.lower_bound(k); }
	template<typename K> friend iterator		upper_boundc(e_rbtree &t, K &&k)		{ return t.upper_bound(k); }
	template<typename K> friend iterator		find(e_rbtree &t, K &&k)				{ return t.find(k); }
};

//-----------------------------------------------------------------------------
// tree selector
//-----------------------------------------------------------------------------

template<typename T> class tree :
	if_t<T_is_base_of<e_treenode<T>,		T>::value,	e_tree<T, less>,
	if_t<T_is_base_of<e_rbnode<T, false>,	T>::value,	e_rbtree<T, less>,
	if_t<T_is_base_of<e_rbnode<T, true>,	T>::value,	e_rbtree<T, less>,
	void>>> {};

//-----------------------------------------------------------------------------
// map
//-----------------------------------------------------------------------------

template<typename K, typename V, bool PARENT> struct map_node : public rbnode_base<map_node<K, V, PARENT>, PARENT>, pair<K, V> {
	typedef pair<K, V>	B;

	template<typename...P>	map_node(const K &k, P&&...p)	: B(k, V(forward<P>(p)...))	{}
	explicit map_node(const K &k) : B(k)	{}
	K&			key()				{ return B::a; }
	const K&	key()		const	{ return B::a; }
	V&			value()				{ return B::b; }
	const V&	value()		const	{ return B::b; }
	B&			key_value()			{ return *this; }
	const B&	key_value()	const	{ return *this; }
};

template<typename K, typename V, typename C = less, bool PARENT = false> class map : public rbtree_base<map_node<K, V, PARENT>, C, PARENT> {
	typedef map_node<K, V, PARENT> 		N;
	typedef rbtree_base<N, C, PARENT>	B;

public:
	map(const _none&, const C &comp = C())	: B(comp) {}
	map(const C &comp = C())				: B(comp) {}
	map(const map &b)	= default;
	map(map &&b)		= default;
	~map()							{ B::deleteall(); }

	void	clear()					{ B::deleteall(); }
	map&	operator=(const map &b)	{ clear(); B::assign(b); return *this; }
	map&	operator=(map &&b)		{ clear(); B::assign(move(b)); return *this; }

	void	remove(iterator &&i) {
		delete B::remove(i);
	}

	map&	operator+=(map &t2) {
		for (auto i = t2.begin(), e = t2.end(); i != e; ++i)
			put(i.key(), *i);
		return *this;
	}

	V&		put(const K &k, const V &v) {
		auto	i		= root();
		if (auto side = i.scan_insertion(k, B::comp())) {
			N	*n = new N(k, v);
			B::insert_side(move(i), n, side > 0);
			return n->value();
		}
		return *i = v;
	}
	template<typename...P> V&	emplace(const K &k, P&&... p) {
		auto	i		= root();
		if (auto side = i.scan_insertion(k, B::comp())) {
			N	*n = new N(k, forward<P>(p)...);
			B::insert_side(move(i), n, side > 0);
			return n->value();
		}
		return *i = V(forward<P>(p)...);
	}

	V&		operator[](const K &k) {
		auto	i = root();
		if (auto side = i.scan_insertion(k, B::comp())) {
			N	*n = new N(k);
			B::insert_side(move(i), n, side > 0);
			return n->value();
		}
		return i.node()->value();
	}
	optional<const V&>	operator[](const K &k) const {
		auto	i = root();
		if (auto side = i.scan_insertion(k, B::comp()))
			return none;
		return i.node()->value();
	}

	bool			count(const K &k)		const	{ auto i = root(); i.scan_lower_bound(k, B::comp()); return i && !B::comp()(k, i.key()); }

	//friends (using auto prevents NVO on clang)
	template<typename K2> friend const_iterator	lower_boundc(const map &m, K2 &&k)	{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const map &m, K2 &&k)	{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	find(const map &m, K2 &&k)			{ return m.find(k); }
	template<typename K2> friend iterator		lower_boundc(map &m, K2 &&k)		{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(map &m, K2 &&k)		{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(map &m, K2 &&k)				{ return m.find(k); }

	friend map	operator+(const map &a, const map &b)	{ return map(a) += b; }
};

//-----------------------------------------------------------------------------
// multimap
//-----------------------------------------------------------------------------

template<typename K, typename V, typename C = less, bool PARENT = false> class multimap : public rbtree_base<map_node<K, V, PARENT>, C, PARENT> {
	typedef map_node<K, V, PARENT> 		N;
	typedef rbtree_base<N, C, PARENT>	B;
public:

	multimap(const _none&, const C &comp = C())	: B(comp) {}
	multimap(const C &comp = C())				: B(comp) {}
	multimap(const multimap &b)		= default;
	multimap(multimap &&b)			= default;
	~multimap()									{ B::deleteall(); }

	void	clear()								{ B::deleteall(); }
	auto&	operator=(const multimap &b)		{ clear(); B::assign(b); return *this; }
	auto&	operator=(multimap &&b)				{ clear(); B::assign(move(b)); return *this; }

	void	remove(iterator &&i) {
		delete B::remove(i);
	}

	multimap&	operator+=(multimap &t2) {
		for (auto i = t2.begin(), e = t2.end(); i != e; ++i)
			put(i.key(), *i);
		return *this;
	}

	V&		put(const K &k, const V &v) {
		auto	i	= root();
		i.scan_upper_bound(k, B::comp());
		N	*n = new N(k, v);
		B::insert(move(i), n);
		return n->value();
	}
	template<typename...P> V&	emplace(const K &k, P&&... p) {
		auto	i	= root();
		i.scan_upper_bound(k, B::comp());
		N	*n = new N(k, forward<P>(p)...);
		B::insert(move(i), n);
		return n->value();
	}

	auto			count(const K &k) const {
		auto	i	= root();
		i.scan_lower_bound(k, B::comp());
		uint32	n = 0;
		while (i && !B::compare(k, i.key())) {
			++n;
			++i;
		}
		return n;
	}
	auto			bounds(const K &k) const {
		auto	i = root();
		i.scan_lower_bound(k, B::comp());
		auto	j = copy(i);
		j.scan_upper_bound(k, B::comp());
		return make_range(i, j);
	}

	//friends (using auto prevents NVO on clang)
	template<typename K2> friend const_iterator	lower_boundc(const multimap &m, K2 &&k)	{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const multimap &m, K2 &&k)	{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	find(const multimap &m, K2 &&k)			{ return m.find(k); }
	template<typename K2> friend iterator		lower_boundc(multimap &m, K2 &&k)		{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(multimap &m, K2 &&k)		{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(multimap &m, K2 &&k)				{ return m.find(k); }

	friend multimap	operator+(const multimap &a, const multimap &b)	{ return multimap(a) += b; }
};

//-----------------------------------------------------------------------------
// set
//-----------------------------------------------------------------------------

template<typename K, bool PARENT> struct set_node : public rbnode_base<set_node<K, PARENT>, PARENT> {
	K	k;
	set_node(const K &k) : k(k)	{}
	K&			key()				{ return k;	}
	const K&	key()	const		{ return k;	}
	K&			value()				{ return k;	}
	const K&	value()	const		{ return k;	}
};

template<typename K, typename C = less, bool PARENT = false> class set : public rbtree_base<set_node<K, PARENT>, C, PARENT> {
	typedef set_node<K, PARENT>			N;
	typedef rbtree_base<N, C, PARENT>	B;

	bool check_all(const set &b) const	{
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (!b.find(*i))
				return false;
		}
		return true;
	}
public:
	set(const _none&, const C &comp = C())	: B(comp)	{}
	set(const C &comp = C())				: B(comp)	{}
	set(const set &b)	= default;
	set(set &&b)		= default;
	~set()								{ B::deleteall(); }

	void	clear()						{ B::deleteall(); }
	set&	operator=(const set &b)		{ clear(); B::assign(b); return *this; }
	set&	operator=(set &&b)			{ clear(); B::assign(move(b)); return *this; }
	template<typename C> set&	operator=(C &&c)	{ clear(); for (auto &&i : c) insert(i); return *this; }

	bool	insert(const K &k) {
		auto	i	= root();
		if (auto side = i.scan_insertion(k, B::comp())) {
			B::insert_side(move(i), new N(k), side > 0);
			return true;
		}
		return false;
	}

	void	remove(iterator &&i) {
		delete B::remove(i);
	}

	_not<set> operator~()		const	{ return *this; }

	bool	count(const K &k)	const	{ auto i = root(); i.scan_lower_bound(k, B::comp()); return i && !B::comp()(k, *i); }

	set& operator&=(const set &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (!b.find(*i))
				remove(move(i));	//dodgy!!
		}
		return *this;
	}
	set& operator|=(const set &b) {
		for (auto &i : b)
			insert(i);
		return *this;
	}
	set& operator^=(const set &b) {
		for (auto &i : b) {
			if (auto j = find(i))
				remove(move(j));
			else
				insert(i);
		}
		return *this;
	}
	set& operator-=(const set &b) {
		for (auto &i : b) {
			if (auto j = find(i))
				remove(move(j));
		}
		return *this;
	}

	set& operator*=(const set &b)			{ return operator&=(b); }
	set& operator+=(const set &b)			{ return operator|=(b); }

	bool operator==(const set &b)	const	{ return B::size() == b.size() && check_all(b); }
	bool operator<=(const set &b)	const	{ return B::size() <= b.size() && check_all(b); }
	bool operator!=(const set &b)	const	{ return !(*this == b); }
	bool operator> (const set &b)	const	{ return !(*this <= b); }
	bool operator>=(const set &b)	const	{ return b <= *this; }
	bool operator< (const set &b)	const	{ return b >  *this; }

	//friends (using auto prevents NVO on clang)
	template<typename K2> friend const_iterator	lower_boundc(const set &m, K2 &&k)	{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	find(const set &m, K2 &&k)			{ return m.find(k); }
	template<typename K2> friend iterator		lower_boundc(set &m, K2 &&k)		{ return m.lower_bound(k); }
	template<typename K2> friend iterator		find(set &m, K2 &&k)				{ return m.find(k); }

	friend set operator&(const set &a, const set &b)		{ set t(a); t &= b; return t; }
	friend set operator|(const set &a, const set &b)		{ set t(a); t |= b; return t; }
	friend set operator^(const set &a, const set &b)		{ set t(a); t ^= b; return t; }
	friend set operator-(const set &a, const set &b)		{ set t(a); t -= b; return t; }
};

//-----------------------------------------------------------------------------
// multiset
//-----------------------------------------------------------------------------

template<typename K, bool PARENT> struct multiset_node : public rbnode_base<multiset_node<K, PARENT>, PARENT>, pair<K, uint32> {
	typedef pair<K, uint32>	B;

	multiset_node(const K &k) : B(k, 1) {}
	K&			key()				{ return B::a;	}
	const K&	key()	const		{ return B::a;	}
	B&			value()				{ return *this; }
	const B&	value()	const		{ return *this; }
};

template<typename K, typename C = less, bool PARENT = false> class multiset : public rbtree_base<multiset_node<K, PARENT>, C, PARENT> {
	typedef multiset_node<K, PARENT>	N;
	typedef rbtree_base<N, C, PARENT>	B;

	int check_all(const multiset &b) const	{
		bool	more = false;
		for (auto &i : *this) {
			int	n = b.count(i.a);
			if (n < i.b)
				return 0;
			more = more || n > i.b;
		}
		return int(more) + 1;
	}
public:
	multiset(const _none&, const C &comp = C())	: B(comp) {}
	multiset(const C &comp = C())				: B(comp) {}
	multiset(const multiset &b)		= default;
	multiset(multiset &&b)			= default;
	~multiset()									{ B::deleteall(); }

	void	clear()								{ B::deleteall(); }
	multiset&	operator=(const multiset &b)	{ clear(); B::assign(b); return *this; }
	multiset&	operator=(multiset &&b)			{ clear(); B::assign(move(b)); return *this; }
	template<typename C> multiset&	operator=(C &&c)	{ clear(); for (auto &&i : c) insert(i); return *this; }

	void	insert(const K &k) {
		auto	i = root();
		if (auto side = i.scan_insertion(k, B::comp())) {
			B::insert_side(move(i), new N(k), side > 0);
		} else {
			++i.node()->b;
		}
	}
	void	remove(iterator &&i) {
		delete B::remove(i);
	}

	uint32	count(const K &k)	const	{ auto i = lower_boundc(*this, k); return !B::comp()(k, i.key()) ? i->b : 0; }

	multiset& operator+=(const multiset &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (auto j = b.find(i->a))
				i->b += j->b;
		}
		return *this;
	}
	multiset& operator-=(const multiset &b) {
		for (auto &i : b) {
			if (auto j = find(i.a)) {
				if (i.b >= j->b)
					remove(move(j));
				else
					j->b -= i.b;
			}
		}
		return *this;
	}
	multiset& operator*=(const multiset &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (auto j = b.find(i->a))
				i->b = min(i->b, j->b);
			else
				remove(move(i));	//dodgy!!
		}
		return *this;
	}
	multiset& operator|=(const multiset &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (auto j = b.find(i->a))
				i->b = max(i->b, j->b);
		}
		return *this;
	}
	bool operator==(const multiset &b)	const	{ return B::size() == b.size() && check_all(b) == 1; }
	bool operator<=(const multiset &b)	const	{ return B::size() <= b.size() && check_all(b) > 0; }
	bool operator!=(const multiset &b)	const	{ return !(*this == b); }
	bool operator> (const multiset &b)	const	{ return !(*this <= b); }
	bool operator>=(const multiset &b)	const	{ return b <= *this; }
	bool operator< (const multiset &b)	const	{ return b > *this; }

	//friends (using auto prevents NVO on clang)
	template<typename K2> friend const_iterator	lower_boundc(const multiset &m, K2 &&k)	{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	find(const multiset &m, K2 &&k)			{ return m.find(k); }
	template<typename K2> friend iterator		lower_boundc(multiset &m, K2 &&k)		{ return m.lower_bound(k); }
	template<typename K2> friend iterator		find(multiset &m, K2 &&k)				{ return m.find(k); }

	friend multiset operator+(const multiset &a, const multiset &b)	{ multiset t(a); t += b; return t; }
	friend multiset operator-(const multiset &a, const multiset &b)	{ multiset t(a); t -= b; return t; }
	friend multiset operator*(const multiset &a, const multiset &b)	{ multiset t(a); t *= b; return t; }
	friend multiset operator|(const multiset &a, const multiset &b)	{ multiset t(a); t |= b; return t; }
};

//-----------------------------------------------------------------------------
// interval_tree
//-----------------------------------------------------------------------------

template<template<typename> class B, typename N, typename K> struct interval_node_base : B<N>, interval<K> {
	K		max;
	interval_node_base(const interval<K> &i) : interval<K>(i), max(i.b) {}
	template<typename N2> void	attach(N2 n, bool right) {
		K	m = this->b;
		if (N *s = this->child[!right]) {
			if (s->max > m)
				m = s->max;
		}
		if (n && n->max > m)
			m = n->max;
		max			= m;
		B<N>::attach(n, right);
	}
	interval<K>			tree_interval()	const	{ return {this->a, max}; }
	const interval<K>&	key()			const	{ return *this;	}
};

template<typename I, typename K> class interval_iterator : public I {
	using I::sp;
	using N = typename I::node_type;
	interval<K>	i;

	void	fix(bool right) {
		if (N *n = this->node()) {
			K	m	= n->b;
			if (m > n->max) {
				for (N **j = sp; depth(j--) > 0 && m > j[0]->max;)
					j[0]->max = m;
			} else {
				for (N **j = sp; depth(j--) > 0 && m < j[0]->max;) {
					if ((n = j[0]->child[right]) && n->max > m)
						m = n->max;
				}
			}
		}
	}
	int	last(N *p, bool right) {
		int		pushes = 0;
		if (!right) {
			while (p && !(p->max < i.a)) {
				push(p);
				++pushes;
				p = p->child[right];
			}
		} else {
			while (p && p->a < i.b) {
				push(p);
				++pushes;
				p = p->child[right];
			}
		}
		return pushes;
	}
	void	next(bool right) {
		fix(right);
		N*	n = this->node();
		do {
			if (last(n->child[right], !right) == 0) {
				N	*c;
				do {
					c	= n;
					n	= this->parent_pop();
				} while (n && c == n->child[right]);
			}
			n = this->node();
		} while (n && !overlap(i, *n));
	}
public:
	interval_iterator(N *p, const interval<K> &i, bool right) : I(0), i(i) {
		if (last(p, right) && !overlap(i, *this->node()))
			next(!right);
	}
	interval_iterator(N *p, const interval<K> &i) : I(p), i(i) {}
	interval_iterator&	operator++()	{ next(true); return *this;	}
	interval_iterator	operator++(int)	{ I i = *this; next(true); return i; }
	interval_iterator&	operator--()	{ next(false); return *this;	}
	interval_iterator	operator--(int)	{ I i = *this; next(false); return i; }
	interval<K>&		key()			{ return unconst(I::node()->key()); }
};

template<typename T, typename K> auto	interval_begin(T &tree, const interval<K> &x)	{
	return interval_iterator<typename T::iterator, K>(tree.root().node(), x, false);
}

template<typename T, typename K> auto	interval_begin(T &tree, const K &a, const K &b)	{
	return interval_begin(tree, interval<K>(a, b));
}

template<typename N> using rbnode_base0 = rbnode_base<N, false>;
template<typename K, typename V> struct interval_node : interval_node_base<rbnode_base0, interval_node<K, V>, K> {
	typedef	interval_node_base<rbnode_base0, interval_node<K, V>, K>	B;
	V		v;
	interval_node(const interval<K> &i)				: B(i)			{}
	interval_node(const interval<K> &i, const V &v) : B(i), v(v)	{}
	V&			value()			{ return v;	}
	const V&	value()	const	{ return v;	}
};

template<typename K, typename V> class interval_tree : public rbtree_base<interval_node<K, V>, less> {
	typedef interval_node<K, V> 	N;
	typedef rbtree_base<N, less>	B;
public:
	using B::remove;
	interval_tree()				: B(less()) {}
	interval_tree(const _none&)	: B(less()) {}
	~interval_tree()			{ B::deleteall(); }
	void	clear()				{ B::deleteall(); }

	void	insert(const interval<K> &i, const V &v) {
		auto	p = root();
		p.scan_upper_bound(i, B::comp());
		B::insert(move(p), new N(i, v));
	}
	void	remove(iterator &&i) {
		delete B::remove(i);
	}
	void	remove(iterator &i) {
		auto *n = B::remove(i);
		i.scan_lower_bound(n->a, less());
		delete n;
	}

	interval_iterator<iterator, K>	begin(const interval<K> &x) { return {B::root, x, false}; }

//friends
	template<typename K2> friend const_iterator	lower_boundc(const interval_tree &t, K2 &&k)	{ return t.lower_bound(k); }
	template<typename K2> friend iterator		lower_boundc(interval_tree &t, K2 &&k)			{ return t.lower_bound(k); }

	template<typename F> friend void intersect(interval_tree &t0, interval_tree &t1, F f) {
		for (auto i0 = t0.begin(t1.root()->tree_interval()); i0; ++i0) {
			for (auto i1 = t1.begin(i0.node()->tree_interval()); i1; ++i1)
				f(*i0, *i1);
		}
	}
};

//interval map just needs this:

template<typename K, typename V, bool PARENT> struct map_node<interval<K>, V, PARENT> : public interval_node_base<rbnode_base0, map_node<interval<K>, V, PARENT>, K> {
	typedef interval_node_base<rbnode_base0, map_node<interval<K>, V, PARENT>, K> B;
	V		v;
	template<typename...P>	map_node(const  interval<K> &i, P&&...p)	: B(i), v(forward<P>(p)...)	{}

	explicit map_node(const interval<K> &i)		: B(i)			{}
	const V&	value()		const	{ return v;	}
	V&			value()				{ return v;	}
};

}//namespace iso

//-----------------------------------------------------------------------------
//	TESTS in test_tree.cpp
//-----------------------------------------------------------------------------

#endif	// TREE_H
