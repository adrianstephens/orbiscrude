#include "main.h"
#include "base/block.h"
#include "base/algorithm.h"
#include "iso/iso.h"
#include "iso/iso_convert.h"
#include "maths/geometry.h"
#include "maths/geometry_iso.h"
#include "maths/sobol.h"
#include "maths/triangulate.h"
#include "maths/tesselate.h"
#include "maths/simplex.h"
#include "maths/bezier.h"
#include "maths/bspline.h"
#include "maths/martinez.h"
#include "vector_iso.h"
#include "vector_string.h"
#include "2d/fonts.h"
#include "windows/d2d.h"
#include "extra/random.h"

using namespace app;

static position2	global_point;

typedef convex_polygon<ref_array_size<position2>> ref_polygon;

struct temp_points2 : auto_block<position2, 1> {
	int	_size;
	auto				cloud()											const	{ return make_point_cloud(begin(), _size); }
	int					size()											const	{ return _size; }
	bool				empty()											const	{ return _size == 0; }
	auto				end()											const	{ return begin() + _size; }
	position2			front()											const	{ return *begin(); }
	position2			back()											const	{ return begin()[_size - 1]; }
	rectangle			get_box()										const	{ return get_extent(*this);	}
	bool				contains(param(position2) p)					const	{ return make_polygon(cloud()).contains(p); }
	position2			centre()										const	{ return centroid(*this); }
	position2			closest(param(position2) p)						const	{ return get_hull(cloud()).closest(p); }
	position2			support(param(float2) v)						const	{ return get_hull(cloud()).support(v); }
	bool				ray_check(param(ray2) r, float &t, float2 *normal) const{ return get_hull(cloud()).ray_check(r, t, normal); }
	float				ray_closest(param(ray2) r)						const	{ return get_hull(cloud()).ray_closest(r); }
	friend position2	uniform_interior(const temp_points2 &s, param(float2) v)	{ return get_hull(s.cloud()).uniform_interior(v); }
	friend position2	uniform_perimeter(const temp_points2 &s, float t)		{ return make_polygon(s.cloud()).uniform_perimeter(t); }
	friend float		area(const temp_points2 &s)								{ return as_simple_polygon(s.cloud()).area(); }
	friend float		dist(const temp_points2 &s, param(position2) p)			{ using iso::dist; return distance(get_hull(s.cloud()), p); }
	friend position2	any_point(const temp_points2 &s)						{ return *s.begin(); }

	temp_points2() : _size(0) {}
	temp_points2(const ISO_openarray<float2p> &p, int extra = 0) : auto_block<position2, 1>(make_auto_block<position2>(p.Count() + extra)), _size(p.Count()) {
		const float2p	*j = p.begin();
		for (auto &i : *this)
			i = position2(*j++);
	}
	temp_points2(const position2 *p, int size) : auto_block<position2, 1>(make_auto_block<position2>(size)), _size(size) {
		for (auto &i : *this)
			i = *p++;
	}
};

template<int N> struct bezier2dN : bezier_splineT<float2, N> {
	typedef bezier_splineT<float2, N> B;
//	bezier2dN(float2 *p) : B{p[0], p[1], p[2]} {}
	bezier2dN&	operator*=(const float2x3 &m)	{ *(B*)this = m * *this; return *this; }
	position2			closest(param(position2) p)						const	{ return position2(B::closest(p, 0, 0)); }
	bool				contains(param(position2) p)					const	{ return len2(closest(p) - p) < 0.01f; }
	auto				centre()										const	{ return get_box().centre(); }
	float				ray_closest(param(ray2) r)						const	{ return 0; }
	friend position2	uniform_perimeter(const bezier2dN &b, float t)			{ return position2(b.evaluate(t)); }
	friend position2	uniform_interior(const bezier2dN &b, param(float2) v)	{ return uniform_perimeter(b, v.x);	}
	friend float		area(const bezier2dN &b)								{ return 0; }
	friend position2	any_point(const bezier2dN &b)							{ return position2(b[0]); }
};

typedef bezier2dN<2> bezier2d2;
typedef bezier2dN<3> bezier2d;

template<typename T, int N, typename C> struct bezier_chain2dN : bezier_chain<T, N, C> {
	typedef bezier_chain<T, N, C> B;
	bezier_chain2dN(const bezier_chain2dN &b)		: B(copy(b.c)) {}
	bezier_chain2dN(B &&b)		: B(move(b.c)) {}
	bezier_chain2dN(const B &b)	: B(copy(b.c)) {}
	bezier_chain2dN(const C &b) : B(copy(b)) {}
	bezier_chain2dN&	operator*=(const float2x3 &m) {
		for (auto &i : c)
			i = m * position2(i);
		return *this;
	}
	position2			closest(param(position2) p)						const	{ return position2(B::closest(p.v, 0, 0)); }
	bool				contains(param(position2) p)					const	{ return len2(closest(p) - p) < 0.01f; }
	auto				centre()										const	{ return get_box().centre(); }
	float				ray_closest(param(ray2) r)						const	{ return 0; }

	friend position2	uniform_perimeter(const bezier_chain2dN &b, float t)		{ return position2(b.evaluate(t * b.size())); }
	friend position2	uniform_interior(const bezier_chain2dN &b, param(float2) v)	{ return uniform_perimeter(b, v.x);	}
	friend float		area(const bezier_chain2dN &b)								{ return 0; }
	friend position2	any_point(const bezier_chain2dN &b)							{ return position2(b.c[0]); }
};

typedef bezier_chain2dN<float2p, 2, ISO::OpenArray<float2p>> bezier_chain2d2;
typedef bezier_chain2dN<float2p, 3, ISO::OpenArray<float2p>> bezier_chain2d;

template<typename T, int N> auto	to_bez(const bspline<T, N> &spline) {
	auto	bez		= to_beziers(spline);
	bezier_chain<T, N>	chain;
	chain.add((bezier_splineT<T, N>*)bez.c.begin(), bez.c.size() / (N + 1));
	return chain;
}


template<typename T, int N> struct bspline2dN : bspline<T, N> {
	typedef bspline<T, N> B;

	bspline2dN(const ISO::bspline2d &b)		: B(b.c, b.k) {}
	bspline2dN(const ISO::bspline2d2 &b)	: B(b.c, b.k) {}
	bspline2dN(const bspline<float2p,N> &b)	: B(b.c, copy(b.k)) {}
	bspline2dN&	operator*=(const float2x3 &m)	{ 
		for (auto &i : c)
			i = m * position2(i);
		return *this;
	}
	position2			support(param(float2) v)						const	{ return position2(to_bez(*this).support(v)); }
	position2			closest(param(position2) p)						const	{ return position2(to_bez(*this).closest(p.v, 0, 0)); }
	bool				contains(param(position2) p)					const	{ return len2(closest(p) - p) < 0.01f; }
	auto				centre()										const	{ return get_box().centre(); }
	float				ray_closest(param(ray2) r)						const	{ return 0; }
	bool				ray_check(param(ray2) r, float &t, float2 *normal) const{ return to_bez(*this).ray_check(r, t, normal); }
	friend position2	uniform_perimeter(const bspline2dN &b, float t)			{ return position2(b.evaluate(b.domain().from(t))); }
	friend position2	uniform_interior(const bspline2dN &b, param(float2) v)	{ return uniform_perimeter(b, v.x);	}
	friend float		area(const bspline2dN &b)								{ return 0; }
	friend position2	any_point(const bspline2dN &b)							{ return position2(b.c[0]); }
};

typedef bspline2dN<float2, 3> bspline2d;
typedef bspline2dN<float2, 2> bspline2d2;

template<typename T, int N> auto	to_bez(const rational<bspline<T, N>> &spline) {
	return make_rational(to_bez((const bspline<T, N>&)spline));
}

template<typename T, int N> struct nurbs2dN : rational<bspline<T, N>> {
	typedef rational<bspline<T, N>> B;

	nurbs2dN(const ISO::nurbs2d &b)		: B(b.c, b.k) {}
	nurbs2dN(const ISO::nurbs2d2 &b)	: B(b.c, b.k) {}
	nurbs2dN&	operator*=(const float2x3 &m)	{ 
		//for (auto &i : c)
		//	i = m * i;
		return *this;
	}
	position2			support(param(float2) v)						const	{ return position2(to_bez(*this).support(v)); }
	position2			closest(param(position2) p)						const	{ return to_bez(*this).closest(p, 0, 0); }
	bool				contains(param(position2) p)					const	{ return len2(closest(p) - p) < 0.01f; }
	auto				centre()										const	{ return any_point(*this); }//get_box().centre(); }
	float				ray_closest(param(ray2) r)						const	{ return 0; }
	bool				ray_check(param(ray2) r, float &t, float2 *normal) const{ return to_bez(*this).ray_check(r, t, normal); }
	friend position2	uniform_perimeter(const nurbs2dN &b, float t)			{ return position2(b.evaluate(b.domain().from(t))); }
	friend position2	uniform_interior(const nurbs2dN &b, param(float2) v)	{ return uniform_perimeter(b, v.x);	}
	friend float		area(const nurbs2dN &b)									{ return 0; }
	friend position2	any_point(const nurbs2dN &b)							{ return project(b.c[0]); }
};

typedef nurbs2dN<float3, 2> nurbs2d2;
typedef nurbs2dN<float3, 3> nurbs2d;

//-----------------------------------------------------------------------------
//	Drawing
//-----------------------------------------------------------------------------

void DrawPoint(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, param(position2) p, uint32 size = 4) {
	position2	a = transform * p;
	d2d.Fill(rectangle(a - size, a + size), brush);
}

template<typename C> void DrawPoints(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, C&& c, uint32 size = 4) {
	for (auto &&i : c)
		DrawPoint(d2d, transform, brush, i, size);
}

