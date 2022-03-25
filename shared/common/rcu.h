#ifndef RCU_H
#define RCU_H

//-----------------------------------------------------------------------------
//	RCU - read-copy-update
//-----------------------------------------------------------------------------

#include "base/atomic.h"
#include "base/list.h"
#include "base/functions.h"
#include "thread.h"
#include "lockfree.h"

namespace iso {

template<typename T> class rcu;

template<typename T> inline T*		rcu_dereference(volatile T *p)					{ return const_cast<T*>(p); }//T *p1 = const_cast<T*>(p); _atomic_barrier(); return p1; }
template<typename T> inline bool	rcu_cas_pointer(T *&p, const T *a, const T *b)	{ return _cas_exch(&p, a, b); }
template<typename T> inline T*		rcu_exch_pointer(T *&p, const T *b)				{ return _atomic_exch(&p, b); }
template<typename T> inline void	rcu_set_pointer(T *&p, const T *b)				{ _atomic_store_release((intptr_t*)&p, (intptr_t)b); }

template<typename T> class rcu<T*> {
	T	*p;
public:
	operator T*() const	volatile			{ T *p1 = const_cast<T*>(p); _atomic_barrier(); return p1; }
	T*			operator=(T *b)				{ _atomic_store((intptr_t*)&p, (intptr_t)b); return b; }
	bool		cas(T *a, T *b)				{ return _cas_exch(&p, a, b); }
	friend T*	exchange(rcu<T*> &a, T *b)	{ return _atomic_exch(&a.p, b); }
};

template<typename T> rcu<T>& as_rcu(T &t) { return reinterpret_cast<rcu<T>&>(t); }

//-----------------------------------------------------------------------------
// doubly linked list
//-----------------------------------------------------------------------------

template<typename L> class rcu<link_base<L>> : public link_base<L> {
	typedef link_base<L> B;
public:
	using B::prev; using B::next;
	L		*get()						{ return rcu_dereference(B::get()); }
	const L	*get()		const			{ return rcu_dereference(B::get()); }

//	void	join(rcu *link)				{ next = link->get(); rcu_set_pointer(link->prev, get()); }
	void	join(rcu *link)				{ next = link->get(); as_rcu(link->prev) = get(); }
	void	insert_after(L *link)		{ link->B::join(next); join(link); }
	void	insert_before(L *link)		{ link->next = get(); link->prev = prev; as_rcu(prev->next) = link; prev = link; }
	void	insert_after(L *a, L *b)	{ b->B::join(next); join(a); }
	void	insert_before(L *a, L *b)	{ prev->B::join(a); b->join(get()); }
	L		*unlink()					{ prev->join(next); return get(); }
	L		*unlink_next()				{ return next->unlink(); }
	void	unlink(L *to)				{ prev->join(to); }
	void	replace(L *b)				{ b->B::join(next); prev->join(b); }
};

template<typename L> class rcu<list_iterator_base<L>> : public list_iterator_base<L> {
	typedef list_iterator_base<L> B;
	using B::p;
	using element = it_element_t<B>;//typename B::element;
//	const link_base<noconst_t<L>> *orig;
public:
	rcu(L *p) : B(p)	{}//, orig(0)	{}
	rcu(const link_base<noconst_t<L>> &p) : B(const_cast<L*>(p.get())) {}//, orig(&p) {}
	operator element*()			const	{ return rcu_dereference(B::get()); }
	element*	operator->()	const	{ return rcu_dereference(B::get()); }
	L*			link()			const	{ return p; }
	rcu&		operator++()			{ p = rcu_dereference(p->next); return *this; }
	rcu			operator++(int)			{ auto i = *this; p = rcu_dereference(p->next); return i; }
	rcu&		operator--()			{ p = rcu_dereference(p->prev); return *this; }
	rcu			operator--(int)			{ auto i = *this; p = rcu_dereference(p->prev); return i; }
//	bool operator!=(const rcu &b) const { return p != b.orig->get(); }
	bool operator!=(const rcu &b) const { return p != b.p; }
};

template<typename L, class P> auto remove(rcu<list_iterator_base<L>> i, rcu<list_iterator_base<L>> end, P pred) {
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

template<typename T> class rcu<e_link<T>> : public rcu<link_base<T>> {
public:
	rcu()	{ this->next = 0; }
//	~rcu()	{ ISO_ASSERT(!this->next); }
};

template<typename T> class rcu<e_list<T> > : public e_list<T> {
	typedef e_list<T>	B;
	using B::head;
public:
	typedef rcu<list_iterator_base<T>>			iterator;
	typedef rcu<list_iterator_base<const T>>	const_iterator;

	rcu()		{}
	rcu(iterator begin, iterator end) : B(begin, end) {}
	rcu(rcu &&b) : B(move(b)) {}
	void			operator=(rcu &&b)	{ B::clear(); B::operator=(move(b)); }

	void			del(iterator i);
	void			del(iterator first, iterator last);
	void			deleteall()		{ del(begin(), end()); }

	const_iterator	begin()	const	{ return head.next; }
	const_iterator	end()	const	{ return head; }
	iterator		begin()			{ return head.next; }
	iterator		end()			{ return head; }

	friend void swap(rcu &a, rcu &b) { swap((B&)a, (B&)b); }
};

//-----------------------------------------------------------------------------
// singly linked list (terminated with null)
//-----------------------------------------------------------------------------

template<typename L> class rcu<slink_base<L>> : public slink_base<L> {
	typedef slink_base<L> B;
public:
	using B::next;
	L		*get()				{ return rcu_dereference(B::get()); }
	const L	*get()		const	{ return rcu_dereference(B::get()); }

	void	insert_after(L *link)		{ link->next = next; rcu_set_pointer(next, link); }
	void	insert_after(L *a, L *b)	{ b->next = next; rcu_set_pointer(next, a); }
	L*		unlink_next()				{ L *n = next; rcu_set_pointer(next, n->next); return n; }
	L*		unlink(L *t)				{ L *n = next; rcu_set_pointer(next, t->next); return n; }
	void	init()						{ next = nullptr; }
};

template<typename L> class rcu<slist_iterator_base<L>> : public slist_iterator_base<L> {
	typedef slist_iterator_base<L> B;
	using B::p;
	using typename B::element;
public:
	rcu(L *p) : B(p)	{}
	rcu(const slink_base<noconst_t<L>> &p) : B(p)	{}
	operator element*()			const	{ return B::get(); }
	element*	operator->()	const	{ return B::get(); }
	L*			link()			const	{ return p; }
	rcu&		operator++()			{ p = rcu_dereference(p->next); return *this; }
	rcu			operator++(int)			{ auto i = *this; p = rcu_dereference(p->next); return i; }
};

template<typename T> class rcu<e_slist<T>> : public e_slist<T> {
	typedef e_slist<T>	B;
	using B::head;
public:
	typedef rcu<slist_iterator_base<T>>		iterator;
	typedef rcu<slist_iterator_base<const T>>	const_iterator;

	rcu()				{}
	rcu(rcu &&b) : B(move(b))	{}

	bool			empty()	const	{ return !head.next; }
	const_iterator	begin()	const	{ return head.next; }
	const_iterator	end()	const	{ return nullptr; }
	iterator		begin()			{ return head.next; }
	iterator		end()			{ return nullptr; }

	friend void swap(rcu &a, rcu &b) { swap((B&)a, (B&)b); }
};

//-----------------------------------------------------------------------------
// RCU control
//-----------------------------------------------------------------------------

class RCU {
	enum STATE {
		INACTIVE,
		ACTIVE_CURRENT,
		ACTIVE_OLD,
	};
	enum {
//		PHASE_MASK	= 0x80000000,
		PHASE_MASK	= 0xffff0000,
		NEST_MASK	= ~PHASE_MASK,
		SIZE_DEFER	= 256,
	};

	struct per_thread : rcu<link_base<per_thread>> {
		atomic<int32>			ctr;
		block_fifo<SIZE_DEFER>	defered;

		typedef virtfunc<void()>	defer_t;

		per_thread() : ctr(0) {
			RCU::get().add_reader(this);
		}
		~per_thread() { unlink(); }
		inline bool		reading()	const {
			return (ctr & NEST_MASK) > 0;
		}
		inline STATE	state(uint32 phase) const {
			uint32	t = ctr;
			return	!(t & NEST_MASK) 			? INACTIVE
				: 	!((t ^ phase) & PHASE_MASK)	? ACTIVE_CURRENT
				: 	ACTIVE_OLD;
		}
		inline void		read_lock() {
			if ((ctr++ & NEST_MASK) == 0)
				ctr = RCU::get().ctr;
		}
		inline void		read_unlock() {
			if ((ctr-- & NEST_MASK) == 1)
				RCU::get().wake();
		}
		template<typename L> void defer(L &&x)	{
			while (!defered.push_back(make_lambda<defer_t>(forward<L>(x))))
				RCU::synchronise();
		}
		void do_defered() {
			while (defer_t *i = defered.pop_front()) {
				(*i)();
			}
			defered.clear();
		}
	};

	atomic<uint32>	ctr;	// current phase, and a count of 1 to accelerate the reader fast path
	atomic<uint32>	futex;
	static thread_local per_thread _per_thread;// alignas(CACHELINE_SIZE);

	static RCU& get() {
		static manual_static<RCU>	r alignas(CACHELINE_SIZE);
		//static RCU r alignas(CACHELINE_SIZE);
		return r.get();
	}

	MCS_lock				waiters;
	Mutex					waiters_lock;

	rcu<e_list<per_thread>>	readers;
	Mutex					readers_lock;

	void	wake()						{ if (futex) futex = 0; }
	void	add_reader(per_thread *p)	{ with(readers_lock), readers.push_back(p); }
	void	_synchronise();

public:
	RCU() : ctr(1)						{}
	static void synchronise()	{ get()._synchronise(); }
	static void read_lock()		{ _per_thread.read_lock(); }
	static void read_unlock()	{ _per_thread.read_unlock(); }
	static bool reading()		{ return _per_thread.reading(); }
	template<typename L> static void defer(L &&x) { return _per_thread.defer(forward<L>(x)); }
};

template<typename T> void rcu<e_list<T> >::del(rcu<e_list<T> >::iterator i) {
	i->unlink();
	RCU::synchronise();
	delete i;
}
template<typename T> void rcu<e_list<T> >::del(rcu<e_list<T> >::iterator first, rcu<e_list<T> >::iterator last) {
	if (first != last) {
		e_list<T>	copy(first, last);
		RCU::synchronise();
		copy.deleteall();
	}
}

struct with_rcu {
	with_rcu()	{ RCU::read_lock(); }
	~with_rcu()	{ RCU::read_unlock(); }
};

}//namespace iso

#endif	// RCH_H
