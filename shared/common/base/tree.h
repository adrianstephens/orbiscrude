#ifndef TREE_H
#define TREE_H

#include "defs.h"
#include "interval.h"

namespace iso {


template<typename T, typename I> T&	tree_assign(T &tree, I a, I b) {
	tree.clear();
	while (a != b) {
		tree.insert(*a);
		++a;
	}
	return tree;
}

//-----------------------------------------------------------------------------
// treenode_base/tree_base (no parent)
//-----------------------------------------------------------------------------

template<typename N, typename P = N*, bool PARENT = false>	struct treenode_base;
template<typename N, typename C, bool PARENT = N::PARENT>	class tree_base;

template<typename N, typename P> struct treenode_base<N, P, false> {
	static const bool PARENT = false;
	P		child[2];

	treenode_base()						{ child[0] = child[1] = 0; }
	void	attach(N *n, int side)		{ child[side] = n; }
	N*		other(const N *n)	const	{ return child[child[0] == n]; }
	int		side(const N *n)	const	{ return child[1] == n; }

	N*		last(int side) {
		N	*p = static_cast<N*>(this);
		while (N *q = p->child[side])
			p = q;
		return p;
	}
	//N*		rotate(int side) {
	//	N	*n	= static_cast<N*>(this);
	//	N	*p	= child[!side];
	//	n->attach(p->child[side], !side);
	//	p->attach(n, side);
	//	return p;
	//}
	N*		remove() {
		N	*n0 = child[0], *n1 = child[1];
		if (!n1)
			return n0;
		if (!n0)
			return n1;

		N	*n = n1, *prev = 0;
		while (N *next = n->child[0]) {
			prev	= n;
			n		= next;
		}
		if (prev) {
			prev->attach(n->child[1], 0);
			n->attach(n1, 1);
		}
		n->attach(n0, 0);
		return n;
	}

	size_t	subtree_size() const {
		size_t	size = 1;
		if (N *n = child[0])
			size += n->subtree_size();
		if (N *n = child[1])
			size += n->subtree_size();
		return size;
	}

	N		*clone() const {
		N	*n		= new N(*static_cast<const N*>(this));
		if (N *c = child[0])
			n->child[0] = c->clone();
		if (N *c = child[1])
			n->child[1] = c->clone();
		return n;
	}


	constexpr bool	valid() const {
		return true;
	}
};

template<typename N> class tree_stackptr {
protected:
	N		**sp;

	void	next(N **stack, int side) {
		if (N *p = sp > stack ? (N*)sp[-1]->child[side] : 0) {
			last(p, !side);
		} else {
			do
				--sp;
			while (sp > stack && sp[0] == sp[-1]->child[side]);
			//while (sp >= stack && sp[1] == sp[0]->child[side]);
		}
	}
	tree_stackptr(N **stack) : sp(stack)	{}

	N*		pop()						{ return *--sp; }
	N*		node()				const	{ return sp[-1]; }
//	int		side()				const	{ return sp[-2]->side(sp[-1]); }
	int		side(const N *n)	const	{ return sp[-1]->side(n); }

	int	next_leaf(int side) {
		N	*p = sp[-1]->child[side];
		if (!p)
			return side;
		last(p, !side);
		return !side;
	}
	template<typename K, typename C> N **scan_lower_bound(const K &k, const C &comp) {
		N	*n		= pop();
		N	**best	= sp;
		while (n) {
			push(n);
			int	dir = comp(n->get(), k);
			if (!dir)
				best = sp;
			n = n->child[dir];
		}
		return best;
	}
	template<typename K, typename C> N **scan_upper_bound(const K &k, const C &comp) {
		N	*n		= pop();
		N	**best	= sp;
		while (n) {
			push(n);
			int	dir = !comp(k, n->get());
			if (!dir)
				best = sp;
			n = n->child[dir];
		}
		return best;
	}

public:
	typedef N	node_type;

	void move_to_leaf(N *n, int side) {
		N	*r = n->child[side]->last(!side);
		N	*p = node();
		p->attach(r, p->side(n));

		push(r);
		for (N *t = n->child[side]; t != r; t = t->child[!side])
			push(t);

		swap(*n, *r);

		p = node();
		if (p == r)
			r->attach(n, side);
		else
			p->attach(n, !side);
	}



	void	push(N *n) {
		*sp++ = n;
	}
	void	last(N *p, int side) {
		while (p) {
			push(p);
			p = p->child[side];
		}
	}
};

template<typename N, int D=32> class tree_stack : public tree_stackptr<N> {
	typedef tree_stackptr<N> B;
	N		*stack[D];
public:
	using	B::sp;

	tree_stack(N *p = nullptr)	: B(stack) { if (p) B::push(p); }
	tree_stack(N *p, int side)	: B(stack) { B::last(p, side); }
	tree_stack(const tree_stack &b)	: B(stack) {
		for (N *const *i = b.stack, **e = b.sp; i != e; ++i)
			B::push(*i);
	}
	tree_stack& operator=(const tree_stack &b) {
		sp = stack;
		for (N *const *i = b.stack, **e = b.sp; i != e; ++i)
			B::push(*i);
		return *this;
	}

	void	next(int side)							{ return B::next(stack, side); }
	N*		pop()									{ return sp > stack ? B::pop() : 0; }
	int		depth()							const	{ return sp - stack; }
	int		depth(N **p)					const	{ return p - stack; }
	N*		node()							const	{ return sp > stack ? B::node() : 0; }
//	int		side()							const	{ return sp < stack + 2 || B::side(); }
	int		side(const N *n)				const	{ return sp < stack + 1 || B::side(n); }
	N*		operator[](int i)				const	{ return stack[i]; }
	N*&		operator[](int i)						{ return stack[i]; }

	template<typename K, typename C> void scan_lower_bound(const K &k, const C &comp) {
		if (sp > stack)
			sp = B::scan_lower_bound(k, comp);
	}
	template<typename K, typename C> void scan_upper_bound(const K &k, const C &comp) {
		if (sp > stack)
			sp = B::scan_upper_bound(k, comp);
	}
	template<typename K, typename C> bool scan(const K &k, const C &comp) {
		if (sp > stack) {
			N	**s = B::scan_lower_bound(k, comp);
			if (s > stack && !comp(k, s[-1]->get())) {
				sp = s;
				return true;
			}
		}
		return false;
	}
	int	next_leaf(int side) {
		return sp > stack ? B::next_leaf(side) : !side;
	}
};

template<typename N, typename C> class tree_base<N, C, false> : pair<C, N*> {
	typedef pair<C, N*>	B;
protected:
	void		assign(const tree_base &b) { B::operator=(b); if (root()) root() = root()->clone(); }
#ifdef USE_RVALUE_REFS
	void		assign(tree_base &&b)		{ B::operator=(b); b.root() = nullptr; }
#else
	void		assign(tree_base &b)		{ B::operator=(b); b.root() = nullptr; }
#endif

	const C&	comp()	const		{ return B::a; }
	void		set_root(N *n)		{ B::b = n; }
	template<typename X, typename Y> inline bool compare(const X &x, const Y &y) const { return B::a(x, y); }
public:
	typedef tree_stack<N>	iterator;
	typedef N				node_type;

	tree_base(const C &comp = C())	: B(comp, nullptr) {}
	tree_base(const tree_base &b)	{ assign(b); }
#ifdef USE_RVALUE_REFS
	tree_base(tree_base &&b)		{ assign(move(b)); }
#endif

	N*		root()		const		{ return B::b; }
	N*&		root()					{ return B::b; }
	void	clear()					{ root() = nullptr; }
	bool	empty()		const		{ return !root(); }
	N		*head()		const		{ return root() ? root()->last(0) : 0;	}
	N		*tail()		const		{ return root() ? root()->last(1) : 0;	}
	size_t	size()		const		{ return root() ? root()->subtree_size() : 0; }

	void	insert(N *n) {
		int		side	= 0;
		N		*last	= 0;
		for (N *node = root(); node; node = node->child[side]) {
			last = node;
			side = compare(node->get(), n->get());
		}
		if (last)
			last->attach(n, side);
		else
			root() = n;
	}

	void	deleteall() {
		for (N *i = root(), *n; i; i = n) {
			if (n = i->child[0]) {
				i->child[0] = n->child[1];
				n->child[1] = i;
			} else {
				n = i->child[1];
				delete i;
			}
		}
		clear();
	}
};

//-----------------------------------------------------------------------------
// treenode_base/tree_base (with parent)
//-----------------------------------------------------------------------------

template<typename N, typename P> struct treenode_base<N, P, true> : treenode_base<N, P, false> {
	static const bool PARENT = true;
	typedef	treenode_base<N, P, false>	B;
	using			B::child;
	P				parent;

	treenode_base() : parent(0) {}
	void	attach(N *n, int side) {
		if (child[side] = n)
			n->parent = static_cast<N*>(this);
	}
	N*		sibling() {
		return parent->other(static_cast<N*>(this));
	}
	N*		next(int side) {
		if (N *p = child[side])
			return p->last(!side);
		N *p = static_cast<N*>(this), *q;
		do {
			q = p;
			p = p->parent;
		} while (p && q == p->child[side]);
		return p;
	}

	N		*clone() const {
		N	*n		= new N(*static_cast<const N*>(this));
		if (N *c = child[0]) {
			n->child[1] = c->clone();
			n->child[1]->parent = n;
		}
		if (N *c = child[1]) {
			n->child[1] = c->clone();
			n->child[1]->parent = n;
		}
		return n;
	}

	constexpr bool	valid() const { 
		return (!child[0] || child[0]->parent == this) && (!child[1] || child[1]->parent == this);
	}
};

template<typename N> class treepointer {
protected:
	N		*p;

	template<typename K, typename C> static N *_scan_lower_bound(N *n, const K &k, const C &comp) {
		N	*best	= nullptr;
		for (int dir; n; n = n->child[dir]) {
			if (!(dir = comp(n->get(), k)))
				best = n;
		}
		return best;
	}
	template<typename K, typename C> static N *_scan_upper_bound(N *n, const K &k, const C &comp) {
		N	*best	= nullptr;
		for (int dir; n; n = n->child[dir]) {
			if (!(dir = !comp(k, n->get())))
				best = n;
		}
		return best;
	}
	void	next(int side) {
		p = p->next(side);
	}
	void	last(N *n, int side) {
		p = n->last(side);
	}

public:
	typedef N	node_type;
	treepointer(N *p) : p(p)						{}
	treepointer(N *p, int side) : p(p ? p->last(side) : p)	{}
	N*		node()				const	{ return p; }
	void	push(N *n)					{ p = n; }
	N*		pop()						{ return p ? exchange(p, p->parent) : p; }
	int		side(const N *n)	const	{ return !p || !p->parent || p->parent->side(n); }

	template<typename K, typename C> void scan_lower_bound(const K &k, const C &comp) {
		auto r = _scan_lower_bound(p, k, comp);
		p = r ? r : p ? p->last(1) : p;
	}
	template<typename K, typename C> void scan_upper_bound(const K &k, const C &comp) {
		auto r = _scan_upper_bound(p, k, comp);
		p = r ? r : p ? p->last(1) : p;
	}
	template<typename K, typename C> bool scan(const K &k, const C &comp) {
		auto r = _scan_lower_bound(p, k, comp);
		p = r ? r : p ? p->last(1) : p;
		return r && !comp(k, r->get());
	}
	void move_to_leaf(N *n, int side) {
		N	*r = n->child[side]->last(!side);
		p->attach(r, p->side(n));

		push(r);
		for (N *t = n->child[side]; t != r; t = t->child[!side])
			push(t);

		swap(*n, *r);

		if (p == r)
			r->attach(n, side);
		else
			p->attach(n, !side);
	}
};

template<typename N, typename C> class tree_base<N, C, true> : public tree_base<N, C, false> {
	typedef tree_base<N, C, false> B;

protected:
	void		set_root(N *n)	{
		if (n)
			n->parent = nullptr;
		B::set_root(n);
	}
public:
	typedef treepointer<N>		iterator;

	tree_base(const C &comp) : B(comp) {}
	void remove(N *n) {
		if (N *p = n->parent)
			p->attach(n->remove(), p->side(n));
		else if (B::root() = n->remove())
			B::root()->parent = 0;
	}

	void deleteall() {
		N	*n = B::root(), *c;
		while (n) {
			while (c = n->child[1])
				n = c->last(0);

			do {
				c = n;
				n = n->parent;
				delete c;
			} while (n && n->child[1] == c);
		}
		B::clear();
	}
};

//-----------------------------------------------------------------------------
// treeiterator
//-----------------------------------------------------------------------------

template<typename P> class treeiterator0 : public P {
public:
	using typename P::node_type;
	using P::node;

	treeiterator0(node_type *p) : P(p) {}
	treeiterator0(node_type *p, int side) : P(p, side) {}
	operator				node_type*()		const	{ return node();	}
	node_type*				operator->()		const	{ return node();	}
	treeiterator0&			operator++()				{ P::next(1); return *this;	}
	treeiterator0			operator++(int)				{ treeiterator0 i = *this; P::next(1); return i; }
	treeiterator0&			operator--()				{ P::next(0); return *this;	}
	treeiterator0			operator--(int)				{ treeiterator0 i = *this; P::next(0); return i; }
	bool	operator==(const treeiterator0 &i)	const	{ return node() == i.node(); }
	bool	operator!=(const treeiterator0 &i)	const	{ return node() != i.node(); }

	template<typename K, typename C> treeiterator0& scan_lower_bound(const K &k, const C &comp) {
		P::scan_lower_bound(k, comp);
		return *this;
	}
	template<typename K, typename C> treeiterator0& scan_upper_bound(const K &k, const C &comp) {
		P::scan_upper_bound(k, comp);
		return *this;
	}
};

template<typename P, typename T> class treeiterator : public P {
public:
	typedef bidirectional_iterator_t iterator_category;
	typedef T		element, &reference;
	using typename P::node_type;
	using P::node;

	treeiterator(node_type *p) : P(p) {}
	treeiterator(node_type *p, int side) : P(p, side) {}
	operator				element*()			const	{ return &node()->get();	}
	element*				operator->()		const	{ return &node()->get();	}
	treeiterator&			operator++()				{ P::next(1); return *this;	}
	treeiterator			operator++(int)				{ treeiterator i = *this; P::next(1); return i; }
	treeiterator&			operator--()				{ P::next(0); return *this;	}
	treeiterator			operator--(int)				{ treeiterator i = *this; P::next(0); return i; }
	bool	operator==(const treeiterator &i)	const	{ return node() == i.node(); }
	bool	operator!=(const treeiterator &i)	const	{ return node() != i.node(); }

	template<typename K, typename C> treeiterator& scan_lower_bound(const K &k, const C &comp) {
		P::scan_lower_bound(k, comp);
		return *this;
	}
	template<typename K, typename C> treeiterator& scan_upper_bound(const K &k, const C &comp) {
		P::scan_upper_bound(k, comp);
		return *this;
	}
};

//-----------------------------------------------------------------------------
// unbalanced tree
//-----------------------------------------------------------------------------

template<typename T, bool PARENT> struct e_treenode : public treenode_base<e_treenode<T, PARENT>, e_treenode<T, PARENT>*, PARENT> {
	T&			get()			{ return *static_cast<T*>(this);	}
	const T&	get()	const	{ return *static_cast<const T*>(this);	}
};

template<typename T, typename C = less> class e_tree : public tree_base<e_treenode<T, T::PARENT>, C> {
	typedef e_treenode<T, T::PARENT>	N;
	typedef tree_base<N, C> B;
public:
	typedef T	element, &reference;
	typedef treeiterator<typename B::iterator, T>			iterator;
	typedef treeiterator<typename B::iterator, const T>		const_iterator;

	e_tree(const C &comp = C()) : B(comp)	{}
	const_iterator	begin()		const	{ return const_iterator(B::root, 0); }
	const_iterator	end()		const	{ return 0; }
	iterator		begin()				{ return iterator(B::root, 0); }
	iterator		end()				{ return 0; }

	void			remove(N *n)		{ B::remove(n); n->child[0] = n->child[1] = n->parent = 0; }
	T*				pop_root()			{ T *t = B::root; if (t) B::root = t->remove(); return t; }
	T*				pop_head()			{ T *t = (T*)B::head(); if (t) remove(t); return t;	}
	T*				pop_tail()			{ T *t = (T*)B::tail(); if (t) remove(t); return t;	}

	template<typename K> iterator		lower_bound(const K &k)			{ return iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K> const_iterator	lower_bound(const K &k)	const	{ return const_iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K> iterator		upper_bound(const K &k)			{ return iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K> const_iterator	upper_bound(const K &k)	const	{ return const_iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K> iterator		find(const K &k)				{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }
	template<typename K> const_iterator	find(const K &k)		const	{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }

//friends
	template<typename K> friend iterator		lower_boundc(e_tree &t, const K &k)			{ return t.lower_bound(k); }
	template<typename K> friend const_iterator	lower_boundc(const e_tree &t, const K &k)	{ return t.lower_bound(k); }
	template<typename K> friend iterator		upper_boundc(e_tree &t, const K &k)			{ return t.upper_bound(k); }
	template<typename K> friend const_iterator	upper_boundc(const e_tree &t, const K &k)	{ return t.upper_bound(k); }
	template<typename K> friend iterator		find(e_tree &t, const K &k)					{ return t.find(k); }
	template<typename K> friend const_iterator	find(const e_tree &t, const K &k)			{ return t.find(k); }
};

//-----------------------------------------------------------------------------
// red black tree (parentless)
//-----------------------------------------------------------------------------

enum rbcol { BLACK, RED };

template<typename N, bool PARENT> struct rbnode_base : treenode_base<N, pointer_pair<N,rbcol,2,2>, PARENT> {
	typedef treenode_base<N, pointer_pair<N,rbcol,2,2>, PARENT>	B;
	using	B::child;

	rbcol	col()		const	{ return child[0].b; }
	void	set(rbcol c)		{ child[0].b = c; }

	N	*rotate(int side) {
		N	*n	= static_cast<N*>(this);
		N	*p	= child[!side];
		n->attach(p->child[side], !side);
		p->attach(n, side);
		set(RED);
		p->set(BLACK);
		return p;
	}

	N	*rotate2(int side) {
		N	*n	= static_cast<N*>(this);
		n->attach(child[!side]->rotate(!side), !side);
		return rotate(side);
	}

	friend rbcol col(const rbnode_base *n) { return n ? n->col() : BLACK; }
};

template<typename N, typename C, bool PARENT = N::PARENT> class rbtree_base : public tree_base<N, C, PARENT> {
protected:
	typedef tree_base<N, C, PARENT>		B;
	typedef	typename B::iterator		BI;
	static N*	_remove_balance(N *p, int dir, bool *done);
	static N*	_insert(BI s, N *n, int dir);
	static N*	_remove(BI &s);
public:

	typedef treeiterator0<BI>	iterator;

	rbtree_base(const C &comp) : B(comp) {}
	void	insert(BI &s, N *n, int dir)	{
		B::set_root(_insert(s, n, dir));
	}
	void	insert(BI &s, N *n)	{
		insert(s, n, s.node() && B::compare(s.node()->get(), n->get()));
	}
	void	remove(BI &s) {
		N *n = _remove(s);
		if (!s.node())
			B::set_root(n);
	}
	iterator	begin()	const	{ return iterator(B::root(), 0); }
	iterator	end()	const	{ return 0;	}
};

template<typename N, typename C, bool PARENT> N* rbtree_base<N, C, PARENT>::_insert(BI s, N *n, int dir) {
	n->set(RED);

	while (N *p = s.pop()) {
		p->attach(n, dir);
		int	next	= s.side(p);

		if (col(n) == RED) {
			if (col(p->child[!dir]) == RED) {
				p->set(RED);
				p->child[0]->set(BLACK);
				p->child[1]->set(BLACK);
			} else {
				if (col(n->child[dir]) == RED)
					p = p->rotate(!dir);
				else if (col(n->child[!dir]) == RED)
					p = p->rotate2(!dir);
			}
		}
		dir	= next;
		n	= p;
	}
	n->set(BLACK);
	return n;
}

template<typename N, typename C, bool PARENT> N *rbtree_base<N, C, PARENT>::_remove_balance(N *p, int dir, bool *done) {
	if (N *s = p->child[!dir]) {
		if (col(s->child[0]) == BLACK && col(s->child[1]) == BLACK) {
			if (col(p) == RED)
				*done = true;

			p->set(BLACK);
			s->set(RED);
		} else {
			auto	pcol	= p->col();
			p = col(s->child[!dir]) == RED ? p->rotate(dir) : p->rotate2(dir);
			p->set(pcol);
			p->child[0]->set(BLACK);
			p->child[1]->set(BLACK);

			*done = true;
		}
	}
	return p;
}

template<typename N, typename C, bool PARENT> N* rbtree_base<N, C, PARENT>::_remove(BI &s) {
	N		*p		= s.pop();

	if (p->child[0] && p->child[1])
		s.move_to_leaf(p, 0);

	int		dir		= s.side(p);
	bool	done	= false;
	N		*n		= p->child[p->child[0].a == NULL];

	if (col(p) == RED) {
		done = true;
	} else if (col(n) == RED) {
		n->set(BLACK);
		done = true;
	}

	while (N *p = s.pop()) {
		p->attach(n, dir);
		if (done)
			return nullptr;

		if (col(p->child[!dir]) == RED) {
			n = p->rotate(dir);
			n->child[dir] = _remove_balance(p, dir, &done);
		} else {
			n = _remove_balance(p, dir, &done);
		}

		dir	= s.side(p);
	}

	// new root
	if (n)
		n->set(BLACK);
	return n;
}

template<typename N, bool PARENT> int validate(rbnode_base<N, PARENT> *n) {
	if (!n)
		return 0;
	if (!n->valid() || (n->col() == RED && (col(n->child[0]) == RED || col(n->child[1]) == RED)))
		return -1;
	int	len0	= validate((N*)n->child[0]);
	int	len1	= validate((N*)n->child[1]);
	return len0 < 0 || len0 != len1 ? -1 : len0 + int(!n->col() == RED);
}

template<typename N, typename C> bool validate(rbtree_base<N, C> &t) {
	if (auto r = t.root()) {
		if (!r->valid())
			return false;
		int	len0	= validate((N*)r->child[0]);
		int	len1	= validate((N*)r->child[1]);
		return len0 >= 0 && len0 == len1;
	}
	return true;
}

template<typename T, bool PARENT> struct e_rbnode : rbnode_base<T, PARENT> {
	T&			get()			{ return *static_cast<T*>(this);	}
	const T&	get()	const	{ return *static_cast<const T*>(this);	}
};

template<typename T, typename C = less> class e_rbtree : public rbtree_base<T, C> {
	typedef rbtree_base<T, C>	B;
	using typename B::BI;

public:
	typedef T							element, &reference;
	typedef treeiterator<BI, T>			iterator;
	typedef treeiterator<BI, const T>	const_iterator;

	e_rbtree(const C &comp = C()) : B(comp)	{}

	const_iterator	begin()		const	{ return const_iterator(B::root, 0); }
	const_iterator	end()		const	{ return 0;	}
	iterator		begin()				{ return iterator(B::root(), 0); }
	iterator		end()				{ return 0;	}

	template<typename K> iterator		lower_bound(const K &k)			{ return iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K> const_iterator	lower_bound(const K &k)	const	{ return const_iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K> iterator		upper_bound(const K &k)			{ return iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K> const_iterator	upper_bound(const K &k)	const	{ return const_iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K> iterator		find(const K &k)				{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }
	template<typename K> const_iterator	find(const K &k)		const	{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }

	//friends
	template<typename K> friend iterator		lower_boundc(e_rbtree &t, const K &k)		{ return t.lower_bound(k); }
	template<typename K> friend const_iterator	lower_boundc(const e_rbtree &t, const K &k)	{ return t.lower_bound(k); }
	template<typename K> friend iterator		upper_boundc(e_rbtree &t, const K &k)		{ return t.upper_bound(k); }
	template<typename K> friend const_iterator	upper_boundc(const e_rbtree &t, const K &k)	{ return t.upper_bound(k); }
	template<typename K> friend iterator		find(e_rbtree &t, const K &k)				{ return t.find(k); }
	template<typename K> friend const_iterator	find(const e_rbtree &t, const K &k)			{ return t.find(k); }
};
/*
template<typename T, bool PARENT> struct rbnode : rbnode_base<rbnode<T, PARENT>, PARENT> {
	T				t;
	rbnode(const T &_t) : t(_t)	{}
	T&			get()			{ return t;	}
	const T&	get()	const	{ return t;	}
};

template<typename T, typename C, bool PARENT> class rbtree : public rbtree_base<rbnode<T, PARENT>, C> {
	typedef rbnode<T, PARENT>	N;
	typedef rbtree_base<N, C>	B;
	using typename B::BI;
public:
	typedef T										element, &reference;
	typedef treeiterator<BI, T>			iterator;
	typedef treeiterator<BI, const T>	const_iterator;

	rbtree(const C &comp = C())	: B(comp) {}
	~rbtree()							{ B::deleteall(); }
	void			insert(const T &t)	{
		tree_stack<N>	s(B::root);
		if (!s.scan(t))
			B::insert(s, new N(t));
	}
	void			remove(iterator &i)	{
		auto *n = i.node();
		i.sp	= B::remove(i);
		i.scan_lower_bound(n->k, B::comp());
		delete n;
	}
	void			clear()				{ B::deleteall(); }

	const_iterator	begin()		const	{ return const_iterator(B::root, 0); }
	const_iterator	end()		const	{ return 0;	}
	iterator		begin()				{ return iterator(B::root, 0); }
	iterator		end()				{ return 0;	}

	template<typename K> iterator			lower_bound(const K &k)			{ return iterator(B::root).scan_lower_bound(k, B::comp()); }
	template<typename K> const_iterator		lower_bound(const K &k)	const	{ return const_iterator(B::root).scan_lower_bound(k, B::comp()); }
	template<typename K> iterator			upper_bound(const K &k)			{ return iterator(B::root).scan_upper_bound(k, B::comp()); }
	template<typename K> const_iterator		upper_bound(const K &k)	const	{ return const_iterator(B::root).scan_upper_bound(k, B::comp()); }
	template<typename K> iterator			find(const K &k)				{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }
	template<typename K> const_iterator		find(const K &k)		const	{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }

	//friends
	template<typename K2> friend iterator		lower_boundc(rbtree &m, const K2 &k)		{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	lower_boundc(const rbtree &m, const K2 &k)	{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(rbtree &m, const K2 &k)		{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const rbtree &m, const K2 &k)	{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(rbtree &m, const K2 &k)				{ return m.find(k); }
	template<typename K2> friend const_iterator	find(const rbtree &m, const K2 &k)			{ return m.find(k); }
};
*/
//-----------------------------------------------------------------------------
// tree selector
//-----------------------------------------------------------------------------

template<typename T> class tree :
	if_t<T_is_base_of<e_treenode<T,false>,	T>::value,	e_tree<T, less>,
	if_t<T_is_base_of<e_treenode<T, true>,	T>::value,	e_tree<T, less>,
	if_t<T_is_base_of<e_rbnode<T, false>,	T>::value,	e_rbtree<T, less>,
	if_t<T_is_base_of<e_rbnode<T, true>,	T>::value,	e_rbtree<T, less>,
//	if_t<T_is_base_of<rbnode<T, false>,		T>::value,	rbtree<T, less, false>,
//	if_t<T_is_base_of<rbnode<T, true>,		T>::value,	rbtree<T, less, true>,
	void>>>> {};

//-----------------------------------------------------------------------------
// map
//-----------------------------------------------------------------------------

template<typename K, typename V> struct map_node : public rbnode_base<map_node<K, V>, false> {
	K	k;
	V	v;

	template<typename...P>	map_node(const K &k, P&&...p)	: k(k), v(forward<P>(p)...)	{}
	explicit map_node(const K &k) : k(k), v()	{}
	K&			get()			{ return k;	}
	const K&	get()	const	{ return k;	}
	V&			value()			{ return v;	}
	const V&	value()	const	{ return v;	}
	const K&	key()	const	{ return k;	}
};

template<typename K, typename V, typename C = less> class map : public rbtree_base<map_node<K, V>, C, false> {
	typedef map_node<K, V> 				N;
	typedef rbtree_base<N, C, false>	B;
	typedef typename B::BI				BI;
public:
	class iterator : public BI {
	public:
		typedef bidirectional_iterator_t iterator_category;
		typedef V		element, &reference;
		iterator(N *p)				: BI(p)			{}
		iterator(N *p, int side)	: BI(p, side)	{}
		explicit operator bool()			const	{ return !!BI::node(); }
		V&				operator*()			const	{ return BI::node()->value(); }
		V*				operator->()		const	{ return &BI::node()->value(); }
		bool operator==(const iterator& b)	const	{ return BI::node() == b.node(); }
		bool operator!=(const iterator& b)	const	{ return !operator==(b); }
		iterator&		operator++()				{ BI::next(1); return *this;	}
		iterator		operator++(int)				{ iterator i = *this; BI::next(1); return i; }
		iterator&		operator--()				{ BI::next(0); return *this;	}
		iterator		operator--(int)				{ iterator i = *this; BI::next(0); return i; }
		const K&		key()				const	{ return BI::node()->get(); }
		template<typename K2> iterator& scan_lower_bound(const K2 &k, const C &comp) { BI::scan_lower_bound(k, comp); return *this; }
		template<typename K2> iterator& scan_upper_bound(const K2 &k, const C &comp) { BI::scan_upper_bound(k, comp); return *this; }
	};
	typedef iterator	const_iterator;
	typedef V			element, &reference;

	map(const C &comp = C()) : B(comp) {}
	map(const map &b) : B(b)		{}
	~map()							{ B::deleteall(); }

	void	clear()					{ B::deleteall(); }
	map&	operator=(const map &b)	{ clear(); B::assign(b); return *this; }
	template<typename T> map&	operator=(const T &t)	{ using iso::begin; using iso::end; return tree_assign(*this, begin(t), end(t)); }

#ifdef USE_RVALUE_REFS
	map(map &&b) = default;
	map&	operator=(map &&b)		{ clear(); B::assign(move(b)); return *this; }

	template<typename T> V&		put(const K &k, T &&v) {
		tree_stack<N>	s(B::root());
		if (s.scan(k, B::comp()))
			return s.node()->value() = forward<T>(v);
		N	*n = new N(k, forward<T>(v));
		B::insert(s, n);
		return n->value();
	}
	template<typename...P> V&	emplace(const K &k, P&&... p) {
		tree_stack<N>	s(B::root());
		if (s.scan(k, B::comp()))
			return s.node()->value() = V(forward<P>(p)...);
		N	*n = new N(k, forward<P>(p)...);
		B::insert(s, n);
		return n->value();
	}
#endif

#if 0
	V&		get(const K &k);
	V&		put(const K &k) {
		tree_stack<N>	s(B::root());
		if (s.scan(k, B::comp()))
			return s.node()->value();
		N	*n = new N(k);
		B::insert(s, n);
		return n->value();
	}
	auto	operator[](const K &k) { return putter<map,K>(*this, k); }

#else
	V&		operator[](const K &k) {
		tree_stack<N>	s(B::root());
		if (s.scan(k, B::comp()))
			return s.node()->value();
		N	*n = new N(k);
		B::insert(s, n);
		return n->value();
	}
#endif
	optional<V&>	operator[](const K &k) const {
		tree_stack<N>	s(B::root());
		if (s.scan(k, B::comp()))
			return s.node()->value();
		return none;
	}

	iterator		begin()				{ return iterator(B::root(), 0); }
	iterator		end()				{ return 0; }
	const_iterator	begin()		const	{ return const_iterator(B::root(), 0); }
	const_iterator	end()		const	{ return 0; }

	B&				with_keys()			{ return *this; }
	const B&		with_keys()	const	{ return *this; }

	template<typename K2> iterator			lower_bound(const K2 &k)			{ return iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K2> const_iterator	lower_bound(const K2 &k)	const	{ return const_iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K2> iterator			upper_bound(const K2 &k)			{ return iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K2> const_iterator	upper_bound(const K2 &k)	const	{ return const_iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K2> iterator			find(const K2 &k)					{ auto i = lower_bound(k); return i && !B::compare(k, i.key()) ? i : 0; }
	template<typename K2> const_iterator	find(const K2 &k)			const	{ auto i = lower_bound(k); return i && !B::compare(k, i.key()) ? i : 0; }
	template<typename K2> bool				count(const K2 &k)			const	{ auto i = lower_bound(k); return i && !B::compare(k, i.key()); }
//friends
	template<typename K2> friend iterator		lower_boundc(map &m, const K2 &k)		{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	lower_boundc(const map &m, const K2 &k)	{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(map &m, const K2 &k)		{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const map &m, const K2 &k)	{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(map &m, const K2 &k)				{ return m.find(k); }
	template<typename K2> friend const_iterator	find(const map &m, const K2 &k)			{ return m.find(k); }
	template<typename K2> friend bool			count(const map &m, const K2 &k)		{ return m.count(k); }
};

//-----------------------------------------------------------------------------
// multimap
//-----------------------------------------------------------------------------

template<typename K, typename V, typename C = less> class multimap : public rbtree_base<map_node<K, V>, C> {
	typedef map_node<K, V> 		N;
	typedef rbtree_base<N, C>	B;
	using typename B::BI;
public:
	class iterator : public BI {
	public:
		typedef bidirectional_iterator_t iterator_category;
		typedef V		element, &reference;
		iterator(N *p)				: BI(p)			{}
		iterator(N *p, int side)	: BI(p, side)	{}
		operator		V*()				const	{ N *p = BI::node(); return p ? &p->value() : 0;	}
		V*				operator->()		const	{ N *p = BI::node(); return p ? &p->value() : 0;	}
		iterator&		operator++()				{ BI::next(1); return *this;	}
		iterator		operator++(int)				{ iterator i = *this; BI::next(1); return i; }
		iterator&		operator--()				{ BI::next(0); return *this;	}
		iterator		operator--(int)				{ iterator i = *this; BI::next(0); return i; }
		K&				key()				const	{ return BI::node()->get(); }
		template<typename K2> iterator& scan_lower_bound(const K2 &k, const C &comp) { BI::scan_lower_bound(k, comp); return *this; }
		template<typename K2> iterator& scan_upper_bound(const K2 &k, const C &comp) { BI::scan_upper_bound(k, comp); return *this; }
	};
	typedef iterator	const_iterator;
	typedef V			element, &reference;

	multimap(const C &comp = C())	: B(comp)	{}
	multimap(const multimap &b)		: B(b)		{}
	~multimap()									{ B::deleteall(); }
	void	clear()								{ B::deleteall(); }
	multimap&	operator=(const multimap &b)	{ clear(); B::assign(b); return *this; }
	template<typename T> multimap&	operator=(const T &t)	{ using iso::begin; using iso::end; return tree_assign(*this, begin(t), end(t)); }

#ifdef USE_RVALUE_REFS
	multimap(multimap &&b) = default;
	multimap&	operator=(multimap &&b)			{ clear(); B::assign(move(b)); return *this; }
#endif

	multimap&	operator+=(multimap &t2) {
		for (auto i = t2.begin(), e = t2.end(); i != e; ++i)
			insert(*i);
		return *this;
	}

	V&		operator[](const K &k) {
		tree_stack<N>	s(B::root());
		s.scan_upper_bound(k, B::comp());
		int	dir;
		if (!s.node()) {
			s.last(B::root(), 1);
			dir = 1;
		} else {
			dir = s.next_leaf(0);
		}
		N	*n = new N(k);
		B::insert(s, n, dir);
		return n->value();
	}

	iterator		begin()				{ return iterator(B::root(), 0); }
	iterator		end()				{ return 0; }
	const_iterator	begin()		const	{ return const_iterator(B::root(), 0); }
	const_iterator	end()		const	{ return 0; }

	B&				with_keys()			{ return *this; }
	const B&		with_keys()	const	{ return *this; }

	template<typename K2> iterator			lower_bound(const K2 &k)			{ return iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K2> const_iterator	lower_bound(const K2 &k)	const	{ return const_iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K2> iterator			upper_bound(const K2 &k)			{ return iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K2> const_iterator	upper_bound(const K2 &k)	const	{ return const_iterator(B::root()).scan_upper_bound(k, B::comp()); }
	template<typename K2> iterator			find(const K2 &k)					{ auto i = lower_bound(k); return i && !B::compare(k, i.key()) ? i : 0; }
	template<typename K2> const_iterator	find(const K2 &k)			const	{ auto i = lower_bound(k); return i && !B::compare(k, i.key()) ? i : 0; }
	template<typename K2> uint32			count(const K2 &k)			const	{
		uint32	n = 0;
		for (auto i = const_iterator(B::root()).scan_lower_bound(k); i && !B::compare(k, i.key()); ++i)
			++n;
		return n;
	}
	template<typename K2> range<const_iterator>	bounds(const K2 &k)		const	{
		auto i = const_iterator(B::root()).scan_lower_bound(k, B::comp());
		auto j = const_iterator(i).scan_upper_bound(k, B::comp());
		return range<const_iterator>(i, j);
	}

//friends
	template<typename K2> friend iterator		lower_boundc(multimap &m, const K2 &k)			{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	lower_boundc(const multimap &m, const K2 &k)	{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(multimap &m, const K2 &k)			{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const multimap &m, const K2 &k)	{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(multimap &m, const K2 &k)					{ return m.find(k); }
	template<typename K2> friend const_iterator	find(const multimap &m, const K2 &k)			{ return m.find(k); }
	template<typename K2> friend uint32			count(const multimap &m, const K2 &k)			{ return m.count(k); }
};

//-----------------------------------------------------------------------------
// set
//-----------------------------------------------------------------------------

template<typename K> struct set_node : public rbnode_base<set_node<K>, false> {
	K	k;
	set_node(const K &_k) : k(_k)	{}
	K&			get()				{ return k;	}
	const K&	get()	const		{ return k;	}
};

template<typename K, typename C = less> class set : public rbtree_base<set_node<K>, C> {
	typedef set_node<K> 		N;
	typedef rbtree_base<N, C>	B;
	using typename B::BI;

	bool check_all(const set &b) const	{
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (!b.find(*i))
				return false;
		}
		return true;
	}
public:
	class iterator : public BI {
	public:
		typedef bidirectional_iterator_t iterator_category;
		typedef K		element, &reference;
		iterator(N *p = nullptr)	: BI(p)			{}
		iterator(N *p, int side)	: BI(p, side)	{}
		operator		K*()				const	{ N *p = BI::node(); return p ? &p->get() : 0;	}
		K*				operator->()		const	{ N *p = BI::node(); return p ? &p->get() : 0;	}
		iterator&		operator++()				{ BI::next(1); return *this;	}
		iterator		operator++(int)				{ iterator i = *this; BI::next(1); return i; }
		iterator&		operator--()				{ BI::next(0); return *this;	}
		iterator		operator--(int)				{ iterator i = *this; BI::next(0); return i; }
		template<typename K2> iterator& scan_lower_bound(const K2 &k, const C &comp) { BI::scan_lower_bound(k, comp); return *this; }
	};
	typedef iterator	const_iterator;
	typedef K			element, &reference;

	set(const C &comp = C()) : B(comp)	{}
	set(const set &b) : B(b)			{}
	~set()								{ B::deleteall(); }
	void	clear()						{ B::deleteall(); }
	set&	operator=(const set &b)		{ clear(); B::assign(b); return *this; }
	template<typename T> set&	operator=(const T &t)	{ using iso::begin; using iso::end; return tree_assign(*this, begin(t), end(t)); }
#ifdef USE_RVALUE_REFS
	set(set &&b) = default;
	set&	operator=(set &&b)			{ clear(); B::assign(move(b)); return *this; }
#endif

	bool			insert(const K &k) {
		auto	i	= begin();
		//tree_stack<N>	s(B::root());
		if (i.scan(k, B::comp()))
			return false;
		B::insert(i, new N(k));
		return true;
	}
	iterator		insert_it(const K &k) {
		auto	i	= begin();
		if (i.scan(k, B::comp()))
			return i;
		B::insert(i, new N(k));
		return lower_bound(k);
	}

	iterator		begin()				{ return iterator(B::root(), 0); }
	iterator		end()				{ return 0; }
	const_iterator	begin()		const	{ return const_iterator(B::root(), 0); }
	const_iterator	end()		const	{ return 0; }

	_not<set> operator~()	const {
		return *this;
	}
	set& operator&=(const set &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (!b.find(*i))
				remove(i);
		}
		return *this;
	}
	set& operator&=(const _not<set> &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (b.t.find(*i))
				remove(i);
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
				remove(j);
			else
				insert(i);
		}
		return *this;
	}

	set& operator*=(const set &b)			{ return operator&=(b); }
	set& operator+=(const set &b)			{ return operator|=(b); }
	set& operator-=(const set &b)			{ return operator&=(~b); }

	bool operator< (const set &b)	const	{ return B::size() <  b.size() && check_all(b); }
	bool operator<=(const set &b)	const	{ return B::size() <= b.size() && check_all(b); }
	bool operator> (const set &b)	const	{ return b <  *this; }
	bool operator>=(const set &b)	const	{ return b <= *this; }

	template<typename K2> iterator			lower_bound(const K2 &k)			{ return iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K2> const_iterator	lower_bound(const K2 &k)	const	{ return const_iterator(B::root()).scan_lower_bound(k, B::comp()); }
	template<typename K2> iterator			find(const K2 &k)					{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }
	template<typename K2> const_iterator	find(const K2 &k)			const	{ auto i = lower_bound(k); return i && !B::compare(k, *i) ? i : 0; }
	template<typename K2> bool				count(const K2 &k)			const	{ auto i = lower_bound(k); return i && !B::compare(k, *i); }

//friends
	friend set operator&(const set &a, const set &b)		{ set t(a); t &= b; return t; }
	friend set operator&(const set &a, const _not<set> &b)	{ set t(a); t &= b; return t; }
	friend set operator|(const set &a, const set &b)		{ set t(a); t &= b; return t; }
	friend set operator^(const set &a, const set &b)		{ set t(a); t ^= b; return t; }

	template<typename K2> friend iterator		lower_boundc(set &m, const K2 &k)		{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	lower_boundc(const set &m, const K2 &k)	{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(set &m, const K2 &k)		{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const set &m, const K2 &k)	{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(set &m, const K2 &k)				{ return m.find(k); }
	template<typename K2> friend const_iterator	find(const set &m, const K2 &k)			{ return m.find(k); }
	template<typename K2> friend bool			count(const set &m, const K2 &k)		{ return m.count(k); }
};

//-----------------------------------------------------------------------------
// multiset
//-----------------------------------------------------------------------------

template<typename K> struct multiset_node : public rbnode_base<multiset_node<K>, false> {
	K		k;
	uint32	count;
	multiset_node(const K &k) : k(k), count(1) {}
	K&			get()				{ return k;	}
	const K&	get()	const		{ return k;	}
};

template<typename K, typename C = less> class multiset : public rbtree_base<multiset_node<K>, C> {
	typedef multiset_node<K>	N;
	typedef rbtree_base<N, C>	B;
	using typename B::BI;

	bool check_all(const multiset &b) const	{
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (b.count(*i) < i.count())
				return false;
		}
		return true;
	}
public:
	class iterator : public BI {
	public:
		typedef bidirectional_iterator_t iterator_category;
		typedef K		element, &reference;
		iterator(N *p)				: BI(p)			{}
		iterator(N *p, int side)	: BI(p, side)	{}
		operator		K*()				const	{ N *p = BI::node(); return p ? &p->get() : 0;	}
		K*				operator->()		const	{ N *p = BI::node(); return p ? &p->get() : 0;	}
		iterator&		operator++()				{ BI::next(1); return *this;	}
		iterator		operator++(int)				{ iterator i = *this; BI::next(1); return i; }
		iterator&		operator--()				{ BI::next(0); return *this;	}
		iterator		operator--(int)				{ iterator i = *this; BI::next(0); return i; }
		uint32			count()				const	{ return B::node()->count; }
		template<typename K2> iterator& scan_lower_bound(const K2 &k) { BI::scan_lower_bound(k); return *this; }
	};
	typedef iterator	const_iterator;
	typedef K			element, &reference;

	multiset(const C &comp = C()) : B(comp)		{}
	multiset(const multiset &b)	: B(b)			{}
	~multiset()									{ B::deleteall(); }
	void	clear()								{ B::deleteall(); }
	multiset&	operator=(const multiset &b)	{ clear(); B::assign(b); return *this; }
	template<typename T> multiset&	operator=(const T &t)	{ using iso::begin; using iso::end; return tree_assign(*this, begin(t), end(t)); }
#ifdef USE_RVALUE_REFS
	multiset(multiset &&b) = default;
	multiset&	operator=(multiset &&b)			{ clear(); B::assign(move(b)); return *this; }
#endif

	void			insert(const K &k) {
		tree_stack<N>	s(B::root());
		if (s.scan(k)) {
			++s.node()->count;
		} else {
			B::insert(s, new N(k));
		}
	}

	iterator		begin()				{ return iterator(B::root(), 0); }
	iterator		end()				{ return 0; }
	const_iterator	begin()		const	{ return const_iterator(B::root(), 0); }
	const_iterator	end()		const	{ return 0; }

	multiset& operator+=(const multiset &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (auto j = b.find(*i))
				i.node()->count += j.count();
		}
		return *this;
	}
	multiset& operator-=(const multiset &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (auto j = b.s.find(*i)) {
				if (i.node()->count < j.count())
					remove(i);
				else
					i.node()->count -= j.count();
			}
		}
		return *this;
	}
	multiset& operator*=(const multiset &b) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (auto j = b.s.find(*i))
				i.node()->count = min(i.node()->count, j.count());
			else
				remove(i);
		}
		return *this;
	}

	bool operator< (const multiset &b)	const	{ return B::size() <  b.size() && check_all(b); }
	bool operator<=(const multiset &b)	const	{ return B::size() <= b.size() && check_all(b); }
	bool operator> (const multiset &b)	const	{ return b <  *this; }
	bool operator>=(const multiset &b)	const	{ return b <= *this; }

	template<typename K2> iterator				lower_bound(const K2 &k)			{ return iterator(B::root()).scan_lower_bound(k); }
	template<typename K2> const_iterator		lower_bound(const K2 &k)	const	{ return const_iterator(B::root()).scan_lower_bound(k); }
	template<typename K2> iterator				find(const K2 &k)					{ auto i = lower_bound(k); return !B::compare(k, *i) ? i : 0; }
	template<typename K2> const_iterator		find(const K2 &k)			const	{ auto i = lower_bound(k); return !B::compare(k, *i) ? i : 0; }
	template<typename K2> uint32				count(const K2 &k)			const	{ auto i = lower_bound(k); return !B::compare(k, *i) ? i.count() : 0; }

//friends
	friend multiset operator+(const multiset &a, const multiset &b)	{ multiset t(a); t += b; return t; }
	friend multiset operator-(const multiset &a, const multiset &b)	{ multiset t(a); t -= b; return t; }
	friend multiset operator*(const multiset &a, const multiset &b)	{ multiset t(a); t *= b; return t; }

	template<typename K2> friend iterator		lower_boundc(multiset &m, const K2 &k)			{ return m.lower_bound(k); }
	template<typename K2> friend const_iterator	lower_boundc(const multiset &m, const K2 &k)	{ return m.lower_bound(k); }
	template<typename K2> friend iterator		upper_boundc(multiset &m, const K2 &k)			{ return m.upper_bound(k); }
	template<typename K2> friend const_iterator	upper_boundc(const multiset &m, const K2 &k)	{ return m.upper_bound(k); }
	template<typename K2> friend iterator		find(multiset &m, const K2 &k)					{ return m.find(k); }
	template<typename K2> friend const_iterator	find(const multiset &m, const K2 &k)			{ return m.find(k); }
	template<typename K2> friend uint32			count(const multiset &m, const K2 &k)			{ return m.count(k); }
};

//-----------------------------------------------------------------------------
// interval_tree
//-----------------------------------------------------------------------------

template<template<typename> class B, typename N, typename K> struct interval_node_base : B<N>, interval<K> {
	K		max;
	interval_node_base(const interval<K> &i) : interval<K>(i), max(i.b) {}
	void	attach(N *n, int side) {
		K	m = this->b;
		if (N *s = this->child[!side]) {
			if (s->max > m)
				m = s->max;
		}
		if (n && n->max > m)
			m = n->max;
		max			= m;
		this->child[side] = n;
	}
	interval<K>			tree_interval()	const	{ return interval<K>(this->a, max);	}
	const interval<K>&	get()			const	{ return *this;	}
};

template<typename N, typename I, typename K> class interval_iterator : public I {
	using I::sp;

	interval<K>	i;

	void	fix(int side) {
		if (N *n = this->node()) {
			K	m	= n->b;
			if (m > n->max) {
				for (N **j = sp; depth(j--) > 0 && m > j[0]->max;)
					j[0]->max = m;
			} else {
				for (N **j = sp; depth(j--) > 0 && m < j[0]->max;) {
					if ((n = j[0]->child[side]) && n->max > m)
						m = n->max;
				}
			}
		}
	}
	int	last(N *p, int side) {
		N	**sp0 = sp;
		if (side == 0) {
			while (p && !(p->max < i.a)) {
				push(p);
				p = p->child[side];
			}
		} else {
			while (p && p->a < i.b) {
				push(p);
				p = p->child[side];
			}
		}
		return sp - sp0;
	}
	void	next(int side) {
		fix(side);
		do {
			if (this->depth() > 0 && last(sp[-1]->child[side], !side) == 0) {
				do
					--sp;
				while (this->depth() > 0 && sp[0] == sp[-1]->child[side]);
			}
		} while (this->depth() > 0 && !overlap(i, *this->node()));
	}
public:
	interval_iterator(N *p, const interval<K> &_i, int side) : I(0), i(_i) {
		if (last(p, side) && !overlap(i, *this->node()))
			next(1);
	}
	interval_iterator(N *p, const interval<K> &_i) : I(p), i(_i) {}
	interval_iterator&	operator++()			{ next(1); return *this;	}
	interval_iterator	operator++(int)			{ I i = *this; next(1); return i; }
	interval_iterator&	operator--()			{ next(0); return *this;	}
	interval_iterator	operator--(int)			{ I i = *this; next(0); return i; }
	interval<K>&		key()					{ return unconst(I::node()->get()); }
};

template<typename T, typename K> interval_iterator<typename T::node_type, typename T::iterator, K>	interval_begin(T &tree, const interval<K> &x)	{
	return interval_iterator<typename T::node_type, typename T::iterator, K>(tree.root(), x, 0);
}

template<typename T, typename K> interval_iterator<typename T::node_type, typename T::iterator, K>	interval_begin(T &tree, const K &a, const K &b)	{
	return interval_begin(tree, interval<K>(a, b));
}

template<typename N> using rbnode_base0 = rbnode_base<N, false>;
template<typename K, typename V> struct interval_node : interval_node_base<rbnode_base0, interval_node<K, V>, K> {
	typedef	interval_node_base<rbnode_base0, interval_node<K, V>, K>	B;
	V		v;
	interval_node(const interval<K> &i)				: B(i)			{}
	interval_node(const interval<K> &i, const V &v) : B(i), v(v)	{}
};

template<typename K, typename V> class interval_tree : public rbtree_base<interval_node<K, V>, less> {
	typedef interval_node<K, V> 	N;
	typedef rbtree_base<N, less>	B;
public:
	class iterator : public tree_stack<N> {
		typedef tree_stack<N> B;
	public:
		iterator(N *p)				: B(p)			{}
		iterator(N *p, int side)	: B(p, side)	{}
		operator		V*()				const	{ N *p = B::node(); return p ? &p->v : 0;	}
		V*				operator->()		const	{ N *p = B::node(); return p ? &p->v : 0;	}
		iterator&		operator++()				{ B::next(1); return *this;	}
		iterator		operator++(int)				{ iterator i = *this; B::next(1); return i; }
		iterator&		operator--()				{ B::next(0); return *this;	}
		iterator		operator--(int)				{ iterator i = *this; B::next(0); return i; }
		const interval<K> &key()			const	{ return B::node()->get(); }
//		template<typename K2> iterator& scan_lower_bound(const interval<K2> &k) { B::scan_lower_bound(k); return *this; }
//		template<typename K2> iterator& scan_lower_bound(const K2 &k)			{ return scan_lower_bound(interval<K>(k, k)); }
		template<typename K2> iterator& scan_lower_bound(const K2 &k)			{ B::scan_lower_bound(k, less()); return *this; }
	};

	typedef iterator	const_iterator;
	typedef V			element, &reference;
	typedef	interval_iterator<N, iterator, K>	iterator2;

	interval_tree()				: B(less()) {}
	~interval_tree()			{ B::deleteall(); }
	void	clear()				{ B::deleteall(); }

	void	insert(const interval<K> &i, const V &v) {
		tree_stack<N>	s(B::root());
		s.scan_upper_bound(i, B::comp());
		int	dir;
		if (!s.node()) {
			s.last(B::root(), 1);
			dir = 1;
		} else {
			dir = s.next_leaf(0);
		}
		B::insert(s, new N(i, v), dir);
	}

	bool	remove(const interval<K> &i) {
		tree_stack<N>	s(B::root());
		if (!s.scan(i, B::comp()))
			return false;
		B::remove(s);
		return true;
	}

	void	insert(const K &a, const K &b, const V &v)	{ return insert(interval<K>(a, b), v); }
	bool	remove(const K &a, const K &b)				{ return remove(interval<K>(a, b)); }
	void	remove(iterator &i)							{
		auto *n = i.node();
		B::remove(i);
		i.scan_lower_bound(n->a);
		delete n;
	}

	iterator			begin()							{ return iterator(B::root(), 0); }
	iterator			end()							{ return 0; }
	const_iterator		begin()		const				{ return const_iterator(B::root(), 0); }
	const_iterator		end()		const				{ return 0; }

	iterator2			begin(const interval<K> &x)		{ return iterator2(B::root(), x, 0); }
	iterator2			begin(const K &a, const K &b)	{ return begin(interval<K>(a, b)); }

	template<typename K2> iterator			lower_bound(const K2 &k)			{ return iterator(B::root()).scan_lower_bound(k); }
	template<typename K2> const_iterator	lower_bound(const K2 &k)	const	{ return iterator(B::root()).scan_lower_bound(k); }

	template<typename K2> friend iterator		lower_boundc(interval_tree &t, const K2 &k)			{ return t.lower_bound(k); }
	template<typename K2> friend const_iterator	lower_boundc(const interval_tree &t, const K2 &k)	{ return t.lower_bound(k); }

	template<typename F> friend void intersect(interval_tree &t0, interval_tree &t1, F f) {
		for (auto i0 = t0.begin(t1.root()->tree_interval()); i0; ++i0) {
			for (auto i1 = t1.begin(i0.node()->tree_interval()); i1; ++i1)
				f(*i0, *i1);
		}
	}
};

//interval map just needs this:

template<typename K, typename V> struct map_node<interval<K>, V> : public interval_node_base<rbnode_base0, map_node<interval<K>, V>, K> {
	typedef interval_node_base<rbnode_base0, map_node<interval<K>, V>, K> B;
	V		v;
	template<typename T> map_node(const interval<K> &i, T &&v)	: B(i), v(forward<T>(v))	{}
	explicit map_node(const interval<K> &i)		: B(i)			{}
	const V&			value()		const	{ return v;	}
	V&					value()				{ return v;	}
};

}//namespace iso

#endif	// TREE_H
