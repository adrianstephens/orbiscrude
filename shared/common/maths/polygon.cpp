#include "polygon.h"
#include "simplex.h"
#include "polynomial.h"
#include "base/algorithm.h"
#include "extra/indexer.h"
#include "dynamic_vector.h"
#include "utilities.h"

#if 0
#include "gpc.h"
extern "C" {
#include "gpc.orig.h"
}
#endif

namespace iso {

//-----------------------------------------------------------------------------
//
//						2D
//
//-----------------------------------------------------------------------------

float closest_point(const convex_polygon<range<position2*>> &a, const convex_polygon<range<position2*>> &b, int &va, int &vb) {
	struct convex_polygon_track_index {
		convex_polygon<range<position2*>> p;
		mutable intptr_t	prev_i;
		position2	operator()(param(float2) v) const {
			position2	*s = convex_support(p.points(), v);
			prev_i	= p.points().index_of(s);
			return *s;
		}
		convex_polygon_track_index(const convex_polygon<range<position2*>> &p) : p(p), prev_i(0)	{}
	};

	convex_polygon_track_index	ca(a);
	convex_polygon_track_index	cb(b);
	simplex_difference<2>	simp;
	position2	pa, pb;
	float		dist;
	simp.gjk(ca, cb, a[0] - b[0], 0, 0, 0, pa, pb, dist);
	va	= (int)ca.prev_i;
	vb	= (int)cb.prev_i;
	return dist;
}


//-----------------------------------------------------------------------------
// minimum obb2
//-----------------------------------------------------------------------------

template<> obb<float,2> minimum_obb(stride_iterator<position2> p, uint32 n) {
//	p[n]	= p[0];

	auto	p0	= p;
	auto	p1	= convex_support(make_range(p + 1, p + n), perp(p[0] - p[n - 1]));

	float2		d0		= normalise(p[0] - p[n - 1]);
	float2		d1		= normalise(p1[-1] - *p1);

	float		maxx	= cross(d0, *p1 - *p0);//len(*p1 - *p0);
	float2		maxd	= d0, minp = p0->v;

	for (auto end = p1; p0 != end;) {
		float	t = cross(p0[1] - p0[0], p1[1] - p1[0]);
		if (t >= 0) {
			position2	p1a = *p1++;
			if (p1 == p + n)
				p1 = p;
			d1 = normalise(p1a - *p1);
		}
		if (t <= 0) {
			position2	p0a = *p0++;
			d0 = normalise(*p0 - p0a);
		}
		float2	d = t >= 0 ? d1 : d0;
		float	x = cross(d, *p1 - *p0);
		if (x < maxx) {
			maxd = d;
			maxx = x;
			minp = p0->v;
		}
	}

	float	miny	= dot(p[0] - minp, maxd), maxy = miny;
	for (int i = 1; i < n; i++) {
		float	y	= dot(p[i] - minp, maxd);
		miny = min(miny, y);
		maxy = max(maxy, y);
	}

	float2	y(maxd);
	float2	x(perp(y));

	return obb2(float2x3(x * (maxx / 2), y * ((maxy - miny) / 2), minp + x * (maxx / 2) + y * ((maxy + miny) / 2)));
}


//-----------------------------------------------------------------------------
// minimum triangle
//-----------------------------------------------------------------------------

#if 1


//	Linear minimum area enclosing triangle implementation
//	http://blog.ovidiuparvu.com/linear-minimum-area-enclosing-triangle-implementation/

static bool gamma(position2 &gamma, param(position2) p, const line2 &aline, const line2 &cline, float cside) {
	// Get intersection points if they exist
	float3	t	= cross(as_vec(aline), as_vec(cline));
	if (approx_equal(t.z, zero))
		return false;

	float2	d	= aline.dir() * cline.dist(p) * two;
	gamma		= position2((t.xy - d) / t.z);

	// Select the point which is on the same side of line2 C as the p
	if (cline.dist(gamma) * cside < zero)
		gamma = position2((t.xy + d) / t.z);
	return true;
}

// Intersection of line2 and p
enum {
	INTERSECTS_BELOW	= 1,
	INTERSECTS_ABOVE	= 2,
	INTERSECTS_CRITICAL	= 3,
};
static uint32 intersects(slope angleGammaAndPoint, uint32 i, stride_iterator<position2> p, uint32 n, const line2 &cline) {
	slope angle0	= slope(p[dec_mod(i, n)], p[i]);
	slope angle1	= slope(p[inc_mod(i, n)], p[i]);
	slope edge		= cline;//slope(p[dec_mod(c, n)], p[c]);

	if (between(edge, angle0, angle1) || between(edge = -edge, angle0, angle1)) {
		if (between(angleGammaAndPoint, angle0, edge) || approx_equal(angleGammaAndPoint, angle0))
			return cline.dist(p[dec_mod(i, n)]) > cline.dist(p[i]) ? INTERSECTS_ABOVE : INTERSECTS_BELOW;

		if (between(angleGammaAndPoint, angle1, edge) || approx_equal(angleGammaAndPoint, angle1))
			return cline.dist(p[inc_mod(i, n)]) > cline.dist(p[i]) ? INTERSECTS_ABOVE : INTERSECTS_BELOW;

	} else if (between(angleGammaAndPoint, angle0, angle1)
	|| (approx_equal(angleGammaAndPoint, angle0) && !approx_equal(angleGammaAndPoint, edge))
	|| (approx_equal(angleGammaAndPoint, angle1) && !approx_equal(angleGammaAndPoint, edge))
	) {
		return INTERSECTS_BELOW;
	}

	return INTERSECTS_CRITICAL;
}

// Possible values for validation flag
enum {
	VALIDATION_SIDE_A_TANGENT	= 0,
	VALIDATION_SIDE_B_TANGENT	= 1,
	VALIDATION_SIDES_FLUSH		= 2,
};

triangle minimum_triangle(stride_iterator<position2> p, uint32 n) {
//	n = generate_hull_2d(p, n);
	if (n == 3)
		return triangle(p[0], p[1], p[2]);

	float		area = maximum;
	triangle	best;

	position2	C1 = p[n - 1];
	for (uint32 c = 0, a = 1, b = 2; c < n; c++) {
		position2	C0		= C1;
		C1					= p[c];
		line2		cline	= line2(C1, C0);
		float		cside	= cline.dist(p[inc_mod(c, n)]);

		//advanceBToRightChain
		while (approx_greater_equal(abs(cline.dist(p[inc_mod(b, n)])), abs(cline.dist(p[b]))))
			b = inc_mod(b, n);

		//moveAIfLowAndBIfHigh
		position2 Agamma;
		while (abs(cline.dist(p[b])) > abs(cline.dist(p[a]))) {
			if (gamma(Agamma, p[a], line2(p[a], p[dec_mod(a,n)]), cline, cside) && intersects(slope(p[b], Agamma), b, p, n, cline) == INTERSECTS_BELOW)
				b = inc_mod(b, n);
			else
				a = inc_mod(a, n);
		}

		position2	A0		= p[dec_mod(a, n)];
		position2	A1		= p[a];
		line2		aline	= line2(A1, A0);

		//searchForBTangency
		position2	Bgamma;
		while ((gamma(Bgamma, p[b], aline, cline, cside) && intersects(slope(p[b], Bgamma), b, p, n, cline) == INTERSECTS_BELOW) && approx_greater_equal(abs(cline.dist(p[b])), abs(cline.dist(A0))))
			b = inc_mod(b, n);

		position2	B0		= p[dec_mod(b, n)];
		position2	B1		= p[b];
		line2		bline	= line2(B1, B0);

		float2	A, B, C;
		uint32	validation;

		// isNotBTangency
		if ((gamma(Bgamma, B1, aline, cline, cside) && intersects(slope(Bgamma, B1), b, p, n, cline) == INTERSECTS_ABOVE) || abs(cline.dist(B1)) < abs(cline.dist(A0))) {
			// Side B is flush with edge [b, b-1] - find middle point of side B
			if (intersection(bline, cline, A) && intersection(bline, aline, C) && abs(cline.dist(position2((A + C) / two))) < abs(cline.dist(A0))) {
				ISO_VERIFY(gamma(A1, A0, bline, cline, cside));
				aline		= line2(A1, A0);
				validation	= VALIDATION_SIDE_A_TANGENT;
			} else {
				validation	= VALIDATION_SIDES_FLUSH;
			}
		} else {
			ISO_VERIFY(gamma(B0, B1, aline, cline, cside));
			bline		= line2(B1, B0);
			validation	= VALIDATION_SIDE_B_TANGENT;
		}

		if (intersection(aline, bline, C) && intersection(aline, cline, B) && intersection(bline, cline, A)) {
			position2 Amid = position2((B + C) * half);
			position2 Bmid = position2((A + C) * half);
			position2 Cmid = position2((A + B) * half);

			if ((validation == VALIDATION_SIDE_A_TANGENT ? approx_equal(Amid, A0) : ray2(A0, A1).approx_on(Amid))
			&&	(validation == VALIDATION_SIDE_B_TANGENT ? approx_equal(Bmid, B1) : ray2(B0, B1).approx_on(Bmid))
			&&	ray2(C0, C1).approx_on(Cmid)
			) {
				triangle	tri	= triangle(position2(A), position2(B), position2(C));
				float		a	= tri.area();
				if (a < area) {
					best = tri;
					area = a;
				}
			}
		}
	}
	return best;
}

#else
triangle triangle::minimum(position2 *p, uint32 n) {
	n = generate_hull_2d(p, n);
	if (n == 3)
		return triangle(p[0], p[1], p[2]);

	float		min_area = 1e38f;
	triangle	best;

	for (int a = 1, b = 2, c = 0; c < n; ++c) {
		vector2		dc = p[c] - p[dec_mod(c, n)];
		vector2		da, db;
		position2	va, vb;

		while (cross(dc, p[inc_mod(b, n)] - p[b]) >= 0)
			b = inc_mod(b, n);

		while (cross(dc, p[b] - p[a]) > 0) {
			da	= p[a] - p[dec_mod(a, n)];
			va	= p[a] + da * cross(dc, p[a] - p[c]) / cross(dc, da);
			if (cross(p[inc_mod(b, n)] - p[b], va - p[b]) < 0)
				b = inc_mod(b, n);
			else
				a = inc_mod(a, n);
		}

		int		a0	= dec_mod(a, n);
		da	= p[a] - p[a0];

		for (;;) {
			vb	= p[a] + da * cross(dc, p[b] * 2 - p[a] - p[c]) / cross(dc, da);
			if (cross(p[b] - p[inc_mod(b, n)], vb - p[b]) <= 0 || cross(dc, p[b] - p[a0]) < 0)
				break;
			b = inc_mod(b, n);
		}

		int		ai	= a, bi = b;
		db	= p[b] - p[dec_mod(b, n)];
		if (cross(db, vb - p[b]) > 0 || cross(dc, p[b] - p[a0]) < 0) {
			vb	= p[b] + db * cross(dc, p[b] - p[c]) / cross(dc, db);
			if (cross(dc, p[b] - p[a]) < 0)
				ai = a0;
		}

		da	= vb - p[ai];
		db	= vb - p[bi];
		float	ca	= cross(dc, da);
		float	cb	= cross(dc, db);

		if (ca != 0 && cb != 0) {
			float	h = cross(dc, vb);
			ca	= dot(dc, da) * (-h / ca);
			cb	= dot(dc, db) * (-h / cb);
			float	area	= ca - cb;

			if (area < min_area) {
				min_area = area;
				float		c0	= dot(dc, vb - p[c]);
				position2	pa	= p[c] + dc * (c0 + ca) / len2(dc);
				position2	pb	= p[c] + dc * (c0 + cb) / len2(dc);
				best	= triangle(vb, pb, pa);
			}
			break;
		}
	}

	return best;
}
#endif

//-----------------------------------------------------------------------------
// minimum circle
//-----------------------------------------------------------------------------

circle minimum_circle(stride_iterator<position2> p, uint32 n, uint32 b) {
	circle m;
	switch (b) {
		case 0:	m = circle(zero);						break;
		case 1:	m = circle::with_r2(p[-1], zero);		break;
		case 2:	m = circle::through(p[-1], p[-2]);		break;
		case 3:	return circle::through(p[-1], p[-2], p[-3]);
	}

	for (uint32 i = 0; i < n; i++) {
		position2 t = p[i];
		if (!m.contains(t)) {
			for (int j = i; j > 0; j--)
				p[j] = p[j - 1];
			p[0] = t;
			m	= minimum_circle(p + 1, i, b + 1);
			((float3&)m).z *= 0.9999f;	// robustness fudge
		}
	}
	return m;
}

template<> circle _minimum_sphere(stride_iterator<position2> p, uint32 n) {
	uint32	b = n < 3 ? n : 0;
	return minimum_circle(p + b, n - b, b);
}

//-----------------------------------------------------------------------------
// minimum ellipse
//-----------------------------------------------------------------------------

// Khachiyan Algorithm
conic khachiyan(stride_iterator<position2> p, uint32 n, float eps) {
	static const int	d	= 2;				// Dimension of the points
	auto	u	= dynamic_vector<double>(n);	// u is an nx1 vector where each element is 1/n
	auto	Q	= dynamic_matrix<double3>(n);

	for (int i = 0; i < n; i++) {
		u[i]	= 1.f / n;
		Q[i]	= to<double>(float3(p[i]));
	}

	eps		= max(eps, 1e-6f);

	double err2;
	do {
		auto	X	= Q * scale(u) * transpose(Q);
		auto	M	= (transpose(Q) * inverse(X) * Q).diagonal();

		// Find the value and location of the maximum element in the vector M
		int		j	= max_component_index(M);
		auto	max	= M[j];

		// Calculate the step size for the ascent
		auto	step = (max - d - 1) / ((d + 1) * (max - 1));

		// Calculate the new_u
		u		*= 1 - step;		// multiply all elements in u by (1 - step_size)
		u[j]	+= step;			// Increment the jth element of u by step_size

		err2	= (len2(u) + 1 - u[j] * 2) * square(step);
	} while (err2 > eps);

	auto	A	= to<float>(get(inverse(Q * scale(u) * transpose(Q))));
	conic	c	= make_sym(float3x3(A[0], A[1], A[2]));
	auto	centre	= c.centre();
	return (translate(centre) * scale(sqrt(float(d))) * translate(-centre)) * c;
}

struct ellipse_min {
	conic		c1, c2;
	position2	p1, p2;
	int			n;

