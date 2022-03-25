#ifndef POOL_H
#define POOL_H

#include "base/list.h"
#include "base/array.h"
#include "allocator.h"
#include "tlsf.h"

namespace iso {

//-----------------------------------------------------------------------------
// freelist:
//-----------------------------------------------------------------------------

template<typename T> class freelist {
protected:
public:
	T*	avail;
public:
	freelist() : avail(nullptr) {}
	freelist(freelist &&b) : avail(exchange(b.avail, nullptr)) {}

	bool	empty() const {
		return !avail;
	}
	T*		reset(T *t = 0) {
		return exchange(avail, t);
	}
	void	release(T *t, int n) {
		T	*t1 = t;
		while (--n) {
			*(T**)t1 = t1 + 1;
			++t1;
		}
		*(T**)t1 = avail;
		avail	= t;
	}
	void	release(T *t) {
		*(T**)t = avail;
		avail	= t;
	}
	T*		alloc() {
		T	*t = avail;
		if (t)
			avail = *(T**)t;
		return t;
	}
};

//-----------------------------------------------------------------------------
// fixed_pool:
//-----------------------------------------------------------------------------

template<typename T, int N> class fixed_pool {
protected:
	typedef uint_bits_t<LOG2_CEIL(N + 1)>	I;
	I				avail;
	space_for<T[N]>	array;
public:
	typedef T	element;
	fixed_pool() : avail(1) {
		for (int i = 0; i < N - 1; i++)
			*(I*)&array[i] = i + 2;
		*(I*)&array[N - 1] = 0;
	}

	int		index_of(const T *t) const	{ return int(t - array + 1); }
	T*		by_index(int i)				{ return array + i - 1; }

	T*		alloc() {
		if (!avail)
			return 0;
		T	*t = by_index(avail);
		avail = *(I*)t;
		return t;
	}
	void	release(T *t) {
		*(I*)t	= avail;
		avail	= index_of(t);
	}
	bool	empty() const {
		return !avail;
	}
};

//-----------------------------------------------------------------------------
// fixed_pool_deferred:
//-----------------------------------------------------------------------------

template<typename T, int N> class fixed_pool_deferred : public fixed_pool<T, N> {
	typedef fixed_pool<T, N>	B;
	using typename B::I;
	I	deferred = 0;
public:
	void	deferred_release(T *t) {
		if (deferred) {
			*(I*)t = exchange(*(I*)by_index(deferred), B::index_of(t));
		} else {
			deferred = *(I*)t = B::index_of(t);
		}
	}
	void	process_deferred() {
		if (deferred) {
			B::avail	= exchange(*(I*)by_index(deferred), B::avail);
			deferred = 0;
		}
	}
};

//-----------------------------------------------------------------------------
// growing_pool:
//-----------------------------------------------------------------------------

template<typename T, int N = 256> class growing_pool : public freelist<T> {
public:
	T*	alloc() {
		T	*t = freelist<T>::alloc();
		if (!t) {
			t = allocate<T>(N);
			this->release(t + 1, N - 1);
		}
		return t;
	}
};

//-----------------------------------------------------------------------------
// pool_index:
//-----------------------------------------------------------------------------

template<typename P, typename I> struct pool_index {
	typedef typename P::element	element;
	I			i;
#if 1
	static P	pool;
	template<typename...A> element *alloc(A&&...a)	{ element *t = new(pool) element(forward<A>(a)...); i = pool.index_of(t); return t; }
	void		release()							{ pool.release(get()); }
	element		*get()			const				{ return i == 0 ? 0 : pool.by_index(i); }
#else
	static P&	pool() { static P pool; return pool; }
	template<typename...A> element *alloc(A&&...a)	{ element *t = new(pool()) element(forward<A>(a)...); i = pool().index_of(t); return t; }
	void		release()							{ pool().release(get()); }
	element		*get()			const				{ return i == 0 ? 0 : pool().by_index(i); }
#endif
	element		*operator->()	const				{ return get(); }
	bool		operator!()		const				{ return i == 0; }
	explicit operator bool()	const				{ return i != 0; }
	pool_index() : i(0) {}
};

template<typename P, typename I> P	pool_index<P, I>::pool;

//-----------------------------------------------------------------------------
// chunk_allocator:
//-----------------------------------------------------------------------------

#define DEBUG_ALLOCATOR(x)//	x;
class chunk_allocator {
public:
	struct alloc_unit : e_link<alloc_unit> {
		uint32	start, size;
		void	set(uint32 _start, uint32 _size) { start = _start; size = _size; }
	};
	typedef	e_list<alloc_unit>	list;

private:
	list					avail;
	list					allocated;
	freelist<alloc_unit>	pool;
	alloc_unit				*allocs;
	DEBUG_ALLOCATOR(uint32 total_alloc)

