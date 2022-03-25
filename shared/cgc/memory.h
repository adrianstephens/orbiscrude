#ifndef MEMORY_H
#define MEMORY_H

namespace cgc {

struct MemoryPool;
typedef void MemoryPoolCleanup(void*);

MemoryPool*	mem_CreatePool(size_t chunksize, unsigned align);
void		mem_FreePool(MemoryPool *);
void*		mem_Alloc(MemoryPool *p, size_t size);
void*		mem_Realloc(MemoryPool *p, void *old, size_t oldsize, size_t newsize);
bool		mem_AddCleanup(MemoryPool *p, MemoryPoolCleanup *fn, void *arg);

} //namespace cgc

#ifdef __cplusplus

template<typename T> struct NextIterator {
	T *s;
	NextIterator(T *s) : s(s) {}
	NextIterator&	operator++()	{ s = s->next; return *this; }
	operator T*()	const			{ return s; }
	T*	operator*()	const			{ return s; }
};

template<typename T> struct NextBuilder {
	T 	*&head;
	T	*last;
	NextBuilder(T *&head) : head(head) {
		if (last = head) {
			while (last->next)
				last = last->next;
		}
	}
	void append(T *t) {
		if (!last)
			head = t;
		else
			last->next = t;
		last = t;
	}
};

template<typename T> NextBuilder<T> BuildList(T *&head) {
	return head;
}

template<typename T> struct List {
	struct iterator {
		const List	*p;
		iterator(const List *p) : p(p) {}
		iterator&	operator++()			{ p = p->next; return *this; }
		operator const List<T>*()	const	{ return p; }
		T*			operator*()		const	{ return p->p; }
	};

	List	*next;
	T		*p;
	List(T *p) : next(0), p(p) {}
	T		*operator[](int i) const;
	
	inline iterator	begin()	const 	{ return this; }
	inline iterator	end() 	const	{ return nullptr; }
	
	T *operator[](int i) {
		List	*p = this;
		while (i-- && p)
			p = p->next;
		return p ? p->p : 0;
	}
};

struct object_size {
	size_t	size;
	object_size(size_t _size) : size(_size) {}
};
inline void *operator new(size_t size, const object_size &realsize)	{ return operator new(realsize.size); }
inline void *operator new(size_t size, cgc::MemoryPool *pool)		{ return mem_Alloc(pool, size); }
inline void *operator new[](size_t size, cgc::MemoryPool *pool)		{ return mem_Alloc(pool, size); }
inline void operator delete(void *p, const object_size &realsize)	{}
inline void operator delete(void *p, cgc::MemoryPool *pool)			{}
inline void operator delete[](void *p, cgc::MemoryPool *pool)		{}
#endif

#endif // MEMORY_H