	ellipse_min(stride_iterator<position2> p, int n) : n(n) {
		switch (n) {
			default:
				break;
			case 3:
				c1	= conic::ellipse_through(p[0], p[1], p[2]);
				{
					auto	info = c1.analyse();
					if (!info.degenerate) {
						if (info.orientation != POS)
							c1.flip();
						break;
					}
				}
				n	= 2;
				//fallthrough
			case 2:
				p2 = p[1];
				//fallthrough
			case 1:
				p1 = p[0];
				break;
			case 4:
				conic::two_linepairs(c1, c2, p[0], p[1], p[2], p[3]);
				break;
			case 5:
				c1	= conic::through(p[0], p[1], p[2], p[3], p[4]);
				if (c1.analyse().orientation != POS)
					c1.flip();
				break;
		}
	}

	bool check_inside(param(position2) p, float eps) const;

	operator conic() const {
		if (n == 4)
			return lerp(c1, c2, vol_minimum(c1, c2 - c1));
		return c1;
	}
	operator ellipse() const {
		return operator conic();
	}
};

bool ellipse_min::check_inside(param(position2) p, float eps) const {
	switch (n) {
		default:
			return false;
		case 1:
			return approx_equal(p, p1, eps);
		case 2:
			return ray2(p1, p2).approx_on(p, eps);
		case 3:
		case 5:
			return c1.evaluate(p) >= -eps;

		case 4: {
			conic	c	= c1 * c2.evaluate(p) - c2 * c1.evaluate(p);
			conic::info i = c.analyse();
			if (i.type == conic::ELLIPSE) {
				conic	d = c2 * c1.d.x - c1 * c2.d.x;
				return get_sign1(vol_derivative(c, d)) == i.orientation;
			}

			float b = two * (c1.d.x * c2.d.y + c1.d.y * c2.d.x) - four * c1.o.x * c2.o.x;
			c = c1 * (c2.det2x2() - b) + c2 * (c1.det2x2() - b);
			return get_sign1(c.evaluate(p)) == c.analyse().orientation;
		}
	}
}

ellipse_min minimum_ellipse(stride_iterator<position2> p, uint32 n, uint32 b, float eps) {
	ellipse_min	m(p - b, b);
	for (uint32 i = 0; i < n; i++) {
		position2	t = p[i];
		if (!m.check_inside(t, eps)) {
			for (int j = i; j > 0; j--)
				p[j] = p[j - 1];
			p[0]	= t;
			m		= minimum_ellipse(p + 1, i, b + 1, eps);
		}
	}
	return m;
}

template<> ellipse minimum_ellipsoid(stride_iterator<position2> p, uint32 n, float eps) {
	switch (n) {
		case 0: return ellipse(position2(zero), float2(zero), zero);
		case 1: return ellipse(p[0], float2(zero), zero);
		case 2: return ellipse::through(p[0], p[1]);
		case 3: return ellipse::through(p[0], p[1], p[2]);
//		case 4: return ellipse::through(p[0], p[1], p[2], p[3]);
//		case 5: return ellipse::through(p[0], p[1], p[2], p[3], p[4]);
		default: return minimum_ellipse(p, n, 0, eps);
	}
}


#if 0
dynamic_array<position2> sub_poly(range<position2*> a, range<position2*> b) {
#if 0
	ConcurrentJobs::Get().add([&]() {
		dynamic_array<::gpc_vertex>	va = transformc(a, [](param(position2) v) { ::gpc_vertex g = {v.x, v.y}; return g; });
		dynamic_array<::gpc_vertex>	vb = transformc(b, [](param(position2) v) { ::gpc_vertex g = {v.x, v.y}; return g; });

		::gpc_vertex_list	ca = {va.size32(), va.begin()};
		::gpc_vertex_list	cb = {vb.size32(), vb.begin()};
		::gpc_polygon	ga, gb;

		clear(ga);
		clear(gb);

		gpc_add_contour(&ga, &ca, false);
		gpc_add_contour(&gb, &cb, false);

		::gpc_polygon	r;
		gpc_polygon_clip(::GPC_DIFF, &ga, &gb, &r);

		return transformc(make_range_n(r.contour[0].vertex, r.contour[0].num_vertices), [](const ::gpc_vertex &v) { return position2(v.x, v.y); });
	});
#endif
#if 0
	iso::gpc_polygon	ga, gb;
	ga.emplace_back(transformc(a, [](param(position2) v) { return gpc_vertex(v.x, v.y); }), false);
	gb.emplace_back(transformc(b, [](param(position2) v) { return gpc_vertex(v.x, v.y); }), false);

	auto	r = gpc_polygon_clip(GPC_DIFF, ga, gb);
	return transformc(r[0], [](const gpc_vertex &v) { return position2(v.x, v.y); });
#endif
	return none;
}
#endif

//-----------------------------------------------------------------------------
// halfspace_intersection2
//-----------------------------------------------------------------------------

//	convex polygon formed by intersection of line2's
int intersection(const normalised<n_plane<float, 2>> *p, int n, position2 *out) {
	position2 *v = out;
	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			if (colinear(p[i].normal(), p[j].normal()))
				continue;

			position2	pt	= p[i] & p[j];
			bool		outside = false;
			for (int k = 0; !outside && k < n; k++) {
				if (k != i && k != j && p[k].test(pt))
					outside = true;
			}

			if (!outside)
				*v++ = pt;
		}
	}
	n = int(v - out);
	position2	minp	= out[0];
	int			mini	= 0;
	for (int i = 1; i < n; i++) {
		if (out[i].v.y != minp.v.y ? bool(out[i].v.y < minp.v.y) : bool(out[i].v.x < minp.v.x))
			minp = out[mini = i];
	}
	swap(out[0], out[mini]);
	sort(out + 1, out + n, [minp](param(position2) a, param(position2) b) {
		return cross(a - minp, b - minp) < zero;
	});

	return n;
}