	alloc_unit	*make_record(uint32 start, uint32 size) {
		alloc_unit	*p = pool.alloc();
		if (p) {
			p->set(start, size);
		}
		return p;
	}
	void	release_to_pool(alloc_unit *p) {
		p->unlink();
		pool.release(p);
	}
	uint32 allocate(alloc_unit &i, uint32 size, uint32 offset) {
		uint32		start = i.start + offset;
		if (offset)
			i.insert_before(ISO_VERIFY(make_record(i.start, offset)));
		if ((i.size -= size + offset) == 0)
			release_to_pool(&i);
		else
			i.start += size + offset;
		DEBUG_ALLOCATOR(total_alloc += size)
		return start;
	}
public:
	chunk_allocator(int N)	{
		allocs = new alloc_unit[N];
		pool.release(allocs, N);
		DEBUG_ALLOCATOR(total_alloc = 0)
	}
	chunk_allocator(uint32 start, uint32 size, int N)	{
		allocs	= new alloc_unit[N];
		pool.release(allocs + 1, N - 1);
		DEBUG_ALLOCATOR(total_alloc = 0)
		allocs[0].set(start, size);
		avail.push_back(&allocs[0]);
	}

#ifndef ISO_RELEASE
	bool	validate(uint32 *total = NULL, uint32 *biggest = NULL);
	void	print();
#endif
	uint32	alloc(uint32 size) {
		for (auto &i : avail) {
			if (i.size >= size)
				return allocate(i, size, 0);
		}
		return 0;
	}
	uint32	alloc(uint32 size, uint32 alignment) {
		for (auto &i : avail) {
			if (i.size >= size) {
				uint32	a = -int32(i.start) & (alignment - 1);
				if (i.size >= size + a)
					return allocate(i, size, a);
			}
		}
		return 0;
	}
	uint32	alloc_end(uint32 size, uint32 alignment) {
		for (auto &i : reversed(avail)) {
			if (i.size >= size) {
				uint32	start = (i.start + i.size - size) & -int32(alignment);
				if (start >= i.start)
					return allocate(i, size, start - i.start);
			}
		}
		return 0;
	}
	bool	free(uint32 start, uint32 size) {
		for (auto &i : avail) {
			if (i.start > start) {
				alloc_unit	*j;
				uint32		end = start + size;
				ISO_ASSERT(end <= i.start);
				if (end == i.start) {
					i.start = start;
					i.size	+= size;
					j = &i;
				} else {
					j = make_record(start, size);
					i.insert_before(j);
				}
				if (j != avail.begin().get()) {
					alloc_unit	*p = j->prev;
					ISO_ASSERT(p->start + p->size <= start);
					if (p->start + p->size == start) {
						p->size += j->size;
						release_to_pool(j);
					}
				}

				DEBUG_ALLOCATOR(total_alloc -= size)
				return true;
			}
		}
		avail.push_back(make_record(start, size));
		return false;
	}
	// Must be called with alloc or alloc_end
	bool	add_allocated(uint32 start, uint32 size) {
		if (alloc_unit *p = make_record(start, size)) {
			allocated.push_back(p);
			return true;
		}
		return false;
	}
	// Can be called by itself
	bool	free_allocated(uint32 start) {
		for (auto &i : avail) {
			if (i.start == start) {
				release_to_pool(&i);
				free(start, i.size);
				return true;
			}
		}
		return false;
	}
};

}//namespace iso

