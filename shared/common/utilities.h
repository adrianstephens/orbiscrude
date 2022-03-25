#ifndef UTILITIES_H
#define UTILITIES_H

#include "base/defs.h"
#include "base/array.h"
#include "extra/random.h"
#include "allocators/allocator.h"
#include "events.h"
#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Random
//-----------------------------------------------------------------------------

extern rng<simple_random>				random;
extern rng<mersenne_twister32_19937>	random2;

//-----------------------------------------------------------------------------
//	Callbacks
//-----------------------------------------------------------------------------

struct AppEvent : Message<AppEvent> {
	enum STATE {PRE_GRAPHICS, BEGIN, END, UPDATE, RENDER};
	STATE	state;
	AppEvent(STATE _state) : state(_state) {}
};

//-----------------------------------------------------------------------------
//	deferred_delete
//-----------------------------------------------------------------------------

template<class T> inline bool verify_valid(const T *t)	{ return true; }

class deferred_deletes : dynamic_array<callback<void()>> {
public:
	template<typename T> bool undelete(T *t) {
		for (auto &i : *this) {
			if (i.get_me() == t) {
				erase_unordered(&i);
				return true;
			}
		}
		return false;
	}
	
	template<typename T> void push_back(T *t) {
		ISO_ASSERT(verify_valid(t));
#ifndef ISO_RELEASE
		for (auto &i : *this)
			ISO_ASSERT(i.get_me() != t);
#endif
		emplace_back(t, &deleter_fn<T>);
	}
	void		process() {
		for (auto &i : *this)
			i();
		resize(0);
	}
//	~deferred_deletes() { process(); }
	deferred_deletes()	{}
	deferred_deletes(deferred_deletes&&)			= default;
	deferred_deletes& operator=(deferred_deletes&&)	= default;
};

extern deferred_deletes _dd, cleanup_dd;

template<typename T> void deferred_delete(T *t)		{ _dd.push_back(t); }
inline void do_deferred_deletes()					{ _dd.process(); }


//-----------------------------------------------------------------------------
//	thread_temp_allocator
//	arena_allocator on each thread
//-----------------------------------------------------------------------------

extern thread_local arena_allocator<4096> thread_temp_allocator;

struct temp_block : memory_block {
	constexpr temp_block()								{}
	constexpr temp_block(const _none&)					{}
	temp_block(size_t n)								: memory_block(thread_temp_allocator._alloc(n), n)	{}
	template<typename R> temp_block(R &&r, size_t n)	: temp_block(n)					{ this->n = r.readbuff(p, n); }
	temp_block(temp_block &&b)							: memory_block(exchange(b.p, nullptr), b.n)	{}
	~temp_block()										{ if (p) thread_temp_allocator._free_last(p); }
};

template<typename T> class _temp_array : public _ptr_array<T> {
protected:
	constexpr _temp_array(size_t curr_size)	: _ptr_array<T>(thread_temp_allocator.alloc<T>(curr_size), curr_size)	{}
	_temp_array(_temp_array&& b) : _ptr_array<T>(exchange(b.p, nullptr), b.curr_size) {}
	~_temp_array() { thread_temp_allocator._free_last(this->p); }
};
template<typename T> class temp_array : public array_mixout<_temp_array<T>, T> {
	typedef array_mixout<_temp_array<T>, T>	B;
	using B::p; using B::curr_size;
public:
	temp_array(temp_array&&) = default;
	temp_array(size_t n)																		: B(n)						{ fill_new_n(p, n); }
	template<typename U> temp_array(size_t n, U&& u)											: B(n)						{ fill_new_n(p, n, forward<U>(u)); }
	temp_array(initializer_list<T> init) 														: B(init.size())			{ copy_new_n(init.begin(), p, curr_size); }
	template<typename C, typename=enable_if_t<has_begin_v<C>>> 		temp_array(const C &c)		: B(num_elements(c))		{ using iso::begin; copy_new_n(begin(c), p, curr_size); }
	template<typename I, typename=enable_if_t<is_iterator_v<I>>>	temp_array(I first, I last)	: B(distance(first, last))	{ copy_new_n(first, p, curr_size); }
	template<typename R, typename = is_reader_t<R>>					temp_array(R &&r, size_t n)	: B(n)						{ read_new_n(r, p, curr_size); }
	//temp_array& operator=(temp_array&& c) { swap(*(_ptr_array<T>*)this, static_cast<_ptr_array<T>&>(c)); return *this;	}
	template<typename C> auto& operator=(const C& c) { copy_n(begin(c), p, min(num_elements(c), curr_size)); return *this; }
};

template<typename T> class _temp_dynamic_array : public _ptr_max_array<T> {
protected:
	constexpr _temp_dynamic_array(size_t curr_size, size_t max_size)	: _ptr_max_array<T>(thread_temp_allocator.alloc<T>(max_size), curr_size, max_size)	{}
	constexpr _temp_dynamic_array(size_t curr_size)						: _temp_dynamic_array(curr_size, curr_size)								{}
	_temp_dynamic_array(_temp_dynamic_array&& b)						: _ptr_max_array<T>(exchange(b.p, nullptr), b.curr_size, b.max_size)	{}
	~_temp_dynamic_array() { thread_temp_allocator._free_last(this->p); }
};
template<typename T> class temp_dynamic_array : public dynamic_mixout<_temp_dynamic_array<T>, T> {
	typedef dynamic_mixout<_temp_dynamic_array<T>, T>	B;
	using B::p; using B::curr_size;
public:
	temp_dynamic_array(size_t n)																		: B(0, n)					{}
	template<typename U> temp_dynamic_array(size_t n, U&& u)											: B(n)						{ fill_new_n(p, n, forward<U>(u)); }
	temp_dynamic_array(initializer_list<T> init) 														: B(init.size())			{ copy_new_n(init.begin(), p, curr_size); }
	template<typename C, typename=enable_if_t<has_begin_v<C>>> 		temp_dynamic_array(const C &c)		: B(num_elements(c))		{ using iso::begin; copy_new_n(begin(c), p, curr_size); }
	template<typename I, typename=enable_if_t<is_iterator_v<I>>>	temp_dynamic_array(I first, I last)	: B(distance(first, last))	{ copy_new_n(first, p, curr_size); }
	template<typename R, typename=is_reader_t<R>>					temp_dynamic_array(R &&r, size_t n)	: B(n)						{ read_new_n(r, p, curr_size); }
	template<typename C> auto& operator=(C&& c) { this->clear(); return append(c); }
};


template<typename T> struct	temp_deleter {
	void operator()(T *p)	{ thread_temp_allocator._free_last(p); }
};
template<typename T, size_t N> struct temp_deleter<T[N]> {
	void operator()(T *p)	{ thread_temp_allocator._free_last(p); }
};

template<typename T> using temp_ptr = unique_ptr<T, temp_deleter<T>>;

template<typename T, typename... P> force_inline temp_ptr<T> make_temp(P&&...p) {
	return thread_temp_allocator.make<T>(forward<P>(p)...);
}


template<typename T> class temp_var : public temp_ptr<T> {
public:
	template<typename... P> temp_var(P&&...p) : temp_ptr<T>(thread_temp_allocator.make<T>(forward<P>(p)...)) {}
};

}// namespace iso

#endif	// UTILITIES_H