template<> position2 halfspace_intersection<float,2>::any_interior() const {
	position2	*pp	= alloc_auto(position2, size());
	return centroid(pp, pp + intersection(a, int(b - a), pp));
}

//-----------------------------------------------------------------------------
// 2d hull
//-----------------------------------------------------------------------------

bool check_inside_subhull(position2 a, position2 b, position2 *p, int n, param(position2) v) {
	position2	c;
	while (int	k = get_right_of_line(p, p + n, p, a, b, c)) {
		bool	out0 = cross(a - v, c - v) < zero;
		bool	out1 = cross(c - v, b - v) < zero;
		if (out0 && out1)
			return false;

		if (!out0 && !out1)
			return true;

		(out0 ? b : a) = c;
		n = k;
	}
	return false;
}

float dist_to_subhull(position2 a, position2 b, position2 *p, int n, param(position2) v) {
	position2	c;
	while (int k = get_right_of_line(p, p + n, p, a, b, c)) {
		auto	t		= c - v;
		bool	out0	= cross(a - v, t) < zero;
		bool	out1	= cross(t, b - v) < zero;
		if (out0 && out1)
			return len(t);

		if (!out0 && !out1)
			return 0;

		(out0 ? b : a) = c;
		n = k;
	}
	return sqrt(min(len2(a - v), len2(b - v)));
}

