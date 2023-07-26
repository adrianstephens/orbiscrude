#ifndef BEZIER_H
#define BEZIER_H

#include "maths/geometry.h"
#include "maths/polynomial.h"
#include "maths/polygon.h"

namespace iso {


template<typename T, int N> class bezier_splineT;
template<typename T, int N, typename C = dynamic_array<T>> struct bezier_chain;

//-----------------------------------------------------------------------------
// bezier_helpers - for degree-specific stuff
//-----------------------------------------------------------------------------

template<int N> struct bezier_helpers {
	static	mat<float, N+1, N+1>	blend;
	static	mat<float, N+1, N>		tangentblend;

	template<typename T> static constexpr T mid(const T *b);
	template<typename T> static constexpr T mid_tan(const T *b);

	template<typename T> static void		relaxed(T* d, const T* points, bool first);
	template<typename T> static void		interpolating(T* d, range<const T*> points);
};

template<> template<typename T> static constexpr T bezier_helpers<2>::mid(const T *b)		{ return (b[0] + b[1] * 2 + b[2]) / 4; }
template<> template<typename T> static constexpr T bezier_helpers<2>::mid_tan(const T *b)	{ return (b[2] - b[0]) * half; }

template<> template<typename T> static constexpr T bezier_helpers<3>::mid(const T *b)		{ return (b[0] + (b[1] + b[2]) * 3 + b[3]) / 8; }
template<> template<typename T> static constexpr T bezier_helpers<3>::mid_tan(const T *b)	{ return (b[2] + b[3] - b[0] - b[1]) * (3.f / 4); }


template<typename L, size_t...I>	static auto	bezier_maker(L &&lambda, index_list<I...>)->bezier_splineT<decltype(lambda(0)), sizeof...(I) - 1> { return {lambda(I)...}; }
template<size_t N, typename L>		static auto	bezier_maker(L &&lambda) { return bezier_maker(forward<L>(lambda), meta::make_index_list<N+1>()); }

//-----------------------------------------------------------------------------
// bezier_spline
//-----------------------------------------------------------------------------

template<typename T, int N> class bezier_splineT {
protected:
	array<T, N + 1>	c;
	template<size_t... I> bezier_splineT<T, N+1>	_elevate(index_list<I...>)	const	{
		return {c[0], ((c[I] * (I + 1) + c[I + 1] * (N - I)) / (N + 1))..., c[N]};
	}

public:
	static const int			M = num_elements_v<T>;
	typedef element_type<T>		E;
	typedef mat<E, M, N + 1>	MT;

	constexpr				bezier_splineT() {}
	template<typename P, size_t...I>	constexpr bezier_splineT(const P &p, index_list<I...>) : c{p[I]...} {}
	constexpr				bezier_splineT(const MT &m) : bezier_splineT(m, meta::make_index_list<N+1>()) {}
	constexpr				bezier_splineT(initializer_list<T> p) : c(p) {}

	const T&				operator[](int i)			const	{ return c[i]; }
	T&						operator[](int i)					{ return c[i]; }
	constexpr polynomial<T,N>	spline()				const	{ return (MT)((MT&)c * bezier_helpers<N>::blend); }
	constexpr polynomial<T,N-1>	tangent_spline()		const	{ return (MT&)c * bezier_helpers<N>::tangentblend; }

	template<typename X> constexpr T	evaluate(X x)	const	{ return spline()(x); }
	template<typename X> constexpr T	tangent(X x)	const	{ return tangent_spline()(x); }

	constexpr T				evaluate(decltype(zero))	const	{ return c[0]; }
	constexpr T				evaluate(decltype(one))		const	{ return c[N]; }
	constexpr T				evaluate(decltype(half))	const	{ return bezier_helpers<N>::mid(c.begin()); }
	constexpr T				tangent(decltype(zero))		const	{ return (c[1] - c[0]) * N; }
	constexpr T				tangent(decltype(one))		const	{ return (c[N] - c[N - 1]) * N; }
	constexpr T				tangent(decltype(half))		const	{ return bezier_helpers<N>::mid_tan(c.begin()); }

	auto					get_box()					const	{ return get_extent(c); }
	auto					corner(CORNER i)			const	{ return c[(i & 1) * N]; }
	template<int J> auto	elevate()					const	{ return _elevate(meta::make_index_list<N>()).template elevate<J - 1>(); }
	template<>		auto	elevate<0>()				const	{ return *this; }
	bezier_splineT			flip()						const	{ return bezier_maker<N>([this](int i) { return c[N - i]; }); }
	float					support_param(T v)			const	{ return maxima_from_deriv(dot(tangent_spline(), v), zero, one); }
	T						support(T v)				const	{ return evaluate(support_param(v)); }

	T						closest(T v, float *_t, float *_d)						const;
	bool					ray_check(param(ray2) r, float &t, vector2 *normal)		const;

	template<int MN> friend auto operator*(const mat<E, MN, M> &m, const bezier_splineT& b) {
		return bezier_maker<N>([&b, &m](int i) { return m * b[i];});
	}
	template<int MN> friend auto operator*(const mat<E, MN, M+1> &m, const bezier_splineT& b) {
		return bezier_maker<N>([&b, &m](int i) { return as_vec(m * pos<E,M>(b[i]));});
	}
	friend bool	operator==(const bezier_splineT &b1, const bezier_splineT &b2) {
		return b1.c == b2.c;
	}
	friend auto	make_bezier_chain(const bezier_splineT<T, N> &c) { return bezier_chain<T, N>(c.c); }
};

template<int N> auto inflection(const bezier_splineT<float2, N> &b) {
	vec<float, N * 2 - 4>	roots;
	int	n = inflections_from_tangent(b.tangent_spline(), roots);
	return select(bits(n), roots, nan);
}

template<typename T, int N> T bezier_splineT<T, N>::closest(T pos, float *_t, float *_d) const {
	auto	s	= spline() - pos;
	vec<float, N * 2 - 1>	roots;
#if 1
	int		num_roots = dot(s, deriv(s)).roots(roots);
#else
	auto	x	= dot(s, x_axis);
	auto	y	= dot(s, y_axis);
	int		num_roots = deriv(x * x + y * y).roots(roots);
#endif

	roots = clamp(roots, zero, one);

	float	t	= roots.x;
	auto	v	= s(roots.x);
	float	d	= len2(v);

	for (int i = 1; i < num_roots; i++) {
		float	t1	= roots[i];
		auto	v1	= s(t1);
		float	d1	= len2(v1);
		if (d1 < d) {
			v	= v1;
			d	= d1;
			t	= t1;
		}
	}

	if (_t)
		*_t = t;
	if (_d)
		*_d = sqrt(d);
	return v + pos;
}

template<typename T, int N> bool bezier_splineT<T, N>::ray_check(param(ray2) r, float &t, vector2 *normal) const {
	auto	s	= (r.inv_matrix() * *this).spline();
	auto	x	= dot(s, x_axis);
	auto	y	= dot(s, y_axis);

	vec<float,N>	roots;
	auto	num_roots	= x.real_roots(roots);
	float	minr, miny	= maximum;

	for (int i = 0; i < num_roots; i++) {
		float	r = roots[i];
		if (between(r, zero, one)) {
			float	t = y.eval(r);
			if (between(t, zero, one) && t < miny) {
				miny	= t;
				minr	= r;
			}
		}
	}
	if (miny == float(maximum))
		return false;

	t = miny;
	if (normal)
		*normal = perp(tangent_spline()(minr));
	return true;
}

//-----------------------------------------------------------------------------
// chain helpers
//-----------------------------------------------------------------------------

template<typename T, int N> T *reduce_spline(const bezier_splineT<T, N> &b, T *p, T *end, float tol);

template<typename C, typename T> T closestc(const C &cont, T v, float* _t, float* _d) {
	T		c;
	float	d	= maximum, t = 0;
	for (auto &i : cont) {
		float	d2, t2;
		auto	c2 = i.closest(v, &t2, &d2);
		if (d2 < d) {
			t = t2;
			d = d2;
			c = c2;
		}
	}
	if (_t)
		*_t = t;
	if (_d)
		*_d = d;
	return c;
}

template<typename C, typename R, typename V = typename R::D> bool ray_checkc(const C &cont, const R &r, float &t, V *normal) {
	bool	got	= false;
	for (auto &i : cont) {
		float	t2;
		V		normal2;
		if (i.ray_check(r, t2, normal ? &normal2 : nullptr)) {
			if (!got || t2 < t) {
				got	= true;
				t	= t2;
				if (normal)
					*normal = normal2;
			}
		}
	}
	return got;
}

template<typename C, typename T, typename R = decltype(declval<C>().evaluate(0))> R supportc(const C& cont, T v) {
	R	c = R(T(nan));
	for (auto &i : cont) {
		float	r	= i.support_param(v);
		if (!is_nan(r)) {
			R	c2 = i.evaluate(r);
			if (is_nan(c) || dot(c2 - c, v) > zero)
				c = c2;
		}
	}
	return c;
}

//-----------------------------------------------------------------------------
// bezier_chain
//-----------------------------------------------------------------------------

template<typename T, int N, typename C> struct bezier_chain {
	typedef bezier_splineT<T, N>	S;

	struct iterator {
		const T	*c;
		iterator(const T *c) : c(c) {}
		auto&	operator++()						{ c += N; return *this; }
		auto&	operator*()			const			{ return *(const S*)c; }
		bool	operator!=(const iterator &b) const { return c != b.c; }
		iterator operator+(int i)	const			{ return c + i * N; }
	};
	C	c;

	static bezier_chain relaxed(range<const T*> points);
	static bezier_chain interpolating(range<const T*> points);

	bezier_chain()										{}
	bezier_chain(C &&c)				: c(forward<C>(c))	{}

	bool		loop()			const	{ return num_elements(c) % N == 0; }
	auto		get_box()		const	{ return get_extent(c); }
	size_t		size()			const	{ return num_elements(c) / N; }
	iterator	begin()			const	{ using iso::begin; return begin(c); }
	iterator	end()			const	{ return begin() + size(); }
	auto&	operator[](int i)	const	{ return *(begin() + i); }
	auto		evaluate(float t)const	{ return (*this)[floor(t)].evaluate(frac(t)); }

	template<int J> bezier_chain<T, N + J>	elevate()								const;
	bezier_chain<T, N - 1>					reduce(int max_per_curve, float tol)	const;
	bezier_chain							reverse()	const	{ return (C)reversed(c); }

	T		support(T v)											const	{ return supportc(*this, v); }
	T		closest(T v, float* _t, float* _d)						const	{ return closestc(*this, v, _t, _d); }
	bool	ray_check(param(ray2) r, float &t, vector2 *normal)		const	{ return ray_checkc(*this, r, t, normal); }

	void	add(S* s, int n) {
		auto	i	= size();
		c.resize((i + n) * N + 1);
		while (n--)
			(S&)c[i++ * N] = *s++;
	}
	template<typename MT, typename=enable_if_t<is_mat<MT>>> friend auto operator*(const MT &m, const bezier_chain& p) {
		bezier_chain	result;
		dynamic_array<S>	temp =  transformc(p, [m](const S &s) { return m * s; });
		result.add(temp, temp.size());
		return result;
	}
};

template<int N, typename C> bezier_chain<element_t<C>, N, C>	make_bezier_chain(C &&c) { return c; }

template<typename T, int N, typename C> template<int J> bezier_chain<T, N + J>	bezier_chain<T, N, C>::elevate()	const	{
	C		result(size() * (N + J) + 1);
	T		*out = result.begin();
	for (auto &i : *this) {
		copy_n(&i.template elevate<J>()[0], out, N + J);
		out	+= N + J;
	}
	result.back() = c.back();
	return move(result);
}

template<typename T, int N, typename C> bezier_chain<T, N - 1> bezier_chain<T, N, C>::reduce(int max_per_curve, float tol) const {
	temp_array<T>	temp(size() * max_per_curve * (N - 1) + 1);
	T	*p	= temp;
	*p++	= c[0];
	for (auto& i : *this)
		p = reduce_spline(i, p, p + max_per_curve * (N - 1), tol);
	return bezier_chain<T, N - 1>(temp.slice_to(p));
}

template<> template<typename T> void bezier_helpers<2>::relaxed(T* d, const T* points, bool first) {
	d[0] = first ? points[0] : T((points[0] + points[-1]) / 2);
	d[1] = points[0];
}
template<> template<typename T> void bezier_helpers<2>::interpolating(T* d, range<const T*> points) {
	uint32	n = points.size32();
	for (int i = 1; i < n - 1; ++i)
		d[i] = points[i] * 2 - d[i - 1];
}

template<> template<typename T> void bezier_helpers<3>::relaxed(T* d, const T* points, bool first) {
	d[0] = first ? points[0] : T(points[-1] / 6 + points[0] * 2 / 3 + points[1] / 6);
	d[1] = (points[0] * 2 + points[1]) / 3;
	d[2] = (points[0] + points[1] * 2) / 3;
}
template<> template<typename T> void bezier_helpers<3>::interpolating(T* d, range<const T*> points) {
	uint32	n = points.size32();
	for (int i = 1; i < n - 1; i++)
		d[i] = points[i] * 6;

	d[n - 2]	-= points[n - 1];

	temp_array<float>	m(n - 2);
	// Forward sweep
	float	m0	= 0;
	for (int i = 0; i < n - 2; i++) {
		m[i]		= m0 = 1 / (4 - m0);
		d[i + 1]	= (d[i + 1] - d[i]) * m0;
	}
	// Back substitution
	for (int i = n - 3; i > 0; i--)
		d[i] -= d[i + 1] * m[i - 1];
}

template<typename T, int N, typename C> bezier_chain<T, N, C> bezier_chain<T, N, C>::relaxed(range<const T*> points) {
	uint32		n = points.size32() - 1;
	C			c(n * N + 1);

	if (n == 1) {
		for (int i = 1; i < N; i++)
			c[i] = points[0];

	} else {
		for (int i = 0; i < n; i++)
			bezier_helpers<N>::relaxed(c + i * N, points.begin() + i, i == 0);
	}
	c.back()	= points.back();
	return move(c);
}

template<typename T, int N, typename C> bezier_chain<T, N, C> bezier_chain<T, N, C>::interpolating(range<const T*> points) {
	uint32	n = points.size32();
	if (n < N)
		return relaxed(points);

	temp_array<T>	d(n);
	d[0]		= points[0];
	d[n - 1]	= points[n - 1];
	bezier_helpers<N>::interpolating(d.begin(), points);

	return relaxed(d);
}

template<typename T, typename E> auto split(const bezier_splineT<T,2> &b, E t) {
	auto	p0	= lerp(b[0],	b[1],	t),
			p1	= lerp(b[1],	b[2],	t),
			cs	= lerp(p0,		p1,		t);
	return bezier_chain<T, 2, array<T, 2 * 2 + 1>>({b[0], p0, cs, p1, b[2]});
}

template<typename T, typename E> auto split(const bezier_splineT<T,3> &b, E t) {
	auto	h	= lerp(b[1],	b[2],	t),
			cl1 = lerp(b[0],	b[1],	t),
			cr2 = lerp(b[2],	b[3],	t),
			cl2 = lerp(cl1,		h,		t),
			cr1 = lerp(h,		cr2,	t),
			cs	= lerp(cl2,		cr1,	t);
	return bezier_chain<T, 3, array<T, 2 * 3 + 1>>({b[0], cl1, cl2, cs, cr1, cr2, b[3]});
}

template<typename T, int N> auto split(const bezier_splineT<T,N> &b) {
	return split(b, half);
}

template<typename T, int N, typename C> float len(const bezier_chain<T,N,C> &chain, int level = 3) {
	float	total = zero;
	for (auto &i : chain)
		total += len(i, level);
	return total;
}

template<typename T, int N> float len(const bezier_splineT<T,N> &b, int level = 3) {
	return level
		? len(split(b), level - 1)
		: (path_len(&b[0], &b[N]) + len(b[N] - b[0])) * half;
}

//-----------------------------------------------------------------------------
// bezier_patch
//-----------------------------------------------------------------------------

template<typename T, int N, int M> class bezier_splineT<bezier_splineT<T, N>, M> {
	typedef element_type<T>	E;
	array<bezier_splineT<T, N>, M + 1> c;

public:

	force_inline bezier_splineT()	{}
//	template<typename...P>	constexpr bezier_splineT(const P&...p) : c{p...} {}
	constexpr bezier_splineT(initializer_list<bezier_splineT<T, N>> p) : c(p) {}

	auto&					operator[](int i)const	{ return c[i]; }
	force_inline auto&		row(int i)		const	{ return c[i]; }
	force_inline auto		col(int i)		const	{ return bezier_maker<M>([this, i](int j) { return c[j][i]; }); }
	force_inline auto&		bottom()		const	{ return row(0); }
	force_inline auto		top()			const	{ return row(M).flip(); }
	force_inline auto		left()			const	{ return col(0).flip(); }
	force_inline auto		right()			const	{ return col(N); }
	force_inline T&			cp(int i)				{ return c[i / N][i % N];}
	force_inline const T&	cp(int i)		const	{ return c[i / N][i % N];}

	template<typename X>			auto	evaluate_u(X x)		const	{ return bezier_maker<M>([this, x](int i) { return row(i).evaluate(x); }); }
	template<typename X>			auto	evaluate_v(X x)		const	{ return bezier_maker<N>([this, x](int i) { return col(i).evaluate(x); }); }
	template<typename U, typename V> auto	evaluate(U u, V v)	const	{ return evaluate_u(u).evaluate(v);	}
	template<typename U, typename V> auto	normal(U u, V v)	const	{ return cross(evaluate_v(v).tangent(u), evaluate_u(u).tangent(v));	}
	force_inline auto		evaluate(param(float2) uv)			const	{ return evaluate(uv.x, uv.y);	}
	force_inline auto		normal(param(float2) uv)			const	{ return normal(uv.x, uv.y);	}

	bool					ray_check(param(ray3) r, float &t, vector3 *normal)						const;
	bool					test(param(sphere) s, float &t, vector3 *normal)						const;
	bool					test(param(obb3) obb, float &t, position3 *position, vector3 *normal)	const;

	auto					corner(CORNER i)			const	{ return c[(i & 1) * N][i & 2 ? M : 0]; }
	auto					get_box()					const	{ return c[0].get_box() | c[1].get_box() | c[2].get_box() | c[3].get_box(); }

	template<typename MT, typename=enable_if_t<is_mat<MT>>> friend auto operator*(const MT &m, const bezier_splineT& p) {
//	template<int MN, int MM> friend auto operator*(const mat<E, MN, MM> &m, const bezier_splineT& p) {
		return bezier_maker<N>([&p, &m](int i) { return m * p[i]; });
	}
};

template<typename T, int N, int M> using bezier_patchT = bezier_splineT<bezier_splineT<T, N>, M>;

template<int N, typename T> auto	make_bezier_spline(T a, T b) {
	return bezier_maker<N>([&](int i) {
		return lerp(a, b, uint2float(i) / N);
	});
}

template<int N, int M> auto	make_bezier_patch(const quadrilateral3 &q) {
	return bezier_maker<M>([&q](int i) {
		float	t = uint2float(i) / M;
		return make_bezier_spline<N>(
			as_vec(q.from_parametric(float2{t, zero})),
			as_vec(q.from_parametric(float2{t, one}))
		);
	});
}

template<int N, int M, int L> auto	make_bezier_space(const polytope8 &p) {
	return bezier_maker<L>([&p](int i) {
		float	t = uint2float(i) / M;
		return make_bezier_patch<M, N>(quadrilateral3(
			p.from_parametric(float3{t, zero,	zero}),
			p.from_parametric(float3{t, one,	zero}),
			p.from_parametric(float3{t, zero,	one}),
			p.from_parametric(float3{t, one,	one})
		));
	});
}

template<typename T, int N, int M> bool calc_params_nr(const bezier_patchT<T, N, M> &patch, identity_t<T> p, T &result, vector3 *normal, float2 uv) {
	for (int i = 0; i < 10; i++) {
		bool		last	= i == 9;
		auto		bezu	= patch.evaluate_u(uv.x);
		auto		bezv	= patch.evaluate_v(uv.y);

		auto		vp		= bezu.evaluate(uv.y);
		auto		du		= bezv.tangent_spline()(uv.x).xyz;
		auto		dv		= bezu.tangent_spline()(uv.y).xyz;
		auto		off		= p - vp;

		if (len2(du) == zero)
			du = (bezv[3] - bezv[0]).xyz;
		if (len2(dv) == zero)
			dv = (bezu[3] - bezu[0]).xyz;

		auto		n	= normalise(cross(du, dv));
		if (abs(abs(dot(normalise(off), n)) - one) < (last ? .02f : .001f)) {
			if (all(uv >= select(last, -0.1f, zero)) && all(uv <= select(last, 1.1f, one))) {
				result	= vp;//concat(uv, dot(off, n));
				if (normal)
					*normal	= n;
				return true;
			}
		}

		float2 dotuv	= {dot(off, du), dot(off, dv)};
		uv += select(dotuv != zero, dotuv / (len2(du), len2(dv)), zero);

		if (any(uv < -one) || any(uv > 2))
			break;
	}
	return false;
}

template<typename T, int N, int M> float2 estimate_bezier_patch_params(const bezier_patchT<T, N, M> &patch, param(position2) pos) {
	return quadrilateral(
		position2(patch[0][0].xy),
		position2(patch[0][N].xy),
		position2(patch[M][0].xy),
		position2(patch[M][N].xy)
	).parametric(pos);
}

//-----------------------------------------------------------------------------
// bezier_grid
//-----------------------------------------------------------------------------

template<typename T, int N, int M, typename C = dynamic_array<bezier_chain<T,N>>> struct bezier_grid {
	typedef bezier_patchT<T, N, M>	P;
	
	struct row {
		iterator_t<const C>	c;
		row(iterator_t<const C> c) : c(c) {}
		P	operator[](int i)	const	{ return {c[0][i], c[1][i], c[2][i], c[3][i]}; }
	};
	struct iterator : row {
		using row::row;
		using row::c;
		auto&		operator++()						{ c += M; return *this; }
		const row&	operator*()			const			{ return *this; }
		bool		operator!=(const iterator &b) const { return c != b.c; }
		iterator	operator+(int i)	const			{ return c + i * N; }
	};

	C	c;
	bezier_grid()		{}
	bezier_grid(C &&c)					: c(forward<C>(c)) {}
	size_t		size()			const	{ using iso::size; return size(c) / M; }
	iterator	begin()			const	{ using iso::begin; return begin(c); }
	iterator	end()			const	{ return begin() + size(); }
	auto&	operator[](int i)	const	{ return *(begin() + i); }

};

template<typename T, int N, int M, typename U, typename V> auto split(const bezier_patchT<T,N,M> &patch, U u, V v) {
	// subdivide to 7x4
	typedef bezier_chain<T, M, array<T, M * 2 + 1>>	chain;
	chain	gridx[N + 1];

	for (int i = 0; i < N + 1; i++)
		gridx[i] = split(patch.col(i), v);

	// subdivide the other way to reach 7x7
	bezier_grid<T, N, M, array<chain, M * 2 + 1>>	grid;

	for (int i = 0; i < M * 2 + 1; i++) {
		bezier_splineT<T, N>	p = bezier_maker<N>([&](int j) { return gridx[j].c[i]; });
		grid.c[i] = split(p, u);
	}
	return grid;
}

template<typename T, int N, int M> bool check_inside_xy_hull(const bezier_patchT<T, N, M> &patch, position2 pos) {
	return check_inside_hull(make_range_n(strided((position2*)&patch, sizeof(T)), (N + 1) * (M + 1)), position2(zero));
}

template<typename T, int N, int M> bool bezier_patchT<T, N, M>::ray_check(param(ray3) r, float &t, vector3 *normal) const {
	if (len2(r.d) == zero)
		return false;

	bezier_patchT<float3, N, M>	stack[32], *patch = stack;
	*patch	= r.inv_matrix() * *this;

	auto	ext = patch->get_box();
	if (any(ext.a.xy > zero) || any(ext.b.xy < zero))
		return false;

	if (!check_inside_xy_hull(*patch, position2(zero)))
		return false;

	float	maxsize	= len2(r.d) * 1000000.f;
	float3	result;
	if (len2(ext.extent().xy) < maxsize) {
		float2	uv	= estimate_bezier_patch_params(*patch, position2(zero));
		if (calc_params_nr(*patch, zero, result, normal, uv)) {
			if (any(result.xy < zero) || any(result.xy > one)) {
				auto	p = evaluate(clamp(result.xy, zero, one));
				if (len(p.xy - r.p.v.xy) > 0.1f)
					return false;
			}
			t = result.z / len(r.d);
			if (normal)
				*normal = r.matrix() * *normal;
			ISO_ASSERT(!is_nan(*normal));
			return true;
		}
	}

	do {
		auto	pq = split(*patch, half, half);
		// push appropriate subpatch(es) on stack
		for (int i = 0; i < 4; i++) {
			*patch = pq[i & 1][i >> 1];
			if (check_inside_xy_hull(*patch, position2(zero))) {
				if (len2((*patch)[M][N].xy - (*patch)[0][0].xy) < maxsize) {
					// try Newton Raphson
					float2	uv = clamp(estimate_bezier_patch_params(*patch, position2(zero)), zero, one);
					if (calc_params_nr(*patch, zero, result, normal, uv)) {
						t = result.z / len(r.d);
						if (normal)
							*normal = r.matrix() * *normal;
						return true;
					}
				}
				patch++;
				ISO_ASSERT(patch < &stack[32]);
			}
		}
	} while (patch-- > stack);
	return false;
}

template<typename T, int N, int M> bool bezier_patchT<T, N, M>::test(param(sphere) s, float &t, vector3 *normal) const {
	float4	result;
	if (calc_params_nr(*this, s.centre(), result, normal, float2(half)) && abs(result.z) < s.radius()) {
		t = s.radius() - result.z;
		return true;
	}
	return false;
}

template<typename T, int N, int M> plane rough_plane(const bezier_patchT<T, N, M> &patch) {
	return {position3(patch[0][0]), position3(patch[0][N]), position3(patch[M][0])};
};

template<typename T, int N, int M> float2 plane_distances(const bezier_patchT<T, N, M> &patch, plane p) {
	float2	ds	= zero;
	for (int i = 0; i < M; i++) {
		for (int j = 0; j < N; j++)
			ds	= max(ds, plus_minus(p.dist(position3(patch[i][j]))));
	}
	return {ds.x, -ds.y};
}

template<typename T, int N, int M> bool clipped_patch_centre(const bezier_patchT<T, N, M> &patch, position3 &centre) {
	position3	clipped[10];
	clipped[0]	= position3(patch[0][0]);
	clipped[1]	= position3(patch[0][N]);
	clipped[2]	= position3(patch[M][N]);
	clipped[3]	= position3(patch[M][0]);

	if (auto n = clip_poly(clipped, clipped + 4, clipped) - clipped) {
		centre = centroid(clipped, clipped + n);
		return true;
	}
	return false;
}


template<typename T, int N, int M> bool bezier_patchT<T, N, M>::test(param(obb3) obb, float &t, position3 *position, vector3 *normal) const {
	bezier_patchT<T, N, M>	stack[32], *patch = stack;
	float3x4		im = obb.inv_matrix();
	*patch = (float4x4)im * *this;

	cuboid	ext = patch->get_box();
	if (!overlap(ext, unit_cube))
		return false;

	plane	p		= rough_plane(*patch);
	float2	ds		= plane_distances(*patch, p);

	if (reduce_add(abs(p.normal())) < -(p.dist() + ds.x))
		return false;

	position3	centre;
	if (ds.x - ds.y < 0.1f && clipped_patch_centre(*patch, centre)) {
		float	x	= reduce_min(float3(centre - sign1(p.normal())) / p.normal());
		*normal		= normalise((p / im).normal());
		*position	= obb.matrix() * centre;
		t			= len(obb.matrix() * p.normal()) * x;
		return true;
	}

	t = 0;
	do {
		auto	pq = split(*patch, half, half);
		for (int i = 0; i < 4; i++) {
			*patch = pq[i & 1][i >> 1];
			cuboid	ext = patch->get_box();
			if (overlap(ext, unit_cube)) {
				plane	p		= rough_plane(*patch);
				float2	ds		= plane_distances(*patch, p);
				if (reduce_add(abs(p.normal())) < -(p.dist() + ds.x))
					continue;

				position3	centre;
				if (ds.x - ds.y < 0.1f && clipped_patch_centre(*patch, centre)) {
					vector3	c	= -sign1(p.normal());
					float	x	= reduce_min(select(p.normal() == zero, 1e6f, (centre - c) / p.normal()));
					float	pen	= len(obb.matrix() * p.normal()) * x;
					if (pen > t) {
						*normal		= normalise((p / im).normal());
						*position	= obb.matrix() * centre;
						t			= pen;
					}
					continue;
				}
				patch++;
				ISO_ASSERT(patch < &stack[32]);
			}
		}
	} while (patch-- > stack);
	return t != 0;
}

//-----------------------------------------------------------------------------
// bezier_space
//-----------------------------------------------------------------------------
#if 0
template<typename T, int N, int M, int L> class bezier_splineT<bezier_splineT<bezier_splineT<T, N>, M>, L> {
//template<typename T, int N, int M, int L> class bezier_splineT<bezier_patchT<T, N, M>, L> {
	typedef element_type<T>	E;
	bezier_splineT<T, N>	c[M + 1];

public:

	force_inline bezier_splineT()	{}
	template<typename...P>	constexpr bezier_splineT(const P&...p) : c{p...} {}

	auto&					operator[](int i)const	{ return c[i]; }
	force_inline auto&		slice(int i)	const	{ return c[i]; }

	template<typename X>	auto	evaluate_u(X x)		const	{ return bezier_maker<M>([this, x](int i) { return slice(i).evaluate_u(x); }); }
	template<typename U, typename V, typename W> auto	evaluate(U u, V v, W w)	const	{ return evaluate_u(u).evaluate(v, w);	}
	force_inline auto		evaluate(param(float3) t)	const	{ return evaluate(t.x, t.y, t.z);	}

	auto					corner(CORNER i)			const	{ return c[(i & 1) * N][i & 2 ? M : 0][i & 4 ? L : 0]; }
	auto					get_box()					const	{ return c[0].get_box() | c[1].get_box() | c[2].get_box() | c[3].get_box(); }

	template<typename MT, typename=enable_if_t<is_mat<MT>>> friend auto operator*(const MT &m, const bezier_splineT& p) {
		return bezier_maker<N>([&p, &m](int i) { return m * p[i]; });
	}
};
#endif
//template<typename T, int N, int M, int L> using bezier_spaceT = bezier_splineT<bezier_patchT<T, N, M>, L>;
template<typename T, int N, int M, int L> using bezier_spaceT = bezier_splineT<bezier_splineT<bezier_splineT<T, N>, M>, L>;

typedef bezier_splineT<float4, 3>	bezier_spline;
typedef bezier_patchT<float4, 3, 3>	bezier_patch;

// [https://en.wikipedia.org/wiki/Centripetal_Catmull%E2%80%93Rom_spline]
// [https://stackoverflow.com/questions/30748316/catmull-rom-interpolation-on-svg-paths/30826434#30826434]
template<typename T, typename C = dynamic_array<T>> bezier_chain<T, 3, C> catmull_rom_to_bezier(range<const T*> points, float alpha, const T* first, const T* last, float epsilon = 1e-4) {
	// Remove redundant points
	temp_array<T>	cr(points.size() + 2);
	T	*p	= cr;
	*++p	= points[0];
	for (auto &p1 : points.slice(1)) {
		if (len(p1 - *p) > epsilon)
			*++p = p1;
	}

	// Add or generate first and last
	cr[0]	= first && len(*first - cr[1]) > epsilon ? *first : T(cr[1] + (cr[1] - cr[2]));
	p[1]	= last && len(*p - *last) > epsilon ? *last : T(*p + (*p - p[-1]));

	uint32	num_bez = p - cr - 1;
	C		c(num_bez * 3 + 1);

	for (int i = 0; i < num_bez; i++) {
		auto&	p0 = cr[i + 0];
		auto&	p1 = cr[i + 1];
		auto&	p2 = cr[i + 2];
		auto&	p3 = cr[i + 3];

		float	t1 = pow(len(p1 - p0), alpha);
		float	t2 = pow(len(p2 - p1), alpha);
		float	t3 = pow(len(p3 - p2), alpha);

		c[i * 3 + 0] = p1;
		c[i * 3 + 1] = p1 + lerp((p1 - p0) * t2 / t1, p2 - p1, t2 / (t1 + t2)) / 3;
		c[i * 3 + 2] = p2 - lerp((p3 - p2) * t2 / t3, p2 - p1, t2 / (t2 + t3)) / 3;
	}
	
	c.back() = p[1];
	return move(c);
}

//-----------------------------------------------------------------------------
// rational
//-----------------------------------------------------------------------------

template<typename T, int N> struct rational<bezier_splineT<T, N>> : bezier_splineT<T, N> {
	typedef	bezier_splineT<T, N>					B;
	typedef decltype(project(declval<T>()))			P;
	typedef decltype(declval<P>() - declval<P>())	V;
	using B::B;

	template<typename X> constexpr auto	evaluate(X x) const {
		return as_homo(B::evaluate(x));
	}
	template<typename X> constexpr auto	tangent(X x) const {
		auto	p = as_homo(B::evaluate(x));
		auto	t = as_homo(B::tangent(x));
		return (t.real * p.scale - p.real * t.scale) / square(p.scale);
	}

	constexpr const B&	non_rational()							const	{ return *this; }
	float	support_param(V v)									const	{ return nan; }
	V		support(V v)										const	{ return evaluate(support_param(v)); }
	P		closest(P v, float *_t, float *_d)					const	{
		if (_d)
			*_d = maximum;
		return v;
	}
	bool	ray_check(param(ray2) r, float &t, V *normal)		const	{ return false; }
};

template<> position2	rational<bezier_splineT<float3, 2>>::closest(position2 v, float* _t, float* _d) const;
template<> float		rational<bezier_splineT<float3, 2>>::support_param(V v) const;
template<> bool			rational<bezier_splineT<float3, 2>>::ray_check(param(ray2) r, float &t, vector2 *normal) const;

template<typename T, int N, typename C> struct rational<bezier_chain<T, N, C>> : bezier_chain<T, N, C> {
	typedef bezier_chain<T, N, C>	B;
	typedef typename B::iterator	BI;
	typedef rational<typename B::S>	S;
	typedef typename S::P	P;
	typedef typename S::V	V;
	using B::B;

	struct iterator : BI {
		iterator(const BI &i) : BI(i)	{}
		auto&	operator++()				{ BI::operator++(); return *this; }
		auto&	operator*()			const	{ return (const S&)BI::operator*(); }
		iterator operator+(int i)	const	{ return BI::operator+(i); }
	};

	rational(const B &b) : B(b) {}
	iterator	begin()			const	{ return B::begin(); }
	iterator	end()			const	{ return B::end(); }
	auto&	operator[](int i)	const	{ return *(begin() + i); }

	P		support(V v)									const	{ return supportc<rational, V, P>(*this, v); }
	P		closest(P v, float* _t, float* _d)				const	{ return closestc(*this, v, _t, _d); }
	bool	ray_check(param(ray2) r, float &t, V *normal)	const	{ return ray_checkc(*this, r, t, normal); }
};

template<typename T, int N, typename E> auto split(const rational<bezier_splineT<T,N>> &b, E t) {
	return make_rational(split(b.non_rational()));
}

template<typename T, int N> auto split(const rational<bezier_splineT<T,N>> &b) {
	return split(b, half);
}


//alternative formulation of a conic from 3 bezier control points with weights {1,w,1}
//https://dcain.etsin.upm.es/~alicia/publicaciones/conic.pdf

struct ConicBezier0 {
	float		w;

