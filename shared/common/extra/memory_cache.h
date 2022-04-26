#ifndef MEMORY_CACHE_H
#define MEMORY_CACHE_H

#include "base/defs.h"
#include "base/list.h"
#include "thread.h"

namespace iso {
	
	//-----------------------------------------------------------------------------
	//	memory_interface
	//-----------------------------------------------------------------------------

	struct memory_interface : refs<memory_interface> {
		virtual ~memory_interface()	{}
		virtual bool	_get(void *buffer, uint64 size, uint64 address)	= 0;
		virtual uint64	_next(uint64 addr, uint64 &size, bool dir)	{ return 0; };

		bool			get(void *buffer, uint64 size, uint64 address)	{ return _get(buffer, size, address); }
		bool			get(const memory_block &m, uint64 address)		{ return _get(m.p, m.size(), address); }

		malloc_block	get(uint64 address, uint64 size) {
			malloc_block	m(size);
			return _get(m, size, address) ? m : none;
		}
		malloc_block	get(const memory_block &m) {
			return get((uint64)(void*)m, m.size32());
		}
		template<typename T> bool	read(T &t, uint64 address) {
			return _get(&t, sizeof(T), address);
		}

		bool	operator()(uint64 address, void *buffer, uint64 size) {
			return _get(buffer, size, address);
		}

	};

	struct volatile_memory_interface : memory_interface {
		bool	_get(void *buffer, uint64 size, uint64 address) { return true; }
	};

	struct throw_memory_interface : memory_interface {
		bool	_get(void *buffer, uint64 size, uint64 address) { throw_accum("No memory at 0x" << hex(address)); return false; }
	};

	struct local_memory_interface : memory_interface {
		bool	_get(void *buffer, uint64 size, uint64 address) { memcpy(buffer, (void*)address, size); return true; }
	};

	//-----------------------------------------------------------------------------
	//	memory_cache
	//-----------------------------------------------------------------------------

	template<typename T, typename C> struct memory_cache_getter {
		C					&cache;
		memory_interface	*m;
		T					addr;
		struct ref {
			const memory_cache_getter	&g;
			ref(const memory_cache_getter &_g) : g(_g) {}
			template<typename U> operator U&() const	{ return *(U*)g; }
		};

		memory_cache_getter(C &_cache, T _addr, memory_interface *_m = 0) : cache(_cache), m(_m), addr(_addr) {}
		template<typename U> operator U*()	const	{ return cache(addr, sizeof(U), m); }
		ref		operator*()					const	{ return *this; }
	};

	template<typename T> class memory_cache {
	protected:
		struct rec : e_link<rec> {
			atomic<uint32>	nrefs;
			T				start, end;
			rec(T start, T end) : nrefs(1), start(start), end(end) {}
			~rec()								{ ISO_ASSERT(nrefs == 0); }
			uint8	*data()				const	{ return (uint8*)(this + 1); }
			T		size()				const	{ return end - start; }
			void	addref()					{ ++nrefs; }
			void	release()					{ if (!--nrefs) delete this; }
			bool	fill(memory_interface &m)	{ return m.get(data(), size(), start); }
		};

		e_list<rec> list;

		auto	find_block(T start) {
			auto r	= list.begin();
			while (r != list.end() && r->end < start)	// don't need r->end <= start
				++r;
			return r;
		}
		rec*	create_block(T start, T len) {
			return new(malloc(sizeof(rec) + len)) rec(start, start + len);
		}
		rec*	merge_block(memory_interface &m, rec *r, T start, T end);

	public:
		typedef	typename e_list<rec>::iterator			iterator;
		typedef	typename e_list<rec>::const_iterator	const_iterator;
		iterator		begin()				{ return list.begin(); }
		iterator		end()				{ return list.end(); }
		const_iterator	begin()	const		{ return list.begin(); }
		const_iterator	end()	const		{ return list.end(); }

		struct cache_block : memory_block {
			ref_ptr<rec>	r;
			cache_block()				: memory_block()	{}
			cache_block(const _none&)	: memory_block()	{}
			cache_block(const cache_block &b)			: memory_block(b.begin(), b.length()), r(b.r) {}
//			cache_block(cache_block &&b)				: r(move(b.r)), memory_block(b.begin(), b.length()) {}
			cache_block(rec *r, T start, T len)			: memory_block(r->data() + start - r->start, len), r(r) {}
			cache_block(rec *r, const memory_block &b)	: memory_block(b), r(r) {}
			cache_block(const memory_block &b)			: memory_block(b) {}
			cache_block		slice(void *a, void *b)				const	{ return cache_block(r, memory_block::slice(a, b)); }
			cache_block		slice(intptr_t a, intptr_t b = 0)	const	{ return cache_block(r, memory_block::slice(a, b)); }
			cache_block		sub_block_to(intptr_t b)			const	{ return slice(0, b); }
			cache_block		sub_block_to(void *b)				const	{ return slice(start, b); }
			T				address(const void *a)				const	{ return r ? r->start + T((uint8*)a - r->data()) : 0; }
			T				address()							const	{ return address(memory_block::p); }
			arbitrary_ptr	at_address(T a)						const	{ return r->data() + (a - r->start); }
			bool			contains(T a)						const	{ return r && a >= r->start && a < r->end; }
		};

		template<typename U> struct typed_cache_block : cache_block {
			U	*operator->()	const	{ return *this; }
			typed_cache_block(const cache_block &b) : cache_block(b) {}
		};

		~memory_cache();

		uint8*	add_block(T start, T len);
		void	remove(T start, T len);
		void	add_block(T start, const const_memory_block &mem)		{ memcpy(add_block(start, (T)mem.length()), mem, mem.length()); }

		cache_block operator()(T start);
		cache_block operator()(T start, T len, memory_interface *m = 0);

		template<typename U> U *get(T start, memory_interface *m = 0) {
			return (U*)operator()(start, sizeof(U), m);
		}
		memory_cache_getter<T, memory_cache<T> > get(T start, memory_interface *m = 0) {
			return memory_cache_getter<T, memory_cache<T> >(*this, start, m);
		}
		T	find_next(T start) {
			iterator r	= list.begin();
			while (r != list.end() && r->end <= start)
				++r;
			return r != list.end() ? max(start, r->start) : 0;
		}
	};

