#ifndef LOCKFREE_H
#define LOCKFREE_H

#include "base/bits.h"
#include "base/list.h"
#include "base/hash.h"
#include "base/interval.h"
#include "allocators/lf_allocator.h"
#include "allocators/lf_pool.h"
#include "thread.h"

namespace iso {

//-----------------------------------------------------------------------------
//	lf_array_queue (ringbuffer)
//-----------------------------------------------------------------------------

template<class T, int N> class lf_array_queue {
	atomic<uint32>	head, head2;
	atomic<uint32>	tail, tail2;
	space_for<T>	array[N];
	//T				array[N];

	class putter {
		lf_array_queue	*fifo;
		uint32	t, n;
	public:
		putter(lf_array_queue *fifo, uint32 t, uint32 n) : fifo(fifo), t(t), n(n)	{}
		~putter()						{ no_lock_update(fifo->tail2, t, t + n); }
		//operator T*()			const	{ return fifo->array + t % N; }
		//T &operator[](uint32 i)	const	{ return fifo->array + (t + i) % N; }
		void *alloc(size_t size, size_t align) const	{ return fifo->array + t % N; }
	};

	class getter {
		lf_array_queue	*fifo;
		uint32	t, n;
	public:
		getter(lf_array_queue *fifo, uint32 t, uint32 n) : fifo(fifo), t(t), n(n)	{}
		~getter()						{ no_lock_update(fifo->head2, t, t + n); }
		operator T*()			const	{ return fifo->array + t % N; }
		T &operator[](uint32 i)	const	{ return fifo->array + (t + i) % N; }
	};

public:

	class iterator {
		lf_array_queue	*fifo;
		int		t;
	public:
		iterator(lf_array_queue *_fifo, int _t) : fifo(_fifo), t(_t)	{}
					operator T*()					const	{ return fifo->array + t % N; }
		T*			operator->()					const	{ return fifo->array + t % N; }
		bool		operator==(const iterator &i)	const	{ return t == i.t;	}
		bool		operator!=(const iterator &i)	const	{ return t != i.t;	}
		iterator	operator++()							{ ++t; return *this;	}
	};

	lf_array_queue() : head(0), head2(0), tail(0), tail2(0) {}
	
	template<typename U>	bool	push_back(U &&u) {
		uint32	t	= tail;
		do {
			if (t == head2 + N)
				return false;
		} while (!tail.cas_exch(t, t + 1));
		new(array[t % N]) T(forward<U>(u));
		no_lock_update(tail2, t, t + 1);
		return true;
	}
	template<typename...U>	bool emplace_back(U&&...u) {
		uint32	t	= tail;
		do {
			if (t == head2 + N)
				return false;
		} while (!tail.cas_exch(t, t + 1));
		new(array[t % N]) T(forward<U>(u)...);
		no_lock_update(tail2, t, t + 1);
		return true;
	}
	
	bool	pop_front(T &data) {
		uint32	t	= head;
		do {
			if (t == tail2)
				return false;
		} while (!head.cas_exch(t, t + 1));
		data = reinterpret_cast<T&&>(array[t % N]);
		no_lock_update(head2, t, t + 1);
		return true;
	}

	putter	put(int n = 1) {
		uint32	t = tail, n2;
		do {
			n2	= min(head2 + N - t, n);
		} while (!tail.cas_exch(t, t + n2));
		return putter(this, t, n2);
	}

	getter	get(int n = 1) {
		uint32	t = head, n2;
		do {
			n2	= min(tail2 - t, n);
		} while (!head.cas_exch(t, t + n2));
		return getter(this, t, n2);
	}

	putter	put_contig(int n = 1) {
		uint32	t = tail, n2;
		do {
			if (t == head2 + N)
				return putter(this, t, 0);

			uint32	t1 = t % N;
			uint32	t2 = head2 - (t1 - 1) * N;
			n2 = min(n, min(t2, N) - t1);
		} while (!tail.cas_exch(t, t + n2));
		return putter(this, t, n2);
	}

	getter	get_contig(int n = 1) {
		uint32	t = head, n2;
		do {
			if (t == tail2)
				return getter(this, t, 0);

			uint32	t1 = t % N;
			uint32	t2 = tail2 - t1 * N;
			n2 = min(n, min(t2, N) - t1);
		} while (!head.cas_exch(t, t + n2));
		return getter(this, t, n2);
	}
	void		clear()					{ tail = tail2 = head = head2 = 0;	}
	int			size()			const	{ return tail - head;				}
	bool		empty()			const	{ return tail2 == head;				}
	bool		full()			const	{ return tail2 - head == N;			}
	iterator	begin()					{ return iterator(this, head);		}
	iterator	end()					{ return iterator(this, tail);		}
	int			head_index()			{ return int(head);					}
	int			tail_index()			{ return int(tail);					}
};

//-----------------------------------------------------------------------------
//	lf_list_queue
// based on: Optimized Lock-Free FIFO Queue continued, Dominique Fober, Yann Orlarey, Stéphane Letz
//-----------------------------------------------------------------------------

template<typename L> struct atomic<slink_base<L>> {
	atomic<L*>	next;
	L		*get()				{ return static_cast<L*>(this); }
	const L	*get()		const	{ return static_cast<const L*>(this); }
	bool	is_linked()	const	{ return next != 0; }

	void	insert_after(L *link)		{ link->next = next; next = link; }
	void	insert_after(L *a, L *b)	{ b->next = next; next = a; }
	L*		unlink_next()				{ L *n = next; next = n->next; n->next = 0; return n; }
	L*		unlink(L *t)				{ L *n = next; next = t->next; n->next = 0; return n; }
	void	init()						{ next = get(); }
};

template<typename T> struct atomic<slink<T>> : public atomic<slink_base<atomic<slink<T>>>> {
	T		t;
	atomic(const T &t) : t(t)	{}
	atomic(T &&t) : t(move(t))	{}
};

template<typename T> struct link_payload<atomic<slink<T>>> {
	static	T	*get(atomic<slink<T>> *p)	{ return &p->t; }
};

// T should inherit from (something like) atomic<slink_base<T>>

template<typename T> class lf_list_queue {
public:
	typedef tagged_pointer<T>	ptr_count;
	atomic<ptr_count>		head;	// a pointer to head cell + total count of pop operations
	atomic<ptr_count>		tail;	// a pointer to tail cell + total count of push operations
	struct {
		atomic<T*>	next;
		operator T*() const { return (T*)this; }
	} dummy;						// a dummy private cell
public:
	lf_list_queue() {
		dummy.next	= (T*)this;
		head		= {(T*)dummy, 0};
		tail		= {(T*)dummy, 0};
	}

	bool	empty() const {
		ptr_count	h	= head;
		return h.a->next == 0;
	}

	//---------------------------------
	//	back
	//---------------------------------

	void	push_back(T *x) {
		x->next		= (T*)this;	//end marker
		for (;;) {
			ptr_count	t = tail;
			if (t.a->next.cas((T*)this, x))	{	// try to link the cell to the tail cell
				tail.cas(t, {x, t.b + 1});		// enqueue done, try to set tail to the enqueued cell
				return;
			}
			tail.cas(t, {t.a->next, t.b + 1});	// tail was not pointing to the last cell, try to set tail to the next cell
		}
	}

	T*		back() {
		for (;;) {
			ptr_count	t = tail;
			if (t.a->next != (T*)this)
				return t.a == dummy ? (T*)nullptr : t.a;
			tail.cas(t, {t.a->next, t.b + 1});	// tail was not pointing to the last cell, try to set tail to the next cell
		}
	}

	bool	try_push_back(T *prev_tail, T *x) {
		if (prev_tail == 0)
			prev_tail = dummy;

		x->next		= (T*)this;
		if (!prev_tail->next.cas((T*)this, x))	// try to link the cell to the tail cell
			return false;

		ptr_count	t	= tail;
		if (t.a == prev_tail)
			tail.cas(t, {x, t.b + 1});		// enqueue done, try to set tail to the enqueued cell
		return true;
	}

	bool	is_back(T *p) const {
		return p->next == (T*)this || (p->next == dummy && dummy.next == (T*)this);
	}

	//---------------------------------
	//	front
	//---------------------------------

	T	*pop_front() {
		for (;;) {
			ptr_count	h	= head;
			ptr_count	t	= tail;
			T			*n	= h.a->next;
			if (h == head) {					// consistency
				if (h.a == t.a) {				// is queue empty or tail falling behind ?
					if (n == (T*)this)
						return 0;				// empty

					tail.cas(t, {n, t.b + 1});	// try to set tail to the next cell

				} else if (n != (T*)this) {			// try to set head to the next cell
					if (head.cas(h, {n, h.b + 1})) {
						if (h.a != dummy) {
							h.a->next = 0;
							return h.a;			// return head cell
						}
						
						push_back(dummy);		// push the dummy cell back to the fifo and try again
					}
				}
			}
		}
	}

	T*		front() {
		for (;;) {
			ptr_count	h	= head;
			if (h.a != dummy)
				return h.a;

			ptr_count	t	= tail;
			T			*n	= dummy.next;
			if (h == head) {
				if (n == (T*)this)
					return n;

				if (h.a == t.a)
					tail.cas(t, {n, t.b + 1});	// try to move tail past head

				if (head.cas(h, {n, h.b + 1}))
					push_back(dummy);			// push the dummy cell back to the fifo and try again
			}
		}
	}
	ptr_count safe_front() {
		for (;;) {
			ptr_count	h	= head;
			if (h.a != dummy)
				return h;

			ptr_count	t	= tail;
			T			*n	= dummy.next;
			if (h == head) {
				if (n == (T*)this)
					return {n, h.b};

				if (h.a == t.a)
					tail.cas(t, {n, t.b + 1});	// try to move tail past head

				if (head.cas(h, {n, h.b + 1}))
					push_back(dummy);			// push the dummy cell back to the fifo and try again
			}
		}
	}

	// returns unused node
	T*		try_pop_front(T *front) {
		ptr_count	h	= head;
		ptr_count	t	= tail;
		T			*n	= front->next;

		if (n != (T*)this && h.a == front && h == head) {
			if (front == t.a)	// is tail falling behind ?
				tail.cas(t, {n, t.b + 1});

			if (head.cas(h, {n, h.b + 1}))
				return front;
		}
		return 0;
	}

	T*		try_pop_front(ptr_count front) {
		ptr_count	h	= head;
		ptr_count	t	= tail;
		T			*n	= front->next;

		if (n != (T*)this && h == front && h == head) {
			if (front.a == t.a)	// is tail falling behind ?
				tail.cas(t, {n, t.b + 1});

			if (head.cas(h, {n, h.b + 1}))
				return front;
		}
		return 0;
	}
};

//-----------------------------------------------------------------------------
//	lf_array_queue_list
//	a list of array queues
//-----------------------------------------------------------------------------

template<class T, int N> struct lf_array_queue_node : atomic<slink_base<lf_array_queue_node<T,N>>>, lf_array_queue<T, N> {};

template<class T, int N> struct lf_array_queue_list {
	typedef lf_array_queue_node<T,N> L;
	lf_list_queue<L>			queue;
	atomic<tagged_pointer<L>>	put_fifo;
	
	lf_array_queue_list() {
		queue.push_back(put_fifo = new L);
	}
	bool	empty() const {
		return queue.empty();
	}
	
	void put(T &&f) {
		for (;;) {
			tagged_pointer<L>	x = put_fifo;

			if (x && x->push_back(move(f)))
				break;

			L	*x2 = new L;

			if (put_fifo.cas(x, {x2, x.b + 1})) {
				queue.push_back(x2);
			} else {
				delete x2;
			}
		}
	}
	
	bool get(T &t) {
		for (;;) {
			auto	f = queue.safe_front();
			if (!f)
				return false;

			if (f->pop_front(t))
				return true;

			if (f == put_fifo.get().a)
				return false;

			if (auto *h = queue.try_pop_front(f))
				delete h;
		}
	}

};

//-----------------------------------------------------------------------------
//	lf_block_fifo_list
//	a list of block fifos
//-----------------------------------------------------------------------------

template<int N> struct lf_block_fifo_node : atomic<slink_base<lf_block_fifo_node<N>>>, atomic<block_fifo<N>> {
	lf_block_fifo_node() { this->next = 0; }
};

template<int N> struct lf_block_fifo_list {
	typedef lf_block_fifo_node<N> L;
	lf_list_queue<L>			queue;
	atomic<tagged_pointer<L>>	put_fifo;
	atomic<freelist<L>>			pool;
	bool						dummy;
	
	using getter = typename L::getter;
	using putter = typename L::putter;
	
	lf_block_fifo_list() {
		queue.push_back(put_fifo = new L);
	}

	bool	get(getter &g) {
		for (;;) {
			auto	f = queue.safe_front();
			if (!f)
				return false;
				
			if (f->pop_front(g))
				return true;

			if (f == put_fifo.get().a)
				return false;

			if (!f->stop_queuing())
				return false;

			if (auto *h = queue.try_pop_front(f)) {
				ISO_CHEAPASSERT(h != queue.tail.get().a);
				pool.release(h);
			}
		}
	}
	putter	put(uint32 size, uint32 align) {
		for (;;) {
			tagged_pointer<L>	f = put_fifo;
			if (auto p = f->push_back(size, align))
				return p;

			auto	f2 = pool.alloc();
			if (!f2) {
				f2 = new L;
			} else {
				f2->start_queuing();
			}

			if (put_fifo.cas(f, {f2, f.b + 1})) {
				ISO_CHEAPASSERT(f2 != queue.tail.get().a);
				queue.push_back(f2);
			} else {
				f2->stop_queuing();
				pool.release(f2);
			}
		}
	}
	
	template<typename T> T *put(T &&t) {
		return new (put(sizeof(T), alignof(T))) T(move(t));
	}
};

//-----------------------------------------------------------------------------
//	lf_list_queue2
//	based on: An Optimistic Approach to Lock-Free FIFO Queues,Edya Ladan-Mozes and Nir Shavit
//-----------------------------------------------------------------------------

struct lf_list_queue2 {
	struct N {
		atomic<N*>	next, prev;
	};
	atomic<N*>		tail, head;
	N				dummy;