void DrawLine(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, param(position2) a, param(position2) b) {
	d2d.DrawLine(transform * a, transform * b, brush, 1);
}

template<typename C> void DrawLines(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, C &&c, bool loop = true) {
	if (!is_empty(c)) {
		position2	a = loop ? back(c) : front(c);
		for (position2 b : slice(c, loop ? 0 : 1)) {
			DrawLine(d2d, transform, brush, a, b);
			a = b;
		}
	}
}

template<typename C> void FillPoly(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, C &&c) {
	if (!is_empty(c)) {
		d2d::Geometry	geom(d2d);
		{
			auto	sink = geom.Open();
			sink->BeginFigure(d2d::point(transform * front(c)), D2D1_FIGURE_BEGIN_FILLED);
			for (position2 b : slice(c, 1))
				sink->AddLine(d2d::point(transform * b));
			sink->EndFigure(D2D1_FIGURE_END_CLOSED);
		}
		d2d.Fill(geom, brush);
	}
}

template<typename C> void DrawPoly(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, C &&c) {
	if (fill)
		FillPoly(d2d, transform, fill, c);
	DrawLines(d2d, transform, line, c, true);
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const circle &c) {
	position2	centre	= transform * c.centre();
	vector2		radii	= transform * vector2(c.radius());
	d2d::ellipse	ellipse(centre, radii);
	if (fill)
		d2d.FillEllipse(ellipse, fill);
	d2d.DrawEllipse(ellipse, line);
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const ellipse &e) {
	float2		a		= normalise(e.major());
	float2x3	m(a, perp(a), transform * e.centre());
	d2d.SetTransform(m);
	d2d::ellipse	ellipse(d2d::point(0, 0), len(transform * e.major()), len(transform * e.minor()));
	if (fill)
		d2d.FillEllipse(ellipse, fill);
	d2d.DrawEllipse(ellipse, line);
	d2d.SetTransform(identity);
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const shear_ellipse &e) {
	d2d.SetTransform(transform * e.matrix());
	d2d::ellipse	ellipse(unit_circle);
	if (fill)
		d2d.FillEllipse(ellipse, fill);
	if (line)
		d2d.DrawEllipse(ellipse, line);
	d2d.SetTransform(identity);
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const triangle &tri) {
	DrawPoly(d2d, transform, fill, line, tri.corners());
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const quadrilateral &q) {
	DrawPoly(d2d, transform, fill, line, q.corners_cw());
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const temp_points2 &poly) {
	DrawPoly(d2d, transform, fill, line, poly);
}

//void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const simple_polygon &poly) {
//	DrawPoly(d2d, transform, fill, line, poly.begin(), poly.size());
//}

template<typename T, typename C> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *brush, const bezier_chain<T, 2, C> &chain) {
	d2d::Geometry	geom(d2d);
	{
		auto		sink	= geom.Open();
		position2	a		= transform * position2(chain[0][0]);

		sink->BeginFigure(d2d::point(a), D2D1_FIGURE_BEGIN_HOLLOW);

		for (auto &i : chain) {
			position2	a = transform * position2(i[1]);
			position2	b = transform * position2(i[2]);
			sink->AddQuadraticBezier(d2d::bezier2_segment(a, b));
		}
		sink->EndFigure(D2D1_FIGURE_END_OPEN);
	}
	d2d.Draw(geom, brush);
}

template<typename T, typename C> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *brush, const bezier_chain<T, 3, C> &chain) {
	d2d::Geometry	geom(d2d);
	{
		auto		sink	= geom.Open();
		position2	a		= transform * position2(chain[0][0]);

		sink->BeginFigure(d2d::point(a), D2D1_FIGURE_BEGIN_HOLLOW);

		for (auto &i : chain) {
			position2	a = transform * position2(i[1]);
			position2	b = transform * position2(i[2]);
			position2	c = transform * position2(i[3]);
			sink->AddBezier(d2d::bezier_segment(a, b, c));
		}
		sink->EndFigure(D2D1_FIGURE_END_OPEN);
	}
	d2d.Draw(geom, brush);
}

template<int N> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const bezier_splineT<float2, N> &bez) {
	Draw(d2d, transform, fill, line, make_bezier_chain(bez));
}

template<typename T, int N> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *brush, const bspline<T,N> &spline) {
#if 1
	/*
	auto	bp0 = bspline_poly<3>::f(spline.k + 0);
	auto	bp1 = bspline_poly<3>::f(spline.k + 1);
	auto	bp2 = bspline_poly<3>::f(spline.k + 2);
	auto	bp3 = bspline_poly<3>::f(spline.k + 3);

//	auto	bp20 = bspline_poly2<3,3>::f(spline.k + 0);
*/

	if (fill) {
		dynamic_array<position2>	pts(101);
		auto	dom = spline.domain();

		for (int i = 0; i <= 100; i++)
			pts[i] = position2(spline.evaluate2(dom.from(i / 100.f)).result());
		DrawLines(d2d, transform, fill, pts, false);
	}
#endif
	Draw(d2d, transform, fill, brush, to_bez(spline));
}

template<typename T, int N> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *brush, const rational<bspline<T,N>> &spline) {
	DrawLines(d2d, transform, brush, transformc(int_range(101), [&, dom = spline.domain()](int i) {
		return position2(spline.evaluate(dom.from(i / 100.f)));
	}), false);
}

float hyperbolic_param(float2 v) {
	return v.x < 0
		? sqrt((v.x - v.y) / (v.x + v.y))
		: sqrt((v.x + v.y) / (v.x - v.y));
}

float parabolic_param(float2 v) {
	return v.x;
}

void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, const conic &c) {
	float2x3	m;
	auto		info	= c.analyse(m);

	if (info.type == conic::TRIVIAL || info.empty)
		return;

	if (info.type == conic::ELLIPSE) {
		Draw(d2d, transform, nullptr, brush, ellipse(m));
		return;
	}

	auto	size	= d2d.Size();
	line2	edges[]	= {
		{-y_axis, -64},		//top
		{y_axis, size.y - 32},	//bottom
		{-x_axis, -32},		//left
		{x_axis, size.x - 32},	//right
	};
	for (auto &i : edges)
		i = i / transform;

	interval<float>	ints[] = {
		c & edges[0],
		c & edges[1],
		c & edges[2],
		c & edges[3],
	};

	if (info.type == conic::HYPERBOLA) {
		auto	pa	= from_x_axis(edges[0]) * position2(ints[0].a, zero);
		auto	pb	= from_x_axis(edges[0]) * position2(ints[0].b, zero);

		auto	pc	= from_x_axis(edges[1]) * position2(ints[1].b, zero);
		auto	pd	= from_x_axis(edges[1]) * position2(ints[1].a, zero);

		if (ints[0].a == ints[0].b)
			pb	= from_x_axis(edges[2]) * position2(ints[2].a, zero);
		if (ints[1].a == ints[1].b)
			pc	= from_x_axis(edges[3]) * position2(ints[3].a, zero);

		if (edges[2].test(pa))				//pa off left
			pa	= from_x_axis(edges[2]) * position2(ints[2].a, zero);
		else if (edges[3].test(pa))			//pa off right
			pa	= from_x_axis(edges[3]) * position2(ints[3].b, zero);

		if (edges[2].test(pb))				//pb off left
			pb	= from_x_axis(edges[2]) * position2(ints[2].a, zero);
		else if (edges[3].test(pb))			//pa off right
			pb	= from_x_axis(edges[3]) * position2(ints[3].b, zero);

		if (edges[2].test(pc))				//pc off left
			pc	= from_x_axis(edges[2]) * position2(ints[2].b, zero);
		else if (edges[3].test(pc))			//pc off right
			pc	= from_x_axis(edges[3]) * position2(ints[3].a, zero);

		if (edges[2].test(pd))				//pd off left
			pd	= from_x_axis(edges[2]) * position2(ints[2].b, zero);
		else if (edges[3].test(pd))			//pa off right
			pd	= from_x_axis(edges[3]) * position2(ints[3].a, zero);

		if (info.degenerate) {
			DrawLine(d2d, transform, brush, pa, pd);
			DrawLine(d2d, transform, brush, pb, pc);

		} else {
			float	ta	= hyperbolic_param(pa / m);
			float	tb	= hyperbolic_param(pb / m);
			float	tc	= hyperbolic_param(pc / m);
			float	td	= hyperbolic_param(pd / m);

			DrawLines(d2d, transform, brush, transformc(int_range(101), [&](int i) {
				float	t = lerp(tb, td, i / 100.f);
				return  m * position2((square(t) + 1) / (t * 2), (square(t) - 1) / (t * 2));
			}), false);
			DrawLines(d2d, transform, brush, transformc(int_range(101), [&](int i) {
				float	t = lerp(ta, tc, i / 100.f);
				return  m * position2(-(square(t) + 1) / (t * 2), (square(t) - 1) / (t * 2));
			}), false);
		}

	} else {	//conic::PARABOLA:
		auto	pa	= from_x_axis(edges[0]) * position2(ints[0].a, zero);
		auto	pb	= from_x_axis(edges[0]) * position2(ints[0].b, zero);

		if (ints[0].a == ints[0].b)
			pb	= from_x_axis(edges[1]) * position2(ints[1].a, zero);

		if (edges[2].test(pa))				//pa off left
			pa	= from_x_axis(edges[2]) * position2(ints[2].a, zero);
		else if (edges[3].test(pa))			//pa off right
			pa	= from_x_axis(edges[3]) * position2(ints[3].b, zero);

		if (edges[2].test(pb))				//pb off left
			pb	= from_x_axis(edges[2]) * position2(ints[2].a, zero);
		else if (edges[3].test(pb))			//pa off right
			pb	= from_x_axis(edges[3]) * position2(ints[3].b, zero);

		if (info.degenerate) {
			DrawLine(d2d, transform, brush, pa, pb);

		} else {
			float	ta	= parabolic_param(pa / m);
			float	tb	= parabolic_param(pb / m);

			DrawLines(d2d, transform, brush, transformc(int_range(101), [&](int i) {
				float	t = lerp(tb, ta, i / 100.f);//(i - 50) / 50.f;
				return m * position2(t, -square(t) / two);
			}), false);
		}
	}
		
}

