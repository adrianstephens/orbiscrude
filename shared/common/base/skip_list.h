#ifndef SKIP_LIST_H
#define SKIP_LIST_H

#include "base/bits.h"

namespace iso {

class skip_list_base {
protected:
	struct node {
		node	*next[1];
	};
	uint32		level;

	static void	remove(node *n, node &update, int level) {
		for (uint32 i = 0; i <= level && update.next[i] == n; i++)
			update.next[i] = n->next[i];
	}
	static void	insert(node *n, node &update, int level) {
		for (uint32 i = 0; i <= level; i++) {
			node	*prev	= update.next[i];
			n->next[i]		= prev->next[i];
			prev->next[i]	= n;
		}
	}
	static void succ(node &update, int level) {
		node	*next	= update.next[0]->next[0];
		update.next[0]	= next;
		for (int i = 1; i <= level/* && next*/ && update.next[i]->next[i] == next; ++i)
			update.next[i]	= next;
	}
	static void pred(node &update, const node &head, int level) {
		node	*end	= update.next[0];
		int		i		= 1;
		while (i <= level && update.next[i] == end)
			++i;
		node *curr = end == 0 || i > level || update.next[i] == 0 ? const_cast<node*>(&head) : update.next[i];
		while (i--) {
			node	*next;
			while ((next = curr->next[i]) && next != end)
				curr = next;
			update.next[i] = curr;
		}
	}
	uint32	random_level(node &update, int max_level, node &head) {
		static uint64	seed = 31415926535L;
		seed = 1664525L * seed + 1013904223L;
		uint32	level2	= lowest_set_index(uint32(seed));
		if (level2 > level) {
			if (level < max_level)
				update.next[++level] = &head;
			level2 = level;
		}
		return level2;
	}

