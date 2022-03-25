#ifndef POLYGON_H
#define POLYGON_H

#include "maths/geometry.h"
#include "base/algorithm.h"

namespace iso {

struct slope {
	float2	v;
	slope(param(float2) v)					: v(v)			{}
	slope(param(position2) a, param(position2) b) : v(b - a)		{}
	slope(const line2 &p)					: v(p.dir())	{}
	slope operator-() const { return -v; }

	friend bool approx_equal(slope a, slope b, float tol = ISO_TOLERANCE) {
		float2	t = a.v.xy * b.v.yx;
		return dot(a.v.xy, b.v.xy) > zero && approx_equal(t.x, t.y, tol);
	}
	friend bool between(slope x, slope a, slope b) {
		float	x2 = cross(x.v, a.v);
		return x2 * cross(x.v, b.v) < zero && x2 * cross(a.v, b.v) < zero;
	}
	// compare slopes
	friend bool operator==(const slope &a, const slope &b) {
		float2	t = a.v * b.v.yx;
		return t.x == t.y;
	}
	friend bool operator>=(const slope &a, const slope &b) {
		float2	t = a.v * b.v.yx;
		return t.y >= t.x;
	}
	friend bool operator>(const slope &a, const slope &b) {
		float2	t = a.v * b.v.yx;
		return t.y > t.x;
	}
};

int			optimise_polyline(position2 *p, int n, float eps, bool closed);
int			optimise_polyline_verts(position2 *p, int n, int verts, bool closed);
int			expand_polygon(const position2 *p, int n, float x, position2 *result);

int			intersection(const normalised<n_plane<float, 2>> *p, int n, position2 *out);
int			intersection(const plane *p, int n, position3 *out);
int			intersection(const plane *p, int n, param(plane) pl, position3 *out);

template<int N>	obb<float,N>	minimum_obb(stride_iterator<pos<float, N>> p, uint32 n);
template<>		obb<float,2>	minimum_obb(stride_iterator<position2> p, uint32 n);
template<>		obb<float,3>	minimum_obb(stride_iterator<position3> p, uint32 n);


template<int N>	n_sphere<float, N>	_minimum_sphere(stride_iterator<pos<float, N>> p, uint32 n);

template<int N, typename C> auto minimum_sphere(C &&c) {
	typedef pos<float, N>	P;
	return _minimum_sphere<N>(new_auto_container(P, c).begin(), c.size32());
}
template<typename C>	auto	minimum_circle(C &&c) {
	return _minimum_sphere<2>(new_auto_container(position2, c).begin(), c.size32());
}

template<int N> struct ellipsoid_type	: T_type<ellipse> {};
template<> struct ellipsoid_type<3>		: T_type<ellipsoid> {};
template<int N> using ellipsoid_t		= typename ellipsoid_type<N>::type;

template<int N>	ellipsoid_t<N>	minimum_ellipsoid(stride_iterator<pos<float, N>> p, uint32 n, float eps);
template<>		ellipsoid_t<2>	minimum_ellipsoid(stride_iterator<position2> p, uint32 n, float eps);
template<>		ellipsoid_t<3>	minimum_ellipsoid(stride_iterator<position3> p, uint32 n, float eps);
inline			ellipse			minimum_ellipse(stride_iterator<position2> p, uint32 n, float eps) { return minimum_ellipsoid(p, n, eps); }

triangle	minimum_triangle(stride_iterator<position2> p, uint32 n);

dynamic_array<position2> sub_poly(range<position2*> a, range<position2*> b);

//-----------------------------------------------------------------------------
// hull
//-----------------------------------------------------------------------------

bool	check_inside_subhull(position2 a, position2 b, position2 *p, int n, param(position2) v);
float	dist_to_subhull(position2 a, position2 b, position2 *p, int n, param(position2) v);
int		optimise_hull_2d(stride_iterator<position2> p, int n, int m);

template<typename I> int get_right_of_line(I i0, I i1, position2 *dest, param(position2) a, param(position2) b, position2 &c) {
	float	maxd	= zero;
	int		k		= 0;
	while (i0 != i1) {
		position2	t = *i0++;
		float		d = cross(a - t, b - t);
		if (d < zero) {
			if (d < maxd) {
				maxd = d;
				c	 = t;
			}
			dest[k++] = t;
		}
	}
	return k;
}

template<typename I> auto get_extrema(I i0, I i1, I *extrema) {
	auto		a	= decltype(plus_minus(*i0))(minimum);
	while (i0 != i1) {
		auto	t	= plus_minus(*i0);
		int		mask = bit_mask(t < a);
		for (auto *e = extrema; mask; ++e, mask >>= 1) {
			if (mask & 1)
				*e = i0;
		}
		a = min(a, t);
		++i0;
	}
	return a;
}

template<typename I> bool check_inside_hull(I i0, I i1, param(position2) v) {
	I			extrema[4]	= {i0, i0, i0, i0};
	float4	a = get_extrema(i0, i1, extrema);
	if (any(plus_minus(v.v) < a))
		return false;

	size_t		n		= distance(i0, i1);
	position2	*temp	= alloc_auto(position2, n);

	I	x0 = extrema[3];
	for (int i = 0; i < 4; i++) {
		I	x1 = extrema[i];
		if (x0 != x1) {
			position2	a = *x0, b = *x1, c;
			if (cross(a - v, b - v) < zero) {
				if (int k = get_right_of_line(i0, i1, temp, a, b, c)) {
					bool	out0 = cross(a - v, c - v) < zero;
					bool	out1 = cross(c - v, b - v) < zero;
					if (out0 && out1)
						return false;
					if (!out0 && !out1)
						return true;
					(out0 ? b : a) = c;
					return check_inside_subhull(a, b, temp, k, v);
				}
				return false;
			}
			x0 = x1;
		}
	}
	return true;
}
template<typename C> bool check_inside_hull(const C &c, param(position2) v) {
	return check_inside_hull(c.begin(), c.end(), v);
}

template<typename I> float dist_to_hull(I i0, I i1, param(position2) v) {
	I			extrema[4] = {i0, i0, i0, i0};
	get_extrema(i0, i1, extrema);
	size_t		n		= distance(i0, i1);
	position2	*temp	= alloc_auto(position2, n);

	I	x0 = extrema[3];
	for (int i = 0; i < 4; i++) {
		I	x1 = extrema[i];
		if (x0 != x1) {
			position2	a = *x0, b = *x1, c;
			if (cross(a - v, b - v) < zero) {
				if (int k = get_right_of_line(i0, i1, temp, a, b, c)) {
					auto	t		= c - v;
					bool	out0	= cross(a - v, t) < zero;
					bool	out1	= cross(t, b - v) < zero;
					if (out0 && out1)
						return len(t);
					if (!out0 && !out1)
						return 0;
					(out0 ? b : a) = c;
					return dist_to_subhull(a, b, temp, k, v);
				}
				return sqrt(min(len2(a - v), len2(b - v)));
			}
			x0 = x1;
		}
	}
	return true;
}
template<typename C> float dist_to_hull(const C &c, param(position2) v) {
	return dist_to_hull(c.begin(), c.end(), v);
}

template<typename I> int generate_hull_2d(I i0, I i1) {
	if (i0 == i1)
		return 0;

	typedef typename iterator_traits<I>::element T;
	I		p		= i0;
	I		mini	= i0, maxi = i0;
	float	minx	= as_vec(*i0).x, maxx = minx;
	for (I i = i0 + 1; i != i1; ++i) {
		float x	= as_vec(*i).x;
		if (x < minx)
			mini = i;
		else if (x > maxx)
			maxi = i;
		minx	= min(minx, x);
		maxx	= max(maxx, x);
	}

	//get line between maximal points
	T		p0		= *mini;
	*mini	= *i0;
	*i0		= p0;
	if (maxi == i0)
		maxi = mini;
	++i0;

	T		p1		= *maxi;
	*maxi	= *i0;
	*i0		= p1;

	T		p2		= p0;
	I		dest	= i0;
	++i0;

	struct {
		T	p;
		I	i0, i1;
	} stack[32], *sp = stack;

	for (;;) {
		if (i0 < i1) {
			auto	dir_left	= p1 - p0;
			auto	dir_right	= p2 - p1;
			float	maxd_left	= 0, maxd_right = 0;
			I		maxi_left	= i0, maxi_right = i1;
			I		i = i0, ileft = i0, iright = i1;
			T		v = *i++;

			while (i <= iright) {
				float	d	= cross(v - p0, dir_left);
				if (d > 0) {
					if (d > maxd_left) {
						maxi_left = ileft;
						maxd_left = d;
					}
					*ileft++ = v;
					v = *i++;
				} else {
					d	= cross(v - p1, dir_right);
					if (d > 0) {
						swap(v, *--iright);
						if (d > maxd_right) {
							maxi_right	= iright;
							maxd_right	= d;
						}
					} else {
						v = *i++;
					}
				}
			}

			if (maxi_right != iright)
				swap(*maxi_right, *iright);

			if (ileft > i0) {
				sp->p		= p2;
				sp->i0		= iright;
				sp->i1		= i1;
				sp++;

				p2			= p1;
				p1			= *maxi_left;
				*maxi_left	= *i0++;
				i1			= ileft;
				continue;
			}

			i0 = iright;
		}

		for (;;) {
			*dest++ = p1;
			p0		= p1;

			if (i0 < i1) {
				p1		= *i0++;
				break;
			} else if (sp > stack) {
				--sp;
				p1		= p2;
				p2		= sp->p;
				i0		= sp->i0;
				i1		= sp->i1;
			} else {
				return int(dest - p);
			}
		}
	}
}

int		generate_hull_3d(const position3 *p, int n, uint16 *indices, plane *planes);
inline int generate_hull_3d(position3* p, int n, uint16* indices, plane* planes) {
	return generate_hull_3d((const position3*)p, n, indices, planes);
}
template<typename I> int	generate_hull_3d(I p, int n, uint16* indices, plane* planes) {
	return generate_hull_3d(new_auto_init(position3, n, p).begin(), n, indices, planes);
}

template<int N>	struct generate_hull_s;

template<>	struct generate_hull_s<2> {
	template<typename I> static int generate(I i0, I i1) { return generate_hull_2d(i0, i1); }
	template<typename I> static int generate(I i0, I i1, normalised<n_plane<float, 2>> *planes);

};
template<>	struct generate_hull_s<3> {
	template<typename I> static int generate(I i0, I i1) { return generate_hull_3d(i0, distance32(i0, i1), 0, 0); }
	template<typename I> static int generate(I i0, I i1, normalised<n_plane<float, 3>> *planes) { return generate_hull_3d(i0, distance32(i0, i1), 0, planes); }
};

template<int N, typename I>	int	generate_hull(I i0, I i1) {
	return generate_hull_s<N>::generate(i0, i1);
}

template<int N, typename I>	int	generate_hull(I i0, I i1, normalised<n_plane<float, N>> *planes) {
	return generate_hull_s<N>::generate(i0, i1, planes);
}

template<int N, typename I> int generate_hull(I i0, I i1, int *out) {
	int		n	= distance32(i0, i1);
	float4	*p	= alloc_auto(float4, n);

	for (int i = 0; i < n; ++i, ++i0)
		p[i] = concat(float2(*i0), zero, i);

	int		nh	= generate_hull<N>(p, p + n);
	for (int i = 0; i < nh; i++)
		out[i] = p[i].w;
	return nh;
}

template<int N, typename C> int generate_hull(const C &c, int *out) {
	return generate_hull<N>(c.begin(), c.end(), out);
}

//-----------------------------------------------------------------------------
// point_cloud
//-----------------------------------------------------------------------------

template<typename C> struct point_cloud {
	typedef element_t<C>						E;
	static const int N = num_elements_v<E>;
	typedef mat<float, N, N+1>					M;

	C	c;
	template<typename C2>	explicit point_cloud(const point_cloud<C2> &c)	: c(c.c) 	{}
	template<typename...U>	explicit point_cloud(U&&...u)	: c(forward<U>(u)...) 	{}
	explicit		point_cloud(initializer_list<E> i)	: c(i) 	{}
	//point_cloud(C &&c)		: c(forward<C>(c)) 	{}
	auto			antipode(param_t<E> p0, param_t<E> p1)	const	{ return iso::support(points(), perp(p1 - p0)); }
	auto			support(param_t<vec<float, N>> v)		const	{ return pos<float, N>(*iso::support(points(), v)); }
	bool			hull_contains(param_t<E> b)	const	{ return check_inside_hull<N>(c, b); }
	auto			circumscribe()				const	{ return minimum_sphere<N>(c); }
	E				centroid()					const	{ return iso::centroid(c); }
	E				centre()					const	{ return iso::centroid(c); }
	auto			get_box()					const	{ return as_aabb(get_extent(c.begin(), c.end())); }
	auto			get_obb()							{ int n = generate_hull<N>(c.begin(), c.end()); return minimum_obb<N>((pos<float, N>*)c.begin(), n); }
	quadrilateral	get_quad()							{ int n = generate_hull<N>(c.begin(), c.end()); auto a = c.begin(); optimise_hull_2d(a, n, 4); return quadrilateral(a[0], a[1], a[3], a[2]); }
	triangle		get_tri()					const	{ return minimum_triangle(c.begin(), c.size32()); }
	auto			get_ellipse()				const	{ return minimum_ellipsoid<N>(c.begin(), uint32(c.size()), ISO_TOLERANCE); }

	const C&		points()					const	{ return c; }
	C&				points()							{ return c; }
	auto			begin()						const	{ return c.begin(); }
	auto			end()						const	{ return c.end(); }
	auto			operator[](int i)			const	{ return c[i]; }
	bool			empty()						const	{ return c.empty(); }
	auto			size()						const	{ return c.size(); }
	auto			size32()					const	{ return uint32(size()); }

	point_cloud&	operator*=(const M &m) {
		for (auto &&i : c)
			i = m * get(i);
		return *this;
	}
};

template<typename C> auto make_point_cloud(C &&c) {
	return point_cloud<C>(forward<C>(c));
}
template<typename I> auto make_point_cloud(I &&a, I &&b) {
	return make_point_cloud(range<I>(forward<I>(a), forward<I>(b)));
}
template<typename I> auto make_point_cloud(I a, size_t n) {
	return make_point_cloud(range<I>(a, a + n));
}

//-----------------------------------------------------------------------------
// path - sequence of vertices
//-----------------------------------------------------------------------------

template<typename I> float path_len(I begin, I end) {
	if (begin == end)
		return zero;
	--end;
	float	t	= 0;
	for (auto i = begin; i != end; ++i)
		t += len(i[1] - i[0]);
	return t;
}
template<typename C> float path_len(C &&c)	{ return path_len(begin(c), end(c)); }

template<typename I> auto uniform_path(I begin, I end, float t) {
	t *= path_len(begin, end);
	auto	i = begin;
	float	x = 0;
	do {
		t -= x;
		x = len(i[1] - i[0]);
	} while (t > x);

	return lerp(i[0], i[1], t / x);
}

template<typename I1, typename I2> float path_dist(I1 begin1, I1 end1, I2 begin2) {
	float	t	= 0;
	while (begin1 != end1) {
		t += len(*begin1 - *begin2);
		++begin1;
		++begin2;
	}
	return t;
}

template<typename I1, typename I2> float path_dist(I1 begin1, I2 begin2, size_t n) {
	float	t	= 0;
	for (size_t i = n; i--; ++begin1, ++begin2)
		t += len(*begin1 - *begin2);
	return t;
}

template<typename O, typename I> O resample_path(I points, I end, O out, float px) {
	auto	prev = *points;
	*out++ = prev;

	float	D = 0;			// accumulated distance
	while (++points != end) {
		auto	pt	= *points;
		float	dxy = len(pt - prev);
		while (D + dxy > px) {
			// append interpolated point
			*out++ = lerp(prev, pt, (px - D) / dxy);
			dxy -= px - D;
			D	= 0;
		}
		prev	= pt;
		D		+= dxy;
	}
	if (D > 0)
		*out++ = prev;

	return out;
}

template<typename O, typename C> auto resample_path(C &&c, O out, float px)	{
	return resample_path(begin(c), end(c), out, px);
}

template<typename C> struct path : point_cloud<C> {
	typedef point_cloud<C>	B;
	using B::c;

	template<typename C2> path(C2 &&c) : B(forward<C2>(c)) 	{}
	auto			edge(int j)						const	{ return make_ray(c[j], c[j + 1]); }
	line2			line(int j)						const	{ auto a = c.begin(); return line2(a[j], a[j + 1]); }
	float			length()						const	{ return iso::path_len(c.begin(), c.end()); }
	auto			point(float t)					const	{ return edge(int(t)).from_parametric(frac(t)); }
	auto			uniform_point(float t)			const	{ return uniform_path(c.begin(), c.end(), t); }

	auto lines() const { return transformc(range<int_iterator<int> >(0, c.size32() - 1), [this](int i) { return line(i); }); }
	auto edges() const { return transformc(range<int_iterator<int> >(0, c.size32() - 1), [this](int i) { return edge(i); }); }
};

template<typename C> path<C&>	make_path(point_cloud<C> &c)	{ return c.c; }
template<typename C> path<C>	make_path(point_cloud<C> &&c)	{ return move(c.c); }

//-----------------------------------------------------------------------------
// perimeter functions
//-----------------------------------------------------------------------------

template<typename I> float perimeter(I begin, I end) {
	if (begin == end)
		return zero;
	auto	t	= len(*begin - *--end);
	for (auto i = begin; i != end; ++i)
		t += len(i[1] - i[0]);
	return t;
}
template<typename C> float perimeter(C &&c) { return perimeter(begin(c), end(c)); }

template<typename I> auto uniform_perimeter(I begin, I end, float t) {
	t *= perimeter(begin, end);

	--end;
	for (auto i = begin; i != end; ++i) {
		float	x = len(i[1] - i[0]);
		if (t < x)
			return lerp(i[0], i[1], t / x);
		t -= x;
	}
	return lerp(*end, *begin, t / len(*begin - *end));
}

template<typename I> int crossing(I begin, I end, param(position2) b) {
	if (begin == end)
		return 0;
	int			cn	= 0;
	float2		p0	= end[-1].v;
	for (auto *i = begin; i != end; ++i) {
		float2 p1 = *i;
		if (bool(p0.y > b.v.y) != bool(p1.y > b.v.y))
			cn += int(b.v.x < p0.x + (b.v.y - p0.y) * (p1.x - p0.x) / (p1.y - p0.y));
		p0 = p1;
	}
	return cn;
}

template<typename I> int winding(I begin, I end, param(position2) b) {
	if (begin == end)
		return 0;
	int		wn	= 0;
	float2	p0	= end[-1].v;
	for (auto i = begin; i != end; ++i) {
		float2 p1 = *i;
		if (p0.y <= b.v.y) {
			if (p1.y > b.v.y)
				wn += int(cross(p1 - p0, b.v - p0) > zero);
		} else {
			if (p1.y <= b.v.y)
				wn -= int(cross(p1 - p0, b.v - p0) < zero);
		}
		p0 = p1;
	}
	return wn;
}

//-----------------------------------------------------------------------------
// clipping
//-----------------------------------------------------------------------------

template<typename T> inline T	edge_lerp(T v0, T v1, float t)				{ return lerp(v0, v1, t); }
inline homo2		edge_lerp(param(homo2) v0, param(homo2) v1, float t)	{ return force_cast<homo2>(lerp(v0.v.xyz, v1.v.xyz, t)); }
inline homo3		edge_lerp(param(homo3) v0, param(homo3) v1, float t)	{ return force_cast<homo3>(lerp(v0.v.xyzw, v1.v.xyzw, t)); }

// clip poly to line or plane
template<typename I, typename O, typename P> O clip_poly(I i0, I i1, O out, P p) {
	auto	v0	= i1[-1];
	auto	d0	= p.unnormalised_dist(v0);

	for (auto i = i0; i != i1; ++i) {
		auto	v1	= *i;
		auto	d1	= p.unnormalised_dist(v1);
		if (d0 * d1 < zero)
			*out++ = edge_lerp(v0, v1, d0 / (d0 - d1));
		if (d1 < zero)
			*out++ = v1;
		v0	= v1;
		d0	= d1;
	}
	return out;
}

// clip poly to lines or planes
template<typename I, typename O, typename P> O clip_poly(I i0, I i1, O out, range<P> ps) {
	O		o	= out + (ps.size() - 1);
	O		o1	= clip_poly(i0, i1, o, ps.front());
	for (auto &p : slice(ps, 1)) {
		auto	o0 = o--;
		o1	= clip_poly(o0, o1, o, p);
	}
	return o1;
}

template<int D> struct clip_poly_s;

// clip poly to unit rect
template<> struct clip_poly_s<2> {
	template<typename I, typename O> static O clip(I i0, I i1, O result) {
		O	o1	= clip_poly(i0, i1, result + 3,			unit_plane<PLANE_MINUS_X>());
		o1		= clip_poly(result + 3, o1, result + 2,	unit_plane<PLANE_PLUS_X	>());
		o1		= clip_poly(result + 2, o1, result + 1,	unit_plane<PLANE_MINUS_Y>());
		o1		= clip_poly(result + 1, o1, result,		unit_plane<PLANE_PLUS_Y	>());
		return o1;
	}
};

// clip poly to unit cube
template<> struct clip_poly_s<3> {
	template<typename I, typename O> static O clip(I i0, I i1, O result) {
		O	o1	= clip_poly(i0, i1, result + 5,			unit_plane<PLANE_MINUS_X>());
		o1		= clip_poly(result + 5, o1, result + 4,	unit_plane<PLANE_PLUS_X	>());
		o1		= clip_poly(result + 4, o1, result + 3,	unit_plane<PLANE_MINUS_Y>());
		o1		= clip_poly(result + 3, o1, result + 2,	unit_plane<PLANE_PLUS_Y	>());
		o1		= clip_poly(result + 2, o1, result + 1,	unit_plane<PLANE_MINUS_Z>());
		o1		= clip_poly(result + 1, o1, result,		unit_plane<PLANE_PLUS_Z	>());
		return o1;
	}
};

// clip poly to unit rect or cube
template<typename I, typename O> O clip_poly(I i0, I i1, O result) {
	return clip_poly_s<num_elements_v<noref_t<decltype(*i0)>>>::clip(i0, i1, result);
}

// clip plane to unit cube
position3 *clip_plane(const n_plane<float, 3> &p, position3 *out);

// find splits across line or plane; return true if any on correct side
// t0 is transition from inside to outside
// t1 is transition from outside to inside
template<typename I, typename P> bool find_split(I i0, I i1, P p, float &t0, float &t1) {
	t0 = 0;
	t1 = int(i1 - i0);

	auto	v0	= i1[-1];
	auto	d0	= p.unnormalised_dist(v0);

	for (auto i = i0; i != i1; ++i) {
		auto	v1	= *i;
		auto	d1	= p.unnormalised_dist(v1);
		if (d0 * d1 < zero)
			(d0 > zero ? t1 : t0) = int((i == i0 ? i1 : i - 1) - i0) + d0 / (d0 - d1);
		v0	= v1;
		d0	= d1;
	}
	return t0 || d0 > 0;
}

//lop off everything between i0 and i1
template<typename I, typename O> int lop_poly(I i0, I i1, O out, float t0, float t1) {
	auto	i = i0 + int(t1);
	if (float f = frac(t1)) {
		auto	j	= i++;
		if (i == i1)
			i = i0;
		*out++ = edge_lerp(*j, *i, f);
	}

	auto	e0 = i0 + int(t0);
	for (auto e = e0 + 1; i != e; ++i) {
		if (i == i1)
			i = i0;
		*out++ = *i;
	}

	if (float f = frac(t0)) {
		if (i == i1)
			i = i0;
		*out++ = edge_lerp(*e0, *i, f);
	}

	return out;
}

// split poly across line or plane
template<typename I, typename O, typename P> auto split_poly(I i0, I i1, O outf, O outb, P p) {
	auto	v0	= i1[-1];
	auto	d0	= p.dist(v0);

	for (auto i = i0; i != i1; ++i) {
		auto	v1	= *i;
		auto	d1	= p.dist(v1);

		if (d0 * d1 < zero) {
			auto	v = edge_lerp(v0, v1, d0 / (d0 - d1));
			*outf++ = v;
			*outb++ = v;
		}
		if (d1 < zero)
			*outb++ = v1;
		if (d1 > zero)
			*outf++ = v1;

		v0	= v1;
		d0	= d1;
	}
	return make_pair(outf, outb);
}

//-----------------------------------------------------------------------------
// polygon_helper
//-----------------------------------------------------------------------------

template<int D> struct polygon_helper;

//-------------------------------------
// 2d
//-------------------------------------

template<> struct polygon_helper<2> {
	template<typename I> static auto normal(I begin, I end) {
		return z_axis;
	}
	template<typename I> static auto plane(I begin, I end) {
		return z_axis;
	}
	template<typename I> static float3 centroid0(I begin, I end) {
		float3		sum(zero);
		auto		p0	= end[-1];
		for (auto i = begin; i != end; ++i) {
			auto	p1	= *i;
			sum		+= concat(p0 + p1, three) * cross(p0, p1);
			p0		= p1;
		}
		return sum;
	}
	template<typename I> static position2 centroid(I begin, I end) {
		if (begin == end)
			return position2(zero);
		auto	sum = centroid0(begin, end);
		return position2(select(sum.z == zero, *begin, sum.xy / sum.z));
	}
	template<typename I> static float area(I begin, I end) {
		float2	area(zero);
		auto	p0	= end[-1];
		for (auto i = begin; i != end; ++i) {
			auto p1	= *i;
			area	+= as_vec(p0) * as_vec(p1).yx;
			p0		= p1;
		}
		return abs(diff(area)) * half;
	}
	template<typename I> static bool convex_contains(I begin, I end, param(position2) b) {
		auto	p0	= end[-1] - b;
		for (auto i = begin; i != end; ++i) {
			auto	p1	= *i - b;
			if (cross(p0, p1) > zero)
				return false;
			p0	= p1;
		}
		return true;
	}
	template<typename I> static circle		convex_inscribe(I begin, I end, float eps);
	template<typename I> static position2	convex_uniform_interior(I begin, I end, param(float2) v);
};

//Efficient Algorithm for Approximating Maximum Inscribed Sphere in High Dimensional Polytope
//Yulai Xie, Jack Snoeyink, Jinhui Xu

template<typename I> circle polygon_helper<2>::convex_inscribe(I begin, I end, float eps) {
	size_t		n		= end - begin;
	position2	*dual	= alloc_auto(position2, n);

	for (position2 o = iso::centroid(begin, end);;) {
		position2	p0	= end[-1] - o;
		position2	*p	= dual;
		for (auto i = begin; i != end; ++i) {
			position2	p1	= *i - o;
			line2		d(p0, p1);
			*p++	= position2(d.normal() / d.dist());
			p0		= p1;
		}
		circle	c	= _minimum_sphere<2>(dual, uint32(n));
		float	d2	= len2(c.centre());

		if (d2 < eps)
			return circle::with_r2(o, reciprocal(c.radius2()));

		o += normalise(c.centre().v) * sqrt(d2) / (c.radius2() - d2);
	}
}

template<typename I> position2 polygon_helper<2>::convex_uniform_interior(I begin, I end, param(float2) v) {
	size_t	n		= end - begin;
	float	*areas	= alloc_auto(float, n - 1);
	float	area	= 0;

	areas[0] = area;
	for (int i = 1; i < n - 1; i++) {
		area += triangle(begin[0], begin[i], begin[i + 1]).area();
		areas[i] = area;
	}

	float	x = v.x * area;
	auto	i = lower_bound(areas + 1, areas + n - 2, x) - areas;
	return uniform_interior(triangle(begin[0], begin[i], begin[i + 1]), float2{(x - areas[i - 1]) / (areas[i] - areas[i - 1]), v.y});
}

//-------------------------------------
// 3d
//-------------------------------------

template<> struct polygon_helper<3> {
	template<typename I> static auto normal(I begin, I end) {
		size_t n = end - begin;
		return cross(begin[n / 3] - begin[0], begin[n * 2 / 3] - begin[0]);
	}
	template<typename I> static auto plane(I begin, I end) {
		return make_plane(normal(begin, end), begin[0]);
	}
	template<typename I> static float4 centroid0(I begin, I end) {
		float4		sum(zero);
		auto		norm	= normal(begin, end);
		auto		p0		= end[-1];
		for (auto i = begin; i != end; ++i) {
			auto p1	= *i;
			sum		+= concat((float3)p0 + (float3)p1, three) * dot(cross(p0, p1), norm);
			p0		= p1;
		}
		return sum;
	}
	template<typename I> static position3 centroid(I begin, I end) {
		if (begin == end)
			return position3(zero);
		auto	sum = centroid0(begin, end);
		return position3(select(sum.w == zero, *begin, sum.xy / sum.z));
	}
	template<typename I> static float area(I begin, I end) {
		float3	area(zero);
		auto	p0	= end[-1];
		for (auto i = begin; i != end; ++i) {
			auto	p1 = *i;
			area	+= cross(p0, p1);
			p0		= p1;
		}
		return len(area) * half;
	}
	template<typename I> static bool convex_contains(I begin, I end, param(position3) b) {
		auto	p = plane(begin, end);

		if (!approx_equal(p.dist(b), zero))
			return false;

		auto	p0	= end[-1] - b;
		for (auto i = begin; i != end; ++i) {
			auto	p1	= *i - b;
			if (dot(cross(p0, p1), p.normal()) > zero)
				return false;
			p0	= p1;
		}
		return true;
	}
};

//-----------------------------------------------------------------------------
// polygon - sequence of vertices (potentially crossing edges)
//-----------------------------------------------------------------------------

template<typename C> struct polygon : point_cloud<C> {
	typedef point_cloud<C>	B;
	using B::B;
	using typename B::E;
	using B::N;
	using B::c;
	typedef polygon_helper<N>	helper;
	typedef diff_t<E>	V;

//	template<typename C2> polygon(C2 &&c) : B(forward<C2>(c)) 	{}
	float			area()							const	{ return c.empty() ? zero : helper::area(c.begin(), c.end()); }
	E				centroid()						const	{ return helper::centroid(c.begin(), c.end()); }
	bool			contains(param_t<E> b, WINDING_RULE r = WINDING_NONZERO)	const { return test_winding(r, winding(b)); }
	int				crossing(param_t<E> b)			const	{ return iso::crossing(c.begin(), c.end(), b); }
	int				winding(param_t<E> b)			const	{ return iso::winding(c.begin(), c.end(), b); }
	void			expand(polygon &d, float x)		const	{ d.b = d.begin() + expand_polygon(c.begin(), c.size(), x, d.begin()); }
//	void			optimise(float eps)						{ b = c.begin() + optimise_polyline(c.begin(), c.size(), eps, true); }
	auto			edge(int j)						const	{ auto a = c.begin(); return make_interval(a[j], a[j == c.size() - 1 ? 0 : j + 1]); }
	line2			line(int j)						const	{ auto a = c.begin(); return line2(a[j], a[j == c.size() - 1 ? 0 : j + 1]); }
	auto			perimeter()						const	{ return iso::perimeter(c.begin(), c.end()); }
	auto			perimeter(float t)				const	{ return edge(int(t)).from(frac(t)); }
	auto			uniform_perimeter(float t)		const	{ return iso::uniform_perimeter(c.begin(), c.end(), t); }

	float3			normal()						const	{ return helper::normal(c.begin(), c.end()); }
	plane			plane()							const	{ return helper::plane(c.begin(), c.end()); }
	polygon<dynamic_array<E>> operator-(const polygon &b) const { return sub_poly(c, b.c); }

	auto			lines()							const	{ return transformc(range<int_iterator<int> >(0, c.size32()), [this](int i) { return line(i); }); }
	auto			edges()							const	{ return transformc(range<int_iterator<int> >(0, c.size32()), [this](int i) { return edge(i); }); }
};


template<typename C> auto	make_polygon(point_cloud<C> &c)		{ return polygon<C&>(c.c); }
template<typename C> auto	make_polygon(point_cloud<C> &&c)	{ return polygon<C>(move(c.c)); }

//-----------------------------------------------------------------------------
// polyhedron
//-----------------------------------------------------------------------------

//	V-E+F=2
// 
//	Consider tuples for each case of a vertex-edge-face adjacency (for a cube there are 6 such adjacencies for each vertex, or 48 total)
//	(V,T,N,B)
//	V	Coordinates of a vertex
//	T	Unit tangent vector from the vertex along the edge towards the other end
//	N	Unit vector normal to the edge in the plane of the face, and pointing from the edge into the face
//	B	Unit vector normal to the plane of the face, and pointing into the polyhedron
//
//	Volume of the polyhedron		VOLUME	= -1/6 sum( V.T V.N V.B )
//	Total area of all the faces		AREA	=  1/2 sum( V.T V.N )
//	Total length of all the edges	LENGTH	= -1/2 sum( V.T )

template<typename C> struct polyhedron : point_cloud<C> {
	typedef point_cloud<C>	B;
	using B::B;
	template<typename C2> polyhedron(C2 &&c)	: B(forward<C2>(c)) 	{}
	int			num_tris()		const	{ return B::size32() / 3; }
	triangle3	tri(int j)		const	{ auto a = B::points().begin() + j * 3; return triangle3(a[0], a[1], a[2]); }
	auto		tris()			const	{ return transformc(range<int_iterator<int> >(0, num_tris()), [this](int i) { return tri(i); }); }

	float		volume() const {
		float	vol(zero);
		for (auto i : tris())
			vol += i.signed_volume();
		return vol / 6;
	}
	float		surface_area() const {
		float	area(zero);
		for (auto i : tris())
			area += i.area();
		return area;
	}
	position3	centroid() const {
		//http://wwwf.imperial.ac.uk/~rn/centroid.pdf
		float3	t(zero);
		float	v(zero);
		for (auto i : tris()) {
			float3		n = i.normal();
			v	+= dot(n, i.z);
			t	+= n * (square(i.z + i.x * half) + square(i.z + i.y * half) + square(i.z + (i.x + i.y) * half));
		}
		return position3(t / (v * 4));
	}
	float		winding_number(param(position3) p) const {
		float	sum(zero);
		for (auto i : tris())
			sum += i.solid_angle(p);
		return sum / (pi * 4);
	}

	bool		ray_check(param(ray3) r, float &t, vector3 *normal) const {
		for (auto&& tri : tris()) {
			if (tri.ray_check(r, t, normal))
				return true;
		}
		return false;
	}

	static constexpr auto	matrix()	{ return identity; }

	friend auto	uniform_surface(const polyhedron& c, param(float2) t) {
		size_t	n		= c.num_tris();
		float	*areas	= alloc_auto(float, n + 1), *pa = areas;
		float	area	= 0;

		for (auto &&tri : c.tris()) {
			*pa++ = area;
			area += tri.area();
		}
		*pa++ = area;

		float	x = t.x * area;
		auto	i = lower_bound(areas, areas + n, x);
		return uniform_interior(c.tri(i - areas), float2{(x - i[-1]) / (i[0] - i[-1]), t.y});
	}

	friend auto surface_optimised(const polyhedron& c) {
		size_t		n		= c.num_tris();
		dynamic_array<float>	areas(n + 1);
		auto		pa		= areas.begin();
		float		area	= 0;

		for (auto &&tri : c.tris()) {
			*pa++ = area;
			area += tri.area();
		}
		*pa++ = area;

		for (auto &i : areas)
			i /= area;

		return [&c, areas = move(areas)](param(float2) t) {
			auto	i = lower_boundc(areas, t.x);
			return uniform_interior(c.tri(i - areas.begin()), float2{(t.x - i[-1]) / (i[0] - i[-1]), t.y});
		};
	}
};

//-----------------------------------------------------------------------------
// simple_polygon - non-intersecting edges
//-----------------------------------------------------------------------------

template<typename C> struct simple_polygon : polygon<C> {
	typedef polygon<C>	B;
	using B::B;
	using typename B::E;
	using typename B::V;
	using typename B::helper;
	using B::c;

	template<typename C1> simple_polygon(C1 &&c) : B(forward<C1>(c)) {}
	float			distance2(param_t<E> b)			const;// distance squared from point to polygon outline
	float			distance(param_t<E> b)			const;// signed distance from point to polygon outline (negative if point is outside)
	bool			contains(param_t<E> b)			const	{ return crossing(b) & 1; }
	circle			inscribe(float eps = 1e-3)		const;
	E				uniform_interior(V v)			const	{ return E(zero); }	//TBD
};

template<typename C> simple_polygon<C&>	as_simple_polygon(point_cloud<C> &c)	{ return c.c; }
template<typename C> simple_polygon<C>	as_simple_polygon(point_cloud<C> &&c)	{ return move(c.c); }
template<typename C> simple_polygon<C&>	as_simple_polygon(point_cloud<C&> &&c)	{ return c.c; }

template<typename C> auto make_star_polygon(point_cloud<C> &&c) {
	typedef element_t<C>	E;
	auto		poly	= as_simple_polygon(c);
	sort(poly.c, [centre = c.centroid()](const E& a, const E& b) {
		auto	a1 = a - centre, b1= b - centre;
		if (a1.y * b1.y < zero)
			return a1.y > zero;
		return cross(a1, b1) < zero;
	});
	return poly;
}

template<typename C> float simple_polygon<C>::distance2(param_t<E> b) const {
	if (c.empty())
		return 0;
	float	d2	= infinity;
	auto	p0	= c.back();
	for (auto p1 : c) {
		d2 = min(d2, segment_distance2(p0, p1, b));
		p0 = p1;
	}
	return d2;
}

template<typename C> float simple_polygon<C>::distance(param_t<E> b) const {
	if (c.empty())
		return 0;
	float		d2 = infinity;
	int			cn	= 0;
	float2		p0	= c.back().v;
	for (auto p1 : *this) {
		if (bool(p0.y > b.y) != bool(p1.y > b.y))
			cn += int(b.x < p0.x + (b.y - p0.y) * (p1.x - p0.x) / (p1.y - p0.y));
		d2 = min(d2, segment_distance2(p0, p1, b));
		p0 = p1;
	}
	return (cn & 1 ? 1 : -1) * sqrt(d2);
}

struct Cell : circle {	// cell center & distance from cell center to polygon
	float		h;		// half the cell size
	float		max;	// max distance to polygon within a cell
	Cell(param(circle) c, float h) : circle(c), h(h), max(radius() + h * sqrt(2)) {}
	template<typename C> Cell(param(position2) c, float h, const simple_polygon<C> &poly) : circle(with_r2(c, poly.distance2(c))), h(h), max(radius() + h * sqrt(2)) {}
	friend bool operator<(const Cell &a, const Cell &b) { return a.max < b.max; }
	friend bool operator>(const Cell &a, const Cell &b) { return a.max > b.max; }
};

template<typename C> circle simple_polygon<C>::inscribe(float eps) const {
	const rectangle rect	= this->get_box();
	const float cell_size	= reduce_min(rect.extent());
	float		h			= cell_size / 2;

	// a priority queue of cells in order of their "potential" (max distance to polygon)
	priority_queue<dynamic_array<Cell>, greater> queue;

	// cover polygon with initial cells
	for (float x = rect.a.v.x; x < rect.b.v.x; x += cell_size) {
		for (float y = rect.a.v.y; y < rect.b.v.y; y += cell_size)
			queue.push(Cell(position2(x + h, y + h), h, *this));
	}

	// take centroid as the first best guess
	Cell bestCell(this->centroid(), 0, *this);

	// special case for rectangular polygons
	Cell bboxCell(rect.centre(), 0, *this);
	if (bboxCell.radius2() > bestCell.radius2())
		bestCell = bboxCell;

	while (!queue.empty()) {
		// pick the most promising cell from the queue
		auto cell = queue.pop_value();

		// update the best cell if we found a better one
		if (cell.radius2() > bestCell.radius2())
			bestCell = cell;

		// do not drill down further if there's no chance of a better solution
		if (cell.max - bestCell.radius() > eps) {
			// split the cell into four cells
			h = cell.h / 2;
			queue.push(Cell(cell.centre() + float2{-h, -h}, h, *this));
			queue.push(Cell(cell.centre() + float2{+h, -h}, h, *this));
			queue.push(Cell(cell.centre() + float2{-h, +h}, h, *this));
			queue.push(Cell(cell.centre() + float2{+h, +h}, h, *this));
		}
	}

	return bestCell;
}

//-----------------------------------------------------------------------------
// complex_polygon - multiple simple polygons
//-----------------------------------------------------------------------------

// C is a container of simple_polygons
template<typename C> struct complex_polygon {
	C	c;

	complex_polygon() {}
	template<typename S> complex_polygon(const simple_polygon<S>& s) { c.push_back(s); }

	auto			begin()				const	{ return c.begin(); }
	auto			end()				const	{ return c.end(); }
	decltype(auto)	operator[](int i)	const	{ return c[i]; }
	bool			empty()				const	{ return c.empty(); }
	auto			size()				const	{ return c.size(); }
	auto			size32()			const	{ return uint32(size()); }
	const C&		contours()			const	{ return c; }
	C&				contours()					{ return c; }

	auto			get_box() const {
		decltype(c.front().get_box())	box(none);
		for (auto &i : c)
			box |= i.get_box();
		return box;
	}

	float			area() const {
		float	area = 0;
		for (auto &i : c)
			area += i.area();
		return area;
	}
	auto			centroid() const {
		float3	sum(zero);
		for (auto &i : c)
			sum += polygon_helper<2>::centroid0(i.begin(), i.end());
		return position2(sum.xy / sum.z);
	}
	auto		centre() const { return centroid(); }
	bool		ray_check(param_t<ray2> r, float &t, float2 *normal) const { return false; }
	float		ray_closest(param_t<ray2> r)	const	{ return 0; }
	position2	support(param_t<float2> v)		const	{ return position2(zero); }
	position2	closest(param_t<position2> p)	const	{ return position2(zero); }
	bool		contains(param_t<position2> p)	const	{
		for (auto&& i : c)
			if (i.contains(p))
				return true;
		return false;
	}

	complex_polygon&	operator*=(const float2x3 &m) {
		for (auto &&i : c)
			i *= m;
		return *this;
	}
};


//-----------------------------------------------------------------------------
// convex_polygon
//-----------------------------------------------------------------------------

template<typename I, typename V> I convex_support(I i0, I i1, V v) {
	I	i = i0;
	for (auto n = i1 - i0; n; n >>= 1) {
		I	m0	= i + (n >> 1);
		I	m1	= m0;
		if (++m1 == i1)
			m1 = i0;
		if (dot(*m0, v) < dot(*m1, v)) {
			i = m1;
			--n;
		}
	}
	return i;
}

template<typename C, typename V> auto convex_support(const C &c, V v) {
	return convex_support(begin(c), end(c), v);
}

template<typename I, typename V> I convex_closest(I i0, I i1, V v) {
	auto	d	= v - *i0;
	auto	dp	= perp(d);
	I		j0	= convex_support(i0, i1, -dp);
	I		j1	= convex_support(i0, i1, +dp);

	if (j1 < j0)
		j1 += i1 - i0;

	I	i = j0;
	for (auto n = j1 - j0 + 1; n; n >>= 1) {
		I	m0	= i + (n >> 1);
		if (m0 >= i1)
			m0 -= i1 - i0;
		I	m1	= m0;
		if (++m1 == i1)
			m1 = i0;

		if (dot(*m1 - *m0, v -*m0) > zero) {
			i = m1;
			--n;
		}
	}
	return i;
};

template<typename I, typename P, typename V> bool convex_check_ray(I i0, I i1, P p, V v) {
	auto	vp	= perp(v);
	if (dot(*i0 - p, vp) > zero)
		vp = -vp;

	auto	m = convex_support(i0, i1, vp);
	return dot(*m - p, vp) >= zero;
}

template<typename I, typename P, typename V> bool convex_check_ray(I i0, I i1, P p, V v, float &t, V *normal) {
	V		vp	= perp(v);

	auto	j0 = convex_support(i0, i1, vp);
	if (dot(*j0 - p, vp) < zero)
		return false;

	auto	j1 = convex_support(i0, i1, -vp);
	if (dot(*j1 - p, vp) > zero)
		return false;

	if (j1 < j0)
		j1 += i1 - i0;

	I	i = j0;
	for (auto n = j1 - j0 + 1; n; n >>= 1) {
		I		m	= i + (n >> 1);
		if (m >= i1)
			m -= i1 - i0;

		if (dot(*m - p, vp) > 0) {
			i = ++m == i1 ? i0 : m;
			--n;
		}
	}

	auto	p1	= *i;
	i = (i == i0 ? i1 : i) - 1;
	auto	v1	= *i - p1;
	t = cross(p1 - p, v1) / cross(v, v1);
	if (normal)
		*normal = perp(v1);
	return true;
}

template<typename I, typename P, typename V> float convex_ray_closest(I i0, I i1, P p, V v) {
	V		vp	= perp(v);

	auto	j0 = convex_support(i0, i1, vp);
	if (dot(*j0 - p, vp) < zero)
		return dot(*j0 - p, v) / len2(v);

	auto	j1 = convex_support(i0, i1, -vp);
	if (dot(*j1 - p, vp) > zero)
		return dot(*j1 - p, v) / len2(v);

	if (j1 < j0)
		j1 += i1 - i0;

	I	i = j0;
	for (auto n = j1 - j0 + 1; n; n >>= 1) {
		I		m	= i + (n >> 1);
		if (m >= i1)
			m -= i1 - i0;

		if (dot(*m - p, vp) > 0) {
			i = ++m == i1 ? i0 : m;
			--n;
		}
	}

	if (i >= i1)
		i -= i1 - i0;

	auto	p1	= *i;
	i = (i == i0 ? i1 : i) - 1;
	auto	v1	= *i - p1;
	return cross(p1 - p, v1) / cross(v, v1);
}

template<typename I> void find_tangent(range<I> pa, I &_ia, range<I> pb, I &_ib) {
	I	ia = _ia, ib = _ib;
	for (;;) {
		slope	g(*ia, *ib);

		if (slope(*ia, *pa.inc_wrap(ia)) >= g)
			ia = pa.inc_wrap(ia);
		else if (slope(*ib, *pb.inc_wrap(ib)) > g)
			ib = pb.inc_wrap(ib);
		else
			break;
	}
	_ia = ia;
	_ib = ib;
}

template<typename I> void convex_merge(I begin1, I end1, I begin2, I end2) {
	I	i	= begin1;
	I	al	= i, ar = i;
	while (++i != end1) {
		if (i->x < al->x)
			al = i;
		else if (i->x > ar->x)
			ar = i;
	}

	i	= begin2;
	I	bl	= i, br = i;
	while (++i != end2) {
		if (i->x < bl->x)
			bl = i;
		else if (i->x > br->x)
			br = i;
	}

	if (ar->v.x < br->v.x)
		find_tangent(make_range(begin1, end1), ar, make_range(begin2, end2), br);
	else
		find_tangent(make_range(begin2, end2), br, make_range(begin1, end1), ar);

	if (al->v.x < bl->v.x)
		find_tangent(make_range(begin1, end1), al, make_range(begin2, end2), bl);
	else
		find_tangent(make_range(begin2, end2), bl, make_range(begin1, end1), al);

	// omit al..ar (not inclusive) and br..bl (not inclusive)
	int		nb = wrap_neg(br - bl + 1, end2 - begin2);
	I		pb;

	if (al < ar) {
		// a0..al; bl..bn; b0..br;ar..an;
		pb = al + 1;
		copy(ar, end1, pb + nb);
	} else {
		// ar..al; bl..bn; b0..br;
		copy(ar, al + 1, begin1);
		pb = begin1 + (al - ar + 1);
	}

	if (bl < br) {
		// bl..br;
		copy(bl, br + 1, pb);
	} else {
		// bl..bn; b0..br;
		copy(bl, end2, pb);
		copy(begin2, br + 1, pb + (end2 - bl));
	}
}

template<typename C> struct convex_polygon : simple_polygon<C> {
	typedef simple_polygon<C> B;
	using typename B::E;
	using typename B::helper;
	typedef diff_t<E>	V;
	using B::c;
	typedef n_plane<float, B::N>	splitter;

	explicit convex_polygon(C &&c) : simple_polygon<C>(forward<C>(c)) 	{}
	explicit convex_polygon(const point_cloud<C> &p)	: simple_polygon<C>(make_range_n(p.c.begin(), generate_hull_2d(p.c.begin(), p.c.end()))) {}

	bool		contains(param_t<E> b)									const	{ return !c.empty() && helper::convex_contains(c.begin(), c.end()); }
	E			support(V v)											const	{ return *convex_support(c, v); }
	auto		antipode(param_t<E> p0, param_t<E> p1)							{ return convex_support(c, perp(p1 - p0)); }
	E			closest(param_t<E> p0)									const	{ return *convex_closest(c.begin(), c.end(), p0); }
	bool		ray_check(param_t<ray2> r)								const	{ return convex_check_ray(c.begin(), c.end(), r.p, r.d); }
	bool		ray_check(param_t<ray2> r, float &t, V *n)				const	{ return convex_check_ray(c.begin(), c.end(), r.p, r.d, t, n); }
	float		ray_closest(param_t<ray2> r)							const	{ return convex_ray_closest(c.begin(), c.end(), r.p, r.d); }

	obb2		get_obb()												const	{ return minimum_obb(c.begin(), c.size()); }
	circle		inscribe(float eps = 1e-5)								const	{ return helper::convex_inscribe(c.begin(), c.end(), eps); }
	E			uniform_interior(param_t<V> v)							const	{ return helper::convex_uniform_interior(c.begin(), c.end(), v); }

	bool		find_split(param_t<splitter> p, float &i0, float &i1)	const	{ return iso::find_split(c.begin(), c.end(), p, i0, i1); }
	int			lop(E *out, float i0, float i1)							const	{ return lop_poly(c.begin(), c.end(), out, i0, i1) - out; }
	int			clip(E *out, param_t<splitter> p)						const	{ return clip_poly(c.begin(), c.end(), out, p) - out; }
	auto		split(E *outf, E *outb, param_t<splitter> p)			const	{ return split_poly(c.begin(), c.size32(), outf, outb, p); }

	convex_polygon&	merge(const convex_polygon &b)	{ convex_merge(c.begin(), c.end(), b.c.begin(), b.c.end()); return *this; }

	template<typename S> bool	contains_shape(const S &s)	const {
		if (B::empty())
			return false;
		for (auto i : B::lines()) {
			if (!i.test(s.support(-i.normal())))
				return false;
		}
		return true;
	}
	template<typename S> bool	overlaps_shape(const S &s)	const {
		if (B::empty())
			return false;
		for (auto i : B::lines()) {
			if (!i.test(s.support(i.normal())))
				return false;
		}
		return true;
	}
	friend float dist(const convex_polygon &p, param(position2) v) {
		return dist_to_hull(p.c, v);
	}
	friend float distance(const convex_polygon &p, param(position2) v) {
		return dist_to_hull(p.c, v);
	}
};

template<typename C> convex_polygon<C> get_hull(const point_cloud<C> &p) {
	return convex_polygon<C>(make_range_n(p.c.begin(), generate_hull_2d(p.c.begin(), p.c.end())));
}

template<typename C> auto	make_convex_polygon(C &&c)				{ return convex_polygon<C>(forward<C>(c)); }
template<typename C> auto	as_convex_polygon(point_cloud<C> &c)	{ return convex_polygon<C&>(c.c); }
template<typename C> auto	as_convex_polygon(point_cloud<C> &&c)	{ return convex_polygon<C>(move(c.c)); }
template<typename C> auto	as_convex_polygon(point_cloud<C&> &&c)	{ return convex_polygon<C&>(c.c); }

//-----------------------------------------------------------------------------
// convex_polyhedron
//-----------------------------------------------------------------------------

struct convex_polyhedron_base {
	static int	max_tris(int n)		{ return max(2 * n - 4, 0); }
	static int	max_indices(int n)	{ return max((2 * n - 4) * 3, n); }
	static int	max_edges(int n)	{ return max(3 * n - 6, 0); }
	static int	max_verts(int n)	{ return n; }	//duh
};

template<typename C> struct convex_polyhedron : convex_polyhedron_base, polyhedron<C> {
	typedef polyhedron<C>	B;
	using B::c;
	explicit convex_polyhedron(polyhedron<C> &&b) : B(move(b)) 	{}
	template<typename C2> convex_polyhedron(const convex_polyhedron<C2> &b) : B(b.c) {}
	sphere				inscribe(float epsilon = 1e-5)	const;
	convex_polyhedron&	merge(const convex_polyhedron &b);

	position3	closest(param_t<position3> p0) const {
		return *convex_closest(c.begin(), c.end(), p0);
	}
	
	plane		plane(int j)	const {
		auto a = B::points().begin() + j * 3;
		return iso::plane(a[0], a[1], a[2]);
	}
	auto		planes()		const {
		return transformc(range<int_iterator<int> >(0, B::num_tris()), [this](int i) { return plane(i); });
	}

	bool contains(param(position3) b) const {
		if (c.empty())
			return false;

		for (auto &&i : B::tris()) {
			if (i.signed_volume(b) > zero)
				return false;
		}
		return true;
	}

//	bool ray_check(param(ray3) r, float &t, vector3 *normal) const;

	float ray_closest(param(ray3) r) const {
		//TBD
		return 0;
	}

	template<typename S> bool	contains_shape(const S &s)	const {
		if (B::empty())
			return false;
		for (auto i : planes()) {
			if (i.test(s.support(i.normal())))
				return false;
		}
		return true;
	}
	template<typename S> bool	overlaps_shape(const S &s)	const {
		if (B::empty())
			return false;
		for (auto i : planes()) {
			if (i.test(s.support(-i.normal())))
				return false;
		}
		return true;
	}

	friend position3	uniform_interior(const convex_polyhedron& c, param(float3) t) {
		position3	p0		= c.points()[0];
		size_t		n		= c.num_tris();
		float		*vols	= alloc_auto(float, n), *pv = vols;
		float		vol		= 0;

		for (auto &&tri : c.tris()) {
			vol		+= tri.signed_volume(p0);
			*pv++	= vol;
		}

		float	x = t.x * vol;
		auto	i = lower_bound(vols, vols + n, x);
		return uniform_interior(tetrahedron(c.tri(i - vols + 1), p0), concat((x - i[-1]) / (i[0] - i[-1]), t.yz));
	}

	friend auto interior_optimised(const convex_polyhedron& c) {
		position3	p0		= c.points()[0];
		size_t		n		= c.num_tris();
		dynamic_array<float>	vols(n);
		auto		pv		= vols.begin();
		float		vol		= 0;

		for (auto &&tri : c.tris()) {
			vol		+= tri.signed_volume(p0);
			*pv++	= vol;
		}
		for (auto &i : vols)
			i /= vol;

		return [&c, vols = move(vols)](param(float3) t) {
			auto	i = lower_boundc(vols, t.x);
			return uniform_interior(tetrahedron(c.tri(i - vols + 1), c.points()[0]), concat((t.x - i[-1]) / (i[0] - i[-1]), t.yz));
		};
	}
	friend auto surface_optimised(const convex_polyhedron& c) {
		return surface_optimised((const B&)c);
	}
};

template<typename C> convex_polyhedron<C> as_convex_polyhedron(C &&c) {
	return convex_polyhedron<C>(forward<C>(c));
}

template<typename C> auto get_hull3d(const point_cloud<C> &c) {
	int		maxi	= convex_polyhedron_base::max_indices(c.size());
	uint16	*i		= alloc_auto(uint16, maxi);
	int		numi	= generate_hull_3d(c.begin(), c.size32(), i, 0);
	return as_convex_polyhedron(make_indexed_container(c.c, ref_array_size<uint16>(make_range_n(i, numi))));
}


//-----------------------------------------------------------------------------
// halfspace_intersection
//-----------------------------------------------------------------------------

template<typename E, int N> struct halfspace_intersection : range<const normalised<n_plane<E, N>>*> {
	typedef normalised<n_plane<E, N>>	plane;
	typedef pos<E,N>		P;
	halfspace_intersection(const range<const plane*> &t)				: range<const plane*>(t) {}
	template<typename C> halfspace_intersection(const point_cloud<C> &c, plane *p)	: range<const plane*>(p, p + generate_hull<N>(c.begin(), c.end(), p)) {}

	bool				contains(P b)	const {
		if (this->empty())
			return false;
		for (auto &i : *this) {
			if (!i.test(b))
				return false;
		}
		return true;
	}

	sphere				inscribe(float epsilon = 1e-5)	const;
	auto				points(P *v)					const	{ return make_point_cloud(v, intersection(this->begin(), this->size32(), v)); }
	const range<const plane*>&	planes()				const	{ return *this; }
	P					any_interior()					const;
	bool				clip(ray<E, N> &r, float &t0, float &t1) const;

	auto		cross_section(plane pl, P *out) const {
		return convex_polygon<range<P*>>(make_range_n(out, intersection(this->begin(), this->size32(), pl, out)));
	}

	template<typename S> bool	contains_shape(const S &s)	const {
		if (this->empty())
			return false;
		for (auto &i : *this) {
			if (!i.test(s.support(-i.normal())))
				return false;
		}
		return true;
	}
	template<typename S> bool	overlaps_shape(const S &s)	const {
		if (this->empty())
			return false;
		for (auto &i : *this) {
			if (!i.test(s.support(i.normal())))
				return false;
		}
		return true;
	}

	static int	max_planes(int n)	{ return N == 2 ? n : max(2 * n - 4, 0); }
	size_t		max_verts()			{ return N == 2 ? this->size() : max(2 * this->size() - 4, 0); }
};

// return true if some remains
template<typename E, int N> bool halfspace_intersection<E,N>::clip(ray<E,N> &r, float &t0, float &t1) const {
	float	eps = epsilon;
	t1		= infinity;
	t0		= -t1;

	for (auto &i : *this) {
		float	dir = dot(r.dir(), i.normal());
		if (abs(dir) < eps) {
			if (i.dist(r.pt0()) < -eps)
				return false;
		} else {
			float	d = i.dist(r.pt0()) / dir;
			if (dir > zero)
				t0 = max(t0, -d);
			else
				t1 = min(t1, -d);
			if (t1 < t0)
				return false;
		}
	}
	r.p	= r.from_parametric(t0);
	if (t1 != infinity)
		r.d	*= (t1 - t0);
	return true;
}

//Efficient Algorithm for Approximating Maximum Inscribed Sphere in High Dimensional Polytope
//Yulai Xie, Jack Snoeyink, Jinhui Xu

template<typename E, int N> sphere halfspace_intersection<E,N>::inscribe(float eps) const {
	P	*dual	= alloc_auto(P, this->size());

	for (P o = any_interior();;) {
		auto		trans	= translate(-o);
		P	*p		= dual;
		for (auto &i : *this) {
			plane	d = trans * i;
			*p++ = project(as_vec(d));
		}
		auto	s	= _minimum_sphere<N>(dual, this->size32());
		float	d2	= len2(s.centre().v);

		if (d2 < eps)
			return decltype(s)(o, reciprocal(s.radius()));

		o += normalise(s.centre().v) * sqrt(d2) / (s.radius2() - d2);
	}
}

typedef halfspace_intersection<float,2> halfspace_intersection2;
typedef halfspace_intersection<float,3> halfspace_intersection3;


template<typename C> sphere convex_polyhedron<C>::inscribe(float eps) const {
	return halfspace_intersection3(*this, alloc_auto(iso::plane, B::num_tris())).inscribe(eps);
}


} //namespace iso

#endif	// POLYGON_H