template<typename T> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *brush, const rational<bspline<T,2>> &spline) {
#if 0
	//get conics
	auto	b = to_bez(spline);
	for (auto& i : b) {
		conic	c1	= ConicBezier(i[0], i[1], i[2]);
		Draw(d2d, transform, brush, c1);
		break;
	}
#endif
	auto	dom = spline.domain();
	DrawLines(d2d, transform, brush, transformc(int_range(101), [&](int i) {
		return position2(spline.evaluate(dom.from(i / 100.f)));
	}), false);
}

template<typename S> void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line, const complex_polygon<S> &poly) {
	for (auto &i : poly)
		DrawPoly(d2d, transform, fill, line, i.points());
}

struct CurveMaker : CurveTranslator, d2d::Geometry::Sink {
	void Arc(const ArcParams &arc, float2 p0, float2 p1) {
		get()->AddArc(d2d::arc_segment(p1, arc.radii, to_degrees(arc.angle), arc.clockwise, arc.big));
	}
	void Begin(float2 p0)									{ get()->BeginFigure(d2d::point(p0), D2D1_FIGURE_BEGIN_HOLLOW); }
	void End()												{ get()->EndFigure(D2D1_FIGURE_END_CLOSED); }
	void Line(float2 p0, float2 p1)							{ get()->AddLine(d2d::point(p1)); }
	void Bezier(float2 p0, float2 p1, float2 p2)			{ get()->AddQuadraticBezier(d2d::bezier2_segment(p1, p2)); }
	void Bezier(float2 p0, float2 p1, float2 p2, float2 p3)	{ get()->AddBezier(d2d::bezier_segment(p1, p2, p3)); }
	void Arc(float2 p0, float2 p1, float2 p2)				{ Arc(ArcParams(p0, p1, p2), p0, p2); }
	void Arc(float2 p0, float2 p1, float2 p2, float2 p3)	{ Arc(ArcParams(p0, p1, p2, p3), p0, p3); }

	CurveMaker(const d2d::Geometry &geom) : d2d::Geometry::Sink(geom.Open()) {}
//	~CurveMaker() { close(*this); }
};

void DrawCurve(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *brush, range<curve_vertex*> curve) {
	d2d::Geometry	geom(d2d);
	CurveMaker	cm(geom);
	for (auto &i : curve)
		cm.add(cm, i, i.flags);
	cm.close(cm);

	d2d.Draw(d2d::Geometry(d2d.factory, geom, transform), brush);
}

//-----------------------------------------------------------------------------
//	Shape2
//-----------------------------------------------------------------------------

auto	get_corners_cw(...)		{ return none; }
template<typename T, typename=decltype(declval<T>().corner(CORNER_MM))>	auto	get_corners_cw(const T &t)		{ return transformc(corners2_cw(), [t](CORNER i){ return t.corner(i); }); }


template<typename T> ref_polygon get_polygon(const T &t) { return ref_polygon(none); }
ref_polygon get_polygon(const triangle &t)		{ return ref_polygon(t.corners()); }
ref_polygon get_polygon(const quadrilateral &t) { return ref_polygon(get_corners_cw(t)); }
ref_polygon get_polygon(const obb2 &t)			{ return ref_polygon(get_corners_cw(t)); }
ref_polygon get_polygon(const parallelogram &t)	{ return ref_polygon(get_corners_cw(t)); }
ref_polygon	get_polygon(const temp_points2 &t)	{ return ref_polygon(t); }

template<typename S> auto uniform_interior(const complex_polygon<S>& poly, float2 x) {
	return poly.contours()[0].uniform_interior(x);
}
template<typename S> auto uniform_perimeter(const complex_polygon<S>& poly, float x) {
	return poly.contours()[0].uniform_perimeter(x);
}

float area(const ellipse &t)	{
	conic	c(t);
	return pi * c.det() / c.det2x2() * rsqrt(c.det2x2());
	return conic(t).area();
}

struct Shape2 {
	enum MODE {
		normal,
		occluder,
		includer,
	} mode;

	Shape2() : mode(normal) {}
	virtual	~Shape2()	{}
	virtual	void		Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line) const = 0;
	virtual rectangle	get_box()							const = 0;
	virtual ref_polygon	get_polygon()						const = 0;
	virtual bool		contains(param(position2) p)		const = 0;
	virtual position2	any_point()							const = 0;
	virtual position2	closest(param(position2) p)			const = 0;
	virtual float		dist(param(position2) p)			const = 0;
	virtual position2	support(param(float2) v)			const = 0;
	virtual async_callback<position2(param(float2))> support()const = 0;
	virtual float		area()								const = 0;
	virtual bool		ray_check(param(ray2) r, float &t, float2 *n) const = 0;
	virtual float		ray_closest(param(ray2) r)			const = 0;
	virtual void		Move(const float2x3 &m)	= 0;

	virtual async_callback<position2(float)>			perimeter_optimised()	const = 0;
	virtual async_callback<position2(param(float2))>	interior_optimised()	const = 0;

	friend	auto		perimeter_optimised(const Shape2 &s)				{ return s.perimeter_optimised();	}
	friend	auto		interior_optimised(const Shape2 &s)					{ return s.interior_optimised();	}
};

//template<> struct deleter<Shape2> { void operator(void*); };

template<typename T> struct ShapeT : Shape2 {
	T	x;
	ShapeT(const T &_t) : x(_t) {}
	template<typename T2> ShapeT(T2 &&t)	: x(forward<T2>(t)) {}
	void Draw(const d2d::Target &d2d, param(float2x3) transform, ID2D1Brush *fill, ID2D1Brush *line) const {
		::Draw(d2d, transform, fill, line, x);
	}
	rectangle	get_box()							const	{ return x.get_box();	}
	ref_polygon	get_polygon()						const	{ return ::get_polygon(x); }
	bool		contains(param(position2) p)		const	{ return x.contains(p);	}
	position2	any_point()							const	{ using iso::any_point; return any_point(x);	}
	position2	closest(param(position2) p)			const	{ return x.closest(p);	}
	float		dist(param(position2) p)			const	{ using iso::dist; return dist(x, p); }
	position2	support(param(float2) v)			const	{ return position2(x.support(v));	}
	async_callback<position2(param(float2))> support()const	{ return [&x=x](param(float2) v) { return position2(x.support(v)); };	}
	float		area()								const	{ using iso::area; return area(x);	}
	bool		ray_check(param(ray2) r, float &t, float2 *n) const { return x.ray_check(r, t, n);	}
	float		ray_closest(param(ray2) r)			const	{ return x.ray_closest(r);	}
	void		Move(const float2x3 &m)						{ x *= (translate(x.centre()) * m * translate(-x.centre()));	}

	async_callback<position2(float)>			perimeter_optimised()	const	{ using iso::perimeter_optimised; return perimeter_optimised(x); }
	async_callback<position2(param(float2))>	interior_optimised()	const	{ using iso::interior_optimised; return interior_optimised(x); }
};

template<typename T> Shape2* make_shape(T &&t) { return new ShapeT<typename T_noref<T>::type>(forward<T>(t)); }

//-----------------------------------------------------------------------------
//	occlusion
//-----------------------------------------------------------------------------
#if 1
static int process_occlusion_edge(const ref_polygon &inc, const interval<position2> &occ, quedge2 *wedges) {
	ray2	r(occ.a, occ.b);
	float	i0, i1;
	if (!inc.find_split(line2(r.d, r.p), i1, i0))
		return 0;		// whole occluder is outside includer

	if (i1 == 0)
		return 0;		// this edge of occluder outside includer

	position2	p0	= inc.perimeter(i0);
	position2	p1	= inc.perimeter(i1);
	float		t0	= r.project_param(p0);
	float		t1	= r.project_param(p1);
	ISO_ASSERT(t0 < t1);

	if (t0 >= 1 || t1 <= 0)
		return 0;		// this edge of occluder does not need checking

	if (t0 <= 0 && t1 >= 1) {
		//unclipped occluder edge
		*wedges = -r;
		return 1;
	}

	if (t0 > 0) {
		if (t1 < 1)
			*wedges = quedge2(ray2(p0, inc.edge(int(i0)).extent()), ray2(p1, inc.edge(int(i1)).extent()));	// double wedge
		else
			*wedges = quedge2(p0, -r.d, inc.edge(int(i0)).extent());	// single wedge at p0
	} else {
		*wedges = quedge2(p1, inc.edge(int(i1)).extent(), -r.d);	// single wedge at p1
	}

	return 1;
}

struct Includer {
	ref_polygon				inc;
	dynamic_array<quedge2>	wedges;
	Includer(ref_polygon &&inc) : inc(move(inc)) {}

	void	add_occluder(const ref_polygon &occ) {
		for (auto e : occ.edges()) {
			quedge2	w;
			if (process_occlusion_edge(inc, e, &w))
				wedges.push_back(w);
		}
	}
	template<typename S>	bool test(const S &s) const {
		if (inc.overlaps_shape(s)) {
			if (wedges.empty())
				return true;

			for (auto &w : wedges) {
				if (w.contains_shape(s))
					return true;
			}
		}
		return false;
	}
};

static bool test_occlusion(const Shape2 *s, const dynamic_array<Includer> &includers) {
	for (auto &i : includers) {
		if (i.test(*s))
			return true;
	}
	return false;
}

#endif

