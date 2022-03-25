#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "base/defs.h"
#include "stddef.h"

namespace iso {

// need to provide:
//
//	void*	_alloc(size_t size);
//	void*	_alloc(size_t size, size_t align);
//	bool	_free(void *p, size_t size);
//	bool	_free(void *p);

template<class A> class allocator {
public:
	void				*alloc(size_t size)						{ return static_cast<A*>(this)->_alloc(size); }
	void				*alloc(size_t size, size_t align)		{ return static_cast<A*>(this)->_alloc(size, align); }
	bool				free(void *p, size_t size)				{ return static_cast<A*>(this)->_free(p, size); }
	bool				free(void *p)							{ return static_cast<A*>(this)->_free(p); }
	memory_block		alloc_block(size_t size)				{ return memory_block(alloc(size), size); }
	memory_block		alloc_block(size_t size, size_t align)	{ return memory_block(alloc(size, align), size); }

	template<typename T>	T*			alloc()					{ return (T*)alloc(sizeof(T), alignof(T)); }
	template<typename T>	T*			alloc(size_t n)			{ return (T*)alloc(sizeof(T) * n, alignof(T)); }
	template<typename T, typename...P>	enable_if_t<!is_array<T>, T*>	make(P&&...p)		{ return new(alloc(calc_size<T>(forward<P>(p)...), alignof(T))) T(forward<P>(p)...); }
	template<typename T>	exists_t<array_t<T>*>	make()		{ return make_array<array_t<T>>(sizeof(T) / sizeof(array_t<T>)); }
	template<typename T>	T*			make_array(size_t n)	{ return new(*this, alignof(T)) T[n]; }
	template<typename T>	void		del(T *t)				{ t->~T(); free(t, sizeof(T)); }
	template<typename T>	T			get()					{ return alloc<typename T_deref<T>::type>(); }
	getter<allocator>					get()					{ return *this; }
	template<typename T>	auto		get_deleter()			{ return [this](void *p) { del((T*)p); }; }
};

struct vallocator : allocator<vallocator> {
	void*	me;
	void*	(*valloc)	(void*, size_t, size_t);
	bool	(*vfree)	(void*, void*);
	bool	(*vfree2)	(void*, void*, size_t);
	void*	(*vrealloc)	(void*, void*, size_t, size_t);

	void*	_alloc(size_t size, size_t align = 16)				const	{ return valloc(me, size, align);		}
	bool	_free(void *p)										const	{ return vfree(me, p);					}
	bool	_free(void *p, size_t size)							const	{ return vfree2(me, p, size);			}
	void*	realloc(void *p, size_t size, size_t align = 16)	const	{ return vrealloc(me, p, size, align);	}

	template<class T> struct thunk {
		static void*	alloc	(void *me, size_t size, size_t align)			{ return ((T*)me)->alloc(size, align);		}
		static bool		free	(void *me, void *p)								{ return ((T*)me)->free(p);					}
		static bool		free2	(void *me, void *p, size_t size)				{ return ((T*)me)->free(p, size);			}
		static void*	realloc	(void *me, void *p, size_t size, size_t align)	{ return ((T*)me)->realloc(p, size, align);	}
	};
	template<typename T> void set(T *t) {
		me			= t;
		valloc		= thunk<T>::alloc;
		vfree		= thunk<T>::free;
		vfree2		= thunk<T>::free2;
		vrealloc	= thunk<T>::realloc;
	}
	vallocator()		{}
	template<typename T> vallocator(T *t) { set(t); }
};

template<typename A> class from {
	static inline	A&	allocator()				{ static A _allocator; return _allocator; }
public:
	inline void*	operator new(size_t size)	{ return allocator().alloc(); }
	inline void		operator delete(void *p)	{ allocator().release((typename A::element*)p); }
};

//-----------------------------------------------------------------------------
//	class malloc_allocator
//-----------------------------------------------------------------------------

class malloc_allocator : public allocator<malloc_allocator> {
	static void*	_alloc(size_t size, size_t align = 16)				{ return iso::malloc(size); }
	static bool		_free(void *p)										{ iso::free(p); return true; }
	static void*	realloc(void *p, size_t size, size_t align = 16)	{ return iso::realloc(p, size); }
};

//-----------------------------------------------------------------------------
//	class linear_allocator
//-----------------------------------------------------------------------------

class linear_allocator : public allocator<linear_allocator> {
protected:
	uint8	*p;
public:
	void	init(void *_p)	{ p = (uint8*)_p; }
	void	*_alloc(size_t size) {
		uint8 *t = p;
		p = t + size;
		return t;
	}
	void	*_alloc(size_t size, size_t align) {
		uint8 *t = (uint8*)((intptr_t(p) + align - 1) & -intptr_t(align));
		p = t + size;
		return t;
	}
	void	*getp()	const	{ return p; }
	linear_allocator(void *p = 0) : p((uint8*)p) {}
};

//-----------------------------------------------------------------------------
//	class checking_linear_allocator
//-----------------------------------------------------------------------------

class checking_linear_allocator : public allocator<checking_linear_allocator> {
	struct back_allocator;

protected:
	uint8	*p, *end;

public:
	checking_linear_allocator() : p(0), end(0) {}
	checking_linear_allocator(const memory_block &m) : p(m), end(m.end()) {}
	void	init(const memory_block &m) { p = m; end = m.end(); }