template<typename T, int N> inline void *operator new(size_t size, iso::fixed_pool<T, N> &a)	{ return a.alloc(); }
template<typename T, int N> inline void operator delete(void *p, iso::fixed_pool<T, N> &a)		{ a.release((T*)p); }
template<typename T, int N> inline void *operator new(size_t size, iso::growing_pool<T, N> &a)	{ return a.alloc(); }
template<typename T, int N> inline void operator delete(void *p, iso::growing_pool<T, N> &a)	{ a.release((T*)p); }

//-----------------------------------------------------------------------------
//	tlsf::allocator
//-----------------------------------------------------------------------------

namespace tlsf {

template<int N> class allocator {
	struct alloc_unit : e_link<alloc_unit> {
		alloc_unit	*next_free, *prev_free;
		uint32		start, size;
		void		set(uint32 _start, uint32 _size) { start = _start; size = _size; }
		bool		is_used() const { return start & 1; }
	};

	TLSF<alloc_unit, 32, 5, 2>	tlsf;
	fixed_pool<alloc_unit,N>	pool;
	e_list<alloc_unit>			by_start;

	alloc_unit	*make_record(uint32 start, uint32 size) {
		alloc_unit	*p = pool.alloc();
		if (p)
			p->set(start, size);
		return p;
	}

	uint32 allocate(alloc_unit *i, uint32 size, uint32 offset) {
		uint32		start = i->start + offset;
		if (offset) {
			alloc_unit	*f = make_record(i->start, offset);
			i->insert_before(f);
			tlsf.insert_free_block(f, offset);
		}
		if (uint32 extra = i->size - size + offset) {
			alloc_unit	*f = make_record(start + size, extra);
			i->insert_after(f);
			tlsf.insert_free_block(f, extra);
		}

		i->set(start | 1, size);
		return start;
	}
public:
	void	add_memory(uint32 start, uint32 size) {
		alloc_unit	*f	= make_record(start, size);
		auto		i	= by_start.begin();
		while (i != by_start.end() && i->start < start)
			++i;
		i->insert_before(f);
		tlsf.insert_free_block(f, size);
	}
	uint32	alloc(uint32 size) {
		alloc_unit* p = tlsf.allocate_block(size);
		if (p)
			return allocate(p, size, 0);
		return 0;
	}
	uint32	alloc(uint32 size, uint32 alignment) {
		alloc_unit* p = tlsf.allocate_block(size + alignment - 1);
		if (p)
			return allocate(p, size, -int32(p->start) & (alignment - 1));
		return ~0u;
	}
	bool	free(uint32 start) {
		uint32	val = start | 1;
		for (auto i = by_start.begin(); i != by_start.end(); ++i) {
			if (i->start == val) {
				// merge prev
				if (i != by_start.begin()) {
					alloc_unit* prev = i->prev;
					if (!prev->is_used()) {
						tlsf.remove_free_block(prev, prev->size);
						prev->size += i->size;
						i->unlink();
						pool.release(i);
						i = prev;
					}
				}

				// merge next
				alloc_unit* next = i->next;
				if (next != by_start.end() && !next->is_used()) {
					tlsf.remove_free_block(next, next->size);
					next->size += i->size;
					i->unlink();
					pool.release(i);
					i = next;
				}
				i->start = start;
				tlsf.insert_free_block(i, i->size);
				return true;
			}
		}
		return false;
	}
};

} // namespace tlsf

#endif //POOL_H