struct edge_links {
	struct link {
		uint16	prev, next;
		float	area;
		bool operator<(const link &b) { return area < b.area; }
	};
	temp_array<link>	links;

	edge_links(int n) : links(n) {
		for (int i = 0; i < n - 1; i++) {
			links[i + 1].prev	= i;
			links[i].next		= i + 1;
		}
		links[0].prev		= n - 1;
		links[n - 1].next	= 0;
	}

	link&	operator[](int i) {
		return links[i];
	}

	position2	get_intersection(stride_iterator<position2> p, int i) const {
		link	lnk = links[i];
		ray2	r0(p[lnk.prev], p[i]);
		ray2	r1(p[lnk.next], p[links[lnk.next].next]);
		return r0 & r1;
	}
	float	get_area(stride_iterator<position2> p, int i, param(position2) x) const {
		return triangle(p[i], x, p[links[i].next]).area();
	}
	float	get_area(stride_iterator<position2> p, int i) const {
		return get_area(p, i, get_intersection(p, i));
	}
};

int optimise_hull_2d(stride_iterator<position2> p, int n, int m) {
	if (n <= m)
		return n;

	edge_links			links(n);
	temp_dynamic_array<uint16>	indices		= int_range(n);
	temp_dynamic_array<uint16>	rev_indices	= int_range(n);

	for (int i = 0; i < n; i++)
		links.links[i].area	= links.get_area(p, i);

#if 0
	auto	double_indices	= make_double_index_iterator(rev_indices, indices);
	auto	ix				= make_index_iterator(links.links, double_indices);
	heap_make(ix, ix + n);

	for (int m1 = m; n > m1;) {
		int		i	= indices[0];
		heap_pop(ix, ix + n);
		--n;

		if (links[i].area < 0) {
			--m1;
			continue;
		}

		edge_links::link&	link	= links[i];
		int					prev	= link.prev;
		int					next	= link.next;

		p[next]				= links.get_intersection(p, i);

		links[prev].next	= next;
		links[next].prev	= prev;

		links[prev].area	= links.get_area(p, prev);
		heap_update(ix, ix + n, ix + rev_indices[prev]);

		links[next].area	= links.get_area(p, next);
		heap_update(ix, ix + n, ix + rev_indices[next]);
	}
#else
	auto	d	= make_double_index_container(rev_indices, indices);
	auto	ix	= make_index_container(links.links, d);
	auto	q	= make_priority_queue_ref(ix);

	for (int m1 = m; n > m1;) {
		int		i	= indices[0];
		q.pop();
		--n;

		if (links[i].area < 0) {
			--m1;
			continue;
		}

		edge_links::link&	link	= links[i];
		int					prev	= link.prev;
		int					next	= link.next;

		p[next]				= links.get_intersection(p, i);

		links[prev].next	= next;
		links[next].prev	= prev;

		links[prev].area	= links.get_area(p, prev);
		q.update(ix.begin() + rev_indices[prev]);

		links[next].area	= links.get_area(p, next);
		q.update(ix.begin() + rev_indices[next]);
	}
#endif
	int i = indices[0], i0;
	do {
		i0 = i;
		i = links.links[i].next;
	} while (i0 < i);

	for (int j = 0; j < m; j++) {
		p[j] = p[i];
		i = links.links[i].next;
	}
	return m;
}