	skip_list_base() : level(0) {}
};

template<typename K, typename V, int M = sizeof(K) * 8> class skip_list : skip_list_base {
	friend class bi_iterator;
	typedef skip_list_base					B;
	typedef typename T_inheritable<V>::type	VI;

	struct nodelist : B::node {
		node	*_[M];
	};
	struct keynode {
		K		key;
		keynode(const K &_key) : key(_key) {}
	};
	struct node : VI, keynode, B::node {
		node(const K &_key) : keynode(_key) {}
		void	*operator new(size_t size, int level)	{ return VI::operator new(size + level * sizeof(B::node*)); }
	};

	nodelist	head;

	static B::node*	_find_lower(const K &key, nodelist &update, B::node *curr, int level);
	static B::node*	_find_upper(const K &key, nodelist &update, B::node *curr, int level);
	static B::node*	_find_lower(const K &key, nodelist &update, B::node *curr, const nodelist &last, int level);
	static B::node*	_find_upper(const K &key, nodelist &update, B::node *curr, const nodelist &last, int level);

	node*		find(const K &key, nodelist &update) const {
		typename B::node *n = _find_lower(key, update, const_cast<nodelist*>(&head), level);
		node *n2 = static_cast<node*>(n->next[0]);
		return n2 && n2->key == key ? n2 : 0;
	}
	void		deleteall() const {
		for (B::node *n = head.next[0], *n2; n; n = n2) {
			n2 = n->next[0];
			delete static_cast<node*>(n);
		}
	}

public:
	struct iterator {
		node	*n;
		iterator(node *_n) : n(_n)	{}
		iterator&	operator++()			{ n = static_cast<node*>(n->next[0]); return *this; }
		iterator	operator++(int)			{ node *n0 = n; n = static_cast<node*>(n->next[0]); return n0; }
		V&			operator*()		const	{ return *n; }
		V*			operator->()	const	{ return n; }
		operator	V*()			const	{ return n; }
		const K&	key()			const	{ return n->key; }
	};

	class bi_iterator {
		friend skip_list;
		nodelist	update;
		const skip_list	&list;
		node		*get()			const	{ return static_cast<node*>(update.next[0]); }
		VI			*getv()			const	{ return get(); }

	public:
		bi_iterator(const skip_list &_list) : list(_list) {}
		bi_iterator &operator=(const bi_iterator &b) { update = b.update; return *this; }
		bi_iterator &operator++()			{ B::succ(update, list.level); return *this; }
		bi_iterator &operator--()			{ B::pred(update, list.head, list.level); return *this; }
		iterator	operator++(int)			{ node *n = get(); B::succ(update, list.level); return n; }
		iterator	operator--(int)			{ node *n = get(); B::pred(update, list.head, list.level); return n; }
		V&			operator*()		const	{ return *getv(); }
		V*			operator->()	const	{ return getv(); }
		operator	V*()			const	{ return getv(); }
		const K&	key()			const	{ return get()->key; }
		bi_iterator &remove()				{ B::remove(update.next[0], update, list.level); return *this; }

		bi_iterator &scan_lower_bound(const K &key) {
			skip_list::_find_lower(key, update, update.next[0], list.level);
			B::succ(update, list.level);
			return *this;
		}
		bi_iterator &scan_upper_bound(const K &key) {
			skip_list::_find_upper(key, update, update.next[0], list.level);
			return *this;
		}
		bi_iterator &scan_lower_bound(const K &key, const bi_iterator &end) {
			skip_list::_find_lower(key, update, update.next[0], end.update, list.level);
			B::succ(update, list.level);
			return *this;
		}
		bi_iterator &scan_upper_bound(const K &key, const bi_iterator &end) {
			skip_list::_find_upper(key, update, update.next[0], end.update, list.level);
			return *this;
		}

		friend bi_iterator lower_bound(const bi_iterator &first, const bi_iterator &last, const K &key) { return bi_iterator(first).scan_lower_bound(key, last); }
		friend bi_iterator upper_bound(const bi_iterator &first, const bi_iterator &last, const K &key) { return bi_iterator(first).scan_upper_bound(key, last); }
	};

	typedef V			element;
	typedef iterator	const_iterator;

	skip_list()							{ clear(head);	}
	~skip_list()						{ deleteall();	}
	void		reset()					{ deleteall(); clear(head); }
	uint32		current_level()	const	{ return level; }
	iterator	begin()					{ return static_cast<node*>(head.next[0]); }
	iterator	end()					{ return 0; }
	bi_iterator	bi_begin()				{ bi_iterator i(bi_head()); B::succ(i.update, level); return i; }
	bi_iterator	bi_end()				{ bi_iterator i(*this); clear(i.update); return i; }

	V*			check(const K &key) const {
		nodelist update;
		if (node *n = find(key, update))
			return n;
		return 0;
	}

	V&			operator[](const K &key) {
		nodelist update;
		node *n = find(key, update);
		if (!n) {
			uint32 level	= B::random_level(update, M - 1, head);
			n				= new(level) node(key);
			B::insert(n, update, level);
		}
		return *n;
	}

	V&			add(const K &key) {
		nodelist update;
		_find_lower(key, update, const_cast<nodelist*>(&head), level);
		uint32 level	= B::random_level(update, M - 1, head);
		node	*n		= new(level) node(key);
		B::insert(n, update, level);
		return *n;
	}

	bool		remove(const K &key) {
		nodelist update;
		if (node *n = find(key, update)) {
			B::remove(n, update, B::level);
			delete n;
			return true;
		}
		return false;
	}

	bi_iterator	bi_head() {
		bi_iterator	i(*this);
		for (int j = 0; j <= level; j++)
			i.update.next[j] = &head;
		return i;
	}

	friend bi_iterator	lower_boundc(skip_list &list, const K &key) { return list.bi_head().scan_lower_bound(key); }
	friend bi_iterator	upper_boundc(skip_list &list, const K &key) { return list.bi_head().scan_upper_bound(key); }
};

template<typename K, typename V, int M> skip_list_base::node *skip_list<K, V, M>::_find_lower(const K &key, nodelist &update, B::node *curr, int level) {
	for (int i = level; i >= 0; --i) {
		B::node	*next;
		while ((next = curr->next[i]) && static_cast<node*>(next)->key < key)
			curr = next;
		update.next[i] = curr;
	}
	return curr;
}

template<typename K, typename V, int M> skip_list_base::node *skip_list<K, V, M>::_find_upper(const K &key, nodelist &update, B::node *curr, int level) {
	for (int i = level; i >= 0; --i) {
		B::node	*next;
		while ((next = curr->next[i]) && static_cast<node*>(next)->key <= key)
			curr = next;
		update.next[i] = curr;
	}
	return curr;
}

template<typename K, typename V, int M> skip_list_base::node *skip_list<K, V, M>::_find_lower(const K &key, nodelist &update, B::node *curr, const nodelist &last, int level) {
	for (int i = level; i >= 0; --i) {
		B::node	*end	= last.next[i], *next;
		while ((next = curr->next[i]) != end && static_cast<node*>(next)->key < key)
			curr = next;
		update.next[i] = curr;
	}
	return curr;
}

template<typename K, typename V, int M> skip_list_base::node *skip_list<K, V, M>::_find_upper(const K &key, nodelist &update, B::node *curr, const nodelist &last, int level) {
	for (int i = level; i >= 0; --i) {
		B::node	*end	= last.next[i], *next;
		while ((next = curr->next[i]) != end && static_cast<node*>(next)->key <= key)
			curr = next;
		update.next[i] = curr;
	}
	return curr;
}

} //namespace iso

#endif	// SKIP_LIST_H