Sobol<2>	_sobol;
inline auto sobol(int k, int n)	{ float2 f; _sobol.generate((float*)&f); return f; }

//-----------------------------------------------------------------------------
//	ViewTest2D
//-----------------------------------------------------------------------------

class ViewTest2D : public aligned<Window<ViewTest2D>, 16> {
	enum MODE {
		MODE_POINTS,
		MODE_LINES,
		MODE_STAR,
		MODE_BEZIER,
		MODE_HULL,
		MODE_DELAUNAY,
		MODE_CIRCLE,
		MODE_INCIRCLE,
		MODE_ELLIPSE,
		MODE_TRIANGLE,
		MODE_BOX,
		MODE_QUAD,
		MODE_NGON,
		MODE_CUBIC_INTERP,
		MODE_CUBIC_RELAXED,
		MODE_CUBIC_CATMULL,
		MODE_CONTOUR,
		MODE_MARTINEZ,
		MODE_STEPPING,
		MODE_POINT_CLOSEST,
		MODE_POINT_DIST,
		MODE_SUPPORT,
		MODE_RAY_CHECK,
		MODE_RAY_CLOSEST,
		MODE_COLLISION,
		MODE_OCCLUSION,
		MODE_BOOLOPS,
			MODE_INTERSECTION = MODE_BOOLOPS,
			MODE_UNION,
			MODE_DIFFERENCE,
			MODE_XOR,

	};
	enum SELECTED_MODE {
		SEL_NONE,
		SEL_POINT,
		SEL_CURVE,
		SEL_SHAPE
	};
	enum NUMBERS {
		NUM_OFF,
		NUM_BASE,
		NUM_HULL,
	};
	enum COMMAND {
		CYCLE				= 0xff,
		SET_MODE			= 0x100,
		SET_NUMBERS			= 0x200,
		SET_WINDING			= 0x300,
		MISC				= 0x400,
		TRIANGULATOR_START	= MISC,
		TRIANGULATOR_NEXT,
		FLIP_Y,
		REVERSE,
		RANDOMISE_INTERIOR,
		RANDOMISE_PERIMETER,
		REGULAR_INTERIOR,
		REGULAR_INTERIOR_SOBOL,
		REGULAR_PERIMETER,
		ADD,
		REMOVE,
		SET_BACKGROUND,
		EXPAND,
		STEP_UP,
		STEP_DOWN,
		NEXT_NUM,
		BREAK,
	};
	friend NUMBERS operator++(NUMBERS &n) { return n = NUMBERS((n + 1) % 3); }

	struct ShapeType { const ISO::Type *type; Shape2 *(*make)(const ISO_ptr<void> &p); };
	static ShapeType	shape_types[];

	MainWindow				&main;
	Accelerator				accel;
	ToolBarControl			toolbar;
	ToolTipControl			tooltip;
	d2d::WND				d2d;
	d2d::Write				write;
	d2d::Font				font;

	ISO_ptr<ISO_openarray<float2p>>			p;
	ISO_ptr<ISO_openarray<curve_vertex>>	curve;
	ISO_ptr<bitmap>			background;
	com_ptr<ID2D1Bitmap>	d2d_background;

	dynamic_array<unique_ptr<Shape2>>	shapes;

	MODE					mode;
	NUMBERS					numbers;
	WINDING_RULE			winding_rule;
	int						selected_index;
	SELECTED_MODE			selected_mode;
	float2					zoom;
	position2				pos;
	position2				ray_start;
	int						prevn;
	int						step;
	Point					mouse;
	uint32					mouse_buttons;
	unique_ptr<Triangulator>	tri;
	ModifyOp				*currentop;

	int			FindPoint(const Point &mouse);
	Shape2*		AddShape(const ISO_ptr<void> &p);
	void		Paint(const Rect &client);
	rectangle	Init(const ISO_ptr<void> &_p);
	bool		Set(ISO::Browser b);
	bool		Select(ISO::Browser b);

	template<typename L> void	GeneratePerimeter(L&& lambda) {
		if (p && selected_mode == SEL_SHAPE)
			*make_rangec(*p) = generate_perimeter(*shapes[selected_index], lambda, p->Count());
	}

	template<typename L> void	GenerateInterior(L&& lambda) {
		if (p) {
			int	n = p->Count();
			if (selected_mode == SEL_SHAPE)
				*make_rangec(*p) = generate_interior(*shapes[selected_index], lambda, n);
			else
				generate_interior(unit_rect, lambda, (position2*)p->begin(), n);
		}
	}

	void	SetSelected(SELECTED_MODE mode, int i) {
		if (selected_mode != mode || selected_index != i) {
			selected_mode	= mode;
			selected_index	= i;
			Invalidate();
		}
	}

	void	DrawText(param(float2x3) transform, ID2D1Brush *brush, param(position2) p, const char *text) {
		position2	x = transform * p;
		d2d.DrawText(rectangle(x, x + 100), str16(text), font, brush);
	}

	void	FitExtent(param(rectangle) r) {
		Rect	client	= GetClientRect();
		int		w		= client.Width(), h = client.Height();
		zoom	= min(w / r.extent().x, h / r.extent().y) * 0.75f;
		pos		= position2(w, h) / 2 - r.centre().v * zoom;
	}

public:
	static bool supported_type(const ISO::Type *type);

	static bool supported_type(const ISO::Browser &b) {
		if (const ISO::Type *type = b.GetTypeDef()) {
			if (supported_type(type))
				return true;

			if (type->GetType() == ISO::VIRTUAL && b.Count() > 1) {
				ISO::Browser2	e = b[0];
				if (e.GetTypeDef()->IsPlainData() && e.Count() == 2)
					return true;
			}

			if (type && (type->GetType() == ISO::ARRAY || type->GetType() == ISO::OPENARRAY)) {
				bool	maybe = false;
				for (auto i : b) {
					if (!supported_type(i.GetTypeDef()))
						return false;
					maybe = true;
				}
				return maybe;
			}
		}
		return false;
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	//ViewTest2D(MainWindow &_main, const WindowPos &wpos, const ISO_ptr<void> &_p);
	ViewTest2D(MainWindow &_main, const WindowPos &wpos, const ISO_VirtualTarget &v);
};

ViewTest2D::ShapeType	ViewTest2D::shape_types[] = {
	{ ISO::getdef<circle		>(), [](const ISO_ptr<void> &p) { return make_shape(*(circle		*)p); } },
	{ ISO::getdef<ellipse		>(), [](const ISO_ptr<void> &p) { return make_shape(*(ellipse		*)p); } },
	{ ISO::getdef<rectangle		>(), [](const ISO_ptr<void> &p) { return make_shape(obb2(*(rectangle*)p)); } },
	{ ISO::getdef<triangle		>(), [](const ISO_ptr<void> &p) { return make_shape(*(triangle		*)p); } },
	{ ISO::getdef<parallelogram	>(), [](const ISO_ptr<void> &p) { return make_shape(*(parallelogram	*)p); } },
	{ ISO::getdef<quadrilateral	>(), [](const ISO_ptr<void> &p) { return make_shape(*(quadrilateral	*)p); } },
	{ ISO::getdef<ISO::bezier2d	>(), [](const ISO_ptr<void> &p) { return make_shape(*(bezier2d		*)p); } },
	{ ISO::getdef<ISO::bezier2d2>(), [](const ISO_ptr<void> &p) { return make_shape(*(bezier2d2		*)p); } },
	{ ISO::getdef<ISO::bezier_chain2d>(),	[](const ISO_ptr<void> &p) { return make_shape(*(bezier_chain2d		*)p); } },
	{ ISO::getdef<ISO::bezier_chain2d2>(),	[](const ISO_ptr<void> &p) { return make_shape(*(bezier_chain2d2	*)p); } },
	{ ISO::getdef<ISO::bspline2d>(),		[](const ISO_ptr<void> &p) { return make_shape<bspline2d>(*(ISO::bspline2d*)p); } },
	{ ISO::getdef<ISO::bspline2d2>(),		[](const ISO_ptr<void> &p) { return make_shape<bspline2d2>(*(ISO::bspline2d2*)p); } },
	{ ISO::getdef<ISO::nurbs2d>(),			[](const ISO_ptr<void> &p) { return make_shape<nurbs2d>(*(ISO::nurbs2d*)p); } },
	{ ISO::getdef<ISO::nurbs2d2>(),			[](const ISO_ptr<void> &p) { return make_shape<nurbs2d2>(*(ISO::nurbs2d2*)p); } },
};

bool ViewTest2D::supported_type(const ISO::Type *type) {
	if (!type)
		return false;

	for (auto &i : shape_types) {
		if (type->SameAs(i.type))
			return true;
	}

	if (type->GetType() != ISO::ARRAY && type->GetType() != ISO::OPENARRAY)
		return false;

	if (type->SubType()->Is<curve_vertex>())
		return true;

	const ISO::Type *sub = type->SubType()->SkipUser();

	if (!sub->IsPlainData())
		return false;

	return (sub->GetType() == ISO::ARRAY && ((ISO::TypeArray*)sub)->Count() == 2)
		|| (sub->GetType() == ISO::COMPOSITE && ((ISO::TypeComposite*)sub)->Count() == 2);
}

LRESULT ViewTest2D::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			Accelerator::Builder	ab;
			Menu	menu	= Menu::Create();
			Menu	sub		= Menu::Create();
			menu.Append("Mode", sub);
			ab.Append(sub, "Points",		SET_MODE | MODE_POINTS,			'P');
			ab.Append(sub, "Lines",			SET_MODE | MODE_LINES,			'L');
			ab.Append(sub, "Star",			SET_MODE | MODE_STAR,			{'*', Accelerator::Key::ASCII});
			ab.Append(sub, "Bezier",		SET_MODE | MODE_BEZIER,			'Z');
			ab.Append(sub, "Hull",			SET_MODE | MODE_HULL,			'H');
			ab.Append(sub, "Delaunay",		SET_MODE | MODE_DELAUNAY,		'D');
			ab.Append(sub, "Circle",		SET_MODE | MODE_CIRCLE,			'C');
			ab.Append(sub, "InCircle",		SET_MODE | MODE_INCIRCLE,		'I');
			ab.Append(sub, "Ellipse",		SET_MODE | MODE_ELLIPSE,		'E');
			ab.Append(sub, "Triangle",		SET_MODE | MODE_TRIANGLE,		'T');
			ab.Append(sub, "Box",			SET_MODE | MODE_BOX,			'B');
			ab.Append(sub, "Quad",			SET_MODE | MODE_QUAD,			'Q');
			ab.Append(sub, "N-gon",			SET_MODE | MODE_NGON);
			ab.Append(sub, "Interpolating Cubic",	SET_MODE | MODE_CUBIC_INTERP);
			ab.Append(sub, "Relaxed Cubic",	SET_MODE | MODE_CUBIC_RELAXED);
			ab.Append(sub, "Catmull-Rom Cubic",	SET_MODE | MODE_CUBIC_CATMULL);
			ab.Append(sub, "Contour",		SET_MODE | MODE_CONTOUR);
			ab.Append(sub, "Martinez",		SET_MODE | MODE_MARTINEZ);
			ab.Append(sub, "Stepping",		SET_MODE | MODE_STEPPING);
			ab.Append(sub, "Point Closest",	SET_MODE | MODE_POINT_CLOSEST);
			ab.Append(sub, "Point Dist",	SET_MODE | MODE_POINT_DIST);
			ab.Append(sub, "Show Support",	SET_MODE | MODE_SUPPORT);
			ab.Append(sub, "Ray Check",		SET_MODE | MODE_RAY_CHECK);
			ab.Append(sub, "Ray Closest",	SET_MODE | MODE_RAY_CLOSEST);
			ab.Append(sub, "Show Collision",SET_MODE | MODE_COLLISION);
			ab.Append(sub, "Show Occlusion",SET_MODE | MODE_OCCLUSION);
			ab.Append(sub, "Intersection",	SET_MODE | MODE_INTERSECTION,	{'I', Accelerator::Key::CTRL});
			ab.Append(sub, "Union",			SET_MODE | MODE_UNION,			{'U', Accelerator::Key::CTRL});
			ab.Append(sub, "Difference",	SET_MODE | MODE_DIFFERENCE,		{'D', Accelerator::Key::CTRL});
			ab.Append(sub, "Xor",			SET_MODE | MODE_XOR,			{'X', Accelerator::Key::CTRL});