position2 *optimise_polyline(position2 *d, const position2 *p, int n, float eps) {
	int stack[32], *sp = stack;

	for (;;) {
		// Find the point with the maximum distance
		float		dmax	= 0;
		int			imax	= 0;
		position2	a		= p[0], b = p[n - 1];

		for (int i = 1; i < n - 2; i++) {
			position2	t	= p[i];
			float		d	= abs(cross(a - t, b - t));
			if (d > dmax) {
				imax	= i;
				dmax	= d;
			}
		}

		// If max distance is greater than eps, recursively simplify
		if (dmax > eps * len(b - a)) {
			*sp++	= n - imax;
			n		= imax + 1;

		} else {
			*d++ = p[0];
			if (sp == stack)
				return d;
			p += n - 1;
			n = *--sp;
		}
	}
}

int optimise_polyline(position2 *p, int n, float eps, bool closed) {
	if (closed) {
		float		dmax	= 0;
		int			imax	= 0;
		position2	a		= p[0];
		for (int i = 1; i < n - 2; i++) {
			position2	t	= p[i];
			float		d	= len2(a - t);
			if (d > dmax) {
				imax	= i;
				dmax	= d;
			}
		}
		position2 *d = optimise_polyline(p, p, imax + 1, eps);
		d		= optimise_polyline(d, p + imax, n - imax, eps);
		*d++	= p[n - 1];
		return int(d - p);
	}

	position2 *d = optimise_polyline(p, p, n, eps);
	*d++	= p[n - 1];
	return int(d - p);
}

int optimise_polyline_verts(position2 *p, int n, int verts, bool closed) {
	float	f	= float(verts) / n;
	float	per	= simple_polygon<range<position2*>>(make_range_n(p, n)).perimeter();
	float	eps	= per * f;

	position2	*d	= alloc_auto(position2, n);
	int			current;

	while ((current = int(optimise_polyline(d, p, n, eps) - d)) < verts)
		eps /= 2;

	while ((current = int(optimise_polyline(d, p, n, eps) - d)) > verts)
		eps *= 2;

	float	e0 = eps / 2, e1 = eps;
	while (current != verts && e1 - e0 > e0 * (float)epsilon) {
		eps		= (e0 + e1) / 2;
		current = int(optimise_polyline(d, p, n, eps) - d);
		if (current < verts)
			e1 = eps;
		else if (current > verts)
			e0 = eps;
	}
	return optimise_polyline(p, n, eps, closed);
}

int expand_polygon(const position2 *p, int n, float x, position2 *result) {
	position2	*d		= result;
	if (n == 1) {
		*d++ = *p + float2{-x, -x};
		*d++ = *p + float2{-x, +x};
		*d++ = *p + float2{+x, +x};
		*d++ = *p + float2{+x, -x};

	} else if (n) {
		position2	pos0	= p[n - 1];
		position2	pos1	= p[0];
		float2		d1		= normalise(pos1 - pos0);

		for (int i = 0; i < n; i++) {
			float2	d0	= d1;

			pos0	= pos1;
			pos1	= p[i + 1 == n ? 0 : i + 1];

			if (!all(pos0 == pos1)) {
				d1		= normalise(pos1 - pos0);

				if (dot(d0, d1) < -.001f) {
					*d++ = pos0 + (perp(d0) + d0) * x;
					*d++ = pos0 + (perp(d1) - d1) * x;
				} else {
					float t	= cross(d0, d1);
					*d++ = pos0 + (t == 0 ? perp(d1) : (d1 - d0) / t) * x;
				}
			}
		}
	}
	return int(d - result);
}

//-----------------------------------------------------------------------------
//
//						3D
//
//-----------------------------------------------------------------------------

position3 *clip_plane_helper(param(float4) p, position3 *out) {
	out[2]	= {-one, -one, -(p.w - p.x - p.y) / p.z};
	out[3]	= {+one, -one, -(p.w + p.x - p.y) / p.z};
	out[4]	= {+one, +one, -(p.w + p.x + p.y) / p.z};
	out[5]	= {-one, +one, -(p.w - p.x + p.y) / p.z};

	auto	end	= clip_poly(out + 2, out + 6, out + 1, unit_plane<PLANE_MINUS_Z>());
	return clip_poly(out + 1, end, out, unit_plane<PLANE_PLUS_Z>());
}

position3 *clip_plane(const n_plane<float, 3> &p, position3 *out) {
	switch (max_component_index(abs(p.normal()))) {
		default: {
			for (auto *end = clip_plane_helper(as_vec(p).yzxw, out); out != end; ++out)
				*out = position3(out->v.zxy);
			return out;
		}
		case 1:
			for (auto *end = clip_plane_helper(as_vec(p).zxyw, out); out != end; ++out)
				*out = position3(out->v.yzx);
			return out;
		case 2:
			return clip_plane_helper(as_vec(p), out);
	}
}

//-----------------------------------------------------------------------------
// minimum obb3
//-----------------------------------------------------------------------------

template<> obb<float,3> minimum_obb(stride_iterator<position3> p, uint32 n) {
	position3	c = centroid(p, p + n);

	symmetrical3 covariance(zero, zero);
	for (int i = 0; i < n; i++) {
		float3		a	= p[i] - c;
		covariance.d	+= a * a;
		covariance.o	+= float3(a.xyx) * a.yzz;
	}

	float3		v	= first_eigenvector(covariance);
	quaternion	q	= quaternion::between(x_axis, v);
//	quaternion	q	= diagonalise(float3x3(covariance));
	float3x4	m	= inverse(q) * translate(-c);

	position3	a = m * p[0], b = a;
	for (unsigned i = 1; i < n; i++) {
		position3	t = m * p[i];
		a = min(a, t);
		b = max(b, t);
	}

	return obb3(translate(c) * q * translate(mid(a, b)) * iso::scale((b - a) * half));
}

//-----------------------------------------------------------------------------
// minimum sphere
//-----------------------------------------------------------------------------

sphere minimum_sphere(stride_iterator<position3> p, uint32 n, uint32 b) {
	sphere m;
	switch (b) {
		case 0:	m = sphere(zero);							break;
		case 1:	m = sphere(p[-1], zero);					break;
		case 2:	m = sphere::through(p[-1], p[-2]);			break;
		case 3:	m = sphere::through(p[-1], p[-2], p[-3]);	break;
		case 4:	return sphere::through(p[-1], p[-2], p[-3], p[-4]);
	}

	for (uint32 i = 0; i < n; i++) {
		position3 t = p[i];
		if (!m.contains(t)) {
			for (int j = i; j > 0; j--)
				p[j] = p[j - 1];
			p[0] = t;
			m = minimum_sphere(p + 1, i, b + 1);
			m *= 1.0001f;
		}
	}
	return m;
}

