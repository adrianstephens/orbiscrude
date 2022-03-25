#ifndef TYPETREE_H
#define TYPETREE_H

#include "base/defs.h"

namespace iso {

template<typename T> struct counted_data {
	T	*t;
	int	n;
	counted_data(T *_t, int _n) : t(_t), n(_n) {}
};
template<typename T> counted_data<T> make_counted_data(T *t, int n) {
	return counted_data<T>(t, n);
}

template<typename T> struct tree_type;

// type_tree
template<typename L, typename R> struct type_tree {
	typedef L		left;
	typedef R		right;
};

#define TT2(A,B)								type_tree<A,B >
#define TT3(A,B,C)								TT2(TT2(A,B),C)
#define TT4(A,B,C,D)							TT2(TT2(A,B),TT2(C,D))
#define TT5(A,B,C,D,E)							TT2(TT2(A,B),TT3(C,D,E))
#define TT6(A,B,C,D,E,F)						TT2(TT4(A,B,C,D),TT2(E,F))
#define TT7(A,B,C,D,E,F,G)						TT2(TT4(A,B,C,D),TT3(E,F,G))
#define TT8(A,B,C,D,E,F,G,H)					TT2(TT4(A,B,C,D),TT4(E,F,G,H))
#define TT9(A,B,C,D,E,F,G,H,I)					TT2(TT5(A,B,C,D,E),TT4(F,G,H,I))
#define TT10(A,B,C,D,E,F,G,H,I,J)				TT2(TT6(A,B,C,D,E,F),TT4(G,H,I,J))
#define TT11(A,B,C,D,E,F,G,H,I,J,K)				TT2(TT6(A,B,C,D,E,F),TT5(G,H,I,J,K))
#define TT12(A,B,C,D,E,F,G,H,I,J,K,L)			TT2(TT6(A,B,C,D,E,F),TT6(G,H,I,J,K,L))
#define TT13(A,B,C,D,E,F,G,H,I,J,K,L,M)			TT2(TT8(A,B,C,D,E,F,G,H),TT5(I,J,K,L,M))
#define TT14(A,B,C,D,E,F,G,H,I,J,K,L,M,N)		TT2(TT8(A,B,C,D,E,F,G,H),TT6(I,J,K,L,M,N))
#define TT15(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O)		TT2(TT8(A,B,C,D,E,F,G,H),TT7(I,J,K,L,M,N,O))
#define TT16(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P)	TT2(TT8(A,B,C,D,E,F,G,H),TT8(I,J,K,L,M,N,O,P))


template<typename T>		struct TT_count							{ enum { value = 1 }; };
template<typename L, typename R> struct TT_count<type_tree<L, R> >	{ enum { value = TT_count<L>::value + TT_count<R>::value }; };

template<typename T>		struct TT_alignment : T_alignment<T>	{};
template<typename L, typename R> struct TT_alignment<type_tree<L, R> >	{ enum { left = TT_alignment<L>::value, right = TT_alignment<R>::value, value = left > right ? left : right }; };

template<typename T, int N>	struct TT_index							{ typedef T type; };
template<typename L, typename R, int N> struct TT_index<type_tree<L, R>, N>	{
	enum { left = TT_count<L>::value };
	typedef typename T_if<(N < left), typename TT_index<L, N>::type, typename TT_index<R, N - left>::type>::type type;
};

template<typename T, int O>	struct TT_offset		{ char a[O]; T b; };
template<typename T>		struct TT_offset<T, 0>	{ T b; };

template<typename T, int O> struct TT_size {
	enum { size	= sizeof(TT_offset<T, O>) };
	template<int N> struct offset { enum {value = O}; };
};
template<typename L, typename R, int O> struct TT_size<type_tree<L, R>, O> {
	enum { 
		left	= TT_count<L>::value,
		off		= TT_size<L, O>::size,
		size	= TT_size<R, off>::size,
	};
	template<int N> struct offset {
		enum { value = T_ifnum<(N < left), TT_size<L, O>::offset<N>::value, TT_size<R, off>::offset<N - left>::value>::value };
	};
};

// function types

template<typename F> struct function_type;
template<typename F> struct function_type<F*> : function_type<F> {};
template<typename r>
struct function_type<r(void)> {
	typedef r R; typedef R (F)(void); typedef void P;
};
template<typename r, typename P1>
struct function_type<r(P1)> {
	typedef r R; typedef R (F)(P1); typedef P1 P;
};
template<typename r, typename P1, typename P2>
struct function_type<r(P1,P2)> {
	typedef r R; typedef R (F)(P1,P2); typedef TT2(P1,P2) P;
};
template<typename r, typename P1, typename P2, typename P3>
struct function_type<r(P1,P2,P3)> {
	typedef r R; typedef R (F)(P1,P2,P3); typedef TT3(P1,P2,P3) P;
};
template<typename r, typename P1, typename P2, typename P3, typename P4>
struct function_type<r(P1,P2,P3,P4)> {
	typedef r R; typedef R (F)(P1,P2,P3,P4); typedef TT4(P1,P2,P3,P4) P;
};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5>
struct function_type<r(P1,P2,P3,P4,P5)> {
	typedef r R; typedef R (F)(P1,P2,P3,P4,P5); typedef TT5(P1,P2,P3,P4,P5) P;
};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
struct function_type<r(P1,P2,P3,P4,P5,P6)> {
	typedef r R; typedef R (F)(P1,P2,P3,P4,P5,P6); typedef TT6(P1,P2,P3,P4,P5,P6) P; 
};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
struct function_type<r(P1,P2,P3,P4,P5,P6,P7)> {
	typedef r R; typedef R (F)(P1,P2,P3,P4,P5,P6,P7); typedef TT7(P1,P2,P3,P4,P5,P6,P7) P;
};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
struct function_type<r(P1,P2,P3,P4,P5,P6,P7,P8)> {
	typedef r R; typedef R (F)(P1,P2,P3,P4,P5,P6,P7,P8); typedef TT8(P1,P2,P3,P4,P5,P6,P7,P8) P;
};

template<typename r, typename P1>
struct function_type_p : function_type<r(P1)> {};
template<typename r, typename P1, typename P2>
struct function_type_p<r, type_tree<P1,P2> > : function_type<r(P1,P2)> {};
template<typename r, typename P1, typename P2, typename P3>
struct function_type_p<r, TT3(P1,P2,P3)> : function_type<r(P1,P2,P3)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4>
struct function_type_p<r, TT4(P1,P2,P3,P4)> : function_type<r(P1,P2,P3,P4)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5>
struct function_type_p<r, TT5(P1,P2,P3,P4,P5)> : function_type<r(P1,P2,P3,P4,P5)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
struct function_type_p<r, TT6(P1,P2,P3,P4,P5,P6)> : function_type<r(P1,P2,P3,P4,P5,P6)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
struct function_type_p<r, TT7(P1,P2,P3,P4,P5,P6,P7)> : function_type<r(P1,P2,P3,P4,P5,P6,P7)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
struct function_type_p<r, TT8(P1,P2,P3,P4,P5,P6,P7,P8)> : function_type<r(P1, P2,P3,P4,P5,P6,P7,P8)> {};

// helper types

template<typename T> struct ptr_type {
	typedef typename T_noconst<T>::type TM;
	T	*t;
	operator T*()	const	{ return t; }
	void operator=(T *_t)	{ t = _t; }
	template<typename T2> void operator=(const T2 &p) {
		if (p)
			*(TM*)(t = new T) = *p;
		else
			t = 0;
	}
};

template<typename T, int I> struct counted_type {
	typedef typename T_noconst<T>::type TM;
	T *t;
	void operator=(T *_t)	{ t = _t; }
	template<typename T2> void operator=(const counted_data<T2> &c) {
		if (c.n) {
			t = new T[c.n];
			TM			*d = (TM*)t;
			const T2	*s = c.t;
			for (int n = c.n; n--;)
				*d++ = *s++;
		} else {
			t = 0;
		}
	}
};

// struct_tree

template<typename T> struct struct_tree;

template<typename L, typename R> struct struct_tree<type_tree<L, R> > {
	enum	{count = TT_count<type_tree<L, R> >::value, alignment = TT_alignment<type_tree<L, R> >::value};
	char	data[TT_size<type_tree<L, R>, 0>::size];

	template<int N>	typename TT_index<type_tree<L, R>, N>::type &get()	{
		return ((TT_offset<typename TT_index<type_tree<L, R>, N>::type, TT_size<type_tree<L, R>, 0>::offset<N>::value>*)data)->b;
	}
	template<int N>	const typename TT_index<type_tree<L, R>, N>::type &get() const {
		return ((TT_offset<const typename TT_index<type_tree<L, R>, N>::type, TT_size<type_tree<L, R>, 0>::offset<N>::value>*)data)->b;
	}

	template<typename L1, typename R1> inline void operator=(const struct_tree<type_tree<L1, R1> > &b);
	template<typename T> inline void operator=(const T &b);

	template<typename D, typename S>		void copy_element(D &d, const S &s)						const { d = s; }
	template<typename D, typename S, int I>	void copy_element(counted_type<D, I> &d, const S &s)	const { d = make_counted_data(&*s, get<I>()); }
	template<typename D, typename S, int I>	void copy_element(D &d, const counted_type<S, I> &s)	const { d = make_counted_data(s.t, get<I>()); }
	template<typename D, typename S>		void copy_element(D &d, const ptr_type<S> &s)			const { d = &*s; }
};

template<int N> struct struct_tree_assign {
	template<typename D, typename S> static inline void f(struct_tree<D> &d, const struct_tree<S> &s) {
		s.copy_element(d.get<N>(), s.get<N>());
		struct_tree_assign<N-1>::f(d, s);
	}
};
template<> struct struct_tree_assign<0> {
	template<typename D, typename S> static inline void f(struct_tree<D> &d, const struct_tree<S> &s) {
		s.copy_element(d.get<0>(), s.get<0>());
	}
};

template<typename L, typename R> template<typename L1, typename R1> void struct_tree<type_tree<L, R> >::operator=(const struct_tree<type_tree<L1, R1> > &b) {
	struct_tree_assign<count-1>::f(*this, b);
}
template<typename L, typename R>  template<typename T> inline void struct_tree<type_tree<L, R> >::operator=(const T &b)	{
	operator=((const struct_tree<tree_type<T>::type>&)b);
}

template<template<class> class M, typename L, typename R>	struct T_map<M, type_tree<L, R> >		{ typedef type_tree<typename T_map<M, L>::type, typename T_map<M, R>::type> type; };
template<template<class> class M, typename T>				struct T_map<M, struct_tree<T> >		{ typedef struct_tree<typename T_map<M, T>::type> type; };
template<template<class> class M, typename F>				struct T_map<M, function_type<F> >		{ typedef typename M<function_type_p<typename T_map<M, typename F::R>::type, typename T_map<M, typename F::P>::type> >::type type; };
template<template<class> class M, typename T, int I>		struct T_map<M, counted_type<T, I> >	{ typedef typename M<counted_type<typename T_map<M, T>::type, I> >::type type; };
template<template<class> class M, typename T>				struct T_map<M, ptr_type<T> >			{ typedef typename M<ptr_type<typename T_map<M, T>::type> >::type type; };

}

#endif	// TYPETREE_H