	lf_list_queue2() {
		head = tail = &dummy;
	}

	void fix(N *t, N *h) {
		for (N *n = tail, *prev; h == head && n != h; n = prev) {
			prev = n->prev;
			if (prev->next != n)
				prev->next = n; // fix
		}
	}

	void push_back(N *n) {
		n->next = 0;
		for (;;) {
			N	*t	= tail;
			n->prev	= t;
			if (tail.cas(t, n)) {
				t->next = n;
				break;
			}
		}
	}
#if 1
	N *pop_front() {
		for (;;) {
			N	*h		= head;
			N	*t		= tail;
			N	*next	= h->next;

			if (h == head) {
				if (h == &dummy) {			// head is dummy? 
					if (t == h)
						return NULL;		// empty

					if (!next)
						fix(t, h);
					else
						head.cas(h, next);	// skip dummy

				} else {
					if (t == h) {			// Last node? 
						dummy.prev = t;
						if (tail.cas(t, &dummy))
							h->next = &dummy;

					} else if (!next) {
						fix(t, h);

					} else if (head.cas(h, next)) {
						return h;			// done
					}
				}
			}
		}
	}
#else
	N *pop_front() {
		for (;;) {
			N	*h	= head;
			if (N *next = h->next) {
				if (head.cas(h, next))
					return next;

			} else if (h == tail) {
				return 0;	// empty

			} else {
				fix(tail, h);
			}
		}
	}
#endif
};

//-----------------------------------------------------------------------------
//	lf_slist
//	based on: High Performance Dynamic Lock-Free Hash Tables and List-Based Sets: Maged M. Michael 
//-----------------------------------------------------------------------------

template<typename L> struct tagged_slink_base {
	atomic<tagged_pointer<L>>	next;
	L		*get()				{ return static_cast<L*>(this); }
	const L	*get()		const	{ return static_cast<const L*>(this); }

	tagged_slink_base()	: next(0)	{}

	void	insert_after(L *link)		{ link->next = next; next = link; }
	void	insert_after(L *a, L *b)	{ b->next = next; next = a; }
	L*		unlink_next()				{ L *n = next; next = n->next; n->next = 0; return n; }
	L*		unlink(L *t)				{ L *n = next; next = t->next; n->next = 0; return n; }
};

template<typename T> struct tagged_slink : public tagged_slink_base<tagged_slink<T>> {
	T		t;
	template<typename T1> tagged_slink(T1 &&t)	: t(forward<T1>(t))	{}
};

template<typename L, typename T, typename P> bool lf_slist_find(const T &key, L *head, L *&_prev, L *&_curr, L *&_next, P &pool) {
	//curr holds pmark
	//next holds cmark

	//for (L *prev = head, *curr = prev->next, *next; unmarked_ptr(curr); curr = next) {

	L *prev = head, *curr = prev->next, *next = 0;
	while (unmarked_ptr(curr)) {
		L*		curr1	= unmarked_ptr(curr);
		auto	ckey	= *get_payload(curr1);

		next = curr1->next;

		if (prev->next != curr1) {
			prev = head;
			curr = prev->next;

		} else if (!is_marked_ptr(next)) {
			if (ckey >= key) {
				_prev = prev;
				_curr = curr;
				_next = next;
				return ckey == key;
			}

			prev = curr;
			curr = next;

		} else if (prev->next.cas(curr1, unmarked_ptr(next))) {
			pool.release(curr1);
			curr = next;

		} else {
			prev = head;
			curr = prev->next;
		}
	}
	_prev = prev;
	_curr = curr;
	_next = next;
	return false;
}

template<typename L, typename P> bool lf_slist_insert(L *x, L *head, P &pool) {
	L	*prev, *curr, *next;
	do {
		if (lf_slist_find(*get_payload(x), head, prev, curr, next, pool))
			return false;
		x->next = unmarked_ptr(curr);
	} while (!prev->next.cas(unmarked_ptr(curr), x));

	return true;
}

template<typename L, typename T, typename P> bool lf_slist_erase(T &key, L *head, P &pool) {
	L	*prev, *curr, *next;
	do {
		if (!lf_slist_find(key, head, prev, curr, next, pool))
			return false;
	} while (!curr->next.cas(unmarked_ptr(next), marked_ptr(next)));

	if (prev->next.cas(unmarked_ptr(curr), unmarked_ptr(next)))
		pool.release(unmarked_ptr(curr));
	else
		lf_slist_find(key, head, prev, curr, next, pool);
	return true;
}

template<typename T> class lf_slist {
	typedef atomic<slink<T>>				L;
	atomic<freelist<L>>						pool;
	atomic<slink_base<atomic<slink<T>>>>	head;

//	typedef tagged_slink<T>	L;
//	tagged_slink_base<L>	head;

public:
	lf_slist() { head.next = 0; }

	bool	insert(const T &t)	{
		L *x = pool.alloc();
		if (!x)
			x = new L(t);
		else
			x->t = t;

		if (lf_slist_insert(x, static_cast<L*>(&head), pool))
			return true;

		pool.release(x);
		return false;
	}

	bool	erase(const T &key) {
		return lf_slist_erase(key, static_cast<L*>(&head), pool);
	}
};

//-----------------------------------------------------------------------------
//	lf_hash_map
//	based on: Split-ordered lists: Lock-free extensible hash tables: Ori Shalev and Nir Shavit
//-----------------------------------------------------------------------------

template<typename H, typename V> class lf_hash_map0 {
	enum {
		MIN_ORDER		= 2,
		MAX_ORDER		= sizeof(H) * 8,
		RESIZE_FACTOR	= 8
	};
	struct N : atomic<slink_base<N>> {
		H				r;
		space_for<V>	_v;
		N(H r) : r(r) {}
		friend H*		get_payload(N *n)		{ return &n->r; }
		friend const H*	get_payload(const N *n)	{ return &n->r; }
	};

	atomic<uint32>			size, order;
	atomic<atomic<N*>*>		orders[MAX_ORDER - MIN_ORDER];
	atomic<growing_pool<N, 64>>		pool;

	bool split(N *x, N *head) {
		N	*prev, *curr, *next;
		do {
			if (lf_slist_find(*get_payload(x), head, prev, curr, next, pool))
				return false;
			x->next = unmarked_ptr(curr);
		} while (!prev->next.cas(unmarked_ptr(curr), nullptr));

		return true;
	}

	N	*get_bucket2(H b) {
		int	o = highest_set_index(b) + 1 - MIN_ORDER;

		if (o <= 0) {
			b	&= bits<H>(MIN_ORDER);
			auto&	bucket	= orders[0][b];
			if (!bucket) {
				N	*dummy	= new(pool) N(reverse_bits(b));
				if (!bucket.cas(0, dummy))
					pool.release(dummy);
			}
			return bucket;

		} else {
			H	b1	= b & bits<H>(o + MIN_ORDER - 1);
			auto&	bucket	= orders[o][b1];
			if (!bucket) {
				N	*dummy	= new(pool) N(reverse_bits(b));
				N	*parent	= get_bucket2(b1);
				if (split(dummy, parent)) {
					bucket = dummy;
				} else {
					pool.release(dummy);
				}
			}
			return bucket;
		}
	}
	N	*get_bucket(H h) {
		return get_bucket2(h & bits64(order + MIN_ORDER));
	}
public:
	lf_hash_map0() : size(0), order(0) { 
		clear(orders);
		orders[0] = new atomic<N*>[1 << MIN_ORDER];
	}
	~lf_hash_map0() {
		for (auto i = &orders[0], e = i + order; i < e; ++i)
			delete[] *i;
	}

	bool	insert(H h, const V &v) {
		uint32	o = order;
		if (size >= (RESIZE_FACTOR << (o + MIN_ORDER))) {
			auto	*n = new atomic<N*>[bit64(o++ + MIN_ORDER)];
			if (orders[o].cas(0, n)) {
				order = o;
			} else {
				delete[] n;
			}
		}

		N	*x		= new(pool) N(reverse_bits(h)|1);
		N	*head	= get_bucket(h);
		N	*curr, *prev, *next;
		do {
			if (lf_slist_find(h, head, prev, curr, next, pool)) {
				pool.release(x);
				return false;
			}
			x->next = unmarked_ptr(curr);
		} while (!prev->next.cas(unmarked_ptr(curr), x));

		new(x->_v) V(v);
		size++;
		return true;
	}

	bool	insert(H h, N *&curr) {
		uint32	o = order;
		if (size >= (RESIZE_FACTOR << (o + MIN_ORDER))) {
			auto	*n = new atomic<N*>[bit64(o++ + MIN_ORDER)];
			if (orders[o].cas(0, n)) {
				order = o;
			} else {
				delete[] n;
			}
		}

		N	*x		= new(pool) N(reverse_bits(h)|1);
		N	*head	= get_bucket(h);
		N	*prev, *next;
		do {
			if (lf_slist_find(h, head, prev, curr, next, pool)) {
				pool.release(x);
				return false;
			}
			x->next = unmarked_ptr(curr);
		} while (!prev->next.cas(unmarked_ptr(curr), x));

		size++;
		curr = x;
		return true;
	}
	N*		find(H h) {
		N	*bucket	= get_bucket(h);
		N	*prev, *curr, *next;
		return lf_slist_find(reverse_bits(h)|1, bucket, prev, curr, next, pool) ? curr : 0;
	}

	bool	erase(H h) {
		N	*bucket	= get_bucket(h);
		if (lf_slist_erase(reverse_bits(h)|1, bucket, pool)) {
			size--;
			return true;
		}
		return false;
	}

	optional<V&>	get(H h) const {
		if (auto e = find(h))
			return *(V*)e->_v;
		return none;
	}
	V&				put(H h) {
		N	*n;
		if (insert(h, n))
			new(n->_v) V();
		return *(V*)n->_v;
	}
	template<typename U> V& put(H h, U &&u) {
		N	*n;
		if (insert(h, n))
			new(n->_v) V(forward<U>(u));
		else
			*(V*)n->_v = forward<U>(u);
		return *(V*)n->_v;
	}
};

template<typename K, typename T> class lf_hash_map : public lf_hash_map0<typename hash_type<K>::type, T> {
	typedef typename hash_type<K>::type	H;
	typedef lf_hash_map0<H, T>			B;
public:
	auto	operator[](const K &k)	const	{ return B::get(hash(k)); }
	auto	operator[](const K &k)			{ return putter<B, H, optional<T&>>(*this, hash(k)); }
};

//-----------------------------------------------------------------------------
//	lf_stack
//-----------------------------------------------------------------------------

enum class LF_EXCH_STATE {
	EMPTY	= 0,
	WAITING	= 1,
	BUSY	= 2,
};

template<typename T> struct lf_exch : pair<T, LF_EXCH_STATE> {
	lf_exch() : pair<T, LF_EXCH_STATE>(T(), LF_EXCH_STATE::EMPTY) {}
	lf_exch(const T &a, LF_EXCH_STATE b) : pair<T, LF_EXCH_STATE>(a, b) {}
};

template<typename T> struct lf_exch<T*> : pointer_pair<T, LF_EXCH_STATE, 4> {
	lf_exch() : pointer_pair<T, LF_EXCH_STATE, 4>(0, LF_EXCH_STATE::EMPTY) {}
	lf_exch(T *a, LF_EXCH_STATE b) : pointer_pair<T, LF_EXCH_STATE, 4>(a, b) {}
};

template<typename T> bool exchange(atomic<lf_exch<T>> &slot, T &t, long spins) {
	while (--spins > 0) {
		lf_exch<T>	slot0 = slot;

		switch (slot0.b) {
			case LF_EXCH_STATE::EMPTY: {  // try to place your item in the slot and set state to WAITING 
				lf_exch<T>	slot1(t, LF_EXCH_STATE::WAITING);
				if (slot.cas(slot0, slot1)) {
					while (--spins > 0) {
						slot1 = slot;
						if (slot1.b == LF_EXCH_STATE::BUSY) {
							slot	= slot0;	// reset slot to EMPTY 
							t		= slot1.a;
							return true;
						}
					}

					// if no other thread shows up 
					if (slot.cas(slot1, slot0))  // reset slot to EMPTY 
						return false;

					// some exchanger process must have shown up 
					slot1	= slot;
					slot	= slot0;			// reset slot to EMPTY 
					t		= slot1.a;
					return true;
				}
				break;
			}

			case LF_EXCH_STATE::WAITING:   // some thread is waiting and slot contains its item 
				// replace the item with your own 
				if (slot.cas(slot0, lf_exch<T>(t, LF_EXCH_STATE::BUSY))) {
					t = slot0.a;				// and return the item of the other process 
					return true;
				}
				break;

			case LF_EXCH_STATE::BUSY:    // two other threads are currently using the slot 
				break;
		}
		_atomic_pause();
	}
	return false;
}

template<int E=16> class lf_stack {
	struct N {
		atomic<N*>	next;
	};

	atomic<N*>	top;
	uint64		successes;
	atomic<lf_exch<N*>>	exchanger[E];

	bool visit(N *&n) {
		int		range	= E;
		long	spins	= 1 << clamp(int(successes >> 10), 10, 20);
		return exchange(exchanger[thread_random() % range], n, spins);
	}

public:
	lf_stack() : top(0), successes(0) {}

	void push(N *n) {
		for (;;) {
			N	*old_top = top;
			n->next = old_top;
			if (top.cas(old_top, n))
				return;

			// try to use the elimination array
			N	*other	= n;
			if (visit(other)) {
				if (!other) {		// check whether the value was exchanged with a pop() method
					++successes;	// adjust backoff
					return;
				}
			} else {
				--successes;		// adjust backoff
			}
		}
	}

	N *pop() {
		for (;;) {
			N *old_top = top;
			if (!old_top)
				return 0;	//EMPTY_STACK

			N *next = old_top->next;
			if (top.cas(old_top, next))
				return old_top;

			N	*other	= 0;
			if (visit(other)) {
				if (other) {		// check whether the value was exchanged with a push() method
					++successes;	// adjust backoff
					return other;
				}
			} else {
				--successes;		// adjust backoff
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	SPINLOCKS
//-----------------------------------------------------------------------------

template<typename T> struct writelock_s {
	T	&t;
	writelock_s(T &t) : t(t) {}
	void lock()		{ t.write_lock(); }
	void unlock()	{ t.write_unlock(); }
};
template<typename T> writelock_s<T> make_writelock(T &t) { return t; }


//-----------------------------------------------------------------------------
//	spinlock
//-----------------------------------------------------------------------------

class spinlock_backoff : public spinlock {
	static const size_t MAX_WAIT_ITERS		= 0x10000;
	static const size_t MIN_BACKOFF_ITERS	= 32;
	static const size_t MAX_BACKOFF_ITERS	= 1024;
public:
	void lock() {
		for (size_t delay = MIN_BACKOFF_ITERS;;) {
			for (size_t i = 0; flag; i++) {
				if (i < MAX_WAIT_ITERS)
					_atomic_pause();
				else
					Thread::sleep(1e-4f);
			}
			if (!exchange(flag, true))
				break;

			for (size_t i = thread_random() % delay; i--;)
				_atomic_pause();

			delay = min(delay * 2, MAX_BACKOFF_ITERS);
		}
	}
};

class spinlock_rw {
	enum { WRFLAG = 1u << 31 };
	atomic<uint32>	u;

public:
	spinlock_rw() : u(0) {}

	void write_lock() {
		do {
			while (u)
				_atomic_pause();
		} while (exchange(u, WRFLAG) != 0);
	}
	void write_unlock() {
		u &= ~WRFLAG;
	}
	void lock() {
		++u;
		while (u & WRFLAG)
			_atomic_pause();
	}
	void unlock() {
		--u;
	}
	auto write() {
		return make_writelock(*this);
	}
};

//-----------------------------------------------------------------------------
//	ticket
//-----------------------------------------------------------------------------

class ticket_lock {
protected:
	alignas(CACHELINE_SIZE) atomic<uint32> serving, next;
public:
	ticket_lock() : serving(0), next(0) {}
	void lock() {
		uint32 ticket = next++;
		while (serving != ticket)
			_atomic_pause();
	}

	void unlock() {
		serving = serving + 1;
	}
};

class sticket_lock_backoff : ticket_lock {
	static const size_t BACKOFF_BASE = 10;
public:
	void lock() {
		uint32 ticket = next++;
		for (uint32 serving2; (serving2 = serving) != ticket;) {
			for (uint32 waits = BACKOFF_BASE * (ticket - serving2); waits--;)
				_atomic_pause();
		}
	}
};

class ticket_lock_rw {
	union {
		atomic<uint32>	u;
		struct {
			uint8 w, r, users;
		};
	};

	ticket_lock_rw() : u(0) {}

	void write_lock() {
		uint8	val	= (u.post() += 1 << 16) >> 16;
		while (val != w)
			_atomic_pause();
	}
	void write_unlock() {
		u += 0x0101;
	}
	bool try_lock_write() {
		uint32	u0	= users * 0x10001 + (r << 8);
		return u.cas(u0, u0 + 0x10000);
	}
	void lock() {
		uint8	val	= (u.post() += 1 << 16) >> 16;
		while (val != r)
			_atomic_pause();
		_atomic_inc(&r);
	}
	void unlock() {
		_atomic_inc(&w);
	}
	bool try_lock() {
		uint32	u0	= users * 0x10100 + w;
		return u.cas(u0, u0 + 0x10100);
	}
	auto write() {
		return make_writelock(*this);
	}
};

//-----------------------------------------------------------------------------
//	MCS
//-----------------------------------------------------------------------------

class MCS_lock {
public:
	struct Node {
		atomic<Node*>	next;
		atomic<bool>	blocked;
		void wait() {
			while (blocked)
				_atomic_pause();
		}
	};
protected:
	struct with_mcs {
		MCS_lock &lock;
		Node	node;
		with_mcs(MCS_lock &lock) : lock(lock)	{ lock.lock(node); }
		~with_mcs()								{ lock.unlock(node); }
	};

	atomic<Node*> tail;
public:
	MCS_lock()				: tail(nullptr) {}
	MCS_lock(MCS_lock &&b)	: tail(exchange(b.tail, nullptr)) {}
	~MCS_lock()				{ unlock_all(); }

	bool try_lock(Node &n) {
		n.next = nullptr;
		if (Node *t = exchange(tail, &n)) {
			n.blocked	= true;
			t->next		= &n;
			return false;
		}
		return true;
	}
	void lock(Node &n) {
		n.next = nullptr;
		if (Node *t = exchange(tail, &n)) {
			n.blocked	= true;
			t->next		= &n;
			n.wait();
		}
	}
	void unlock(Node &n) {
		if (!n.next) {
			if (tail.cas(&n, nullptr))
				return;

			while (!n.next)
				_atomic_pause();
		}
		n.next->blocked = false;
	}
	void unlock_all() {
		for (Node *t = tail, *n; t; t = n) {
			n = t->next;
			t->blocked = false;
		}
	}
	bool locked() const { return !!tail; }

	friend with_mcs with(MCS_lock &t)	{ return t; }
};

class MCS_lock_rw : MCS_lock {
	enum {
		WRITER_INTERESTED	= 1 << 30,
		WRITER_ACTIVE		= 1 << 31,
	};

	atomic<Node*>	reader_head, writer_head;
	atomic<uint32>	readers;

public:
	MCS_lock_rw() : reader_head(nullptr), writer_head(nullptr), readers(0) {}

	void write_lock(Node &n) {
		n.blocked	= true;
		n.next		= 0;
		if (Node *t = exchange(tail, &n)) {
			t->next = &n;
		} else {
			writer_head = &n;
			if ((readers.post() |= WRITER_INTERESTED) == 0) {
				if (readers.cas(WRITER_INTERESTED, WRITER_ACTIVE))
					return;
			}
		}
		while (n.blocked)
			_atomic_pause();
	}
	void write_unlock(Node &n) {
		writer_head = nullptr;
		// clear writer flag and test for waiting readers
		if ((readers.post() &= ~WRITER_ACTIVE) != 0) {
			//waiting readers exist
			if (Node *head = exchange(reader_head, nullptr))
				head->blocked = false;
		}
		if (n.next || !tail.cas(&n, nullptr)) {
			while (!n.next)
				_atomic_pause();
			writer_head = n.next;
			if ((readers.post() |= WRITER_INTERESTED) == 0) {
				if (readers.cas(WRITER_INTERESTED, WRITER_ACTIVE))
					writer_head->blocked = false;
			}
		}
	}
	void lock(Node &n) {
		if (readers++ & WRITER_ACTIVE) {
			n.blocked	= true;
			n.next		= exchange(reader_head, &n);
			if (!(readers & WRITER_ACTIVE)) {
				//writer no longer active - wake any waiting readers
				if (Node *head = exchange(reader_head, nullptr))
					head->blocked = false;
			}
			while (n.blocked)
				_atomic_pause();

			if (n.next)
				n.next->blocked = false;
		}
	}
	void unlock(Node &n) {
		// if I'm the last reader, resume the first waiting writer if any
		if (readers-- == WRITER_INTERESTED + 1) {
			if (readers.cas(WRITER_INTERESTED, WRITER_ACTIVE))
				writer_head->blocked = false;
		}
	}
	auto write() {
		return make_writelock(*this);
	}
};

class MCS_lock_rw_fair {
	enum STATE {
		NONE		= 0,
		BLOCKED		= 1,
		NEXT_READER	= 2,
		NEXT_WRITER	= 4,
	};
	struct Node {
		atomic<Node*>	next;
		atomic<bool>	writer;
		atomic<uint8>	state;
	};

	atomic<Node*>	tail, next_writer;
	atomic<uint32>	readers;

	Node *get_next(Node &n) {
		if (Node *p = n.next)
			return p;

		if (tail.cas(&n, nullptr))
			return 0;

		Node	*p;
		while (!(p = n.next))
			_atomic_pause();
		return p;
	}

public:
	MCS_lock_rw_fair() : tail(0), next_writer(0), readers(0) {}

	void write_lock(Node &n) {
		n.writer	= true;
		n.next		= 0;
		n.state		= BLOCKED;

		if (Node *t = exchange(tail, &n)) {
			t->state |= NEXT_WRITER;
			t->next = &n;

		} else {
			next_writer = &n;
			if (readers == 0 && exchange(next_writer, nullptr) == &n)
				n.state &= ~BLOCKED;
		}
		while (n.state & BLOCKED)
			_atomic_pause();
	}
	void write_unlock(Node &n) {
		if (Node *next = get_next(n)) {
			if (!next->writer)
				++readers;
			next->state &= ~BLOCKED;
		}
	}
	void lock(Node &n) {
		n.writer	= false;
		n.next		= 0;
		n.state		= BLOCKED;
		if (Node *t = exchange(tail, &n)) {
			if (t->writer || t->state.cas(BLOCKED, BLOCKED | NEXT_READER)) {
				// t is writer or a waiting reader; t will increment readers and release me
				t->next = &n;
				while (n.state & BLOCKED)
					_atomic_pause();
			} else {
				++readers;
				t->next = &n;
				n.state &= ~BLOCKED;
			}

		} else {
			++readers;
			n.state &= ~BLOCKED;
		}
		if (n.state & NEXT_READER) {
			while (!n.next)
				_atomic_pause();
			++readers;
			n.next->state &= ~BLOCKED;
		}
	}
	void unlock(Node &n) {
		if (Node *next = get_next(n)) {
			if (n.state & NEXT_WRITER)
				next_writer = next;
		}
		if (readers-- == 1) {
			//I'm last reader; wake writer if any
			if (Node *w = exchange(next_writer, nullptr))
				w->state &= ~BLOCKED;
		}
	}
	auto write() {
		return make_writelock(*this);
	}
};

class MCS_barrier {
public:
	struct Node {
		union four_flags {
			uint32	u;
			uint8	b[4];
			uint8&	operator[](int i)		{ return b[i]; }
			uint8	operator[](int i) const	{ return b[i]; }
		};
		uint8			*parent;
		atomic<Node*>	child[2];
		four_flags		havechild, childnotready;
		uint8			sense, parentsense;

		void init(atomic<Node*>	&root, int i) {
			parent			= 0;
			child[0]		= 0;
			child[1]		= 0;
			parentsense		= false;
			sense			= true;
			havechild.u		= childnotready.u = 0;

			atomic<Node*>	*p = &root;

			for (int j = highest_set_index(i + 1); j; --j) {
				Node	*p2;
				while (!(p2 = *p))
					_atomic_pause();
				p = &p2->child[((i - 1)>> (j - 1)) & 1];
			}
			*p = this;

			if (i) {
				Node	*p2 = root;
				for (int k = (i - 1) / 4, j = highest_set_index(k + 1); j; --j)
					p2 = p2->child[((k - 1)>> (j - 1)) & 1];
		
				int	c	= (i - 1) & 3;
				parent	= &p2->childnotready[c];
				p2->childnotready[c] = p2->havechild[c] = true;
			}
		}

		void barrier() {
			while (childnotready.u)
				_atomic_pause();

			childnotready = havechild; // prepare for next barrier
		
			if (parent) {
				*parent = false; // let parent know I'm ready
				while (parentsense != sense)
					_atomic_pause();
			}

			// signal children in wakeup tree
			if (Node *c = child[0])
				c->parentsense = sense;
			if (Node *c = child[1])
				c->parentsense = sense;

			sense = !sense;
		}

	};

	atomic<uint32>	p;
	atomic<Node*>	root;

	MCS_barrier() : p(0), root(0) {}
	void		init(Node &n)		{ n.init(root, p++); }
	static void	barrier(Node &n)	{ n.barrier(); }
};

//-----------------------------------------------------------------------------
//	CLH
//-----------------------------------------------------------------------------

struct CLH_lock {
	enum {
		READY	= 0,
		WAIT	= 1,
	};
	typedef atomic<uint8> node;

	struct per_thread {
		node *p, n;
		per_thread() : p(&n) {}
	};

	node			spare;
	atomic<node*>	tail;
	node*			locked;

	CLH_lock() : spare(READY), tail(&spare) {}

	void lock() {
		static thread_local per_thread thread;
		per_thread	&local	= thread;
		node		*curr	= local.p;
		*curr				= WAIT;

		if (node *prev = exchange(tail, curr)) {
			while (*prev != READY)
				_atomic_pause();
			local.p	= prev;
		}
		locked	= curr;
	}
	void unlock() {
		*locked = READY;
	}

};

}//namespace iso

#endif	// LOCKFREE_H