template<> sphere _minimum_sphere(stride_iterator<position3> p, uint32 n) {
	uint32	b = n < 4 ? n : 0;
	return minimum_sphere(p + b, n - b, b);
}

//-----------------------------------------------------------------------------
// minimum ellipsoid
//-----------------------------------------------------------------------------

// Khachiyan Algorithm
quadric khachiyan(stride_iterator<position3> p, uint32 n, float eps) {
	static const int	d	= 3;				// Dimension of the points
	auto	u	= dynamic_vector<double>(n);	// u is an nx1 vector where each element is 1/n
	auto	Q	= dynamic_matrix<double4>(n);

	for (int i = 0; i < n; i++) {
		u[i]	= 1.f / n;
		Q[i]	= to<double>(float4(p[i]));
	}

	eps		= max(eps, 1e-6f);

	double err2;
	do {
		auto	X	= Q * scale(u) * transpose(Q);
		auto	M	= (transpose(Q) * inverse(X) * Q).diagonal();

		// Find the value and location of the maximum element in the vector M
		int		j	= max_component_index(M);
		auto	max	= M[j];

		// Calculate the step size for the ascent
		auto	step = (max - d - 1) / ((d + 1) * (max - 1));

		// Calculate the new_u:
		u		*= 1 - step;		// multiply all elements in u by (1 - step_size)
		u[j]	+= step;			// Increment the jth element of u by step_size

		err2	= (len2(u) + 1 - u[j] * 2) * square(step);
	} while (err2 > eps);

	auto	A	= to<float>(get(inverse(Q * scale(u) * transpose(Q))));
	quadric	c	= make_sym(float4x4(A[0], A[1], A[2], A[3]));
	auto	centre	= c.centre();
	return (translate(centre) * scale(sqrt(float(d))) * translate(-centre)) * c;
}

#if 1
template<> ellipsoid minimum_ellipsoid(stride_iterator<position3> p, uint32 n, float eps) {
	return khachiyan(p, n, eps);
}

#else
struct ellipsoid_min {
	quadric		q1, q2;
	position3	p1, p2, p3;
	int			n;