			sub		= Menu::Create();
			menu.Append("Numbers", sub);
			sub.Append("Off",				SET_NUMBERS | NUM_OFF);
			sub.Append("Base",				SET_NUMBERS | NUM_BASE);
			sub.Append("Hull",				SET_NUMBERS | NUM_HULL);

			menu.Append("TRIANGULATOR START",	TRIANGULATOR_START);
			menu.Append("TRIANGULATOR NEXT",	TRIANGULATOR_NEXT);

			sub		= Menu::Create();
			menu.Append("Options", sub);
			ab.Append(sub, "Flip Vertical",			FLIP_Y,					'Y');
			ab.Append(sub, "Reverse Order",			REVERSE,				VK_SUBTRACT);
			ab.Append(sub, "Randomise Interior",	RANDOMISE_INTERIOR,		'R');
			ab.Append(sub, "Randomise Perimeter",	RANDOMISE_PERIMETER,	{'R', Accelerator::Key::SHIFT});
			ab.Append(sub, "Regular Interior",		REGULAR_INTERIOR,		{'R', Accelerator::Key::CTRL});
			ab.Append(sub, "Regular Interior (sobol)",	REGULAR_INTERIOR_SOBOL);
			ab.Append(sub, "Regular Perimeter",		REGULAR_PERIMETER,		{'R', Accelerator::Key::SHIFT | Accelerator::Key::CTRL});
			ab.Append(sub, "Add shape",				ADD,					VK_RETURN);
			ab.Append(sub, "Remove selected",		REMOVE,					VK_BACK);
			sub.Append("Set background",			SET_BACKGROUND);
			sub.Append("Expand",					EXPAND);

			sub		= Menu::Create();
			menu.Append("Winding", sub);
			sub.Append("Non-zero",				SET_WINDING | WINDING_NONZERO);
			sub.Append("Odd",					SET_WINDING | WINDING_ODD);
			sub.Append("Positive",				SET_WINDING | WINDING_POSITIVE);
			sub.Append("Negative",				SET_WINDING | WINDING_NEGATIVE);
			sub.Append("Absolute >= 2",			SET_WINDING | WINDING_ABS_GEQ_TWO);

			ab.Append(SET_NUMBERS | CYCLE,	VK_DECIMAL);
			ab.Append(STEP_UP,				VK_ADD);
			ab.Append(STEP_DOWN,			VK_SUBTRACT);
			ab.Append(BREAK,				VK_F11);

			accel	= Accelerator(ab);

