#ifndef MARSHAL_H
#define MARSHAL_H

#include "base\hash.h"
#include "base\functions.h"
#include "base\algorithm.h"
#include "base\pointer.h"
#include "allocators\allocator.h"
#include "utilities.h"
#include "comms/ip.h"

namespace iso {

class offset_allocator : public allocator_mixin<offset_allocator> {
protected:
	void	*p;
	size_t	offset;
public:
	hash_map<const void*, const void*>	ptrs;

	offset_allocator(void *_p = 0) : p(_p), offset(0) {}
	void	init(void *_p)	{ p = _p; offset = 0; }
	void	*_alloc(size_t size) {
		if (size == 0)
			return 0;
		size_t	t = offset;
		offset	= t + size;
		return (uint8*)p + t;
	}
	void	*_alloc(size_t size, size_t align) {
		if (size == 0)
			return 0;
		size_t	t = (offset + align - 1) & -intptr_t(align);
		offset = t + size;
		return (uint8*)p + t;
	}
	size_t	size()	const				{ return offset; }
	auto	ptr_hash(const void *p)		{ return ptrs[p]; }
};

//collapses
template<typename A, typename S, typename D = void> struct allocator_plus : public allocator_mixin<allocator_plus<A, S, D>> {
	A			&a;
	const S		&src;
	D			&dst;

	allocator_plus(A &&a, const S &s, D &d) : a(a), src(s), dst(d) {}

	void	*_alloc(size_t size)				const { return a._alloc(size); }
	void	*_alloc(size_t size, size_t align)	const { return a._alloc(size, align); }
	A&		operator->()				{ return a; }
	auto	ptr_hash(const void *p)		{ return a.ptr_hash(p); }
};

template<typename A, typename S> struct allocator_plus<A, S, void> : public allocator_mixin<allocator_plus<A, S, void>> {
	A			&a;
	const S		&src;

	allocator_plus(A &&a, const S &s) : a(a), src(s) {}

	void	*_alloc(size_t size)				const { return a._alloc(size); }
	void	*_alloc(size_t size, size_t align)	const { return a._alloc(size, align); }
	A&		operator->()				{ return a; }
	auto	ptr_hash(const void *p)		{ return a.ptr_hash(p); }
};

template<typename A, typename S> allocator_plus<A,S>				make_allocator_plus(A &&a, const S &s)			{ return {a, s}; }
template<typename A, typename S, typename D> allocator_plus<A,S,D>	make_allocator_plus(A &&a, const S &s, D &d)	{ return {a, s, d}; }

template<typename A, typename S0, typename D0, typename S> allocator_plus<A,S>					make_allocator_plus(allocator_plus<A, S0, D0> &a, const S &s)			{ return {a.a, s}; }
template<typename A, typename S0, typename D0, typename S, typename D> allocator_plus<A,S,D>	make_allocator_plus(allocator_plus<A, S0, D0> &a, const S &s, D &d)		{ return {a.a, s, d}; }

//doesn't collapse
template<typename A, typename T> struct lookup_allocator : public A {
	T		&t;
	lookup_allocator(T &_t) : t(_t) {}
	T&		operator->()	{ return t; }
};

template<typename T>		using rel_ptr		= soft_pointer<T, base_relative<int32> >; 
template<typename T, int I> using rel_counted	= counted<T, I, rel_ptr<T> >; 

template<typename T> struct save_location {
	T		t;
	template<class A, typename T1>	friend void allocate(A &&a, const save_location &t0, save_location<T1> *t1) {
		a.ptr_hash(&t0.t) = nullptr;
		allocate(a, t0.t, &t1->t);
	}
	template<class A, typename T1>	friend void transfer(A &&a, const save_location &t0, save_location<T1> &t1)	{
		a.ptr_hash(&t0.t) = &t1.t;
		transfer(a, t0.t, t1.t);
	}
};
struct saved_location { void *p; };

template<template<class> class M, typename T> struct meta::map<M, save_location<T>> : T_type<save_location<map_t<M, T>>> {};

template<typename T> struct dup_pointer {
	T		t;
	void	operator=(const T _t) { t = _t; }
	operator const T&() const	{ return t; }
	operator T&()				{ return t; }
	friend constexpr auto get(dup_pointer &t)		{ return get(t.t); }
	friend constexpr auto get(const dup_pointer &t) { return get(t.t); }
};
template<template<class> class M, typename T> struct meta::map<M, dup_pointer<T>> : map<M, T> {};

template<typename T> struct lookup {
	T		t;
};
template<typename T> struct lookedup {
	T		t;
	void	operator=(const T _t) { t = _t; }
	operator T() const { return t; }
};
struct handle {
	void	*p;
	handle(void *_p = INVALID_HANDLE_VALUE) : p(_p) {}
	bool	valid()		const	{ return p != INVALID_HANDLE_VALUE; }
	operator void*()	const	{ return p; }
};

template<typename T> struct unwrapped {
	T		t;
	void	operator=(const T _t);
};

struct const_memory_block_rel {
	rel_ptr<const void>	start;
	uint32			size;
	void	init(const void *p, uint32 _size)	{ start = p; size = _size; }
	operator const_memory_block()	const		{ return const_memory_block(start, size); }
};

template<int I> counted<uint8, I> make_memory_block(const void *p) { return (uint8*)p; }

//-----------------------------------------------------------------------------
//	allocate
//-----------------------------------------------------------------------------

template<class A, typename T0, typename T1>	size_t size(A &&a, const T0 &t0, const T1*)		{ return sizeof(T1); }
template<class A, typename T0, typename T1>	void allocate(A &&a, const T0 &t0, T1*)			{}
template<class A, typename T0, typename T1>	void allocate(A &&a, const T0 &t0, const T1*)	{ allocate(a, t0, (T1*)nullptr); }

template<class A, typename T0, typename T1> void pointer_allocate_dup(A &&a, const T0 *t0, const T1*) {
	if (t0) {
		auto	p = a.ptr_hash(t0);
		if (!p.exists()) {
			auto	p1	= a.template alloc<T1>();
			p			= nullptr;
			iso::allocate(a, *t0, unconst(p1));
		}
	}
}
//template<class A> void pointer_allocate_dup(A &&a, const void *t0, const void*) {
//	if (t0) {
//		auto	p = a.ptr_hash(t0);
//		ISO_ASSERT(p.exists());
//	}
//}
template<class A> void pointer_allocate_dup(A &&a, const char *t0, const char*) {
	if (t0)
		a.template alloc<char>(string_len(t0) + 1);
}
template<class A> void pointer_allocate_dup(A &&a, const char16 *t0, const char16*) {
	if (t0)
		a.template alloc<char16>(string_len(t0) + 1);
}

template<class A, typename T0, typename T1> void pointer_allocate(A &&a, const T0* t0, const T1* t1) {
	pointer_allocate_dup(a, t0, t1);
}

//keep unchanged pointers (to point into original data)
template<class A, typename T> void pointer_allocate(A &&a, const T* t0, const T* t1) {}

template<class A, typename T0, typename T1>					void allocate(A &&a, T0 *const t0, T1**)								{ pointer_allocate(a, (const T0*)t0, (const T1*)0); }
template<class A, typename T0, typename B0, typename T1>	void allocate(A &&a, const soft_pointer<T0,B0> &t0, T1**)				{ pointer_allocate(a, (const T0*)t0, (const T1*)0); }
template<class A, typename T0, typename T1, typename B1>	void allocate(A &&a, T0 *const t0, soft_pointer<T1,B1>*)				{ pointer_allocate_dup(a, (const T0*)t0, (const T1*)0); }
template<class A, typename T0, typename B0, typename T1, typename B1>	void allocate(A &&a, const soft_pointer<T0,B0> &t0, soft_pointer<T1,B1>*)	{ pointer_allocate_dup(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename T1>					void allocate(A &&a, const dup_pointer<T0> &t0, T1* t1)					{ pointer_allocate_dup(a, get(t0), noref_t<decltype(get(*t1))>()); }
template<class A, typename T0, typename T1>					void allocate(A &&a, const T0 &t0, dup_pointer<T1>* t1)					{ pointer_allocate_dup(a, get(t0), noref_t<decltype(get(*t1))>()); }


template<class A, typename T0, typename T1> void allocate(A &&a, const T0 &t0, TL_tuple<T1> *t1) {
	return allocate(a, (TL_tuple<TL_fields_t<T0>>&)t0, t1);
}
template<class A, typename T0, typename T1> void allocate(A &&a, const TL_tuple<T0> &t0, T1 *t1) {
	return allocate(a, t0, (TL_tuple<TL_fields_t<T1>>*)t1);
}
template<class A, typename T0, typename T1> void allocate(A &&a, const TL_tuple<T0> &t0, const T1 *t1) {
	return allocate(a, t0, (TL_tuple<TL_fields_t<T1>>*)t1);
}

template<class A> void allocate(A &&a, const TL_tuple<type_list<> > &t, TL_tuple<type_list<> >*) {}

template<class A, typename T0, typename T1, size_t... II>	void allocate_tuple(A &&a, const TL_tuple<T0> &t, TL_tuple<T1> *p, index_list<II...>) {
	bool	dummy[] = {(allocate(a, t.template get<II>(), &p->template get<II>()), true)...};
}
template<class A, typename T0, typename T1>	void allocate(A &&a, const TL_tuple<T0> &t0, TL_tuple<T1> *t1) {
	allocate_tuple(make_allocator_plus(a, t0), t0, t1, meta::TL_make_index_list<T0>());
}
template<class A, typename T0, typename TL0, typename T1, typename TL1> void allocate(A &&a, const as_tuple<T0,TL0> &t0, as_tuple<T1,TL1> *t1) {
	allocate_tuple(make_allocator_plus(a, t0), (const TL_tuple<TL0>&)t0, (TL_tuple<TL1>*)t1, meta::TL_make_index_list<TL0>());
}
template<class A, typename T0, typename TL0, typename T1, typename TL1> void allocate(A &&a, const as_tuple<T0,TL0> &t0, const as_tuple<T1,TL1> *t1) {
	allocate_tuple(make_allocator_plus(a, t0), (const TL_tuple<TL0>&)t0, (TL_tuple<TL1>*)t1, meta::TL_make_index_list<TL0>());
}

template<class A, typename T0, typename T1, typename TL1> void allocate(A &&a, const T0 &t0, as_tuple<T1,TL1> *t1) {
	return allocate(a, (as_tuple<T0>&)t0, t1);
}
template<class A, typename T0, typename TL0, typename T1> void allocate(A &&a, const as_tuple<T0,TL0> &t0, T1 *t1) {
	return allocate(a, t0, (as_tuple<T1>*)t1);
}
template<class A, typename T0, typename TL0, typename T1> void allocate(A &&a, const as_tuple<T0,TL0> &t0, const T1 *t1) {
	return allocate(a, t0, (as_tuple<T1>*)t1);
}

template<class A>	void allocate(A &&a, const const_memory_block &t, const_memory_block_rel*) {
	a.alloc(t.size32());
}

//-----------------------------------------------------------------------------
//	transfer
//-----------------------------------------------------------------------------

template<class A, typename T0, typename T1>	void transfer(A &&a, const T0 &t0, T1 &t1)	{
	t1 = t0;
}
template<class A, typename T0, typename T1>	void transfer(A &&a, const T0 &t0, const T1 &t1)	{
	transfer(a, t0, const_cast<T1&>(t1));
}
template<class A, typename T0, typename T1, int N>	void transfer(A &&a, const T0 (&t0)[N], T1 *t1) {
	for (int i = 0; i < N; i++)
		transfer(a, t0[i], t1[i]);
}
template<class A, typename T, int N>	void transfer(A &&a, const as_tuple<T> (&t0)[N], as_tuple<T> *t1) {
	for (int i = 0; i < N; i++)
		transfer(a, t0[i], t1[i]);
}
template<class A, typename T, int N>	void transfer(A &&a, const T (&t0)[N], T *t1) {
	for (int i = 0; i < N; i++)
		t1[i] = t0[i];
}
template<class A, typename T0, typename T1>	void transfer(A &&a, const lookup<T0> &t0, T1 &t1) {
	t1 = a->lookup(t0.t);
}

template<class A>	void transfer(A &&a, const malloc_block &t0, const_memory_block &t1)	{
	void	*p = a.alloc(t0.size());
	t0.copy_to(p);
	t1 = {p, t0.size()};
}


template<class A, typename T0, typename T1> T1 *pointer_transfer_dup(A &&a, const T0 *t0, const T1 *) {
	if (t0) {
		auto	p	= a.ptr_hash(t0);
		if (!p.exists() || !p) {
			T1	*p1	= a.template alloc<T1>();
			p		= p1;
			iso::transfer(a, *t0, *unconst(p1));
			return p1;
		}
		return (T1*)(const void*)p;
	}
	return 0;
}

//template<class A> void *pointer_transfer_dup(A &&a, const void *t0, const void *) {
//	if (t0) {
//		auto	p	= a.ptr_hash(t0);
//		ISO_ASSERT(p.exists());
//		return (void*)(const void*)p;
//	}
//	return 0;
//}
//
template<class A> const char *pointer_transfer_dup(A &&a, const char *t0, const char *t1) {
	if (size_t len = string_len(t0)) {
		char	*p	= a.template alloc<char>(len + 1);
		memcpy(p, t0, len + 1);
		return p;
	}
	return nullptr;
}
template<class A> const char16 *pointer_transfer_dup(A &&a, const char16 *t0, const char16 *t1) {
	if (size_t len = string_len(t0)) {
		char16	*p	= a.template alloc<char16>(len + 1);
		memcpy(p, t0, (len + 1) * 2);
		return p;
	}
	return nullptr;
}

template<class A, typename T0, typename T1> auto pointer_transfer(A &&a, const T0* t0, const T1* t1) {
	return pointer_transfer_dup(a, t0, t1);
}

//keep unchanged pointers (to point into original data)
template<class A, typename T> auto pointer_transfer(A &&a, const T* t0, const T* t1) {
	return unconst(t0);
}

template<class A, typename T0, typename T1>					void transfer(A &&a, T0 *const t0, T1 *&t1)									{ t1 = pointer_transfer(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename B0, typename T1>	void transfer(A &&a, const soft_pointer<T0,B0> &t0, T1 *&t1)				{ t1 = pointer_transfer(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename T1, typename B1>	void transfer(A &&a, T0 *const t0, soft_pointer<T1,B1> &t1)					{ t1 = pointer_transfer_dup(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename B0, typename T1, typename B1>	void transfer(A &&a, const soft_pointer<T0,B0> &t0, soft_pointer<T1,B1> &t1)	{ t1 = pointer_transfer_dup(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename T1>					void transfer(A &&a, const dup_pointer<T0> &t0, T1 &t1)						{ t1 = pointer_transfer_dup(a, get(t0), get(t1));}
template<class A, typename T0, typename T1>					void transfer(A &&a, const T0 &t0, dup_pointer<T1>& t1)						{ t1 = pointer_transfer_dup(a, get(t0), get(t1)); }

template<class A, typename T0, typename T1> void transfer(A &&a, const T0 &t0, TL_tuple<T1> &t1) {
	return transfer(a, (TL_tuple<TL_fields_t<T0>>&)t0, t1);
}
template<class A, typename T0, typename T1> void transfer(A &&a, const TL_tuple<T0> &t0, T1 &t1) {
	return transfer(a, t0, (TL_tuple<TL_fields_t<T1>>&)t1);
}
template<class A, typename T0, typename T1> void transfer(A &&a, const TL_tuple<T0> &t0, const T1 &t1) {
	return transfer(a, t0, (TL_tuple<TL_fields_t<T1>>&)t1);
}

template<class A, typename T0, typename T1, size_t... II> void transfer_tuple(A &&a, const TL_tuple<T0> &t0, TL_tuple<T1> &t1, index_list<II...>) {
	bool	dummy[] = {(transfer(a, t0.template get<II>(), t1.template get<II>()), true)...};
}
template<class A> void transfer(A &&a, const TL_tuple<type_list<> > &t0, TL_tuple<type_list<> > &t1)	{}

template<class A, typename T0, typename T1> void transfer(A &&a, const TL_tuple<T0> &t0, TL_tuple<T1> &t1) {
	transfer_tuple(make_allocator_plus(a, t0, t1), t0, t1, meta::TL_make_index_list<T0>());
}
template<class A, typename T0, typename TL0, typename T1, typename TL1> void transfer(A &&a, const as_tuple<T0,TL0> &t0, as_tuple<T1,TL1> &t1) {
	transfer_tuple(make_allocator_plus(a, t0, t1), (const TL_tuple<TL0>&)t0, (TL_tuple<TL1>&)t1, meta::TL_make_index_list<TL0>());
}
template<class A, typename T0, typename TL0, typename T1, typename TL1> void transfer(A &&a, const as_tuple<T0,TL0> &t0, const as_tuple<T1,TL1> &t1) {
	transfer_tuple(make_allocator_plus(a, t0, t1), (const TL_tuple<TL0>&)t0, (TL_tuple<TL1>&)t1, meta::TL_make_index_list<TL0>());
}

template<class A, typename T0, typename T1, typename TL1> void transfer(A &&a, const T0 &t0, as_tuple<T1,TL1> &t1) {
	return transfer(a, (as_tuple<T0>&)t0, t1);
}
template<class A, typename T0, typename TL0, typename T1> void transfer(A &&a, const as_tuple<T0,TL0> &t0, T1 &t1) {
	return transfer(a, t0, (as_tuple<T1>&)t1);
}
template<class A, typename T0, typename TL0, typename T1> void transfer(A &&a, const as_tuple<T0,TL0> &t0, const T1 &t1) {
	return transfer(a, t0, (as_tuple<T1>&)t1);
}

template<class A>	void transfer(A &&a, const const_memory_block &t0, const_memory_block_rel &t1) {
	uint32	size	= t0.size32();
	void	*p		= a.alloc(t0.size32());
	memcpy(p, t0, size);
	t1.init(p, size);
}

//-------------------------------------
// counted
//-------------------------------------

template<typename T1, class A, typename T0> void allocate_n(A &&a, const T0 *t0, int n) {
	T1		*p	= a.template alloc<T1>(n);
	for (int i = 0; i < n; i++) {
		//a.ptr_hash(&t0[i]) = nullptr;
		allocate(a, t0[i], p);
	}
}

template<typename T1, class A, typename T0> T1 *transfer_n(A &&a, const T0 *t0, int n) {
	T1		*p	= a.template alloc<T1>(n);
	for (int i = 0; i < n; i++) {
		//a.ptr_hash(&t0[i]) = &unconst(p)[i];
		transfer(a, t0[i], unconst(p)[i]);
	}
	return p;
}

template<class A, int I, typename T0, typename P0, typename T1, typename P1> void allocate(A &&a, const counted<T0,I,P0> &t0, counted<T1,I,P1>*) {
	if (t0.begin())
		allocate_n<T1>(a, t0.begin(), a.src.template get<I>());
}
template<class A, int I, typename T0, typename P0, typename T1, typename P1> void transfer(A &&a, const counted<T0,I,P0> &t0, counted<T1,I,P1> &t1) {
	t1 = t0.begin() ? transfer_n<T1>(a, t0.begin(), a.src.template get<I>()) : 0;
}

template<template<class> class M, typename T, int I, typename P> struct meta::map<M, counted<T,I,P> > : T_type<counted<map_t<M, T>, I, map_t<M, P>>> {};

template<typename T, int I, typename P = T*> struct counted2 : range<P> {
	template<class A, typename T1, typename P1> friend void allocate(A &&a, const counted<T1,I,P1> &t0, counted2*) {
		if (t0.begin())
			allocate_n<T>(a, t0.begin(), a.src.template get<I>());
	}
	template<class A, typename T1, typename P1> friend void transfer(A &&a, const counted<T1,I,P1> &t0, counted2 &t1) {
		auto	n = a.src.template get<I>();
		if (t1.a = t0.begin() ? transfer_n<T>(a, t0.begin(), n) : 0)
			t1.b = t1.a + n;
	}
};

template<template<class> class M, typename T, int I, typename P> struct meta::map<M, counted2<T,I,P> > : T_type<counted<map_t<M, T>, I, map_t<M, P>>> {};

//-------------------------------------
// union_first
//-------------------------------------

template<class A, typename... T0, typename... T1> void allocate(A &&a, const union_first<T0...> &t0, union_first<T1...> *t1) {
	allocate(make_allocator_plus(a, t0.t), t0.t.t, &t1->t.t);
}
template<class A, typename... T0, typename... T1> void transfer(A &&a, const union_first<T0...> &t0, union_first<T1...> &t1) {
	transfer(make_allocator_plus(a, t0.t), t0.t.t, t1.t.t);
}
template<class A, typename... T0, typename... T1> size_t size(A &&a, const union_first<T0...> &t0, const union_first<T1...> *t1) {
	return size(make_allocator_plus(a, t0.t), t0.t.t, &t1->t.t);
}
template<class A, typename... T0, typename... T1> void pointer_allocate_dup(A &&a, const union_first<T0...> *t0, const union_first<T1...> *t1) {
	if (t0)
		pointer_allocate_dup(make_allocator_plus(a, t0->t), &t0->t.t);
}
template<class A, typename... T0, typename... T1> auto pointer_transfer_dup(A &&a, const union_first<T0...> *t0, const union_first<T1...> *t1) {
	if (t0)
		return pointer_transfer_dup(make_allocator_plus(a, t0->t), &t0->t.t, &t1->t.t);
	return nullptr;
}

//-------------------------------------
// selection
//-------------------------------------

template<class A, typename...T0, typename...T1> constexpr size_t size_selection(A &&a, const union_of<T0...> &t0, const union_of<T1...> *t1, int i)	{
	return i == 0 ? size(a, t0.t, &t1->t) : size_selection(a, t0.u, &t1->u, i - 1);
}
template<class A, typename T0, typename T1> constexpr size_t size_selection(A &&a, const union_of<T0> &t0, const union_of<T1> *t1, int i) {
	return size(a, t0.t, &t1->t);
}

template<class A, typename... T0, typename...T1> void allocate_selection(A &&a, const union_of<T0...> &t0, union_of<T1...> *t1, int i) {
	if (i == 0)
		allocate(a, t0.t, &t1->t);
	else
		allocate_selection(a, t0.u, &t1->u, i - 1);
}
template<class A, typename T0, typename T1> void allocate_selection(A &&a, const union_of<T0> &t0, union_of<T1> *t1, int i) {
	allocate(a, t0.t, &t1->t);
}

template<class A, typename... T0, typename...T1> void transfer_selection(A &&a, const union_of<T0...> &t0, union_of<T1...> &t1, int i) {
	if (i == 0)
		transfer(a, t0.t, t1.t);
	else
		transfer_selection(a, t0.u, t1.u, i - 1);
}
template<class A, typename T0, typename T1> void transfer_selection(A &&a, const union_of<T0> &t0, union_of<T1> &t1, int i) {
	transfer(a, t0.t, t1.t);
}

template<class A, typename...T0, typename...T1> void pointer_allocate_selection(A &&a, const union_of<T0...> *t0,  const union_of<T1...> *t1, int i) {
	if (i == 0)
		pointer_allocate_dup(a, &t0->t, &t1->t);
	else
		pointer_allocate_selection(a, &t0->u, &t1->u, i - 1);
}
template<class A, typename T0, typename T1> void pointer_allocate_selection(A &&a, const union_of<T0> *t0, const union_of<T1> *t1, int i) {
	pointer_allocate_dup(a, &t0->t, &t1->t);
}

template<class A, typename...T0, typename...T1> static void *pointer_transfer_selection(A &&a, const union_of<T0...> *t0,  const union_of<T1...> *t1, int i) {
	if (i == 0)
		return pointer_transfer_dup(a, &t0->t, &t1->t);
	else
		return pointer_transfer_selection(a, &t0->u, &t1->u, i - 1);
}
template<class A, typename T0, typename T1> static void *pointer_transfer_selection(A &&a, const union_of<T0> *t0, const union_of<T1> *t1, int i) {
	pointer_transfer_dup(a, &t0->t, &t1->t);
}

template<class A, int I, typename... T0, typename...T1> void allocate(A &&a, const selection<I, T0...> &t0, selection<I, T1...> *t1) {
	allocate_selection(a, t0.t, &t1->t, a.src.template get<I>());
}
template<class A, int I, typename... T0, typename...T1> void transfer(A &&a, const selection<I, T0...> &t0, selection<I, T1...> &t1) {
	transfer_selection(a, t0.t, t1.t, a.src.template get<I>());
}
template<class A, int I, typename... T0, typename...T1> size_t size(A &&a, const selection<I, T0...> &t0, const selection<I, T1...> *t1) {
	return size_selection(a, t0.t, &t1->t, a.src.template get<I>());
}
template<class A, int I, typename T0, typename... T1> void pointer_allocate_dup(A &&a, const T0 *t0, const selection<I, T1...> *t1) {
	if (t0)
		pointer_allocate_selection(a, &t0->t, &t1->t, a.src.template get<I>());
}
template<class A, int I, typename T0, typename... T1> auto pointer_transfer_dup(A &&a, const T0 *t0, const selection<I, T1...> *t1) {
	return t0 ? (selection<I, T1...>*)pointer_transfer_selection(a, &t0->t, &t1->t, a.src.template get<I>()) : nullptr;
}

//-------------------------------------
// next_array
//-------------------------------------

template<int I, typename T, size_t ALIGN = alignof(T)> struct next_array {
#if 0
	template<class A, typename T1> friend void allocate(A &&a, const next_array &t0, next_array<I,T1> *t1) {
		auto	p0		= make_const(t0.t);
		auto	end		= (const uint8*)p0 + a.src.template get<I>();
		size_t	total	= 0;

		while ((const uint8*)p0 < end) {
			auto	s0	= align(size(a, *p0), ALIGN);
			total	+= s0;
			p0		= (const T*)((const uint8*)p0 + s0);
		}
		a.alloc(total);

		T1	*p1 = nullptr;
		for (p0 = make_const(t0.t); (const uint8*)p0 < end;) {
			allocate(a, *p0, p1);
			auto	s0	= align(size(a, *p0), ALIGN);
			p0 = (const T*)((const uint8*)p0 + s0);
		}
	}
	template<class A, typename T1> friend void transfer(A &&a, const next_array &t0, next_array<I,T1> &t1) {
		auto	p0		= make_const(t0.t);
		auto	end		= (const uint8*)p0 + a.src.template get<I>();
		size_t	total	= 0;

		while ((const uint8*)p0 < end) {
			auto	s0	= align(size(a, *p0), ALIGN);
			total	+= s0;
			p0		= (const T*)((const uint8*)p0 + s0);
		}
		t1.t = (T1*)a.alloc(total);

		T1	*p1 = t1.t;
		for (p0 = make_const(t0.t); (const uint8*)p0 < end;) {
			transfer(a, *p0, *p1);
			auto	s0	= align(size(a, *p0), ALIGN);
			auto	s1	= align(size(a, *p1), ALIGN);
			p0 = (const T*)((const uint8*)p0 + s0);
			p1 = (T1*)((uint8*)p1 + s1);
		}
	}
#endif
	template<class A, typename T1> friend void pointer_allocate_dup(A &&a, const next_array *t0, const next_array<I,T1> *t1) {
		T1*		p1		= nullptr;
		auto	end		= (const uint8*)t0 + a.src.template get<I>();
		size_t	total	= 0;

		for (auto p0 = (const T*)t0; (const uint8*)p0 < end;) {
			auto	s0	= align(size(a, *p0, p0), ALIGN);
			auto	s1	= align(size(a, *p0, p1), ALIGN);
			total	+= s1;
			p0		= (const T*)((const uint8*)p0 + s0);
		}
		a.alloc(total);

		for (auto p0 = (const T*)t0; (const uint8*)p0 < end;) {
			allocate(a, *p0, p1);
			auto	s0	= align(size(a, *p0, p0), ALIGN);
			p0 = (const T*)((const uint8*)p0 + s0);
		}
	}
	template<class A, typename T1> friend auto pointer_transfer_dup(A &&a, const next_array *t0, const next_array<I,T1> *t1) {
		T1*		p1		= nullptr;
		auto	end		= (const uint8*)t0 + a.src.template get<I>();
		size_t	total	= 0;

		for (auto p0 = (const T*)t0; (const uint8*)p0 < end;) {
			auto	s0	= align(size(a, *p0, p0), ALIGN);
			auto	s1	= align(size(a, *p0, p1), ALIGN);
			total	+= s1;
			p0		= (const T*)((const uint8*)p0 + s0);
		}
		auto	result = a.alloc(total);
		a.dst.template get<I>() = total;

		p1 = (T1*)result;
		for (auto p0 = (const T*)t0; (const uint8*)p0 < end;) {
			transfer(a, *p0, *p1);
			auto	s0	= align(size(a, *p0, p0), ALIGN);
			auto	s1	= align(size(a, *p0, p1), ALIGN);
			p0 = (const T*)((const uint8*)p0 + s0);
			p1 = (T1*)((uint8*)p1 + s1);
		}
		return (next_array<I,T1>*)result;
	}
};
template<template<class> class M, int I, typename T> struct meta::map<M, next_array<I, T>> : T_type<next_array<I, map_t<M, T>>> {};

//-----------------------------------------------------------------------------
//	hash
//-----------------------------------------------------------------------------

template<typename K, typename V> struct flat_hash {
	typedef pair<K, V>	E;
	uint32		count;
	rel_ptr<E>	entries;

	template<class A, typename T0> friend void allocate(A &&a, const T0 &t0, flat_hash *t1) {
		a.template alloc<E>(t0.size());
	}
	template<class A, typename T0> friend void transfer(A &&a, const T0 &t0, flat_hash &t1) {
		t1.count	= t0.size();
		t1.entries	= a.template alloc<E>(t1.count);

		E*		p	= t1.entries;
		for (auto i : with_iterator(t0)) {
			p->a	= i.hash();
			p->b	= *i;
			++p;
		}
	}

	operator hash_map0<K, V, false, 0>() const {
		hash_map0<K, V, false, 0>	map;
		for (auto& i : make_range_n(entries.get(), count))
			map.put(i.a, i.b);
		return map;
	}
	template<typename K1> operator hash_map<K1, V>() const {
		return force_cast<hash_map<K1, V>>(operator hash_map0<K, V, false, 0>());
	}

	template<class A, typename T1> friend void transfer(A &&a, const flat_hash &t0, T1 &t1) {
		construct(t1, t0);
	}

};

//-----------------------------------------------------------------------------
//	flatten_struct
//-----------------------------------------------------------------------------

template<template<class> class M, typename T> malloc_block map_struct(const T &t) {
	typedef	map_t<M, T>	T1;
	offset_allocator	a;

	allocate(a, t, a.alloc<T1>());
	
	malloc_block	m(a.size());
	a.init(m);

	T1	*t1	= a.alloc<T1>();
	transfer(a, t, *t1);

	ISO_ASSERT(a.size() == m.length());
	return m;
}

template<template<class> class M, typename T> unique_ptr<map_t<M, T>> map_unique(const T &t) {
	return map_struct<M>(t);
}

template<template<class> class M, typename T> malloc_block rmap_struct(const void *p) {
	typedef	map_t<M, T>	T1;
	offset_allocator	a;

	T1	&t1 = *(T1*)p;
	allocate(a, t1, a.alloc<T>());

	malloc_block	m(a.size());
	a.init(m);

	T	*t	= a.alloc<T>();
	transfer(a, t1, *t);

	ISO_ASSERT(a.size() == m.length());
	return m;
}

template<template<class> class M, typename T> unique_ptr<T> rmap_unique(const void *p) {
	return rmap_struct<M, T>(p);
}

template<template<class> class M, typename... P> malloc_block save_params(const P&... p) {
	return map_struct<M>(tuple<P...>(p...));
}

template<template<class> class M, typename F> void get_params(const void *p, F f) {
	call(f, *(TL_tuple<map_t<M, typename function<F>::P>>*)p);
}

//-----------------------------------------------------------------------------
// RPC mechanisms
//-----------------------------------------------------------------------------

typedef with_size<malloc_block> malloc_block_all;

// RemoteDll-based

template<typename T>	void DLL_result(void *p, const T &r)					 { *((T*)p) = r; }
template<typename T>	void DLL_result(void *p, const dynamic_array<T> &r)		{ *(flat_array<T>*)p = r; }
inline					void DLL_result(void *p, const const_memory_block &r)	{ r.copy_to(p); }

template<typename F> enable_if_t<!same_v<typename function<F>::R, void>, DWORD> _DLL_RPC(void *p, F f) {
	DLL_result(p, call(f, *(TL_tuple<typename function<F>::P>*)p));
	return 0;
}

template<typename F> enable_if_t<same_v<typename function<F>::R, void>, DWORD> _DLL_RPC(void *p, F f) {
	call(f, *(TL_tuple<typename function<F>::P>*)p);
	return 0;
}

#define DLL_RPC(f)	extern "C" __declspec(dllexport) DWORD RPC_##f(void *p) { return _DLL_RPC(p, f); }

// Socket based

template<typename F> enable_if_t<!same_v<typename function<F>::R, void>> SocketRPC(SocketWait &sock, F f) {
	TL_tuple<typename function<F>::P>	params;
	params.read(sock);
	sock.write(call(f, params));

}

template<typename F> enable_if_t<same_v<typename function<F>::R, void>> SocketRPC(Socket &sock, F f) {
	TL_tuple<typename function<F>::P>	params;
	params.read(sock);
	call(f, params);
}

template<typename R, typename...P> enable_if_t<!same_v<R, void>, R> SocketCallRPC(const SocketWait &sock, int func, const P&...p) {
	sock.putc(func);
	tuple<P...>(p...).write(sock);
	sock.select(1);	// wait up to 1 sec
	return sock.get<R>();
}

template<typename R, typename...P> enable_if_t<same_v<R, void>> SocketCallRPC(const Socket &sock, int func, const P&...p) {
	sock.putc(func);
	tuple<P...>(p...).write(sock);
}


} // namespace iso

#endif // MARSHAL_H