	ellipsoid_min(stride_iterator<position3> p, int n) : n(n) {
		switch (n) {
			default:
				break;
			case 4:
				q1	= quadric::sphere_through(p[0], p[1], p[2], p[3]);
				{
					auto	info = q1.analyse();
					if (!info.degenerate) {
						if (info.orientation != POS)
							q1.flip();
						break;
					}
				}
				n	= 3;
				//fallthrough
			case 3:
				p3 = p[2];
				//fallthrough
			case 2:
				p2 = p[1];
				//fallthrough
			case 1:
				p1 = p[0];
				break;
			case 6:
				q1	= quadric::ellipsoid_through(p[0], p[1], p[2], p[3], p[4], p[5]);
				break;
			case 7:
				q1	= quadric::ellipsoid_through(p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
				break;
			case 8:
				q1	= quadric::ellipsoid_through(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
				break;
			case 9:
				q1	= quadric::through(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
				break;

		}
	}

	bool check_inside(param(position3) p, float eps) const;

	operator quadric() const {
		ISO_ASSERT(n > 4);
		return q1;
	}
	operator ellipsoid() const {
		return operator quadric();
	}
};

bool ellipsoid_min::check_inside(param(position3) p, float eps) const {
	switch (n) {
		default:
			return false;
		case 1:
			return approx_equal(p, p1, eps);
		case 2:
			return ray3(p1, p2).approx_on(p, eps);
		case 3:
			return plane(p1, p2, p3).approx_on(p, eps);
		case 5:
			return q1.evaluate(p) >= -eps;
	}
}

ellipsoid_min minimum_ellipsoid(stride_iterator<position3> p, uint32 n, uint32 b, float eps) {
	ellipsoid_min	m(p -b, b);

	for (uint32 i = 0; i < n; i++) {
		position3	t = p[i];
		if (!m.check_inside(t, eps)) {
			for (int j = i; j > 0; j--)
				p[j] = p[j - 1];
			p[0]	= t;
			m		= minimum_ellipsoid(p + 1, i, b + 1, eps);
		}
	}
	return m;
}

template<> ellipsoid minimum_ellipsoid(stride_iterator<position3> p, uint32 n, float eps) {
	switch (n) {
		case 0: return ellipsoid(position3(zero), float3(zero), float3(zero), zero);
		case 1: return ellipsoid(p[0], float3(zero), float3(zero), zero);
			//		case 2: return ellipsoid::through(p[0], p[1]);
			//		case 3: return ellipsoid::through(p[0], p[1], p[2]);
			//		case 4: return ellipsoid::through(p[0], p[1], p[2], p[3]);
			//		case 5: return ellipsoid::through(p[0], p[1], p[2], p[3], p[4]);
		default: return minimum_ellipsoid(p, n, 0, eps);
	}
}
#endif
//-----------------------------------------------------------------------------
// polyhedron
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// convex_polyhedron
//-----------------------------------------------------------------------------

float closest_point(const convex_polyhedron<range<position3*>> &a, const convex_polyhedron<range<position3*>> &b, int &va, int &vb) {
	struct convex_polyhedron_track_index {
		convex_polyhedron<range<position3*>> p;
		mutable intptr_t	prev_i;
		position3	operator()(param(float3) v) const {
			position3	*s = convex_support(p.points(), v);
			prev_i	= p.points().index_of(s);
			return *s;
		}
		convex_polyhedron_track_index(const convex_polyhedron<range<position3*>> &p) : p(p), prev_i(0)	{}
	};

	convex_polyhedron_track_index	ca(a);
	convex_polyhedron_track_index	cb(b);
	simplex_difference<3>	simp;
	position3	pa, pb;
	float		dist;
	simp.gjk(ca, cb, a[0] - b[0], 0, 0, 0, pa, pb, dist);
	va	= (int)ca.prev_i;
	vb	= (int)cb.prev_i;
	return dist;
}

//-----------------------------------------------------------------------------
// halfspace_intersection
//-----------------------------------------------------------------------------

int intersection(const plane *p, int n, param(plane) pl, position3 *out) {
	float3x4	imat	= from_xy_plane(normalise(pl));
	line2		*lines	= alloc_auto(line2, n);

	int			nl		= 0;
	for (int i = 0; i < n; i++) {
		if (!colinear(pl.normal(), p[i].normal())) {
			line2	l = p[i] / imat & xy_plane;
			lines[nl++] = l;
		}
	}

	position2	*pts	= alloc_auto(position2, n);
	int			nv		= intersection(lines, nl, pts);
	for (int i = 0; i < nv; i++)
		out[i] = imat * position3(pts[i], zero);
	return nv;
}

#if 1

template<> position3 halfspace_intersection<float,3>::any_interior() const {
	float4	t = zero;
	for (auto &i : *this)
		t += as_vec(i);
	
	position3	c	= position3(t);
	ISO_ASSERT(contains(c));
	return c;
}

#elif 0
template<> position3 halfspace_intersection<float,3>::any_interior() const {
	for (const plane *i = a; i < b; ++i) {
		const plane	*j = i + 1;
		while (j < b && colinear(i->normal(), j->normal()))
			++j;

		if (j < b) {
			plane		bi	= bisector(*i, *j);
			position3	*pp	= alloc_auto(position3, size());
			auto		cx	= cross_section(bi, pp);
			if (!cx.empty()) {
				position3	c	= cx.centroid();
				ISO_ASSERT(contains(c));
				return c;
			}
		}
	}
	return position3(zero);
}

#else

template<> position3 halfspace_intersection<float,3>::any_interior() const {
	for (const plane *i = a; i < b; ++i) {
		const plane	*j = i + 1;
		while (j < b && colinear(i->normal(), j->normal()))
			++j;

		if (j < b) {
			ray3	line	= *i & *j;
			float	t0, t1;
			if (clip(line, t0, t1)) {
				// got edge

				position3	c = line.centre();

				for (int k = 0; k < 2; k++) {
					float			maxd = 0;
					const plane*	maxi = 0;
					for (const plane *i = a; i < b; ++i) {
						float	d = i->normalised_dist(c);
						if (d > maxd) {
							maxd = d;
							maxi = i;
						}
					}
					c -= normalise(maxi->normal()) * maxd / 2;
				}
				ISO_ASSERT(contains(c));
				return c;
			}
		}
	}
	return position3(zero);
}
#endif

//-----------------------------------------------------------------------------
// 3D convex hull
//-----------------------------------------------------------------------------

struct HullMaker {
	enum {
		kInside		= -1,
		kUnknown	= -2,
	};

#if 0
	struct Tri {
		int			vert[3];		// vertex indices in clockwise order
		int			neighbour[3];	// neighbouring triangle indices, with neighbour[i] corresponding to edge (vert[i], vert[(i+1)%3])
		plane		p;				// plane equation

		Tri() {}
		Tri(int vi0, int vi1, int vi2, int ni0, int ni1, int ni2, position3* verts) {
			vert[0]			= vi0;
			vert[1]			= vi1;
			vert[2]			= vi2;
			neighbour[0]	= ni0;
			neighbour[1]	= ni1;
			neighbour[2]	= ni2;
			p	= plane(verts[vert[0]], verts[vert[1]], verts[vert[2]], false);
		}
		bool	outside(param(position3) pos) {
			return p.dist(pos) > zero;
		}
		plane	get_plane() const {
			return p;
		}
	};
#else
	//normal + point (helps accuracy)
	struct Tri {
		int			vert[3];		// vertex indices in clockwise order
		int			neighbour[3];	// neighbouring triangle indices, with neighbour[i] corresponding to edge (vert[i], vert[(i+1)%3])
		float3		n;
		position3	v0;

		Tri() {}
		Tri(int vi0, int vi1, int vi2, int ni0, int ni1, int ni2, const position3* verts) {
			vert[0]			= vi0;
			vert[1]			= vi1;
			vert[2]			= vi2;
			neighbour[0]	= ni0;
			neighbour[1]	= ni1;
			neighbour[2]	= ni2;
			n	= cross(verts[vi1] - verts[vi0], verts[vi2] - verts[vi0]);
			v0	= verts[vi0];
		}
		bool	outside(param(position3) pos) {
			return dot(pos - v0, n) > zero;
		}
		plane	get_plane() const {
			return {n, v0};
		}
	};
#endif

	struct Vertex {
		int tri, tri_edge, outside;
		Vertex() : tri(kUnknown), outside(kUnknown) {}
	};

	dynamic_array<Tri>		tris;
	dynamic_array<Vertex>	records;

	// defragment back faces, and fix up the neighbour indices
	void Fixup(int *map) {
		// fixup triangle neighbours
		for (auto &tri : tris) {
			for (int j = 0; j < 3; j++) {
				int n = tri.neighbour[j];
				if (n >= 0)
					tri.neighbour[j] = map[n];
			}
		}
		// fix up 'outside' relationships
		for (auto &rec : records) {
			if (rec.outside >= 0)
				rec.outside = map[rec.outside];
		}
	}

	// remove front faces from the hull, defragment back faces, and fix up the neighbour indices
	void RemoveFrontFaces(param(position3) apex) {
		int		*map	= alloc_auto(int, tris.size()), *pmap = map;
		int		n1		= 0;

		for (auto &tri : tris) {
			if (tri.outside(apex)) {
				*pmap++		= kUnknown;
			} else {
				tris[n1]	= tri;
				*pmap++		= n1++;
			}
		}
		tris.resize(n1);
		Fixup(map);
	}

	void RemoveVertex(int x) {
		int		*map	= alloc_auto(int, tris.size()), *pmap = map;
		int		n1		= 0;

		for (auto &tri : tris) {
			if (tri.vert[0] == x || tri.vert[1] == x || tri.vert[2] == x) {
				*pmap++		= kUnknown;
			} else {
				tris[n1]	= tri;
				*pmap++		= n1++;
			}
		}
		tris.resize(n1);
		Fixup(map);
	}

	// compute the clockwise horizon and cap it
	void AddCap(const position3* verts, int apex) {
		int x0			= -1;
		int num_edges	= 0;

		// create an edge for each face connected to an 'unknown' face (where a back face was removed)
		for (int i = 0; i < tris.size32(); i++) {
			for (int j = 0; j < 3; j++) {
				if (tris[i].neighbour[j] == kUnknown) {
					// get the edge's vertex indices
					x0 = tris[i].vert[(j + 1) % 3];

					// check this point isn't already on horizon
					if (records[x0].tri != kUnknown) {
						for (auto &rec : records)
							rec.tri = kUnknown;
						RemoveVertex(x0);
						num_edges	= 0;
						i			= -1;	// start again
						break;
					}

					// create the edge
					records[x0].tri			= i;
					records[x0].tri_edge	= j;
					num_edges++;
				}
			}
		}

		int cap	= tris.size32();
		for (int i = 0; i < num_edges; i++) {
			int tri			= records[x0].tri;
			int tri_edge	= records[x0].tri_edge;
			int x1			= tris[tri].vert[tri_edge];

			records[x0].tri = kUnknown;//debug

			tris.emplace_back(
				apex, x0, x1,
				cap + dec_mod(i, num_edges), tri, cap + inc_mod(i, num_edges),
				verts
			);

			// connect to it to from the back face over the horizon
			tris[tri].neighbour[tri_edge] = cap + i;
			x0	= x1;
		}
		records[x0].tri = kUnknown;//debug
	}


	HullMaker(const position3* verts, int num_verts, int steps) : records(num_verts) {
		// find the point furthest in each of 4 tetrahedral directions and classify each as inside

		int		i0 = 0, i1 = 0, i2 = 0, i3 = 0;
		float	max_d;
		//i0 = leftmost point
		for (int i = 1; i < num_verts; i++) {
			if (verts[i].v.x < verts[i0].v.x)
				i0 = i;
		}
		records[i0].outside = kInside;

		//i1 = furthest point from i0
		position3	v0 = verts[i0];
		max_d = -1;
		for (int i = 0; i < num_verts; i++) {
			float	d = len2(verts[i] - v0);
			if (d > max_d) {
				max_d	= d;
				i1		= i;
			}
		}
		records[i1].outside = kInside;

		//i2 = furthest point from line i0-i1
		float3	d1 = verts[i1] - v0;
		max_d = -1;
		for (int i = 0; i < num_verts; i++) {
			if (records[i].outside != kInside) {
				float	d = len2(cross(verts[i] - v0, d1));
				if (d > max_d) {
					max_d	= d;
					i2		= i;
				}
			}
		}
		records[i2].outside = kInside;

		//i3 = furthest point from plane i0-i1-i2
		float3	n2 = cross(verts[i2] - v0, d1);
		max_d = -1;
		for (int i = 0; i < num_verts; i++) {
			if (records[i].outside != kInside) {
				float	d = abs(dot(verts[i] - v0, n2));
				if (d > max_d) {
					max_d	= d;
					i3		= i;
				}
			}
		}
		records[i3].outside = kInside;

		// ensure triangle (1,2,3) is clockwise from outside the tetrahedron
		plane	p(verts[i1], verts[i2], verts[i3]);
		if (p.dist(verts[i0]) > zero)
			swap(i0, i1);

//		if (signed_volume(verts[i0], verts[i2], verts[i3], verts[i1]) < zero)
//			swap(i0, i1);

		// set up faces (i0,i2,i1), (i0,i3,i2), (i0,i1,i3), (i1,i2,i3)
		tris.emplace_back(i0, i2, i1, 1, 3, 2, verts);
		tris.emplace_back(i0, i3, i2, 2, 3, 0, verts);
		tris.emplace_back(i0, i1, i3, 0, 3, 1, verts);
		tris.emplace_back(i1, i2, i3, 0, 1, 2, verts);

		// loop till there are no vertices left outside the hull
		for (int apex = 0; apex < steps; apex++) {
			// classify remaining verts
			for (int i = apex; i < num_verts; i++) {
				int	outside = kInside;
				if (records[i].outside == kUnknown) {
					for (auto& tri : tris) {
						if (tri.outside(verts[i])) {
							outside = tris.index_of(tri);
							break;
						}
					}
					records[i].outside = outside;
				}
			}

			while (records[apex].outside == kInside) {
				if (++apex == num_verts)
					return;
			}

			// partition tris into front and back-facing with respect to the apex; remove the front faces and defragment
			RemoveFrontFaces(verts[apex]);

			// compute the horizon as seen from the apex, and create a clockwise cap of faces joining the apex to the horizon
			AddCap(verts, apex);

			records[apex].outside = kInside;
		}
	}
};

int generate_hull_3d(const position3 *p, int n, uint16 *indices, plane *planes) {
	if (n <= 3) {
		if (planes) {
			if (n == 3)
				planes[0] = plane(p[0], p[1], p[2]);
			if (!indices)
				return int(n == 3);
		}
		// make hull just a vertex, line, or triangle
		for (int i = 0; i < n; i++)
			indices[i] = i;
		return n;
	}

	HullMaker	hull(p, n, n);

	if (planes) {
		plane		*out = planes;
		for (auto &tri : hull.tris)
			*out++ = -tri.get_plane();
		if (!indices)
			return int(out - planes);
	}
	uint16		*out = indices;
	for (auto &tri : hull.tris) {
		for (int i = 0; i < 3; i++)
			*out++ = tri.vert[i];
	}
	return int(out - indices);
}

int generate_hull_3d_partial(const position3 *p, int n, uint16 *indices, int steps) {
	if (n < 3) {
		if (indices) {
			// make hull just a vertex or line
			for (int i = 0; i < n; i++)
				indices[i] = i;
			return n;
		}
		return 0;//no planes
	}

	HullMaker	hull(p, n, steps);

	uint16		*out = indices;
	for (auto &tri : hull.tris) {
		for (int i = 0; i < 3; i++)
			*out++ = tri.vert[i];
	}
	return int(out - indices);
}

//-----------------------------------------------------------------------------
//	planes
//-----------------------------------------------------------------------------

int intersection(const plane *p, int n, position3 *out) {
	position3 *v = out;
	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			if (colinear(p[i].normal(), p[j].normal()))
				continue;

			line3	r	= p[i] & p[j];
			float	t1	= infinity;
			float	t0	= -t1;

			for (int k = j + 1; t0 < t1 && k < n; k++) {
				float	dir = dot(r.dir(), p[k].normal());
				if (dir == zero) {
					if (p[k].dist(r.p) < zero)
						t0 = t1;
				} else {
					float	d = p[k].dist(r.p) / dir;
					if (dir > zero)
						t0 = max(t0, -d);
					else
						t1 = min(t1, -d);
				}
			}
			if (t0 < t1 && t1 != infinity)
				*v++ = r.from_parametric(t1);
		}
	}
	return int(v - out);
}

}  // namespace iso