	void	*_alloc(size_t size) {
		uint8 *t = p;
		if (t + size > end)
			return 0;
		p = t + size;
		return t;
	}
	void	*_alloc(size_t size, size_t align) {
		uint8 *t = (uint8*)((intptr_t(p) + align - 1) & -intptr_t(align));
		if (t + size > end)
			return 0;
		p = t + size;
		return t;
	}
	void	*back_alloc(size_t size) {
		uint8 *t = end - size;
		if (p > t)
			return 0;
		end = t;
		return t;
	}
	void	*back_alloc(size_t size, size_t align) {
		uint8 *t = (uint8*)((intptr_t(end) - size) & -intptr_t(align));
		if (p > t)
			return 0;
		end = t;
		return t;
	}

	void	*getp()		const	{ return p; }
	size_t	remaining()	const	{ return (char*)end - (char*)p; }
	back_allocator& back()		{ return *(back_allocator*)this; }
};

struct back_allocator : checking_linear_allocator, allocator<back_allocator> {
	void	*_alloc(size_t size)				{ return back_alloc(size); }
	void	*_alloc(size_t size, size_t align)	{ return back_alloc(size, align); }
};

//-----------------------------------------------------------------------------
//	class circular_allocator
//-----------------------------------------------------------------------------

class circular_allocator : public allocator<circular_allocator> {
protected:
	uint8		*a, *b;
	uint8		*p, *g;
public:
	circular_allocator() : a(0), b(0), p(0), g(0)	{}
	circular_allocator(const memory_block &m)		{ init(m); }
	void	init(const memory_block &m)				{ a = m; b = m.end(); p = g = a; }

	void	*_alloc(size_t size, size_t align = 4)	{
		uint8 *t = (uint8*)((intptr_t(p) + align - 1) & -intptr_t(align));
		if (p >= g) {
			if (t + size < b) {
				p = t + size;
				return t;
			}
			t = (uint8*)((intptr_t(a) + align - 1) & -intptr_t(align));
		}
		if (t + size < g) {
			p = t + size;
			return t;
		}
		return 0;
	}
	memory_block alloc_upto(size_t size, size_t align = 4) {
		uint8	*t		= (uint8*)((intptr_t(p) + align - 1) & -intptr_t(align));
		size_t	avail	= min((p < g ? g : b) - t, size);
		p += avail;
		return memory_block(t, avail);
	}
	void	*getp()				const	{ return p; }
	void	set_get(void *_g)			{ g = (uint8*)_g; }

	ptrdiff_t	relocate(void *x) {
		ptrdiff_t	d = (uint8*)x - a;
		a	= (uint8*)x;
		b	= b + d;
		p	= p + d;
		g	= g + d;
		return d;
	}