			toolbar.Create(*this, 0, CHILD | VISIBLE);
			toolbar.Init(menu);

			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);

			d2d.Init(hWnd, GetClientRect().Size());
			break;
		}

		case WM_SETFOCUS:
			SetAccelerator(*this, accel);
			break;

		case WM_MOUSEACTIVATE:
			SetFocus();
			return MA_NOACTIVATE;

		case WM_SIZE: {
			Rect	rect(Point(0, 0), Point(lParam)), rects[2];
			ControlArrangement::GetRects(ToolbarArrange, rect, rects);
			toolbar.Move(rects[0]);

			if (!d2d.Resize(rect.Size())) {
				d2d_background.clear();
				d2d.DeInit();
			}
			Invalidate();
			break;
		}

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			Rect	client	= GetClientRect();
			d2d.Init(hWnd, client.Size());
			if (!d2d.Occluded()) {
				d2d.BeginDraw();
				Paint(client);
				if (d2d.EndDraw()) {
					d2d_background.clear();
					d2d.DeInit();
				}
			}
			break;
		}

		case WM_CONTEXTMENU: {
			auto		m = ToClient(Point(lParam));
			position2	p = position2((float2{m.x, m.y} - pos.v) / zoom);
			for (auto &i : reversed(shapes)) {
				if (i->contains(p)) {
					Menu	menu	= Menu::Popup();
					menu.Append("Normal", 1);
					menu.Append("Occluder", 2);
					menu.Append("Includer", 3);
					menu.Separator();
					menu.Append("Delete", 4);
					if (int	x = menu.Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_RETURNCMD)) {
						if (x == 4) {
							auto	x = shapes.index_of(i);
							if (selected_mode == SEL_SHAPE && selected_index == x)
								selected_mode = SEL_NONE;
							shapes.erase(&i);
						} else {
							i->mode = (Shape2::MODE)(x - 1);
						}
						Invalidate();
					}
					break;
				}
			}
			break;
		}

		case WM_LBUTTONDOWN: {
			mouse			= Point(lParam);
			if (selected_mode != SEL_SHAPE && (mode == MODE_RAY_CLOSEST || mode == MODE_RAY_CHECK)) {
				ray_start = position2((float2{mouse.x, mouse.y} - pos.v) / zoom);
				Invalidate();
			}

			SetFocus();
			break;
		}

		case WM_RBUTTONDOWN: {
			mouse		= Point(lParam);
			int		i	= FindPoint(mouse);
			if (i >= 0) {
				if (tri) {
//					tri->DeletePoint(selected2);
					tri->MakeEdge(selected_index, i);
					Invalidate();
				}
			}
			break;
		}

		case WM_LBUTTONUP:
			currentop	= NULL;
			break;

		case WM_LBUTTONDBLCLK:
			if (wParam & MK_CONTROL) {
				zoom	= 1;
				pos		= position2(zero);
				Invalidate();
			}
			break;

		case WM_MOUSEMOVE: {
			Point		pt(lParam);
			TrackMouse(TME_LEAVE);
			tooltip.Activate(*this, true);
			tooltip.Track();

			if (wParam & MK_LBUTTON) {
				Point		pt(lParam);
				float2		move{float(pt.x - mouse.x), float(pt.y - mouse.y)};
				float2x3	m;
				if (wParam & MK_CONTROL)
					m = (float2x3)rotate2D(move.x / 100.f);
				else
					m = translate(move / zoom);

				switch (selected_mode) {
					case SEL_POINT: {
						(*p)[selected_index] = (*p)[selected_index] + move / zoom;
						tri = 0;
						if (curve)
							(*curve)[selected_index].set_pos((*p)[selected_index]);
#ifdef ISO_EDITOR
						if (!currentop)
							((IsoEditor&)main).Do(currentop = new ModifyOp(p));
						ISO_ptr<void>	p2 = p;
						((IsoEditor&)main).Update(p2);
#endif
						break;
					}
					case SEL_SHAPE:
						shapes[selected_index]->Move(m);
						break;

					default:
						if (wParam & MK_CONTROL) {
							float2	mult = pow(float2(1.05f), move * float2{1, -1});
							zoom *= mult;

						} else {
							pos += move;
						}
						break;
				}
				Invalidate();

			} else if (wParam & MK_RBUTTON) {
				Invalidate();

			} else {
				int	i = FindPoint(mouse);
				if (i >= 0) {
					SetSelected(SEL_POINT, i);
				} else {
					position2	p	= position2((float2{pt.x, pt.y} - pos.v) / zoom);
					bool		found	= false;
					for (auto &i : reversed(shapes)) {
						if (i->contains(p)) {
							found	= true;
							SetSelected(SEL_SHAPE, shapes.index_of(i));
							break;
						}
					}
					if (!found)
						SetSelected(SEL_NONE, 0);
				}
			}

			if (is_any(mode, MODE_POINT_CLOSEST, MODE_POINT_DIST, MODE_SUPPORT, MODE_RAY_CLOSEST, MODE_RAY_CHECK)) {
				Invalidate();
				Update();
			}
			mouse = pt;
			mouse_buttons = wParam;
			Invalidate();
			break;
		}

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, false);
			return 0;

		case WM_MOUSEWHEEL: {
			Point		pt0		= ToClient(Point(lParam));
			position2	pt(pt0.x, pt0.y);
			float	mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
			zoom	*= mult;
			pos		= pt + (pos - pt) * mult;
			Invalidate();
			break;
		}

		case WM_COMMAND:
			switch (wParam & 0xff00) {
				case SET_MODE:
					mode	= MODE(wParam & 0xff);
					tri		= 0;
					step	= 0;
					Invalidate();
					return 0;

				case SET_NUMBERS:
					numbers = wParam & 0xff == CYCLE ? NUMBERS((numbers + 1) % 3) : NUMBERS(wParam & 0xff);
					Invalidate();
					return 0;

				case SET_WINDING:
					winding_rule = WINDING_RULE(wParam & 0xff);
					Invalidate();
					return 0;

				default: switch (LOWORD(wParam)) {
					case ID_EDIT:	// from main
						if (Set(*(ISO::Browser*)lParam)) {
							Invalidate();
							return true;
						}
						break;

					case ID_EDIT_SELECT:	// from main
						Select(*(ISO::Browser*)lParam);
						break;

//					case TRIANGULATOR_START:
//					case TRIANGULATOR_NEXT:
					case FLIP_Y:
						zoom.y	= -zoom.y;
						Invalidate();
						break;

					case REVERSE:
						if (curve) {
							reverse_curves(*curve);
							float2p	*d = p->begin();;
							for (auto &i : *curve)
								*d++ = float2p{i.x, i.y};

						} else if (p) {
							reverse(*p);
						}
						Invalidate();
						return 0;

					case RANDOMISE_INTERIOR:
						GenerateInterior([](int k, int n) { return random.get<float2>(); });
						Invalidate();
						return 0;

					case RANDOMISE_PERIMETER:
						GeneratePerimeter([](int k, int n) { return random.get<float>(); });
						Invalidate();
						return 0;

					case REGULAR_INTERIOR:
						GenerateInterior(halton2<2,3>);
						Invalidate();
						return 0;

					case REGULAR_INTERIOR_SOBOL:
						GenerateInterior(sobol);
						Invalidate();
						return 0;

					case REGULAR_PERIMETER:
						GeneratePerimeter(linear);
						Invalidate();
						return 0;

					case ADD: {
						if (p) {
							temp_points2	pts(*p, mode == MODE_CONTOUR ? square(p->size32()) : 1);
							switch (mode) {
								//case MODE_BEZIER:	shapes.push_back(make_shape(bezier2d(pts[0], pts[1], pts[2], pts[3]))); break;
								case MODE_BEZIER:	shapes.push_back(make_shape(bezier_chain2d(*p))); break;
								case MODE_HULL:		shapes.push_back(make_shape(temp_points2(pts.begin(), generate_hull_2d(pts.begin(), pts.end())))); break;
								case MODE_CIRCLE:	shapes.push_back(make_shape(pts.cloud().circumscribe())); break;
								case MODE_INCIRCLE:	shapes.push_back(make_shape(as_simple_polygon(pts.cloud()).inscribe(1e-3f))); break;
								case MODE_ELLIPSE:	shapes.push_back(make_shape(pts.cloud().get_ellipse())); break;
								case MODE_TRIANGLE:	shapes.push_back(make_shape(pts.cloud().get_tri())); break;
								case MODE_BOX:		shapes.push_back(make_shape(pts.cloud().get_obb())); break;
								case MODE_QUAD:		shapes.push_back(make_shape(pts.cloud().get_quad())); break;
								case MODE_NGON:		shapes.push_back(make_shape(temp_points2(pts.begin(), optimise_polyline_verts(pts.begin(), pts.size(), step + 3, true)))); break;
								case MODE_STAR:		shapes.push_back(make_shape(temp_points2(make_star_polygon(pts.cloud()).points()))); break;
								case MODE_CUBIC_INTERP:		shapes.push_back(new ShapeT<bezier_chain2d>(bezier_chain<float2p, 3, ISO::OpenArray<float2p>>::interpolating(*p))); break;
								case MODE_CUBIC_RELAXED:	shapes.push_back(new ShapeT<bezier_chain2d>(bezier_chain<float2p, 3, ISO::OpenArray<float2p>>::relaxed(*p))); break;
								case MODE_CUBIC_CATMULL:	shapes.push_back(new ShapeT<bezier_chain2d>(catmull_rom_to_bezier<float2p, ISO::OpenArray<float2p>>(*p, 0.5f, nullptr, nullptr))); break;

								case MODE_CONTOUR: {
									int		*lengths	= alloc_auto(int, pts.size());
									int		nc			= find_contour(winding_rule, pts.begin(), pts.size(), pts.begin(), lengths);
									complex_polygon<dynamic_array<simple_polygon<temp_points2>>> polygon;
									auto	p			= pts.begin();
									for (int c = 0; c < nc; ++c) {
										int	n	= lengths[c];
										polygon.contours().emplace_back(p, n);
										p	+= n;
									}
									shapes.push_back(make_shape(move(polygon)));
									break;
								}
								case MODE_MARTINEZ: {
									auto	polyr	= poly_bool(as_simple_polygon(pts.cloud()), simple_polygon<array<position2,0>>(), Martinez::XOR);
									complex_polygon<dynamic_array<simple_polygon<temp_points2>>> polygon;
									for (auto &c : polyr.contours())
										polygon.contours().push_back(transformc(c.points(), [](param(double2) i) { return to<float>(i); }));
									shapes.push_back(make_shape(move(polygon)));
									break;
								}
								default:			break;
							}
						}
						Invalidate();
						return 0;
					}
					case REMOVE:
						switch (selected_mode) {
							case SEL_POINT:	p->Remove(*p + selected_index); break;
							case SEL_SHAPE:	shapes.erase_unordered(shapes + selected_index); break;
						}
						selected_mode = SEL_NONE;
						Invalidate();
						return 0;

					case SET_BACKGROUND: {
						ISO::Browser	b = ((IsoEditor*)MainWindow::Get())->GetSelection();
						if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(b, ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::RECURSE)) {
							background = bm;
							Invalidate();
						}
						break;
					}

					case STEP_UP:
						++step;
						Invalidate();
						break;

					case STEP_DOWN:
						if (step) {
							--step;
							Invalidate();
						}
						break;

					case NEXT_NUM:
						++numbers;
						Invalidate();
						break;

					case BREAK:
						Invalidate();
						RedrawWindow(*this, NULL, NULL, 0);
						break;
#if 0
					case EXPAND:
						if (p) {
							temp_points2	pts(*p);
							int		n		= pts.size();
							auto	pts2	= make_auto_block<position2>(n * 2);
							int		n2		= expand_polygon(pts, n, 0.5f, pts2);
							//int	n3		= find_contour(pts2, n2, pts2);

							p->Clear();
							for (int i = 0; i < n2; i++) {
								float2p	t = pts2[i].v;
								p->Append(t);
							}
							Invalidate();
						}
						break;
#endif
				}
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TBN_DROPDOWN: {
					NMTOOLBAR	*nmtb	= (NMTOOLBAR*)nmh;
					ToolBarControl(nmh->hwndFrom).GetItem(nmtb->iItem).Param<Menu>().Track(*this, ToScreen(Rect(nmtb->rcButton).BottomLeft()));
					break;
				}

				case TTN_GETDISPINFOA:
					if (nmh->hwndFrom == tooltip) {
						NMTTDISPINFOA	*nmtdi = (NMTTDISPINFOA*)nmh;
						fixed_accum		sa(nmtdi->szText);
						Point			mouse	= ToClient(GetMousePos());
						float2			pt2		= (float2{mouse.x, mouse.y} - pos.v) / zoom;
						sa << float(pt2.x) << ", " << float(pt2.y);
						if (selected_mode == SEL_SHAPE) {
							sa << "; area = " << shapes[selected_index]->area();

						}
					}
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			break;
		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}

int ViewTest2D::FindPoint(const Point &mouse) {
	if (p) {
		for (int i = 0, n = p->Count(); i < n; i++) {
			float2	v = (*p)[i] * zoom + pos.v;
			if (abs(int(v.x) - mouse.x) <= 4 && abs(int(v.y) - mouse.y) <= 4)
				return i;
		}
	}
	return -1;
}

Shape2 *ViewTest2D::AddShape(const ISO_ptr<void> &p) {
	for (auto &i : shape_types) {
		if (p.IsType(i.type, ISO::MATCH_NOUSERRECURSE))
			return shapes.push_back(i.make(p));
	}
	return nullptr;
}