	template<typename T> memory_cache<T>::~memory_cache() {
		while (!list.empty())
			list.pop_back()->release();
	}

	template<typename T> typename memory_cache<T>::cache_block memory_cache<T>::operator()(T start) {
		auto	r = find_block(start);
		return r != list.end() && r->start <= start && r->end > start ? cache_block(r.get(), start, r->end - start) : cache_block();
	}

	template<typename T> typename memory_cache<T>::cache_block memory_cache<T>::operator()(T start, T len, memory_interface *m) {
		auto	r	= find_block(start);
		T		end	= start + len;

		if (r != list.end() && r->start <= start && r->end >= end)
			return cache_block(r.get(), start, len);

		if (!m || len == 0 || start == 0)
			return cache_block();

		static const uint32 alignment	= 256;
		T	end2	= align(end, alignment);
		T	start2	= align_down(start, alignment);

		rec* r2;
		if (r == list.end() || r->start > end2) {
			if (r2 = create_block(start2, end2 - start2)) {
				if (!r2->fill(*m)) {
					r2->release();
					return cache_block();
				}
				r.insert_before(r2);
			}
		} else {
			r2 = merge_block(*m, r.get(), start2, end2);
		}
		return cache_block(r2, start, len);
	}

	template<typename T> typename memory_cache<T>::rec *memory_cache<T>::merge_block(memory_interface &m, typename memory_cache<T>::rec *r, T start, T end) {
		start		= min(r->start, start);
		iterator re	= r->iterator();
		while (re != list.end() && re->start <= end) {
			end	= max(end, re->end);
			++re;
		}

		rec	*r2	= create_block(start, end - start);
		if (r2) {
			r->insert_before(r2);

			T	p	= start;
			while (r != list.end().get() && r->start <= end) {
				if (r->start > p)
					m.get(r2->data() + p - start, r->start - p, p);
				memcpy(r2->data() + r->start - start, r->data(), r->end - r->start);
				rec	*old = r;
				p = r->end;
				r = r->next;
				old->unlink();
				old->release();
			};

			if (p < end)
				m.get(r2->data() + p - start, end - p, p);
		}
		return r2;
	}

	template<typename T> uint8 *memory_cache<T>::add_block(T start, T len) {
		auto	r	= find_block(start);
		T		end	= start + len;

		if (r == list.end() || r->start > start || r->end < end) {
			T	s	= r == list.end() ? start : min(r->start, start);
			T	e	= end;
			for (iterator i = r; i != list.end() && i->start <= end; ++i)
				e	= max(e, i->end);

			auto	r2	= create_block(s, e - s);
			if (!r2)
				return nullptr;

			r.insert_before(r2);

			while (r != list.end() && r->start <= end) {
				memcpy(r2->data() + r->start - s, r->data(), r->end - r->start);
				rec	*old = (r++).get();
				old->unlink();
				old->release();
			}
			--r;	// should be back to one we just added
			ISO_ASSERT(r.get() == r2);
		}

		return r->data() + start - r->start;
	}

	template<typename T> void memory_cache<T>::remove(T start, T len) {
		auto	r	= find_block(start);
		T		end	= start + len;

		if (r == list.end() || end <= r->start || start >= r->end)
			return;

		if (r->end > end) {
			rec	*r1 = create_block(end, r->end - end);
			memcpy(r1->data(), r->data() + end - r->start, r1->size());
			r.insert_after(r1);
		}

		if (r->start < start) {
			rec	*r0 = create_block(r->start, start - r->start);
			memcpy(r0->data(), r->data(), r0->size());
			r.insert_before(r0);
		}
		r->unlink();
		r->release();
	}

	template<typename T> class mutex_memory_cache : public memory_cache<T> {
		typedef memory_cache<T> base;
		Mutex		mutex;
	public:
		using typename memory_cache<T>::cache_block;
		cache_block operator()(T start)									{ return with(mutex), base::operator()(start); }
		cache_block operator()(T start, T len, memory_interface *m = 0)	{ return with(mutex), base::operator()(start, len, m); }
		template<typename U> U *get(T start, memory_interface *m = 0)	{ return (U*)operator()(start, sizeof(U), m); }
		auto	get(T start, memory_interface *m = 0)					{ return memory_cache_getter<T, mutex_memory_cache<T> >(*this, start, m); }
		void	remove(T start, T len)									{ return with(mutex), base::remove(start, len);	 }
		uint8*	add_block(T start, T len)								{ return with(mutex), base::add_block(start, len); }
		void	add_block(T start, const memory_block &mem)				{ with(mutex), base::add_block(start, mem); }
	};


} // namespace iso

#endif //MEMORY_CACHE_H