	size_t	remaining()			const	{ return p < g ? g - p : g - p + b - a; }
	size_t	remaining_linear()	const	{ return (p < g ? g : b) - p; }
};

//-----------------------------------------------------------------------------
//	block_fifo
//	allocate blocks from a ringbuffer
//-----------------------------------------------------------------------------

template<int N> class block_fifo {
	uint32		head, tail;
	alignas(16) uint32	buffer[N];

public:
	block_fifo() : head(0), tail(0) {}
	void		clear()			{ tail = head = 0; }
	bool		empty()	const	{ return tail == head; }

	memory_block	pop_front() {
		uint32	t = head;
		if (t == tail)
			return none;

		uint32	n		= buffer[t % N];
		uint32	size	= n & 0x00ffffff;
		uint32	skip	= n >> 24;
		head = t + size;
		return memory_block(buffer + (t + skip) % N, (size - skip) * sizeof(uint32));
	}

	memory_block	push_back(uint32 size, uint32 align) {
		size	/= sizeof(uint32);
		align	/= sizeof(uint32);

		uint32	t	= tail;
		uint32	t1	= t % N;
		uint32	skip = t1 + size > N
			? N - t1							// extra for wrapping
			: 1 + (-(t1 + 1) & (align - 1));	// extra for alignment (inc. size field)

		uint32	n	= size + skip;
		if (t == ~0 || t + n >= head + N)
			return none;	// full

		buffer[t1]	= n | (skip << 24);
		tail		= t + n;
		return memory_block(buffer + (t + skip) % N, size * sizeof(uint32));
	}

	template<typename T> auto push_back(T &&t) {
		typedef noref_t<T>	T2;
		if (memory_block p = push_back(sizeof(T2), alignof(T2)))
			return new(p) T2(forward<T>(t));
		return (T2*)nullptr;
	}
};

//-----------------------------------------------------------------------------
//	arena_allocator
//	can only free last allocation
//-----------------------------------------------------------------------------

template<size_t BLOCK> class arena_allocator : public allocator<arena_allocator<BLOCK>> {
protected:
	struct Node {
		Node*		next;
		uint8*		begin;
		uint8*		end;
		uint8*		free;
		uint8		data[];
		Node(Node *next, size_t capacity) : next(next), begin(data), end(data + capacity), free(data) {}
	};

	Node*	head	= nullptr;
	Node*	spare	= nullptr;

	void	add_node(size_t capacity) {
		if (spare) {
			if (spare->free + capacity <= spare->end) {
				//spare->next = head;	(should be already correct)
				head	= exchange(spare, nullptr);
				return;
			}
			delete exchange(spare, nullptr);	// if spare too small, get rid of it
		}
		head	= new(malloc(capacity + sizeof(Node))) Node(head, capacity);
	}

public:

	~arena_allocator() {
		for (Node *n = head; n; ) {
			Node* next = n->next;
			delete n;
			n = next;
		}
	}

	void*	_alloc(size_t size) {
		if (size == 0)
			return nullptr;
		uint8	*p	= head ? head->free : nullptr;
		if (!p || p + size > head->end) {
			add_node(align(size, BLOCK));
			p	= head->free;
		}
		head->free = p + size;
		return p;
	}

	void*	_alloc(size_t size, size_t alignment) {
		if (size == 0)
			return nullptr;
		uint8	*p	= head ? align(head->free, alignment) : nullptr;
		if (!p || p + size > head->end) {
			add_node(align(size, BLOCK));
			p	= head->begin = align(head->free, alignment);
		}
		head->free = p + size;
		return p;
	}

	void	_free_last(void *p) {
		if (p) {
			ISO_ASSERT(p >= head->begin && p < head->free);
			head->free = (uint8*)p;
			if (p == head->begin) {
				delete spare;
				spare = exchange(head, head->next);
			}
		}
	}
};

}//namespace iso

//-----------------------------------------------------------------------------
//	global functions
//-----------------------------------------------------------------------------

// need to be global for some reason

template<class A> inline void *operator new(size_t size, iso::allocator<A> &a)					{ return a.alloc(size, sizeof(void*)); }
template<class A> inline void *operator new[](size_t size, iso::allocator<A> &a)				{ return a.alloc(size, sizeof(void*)); }
template<class A> inline void *operator new(size_t size, iso::allocator<A> &a, size_t align)	{ return a.alloc(size, align); }
template<class A> inline void *operator new[](size_t size, iso::allocator<A> &a, size_t align)	{ return a.alloc(size, align); }

#endif // ALLOCATOR_H