	static float get_middle_w(float w0, float w1, float w2) {
		return w1 * rsqrt(w0 * w2);
	}
	static float get_middle_w(position2 p0, position2 p1, position2 p2, position2 s) {
		auto		d	= p2 - p0;
		return cross(s - p0, d) / cross(d, s - p1);
	}

	ConicBezier0(float w)		: w(w) {}

	float2		parametric_centre()					const	{ return float2{-square(w), 0.5f} / (1 - square(w)); }
	float		parametric_test(float2 p)			const	{ return p.y * (1 - p.x - p.y) - square(p.x) / (4 * square(w)); }
	float2		parametric_intersect(float3 a)		const {
		auto	poly	= make_polynomial(a.z, 2 * w * a.x + (2 * w - 2) * a.z, -2 * w * a.x + a.y + a.z * (2 - 2 * w));
		float2	r;
		int		n = poly.roots(r);
		return r;
	}
	float2		evaluate_curve(float t)		const {
		return float2{2 * w * t * (1 - t), square(t)} / (square(1 - t) + 2 * w * t * (1 - t) + square(t));
	}
};

struct ConicBezier : triangle, ConicBezier0 {
	ConicBezier(position2 P, position2 Q, position2 R, float w)		: triangle(P, Q, R), ConicBezier0(w) {}
	ConicBezier(position2 P, position2 Q, position2 R, position2 S) : ConicBezier(P, Q, R, get_middle_w(P, Q, R, S)) {}
	ConicBezier(homo2 P, homo2 Q, homo2 R)							: ConicBezier(P, Q, R, get_middle_w(P.scale, Q.scale, R.scale)) {}