void ViewTest2D::Paint(const Rect &client) {
	position2	mouse_pos	= position2((float2{mouse.x, mouse.y} - pos.v) / zoom);
	vector2		mouse_dir;
	global_point = mouse_pos;


	if (mode == MODE_SUPPORT) {
		d2d::point	p0	= client.Centre();
		int			len	= max(client.Width(), client.Height()) / 2;

		mouse_dir = normalise(float2{mouse.x, mouse.y} - (float2)p0);

		d2d::gradstop	g[] = {
			{0, colour::black},
			{1, colour::white}
		};
		d2d.Fill(client, d2d::LinearGradientBrush(d2d, (float2)p0 + mouse_dir * len, p0, g));

	} else {
		d2d.Clear(colour(1,1,1));
	}

	d2d::SolidBrush	black(d2d,	colour::black);
	d2d::SolidBrush	red(d2d,	colour::red);
	d2d::SolidBrush	green(d2d,	colour::green);
	d2d::SolidBrush	blue(d2d,	colour::blue);
	d2d::SolidBrush	fill(d2d,	colour(.8f,.8f,1));

	float2x3	transform = translate(pos) * scale(zoom);

	if (background) {
		if (!d2d_background)
			d2d.CreateBitmap(&d2d_background, background->All());
		d2d.Draw(d2d::rect(pos, pos + float2{background->Width(), background->Height()} * zoom), d2d_background, 1, false);
	}

	//grid
	{
		position2	s0	= position2(zero);
		position2	s1	= d2d::point(client.Size());
		position2	p0	= s0 / transform;
		position2	p1	= s1 / transform;

		float		ps	= log2(abs(reduce_min(p1 - p0))) * 0.30103f - one;
		float		ps2	= pow(10.f, floor(ps));
		float2		i2	= floor(p0.v / ps2);
		position2	a	= transform * position2(i2 * ps2);
		float2		d	= abs(zoom) * ps2;

		float		fade = ceil(ps) - ps;

		com_ptr<ID2D1SolidColorBrush>	linebrush[2];
		d2d.CreateBrush(&linebrush[0],	colour(fade * 0.75f));
		d2d.CreateBrush(&linebrush[1],	colour(0.75f));

		int	i = int(i2.x);
		for (float x = a.v.x; x < s1.v.x; x += d.x, i++)
			d2d.DrawLine(float2{x, s0.v.y}, float2{x, s1.v.y}, linebrush[(abs(i) % 10) == 0], 1);
		i = int(i2.y);
		for (float y = a.v.y; y < s1.v.y; y += d.y, i++)
			d2d.DrawLine(float2{s0.v.x, y}, float2{s1.v.x, y}, linebrush[(abs(i) % 10) == 0], 1);
	}

	d2d::SolidBrush	fills[] = {
		{d2d,	colour(.8f,.8f,.8f)},
		{d2d,	colour(1,.8f,.8f)},
		{d2d,	colour(.8f,1,.8f)},
		{d2d,	colour(.8f,.8f,.8f)},
	};

	switch (mode) {
		case MODE_OCCLUSION: {
	#if 1
			dynamic_array<Shape2*>	shapes2 = shapes;
			auto	*occluders_begin	= partition(shapes2, [](Shape2 *p) { return p->mode != Shape2::normal; });
			auto	*includers_begin	= partition(occluders_begin, shapes2.end(), [](Shape2 *p) { return p->mode == Shape2::includer; });

			dynamic_array<Includer>		includers = transformc(make_range(includers_begin, shapes2.end()),		[](Shape2 *s) { return s->get_polygon(); });
			dynamic_array<ref_polygon>	occluders = transformc(make_range(occluders_begin, includers_begin),	[](Shape2 *s) { return s->get_polygon(); });

			for (auto &i : includers) {
				for (auto &o : occluders)
					i.add_occluder(o);
			}

			for (auto &i : shapes) {
				int		mode = i->mode;
				if (mode == Shape2::normal)
					mode = test_occlusion(i, includers) ? 0 : 3;
				i->Draw(d2d, transform, fills[mode], selected_mode == SEL_SHAPE && shapes.index_of(&i) == selected_index ? red : black);
			}
	#endif
			break;
		}
		case MODE_COLLISION: {
			temp_points2	epa_pointsA, epa_pointsB;

			for (auto& i : shapes) {
				position2	closestA, closestB;
				float2		normal;
				float		distance;
				bool		hit	= false;

				for (auto& j : slice(shapes, shapes.index_of(i) + 1)) {
	#if 0
					GJK2	gjk;
					float2	separation;
					if (hit = gjk.intersect(
						i->support(), j->support(),
						j->any_point() - i->any_point(), &separation))
						break;
	#else
					simplex_difference<2>	simp;
					if (hit = simp.gjk(i->support(), j->support(), zero, 0, 0, 0, closestA, closestB, distance)) {
						float2	A[64], B[64], C[64];
						for (int i = 0; i < simp.n; i++) {
							A[i] = simp.get_vertexA(i);
							B[i] = simp.get_vertexB(i);
							C[i] = A[i] - B[i];
						}


						EPAPenetration(i->support(), j->support(), A, B, simp.n, 0, closestA, closestB, normal, distance);

						//int	n = EPAPenetrationDebug(i->support(), j->support(), A, B, simp.n, 32, step);
						int	n = EPAProbeT(support2(make_minkowski_diff(i->support(), j->support())), C, simp.n, 0, step);
						epa_pointsA	= temp_points2((position2*)C, n);
						//epa_pointsB	= temp_points2((position2*)B, n);
						break;
					}
	#endif
				}
				i->Draw(d2d, transform, hit ? red : green, selected_mode == SEL_SHAPE && shapes.index_of(&i) == selected_index ? red : black);

				DrawPoint(d2d,transform, black, closestA);
				DrawPoint(d2d,transform, black, closestB);
				DrawLine(d2d,transform, black, closestA, closestA + normal);

				DrawLines(d2d, transform, d2d::SolidBrush(d2d, colour(0.75f, 0, 0)), epa_pointsA);
	//			DrawLines(d2d, transform, d2d::SolidBrush(d2d, colour(0, 0.75f, 0)), epa_pointsB);
	//			DrawLines(d2d, transform, black, transformc(int_range(epa_pointsA.size()), [&](int i) { return position2(epa_pointsA[i] - epa_pointsB[i]);}));
			}
			break;
		}

		case MODE_INTERSECTION:
		case MODE_UNION:
		case MODE_DIFFERENCE:
		case MODE_XOR: {
			if (selected_mode == SEL_SHAPE && shapes.size() > 1) {
				d2d::SolidBrush	grey(d2d,	colour(.25f,.25f,.25f));

				for (auto &i : shapes)
					i->Draw(d2d, transform, nullptr, grey);

				auto	poly0 = shapes[selected_index]->get_polygon();
				for (auto &i : shapes) {
					if (shapes.index_of(i) != selected_index) {
						auto	poly1	= i->get_polygon();
						auto	polyr	= poly_bool(poly0, poly1, Martinez::Op(mode - MODE_BOOLOPS));
						for (auto &c : reversed(polyr)) {
							dynamic_array<iso::pos<double,2>>		c1 = c.points();
							dynamic_array<position2>	c2 = transformc(c.points(), [](param(double2) i) { return to<float>(i); });
							DrawPoly(d2d, transform, signed_area(c[0], c[1], c[2]) > 0 ? red : green, black, c2);
						}
					}
				}
				break;

			}
		}

		default:
			for (auto &i : shapes)
				i->Draw(d2d, transform, fills[i->mode], selected_mode == SEL_SHAPE && shapes.index_of(&i) == selected_index ? red : black);

			switch (mode) {
				case MODE_POINT_CLOSEST:
					for (auto &i : shapes) {
						position2	b	= i->closest(mouse_pos);
						DrawLine(d2d, transform, green, b, mouse_pos);
						DrawPoint(d2d, transform, green, b);
					}
					break;

				case MODE_POINT_DIST:
					for (auto &i : shapes) {
						float	d	= i->dist(mouse_pos);
						//d2d.DrawEllipse({transform * mouse_pos, d * zoom}, green, 1);
						Draw(d2d, transform, nullptr, green, circle(mouse_pos, d));
					}
					break;

				case MODE_SUPPORT:
					for (auto &i : shapes)
						DrawPoint(d2d, transform, green, i->support(mouse_dir));
					break;

				case MODE_RAY_CHECK:
				case MODE_RAY_CLOSEST: {
					ray2		r(ray_start, mouse_pos);
					DrawLine(d2d, transform, green, ray_start, mouse_pos);

					if (mode == MODE_RAY_CHECK && selected_mode == SEL_SHAPE) {
						auto	b = shapes[selected_index]->support();
						for (auto &i : shapes) {
							float		t;
							float2		normal;
							position2	closestA;
							simplex_difference<2>	simp;
							if (!simp.gjk_sweep(i->support(), b, zero, 0, 0, r.p, r.d, t, normal, closestA))
								continue;
							position2	b	= r.from_parametric(t);
							DrawPoint(d2d, transform, green, b);
							DrawLine(d2d, transform, blue, b, b + normal);
						}
					} else {
						for (auto &i : shapes) {
							float	t;
							float2	normal;
							if (mode == MODE_RAY_CHECK) {
								if (!i->ray_check(r, t, &normal))
									continue;
							} else {
								//t	= i->ray_closest(r);
								position2	closestA;
								simplex_difference<2>	simp;
								bool		hit = simp.gjk_sweep(i->support(), [](param(float2) d) {return position2(zero); }, zero, 0, 0, r.p, r.d, t, normal, closestA);

							}
							position2	b	= r.from_parametric(t);
							DrawPoint(d2d, transform, green, b);
							if (mode == MODE_RAY_CHECK)
								DrawLine(d2d, transform, blue, b, b + normal);
						}
					}
					break;
				}
			}
			break;
	}

	if (p) {
		temp_points2	pts(*p, mode == MODE_CONTOUR ? square(p->size32()) : 1);
		int			nh = 0;

		switch (mode) {
			case MODE_LINES:
				if (curve)
					DrawCurve(d2d, transform, black, *curve);
				else
					DrawLines(d2d, transform, black, pts, false);
				break;

			case MODE_STAR: {
				auto	star = make_star_polygon(pts.cloud());
				DrawPoly(d2d, transform, fill, black, star.points());
				break;
			}

			case MODE_BEZIER:
				if (curve) {
					auto	epsilon = len(pts.get_box().extent()) / 250;
					auto	chains = get_bezier3(*curve);
					for (auto &c : chains) {
						Draw(d2d, transform, nullptr, black, c);

						bezier_chain<float2, 2>	c2 = c.reduce(16, epsilon);
						Draw(d2d, transform, nullptr, green, c2);
					}

				} else {
					bezier_chain<float2, 3>	c2 = dynamic_array<float2>(*p);
					Draw(d2d, transform, nullptr, black, c2);

					for (int i = 0; i < pts.size() - 3; i += 3) {
						bezier_splineT<float2, 3>	b{
							pts[i + 0],
							pts[i + 1],
							pts[i + 2],
							pts[i + 3]
						};
						auto	b2	= make_bezier_chain(b).reduce(6, 0.01f);
						for (auto &j : b2) {
							Draw(d2d, transform, nullptr, red, j);
							DrawPoint(d2d, transform, red, position2(j.evaluate(zero)));
						}
					}
				}
				break;

			case MODE_BOX:
				Draw(d2d, transform, fill, black, pts.cloud().get_obb());
				DrawLines(d2d, transform, green, make_range_n(pts.begin(), nh = generate_hull_2d(pts.begin(), pts.end())));
				break;

			case MODE_HULL:
				nh = generate_hull_2d(pts.begin(), pts.end());
				DrawPoly(d2d, transform, fill, black, slice(pts, 0, nh));
				break;

			case MODE_DELAUNAY: {
				if (!tri)
					tri = new Triangulator(pts.begin(), pts.end());
				int	*indices = alloc_auto(int, tri->NumTriangles() * 3);
				for (int j = 0, nt = tri->GetTriangles(indices); j < nt; j++) {
					position2	t[3] = {
						pts[indices[j*3+0]],
						pts[indices[j*3+1]],
						pts[indices[j*3+2]],
					};
					DrawLines(d2d, transform, black, make_range(t));
				}
				break;
			}

			case MODE_CIRCLE:
				Draw(d2d, transform, fill, black, pts.cloud().circumscribe());
				break;

			case MODE_INCIRCLE:
				FillPoly(d2d, transform, fill, pts);
				Draw(d2d, transform, fill, black, as_simple_polygon(pts.cloud()).inscribe(1e-3f));
				break;

			case MODE_ELLIPSE:
				Draw(d2d, transform, fill, black, pts.cloud().get_ellipse());
				break;

			case MODE_TRIANGLE:
				Draw(d2d, transform, fill, black, pts.cloud().get_tri());
				DrawLines(d2d, transform, green, make_range_n(pts.begin(), nh = generate_hull_2d(pts.begin(), pts.end())));
				break;

			case MODE_QUAD:
				Draw(d2d, transform, fill, black, pts.cloud().get_quad());
				DrawLines(d2d, transform, green, make_range_n(pts.begin(), nh = generate_hull_2d(pts.begin(), pts.end())));
				break;

			case MODE_NGON: {
			#if 1
				nh = optimise_polyline_verts(pts.begin(), pts.size(), step + 3, true);
			#else
				nh = generate_hull_2d(pts.begin(), pts.size());
				nh = optimise_hull_2d(pts.begin(), nh, ngon);
			#endif
				DrawPoly(d2d, transform, fill, black, slice(pts, 0, nh));
				DrawLines(d2d, transform, green, make_range_n(pts.begin(), nh = generate_hull_2d(pts.begin(), pts.end())));
				break;
			}

			case MODE_CUBIC_INTERP: {
				auto	b = bezier_chain<float2p, 3>::interpolating(*p);
				Draw(d2d, transform, nullptr, black, b);
				break;
			}
			case MODE_CUBIC_RELAXED: {
				auto	b = bezier_chain<float2p, 3>::relaxed(*p);
				Draw(d2d, transform, nullptr, black, b);
				break;
			}
			case MODE_CUBIC_CATMULL: {
				auto	b  = catmull_rom_to_bezier<float2p>(*p, 0.5f, nullptr, nullptr);
				Draw(d2d, transform, nullptr, black, b);
				break;
			}

			case MODE_CONTOUR: {
				int		*lengths	= alloc_auto(int, pts.size());
				int		nc			= find_contour(winding_rule, pts.begin(), pts.size(), pts.begin(), lengths);
				auto	p			= pts.begin();
				for (int c = 0; c < nc; ++c) {
					int	n	= lengths[c];
					DrawPoly(d2d, transform, fill, black, make_range_n(p, n));
					nh	+= n;
					p	+= n;
				}
				break;
			}
			case MODE_MARTINEZ: {
				auto	polyr	= poly_bool(as_simple_polygon(pts.cloud()), simple_polygon<array<position2,0>>(), Martinez::XOR);

				complex_polygon<dynamic_array<simple_polygon<temp_points2>>> polygon;
				for (auto &c : polyr.contours())
					polygon.contours().push_back(transformc(c.points(), [](param(double2) i) { return to<float>(i); }));

				Draw(d2d, transform, fill, black, polygon);
				break;
			}
		}

		for (int i = 0; i < pts.size(); i++) {
			if (selected_mode != SEL_POINT || i != selected_index)
				DrawPoint(d2d, transform, black, position2((*p)[i]));
		}

		if (selected_mode == SEL_POINT)
			DrawPoint(d2d, transform, red, position2((*p)[selected_index]));

		if (numbers) {
			if (numbers == NUM_HULL && mode != MODE_DELAUNAY) {
				for (int i = 0; i < nh; i++)
					DrawText(transform, green, pts[i], to_string(i));

			} else {
				for (int i = 0; i < pts.size(); i++)
					DrawText(transform,
						selected_mode == SEL_POINT && i == selected_index ? red : numbers == NUM_HULL ? green : black,
						position2((*p)[i]),
						to_string(numbers == NUM_HULL ? tri->InternalIndex(i) : i)
					);
			}
		}
	}

}

