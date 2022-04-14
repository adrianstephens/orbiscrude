#ifndef HOOK_COM_H
#define HOOK_COM_H

#include "com.h"
#include "base\hash.h"
#include "base\functions.h"
#include "base\algorithm.h"
#include "base\pointer.h"
#include "allocators\allocator.h"
#include "utilities.h"
#include "hook.h"
#include "comms/ip.h"
#include <comdef.h>

namespace iso {

class offset_allocator : public allocator<offset_allocator> {
protected:
	void	*p;
	size_t	offset;
public:
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
	size_t	size()	const	{ return offset; }
};

//collapses
template<typename A, typename S, typename D = void> struct allocator_plus : public allocator<allocator_plus<A, S, D>> {
	A			&a;
	const S		&src;
	D			&dst;
	allocator_plus(A &&a, const S &s, D &d) : a(a), src(s), dst(d) {}

	void	*_alloc(size_t size)				const { return a._alloc(size); }
	void	*_alloc(size_t size, size_t align)	const { return a._alloc(size, align); }
	A&		operator->()	{ return a; }
};

template<typename A, typename S> struct allocator_plus<A, S, void> : public allocator<allocator_plus<A, S, void>> {
	A			&a;
	const S		&src;
	allocator_plus(A &&a, const S &s) : a(a), src(s) {}

	void	*_alloc(size_t size)				const { return a._alloc(size); }
	void	*_alloc(size_t size, size_t align)	const { return a._alloc(size, align); }
	A&		operator->()	{ return a; }
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

//-------------------------------------
// counted
//-------------------------------------

template<typename T1, class A, typename T0> void allocate_n(A &&a, const T0 *t0, int n) {
	T1		*p	= a.template alloc<T1>(n);
	for (int i = 0; i < n; i++)
		allocate(a, t0[i], p);
}

template<typename T1, class A, typename T0> T1 *transfer_n(A &&a, const T0 *t0, int n) {
	T1		*p	= a.template alloc<T1>(n);
	for (int i = 0; i < n; i++)
		transfer(a, t0[i], unconst(p)[i]);
	return p;
}

template<typename T, int I, typename P = T*> struct counted {
	P		p;
	counted()					{}
	counted(T *_p)				{ p = _p; }
	template<typename P2> counted(const counted<T,I,P2> &_p) { p = _p; }
	void	operator=(T *_p)	{ p = _p; }
	operator T*()	const		{ return p; }
	T*		begin()	const		{ return p; }

	template<class A, typename T1, typename P1> friend void allocate(A &&a, const counted &t0, counted<T1,I,P1>*) {
		if (t0.begin())
			allocate_n<T1>(a, t0.begin(), a.src.template get<I>());
	}
	template<class A, typename T1, typename P1> friend void transfer(A &&a, const counted &t0, counted<T1,I,P1> &t1) {
		t1 = t0.begin() ? transfer_n<T1>(a, t0.begin(), a.src.template get<I>()) : 0;
	}
};
template<int I, typename T> counted<T, I, T*> make_counted(T *p) { return p; }

template<template<class> class M, typename T, int I, typename P> struct meta::map<M, counted<T,I,P> > : T_type<counted<map_t<M, T>, I, map_t<M, P>>> {};

//-------------------------------------
// union_first
//-------------------------------------

template<typename... T> struct union_first {
	union_of<T...>	t;

	template<class A, typename... T1> friend void allocate(A &&a, const union_first &t0, union_first<T1...> *t1) {
		allocate(make_allocator_plus(a, t0.t), t0.t.t, &t1->t.t);
	}
	template<class A, typename... T1> friend void transfer(A &&a, const union_first &t0, union_first<T1...> &t1) {
		transfer(make_allocator_plus(a, t0.t), t0.t.t, t1.t.t);
	}
	template<class A, typename... T1> friend size_t size(A &&a, const union_first &t0, const union_first<T1...> *t1) {
		return size(make_allocator_plus(a, t0.t), t0.t.t, &t1->t.t);
	}
	template<class A, typename... T1> friend void pointer_allocate_dup(A &&a, const union_first *t0, const union_first<T1...> *t1) {
		if (t0)
			pointer_allocate_dup(make_allocator_plus(a, t0->t), &t0->t.t);
	}
	template<class A, typename... T1> auto pointer_transfer_dup(A &&a, const union_first *t0, const union_first<T1...> *t1) {
		if (t0)
			return pointer_transfer_dup(make_allocator_plus(a, t0->t), &t0->t.t, &t1->t.t);
		return nullptr;
	}
};
template<template<class> class M, typename...T> struct meta::map<M, union_first<T...>> : T_type<union_first<map_t<M, T>...>> {};

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

template<int I, typename... TT> struct selection {
	union_of<TT...>	t;

	template<class A, typename...T1> friend void allocate(A &&a, const selection &t0, selection<I, T1...> *t1) {
		allocate_selection(a, t0.t, &t1->t, a.src.template get<I>());
	}
	template<class A, typename...T1> friend void transfer(A &&a, const selection &t0, selection<I, T1...> &t1) {
		transfer_selection(a, t0.t, t1.t, a.src.template get<I>());
	}
	template<class A, typename...T1> friend size_t size(A &&a, const selection &t0, const selection<I, T1...> *t1) {
		return size_selection(a, t0.t, &t1->t, a.src.template get<I>());
	}
	template<class A, typename T0> friend void pointer_allocate_dup(A &&a, const T0 *t0, const selection *t1) {
		if (t0)
			pointer_allocate_selection(a, &t0->t, &t1->t, a.src.template get<I>());
	}
	template<class A, typename T0> friend auto pointer_transfer_dup(A &&a, const T0 *t0, const selection *t1) {
		return t0 ? (selection*)pointer_transfer_selection(a, &t0->t, &t1->t, a.src.template get<I>()) : nullptr;
	}

};
template<template<class> class M, int I, typename...T> struct meta::map<M, selection<I, T...>> : T_type<selection<I, map_t<M, T>...>> {};

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
//	Recording
//-----------------------------------------------------------------------------

template<typename T>		using rel_ptr		= soft_pointer<T, base_relative<uint32> >; 
template<typename T, int I> using rel_counted	= counted<T, I, rel_ptr<T> >; 

template<typename T> struct dup_pointer {
	T	t;
};

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
//	FM - fix for call to original functions
//-----------------------------------------------------------------------------

template<typename T>		struct FM						: T_type<T> {};
template<typename T>		struct FM<T&>					: T_type<T> {};
template<typename T>		struct FM<T*>					: T_if<is_com<T>::value, unwrapped<T*>, typename FM<T>::type*> {};
//template<typename T>		struct FM<T**>					: T_type<T**> {};
//template<typename T>		struct FM<T* const*>			: T_type<const typename FM<T*>::type*> {};
template<typename T, int I> struct FM<counted<T, I, T*> >	: T_type<counted<map_t<iso::FM, T>, I>> {};

template<typename T>		struct meta::map<FM, T**>		: T_type<T**> {};

//-----------------------------------------------------------------------------
//	RTM - convert to recorded format
//-----------------------------------------------------------------------------

template<typename T>		struct RTM						: T_type<T> {};
template<typename T>		struct RTM<T&>					: meta::map<RTM, T> {};
template<typename T>		struct RTM<T*>					: T_if<is_com<T>::value, T*,		rel_ptr<typename RTM<T>::type>>			{};
template<typename T>		struct RTM<const T*>			: T_if<is_com<T>::value, const T*,	rel_ptr<const typename RTM<T>::type>>	{};
template<>					struct RTM<void*>				: T_type<void*> {};
template<>					struct RTM<const void*>			: T_type<const void*> {};
template<typename T, int I> struct RTM<counted<T, I, T*> >	: T_type<rel_counted<map_t<iso::RTM, T>, I>> {};
template<>					struct RTM<const_memory_block>	: T_type<const_memory_block_rel> {};

//-----------------------------------------------------------------------------
//	KM - as RTM, but dup strings
//-----------------------------------------------------------------------------

template<typename T>		struct KM						: T_type<T> {};
template<typename T>		struct KM<T&>					: T_type<T> {};
template<typename T>		struct KM<T*>					: T_if<is_com<T>::value, T*,		rel_ptr<typename KM<T>::type> >			{};
template<typename T>		struct KM<const T*>				: T_if<is_com<T>::value, const T*,	rel_ptr<const typename KM<T>::type> >	{};
template<>					struct KM<void*>				: T_type<void*> {};
template<>					struct KM<const void*>			: T_type<const void*> {};
template<>					struct KM<const char*>			: T_type<dup_pointer<rel_ptr<const char> >>	{};
template<typename T, int I> struct KM<counted<T, I, T*> >	: T_type<rel_counted<map_t<iso::KM, T>, I>>	{};
template<>					struct KM<const_memory_block>	: T_type<const_memory_block_rel>			{};

//-----------------------------------------------------------------------------
//	PM - convert to playback format
//-----------------------------------------------------------------------------

template<typename T>		struct PM						: T_type<T> {};
template<typename T>		struct PM<T&>					: T_type<T> {};
template<typename T>		struct PM<T*>					: T_if<is_com<T>::value, lookup<T*>,		rel_ptr<typename PM<T>::type> > {};
template<typename T>		struct PM<const T*>				: T_if<is_com<T>::value, lookup<const T*>,	rel_ptr<const typename PM<T>::type> > {};
template<>					struct PM<void*>				: T_type<lookup<handle>> {};
template<>					struct PM<const void*>			: T_type<const void*> {};
template<typename T, int I> struct PM<counted<T, I, T*> >	: T_type<rel_counted<map_t<iso::PM, T>, I>> {};
template<typename T>		struct PM<lookedup<T> >			: T_type<lookup<T>> {};
template<>					struct meta::map<PM, void**>	: T_type<rel_ptr<void*>> {};

//-----------------------------------------------------------------------------
//	allocate
//-----------------------------------------------------------------------------

template<class A, typename T0, typename T1>	size_t size(A &&a, const T0 &t0, const T1 *t1)		{ return sizeof(T1); }
template<class A, typename T0, typename T1>	void allocate(A &&a, const T0 &t0, T1 *t1)			{}
template<class A, typename T0, typename T1>	void allocate(A &&a, const T0 &t0, const T1 *t1)	{ allocate(a, t0, const_cast<T1*>(t1)); }

//-------------------------------------
//	pointers
//-------------------------------------

template<class A, typename T0, typename T1> void pointer_allocate_dup(A &&a, const T0 *t0, const T1 *t1) {
	if (t0)
		iso::allocate(a, *t0, unconst(a.template alloc<T1>()));
}
template<class A> void pointer_allocate_dup(A &&a, const char *t0, const char *t1) {
	a.template alloc<char>(string_len(t0) + 1);
}

template<class A, typename T0, typename T1> void pointer_allocate(A &&a, const T0* t0, const T1* t1) {
	pointer_allocate_dup(a, t0, t1);
}

template<class A, typename T> void pointer_allocate(A &&a, const T* t0, const T* t1) {}	//keep unchanged pointers (to point into original data)

template<class A, typename T0, typename T1>					void allocate(A &&a, T0 *const t0, T1**)								{ pointer_allocate(a, (const T0*)t0, (const T1*)0); }
template<class A, typename T0, typename B0, typename T1>	void allocate(A &&a, const soft_pointer<T0,B0> &t0, T1**)				{ pointer_allocate(a, (const T0*)t0, (const T1*)0); }
template<class A, typename T0, typename T1, typename B1>	void allocate(A &&a, T0 *const t0, soft_pointer<T1,B1>*)				{ pointer_allocate_dup(a, (const T0*)t0, (const T1*)0); }
template<class A, typename T0, typename T1>					void allocate(A &&a, const dup_pointer<T0*> &t0, T1**)					{ pointer_allocate_dup(a, (const T0*)t0.t, (const T1*)0); }
template<class A, typename T0, typename B0, typename T1>	void allocate(A &&a, const dup_pointer<soft_pointer<T0,B0> > &t0, T1**)	{ pointer_allocate_dup(a, (const T0*)t0.t, (const T1*)0); }


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

//-------------------------------------
//	pointers
//-------------------------------------

template<class A, typename T0, typename T1> T1 *pointer_transfer_dup(A &&a, const T0 *t0, const T1 *t1) {
	if (t0) {
		T1	*p	= a.template alloc<T1>();
		iso::transfer(a, *t0, *unconst(p));
		return p;
	}
	return 0;
}

template<class A> const char *pointer_transfer_dup(A &&a, const char *t0, const char *t1) {
	if (size_t len = string_len(t0)) {
		char	*p	= a.template alloc<char>(len + 1);
		memcpy(p, t0, len + 1);
		return p;
	}
	return nullptr;
}

template<class A, typename T0, typename T1> auto pointer_transfer(A &&a, const T0* t0, const T1* t1) {
	return pointer_transfer_dup(a, t0, t1);
}

template<class A, typename T> auto pointer_transfer(A &&a, const T* t0, const T* t1) { return unconst(t0); }	//keep unchanged pointers (to point into original data)

template<class A, typename T0, typename T1>					void transfer(A &&a, T0 *const t0, T1 *&t1)									{ t1 = pointer_transfer(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename B0, typename T1>	void transfer(A &&a, const soft_pointer<T0,B0> &t0, T1 *&t1)				{ t1 = pointer_transfer(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename T1, typename B1>	void transfer(A &&a, T0 *const t0, soft_pointer<T1,B1> &t1)					{ t1 = pointer_transfer_dup(a, (const T0*)t0, (const T1*)0);}
template<class A, typename T0, typename T1>					void transfer(A &&a, const dup_pointer<T0*> &t0, T1 *&t1)					{ t1 = pointer_transfer_dup(a, (const T0*)t0.t, (const T1*)0);}
template<class A, typename T0, typename B0, typename T1>	void transfer(A &&a, const dup_pointer<soft_pointer<T0,B0> > &t0, T1 *&t1)	{ t1 = pointer_transfer_dup(a, (const T0*)t0.t, (const T1*)0);}

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
	T1&					t1 = *(T1*)p;

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
//	COMParse
//-----------------------------------------------------------------------------

template<typename F> static typename function<F>::R COMParse(const void *p, F f) {
	return call<0>(f, *(TL_tuple<map_t<RTM, typename function<F>::P>>*)p);
}

template<typename F> static typename function<F>::R COMParse2(const void *p, F f) {

	typedef TL_tuple<typename function<F>::P>	T0;
	typedef	map_t<RTM, T0>		T1;

	T1&					t1 = *(T1*)p;
	
	offset_allocator	a;
	allocate(a, t1, a.alloc<T0>());

	size_t	size = a.size();
	a.init(alloca(size));
	T0	*t0	= a.alloc<T0>();
	transfer(a, t1, *t0);
	ISO_ASSERT(a.size() == size);

	return call<0>(f, *t0);
}

//-----------------------------------------------------------------------------
//	COMReplay
//-----------------------------------------------------------------------------

template<typename...T>	struct T_noref<type_list<T...> >	{ typedef type_list<typename T_noref<T>::type...> type; };

void throw_hresult(HRESULT hr);

template<typename R>	struct COMcall {
	template<typename T, typename F, typename P> static void call(T &t, F f, const P &p) { iso::call(t, f, p); }
};
template<>	struct COMcall<HRESULT> {
	template<typename T, typename F, typename P> static void call(T &t, F f, const P &p) {
		HRESULT	hr = iso::call(t, f, p);
		if (!SUCCEEDED(hr))
			throw_hresult(hr);
	}
};

template<typename B> struct COMReplay {
	bool						abort;

	template<typename T> unique_ptr<T> Remap(const void *p) {
		typedef	TL_fields_t<T>		RTL0;
		typedef map_t<PM, RTL0>		RTL1;

		abort = false;
		lookup_allocator<offset_allocator, B>	a(*static_cast<B*>(this));
		TL_tuple<RTL1>	*t0 = (TL_tuple<RTL1>*)p;
		allocate(a, *t0, a.template alloc<TL_tuple<RTL0> >());

		size_t	size = a.size();
		malloc_block	m(a.size());
		a.init(m);
		TL_tuple<RTL0>	*t1 = a.template alloc<TL_tuple<RTL0> >();
		transfer(a, *t0, *t1);

		ISO_ASSERT(a.size() == size);
//		if (abort)
//			return none;
		return (T*)m.detach();
	}

	template<typename F1, typename C, typename F2, typename C2> void Replay2(C2 *obj, const void *p, F2 C::*f) {
		typedef	typename function<F1>::P	P;
		typedef	typename T_noref<P>::type	RTL0;
		typedef map_t<PM, P>	RTL1;

		abort = false;
		lookup_allocator<offset_allocator, B>	a(*static_cast<B*>(this));
		TL_tuple<RTL1>	*t0 = (TL_tuple<RTL1>*)p;
		allocate(a, *t0, a.template alloc<TL_tuple<RTL0> >());

		size_t	size = a.size();
		a.init(alloca(size));
		TL_tuple<RTL0>	*t1 = a.template alloc<TL_tuple<RTL0> >();
		transfer(a, *t0, *t1);

		ISO_ASSERT(a.size() == size);
		if (!abort)
			COMcall<typename function<F2>::R>::call(*static_cast<C*>(obj), f, *t1);
	}
	template<typename F1, typename C, typename F2, typename C2> void Replay2(const com_ptr<C2> &obj, const void *p, F2 C::*f) {
		Replay2<F1>(obj.get(), p, f);
	}
	template<typename C, typename F, typename C2> void Replay(C2 *obj, const void *p, F C::*f) {
		Replay2<F>(obj, p, f);
	}
	template<typename C, typename F, typename C2> void Replay(const com_ptr<C2> &obj, const void *p, F C::*f) {
		Replay2<F>(obj.get(), p, f);
	}

	template<typename F> void Replay(const void *p, F f) {
		abort = false;
		typedef	typename function<F>::P		P;
		typedef	typename T_noref<P>::type	RTL0;
		typedef map_t<PM, P>	RTL1;

		lookup_allocator<offset_allocator, B>	a(*static_cast<B*>(this));
		auto	*t0		= (TL_tuple<RTL1>*)p;
		allocate(a, *t0, a.template alloc<TL_tuple<RTL0> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.template alloc<TL_tuple<RTL0> >();
		transfer(a, *t0, *t1);

		ISO_ASSERT(a.size() == size);
		if (!abort)
			call(f, *t1);
	}

};

//-----------------------------------------------------------------------------
//	COMRecording
//-----------------------------------------------------------------------------

template<typename T> T *mem_find(T *a, T *b, T v) {
	while (a < b) {
		if (*a == v)
			return a;
		++a;
	}
	return 0;
}

struct memory_cube {
	void	*data;
	uint32	width, rows, depth;
	uint32	row_pitch, depth_pitch;

	memory_cube(void *data, uint32 width, uint32 rows, uint32 depth, uint32 row_pitch, uint32 depth_pitch) :
		data(data), width(width), rows(rows), depth(depth), row_pitch(row_pitch), depth_pitch(depth_pitch) {}

	size_t	size()			const { return width * rows * depth; }
	size_t	strided_size()	const { return (depth - 1) * depth_pitch + (rows - 1) * row_pitch + width; }

	void	copy_to(void *dest)	const {
		const uint8	*s0	= (const uint8*)data;
		uint8		*d	= (uint8*)dest;
		for (int z = 0; z < depth; z++) {
			const uint8	*s	= s0;
			for (int y = 0; y < rows; y++) {
				memcpy(d, s, width);
				d += width;
				s += row_pitch;
			}
			s0 += depth_pitch;
		}
	}
	size_t	copy_to(void *dest, uint32 skip_val) const {
		bool	prev_skip	= true;
		uint32	*d			= (uint32*)dest;
		uint32	*token		= 0;
		uint32	run			= 0;

		for (int z = 0; z < depth; z++) {
			uint32	offset = z * depth_pitch;

			for (int y = 0; y < rows; y++) {
				for (uint32	*s = (uint32*)((char*)data + offset), *e = (uint32*)((char*)data + offset + width); s < e; ++s) {
					bool skip = *s == skip_val;

					if (skip != prev_skip) {
						if (prev_skip) {
							while (run > 0xffff) {
								*d++ = 0xffff;
								run -= 0xffff;
							}
							token	= d++;
							*token	= run;
						} else {
							*token	|= run << 16;
						}
						prev_skip	= skip;
						run			= 0;
					}

					++run;

					if (!skip) {
						*d++ = *s;
						if (run == 0xffff) {
							*token		|= 0xffff0000;
							token		= d++;
							*token		= 0;
							run			= 0;
						}
					}
				}
				offset += row_pitch;
			}
		}
		if (!prev_skip)
			*token	|= run << 16;

		return (uint8*)d - (uint8*)dest;
	}

	void	strided_copy_to(void *dest)	const {
		memcpy(dest, data, strided_size());
	}
	size_t	strided_copy_to(void *dest, uint32 skip_val) const {
		size_t	total		= 0;
		bool	prev_skip	= true;
		uint32	run			= 0;

		for (int z = 0; z < depth; z++) {
			uint32	offset = z * depth_pitch;

			for (int y = 0; y < rows; y++) {
				uint32	*d = (uint32*)((char*)dest + offset);

				for (uint32	*s = (uint32*)((char*)data + offset), *e = (uint32*)((char*)data + offset + width); s < e; ++s, ++d) {
					bool skip	= *s == skip_val;
					
					if (skip != prev_skip) {
						total		+= int(prev_skip) + (run ? (run - 1) / 0xffff : 0);
						prev_skip	= skip;
						run			= 0;
					}

					if (!skip) {
						*d = *s;
						++total;
					}
					++run;
				}

				offset += row_pitch;
			}
		}

		return total * sizeof(uint32);
	}

	void	copy_from(const void *srce) {
		uint8			*d0	= (uint8*)data;
		const uint8		*s	= (const uint8*)srce;
		for (int z = 0; z < depth; z++) {
			uint8	*d	= d0;
			for (int y = 0; y < rows; y++) {
				memcpy(d, s, width);
				s += width;
				d += row_pitch;
			}
			d0 += depth_pitch;
		}
	}

	void	copy_from_rle(const const_memory_block &rle) {
		uint32	offset	= 0;
		for (const uint32 *p = rle; p != rle.end();) {
			uint32	token	= *p++;
			uint32	len		= token >> 16;

			offset += (token & 0xffff) * 4;

			uint8	*s		= (uint8*)p;
			uint32	x		= offset % width;
			uint32	y		= (offset / width) % rows;
			uint32	z		= offset / (width * rows);

			p		+= len;
			len		*= 4;
			offset	+= len;

			while (len) {
				uint32	len1	= min(width - x, len);
				uint8	*dest	= (uint8*)data + z * depth_pitch + y * row_pitch + x;
				memcpy(dest, s, len1);

				s	+= len1;
				len	-= len1;
				x	= 0;
				if (++y == rows) {
					y = 0;
					++z;
				}
			}

			ISO_ASSERT((uint8*)p == s);
		}
	}

};


template<typename F, typename W, typename... P> typename function<F>::R COMRun(W *wrap, F f, P... p) {
	return (wrap->orig->*f)(p...);
}

template<typename F, typename W, typename... P> typename function<F>::R COMRun2(W *wrap, F f, P... p) {
	typedef type_list<P...>					TL0;
	typedef map_t<FM, TL0>	TL1;

	offset_allocator	a;
	auto		t0	= TL_tuple<TL0>(p...);
	allocate(a, t0, a.alloc<TL_tuple<TL1> >());

	size_t	size	= a.size();
	a.init(alloca(size));
	TL_tuple<TL1>	*t1 = a.alloc<TL_tuple<TL1> >();
	transfer(a, t0, *t1);
	ISO_ASSERT(a.size() == size);

	return call(*wrap->orig, f, *(TL_tuple<TL0>*)t1);
}

struct COMRecording {
	malloc_block	recording;
	size_t			total;
	bool			enable;

	class header {
		uint16			size;
	public:
		uint16			id;
		header(uint16 _id, size_t _size) : size((uint16)_size), id(_id) {
			if (_size >= 0x10000) {
				size |= 1;
				(&id)[1] = uint16(_size >> 16);
			}
		}
		void				*data()			{ return &id + 1 + (size & 1); }
		const void			*data()	const	{ return &id + 1 + (size & 1); }
		const_memory_block	block()	const	{ return const_memory_block(data(), next()); }
		const header		*next()	const	{ return (header*)((uint8*)this + (size & 1 ? ((&id)[1] << 16) | (size - 1) : size)); }
	};

	memory_block	get_buffer() const {
		return memory_block(recording, total);
	}
	memory_block	get_buffer_reset() {
		size_t t = total;
		total = 0;
		return memory_block(recording.p, t);
	}

	void*	get_space(uint16 id, size_t size) {
		size += sizeof(header);
		if (size >= 0x10000)
			size += 2;

		if (total + size > recording.length())
			recording.resize(max(recording.length() * 2, total + size * 2));

		header	*p	= new((uint8*)recording + total) header(id, size);
		total		+= size;
		return p->data();
	}
	void add(uint16 id, size_t size, const void *p)			{ memcpy(get_space(id, size), p, size); }
	template<typename T> void add(uint16 id, const T &t)	{ add(id, sizeof(T), &t); }

	COMRecording() : total(0), enable(false) {}
	
	void Reset() {
		total = 0;
	}

	void Record(uint16 id) {
		get_space(id, 0);
	}

	void Record(uint16 id, const const_memory_block &t) {
		t.copy_to(get_space(id, t.length()));
	}

	void Record(uint16 id, const memory_cube &t) {
		t.copy_to(get_space(id, t.size()));
	}

	// record single item (usually a tuple)
	template<typename T0> void Record(uint16 id, const T0 &t0) {
		typedef map_t<RTM, T0>	T1;

		offset_allocator	a;
		allocate(a, t0, a.alloc<T1>());

		size_t	size	= a.size();
		a.init(get_space(id, align(size, 4)));
		transfer(a, t0, *a.alloc<T1>());
		ISO_ASSERT(a.size() == size);
	}

	// record parameters
	template<typename... P> void Record(uint16 id, const P&... p) {
		Record(id, tuple<P...>(p...));
	}

	// helpers for passing return value back
	template<typename R> struct RunRecord_s {
		template<typename F, typename W, typename... P> static R f(COMRecording *rec, W *wrap, F f, int id, P... p) {
			R	r = (wrap->orig->*f)(p...);
			if (rec)
				rec->Record(uint16(id), p...);
			return r;
		}
		template<typename F, typename W, typename T0, typename T1> static R f2(COMRecording *rec, W *wrap, F f, int id, T0 &t0, T1 &t1) {
			R	r = call(*wrap->orig, f, t1);
			if (rec)
				rec->Record(uint16(id), t0);
			return r;
		}
		template<typename F, typename W, typename T, typename T0, typename T1, typename...P> static R fwrap(COMRecording *rec, W *wrap, F f, int id, T **pp, T0 &t0, T1 &t1, P... p) {
			R	r = com_wrap_system->make_wrap(call(*wrap->orig, f, t1), pp, wrap, p...);
			if (rec)
				rec->Record(uint16(id), t0);
			return r;
		}
	};

	template<> struct RunRecord_s<void> {
		template<typename F, typename W, typename... P> static void f(COMRecording *rec, W *wrap, F f, int id, P... p) {
			(wrap->orig->*f)(p...);
			if (rec)
				rec->Record(uint16(id), p...);
		}
		template<typename F, typename W, typename T0, typename T1> static void f2(COMRecording *rec, W *wrap, F f, int id, T0 &t0, T1 &t1) {
			call(*wrap->orig, f, t1);
			if (rec)
				rec->Record(uint16(id), t0);
		}
		template<typename F, typename W, typename T, typename T0, typename T1, typename...P> static void fwrap(COMRecording *rec, W *wrap, F f, int id, T **pp, T0 &t0, T1 &t1, P... p) {
			call(*wrap->orig, f, t1);
			if (pp)
				*pp = com_wrap_system->make_wrap(*pp, wrap, p...);
			if (rec)
				rec->Record(uint16(id), t0);
		}
	};

	// run function and record parameters
	template<typename F, typename W, typename... P> typename function<F>::R RunRecord(W *wrap, F f, int id, P... p) {
		return RunRecord_s<typename function<F>::R>::f(enable ? this : nullptr, wrap, f, id, p...);
	}

	// fix parameters and run function
	template<typename F, typename W, typename... P> typename function<F>::R Run2(W *wrap, F f, P... p) {
		typedef type_list<P...>						TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p...);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return call(*wrap->orig, f, *(TL_tuple<TL0>*)t1);
	}

	// fix parameters, run function, and record
	template<typename F, typename W, typename... P> typename function<F>::R RunRecord2(W *wrap, F f, int id, P... p) {
		typedef type_list<P...>						TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p...);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return RunRecord_s<typename function<F>::R>::f2(enable ? this : nullptr, wrap, f, id, t0, *(TL_tuple<TL0>*)t1);
	}

	// fix parameters, run function, record, and wrap result using IID
	template<typename F, typename W, typename T, typename... P> typename function<F>::R RunRecord2Wrap(W *wrap, F f, int id, REFIID riid, T **pp, P... p) {
		typedef type_list<P..., IID, void**>		TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p..., riid, (void**)pp);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return RunRecord_s<typename function<F>::R>::fwrap(enable ? this : nullptr, wrap, f, id, pp, t0, *(TL_tuple<TL0>*)t1, p...);
	}

	// fix parameters, run function, record, and wrap result
	template<typename F, typename W, typename T, typename... P> typename function<F>::R RunRecord2Wrap(W *wrap, F f, int id, T **pp, P... p) {
		typedef type_list<P..., T**>				TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p..., pp);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return RunRecord_s<typename function<F>::R>::fwrap(enable ? this : nullptr, wrap, f, id, pp, t0, *(TL_tuple<TL0>*)t1, p...);
	}

	COMRecording& WithObject(const void *p, uint16 id = -1) {
		if (enable)
			*(const void**)get_space(id, sizeof(p)) = p;
		return *this;
	}
};

//-----------------------------------------------------------------------------
//	Wrapping
//-----------------------------------------------------------------------------

struct ReWrapper : static_list<ReWrapper>, virtfunc<void*(REFIID,void*)> {
	template<typename T> ReWrapper(const T *t) : virtfunc<void*(REFIID,void*)>(t) {}
};
template<typename T> struct ReWrapperT : ReWrapper {
	void	*operator()(REFIID riid, void *p) const {
		if (riid == __uuidof(T))
			return com_wrap_system->find_wrap_carefully((T*)p);
		return 0;
	}
	ReWrapperT() : ReWrapper(this) {}
};

template<class T> class Wrap;
template<class T, class B> class Wrap2;

template<class I> inline void block_shuffle_down(I a, I b) {
	--b;
	while (a != b) {
		I	a0	= a;
		swap(*a0, *++a);
	}
}

template<class I> inline void block_shuffle_up(I a, I b) {
	++a;
	while (a != b) {
		I	b0	= b;
		swap(*b0, *--b);
	}
}

//override this when necessary
template<typename T> struct	Wrappable { typedef T type; };

template<typename T> static typename Wrappable<T>::type	*GetWrappable(T *p)	{
	return static_cast<typename Wrappable<T>::type*>(p);
}

//---------------------------------
// _com_wrap
//---------------------------------
struct _com_wrap {
//	LONG	refs	= 1;
	_com_wrap();
	virtual ~_com_wrap();
};

template<typename T> Wrap<T>* make_wrap_orphan(T* p) {
	ISO_ASSERT(p);
	return com_wrap_system->make_wrap(p);
}

struct _com_wrap_system {
	hash_map<void*,_com_wrap*, true>	to_wrap;
	hash_set<_com_wrap*, true>			is_wrap;
	dynamic_array<deferred_deletes>		dd;

	_com_wrap_system() : to_wrap(1024), is_wrap(1024)	{}
	~_com_wrap_system() {
		//for (auto &i : dd)
		//	i.reset();
	}

	void	defer_deletes(int num_frames)	{
		dd.resize(num_frames);
	}
	void	end_frame()	{
		if (!dd.empty()) {
			dd.back().process();
			block_shuffle_up(dd.begin(), dd.end());
		}
	}

	void	remove(_com_wrap *p, void *orig) {
		ISO_VERIFY(is_wrap.remove(p));
		ISO_VERIFY(to_wrap.remove(orig));
		if (!dd.empty())
			dd.front().push_back(p);
		else
			delete p;
	}
	bool	add(_com_wrap *p, void *orig) {
		if (dd.empty())
			return false;
		ISO_VERIFY(dd.front().undelete(p));
		ISO_ASSERT(!is_wrap.count(p));
		is_wrap.insert(p);
		to_wrap[orig]	= p;
		return true;
	}

	//---------------------------------
	// make_wrap:
	//---------------------------------
	template<typename T> Wrap<T>	*_make_wrap(T *p) {
		ISO_ASSERT(p);
		auto *w		= new Wrap<T>;
		w->orig		= p;
		//p->AddRef();
		to_wrap[p]	= w;
		return w;
	}
	//template<typename T> Wrap<T>	*make_wrap_orphan(T *p) {
	//	ISO_ASSERT(p);
	//	auto *w		= _make_wrap(p);
	//	w->init_orphan();
	//	return w;
	//}

	template<typename T> auto	*make_wrap(T *p) {
		return _make_wrap(GetWrappable(p));
	}
	template<typename T, typename... X> auto *make_wrap(T *p, const X&... x) {
		if (auto *w = find_wrap_check(GetWrappable(p))) {
			//++w->refs;	//make sure
			return w;
		}
		auto	*w = _make_wrap(GetWrappable(p));
		w->init(x...);
		return w;
	}

	
	// pass through HRESULT
	template<typename T, typename... X> HRESULT make_wrap(HRESULT h, T **pp, const X&... x) {
		if (h == S_OK)
			*pp = make_wrap(*pp, x...);
		return h;
	}

	// need QueryInterface
	template<typename T, typename T0> HRESULT make_wrap_qi(HRESULT h, T0 **pp) {
		if (h == S_OK) {
			T	*t;
			h = (*pp)->QueryInterface(__uuidof(T), (void**)&t);
			if (SUCCEEDED(h)) {
				(*pp)->Release();
				*pp = make_wrap(t);
			}
		}
		return h;
	}
	template<typename T, typename T0, typename... X> HRESULT make_wrap_qi(HRESULT h, T0 **pp, const X&... x) {
		if ((h = make_wrap_qi<T>(h, pp)) == S_OK)
			static_cast<Wrap<T>*>(*pp)->init(x...);
		return h;
	}
	
	//---------------------------------
	// find_wrap: (expects to be passed original)
	//---------------------------------
	template<typename T> Wrap<T>	*_find_wrap(T *p) {
		_com_wrap	*w	= to_wrap[p];
		return ISO_VERIFY(static_cast<Wrap<T>*>(w));
	}
	template<typename T> auto		*find_wrap(T *p) {
		return p ? _find_wrap(GetWrappable(p)) : 0;
	}

	template<typename T> Wrap<T>	*_find_wrap_carefully(T *p) {
		if (p) {
			auto w = to_wrap[p];
			if (w.exists())
				return static_cast<Wrap<T>*>(w.or_default());

			if (!is_wrap.count(static_cast<Wrap<T>*>(p)))
				return (Wrap<T>*)make_wrap_orphan(p);
		}
		return static_cast<Wrap<T>*>(p);
	}
	template<typename T> auto		*find_wrap_carefully(T *p) {
		return p ? _find_wrap_carefully(GetWrappable(p)) : 0;
	}
	template<typename T> Wrap<T>	*find_wrap_check(T *p) {
		auto w = to_wrap[p];
		if (w.exists())
			return static_cast<Wrap<T>*>(w.or_default());
		return 0;
	}

	//---------------------------------
	// ReWrap - FindWrap in place (and addref)
	//---------------------------------
	template<typename T> void rewrap(T **pp) {
		if (pp && *pp) {
			*pp = find_wrap(*pp);
			//(*pp)->AddRef();
		}
	}
	template<typename T> HRESULT rewrap(HRESULT h, T **pp) {
		if (h == S_OK)
			rewrap(pp);
		return h;
	}
	template<typename T> void rewrap(T **pp, UINT num) {
		if (pp) {
			while (num--)
				rewrap(pp++);
		}
	}
	template<typename T> void rewrap_carefully(T **pp) {
		if (pp && (*pp = find_wrap_carefully(*pp)))
			(*pp)->AddRef();
	}
	template<typename T> HRESULT rewrap_carefully(HRESULT h, T **pp) {
		if (h == S_OK)
			rewrap_carefully(pp);
		return h;
	}
	template<typename T> void rewrap_carefully(T **pp, UINT num) {
		if (pp) {
			while (num--)
				rewrap_carefully(pp++);
		}
	}
	//HRESULT	rewrap_carefully(HRESULT h, void **pp) {
	//	if (SUCCEEDED(h))
	//		*pp = find_wrap_carefully((IUnknown*)*pp);
	//	return h;
	//}

	//---------------------------------
	// get_wrap: (expects to be passed wrapped)
	//---------------------------------
	template<typename T> Wrap<T>	*_get_wrap(T *p) {
		Wrap<T>	*w = static_cast<Wrap<T>*>(p);
		return is_wrap.count(w) ? w : find_wrap_carefully(p);
	}
	template<typename T> Wrap<T>	*get_wrap(T *p) {
		return _get_wrap(GetWrappable(p));
	}
	// need QueryInterface
	template<typename T, typename T0> HRESULT get_wrap_qi(HRESULT h, T0 **pp) {
		if (h == S_OK) {
			T	*t;
			h = (*pp)->QueryInterface(__uuidof(T), (void**)&t);
			if (SUCCEEDED(h)) {
				(*pp)->Release();
				*pp = _get_wrap(t);
			}
		}
		return h;
	}
	// from hash
	IUnknown						*get_wrap(REFIID riid, void *p);

//	static HRESULT GetWrap(HRESULT h, REFIID riid, void **pp) {
//		if (h == S_OK)
//			*pp = static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(GetWrap(riid, *pp)));
//		return h;
//	}
	
	//---------------------------------
	// unwrap: - get orig from wrapped
	//---------------------------------
	template<typename T> T		*_unwrap(T *p) {
		if (!p)
			return p;
		Wrap<T>	*w = static_cast<Wrap<T>*>(p);
		ISO_ASSERT(is_wrap.count(w));
		return w->orig;
	}
	template<typename T> T		*unwrap(T *p) {
		return _unwrap(GetWrappable(p));
	}
	// if not wrapped, make wrap
	template<typename T> T		*_unwrap_carefully(T *p) {
		if (p) {
			Wrap<T>	*w = static_cast<Wrap<T>*>(p);
			if (is_wrap.count(w))
				return w->orig;
			if (!to_wrap.check(p))
				make_wrap_orphan(p);
		}
		return p;
	}
	template<typename T> auto *unwrap_carefully(T *p) {
		return _unwrap_carefully(GetWrappable(p));
	}

	// if not wrapped, leave
	template<typename T> T		*_unwrap_safe(T *p) {
		if (p) {
			Wrap<T>	*w = static_cast<Wrap<T>*>(p);
			if (is_wrap.count(w))
				return w->orig;
		}
		return p;
	}
	template<typename T> auto *unwrap_safe(T *p) {
		return _unwrap_safe(GetWrappable(p));
	}
	// if not wrapped, return 0
	template<typename T> T		*_unwrap_test(T *p) {
		if (p) {
			Wrap<T>	*w = static_cast<Wrap<T>*>(p);
			if (is_wrap.count(w))
				return w->orig;
		}
		return 0;
	}
	template<typename T> auto *unwrap_test(T *p) {
		return _unwrap_test(GetWrappable(p));
	}

	template<typename T> T *const *unwrap(T **unwrapped, T *const *pp, UINT num) {
		for (int i = 0; i < num; i++)
			unwrapped[i] = unwrap(pp[i]);
		return (T *const *)unwrapped;
	}

};

static singleton<_com_wrap_system> com_wrap_system;

inline _com_wrap::_com_wrap() {
	ISO_ASSERT(com_wrap_system->is_wrap.count(this) == 0);
	com_wrap_system->is_wrap.insert(this);
}
inline _com_wrap::~_com_wrap() {
	com_wrap_system->is_wrap.remove(this);
}

template<class T> class com_wrap : public T, public _com_wrap {
public:
	T		*orig;

	~com_wrap() {
		//if (orig)
		//	orig->Release();
	}

	void	mark_dead() {
		T	*p = orig;
		orig = (T*)(uintptr_t(p) | 1);
		com_wrap_system->remove(this, p);
	}
	void	mark_undead() {
		orig = (T*)(uintptr_t(orig) & ~1);
		ISO_VERIFY(com_wrap_system->add(this, orig));
	}
	constexpr bool	is_orig_dead()	const {
		return uintptr_t(orig) & 1;
	}

	ULONG STDMETHODCALLTYPE	AddRef() {
		if (is_orig_dead())
			mark_undead();
		return orig->AddRef();// - 1;
	}
	ULONG STDMETHODCALLTYPE	Release() {
		ULONG n = orig->Release();
		if (n == 0)
			mark_dead();
		return n;// - 1;
	}
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		if (check_interface<IUnknown>	(this, riid, pp)
		||	check_interface<T>			(this, riid, pp)
		)	return S_OK;

		auto	h = orig->QueryInterface(riid, pp);
		if (h == S_OK) {
			auto	p = com_wrap_system->get_wrap(riid, *pp);
			p->AddRef();
			((IUnknown*)(*pp))->Release();
			*pp = p;//static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(p));
		}
		return h;
	}
};

inline IUnknown	*_com_wrap_system::get_wrap(REFIID riid, void *p) {
	auto w = to_wrap[p];
	if (auto w2 = w.or_default())
		return static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(w2));

	return (IUnknown*)p;
}

template<> class Wrap<IUnknown> : public com_wrap<IUnknown> {};

template<typename T> void unwrapped<T>::operator=(const T _t)	{
	t = com_wrap_system->unwrap_carefully(_t);
}
//-----------------------------------------------------------------------------
// RPC mechanism
//-----------------------------------------------------------------------------

#if 0

// RemoteDll-based

template<typename T> struct get_result_s {
	static void f(void *p, const T &r) { *((T*)p) = r; }
};

template<typename T> struct get_result_s<dynamic_array<T> > {
	static void f(void *p, const dynamic_array<T> &r) {
		*(flat_array<T>*)p = r;
	}
};

template<> struct get_result_s<const_memory_block> {
	static void f(void *p, const const_memory_block &r) {
		r.copy_to(p);
	}
};

template<typename R> void get_result(void *p, const R &r) {
	get_result_s<R>::f(p, r);
}

template<typename F> struct RPC {
	template<F f> static DWORD thread_fn(void *p) {
		get_result(p, call(f, *(TL_tuple<typename function<F>::P>*)p));
		return 0;
	}
};

#define EXT_RPC(f)	extern "C" { __declspec(dllexport) DWORD RPC_##f(void *p) { return T_get_class<RPC>(f)->thread_fn<f>(p); } }
#endif


struct SocketWait : Socket {
	using Socket::Socket;
	size_t	readbuff(void *buffer, size_t size) const {
		char *p = (char*)buffer;
		while (size) {
			size_t	result = Socket::readbuff(p, size);
			size	-= result;
			p		+= result;
			if (size && !select(1))
				break;
		}

		return p - (char*)buffer;
	}
	template<typename T>T		get()			const	{ T t; read(t); return t; }
	template<typename...T> bool read(T&&...t)	const	{ return iso::read(*this, t...); }
};

#if 1
typedef with_size<malloc_block> malloc_block_all;
#else
struct malloc_block_all : malloc_block {
	using malloc_block::malloc_block;
	using malloc_block::operator=;
	template<typename R>	bool	read(R&& r) {
		auto	size = r.template get<uint32>();
		return malloc_block::read(r, size);
	}
	template<typename W>	bool	write(W&& w) const {
		return w.write(size32()) && malloc_block::write(w);
	}
};
#endif

template<typename F> enable_if_t<!same_v<typename function<F>::R, void>> SocketRPC(SocketWait &sock, F f) {
	TL_tuple<typename function<F>::P>	params;
	params.read(sock);
	sock.write(call(f, params));

}

template<typename F> enable_if_t<same_v<typename function<F>::R, void>> SocketRPC(SocketWait &sock, F f) {
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

template<typename R, typename...P> enable_if_t<same_v<R, void>> SocketCallRPC(const SocketWait &sock, int func, const P&...p) {
	sock.putc(func);
	tuple<P...>(p...).write(sock);
}


} // namespace iso

#endif // HOOK_COM_H