	operator conic() const	{
		auto	P	= position2(z);
		auto	Q	= P + x;
		auto	R	= P + y;
		return conic::line_pair({P,Q}, {R,Q}) + conic::line({P,R}) / (square(w) * 4);
	}
	
	homo2		centre() const	{
//		//return (float3)P + (float3)R - (float3)Q * (square(w) * 2);
		//return z + (z + y) - (z + x) * (square(w) * 2);
		return concat(z + (z + y) - (z + x) * (square(w) * 2), 2 * (1 - square(w)));

	}
	void		axes(line2 &axis1, line2 &axis2)		const;
	void		parametric_axes(float3 &axis1, float3 &axis2) const;

	position2	evaluate_curve(float t)		const	{ return from_parametric(ConicBezier0::evaluate_curve(t)); }
};

struct ArcParams {
	float2	radii;
	float	angle;
	bool	clockwise, big;

	ArcParams(float2 radii, float angle, bool clockwise, bool big) : radii(radii), angle(angle), clockwise(clockwise), big(big) {}
	ArcParams(float2 p0, float2 p1, float2 p2, float2 p3);	// 2 off-curve
	ArcParams(float2 p0, float2 p1, float2 p2);				// 1 off-curve

	float2x2		matrix()	const { return rotate2D(angle) * scale(radii); }

	void			fix_radii(float2 p0, float2 p1);
	shear_ellipse	ellipse(float2 p0, float2 p1) const;
	float2x2		control_points(float2 p0, float2 p1) const;
};

} //namespace iso

#endif	// BEZIER_H
