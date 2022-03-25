#ifndef GALOIS2_H
#define GALOIS2_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/constants.h"

namespace iso {

// extend T_uint_type to bigger than machine sizes with double_int

template<int BYTES>		struct T_uint_type : T_type<double_int<uint_t<BYTES / 2>>> {};

//-----------------------------------------------------------------------------
// carryless operations
//-----------------------------------------------------------------------------

template<typename A, typename B> auto	carryless_fullmul(A a, B b) {
	typedef uint_t<sizeof(A) + sizeof(B)>	R;
	R	r	= zero;
	while (b) {
		r ^= shift_left2<R>(a, lowest_set_index(b));
		b = clear_lowest(b);
	}

	return r;
}

template<typename A, typename B> auto	carryless_mul(A a, B b) {
	typedef uint_t<meta::max<sizeof(A), sizeof(B)>>	R;
	R	r	= zero;
	while (b) {
		r ^= shift_left2<R>(a, lowest_set_index(b));
		b = clear_lowest(b);
	}

	return r;
}

template<typename A, typename B> auto	carryless_mod(A a, B b) {
	int	eb	= highest_set_index(b);

	while (a >= b) {
		int	ea = highest_set_index(a);
		a	^= shift_left2<A>(b, ea - eb);
	}

	return a;
}

template<typename A, typename B> auto	carryless_div(A a, B b) {
	int	eb	= highest_set_index(b);
	A	d	= zero;

	while (a >= b) {
		int	ea = highest_set_index(a);
		a	^= shift_left2<A>(b, ea - eb);
		d	+= shift_left2<A>(1, ea - eb);
	}

	return d;
}

template<typename A, typename B> auto	carryless_divmod(A &a, B b) {
	int	eb	= highest_set_index(b);
	A	d	= zero;

	while (a >= b) {
		int	ea = highest_set_index(a);
		a	^= shift_left2<A>(b, ea - eb);
		d	+= shift_left2<A>(1, ea - eb);
	}

	return d;
}

//-----------------------------------------------------------------------------
//	galois field GF(2)
//-----------------------------------------------------------------------------

template<int N> struct gf2_vec {
	typedef uint_bits_t<N>	U;
	U	u;

	gf2_vec() {}
	gf2_vec(U u) : u(u) {}
	gf2_vec&	operator+=(gf2_vec b)		{ u ^= b.u; return *this; }
	gf2_vec&	operator-=(gf2_vec b)		{ u ^= b.u; return *this; }
	gf2_vec		operator+(gf2_vec b) const	{ return u ^ b.u; }
	gf2_vec		operator-(gf2_vec b) const	{ return u ^ b.u; }
	bool		operator!() const			{ return !u; }
	bool		operator[](int i)	const	{ return !!(shift_right2<uint32>(u, i) & 1); }
	void		clear()						{ iso::clear(u); }
};

template<int A, int B> bool	operator==(const gf2_vec<A> &a, const gf2_vec<B> &b) {
	int		ea = highest_set_index(a.u);
	int		eb = highest_set_index(b.u);
	return ea == eb && memcmp(&a.u, &b.u, (ea + 7) / 8) == 0;
}


template<int N, int M> struct gf2_mat {
	gf2_vec<N>	u[M];

	gf2_mat()	{}
	gf2_mat(const _one&)	{
		gf2_vec<N>	v(1);
		for (int i = 0; i < M; i++, v.shift())
			u[i] = v;
	}
	gf2_vec<N>&	operator[](int i)			{ return u[i]; }
	gf2_vec<N>	operator[](int i)	const	{ return u[i]; }
};

template<int N, int M> gf2_vec<N> madd(const gf2_mat<N, M> &m, gf2_vec<M> v, gf2_vec<N> r) {
	for (auto t = v.u; t; t = clear_lowest(t))
		r += m[lowest_set_index(t)];
	return r;
}

template<int N, int M> gf2_vec<N> operator*(const gf2_mat<N, M> &m, gf2_vec<M> v) {
	return madd(m, v, gf2_vec<N>(0));
}

template<int N, int M, int X> gf2_mat<N, M> operator*(const gf2_mat<N, X> &a, const gf2_mat<X, M> &b) {
	gf2_mat<N, M>	r;
	for (int i = 0; i < M; i++)
		r[i] = a * b[i];
	return r;
}

template<int N> struct gf2_poly : gf2_vec<N> {
	typedef gf2_vec<N>	B;
	using typename B::U;
	using B::u;
	using B::B;

	gf2_poly&	shift() 					{ u <<= 1; return *this; }
	int			degree()	const			{ return highest_set_index(u); }

	gf2_poly&	operator+=(gf2_poly b)		{ u ^= b.u; return *this; }
	gf2_poly&	operator-=(gf2_poly b)		{ u ^= b.u; return *this; }
	gf2_poly	operator+(gf2_poly b) const	{ return u ^ b.u; }
	gf2_poly	operator-(gf2_poly b) const	{ return u ^ b.u; }

	bool	is_primitive() const {
		int		e	= degree();
		U		j	= bits(e + 1);
		U		k	= (j / 2) + 2;
		int		p2	= k;
		for (int n = (1 << e) - 2; n--;) {
			p2 <<= 1;
			p2 |= count_bits(p2 & u) & 1;
			if ((p2 & j) == k)
				return false;
		}
		return true;
	}
};


template<int N, int M> gf2_poly<N + M>	operator*(gf2_poly<N> &a, gf2_poly<M> b) {
	return carryless_fullmul(a.u, b.u);
}

template<int N, int M> gf2_poly<M>	operator%(gf2_poly<N> &a, gf2_poly<M> b) {
	return carryless_mod(a.u, b.u);
}

template<int N, int M> gf2_poly<N>	operator/(gf2_poly<N> &a, gf2_poly<M> b) {
	return carryless_div(a.u, b.u);
}

template<int N, int M> gf2_poly<N>	divmod(gf2_poly<N> &a, gf2_poly<M> b) {
	return carryless_divmod(a.u, b.u);
}

} // namespace iso
#endif // GALOIS2_H
