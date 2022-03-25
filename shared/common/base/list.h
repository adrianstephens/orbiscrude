#ifndef LIST_H
#define LIST_H

#include "defs.h"

namespace iso {

//-----------------------------------------------------------------------------
// doubly linked list
//-----------------------------------------------------------------------------
template<typename L> struct link_base {
	union {
		struct { L *prev, *next; };
		L	*link[2];
	};
	L		*get()				{ return static_cast<L*>(this); }
	const L	*get()		const	{ return static_cast<const L*>(this); }
	bool	is_linked()	const	{ return next != 0; }

	void	join(link_base *link)		{ next = link->get(); link->prev = get(); }
	void	insert_after(L *link)		{ link->join(next); join(link); }
	void	insert_before(L *link)		{ prev->join(link); link->join(get()); }
	void	insert_after(L *a, L *b)	{ b->join(next); join(a); }
	void	insert_before(L *a, L *b)	{ prev->join(a); b->join(get()); }
	L		*unlink()					{ prev->join(next); next = nullptr; return get(); }
	L		*unlink_next()				{ return next->unlink(); }
	void	unlink(L *to)				{ prev->join(to); }
	void	replace(L *b)				{ b->join(next); prev->join(b); }
	void	init()						{ prev = next = get(); }
};

template<typename T> struct link_payload {
	static	T*		get(T *p)	{ return p; }
};
template<typename T> struct link_payload<const T> {
	static	auto	get(const T *p)	{ return make_const(link_payload<T>::get(const_cast<T*>(p))); }
};

template<typename T> auto get_payload(T *p) {
	return link_payload<T>::get(p);
}

template<typename L> class list_iterator_base {
protected:
	L				*p;
public:
	typedef bidirectional_iterator_t iterator_category;	// looks like random access because we defined +/-

	explicit list_iterator_base(L *p = 0) : p(p) {}
	list_iterator_base(const link_base<noconst_t<L>> &p) : p(const_cast<L*>(p.get()))	{}
	list_iterator_base(const list_iterator_base<noconst_t<L>> &i) : p(i.link())	{}
	L*		link()					const	{ return p; }
	auto	get()					const	{ return get_payload(p); }
	explicit operator bool()		const	{ return !!p; }
//	operator decltype(auto)()		const	{ return get(); }
	decltype(auto)	operator*()		const	{ return *get(); }
	auto	operator->()			const	{ return get(); }
	auto&	operator++()					{ p = p->next; return *this; }
	auto	operator++(int)					{ auto i = *this; p = p->next; return i; }
	auto&	operator--()					{ p = p->prev; return *this; }
	auto	operator--(int)					{ auto i = *this; p = p->prev; return i; }
	auto&	operator+=(int i)				{ while (i--) p = p->next; return *this; }
	auto&	operator-=(int i)				{ while (i--) p = p->prev; return *this; }
	bool	operator==(const list_iterator_base &b) const { return p == b.p; }
	bool	operator!=(const list_iterator_base &b) const { return p != b.p; }

	auto	operator+(int i)		const	{ return list_iterator_base(*this) += i; }
	auto	operator-(int i)		const	{ return list_iterator_base(*this) -= i; }

	void	insert_after(L *i)	{ ISO_CHEAPASSERT(!i->is_linked()); p->insert_after(i); }
	void	insert_before(L *i)	{ ISO_CHEAPASSERT(!i->is_linked()); p->insert_before(i); }

	void	insert_after(list_iterator_base i)	{ i.p->unlink(); p->insert_after(i.p); }
	void	insert_before(list_iterator_base i)	{ i.p->unlink(); p->insert_before(i.p); }
	void	insert_after(list_iterator_base b, list_iterator_base e) {
		if (e != b) {
			L	*ep	= e.p->prev;
			b.p->unlink(e.p);
			p->insert_after(b.p, ep);
		}
	}
	void	insert_before(list_iterator_base b, list_iterator_base e) {
		if (e != b) {
			L	*ep	= e.p->prev;
			b.p->unlink(e.p);
			p->insert_before(b.p, ep);
		}
	}
	L		*remove() {
		L	*i = p;
		p = i->prev;
		i->unlink();
		return i;
	}
	void	replace(L *b) {
		p->replace(b);
		p = b;
	}
};

template<typename L> class list_base {
protected:
	link_base<L>	head;
	void			reset()		{ head.init(); }
public:
	typedef list_iterator_base<L>		iterator;
	typedef list_iterator_base<const L>	const_iterator;

	list_base() { reset(); }
	list_base(list_base &&b) {
		if (b.empty()) {
			reset();
		} else {
			head = b.head;
			head.next->prev = head.prev->next = head.get();
			b.reset();
		}
	}
	void operator=(list_base &&b) {
		if (b.empty()) {
			reset();
		} else {
			head = b.head;
			head.next->prev = head.prev->next = head.get();
			b.reset();
		}
	}

	list_base(iterator begin, iterator end) {
		L	*b = begin.link(), *e = end.link();
		if (b != e) {
			head.next		= b;
			head.prev		= e->prev;
			b->prev->next	= e;
			e->prev			= b->prev;
			b->prev			= head.prev->next = head.get();
		} else {
			head.init();
		}
	}

	const_iterator	begin()		const		{ return const_iterator(head.next); }
	const_iterator	end()		const		{ return const_iterator(head); }
	iterator		begin()					{ return iterator(head.next); }
	iterator		end()					{ return iterator(head); }
	auto&			front()		const		{ return *begin(); }
	auto&			back()		const		{ return *const_iterator(head.prev); }
	auto&			front()					{ return *begin(); }
	auto&			back()					{ return *iterator(head.prev); }
	bool			empty()		const		{ return begin() == end(); }

	L*				push_front(L *t)		{ head.get()->insert_after(t); return t; }
	L*				push_back(L *t)			{ head.get()->insert_before(t); return t; }
	L*				pop_front()				{ return head.next->unlink(); }
	L*				pop_back()				{ return head.prev->unlink(); }
	static L*		remove(iterator i)		{ return i.unlink(); }
	auto&			operator[](int i)		{ return *(begin() += i); }
	auto&			operator[](int i) const	{ return *(begin() += i); }

	void			append(list_base &&b)	{ end().insert_before(b.begin(), b.end()); }
	void			prepend(list_base &&b)	{ begin().insert_before(b.begin(), b.end()); }

	iterator		erase(iterator first, iterator last) {
		first.link()->prev->join(last.link());
		return last;
	}

	list_base		extract(iterator first, iterator last) {
		first.link()->prev->join(last.link());
		return list_base(first, last);
	}

	size_t			size()					const;
	uint32			size32()				const { return uint32(size()); }
	int				index_of(const L *t)	const;
	bool			contains(const L *t)	const { return index_of(t) != -1; }
	bool			validate()				const;

	friend void swap(list_base &a, list_base &b) {
		link_base<L>	ahead = a.head;

		if (b.empty()) {
			a.reset();
		} else {
			a.head = b.head;
			a.head.next->prev = a.head.prev->next = a.head.get();
		}

		if (ahead.next == &a.head) {
			b.reset();
		} else {
			b.head = ahead;
			ahead.next->prev = ahead.prev->next = b.head.get();
		}
	}
};

template<typename L> size_t list_base<L>::size() const {
	size_t	size = 0;
	for (const_iterator	i = begin(), e = end(); i != e; ++i)
		++size;
	return size;
}

template<typename L> int list_base<L>::index_of(const L *t) const {
	int	x = 0;
	for (const_iterator i = begin(), e = end(); i != e; ++i, ++x) {
		if (i.link() == t)
			return x;
	}
	return -1;
}

template<typename L> bool list_base<L>::validate() const {
	if (head.next->prev != head.get() || head.prev->next != head.get())
		return false;

	for (const_iterator	i = begin(); i != end(); ) {
		const_iterator	p = i, n = ++i;
		if (p != --n)
			return false;
	}
	return true;
}

template<typename L, class C> void sort(list_iterator_base<L> lo, list_iterator_base<L> hi, C comp) {
	--lo;
	for (int chunk = 1;; chunk <<= 1) { // For each power of two
		int	num_merges = 0;

		for (list_iterator_base<L> i = lo, r = ++i; i != hi; i = r) {
			num_merges++;
			// Find next sublist
			for (int n = chunk; n-- && r != hi; ++r);

			// Merge two sublists
			int	nl = chunk, nr = chunk;
			while (nl && nr && r != hi) {
				if (comp(*r, *i)) {
					i.insert_before(r++);
					--nr;
				} else {
					++i;
					--nl;
				}
			}
			while (nr-- && r != hi)
				++r;
		}
		if (num_merges < 2)
			break;
	}
}


template<typename L, class P> auto remove(list_iterator_base<L> i, list_iterator_base<L> end, P pred) {
	while (i != end && !pred(*i))
		++i;

	auto	last = i;
	while (i != end) {
		auto	prev = i++;
		if (!pred(*prev))
			last.insert_before(prev);
	}
	return last;
}

template<typename L> void reverse(list_iterator_base<L> begin, list_iterator_base<L> end) {
	L	*i = begin.link(), *b = end.link();
	i->prev->next	= b->prev;
	b->prev			= i;
	while (i != b) {
		swap(i->prev, i->next);
		i = i->prev;
	}
}


//-------------------------------------
// e_list
//-------------------------------------

template<typename T> struct e_link : public link_base<T> {
	e_link()	{ this->next = 0; }
	~e_link()	{ if (this->next) this->unlink(); }
	auto	iterator() const	{ ISO_CHEAPASSERT(this->is_linked()); return typename list_base<T>::const_iterator(static_cast<const T*>(this)); }
	auto	iterator()			{ ISO_CHEAPASSERT(this->is_linked()); return typename list_base<T>::iterator(static_cast<T*>(this)); }
};

template<typename T> class e_list : public list_base<T> {
	typedef list_base<T>	B;
public:
	using typename B::iterator;
	e_list()				{}
	e_list(const _none&)	{}
	e_list(iterator begin, iterator end) : B(begin, end)	{}
	e_list(e_list &&b) : B(move(b))			{}
	~e_list()				{ clear(); }
	
	void			operator=(e_list &&b)	{ clear(); B::operator=(move(b)); }

	void			clear()					{ while (!B::empty()) B::pop_back(); }
	T*				tail()					{ return B::head.prev; }

	void			del(iterator first, iterator last) {
		first->prev->join(last.get());
		while (first != last)
			delete (first++).get();
	}
	void			deleteall()				{ del(B::begin(), B::end()); }

	static T*		remove(const iterator &i)				{ return i.link()->unlink(); }
	static void		insert_before(const iterator &i, T *t)	{ i.link()->insert_before(t); }
	static void		insert_after(const iterator &i, T *t)	{ i.link()->insert_after(t); }
	friend void		swap(e_list &a, e_list &b)				{ swap((B&)a, (B&)b); }
};

//-------------------------------------
// list
//-------------------------------------

template<typename T> struct link : public link_base<link<T> > {
	T	t;
	link(const T &t) : t(t)		{}
	template<typename...P> link(P&&...p) : t(forward<P>(p)...)	{}
};

template<typename T> struct link_payload<link<T>> {
	static	T*		get(link<T> *p)		{ return &p->t; }
};

template<typename T> class list : public list_base<link<T>> {
	typedef list_base<link<T>>	B;
public:
	using typename B::iterator;
	list()				{}
	list(const _none&)	{}
	list(iterator begin, iterator end) : B(begin, end)	{}
	~list()				{ clear(); }

	list(list &&b) : B(move(b))							{}
	void			operator=(list &&b)					{ clear(); B::operator=(move(b)); }
	void			push_front(T &&t)					{ B::push_front(new link<T>(move(t))); }
	void			push_back(T &&t)					{ B::push_back(new link<T>(move(t))); }
	void			insert_before(iterator &i, T &&t)	{ i.link()->insert_before(new link<T>(move(t))); }
	void			insert_after(iterator &i, T &&t)	{ i.link()->insert_after( new link<T>(move(t))); }

	template<typename... U> void	emplace_front(U&&...t)					{ B::push_front(new link<T>(forward<U>(t)...)); }
	template<typename... U> void	emplace_back(U&&...t)					{ B::push_back(new link<T>(forward<U>(t)...)); }
	template<typename... U> void	emplace_before(iterator &i, U&&...t)	{ i.link()->insert_before(new link<T>(forward<U>(t)...)); }
	template<typename... U> void	emplace_after( iterator &i, U&&...t)	{ i.link()->insert_after( new link<T>(forward<U>(t)...)); }

	void			push_front(const T &t)					{ B::push_front(new link<T>(t)); }
	void			push_back(const T &t)					{ B::push_back(new link<T>(t)); }
	void			pop_front()								{ delete B::pop_front(); }
	void			pop_back()								{ delete B::pop_back(); }
	auto			pop_front_value()						{ unique_ptr<T> p = B::pop_front(); return move(p->t); }
	auto			pop_back_value()						{ unique_ptr<T> p = B::pop_back(); return move(p->t); }
	void			clear()									{ while (!B::empty()) pop_back(); }

	static void		remove(iterator &i)						{ delete i.remove(); }
	static void		insert_before(iterator &i, const T &t)	{ i.link()->insert_before(new link<T>(t)); }
	static void		insert_after(iterator &i, const T &t)	{ i.link()->insert_after( new link<T>(t)); }
	friend void		swap(list &a, list &b)					{ swap((B&)a, (B&)b); }
};

//-----------------------------------------------------------------------------
// singly linked list
//-----------------------------------------------------------------------------

template<typename L> struct slink_base {
	L		*next;
	L		*get()				{ return static_cast<L*>(this); }
	const L	*get()		const	{ return static_cast<const L*>(this); }
	bool	is_linked()	const	{ return next != 0; }

	void	insert_after(L *link)		{ link->next = next; next = link; }
	void	insert_after(L *a, L *b)	{ b->next = next; next = a; }
	L*		unlink_next()				{ L *n = next; next = n->next; n->next = nullptr; return n; }
	L*		unlink(L *t)				{ L *n = next; next = t->next; n->next = nullptr; return n; }
	void	init()						{ next = get(); }
};

template<typename L> struct slink_appender {
	L	*end, *prev;
	slink_appender(L *end) : end(end), prev(end) {}
	~slink_appender()		{ prev->next = end; }
	void	add(L *link)	{ prev->next = link; prev = link; }
};

// points to current element for efficiency
template<typename L> class slist_iterator_base {
protected:
	L		*p;
public:
	slist_iterator_base(L *p = nullptr) : p(p)	{}
	slist_iterator_base(const slink_base<noconst_t<L>> &p) : p(const_cast<L*>(p.get()))	{}
	L*		link()						const	{ return p; }
	auto	get()						const	{ return get_payload(p); }
	explicit operator bool()			const	{ return !!p; }
//	operator decltype(auto)()			const	{ return get(); }
	decltype(auto)	operator*()			const	{ return *get(); }
	auto	operator->()				const	{ return get(); }
	auto&	operator++()						{ p = p->next; return *this; }
	auto	operator++(int)						{ slist_iterator_base i = *this; operator++(); return i; }
	auto&	operator+=(int i)					{ while (i--) p = p->next; return *this; }
	bool	operator==(const slist_iterator_base &b) const { return p == b.p; }
	bool	operator!=(const slist_iterator_base &b) const { return p != b.p; }

	template<typename I> slist_iterator_base	operator+(I i) const { return slist_iterator_base(*this) += i; }
};

// points to element before current one to allow for insertions/deletions
template<typename L> class slist_iterator_basep {
protected:
	L		*p;
public:
	slist_iterator_basep(L *p = nullptr) : p(p)	{}
	slist_iterator_basep(const slink_base<noconst_t<L>> &p) : p(const_cast<L*>(p.get()))	{}
	operator slist_iterator_base<L>()	const	{ return p->next; }
	auto	get()						const	{ return get_payload(p->next); }
	explicit operator bool()			const	{ return !!p; }
//	operator decltype(auto)()			const	{ return get(); }
	decltype(auto)	operator*()			const	{ return *get(); }
	auto	operator->()				const	{ return get(); }
	auto&	operator++()						{ p = p->next; return *this; }
	auto	operator++(int)						{ auto i = *this; p = p->next; return i; }
	auto&	operator+=(int i)					{ while (i--) p = p->next; return *this; }
	bool	operator==(const slist_iterator_basep &b)	const { return (is_marked_ptr(b.p) ? marked_ptr(p->next) : p) == b.p; }
	bool	operator!=(const slist_iterator_basep &b)	const { return !(*this == b); }
	bool	operator==(const slist_iterator_base<L> &b)	const { return b == *this; }
	bool	operator!=(const slist_iterator_base<L> &b)	const { return b != *this; }

	auto	operator+(int i) const { return slist_iterator_basep(*this) += i; }

	L*		unlink()														{ return p->unlink_next(); }
	void	insert_before(L *n)												{ p->insert_after(n); }
	void	insert_after(slist_iterator_basep i)							{ p->next->insert_after(i.unlink()); }
	void	insert_before(slist_iterator_basep i)							{ p->insert_after(i.unlink()); }
	void	insert_after(slist_iterator_basep b, slist_iterator_basep e)	{ if (e.p != b.p) p->next->insert_after(b.p->unlink(e.p->next), e.p->next); }
	void	insert_before(slist_iterator_basep b, slist_iterator_basep e)	{ if (e.p != b.p) p->insert_after(b.p->unlink(e.p->next), e.p->next); }
};

template<typename L> class slist_base {
protected:
	slink_base<L>	head;

	L*			_back() {
		L *end	= head.get();
		L *i	= end;
		while (i->next != end)
			i = i->next;
		return i;
	}
	const L*	_back()	const {
		const L *end	= head.get();
		const L *i		= end;
		while (i->next != end)
			i = i->next;
		return i;
	}
	void		reset()	{ head.get()->init(); }
public:
	typedef slist_iterator_base<L>			iterator;
	typedef slist_iterator_base<const L>	const_iterator;
	typedef slist_iterator_basep<L>			iteratorp;

	struct _insertable {
		typedef iteratorp	iterator, const_iterator;
		slist_base	&list;
		_insertable(slist_base &list) : list(list) {}
		iterator	begin()	const	{ return list.head.get(); }
		iterator	end()	const	{ return marked_ptr(list.head.get()); }
	};

	slist_base() { reset(); }

	slist_base(slist_base &&b) {
		head = b.head;
		b._back()->next = head.get();
		b.reset();
	}

	L*				prev(const L *p) {
		L *i = head.get();
		while (i->next != p) {
			i = i->next;
			if (i == head.get())
				return 0;
		}
		return i;
	}
	const L*		prev(const L *p) const {
		const L *i = head.get();
		while (i->next != p) {
			i = i->next;
			if (i == head.get())
				return 0;
		}
		return i;
	}

	auto&			front()		const			{ return *begin(); }
	auto&			front()						{ return *begin(); }
	auto&			back()		const			{ return *iterator(_back()); }
	bool			empty()		const 			{ return head.next == head.get(); }
	explicit operator bool()	const 			{ return !empty(); }

	iteratorp		beginp()					{ return head; }
	const_iterator	begin()		const			{ return head.next; }
	const_iterator	end()		const			{ return head; }
	iterator		begin()						{ return head.next; }
	iterator		end()						{ return head; }

	L*				push_front(L *t)			{ head.insert_after(t); return t; }
	L*				pop_front()					{ return head.get()->unlink_next(); }
	L*				push_back(L *t)				{ _back()->insert_after(t); return t; }
	static L*		remove(iteratorp i)			{ return i.unlink(); }
	void			insert(iteratorp i, L *t)	{ i.insert_before(t); }
	auto&			operator[](int i)			{ return *(begin() += i); }
	auto&			operator[](int i) const		{ return *(begin() += i); }

	void			append(slist_base &&b)		{ if (!b.empty()) _back()->insert_after(b.head.next, b._back()); b.reset(); }
	void			prepend(slist_base &&b)		{ if (!b.empty()) head.insert_after(b.head.next, b._back()); b.reset(); }
	_insertable		insertable()				{ return *this; }
	slink_appender<L> appender()				{ return end(); }

	size_t			size()					const;
	uint32			size32()				const { return uint32(size()); }
	int				index_of(const L *t)	const;
	bool			contains(const L *t)	const { return index_of(t) != -1; }

	friend void swap(slist_base &a, slist_base &b) {
		L	*ahead = a.head.next, *aback = a._back();
		L	*bhead = b.head.next, *bback = b._back();

		if (bhead == b.head.get()) {
			a.reset();
		} else {
			a.head.next = bhead;
			bback->next = a.head.get();
		}

		if (ahead == a.head.get()) {
			b.reset();
		} else {
			b.head.next = ahead;
			aback->next = b.head.get();
		}
	}
};

template<typename L> size_t slist_base<L>::size() const {
	size_t	size = 0;
	for (const_iterator	i = begin(), e = end(); i != e; ++i)
		++size;
	return size;
}

template<typename L> int slist_base<L>::index_of(const L *t) const {
	int	x = 0;
	for (const_iterator i = begin(), e = end(); i != e; ++i, ++x) {
		if (i.link() == t)
			return x;
	}
	return -1;
}

template<typename L, class P> void sort(slist_iterator_basep<L> lo, slist_iterator_base<L> hi, P comp) {
	for (int chunk = 1;; chunk <<= 1) { // For each power of two
		int	num_merges = 0;

		for (slist_iterator_basep<L> i = lo, r = i; i != hi; i = r) {
			num_merges++;
			// Find next sublist
			for (int n = chunk; n-- && r != hi; ++r);

			// Merge two sublists
			int	nl = chunk, nr = chunk;
			while (nl && nr && r != hi) {
				if (comp(*r, *i)) {
					i.insert_before(r);
					--nr;
				} else {
					++i;
					--nl;
				}
			}
			while (nr-- && r != hi)
				++r;
		}
		if (num_merges < 2)
			break;
	}
}

//-------------------------------------
// e_slist
//-------------------------------------

template<typename T> struct e_slink : public slink_base<T> {
	e_slink()	{ this->next = nullptr; }
};

template<typename T> class e_slist : public slist_base<T> {
	typedef slist_base<T>	B;
public:
	e_slist()		{}

#ifdef USE_RVALUE_REFS
	e_slist(e_slist &&b) : B(move(b))	{}
#endif
	~e_slist()		{ clear(); }

	void			clear()							{ B::reset(); }
	static void		remove(typename B::iteratorp i) { delete i.unlink(); }
	void			deleteall()						{ while (!B::empty()) delete B::pop_front(); }
	friend void swap(e_slist &a, e_slist &b) { swap((B&)a, (B&)b); }
};

//-------------------------------------
// slist
//-------------------------------------

template<typename T> struct slink : public slink_base<slink<T> > {
	T		t;
	slink(const T &t) : t(t)	{}
	template<typename...P> slink(P&&...p) : t(forward<P>(p)...)	{}
};

template<typename T> struct link_payload<slink<T>> {
	static	T	*get(slink<T> *p)	{ return &p->t; }
};

template<typename T> class slist : public slist_base<slink<T> > {
	typedef slist_base<slink<T> >	B;
public:
	using B::push_front;
	using typename B::iteratorp;
	slist()			= default;
	~slist()		{ clear(); }

	slist(slist&&)	= default;
	void			push_front(T &&t)						{ B::push_front(new slink<T>(move(t))); }
	template<typename... U> void	emplace_front(U&&...t)	{ B::push_front(new slink<T>(forward<U>(t)...)); }
	void			pop_front()								{ delete B::pop_front(); }
	auto			pop_front_value()						{ unique_ptr<T> p = B::pop_front(); return move(p->t); }
	auto&			push_front()							{ return B::push_front(new slink<T>())->t; }
	template<typename U>	auto& push_front(U &&u)			{ return B::push_front(new slink<T>(forward<U>(u)))->t; }
	template<typename U>	static void insert(iteratorp i, U &&u)		{ i.insert_before(new slink<T>(forward<U>(u))); }
	template<typename...P>	static void emplace(iteratorp i, P&&...p)	{ i.insert_before(new slink<T>(forward<P>(p)...)); }

	void			clear()									{ while (!B::empty()) pop_front(); }
	static void		remove(typename B::iteratorp i)			{ delete i.unlink(); }
	friend void swap(slist &a, slist &b)					{ swap((B&)a, (B&)b); }
};

//-----------------------------------------------------------------------------
// single linked list with tail pointer
//-----------------------------------------------------------------------------

template<typename L> class slist_tail_base : public slist_base<L> {
protected:
	typedef slist_base<L> B;
	using B::head;
	L*				tail;
	void			reset()	{ B::reset(); tail = head.get(); }
public:
	using typename B::iteratorp;

	slist_tail_base() : tail(head.get()) {}
	slist_tail_base(slist_tail_base &&b) {
		head = b.head;
		tail = b.tail;
		tail->next = head.get();
		b.reset();
	}

	iteratorp		endp()				{ return tail; }
	auto&			back()		const	{ return *endp(); }
	L*				push_back(L *t)		{ tail->insert_after(t); tail = t; return t; }

	void			append(slist_tail_base &&b)	{
		if (!b.empty()) {
			tail->insert_after(b.head.next, b.tail);
			tail = b.tail;
		}
		b.reset();
	}
	void			prepend(B &&b)	{
		if (!b.empty())
			head.insert_after(b.head.next, b.tail);
		b.reset();
	}

	friend void swap(slist_tail_base &a, slist_tail_base &b) {
		L	*ahead = a.head.next, *atail = a.tail;
		L	*bhead = b.head.next, *btail = b.tail;

		if (bhead == b.head.get())
			a.reset();
		else {
			a.head.next = bhead;
			a.tail		= btail;
			btail->next = a.head.get();
		}

		if (ahead == a.head.get())
			b.reset();
		else {
			b.head.next = ahead;
			b.tail		= atail;
			atail->next = b.head.get();
		}
	}
};

template<typename L> class e_slist_tail : public slist_tail_base<L> {
	typedef slist_tail_base<L>	B;
public:
	void			clear()								{ B::reset(); }
	void			deleteall()							{ while (!B::empty()) delete B::pop_front(); }
	friend void swap(e_slist_tail &a, e_slist_tail &b)	{ swap((B&)a, (B&)b); }
};

template<typename T> class slist_tail : public slist_tail_base<slink<T> > {
	typedef	slink<T>			L;
	typedef slist_tail_base<L>	B;
public:
					~slist_tail()						{ clear(); }
	void			push_front(const T &t)				{ B::push_front(new L(t)); }
	void			push_front(T &&t)					{ B::push_front(new L(move(t))); }
	template<typename... U> void emplace_front(U&&...t)	{ B::push_front(new L(forward<U>(t)...)); }
	void			push_back(const T &t)				{ B::push_back(new L(t)); }
	void			push_back(T &&t)					{ B::push_back(new L(move(t))); }
	template<typename... U> void emplace_back(U&&...t)	{ B::push_back(new L(forward<U>(t)...)); }
	void			pop_front()							{ delete B::pop_front(); }
	void			clear()								{ while (!B::empty()) pop_front(); }
	void			remove(typename B::iteratorp i)		{ delete i.unlink(); }
	friend void swap(slist_tail &a, slist_tail &b)		{ swap((B&)a, (B&)b); }
};

//-----------------------------------------------------------------------------
// circular (works for singly- and doubly-linked lists)
//-----------------------------------------------------------------------------

template<typename L> class circular_iterator {
	pointer_pair<L,bool,2>	p;
public:
	circular_iterator(L *p, bool b = false)	: p(p, p && b)	{}
	L*			link()				const	{ return p; }
	auto		get()				const	{ return get_payload((L*)p); }
	explicit operator bool()		const	{ return !!p; }
//	operator decltype(auto)()		const	{ return get(); }
	decltype(auto)	operator*()		const	{ return *get(); }
	auto			operator->()	const	{ return get(); }
	auto&			operator++()			{ p = {p->next, true}; return *this; }
	auto			operator++(int)			{ circular_iterator i = *this; p = {p->next, true}; return i; }

	friend bool	operator==(const circular_iterator &a, const circular_iterator &b) { return a.p.v == b.p.v; }
	friend bool	operator!=(const circular_iterator &a, const circular_iterator &b) { return a.p.v != b.p.v; }
};

// if doubly-linked, enable decrements and reverse
template<typename L> exists_t<decltype(L::prev), circular_iterator<L>&>	operator--(circular_iterator<L> &i)			{ return i = {i.link()->prev, false}; }
template<typename L> exists_t<decltype(L::prev), circular_iterator<L>>	operator--(circular_iterator<L>& i, int)	{ auto j = i; --i; return j; }

template<typename L> exists_t<decltype(L::prev), void> reverse(circular_iterator<L> a, circular_iterator<L> b) {
	if (a != b) {
		L	*end	= b.link();
		L	*p1		= a.link();
		do {
			L	*p2 = p1->next;
			p1->next = p1->prev;
			p1->prev = p2;
			p1 = p2;
		} while (p1 != end);
	}
}

template<typename L> struct circular_list {
	L	*tail;
public:
	typedef circular_iterator<L>	const_iterator, iterator;

	circular_list() : tail(nullptr)			{}
	circular_list& operator=(circular_list &&b) { swap(tail, b.tail); return *this; }
	L*				front()		const		{ return tail->next; }
	L*				back()		const		{ return tail; }
	bool			empty()		const		{ return !tail; }
	iterator		begin()		const		{ return {front(), false}; }
	iterator		end()		const		{ return {front(), true}; }
	size_t			size()		const;
	uint32			size32()	const		{ return uint32(size()); }

	L*				push_front(L *t)		{ if (tail) tail->insert_after(t); else { t->init(); tail = t; } return t; }
	L*				push_back(L *t)			{ if (tail) tail->insert_after(t); else t->init(); tail = t; return t; }
	L*				pop_front()				{ return tail->unlink_next(); }
	L*				pop_back()				{ return exchange(tail, tail->unlink()); }
	void			shift()					{ if (tail) tail = tail->next; }
	void			clear()					{ tail = nullptr; }

	void			deleteall()	{
		if (tail) {
			L	*i0 = exchange(tail, nullptr), *i = i0;
			do {
				L *t = i;
				i = i->next;
				delete t;
			} while (i != i0);
		}
	}

	friend void reverse(circular_list &c) {
		if (L *p0 = c.tail) {
			L *p1 = p0;
			do {
				L	*p2 = p1->next;
				p1->next = p1->prev;
				p1->prev = p2;
				p1 = p2;
			} while (p1 != p0);
		}
	}
};

template<typename L> size_t circular_list<L>::size() const {
	size_t	size = 0;
	if (L *i = tail) do {
		++size;
		i = i->next;
	} while (i != tail);
	return size;
}

//-----------------------------------------------------------------------------
// linked pointer - no list in pointee (so never pass pointee directly)
//-----------------------------------------------------------------------------

template<typename T> class linked_ptr : public link_base<linked_ptr<T>> {
	typedef link_base<linked_ptr<T>>	B;
	T	*t;
public:
	linked_ptr() : t(nullptr)					{}
	explicit linked_ptr(T *t) : t(t)			{ B::init(); }
	linked_ptr(linked_ptr &p) : t(p.get())		{ p.insert_after(this); }
	~linked_ptr()								{ B::unlink(); }

	linked_ptr& operator=(linked_ptr &p) {
		if (t)
			B::unlink();
		t = p.get();
		p.insert_after(this);
		return *this;
	}

	T*			get()		const	{ return t; }
	T*			operator->()const	{ return t; }
//	operator	T*()		const	{ return t; }
	T&			operator*()	const	{ return *t; }

	void		kill() {
		delete t;
		linked_ptr *p = this;
		do {
			p->t	= nullptr;
			p		= p->next;
		} while (p != this);
	}
	friend T*	get(const linked_ptr &a)		{ return a.get(); }
	friend T*	put(linked_ptr &a)				{ return a.get(); }
};

//-----------------------------------------------------------------------------
// ptr_list / ptr_link
//-----------------------------------------------------------------------------

template<typename T> class ptr_list;

template<typename T> class ptr_link : public link_base<ptr_link<T>> {
	typedef link_base<ptr_link<T>>	B;
	friend class ptr_list<T>;
	T	*t;
public:
	ptr_link() : t(nullptr)			{}
	ptr_link(T *t) : t(t)			{ if (t) t->add(this); }
	ptr_link(ptr_link &p) : t(p)	{ if (t) t->add(this); }
	~ptr_link()						{ if (t) B::unlink(); }

	ptr_link& operator=(T *p) {
		if (t)
			B::unlink();
		if (t = p)
			p->add(this);
		return *this;
	}

	ptr_link& operator=(ptr_link &p) {
		if (t)
			B::unlink();
		if (t = p)
			p->add(this);
		return *this;
	}

	T*			get()		const	{ return t; }
	T*			operator->()const	{ return t; }
	operator	T*()		const	{ return t; }
//	T&			operator*()	const	{ return *t; }
};

template<typename T> class ptr_list : list_base<ptr_link<T>> {
	typedef list_base<ptr_link<T>>	B;
	friend class ptr_link<T>;
	void			add(ptr_link<T> *t)		{ B::head.insert_after(t); }
protected:
	~ptr_list()	{
		for (auto &i : *this)
			i = nullptr;
	}
public:
	bool validate(T *t)	{
		for (auto &i : *this)
			if (i != t)
				return false;
		return true;
	}
};

//-----------------------------------------------------------------------------
// referee/linked_ref
//-----------------------------------------------------------------------------

class referee;

template<typename T> class linked_ref : public e_link<linked_ref<void>> {
	T		*t;
public:
	linked_ref()				: t(nullptr)	{}
	linked_ref(linked_ref &p)	: t(p.t)		{ if (t) p.insert_after(this); }
	linked_ref(referee *r, T *t);
	linked_ref(referee *r);
	~linked_ref() { if (t) unlink(); }

	linked_ref&	operator=(linked_ref &p) {
		if (t)
			unlink();
		if (t = p.t)
			p.insert_after(this);
		return *this;
	}
	linked_ref&	operator=(referee *r);

	explicit operator bool()		const	{ return !!t; }
	T*			get()				const	{ return t; }
	T*			operator->()		const	{ return t; }
	operator	T*()				const	{ return t; }
//	T&			operator*()			const	{ return *t; }
	friend T*	get(const linked_ref &a)	{ return a.t; }
};

template<> class linked_ref<void> : public e_link<linked_ref<void> > {
	friend class referee;
	void	*t;
	linked_ref() = delete;
};

class referee {
	template<typename T> friend class linked_ref;
	e_list<linked_ref<void>>	refs;
protected:
	~referee()	{
		for (auto &i : refs)
			i.t = 0;
	}
public:
	void	killrefs() {
		for (auto &i : refs)
			i.t = 0;
		refs = none;
	}
	template<typename T> void add(linked_ref<T> *r) {
		refs.push_front((linked_ref<void>*)r);
	}
};

template<typename T> inline linked_ref<T>::linked_ref(referee *r, T *t)	: t(t)					{ if (t) r->add(this); }
template<typename T> inline linked_ref<T>::linked_ref(referee *r)		: t(static_cast<T*>(r))	{ if (t) r->add(this); }
template<typename T> inline linked_ref<T>&	linked_ref<T>::operator=(referee *r) {
	if (t)
		unlink();
	if (t = static_cast<T*>(r))
		r->add(this);
	return *this;
}

//-----------------------------------------------------------------------------
//	hierarchy
//-----------------------------------------------------------------------------

template<typename T> class breadth_first_iterator : public e_slist<T>::iterator {
	typedef typename e_slist<T>::iterator	B;
	using B::p;
public:
	breadth_first_iterator(T *t) : B(t) {}
	T*	skip()			{ return p = skip_children(p); }
	T*	operator++()	{ return p = p->has_children() ? p->children.begin().get() : skip_children(p); }
};

template<typename T> class depth_first_iterator : public e_slist<T>::iterator {
	typedef typename e_slist<T>::iterator	B;
	using B::p;
public:
	depth_first_iterator(T *t) : B(t) {}
	T*	operator++() {
		if (T *s = p->sibling())
			return p = last_generation(s);
		return p = p->parent;
	}
};

template<typename T> struct hierarchy_appender : slink_appender<T> {
	typedef slink_appender<T>	B;
	T	*parent;

	hierarchy_appender(T *parent) : B(parent->children.end().get()), parent(parent) {}

	void	add_child(T *child) {
		child->parent = parent;
		B::add(child);
	}
	template<typename T2> void	add_child(T2 &&child) {
		add_child(new T(forward<T2>(child)));
	}
};

template<typename T> struct hierarchy : e_slink<T> {
	T			*parent;
	e_slist<T>	children;

	typedef breadth_first_iterator<T>		iterator;
	typedef breadth_first_iterator<const T>	const_iterator;

	friend int	get_depth(const T *t) {
		int	d = 0;
		while (t = t->parent)
			++d;
		return d;
	}
	friend int	get_ancestry(const T *t, const T **dest, int max) {
		const T	**end = dest + max;
		while (dest < end && (t = t->parent))
			*dest++ = t;
		return dest + max - end;
	}
	friend const T*	get_root(const T *t) {
		while (const T *p = t->parent)
			t = p;
		return t;
	}
	friend const T*	last_generation(const T *t) {
		while (t->has_children())
			t = t->children.begin().get();
		return t;
	}
	friend const T*	skip_children(const T *t) {
		while (t) {
			if (const T *s = t->sibling())
				return s;
			t = t->parent;
		}
		return t;
	}
	friend T*	get_root(T *t)			{ return unconst(get_root(make_const(t))); }
	friend T*	last_generation(T *t)	{ return unconst(last_generation(make_const(t))); }
	friend T*	skip_children(T *t)		{ return unconst(skip_children(make_const(t))); }

	hierarchy() : parent(0)	{}
	hierarchy(const hierarchy &b)	{ ISO_ASSERT(0); }
	template<typename B> hierarchy(const hierarchy<B> &b) : parent(0) {
		hierarchy_appender<T>	a(me());
		for (auto &i : b.children)
			a.add_child(i);
	}
	hierarchy(hierarchy &&b) : parent(0), children(move(b.children)) {
		for (auto &i : children)
			i.parent = me();
	}
	template<typename B> hierarchy& operator=(const hierarchy<B> &b) {
		detach();
		children.clear();
		hierarchy_appender<T>	a(me());
		for (auto &i : b.children)
			a.add_child(i);
		return *this;
	}

	void	attach(e_slist<T> &&list) {
		for (auto &i : list)
			i.parent = me();
		children.prepend(move(list));
	}

	T*			me()					{ return static_cast<T*>(this); }
	const T*	me()			const	{ return static_cast<const T*>(this); }
	const T*	root()			const	{ return get_root(me()); }
	T*			root()					{ return get_root(me()); }
	const T*	child()			const	{ return children.empty() ? 0 : &children.front(); }
	T*			child()					{ return children.empty() ? 0 : &children.front(); }
	bool		has_children()	const	{ return !children.empty(); }
	uint32		num_children()	const	{ return children.size32(); }
	T*			sibling()		const	{ return parent && this->next != parent->children.end().get() ? this->next : 0; }

	void		attach(T *t)			{ t->parent = me(); children.push_front(t); }
	void		push_back(T *t)			{ t->parent = me(); children.push_back(t); }
	void		push_front(T *t)		{ t->parent = me(); children.push_front(t); }

	T *detach() {
		if (parent) {
			parent->children.prev(me())->unlink_next();
			parent	= 0;
		}
		return me();
	}

	~hierarchy() {
		detach();
		while (!children.empty()) {
			T	*t = children.pop_front();
			t->parent	= 0;
			delete t;
		}
	}

	int			depth()								const	{ return get_depth(me()); }
	int			ancestry(const T **dest, int max)	const	{ return get_ancestry(me(), dest, max); }

	range<breadth_first_iterator<T> >		breadth_first()			{ return { me(), skip_children(me()) }; }
	range<depth_first_iterator<T> >			depth_first()			{ return { last_generation(me()), skip_children(me()) }; }
	range<breadth_first_iterator<const T> >	breadth_first()	const	{ return { me(), skip_children(me()) }; }
	range<depth_first_iterator<const T> >	depth_first()	const	{ return { last_generation(me()), skip_children(me()) }; }
};

}//namespace iso

#endif	// LIST_H
