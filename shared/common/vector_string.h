#ifndef VECTOR_STRING_H
#define VECTOR_STRING_H

#include "base/vector.h"
#include "base/strings.h"

namespace iso {

template<typename T0, typename C>	inline accum<C> &put_list(accum<C> &s, T0&& t0) {
	return s << t0;
}
template<typename T0, typename...T, typename C>	inline accum<C> &put_list(accum<C> &s, T0&& t0, T&&... t) {
	return put_list(s << t0 << ", ", t...);
}
template<typename E, size_t...I, typename C>	inline accum<C> &put_vec(accum<C> &s, const vec<E, sizeof...(I)> &v, index_list<I...>) {
	return put_list(s << '(', E(v[I])...) << ')';
}

#ifdef __clang__
template<typename C, typename V> inline enable_if_t<is_vec<V>, accum<C>&> operator<<(accum<C> &s, const V &v) {
	return put_vec<element_type<V>>(s, v, meta::make_index_list<num_elements_v<V>>());
}
#endif

template<typename V> inline enable_if_t<is_vec<V>, size_t> to_string(char *s, const V &v)	{
	fixed_accum	a(s, 256);
	put_vec<element_type<V>>(a, v, meta::make_index_list<num_elements_v<V>>());
	return a.getp() - s;
}
template<typename V> inline enable_if_t<is_vec<V>, fixed_string<16 * num_elements_v<V>>> to_string(const V &v)	{ fixed_string<16 * num_elements_v<V>>	s; s << v; return s; }

inline size_t 				to_string(char *s, param(colour) v)		{ return to_string(s, v.rgba); }
inline fixed_string<64>		to_string(param(colour) v)				{ fixed_string<64>	s; s << v; return s; }

template<typename E, int N> 		inline size_t 	to_string(char *s, const pos<E,N>	&v)	{ return to_string(s, v.v); }
template<typename E, int N> 		inline auto		to_string(const pos<E,N> &v)			{ fixed_string<N*16>	s; s << v; return s; }

template<typename E, int N, int M> 	inline size_t 	to_string(char *s, const mat<E,N,M> &m)	{ size_t x = 0; for (int i = 0; i < M; i++) x += to_string(s + x, m.column(i)); return x; }
template<typename E, int N, int M> 	inline auto		to_string(const mat<E,N,M>	&m)			{ fixed_string<16*N*M>	s; s << m; return s; }

} //namespace iso

#endif // VECTOR_STRING_H
