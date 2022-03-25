#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include "base/defs.h"
#include "base/array.h"
#include "base/vector.h"

namespace iso {
//-----------------------------------------------------------------------------
// evaluate
//-----------------------------------------------------------------------------

template<typename C, typename T, size_t...I>	auto poly_eval(const C &c, const T &x, index_list<I...>)		{ return horner(x, c[I]...); }
template<int N, typename C, typename T>			auto poly_eval(const C &c, const T &x)							{ return poly_eval(c, x, meta::make_index_list<N>()); }
template<typename E, int N, typename T>			auto poly_eval(const E (&c)[N], const T &x)						{ return poly_eval<N>(c, x); }
template<typename V, typename T>				auto poly_eval(const V &c, const T &x)							{ return poly_eval<num_elements_v<V>>(c, x); }

template<typename C, typename T, size_t...I>	auto norm_poly_eval(const C &c, const T &x, index_list<I...>)	{ return horner(x, c[I]..., one); }
template<int N, typename C, typename T>			auto norm_poly_eval(const C &c, const T &x)						{ return norm_poly_eval(c, x, meta::make_index_list<N>()); }
template<typename E, int N, typename T>			auto norm_poly_eval(const E (&c)[N], const T &x)				{ return norm_poly_eval<N>(c, x); }
template<typename V, typename T>				auto norm_poly_eval(const V &c, const T &x)						{ return norm_poly_eval<num_elements_v<V>>(c, x); }

template<typename V> auto poly_deriv(const V &c) {
	static const int N = num_elements_v<V>;
	typedef meta::muladd<1, 1, meta::make_value_sequence<N - 1>>	S;
	return swizzle(S(), c) * to<element_type<V>>(S());
}

template<typename V> auto norm_poly_deriv(const V &c)	{
	static const int N = num_elements_v<V>;
	typedef meta::muladd<1, 1, meta::make_value_sequence<N - 1>>	S;
	return swizzle(S(), c) * to<element_type<V>>(S() / constant_int<N>());
}


//-----------------------------------------------------------------------------
// multiply polynomials
//-----------------------------------------------------------------------------

template<int I> struct poly_mul_s {
	template<typename E, int N1, int N2> static auto f(vec<E, N1> a, vec<E, N2> b) {
		return to<E>(poly_mul_s<I - 1>::template f<E, N1, N2>(a, b), zero) + extend_left<N1 + I>(a * b[I]);
	}
};
template<> struct poly_mul_s<0> {
	template<typename E, int N1, int N2> static auto f(vec<E, N1> a, vec<E, N2> b) {
		return to<E>(a * b.x);
	}
};
template<typename A, typename B> auto poly_mul(const A &a, const B &b) {
	static const int N1 = num_elements_v<A>, N2 = num_elements_v<B>;
	return poly_mul_s<N2 - 1>::template f<element_type<A>, N1, N2>(a, b);
}

// div/mod polynomials
template<typename E, int N1, int N2> struct norm_poly_mod_s {
	static vec<E, N2> f(vec<E, N1> num, vec<E, N2> div) {
		return norm_poly_mod_s<E, N1 - 1, N2>::f(shrink<N1 - 1>(num - extend_right<N1>(div) * num[N1 - 1]), div);
	}
};
template<typename E, int N> struct norm_poly_mod_s<E, N, N> {
	static vec<E, N> f(vec<E, N> num, vec<E, N> div) {
		return num - div * num[N - 1];
	}
};

template<typename A, typename B> auto norm_poly_mod(const A &a, const B &b) {
	return norm_poly_mod_s<element_type<A>, num_elements_v<A>, num_elements_v<B>>::f(a, b);
}

template<typename E, int N> vec<E, N> norm_poly_div(int power, E val, vec<E, N> div) {
	if (power < N)
		return rotate(extend_right<N>(val), power);

	auto	num = extend_left<N>(val);
	for (power -= N; power--;)
		num = to<E>(zero, shrink<N - 1>(num)) - div * num[N - 1];
	return num;
}

//-----------------------------------------------------------------------------
//	lagrange root bounds
//-----------------------------------------------------------------------------

template<typename E, int N> E upper_bound_lagrange(vec<E, N> k) {
	auto	bounds	= pow(max(-k, zero), to<E>(one / (constant_int<N>() - meta::make_value_sequence<N>())));
	auto	a		= reduce_max(bounds);
	return a + reduce_max(select(bounds == a, zero, bounds));
}

// lagrange upper bound applied to f(-x) to get lower bound
template<typename E, int N> E lower_bound_lagrange(vec<E, N> k) {
	return -upper_bound_lagrange<E, N>(-masked(constant_int<((1 << (N | 1)) - 1) / 3>(), k));
}

template<typename E, int N> vec<E,2> bound_lagrange(vec<E, N> k) {
	auto	bounds	= pow(abs(k), to<E>(one / (constant_int<N>() - meta::make_value_sequence<N>())));
	auto	mask	= k < zero;
	auto	upper	= select(mask, bounds, zero);
	auto	lower	= select(select(((1 << (N | 1)) - 1) / 3, ~mask, mask), bounds, zero);
	auto	u1		= reduce_max(upper);
	auto	l1		= reduce_max(lower);
	return {
		-(l1 + reduce_max(select(lower == l1, zero, lower))),
		u1 + reduce_max(select(upper == u1, zero, upper))
	};
}

//-----------------------------------------------------------------------------
//	halley's method to refine roots
//-----------------------------------------------------------------------------
template<typename E, int N, typename T> T halley(vec<E, N> poly, vec<E, N - 1> dpoly, vec<E, N - 2> ddpoly, const T &x) {
	T	f	= poly_eval(poly, x);
	T	f1	= poly_eval(dpoly, x);
	T	f2	= poly_eval(ddpoly, x);
	return x - (f * f1 * two) / (square(f1) * two - f * f2);
}

template<typename E, int N, typename T> T halley(vec<E, N> poly, const T &x) {
	auto	poly1	= poly_deriv(poly);
	auto	poly2	= poly_deriv(poly1);
	return halley<E, N>(poly, poly1, poly2, x);
}
template<typename V, typename T> T halley(V poly, const T &x) {
	return halley<element_type<V>, num_elements_v<V>>(poly, x);
}

template<typename E, int N, typename T> T norm_halley(vec<E, N> poly, vec<E, N - 1> dpoly, vec<E, N - 2> ddpoly, const T &x) {
	T	f	= norm_poly_eval(poly, x);
	T	f1	= norm_poly_eval(dpoly, x) * N;
	T	f2	= norm_poly_eval(ddpoly, x) * N * (N - 1);
	return x - (f * f1 * two) / (square(f1) * two - f * f2);
//	return x - (f * f1 * (N * two)) / (square(f1) * (N * N * two) - f * f2 * (N - 1));
}

template<typename E, int N, typename T> T norm_halley(vec<E, N> poly, const T &x) {
	auto	poly1	= norm_poly_deriv(poly);
	auto	poly2	= norm_poly_deriv(poly1);
	return norm_halley<E, N>(poly, poly1, poly2, x);
}
template<typename V, typename T> T norm_halley(V poly, const T &x) {
	return norm_halley<element_type<V>, num_elements_v<V>>(poly, x);
}

//-----------------------------------------------------------------------------
// order 1
// x + k.x = 0
//-----------------------------------------------------------------------------

template<typename E> int norm_poly_roots(vec<E, 1> k, vec<E, 1> &r) {
	r = -k;
	return 1;
}

//-----------------------------------------------------------------------------
// order 2
// x^2 + k.y * x + k.x = 0
//-----------------------------------------------------------------------------

template<typename E> int norm_poly_roots(vec<E, 2> k, vec<E, 2> &r) {
	// discriminant
	E e = k.y * half;
	E d = square(e) - k.x;
	if (d > zero) {
		r = plus_minus(-sqrt(d)) - e;
		return 2;

	} else if (d == zero) {
		r = -e;
		return 1;

	} else {
		r = vec<E,2>{-e, sqrt(-d)};
		return -2;
	}
}

//-----------------------------------------------------------------------------
// order 3
// x^3 + k.z * x^2 + k.y * x + k.x = 0
//-----------------------------------------------------------------------------

template<typename E> int norm_poly_roots(vec<E, 3> k, vec<E, 3> &r) {
	const vec<E,4>	cv1{two,	-one,	-one,		one};
	const vec<E,4>	cv2{zero,	-sqrt3,	+sqrt3,		0};
	const vec<E,4>	cv3{one,	-half,	+sqrt3/two,	0};
	const vec<E,4>	cv4{one,	-half,	-sqrt3/two,	0};

	E	e	= k.z / 3;
	E	f	= square(e) - k.y / 3;
	E	g	= (e * k.y - k.x) / 2 - cube(e);
	E	h	= square(g) - cube(f);

	if (h < 0) {
		//3 real roots
		auto	t	= sincos(atan2(copysign(sqrt(-h), g), g) / 3);
		r		= (cv1.xyz * t.x + cv2.xyz * t.y) * sqrt(f) - e;
		return 3;

	} else if (h > 0) {
		//1 real root, 2 imaginary (y + iz) & (y - iz)
		auto	t = pow(cv1.wz * sqrt(h) + g, third);
		r		= cv3.xyz * t.x + cv4.xyz * t.y - concat(e, e, zero);
		return -1;

	} else {
		//3 real and equal
		r = pow(-k.x, third);
		return 1;
	}
}

//-----------------------------------------------------------------------------
// order 4
// x^4 + k.w * x^3 + k.z * x^2 + k.y * x + k.x = 0
//-----------------------------------------------------------------------------

template<typename E> int norm_poly_roots(vec<E, 4> k, vec<E, 4> &roots) {
	//biquadratic case
	if (all(k.yw == zero)) {
		vec<E,2>	r2;
		switch (norm_poly_roots<E>(k.xz, r2)) {
			case 1:
				roots.xy = plus_minus(sqrt(r2.x));
				return 2;
			default:
				roots = plus_minus(sqrt(r2));
				return 4;
		}
	}

	E a		= k.w;
	E b		= k.z;
	E c		= k.y;
	E d		= k.x;

	//  substitute x = y - A/4 to eliminate cubic term:
	//	x^4 + px^2 + qx + r = 0

	E a2	= a * a;
	E p		= b - 3 * a2 / 8;
	E q		= a2 * a / 8 - a * b / 2 + c;
	E r		= -3 * a2 * a2 / 256 + a2 * b / 16 - a * c / 4 + d;

	vec<E, 3>	s3;
	if (abs(r) < epsilon) {
		// no absolute term: y(y^3 + py + q) = 0
		int		num3	= norm_poly_roots<E>(vec<E,3>{q, p, zero}, s3);
		roots			= concat(E(zero), s3 - a / 4);
		return num3 < 0 ? 2 : 4;
	}

	// solve the resolvent cubic ...
	norm_poly_roots<E>(vec<E, 3>{-square(q) / 8, square(p) / 4 - r, p}, s3);
	// ... and take the one real solution to build two quadratic equations
	E 	m = s3.x;
	E	v = sqrt(m * 2);
	E	u = q * rsqrt(m * 8);
	
	vec<E, 2>	s2a, s2b;
	int	num_a = norm_poly_roots<E>(vec<E, 2>{m + p / 2 - u,  v}, s2a);
	int	num_b = norm_poly_roots<E>(vec<E, 2>{m + p / 2 + u, -v}, s2b);

	if (num_a < 0 && num_b < 0) {
		return 0;

	} else if (num_a > 0 && num_b > 0) {
		auto	s	= concat(s2a, s2b) - a / 4;
		//single halley iteration to fix cancellation
		roots = norm_halley(k, s);
		return 4;

	} else {
		auto	s = (num_a > 0 ? s2a : s2b) - a / 4;
		//single halley iteration to fix cancellation
		roots.xy = norm_halley(k, s);
		return 2;
	}
}

//-----------------------------------------------------------------------------
// order 5
// x^5 + k[5] * x^4 + k.w * x^3 + k.z * x^2 + k.y * x + k.x = 0
//-----------------------------------------------------------------------------

template<int N> struct norm_poly_roots_sorted_s {
	template<typename E> static int f(vec<E, N> k, vec<E, N> &r) {
		return norm_poly_roots<E>(k, r);
	}
};

template<> struct norm_poly_roots_sorted_s<3> {
	template<typename E> static int f(vec<E, 3> k, vec<E, 3> &r) {
		auto	n = norm_poly_roots<E>(k, r);
		r = sort(r);
		return n;
	}
};

//	adjust intervals to guarantee convergence of halley's method by using roots of further derivatives

template<typename E, int N, int ND> int adjust_roots(vec<E, N> k, vec<E, ND> kd, E *lo, E *hi, int num_roots) {
	vec<E, ND>	rootsd;
	int			num_rootsd	= abs(norm_poly_roots_sorted_s<ND>::template f<E>(kd, rootsd));
	auto		vals		= norm_poly_eval(k, rootsd);

	for (int i = 0; i < num_roots; i++) {
		for (int j = 0; j < num_rootsd; j++) {
			E	r = rootsd[j];
			if (between(r, lo[i], hi[i]))
				(vals[j] > zero ? hi : lo)[i] = r;
		}
	}
	return num_rootsd;
}


template<typename E> int norm_poly_roots(vec<E, 5> k, vec<E, 5> &roots) {
	vec<E, 5>	roots0;
	vec<E, 4>	roots1;
	auto		k1			= norm_poly_deriv(k);
	auto		k2			= norm_poly_deriv(k1);
	auto		bounds		= bound_lagrange<E,5>(k);

	int			num_roots1	= norm_poly_roots<E>(k1, roots1);
	int			num_roots	= 0;

	if (num_roots1 == 4) {
		roots1		= sort(roots1);

		auto	vals 	= norm_poly_eval(k, roots1);
		auto	sel		= concat(E(-1), vals) * concat(vals, 1) < zero;
		roots0			= pack(masked(sel, (concat(bounds.x, roots1) + concat(roots1, bounds.y)) * half));
		num_roots		= count_bits(bit_mask(sel));

	} else {
		vec<E,2>	a		= bounds.x, b = bounds.y;
		num_roots = 1;

		if (num_roots1 == 2) {
			roots1.xy	= sort(get(roots1.xy));
			auto vals	= norm_poly_eval(k, get(roots1.xy));
			if (vals.x < zero) {
				a.x = roots1.y;
			} else if (vals.y > zero) {
				b.x = roots1.x;
			} else {
				a.y = roots1.y;
				b.x = roots1.x;
				num_roots = 2;
			}
		}

		//further subdivide intervals to guarantee convergence of halley's method by using roots of further derivatives
		int	num_roots2	= adjust_roots<E, 5, 3>(k, k2, (E*)&a, (E*)&b, num_roots);
		if (num_roots2 != 3)
			adjust_roots<E, 5, 2>(k, norm_poly_deriv(k2), (E*)&a, (E*)&b, num_roots);

		roots0 = extend_right<5>((a + b) * half);
	}

	//8 halley iterations
	for (int i = 0; i < 8; i++)
		roots0 = norm_halley<E,5>(k, k1, k2, roots0);

	roots = roots0;
	return num_roots;
}

template<typename V> int norm_poly_roots(V k, V &roots) {
	return norm_poly_roots<element_type<V>>(k, roots);
}

//-----------------------------------------------------------------------------
// poly_roots
//-----------------------------------------------------------------------------

template<typename E> int poly_roots(vec<E, 2> k, vec<E, 1> &roots) {
	if (k.y == zero)
		return 0;
	roots = -k.x / k.y;
	return 1;
}

template<typename E, typename VK, typename VR> int poly_roots(VK k, VR &roots) {
	static const int N = num_elements_v<VR>;
	if (abs(k[N]) < 1e-6f)
		return poly_roots<E>(shrink<N>(k), (vec<E, N - 1>&)roots);

	return norm_poly_roots(shrink<N>(k) / k[N], roots);
}

template<typename VK, typename VR> int poly_roots(VK k, VR &roots) {
	return poly_roots<element_type<VK>>(k, roots);
}


//-----------------------------------------------------------------------------
//	polynomial class
//-----------------------------------------------------------------------------

template<typename E, int N> struct normalised_polynomial {
	vec<E, N>	c;
	normalised_polynomial(vec<E, N> c) : c(c) {}

	template<typename T> enable_if_t<!is_mat<T>, T>	eval(const T &x) const { return norm_poly_eval(c, x); }
	template<typename T> enable_if_t<is_mat<T>, T>	eval(const T &t) const { return matrix_polynomial(*this % characteristic(t), t); }

	int	roots(vec<E, N> &r)		const { return norm_poly_roots<E>(c, r); }

	friend normalised_polynomial<E, N - 1>	normalised_deriv(const normalised_polynomial &p)	{ return norm_poly_deriv(p.c); }

	//operator polynomial<E, N>() const { return concat(c, one); }
	//polynomial<E, N> to_poly()	const { return *this; }

	template<int N2> friend normalised_polynomial<E, N2> operator%(const normalised_polynomial &num, const normalised_polynomial<E, N2> &div) {
		return norm_poly_mod_s<E, N, N2>::f(num.c, div.c);
	}
	friend vec<float, N> poly_div(int power, E val, const normalised_polynomial &div) {
		return norm_poly_div(power, val, div.c);
	}
};
template<typename V>				auto make_normalised_polynomial(V v)		{ return normalised_polynomial<element_type<V>, num_elements_v<V>>(v); }
template<typename...T>				auto make_normalised_polynomial(T&&...t)	{ return make_normalised_polynomial(concat(forward<T>(t)...)); }
template<typename E, typename...T>	auto make_normalised_polynomial(T&&...t)	{ return make_normalised_polynomial(concat<E>(forward<T>(t)...)); }

template<typename E, int N> struct polynomial {
	vec<E, N + 1> c;
	polynomial(vec<E, N + 1> c) : c(c) {}
	
	template<typename T> enable_if_t<!is_mat<T>, T>	eval(const T &x) const { return poly_eval(c, x); }
	template<typename T> enable_if_t<is_mat<T>, T>	eval(const T &t) const { return matrix_polynomial(normalise(*this) % characteristic(t), t); }

	int	roots(vec<E, N> &r)		const { return poly_roots(c, r); }

	friend normalised_polynomial<E, N>		normalise(const polynomial &p)	{ return shrink<N>(p.c) / p.c[N]; }
	friend polynomial<E, N - 1>				deriv(const polynomial &p)		{ return poly_deriv(p.c); }

	template<int N2> friend polynomial<E, max(N, N2)> operator+(const polynomial &a, const polynomial<E, N2> &b) {
		return extend_right<max(N, N2) + 1>(a.c) + extend_right<max(N, N2) + 1>(b.c);
	}
	template<int N2> friend polynomial<E, max(N, N2)> operator-(const polynomial &a, const polynomial<E, N2> &b) {
		return extend_right<max(N, N2) + 1>(a.c) - extend_right<max(N, N2) + 1>(b.c);
	}
	template<int N2> friend polynomial<E, N + N2> operator*(const polynomial &a, const polynomial<E, N2> &b) {
		return poly_mul_s<N2>::template f<E, N + 1, N2 + 1>(a.c, b.c);
	}
	template<int N2> friend auto operator%(const polynomial &num, const polynomial<E, N2> &div) {
		return normalise(num) % normalise(div);
	}

	friend vec<float, N> poly_div(int power, E val, const polynomial &div) {
		return poly_div(power, val, normalise(div));
	}
};
template<typename V>				auto make_polynomial(V v)		{ return polynomial<element_type<V>, num_elements_v<V> - 1>(v); }
template<typename...T>				auto make_polynomial(T&&...t)	{ return make_polynomial(concat(forward<T>(t)...)); }
template<typename E, typename...T>	auto make_polynomial(T&&...t)	{ return make_polynomial(concat<E>(forward<T>(t)...)); }

//-----------------------------------------------------------------------------
//	matrices
//-----------------------------------------------------------------------------

template<typename E, int N, typename T> inline T matrix_polynomial(const normalised_polynomial<E, N>& r, const T& t) {
	return norm_poly_eval(r.c, t);
}

template<typename E> normalised_polynomial<E,2>	characteristic(const mat<E,2,2> &m)			{ return vec<E,2>{m.det(), -m.trace()}; }
template<typename E> normalised_polynomial<E,2>	characteristic(const diagonal<E,2> &m)		{ return vec<E,2>{m.det(), -m.trace()}; }
template<typename E> normalised_polynomial<E,2>	characteristic(const symmetrical<E,2> &m)	{ return vec<E,2>{m.det(), -m.trace()}; }

template<typename E> normalised_polynomial<E,3>	characteristic(const mat<E,3,3> &m)			{ auto d = m.diagonal(); return -to<E>(m.det(), reduce_add(concat(m.y.x, m.z.xy) * concat(m.x.yz, m.y.z) - d.xyz * d.yzx), reduce_add(d)); }
template<typename E> normalised_polynomial<E,3>	characteristic(const diagonal<E,3> &m) 		{ return -to<E>(m.det(), reduce_add(m.d.xyz * m.d.yzx), m.trace()); }
template<typename E> normalised_polynomial<E,3>	characteristic(const symmetrical<E,3> &m)	{ return -to<E>(m.det(), reduce_add(m.o * m.o) - reduce_add(m.d.xyz * m.d.yzx), m.trace()); }

template<typename E> normalised_polynomial<E,4> characteristic(const mat<E,4,4> &m) {
	E		t1	= m.trace();
	auto	m2	= m * m;
	E		t2	= m2.trace();
	auto	m3	= m2 * m;
	E		t3	= m3.trace();
	auto	m4	= m3 * m;
	E		t4	= m4.trace();

	return to<E>(
		(t1 * (t1 * (square(t1) - t2 * 6) + t3 * 8) + square(t2) * 3 - t4 * 6) / 24,	//should=det
		(t1 * (-square(t1) + t2 * 3) - t3 * 2) / 6,
		(square(t1) - t2) / 2,
		-t1
	);
}

template<typename E> normalised_polynomial<E,4> characteristic(const diagonal<E,4> &m) {
	return {
		m.det(),
		-reduce_add(m.d * m.d.yzwx * m.d.zwxy),		//	-W X Y - W X Z - W Y Z - X Y Z,
		reduce_add(m.d.xyz * (m.d.yzw + m.d.wxy)),	//	W X + W Y + X Y + W Z + X Z + Y Z,
		-m.trace(),
	};
}

template<typename E> normalised_polynomial<E,4> characteristic(const symmetrical<E,4> &a) {
	return characteristic(mat<E,4,4>(a));
}

template<typename E, int N> vec<E,N> get_eigenvalues(const mat<E,N,N> &m) {
	vec<E,N>	r;
	int			n = characteristic(m).roots(r);
	return select(bits(n), r, zero);
}
template<typename E, int N> vec<E,N> get_eigenvalues(const symmetrical<E,N> &m) {
	vec<E,N>	r;
	int			n = characteristic(m).roots(r);
	return select(bits(n), r, zero);
}

template<typename E> vec<E,2> first_eigenvector(const symmetrical<E,2> &s) {
	vec<E,2>	r;
	int			n = characteristic(s).roots(r);

	if (n == 1)
		return x_axis;//	arbitrary axis

	if (n < 0) {
		//2 imaginary
		symmetrical<E,2>	m	= s - diagonal<E,2>(r.y);
		switch (max_component_index(abs(m))) {	// pick the first eigenvector based on biggest index
			default:return {-m.o.z, zero};
			case 1: return {zero, -m.o.y};
		}
	}

	E					e	= E(reduce_max(abs(r)));
	symmetrical<E,2>	m	= s - diagonal<E,2>(e);
	symmetrical<E,2>	u	= ~m;
	return u.column(max_component_index(abs(u)));
}

template<typename E> vec<E,3> first_eigenvector(const symmetrical<E,3> &s) {
	vec<E,3>	r;
	int			n = characteristic(s).roots(r);

	if (n == 1)
		return x_axis;//	arbitrary axis

	if (n == -1 && abs(r.y) > abs(r.x)) {
		//1 real root, 2 imaginary
		symmetrical<E,3>	m	= symmetrical<E,3>(s.d - r.y, s.o);
		switch (max_component_index(abs(m))) {	// pick the first eigenvector based on biggest index
			case 0:	case 3: return {-m.o.x, m.d.x, zero};
			case 1:	case 4:	return {zero, -m.o.y, m.d.y};
			case 5:			return {m.o.z, zero, -m.d.x};
			default:		return {zero, -m.d.z, m.o.y};
		}
	}

	E	e	= n == -1 ? r.x : E(reduce_max(abs(r)));
	return get_eigenvector(s, e);
}

template<typename E> vec<E,4> first_eigenvector(const symmetrical<E,4> &s) {
	vec<E,4>	r;
	int	n = characteristic(s).roots(r);
	if (n == 0)
		return vec<E,4>(zero);
	return get_eigenvector(s, reduce_max(abs(r)));
}


template<typename E, int N> diagonal<E,N> diagonalise(const symmetrical<E,N> &s) {
	vec<E,N>	r;
	characteristic(s).roots(r);
	return diagonal<E,N>(r);
}

template<typename E> mat<E,2,2> pow(const mat<E,2,2> &x, int y) {
	auto	r = poly_div(y, 1, characteristic(x));
	return x * r.y + diagonal<E,2>(r.x);
}

template<typename E> mat<E,3,3> pow(const mat<E,3,3> &x, int y) {
	vec<E,3>	r = poly_div(y, 1, characteristic(x));
	return x * x * r.z + x * r.y + diagonal<E,3>(r.x);
}

template<typename E> mat<E,4,4> pow(const mat<E,4,4> &x, int y) {
	vec<E,4>	r	= poly_div(y, 1, characteristic(x));
	mat<E,4,4>	x2	= x * x;
	return (x2 * x) * r.w + x2 * r.z + x * r.y + diagonal<E,4>(r.x);
}

//-----------------------------------------------------------------------------
// dynamic_polynomial
//-----------------------------------------------------------------------------

template<typename T> struct dynamic_polynomial {
	ref_array<T>		p;
	uint32				size;

	dynamic_polynomial(uint32 _size) : p(_size), size(_size) {}

	int			degree()	const	{
		int i = size;
		while (i && !p[i - 1])
			--i;
		return i - 1;
	}

	const T		*begin()	const	{ return p; }
	const T		*end()		const	{ return p + size; }
	T			*begin()			{ return p; }
	T			*end()				{ return p + size; }

	operator uint32 dynamic_polynomial::*()	const { return degree() < 0 ? 0 : &dynamic_polynomial::size; }
	bool	operator!()				const { return degree() < 0; }

	friend dynamic_polynomial	unique(const dynamic_polynomial &a)	{
		dynamic_polynomial r(a);
		r.p.grow(a.size);
		return r;
	}

	template<typename B> bool operator< (dynamic_polynomial<B> &b)	{ return size <  b.size; }
	template<typename B> bool operator<=(dynamic_polynomial<B> &b)	{ return size <= b.size; }
	template<typename B> bool operator> (dynamic_polynomial<B> &b)	{ return size >  b.size; }
	template<typename B> bool operator>=(dynamic_polynomial<B> &b)	{ return size >= b.size; }
};

template<typename T1, typename T2> dynamic_polynomial<T1> operator+(const dynamic_polynomial<T1> &a, const dynamic_polynomial<T2> &b) {
	dynamic_polynomial<T1>	r(max(a.size, b.size));
	const T1	*ap		= a.p;
	const T2	*bp		= b.p;
	T1			*rp		= r.p;
	for (T1 *ep = rp + min(a.size, b.size); rp < ep; ++ap, ++bp, ++rp)
		*rp = *ap + *bp;
	if (a.size < b.size)
		ap = bp;
	for (T1 *ep = r.p + r.size; rp < ep; ++ap, ++rp)
		*rp = *ap;
	return r;
}

template<typename T1, typename T2> dynamic_polynomial<T1> operator-(const dynamic_polynomial<T1> &a, const dynamic_polynomial<T2> &b) {
	dynamic_polynomial<T1>	r(max(a.size, b.size));
	const T1	*ap		= a.p;
	const T2	*bp		= b.p;
	T1			*rp		= r.p;
	for (T1 *ep = rp + min(a.size, b.size); rp < ep; ++ap, ++bp, ++rp)
		*rp = *ap - *bp;
	if (a.size < b.size) {
		for (T1 *ep = r.p + r.size; rp < ep; ++bp, ++rp)
			*rp = -*bp;
	} else {
		for (T1 *ep = r.p + r.size; rp < ep; ++ap, ++rp)
			*rp = *ap;
	}
	return r;
}

template<typename T1, typename T2> dynamic_polynomial<T1> divmod(dynamic_polynomial<T1> &a, const dynamic_polynomial<T2> &b) {
	const T2	*b0	= b.begin(), *bi = b.end();
	while (bi > b0 && !bi[-1])
		--bi;

	T1	*a0	= a.begin(), *ai = a.end();
	while (ai > a0 && !ai[-1])
		--ai;

	uint32	bsize = uint32(bi - b0);
	uint32	asize = uint32(ai - a0);
	if (bsize > asize)
		return dynamic_polynomial<T1>(0);

	dynamic_polynomial<T1>	d(asize - bsize + 1);
	a.p.grow(asize);

	T2	top		= bi[-1];
	for (T1 *di = d.end(); ai > a0; --ai, --di) {
		T1	d	= ai[-1] / top;
		T2	*bj	= bi;
		for (T1 *aj = ai; aj > a0; --aj, --bj)
			aj[-1] -= bj[-1] * d;
		di[-1]	= d;
	}

	return d;
}

template<typename T1, typename T2> dynamic_polynomial<T1> divmod(const dynamic_polynomial<T1> &a, const dynamic_polynomial<T2> &b, dynamic_polynomial<T2> &mod) {
	return divmod(mod = a, b);
}

template<typename T1, typename T2> inline dynamic_polynomial<T1> operator/(const dynamic_polynomial<T1> &a, const dynamic_polynomial<T2> &b) {
	dynamic_polynomial<T1> a2	= a;
	return divmod(a2, b);
}

template<typename T1, typename T2> inline dynamic_polynomial<T1> operator%(const dynamic_polynomial<T1> &a, const dynamic_polynomial<T2> &b) {
	dynamic_polynomial<T1> a2	= a;
	divmod(a2, b);
	return a2;
}

template<typename T> dynamic_polynomial<T> gcd(const dynamic_polynomial<T> &_a, const dynamic_polynomial<T> &_b) {
	dynamic_polynomial<T>	a = _a;
	dynamic_polynomial<T>	b = _b;
	if (a > b)
		swap(a, b);
	if (!a)
		return b;

	while (T r = b % a) {
		b = a;
		a = r;
	}

	return a;
}

#ifdef ISO_TEST

struct test_polynomial {
	test_polynomial() {
		auto	p5 = normalised_polynomial<float,1>(1).to_poly()
			* normalised_polynomial<float,1>(7).to_poly()
			* normalised_polynomial<float,1>(3).to_poly()
			* normalised_polynomial<float,1>(4).to_poly()
			* normalised_polynomial<float,1>(5).to_poly();

		vec<float, 5>	roots;
		int		nroots = p5.roots(roots);
		ISO_VERIFY(nroots == 5 && approx_equal(roots, vec<float,5>(1,3,4,5,7));
	}
} _test_polynomial;

#endif

} // namespace iso

#endif //POLYNOMIAL_H