bool ViewTest2D::Set(ISO::Browser b) {
	if (b.GetType() == ISO::REFERENCE) {
		if (auto s = AddShape(*b))
			return true;
	}

	if (auto p2 = ISO_conversion::convert<ISO_openarray<float2p>>(b)) {
		p	= p2;
		tri	= 0;
		return true;
	}

	if (auto p2 = ISO_conversion::convert<ISO_openarray<curve_vertex>>(b)) {
		curve = p2;
		p.Create();
		for (auto &i : *p2)
			p->Append(float2p{i.x, i.y});
		return true;
	}

	return false;
}

bool ViewTest2D::Select(ISO::Browser b) {
	if (p && b.GetTypeDef()->SameAs<float2p>()) {
		int	i = (float2p*)b - *p;
		if (i >= 0 && i < p->Count()) {
			SetSelected(SEL_POINT, i);
			return true;
		}
	}
	if (curve && b.GetTypeDef()->SameAs<curve_vertex>()) {
		int	i = (curve_vertex*)b - *curve;
		if (i >= 0 && i < curve->Count()) {
			SetSelected(SEL_POINT, i);
			return true;
		}
	}

	return false;
}

rectangle ViewTest2D::Init(const ISO_ptr<void> &_p) {
	rectangle	r(none);

	if (Shape2 *s = AddShape(_p)) {
		if (_p.IsType<ISO::bspline2d>() || _p.IsType<ISO::bspline2d2>()) {
			p		= ISO::MakePtr(none, copy(((ISO::bspline2d*)_p)->c));
			mode	= MODE_POINTS;

		} else if (_p.IsType<ISO::nurbs2d2>() || _p.IsType<ISO::nurbs2d>()) {
			auto&	c = ((ISO::nurbs2d*)_p)->c;
			p.Create(none, c.size());
			auto	d = p->begin();
			for (auto& i : c)
				*d++ = project(i);
			mode	= MODE_POINTS;
		}
		r	= s->get_box();

	} else if (p = ISO_conversion::convert<ISO_openarray<float2p>>(_p)) {
		r	= get_extent(*p);
		r	|= r.a + select(r.extent() == zero, float2(one), float2(zero));

	} else if (auto c = ISO_conversion::convert<ISO_openarray<curve_vertex>>(_p)) {
		curve = c;
		p.Create();
		for (auto &i : *c)
			p->Append(float2p{i.x, i.y});
		r	= get_extent(*p);

	} else {
		for (auto i : ISO::Browser2(_p)) {
			if (Shape2 *s = AddShape(i)) {
				r |= s->get_box();

			} else {
				if (auto p1 = (ISO_conversion::convert<ISO_openarray<float2p> >(i))) {
					r |= rectangle(get_extent(*p1));
					if (!p) {
						p = p1;
					} else {
						for (auto &i : *p1)
							p->Append(i);
					}

				} else if (auto c = ISO_conversion::convert<ISO_openarray<curve_vertex>>(i)) {
					if (!p)
						p.Create();
					for (auto &i : *c)
						p->Append(float2p{i.x, i.y});
					r = rectangle(get_extent(*p));
				}
			}
		}
	}
	return r;
}

ViewTest2D::ViewTest2D(MainWindow &main, const WindowPos &wpos, const ISO_VirtualTarget &v)
	: main(main), font(write, L"arial", 16)
	, mode(MODE_LINES), numbers(NUM_OFF), winding_rule(WINDING_NONZERO)
	, zoom(1), pos(zero), prevn(0)
	, currentop(NULL)
{
	Create(wpos, NULL, CHILD | CLIPCHILDREN | VISIBLE, CLIENTEDGE);
	FitExtent(Init(v));
	Invalidate();
}

class EditorTest2D : public Editor {
	virtual bool Matches(const ISO::Browser &b) {
		return ViewTest2D::supported_type(b);
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_VirtualTarget &v) {
		return *new ViewTest2D(main, wpos, v);
	}
public:
	EditorTest2D() {
		ISO::getdef<curve_vertex>();
	}
} editortest2d;
