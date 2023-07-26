#include "base/vector.h"
#include "main.h"
#include "base/algorithm.h"
#include "iso/iso.h"
#include "iso/iso_convert.h"
#include "maths/geometry.h"
#include "maths/geometry_iso.h"
#include "maths/polygon.h"
#include "maths/simplex.h"
#include "mesh/shapes.h"
#include "mesh/shape_gen.h"
#include "graphics.h"
#include "vector_iso.h"
#include "vector_string.h"
#include "windows/d2d.h"
#include "render.h"
#include "extra/random.h"
//#include "2d/fonts.h"
//#include "extra/gjk.h"

namespace iso {
int		generate_hull_3d_partial(const position3 *p, int n, uint16 *indices, int steps);
}

using namespace app;

template<> static const VertexElements ve<ISO::ellipse> = (const VertexElement[]) {
	{&ISO::ellipse::_x,		"position0"_usage},
	{&ISO::ellipse::ratio,	"position1"_usage}
};

template<> static const VertexElements ve<ISO::quadric> = (const VertexElement[]) {
	{&ISO::quadric::d4,		"position0"_usage},
	{&ISO::quadric::d3,		"position1"_usage},
	{&ISO::quadric::o,		"position2"_usage},
};

typedef convex_polyhedron<indexed_container<ref_array_size<position3>, ref_array_size<uint16>>>	ref_polyhedron;

struct temp_points3 : auto_block<position3, 1> {
	temp_points3(ISO_openarray<float3p> &p) : auto_block<position3, 1>(make_auto_block<position3>(p.Count())) {
		copy(p, *(block<float3,1>*)this);
	}
	auto		cloud() const	{ return make_point_cloud(make_range(begin(), end())); }
	position3	closest(param(position3) p) const { return p; }
};


struct Edges {
	typedef	dynamic_array<int>		type;
	dynamic_array<type>	p;

	Edges(int num_verts) : p(num_verts) {}

	bool	check(int v0, int v1)	{
		type	&a	= p[v0];
		for (size_t i = 0, n = a.size(); i < n; i++) {
			if (a[i] == v1)
				return true;
		}
		return false;
	}
	void	add(int v0, int v1)	{
		if (!check(v0, v1) && !check(v1, v0))
			p[v0].push_back(v1);
	}
	void	add(const uint16 *i, int num_faces) {
		for (const uint16 *end = i + num_faces * 3; i < end; i += 3) {
			int	v0	= i[0], v1 = i[1], v2 = i[2];
			int	i = max(v0, v1, v1);
			if (i >= p.size())
				p.resize(i + 1);
			add(v0, v1);
			add(v1, v2);
			add(v2, v0);
		}
	}
	size_t	size() const {
		size_t	t = 0;
		for (auto *i = p.begin(), *e = p.end(); i != e; ++i)
			t += i->size();
		return t;
	}
};

void Fill(GraphicsContext &ctx, const ref_polyhedron &poly) {
	ImmediateStream<float3p>	im(ctx, PRIM_TRILIST, poly.points().size32());
	float3p	*p = im.begin();
	for (auto j : poly.points())
		*p++ = j;
}

void Line(GraphicsContext &ctx, const ref_polyhedron &poly) {
	Edges	edges(poly.points().direct().size32());
	edges.add(poly.points().indices().begin(), poly.points().size32() / 3);

	ImmediateStream<float3p>	im(ctx, PRIM_LINESTRIP, uint32(edges.size()) * 2);

	float3p	*p = im.begin();
	auto	*e = edges.p.begin();
	for (auto p0 : poly.points().direct()) {
		for (auto i1 : *e) {
			*p++ = p0;
			*p++ = poly.points().direct()[i1];
		}
		++e;
	}
}

#if 0
void DrawPlane(GraphicsContext &ctx, const plane &p, const float4x4 &wvp) {
	array<plane,6>	planes = get_inv_planes(frustum(wvp));
	swap(planes[4], planes[5]);
	halfspace_intersection3	h(slice(planes, 0, 5));

	position3	verts[16];
	auto		cp	= h.cross_section(p, verts);
	if (cp.size() > 2) {
		float3		n	= p.normal();

		float4	verts2[16], *v2 = verts2;
		for (auto &i : cp.points())
			*v2++ = wvp * i;

		if ((wvp * p).unnormalised_dist() > zero)
			n = -n;

		ImmediateStream<VertexIndexBuffer::vertex>	imm(ctx, PRIM_TRIFAN, cp.size32());
		auto	*v = imm.begin();
		for (auto &i : cp.points())
			v++->set(i, n);
	}
}
#endif

//-----------------------------------------------------------------------------
//	Shape3
//-----------------------------------------------------------------------------

struct ref_planes : halfspace_intersection3 {
	ref_array<plane>	ref;
	ref_planes() : halfspace_intersection3(none) {}
	ref_planes(decltype(none)) : halfspace_intersection3(none) {}
	ref_planes(const ref_array<plane> &p, int n) : halfspace_intersection3(make_range_n(p.begin(), n)), ref(p) {}
	template<typename C> ref_planes(C &&c) : ref_planes(ref_array<plane>(c), 6) {}
};

auto	get_planes(...)		{ return none; }
template<typename T, typename=decltype(declval<T>().plane(PLANE_MINUS_X))>	auto	get_planes(const T &t)		{ return transformc(planes<3>(), [t](PLANE i){ return t.plane(i); }); }

struct Shape3 {
	enum MODE {
		normal,
		occluder,
		includer,
	} mode;
	bool	hit;

	Shape3() : mode(normal) {}
	virtual	~Shape3()	{}
	virtual void		Move(const float3x4 &m) = 0;
	virtual void		Draw(RenderEvent *re, pass *p)	const = 0;
	virtual	position3	centre()						const = 0;
	virtual bool		ray_check(param(ray3) r, float &t, vector3 *normal) const = 0;
	virtual position3	closest(param(position3) p)		const = 0;
	virtual position3	support(param(float3) v)		const = 0;
	virtual async_callback<position3(param(float3))> support()const = 0;
	virtual bool		is_visible(const float4x4 &m)	const = 0;
	virtual bool		is_within(const float4x4 &m)	const = 0;
	virtual bool		contains(param(position3) pos)	const = 0;
	virtual cuboid		get_box()						const = 0;
	virtual ref_planes	get_planes()					const = 0;
	virtual sphere		circumscribe()					const = 0;
	virtual sphere		inscribe()						const = 0;
	virtual float		ray_closest(param(ray3) r)		const = 0;

	virtual async_callback<position3(param(float2))>	surface_optimised()		const = 0;
	virtual async_callback<position3(param(float3))>	interior_optimised()	const = 0;

	friend	auto		surface_optimised(const Shape3 &s)					{ return s.surface_optimised();	}
	friend	auto		interior_optimised(const Shape3 &s)					{ return s.interior_optimised();	}
};

//template void iso::deleter<Shape3>(Shape3*);


template<typename T> void View3DDraw(RenderEvent *re, const T &x, pass *p) {
	iso::Draw(re, x, p);
}

void draw_quadric(RenderEvent *re, const quadric &q, pass *p) {
	static pass *thickpoint		= *ISO::root("data")["default"]["thickpoint"][0];

	auto		&ctx	= re->ctx;
	re->consts.SetWorld(identity);

#if 0
	static pass *circletess		= *ISO::root("data")["quadric"]["quadric_tess4"][0];

//	ctx.SetBackFaceCull(BFC_NONE);
	Set(ctx, circletess, ISO::MakeBrowser(re->consts));
	ImmediateStream<iso_quadric>	im(ctx, PatchPrim(1), 1);
	im.begin()[0] = q;
#elif 1
	static pass *circletess		= *ISO::root("data")["quadric"]["quadric"][0];
	Set(ctx, circletess, ISO::MakeBrowser(re->consts));
	ImmediateStream<ISO::quadric>	im(ctx, PRIM_POINTLIST, 1);
	im.begin()[0] = q;
#else
//	static pass *circletess		= *ISO::root("data")["default"]["thickcircletessA"][0];
//	static pass *circletess		= *ISO::root("data")["default"]["circletess"][0];
//	static pass *circletess		= *ISO::root("data")["default"]["ellipsetess"][0];
	static pass *circletess		= *ISO::root("data")["default"]["ellipseext"][0];

	quadric		q2		= re->consts.viewProj * q;
	quadric		q3		= q2 / translate(q2.centre());
	auto		qt2		= q2.get_tangents();

	conic		slice	= strip_x(q3);
	auto		ct		= slice.get_tangents();
	auto		zc		= slice & y_axis;

	position3	centre	= qt2.translation();
	float3		xshear	= qt2.x;
	float2		yshear	= ct.x;
	float		zscale	= zc.length() * half;

	lower3		low3(float3(xshear.x, yshear.x, -zscale), float3(xshear.y, yshear.y, xshear.z));

	conic		c		= q2.project_z();

#if 1
	auto		r0		= c.get_box();
	cuboid		box1	= q.get_box();
	cuboid		box2	= q2.get_box();
	auto		r1		= box2.rect_xy();

	float4x4	nmat	= ~low3 * (translate(-centre) * re->consts.viewProj);

	for (int x = 0; x < 100; x++) {
		for (int y = 0; y < 100; y++) {
			float2	p2	= float2(x, y) / (100/2) - one;
			ray3	r(position3(r1.uniform_interior((p2 + one) * half), -one), z_axis);
			float	t;
			if (q2.ray_check(r, t, nullptr)) {
				if (len2(p2) < one) {
					float3	p3(p2, sqrt(one - len2(p2)));
					position3	p0 = r.from_parametric(t);
					position3	p1 = centre + xshear * p3.x + float3(zero, yshear) * p3.y - float3(zero, zero, zscale) * p3.z;
					position3	p4 = centre + low3 * p3;
					position3	wpos = project(p1 / re->consts.viewProj);

					float		p3w		= -dot(nmat.w.xyz, p3) / nmat.w.w;
					auto		norm	= float4(p3, p3w) * nmat;

					ISO_ASSERT(approx_equal(p0, p1, 1e-2f));
				}
			}
		}
	}
#endif

	if (c.analyse().type == conic::ELLIPSE) {
		//ellipse			e		= c;
		//shear_ellipse		e		= c;
		shear_ellipse		e(float2x3(xshear.xy, float2(zero, yshear.x), centre.xy));
		re->consts.SetViewProj(identity);
		re->consts.SetWorld(float3x4(e.matrix()));
	#if 1
		static pass *thicklineA		= *ISO::root("data")["default"]["thicklineA"][0];
		Set(ctx, thicklineA, ISO::MakeBrowser(re->consts));
		CircleVB::get<64>().RenderOutlineA(ctx);
	#else
		//point_size		= float3(8.f / width, 8.f / height, width);
		//diffuse_colour	= colour(0,0,0, 1);

//		ctx.SetBackFaceCull(BFC_NONE);
		Set(ctx, circletess, ISO::MakeBrowser(re->consts));
		ImmediateStream<iso_ellipse>	im(ctx, PRIM_POINTLIST, 1);
		im.begin()[0] = e;
	#endif


	}
#endif
}

void View3DDraw(RenderEvent *re, const ellipsoid &e, pass *p) {
	iso::Draw(re, e, p);
	draw_quadric(re, e, p);
}

void View3DDraw(RenderEvent *re, const sphere &s, pass *p) {
	iso::Draw(re, s, p);
	draw_quadric(re, s.matrix() * quadric::standard_sphere(), p);
}

void View3DDraw(RenderEvent *re, const ref_polyhedron &s, pass *p) {
	Set(re->ctx, p, ISO::MakeBrowser(re->consts));

	float3	*normals	= alloc_auto(float3, s.points().size() / 3), *pn = normals;
	for (auto j : make_split_range<3>(s.points()))
		*pn++ = -normalise(cross(j[1] - j[0], j[2] - j[0]));

	ImmediateStream<VertexIndexBuffer::vertex>	im(re->ctx, PRIM_TRILIST, s.points().size32());
	auto	*d	= im.begin();
	int		i	= 0;
	for (auto &&j : s.points()) {
		d++->set(j, normals[i++ / 3]);
	}
}

template<typename T> struct ShapeT : Shape3 {
	T	x;
	ShapeT(const T &_t) : x(_t) {}
	ShapeT(T &&_t) : x(move(_t)) {}
	void		Move(const float3x4 &m)											{ x *= m; }
	void		Draw(RenderEvent *re, pass *p)							const	{ View3DDraw(re, x, p); }
	position3	centre()												const	{ return x.centre(); }
	bool		ray_check(param(ray3) r, float &t, vector3 *normal)		const	{ return x.ray_check(r, t, normal); }
	position3	closest(param(position3) p)								const	{ return x.closest(p); }
	position3	support(param(float3) v)								const	{ return x.support(v); }
	async_callback<position3(param(float3))> support()const	{ return [&x=x](param(float3) v) { return x.support(v); };	}
	bool		is_visible(const float4x4 &m)							const	{ using iso::is_visible; return is_visible(x, m); }
	bool		is_within(const float4x4 &m)							const	{ using iso::is_within; return is_within(x, m); }
	bool		contains(param(position3) pos)							const	{ return x.contains(pos); }
	cuboid		get_box()												const	{ return x.get_box(); }
	ref_planes	get_planes()											const	{ return ::get_planes(x); }
	sphere		circumscribe()											const	{ using iso::circumscribe; return circumscribe(x); }
	sphere		inscribe()												const	{ using iso::inscribe; return inscribe(x); }
	float		ray_closest(param(ray3) r)								const	{ return x.ray_closest(r); }

	async_callback<position3(param(float2))>	surface_optimised()		const	{ using iso::surface_optimised; return surface_optimised(x); }
	async_callback<position3(param(float3))>	interior_optimised()	const	{ using iso::interior_optimised; return interior_optimised(x); }
};

template<typename T> Shape3* make_shape(T &&t) { return new ShapeT<typename T_noref<T>::type>(forward<T>(t)); }

//-----------------------------------------------------------------------------
//	occlusion
//-----------------------------------------------------------------------------

static int process_occlusion_face(const halfspace_intersection3 &inc, const convex_polygon<range<position3*>> &occ, wedge3 *wedges) {
	wedge3	*wedges0	= wedges;
	bool	clipped		= false;
	/*
	position3		*incv	= alloc_auto(position3, inc.size());
	convex_polygon3	incp	= inc.cross_section(occ.plane(), incv);
	if (incp.empty())
		return 0;
	*/

	for (auto &e1 : inc.planes().slice(-1)) {
		float	i0, i1;
		if (!occ.find_split(e1, i0, i1))
			return 0;		// occluder face totally outside includer

		if (i0 == 0)
			continue;		// occluder face not clipped by this includer face

		// line of occluder face in includer plane
		position3	x0	= occ.perimeter(i0);
		position3	x1	= occ.perimeter(i1);

		for (auto &e2 : inc.planes().slice(&e1 + 1)) {
			float	d0 = e2.dist(x0), d1 = e2.dist(x1);

			if (d0 < 0 && d1 < 0)
				break;			// clipped occluder edge totally outside includer

			if (d0 * d1 < 0) {
				bool		outside = false;
				position3	x		= position3((x0 * d1 - x1 * d0) / (d1 - d0));
				for (auto e3 : slice(inc.planes(), &e2 + 1)) {
					if (!e3.test(x))
						outside = true;
				}
				if (outside)
					continue;	// no visible region

				clipped	= true;

				float3	a	= -occ.normal(), b = e1.normal(), c = e2.normal();
				if (d0 < 0)
					swap(a, b);

				*wedges++ = wedge3(x, a, b, c);
			}
		}
		if (!clipped) {
			//use plane
			//*wedges++ = occ.plane();
		}
	}
	return wedges - wedges0;
	return 0;
}


static bool clipped_test(Shape3 *s, const halfspace_intersection3 &inc, const convex_polygon<range<position3*>> &occ) {
	bool clip = false;

	for (auto &e1 : inc.planes()) {

		if (e1.test(s->support(-e1.normal())))
			continue;			// shape totally inside includer plane

		float i0, i1;
		if (!occ.find_split(e1, i0, i1))
			return true;		// occluder face totally outside includer

		if (i0 == 0)
			continue;			// occluder face not clipped by includer

		// line of occluder face in includer plane
		position3	x0	= occ.perimeter(i0);
		position3	x1	= occ.perimeter(i1);
		clip = true;

		for (auto &e2 : inc.planes().slice(&e1 + 1)) {
			float	d0 = e2.dist(x0), d1 = e2.dist(x1);
			if (d0 < 0 && d1 < 0)
				break;			// clipped occluder edge totally outside includer

			if (d0 * d1 < 0) {
				position3	x	= position3((x0 * d1 - x1 * d0) / (d1 - d0));
				if (!inc.contains(x))
					continue;	// no visible region

				float3		a	= -occ.normal(), b = e1.normal(), c = e2.normal();
				if (d0 < 0)
					swap(a, b);

				if (wedge3(x, a, b, c).contains_shape(*s))
					return false;
			}
		}
	}
	return clip;
}

static bool test_occlusion(Shape3 *s, const range<Shape3**> &includers, const range<Shape3**> &occluders) {
	bool		vis = includers.empty();
	ref_planes	inc;

	for (auto i : includers) {
		auto planes = i->get_planes();
		if (planes.overlaps_shape(*s)) {
			inc = planes;
			vis = true;
			break;
		}
	}

	if (vis) {
		position3	verts[16];	// for storing convex_polygon3 verts

		for (auto i : occluders) {
#if 1
			auto	planes	= i->get_planes();
			wedge3	wedges[20], *pwedges = wedges;
			for (auto e : planes) {
				pwedges += process_occlusion_face(inc, planes.cross_section(e, verts), pwedges);
			}

			bool	all	= pwedges > wedges;
			for (auto &w : range<wedge3*>(wedges, pwedges)) {
				all &= !w.contains_shape(*s);
			}

#else
			bool	all	= true;
			auto	planes	= i->get_planes();
			for (auto e : planes) {
				all &= !e.test(s->support(e.normal())) || clipped_test(s, inc, planes.cross_section(e, verts));
			}
#endif
			if (all) {
				vis = false;
				break;
			}
		}
	}
	return vis;
}

//-----------------------------------------------------------------------------
//	ViewTest3D
//-----------------------------------------------------------------------------

struct DXTarget : d2d::Target {
	bool Init(IDXGISurface *s) {
			D2D1_RENDER_TARGET_PROPERTIES	props = {
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			{DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_IGNORE},
			0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
		};

		com_ptr<ID2D1RenderTarget>		target0;
		return	SUCCEEDED(factory->CreateDxgiSurfaceRenderTarget(s, &props, &target0))
			&&	SUCCEEDED(target0.query(&device));
	}

	DXTarget(IDXGISurface *s) {
		Init(s);
	}

	DXTarget(ID3D11Resource *r) {
		Init(temp_com_cast<IDXGISurface>(r));
	}
};

class ViewTest3D : public aligned<Window<ViewTest3D>, 16>, Graphics::Display {
	enum MODE {
		MODE_POINTS,
		MODE_LINES,
		MODE_HULL,
		MODE_SPHERE,
		MODE_INSPHERE,
		MODE_ELLIPSOID,
		MODE_BOX,
		MODE_HULLDUAL,
		MODE_CLOSEST,
		MODE_SUPPORT,
		MODE_RAY_CLOSEST,
		MODE_COLLISION,
		MODE_OCCLUSION,
	};
	enum SELECTED_MODE {
		SEL_NONE,
		SEL_POINT,
		SEL_SHAPE
	};
	enum NUMBERS {
		NUM_OFF,
		NUM_BASE,
		NUM_HULL,
	};
	enum COMMAND {
		SET_MODE			= 0x100,
		SET_NUMBERS			= 0x200,
		MISC				= 0x300,
		RANDOMISE_INTERIOR	= MISC,
		RANDOMISE_SURFACE,
		REGULAR_INTERIOR,
		REGULAR_SURFACE,
		ADD,	
		REMOVE,
		PROJECTION,
		STEP_UP,
		STEP_DOWN,
	};
	friend NUMBERS operator++(NUMBERS &n) { return n = NUMBERS((n + 1) % 3); }

	struct ShapeType { const ISO::Type *type; Shape3 *(*make)(const ISO_ptr<void> &p); };
	static ShapeType	shape_types[];

	MainWindow				&main;
	Accelerator				accel;
	ToolBarControl			toolbar;
	point					mouse;
	rot_trans				view_loc;
	float					move_scale;
	MODE					mode;
	NUMBERS					numbers;
	SELECTED_MODE			selected_mode;
	position3				selected_pos;
	position3				ray_start;
	int						selected_index, highlighted;
	int						over;
	int						step;
	bool					perspective;

	ISO_ptr<ISO_openarray<float3p> >	p;
	dynamic_array<unique_ptr<Shape3>>	shapes;
	ModifyOp				*currentop;

	void	FitExtent(param(cuboid) r) {
		auto	ext = r.extent();
		view_loc = rot_trans(identity, r.centre() + float3{zero, zero, ext.z + max(ext.x, ext.y)});
	}

	template<typename L> void	GenerateSurface(L&& lambda) {
		if (p && selected_mode == SEL_SHAPE) {
			int	n = p->Count();
			*make_rangec(*p) = generate_surface(*shapes[selected_index], lambda, n);
		}
	}

	template<typename L> void	GenerateInterior(L&& lambda) {
		if (p) {
			int	n = p->Count();
			if (selected_mode == SEL_SHAPE) {
				*make_rangec(*p) = generate_interior(*shapes[selected_index], lambda, n);
			} else {
				generate_interior(unit_cube, lambda, p->begin(), n);
			}
		}
	}

	float4x4 GetProjMatrix() const {
		return perspective
			? perspective_projection(1.f, width/float(height), .1f)
			: parallel_projection(1.f, width/float(height), -1.f, 1.f);
	}
	float4x4 GetMatrices(float3x4 &view, float4x4 &proj) const {
		view	= view_loc;
		proj	= GetProjMatrix();
		return proj * (float4x4)view;
	}
	float4x4 GetMatrices() const {
		float3x4	view;
		float4x4	proj;
		return GetMatrices(view, proj);
	}
	float3x4 ViewportMatrix() const {
		return scale(float3{width / 2, height / 2, one}) * translate(one, one, zero);
	}

	ray3	MouseRay(const point &pt) const {
		float4x4	m	= (float4x4)get(inverse(ViewportMatrix() * GetMatrices()));
		auto		p0	= m * position3(pt.x, pt.y, -1);
		auto		p1	= m * position3(pt.x, pt.y, 1);
		auto		d	= p1 - p0;
		return ray3(p0, float4(d).xyz);
	}
	ray3	MouseRay() const {
		return MouseRay(mouse);
	}

	int		FindPoint(const point &mouse);
	void	Paint(GraphicsContext &ctx);
	Shape3*	AddShape(const ISO_ptr<void> &p);

public:

	static bool supported_type(const ISO::Type *type);

	static bool supported_type(const ISO::Browser &b) {
		if (const ISO::Type *type = b.GetTypeDef()) {
			if (supported_type(type))
				return true;

/*			if (type->GetType() == ISO::VIRTUAL && b.Count() > 1) {
				ISO::Browser2	e = b[0];
				if (e.GetTypeDef()->IsPlainData() && e.Count() == 3)
					return true;
			}
			*/
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
	ViewTest3D(MainWindow &_main, const WindowPos &wpos, const ISO_ptr<void> &_p);
};

ViewTest3D::ShapeType	ViewTest3D::shape_types[] = {
	{ ISO::getdef<sphere	>(), [](const ISO_ptr<void> &p) { return make_shape(*(sphere	*)p); } },
	{ ISO::getdef<ellipsoid	>(), [](const ISO_ptr<void> &p) { return make_shape(*(ellipsoid	*)p); } },
	{ ISO::getdef<cuboid	>(), [](const ISO_ptr<void> &p) { return make_shape(obb3(*(cuboid*)p)); } },
	{ ISO::getdef<cylinder	>(), [](const ISO_ptr<void> &p) { return make_shape(*(cylinder	*)p); } },
	{ ISO::getdef<capsule	>(), [](const ISO_ptr<void> &p) { return make_shape(*(capsule	*)p); } },
	{ ISO::getdef<cone		>(), [](const ISO_ptr<void> &p) { return make_shape(*(cone		*)p); } },
	{ ISO::getdef<tetrahedron>(),[](const ISO_ptr<void> &p) { return make_shape(*(tetrahedron*)p); } },
	{ ISO::getdef<circle3	>(), [](const ISO_ptr<void> &p) { return make_shape(*(circle3	*)p); } },
};

bool ViewTest3D::supported_type(const ISO::Type *type) {
	if (!type)
		return false;

	for (auto &i : shape_types) {
		if (type->SameAs(i.type))
			return true;
	}
	if (type->GetType() != ISO::ARRAY && type->GetType() != ISO::OPENARRAY)
		return false;

	if (type->SubType()->Is("curve_vertex"))
		return false;

	const ISO::Type *sub = type->SubType()->SkipUser();
	if (!sub->IsPlainData())
		return false;

	return (sub->GetType() == ISO::ARRAY && ((ISO::TypeArray*)sub)->Count() == 3)
		|| (sub->GetType() == ISO::COMPOSITE && ((ISO::TypeComposite*)sub)->Count() == 3);
}

quaternion	GetCompassRot(int c) {
	switch (c) {
		case 1: {
			auto		lr1	= log(rotate_in_z(pi * half));
			auto		lr2	= log(rotate_in_x(pi * half));
			auto		lr3	= lr1 + lr2;
			quaternion	q1	= rotate_in_z(pi * half);
			quaternion	q2	= rotate_in_x(pi * half);
			quaternion	q3	= q2 * q1;
			quaternion	q1b	= exp(lr1);
			quaternion	q2b	= exp(lr2);
			quaternion	q3b	= exp(lr3);

			rotation_vector		r1	= rotate_in_z(pi * half);
			rotation_vector		r2	= rotate_in_x(pi * half);
			rotation_vector		r3	= r1 * r2;

			return q3;

			return exp(log(rotate_in_x(pi * half)));// * rotate_in_z(pi * half);
		}
		case 2:		return rotate_in_x(pi * half);
		default:	return identity;
		case -1:	return rotate_in_x(pi * half) * rotate_in_z(pi * half) * rotate_in_z(pi);
		case -2:	return rotate_in_x(pi * half) * rotate_in_z(pi);
		case -3:	return rotate_in_x(pi);
	}
}

LRESULT ViewTest3D::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			Accelerator::Builder	ab;
			Menu	menu	= Menu::Create();
			Menu	sub		= Menu::Create();

			menu.Append("Mode", sub);
			ab.Append(sub, "Points",		SET_MODE | MODE_POINTS,			'P');
			ab.Append(sub, "Lines",			SET_MODE | MODE_LINES,			'L');
			ab.Append(sub, "Hull",			SET_MODE | MODE_HULL,			'H');
			ab.Append(sub, "Sphere",		SET_MODE | MODE_SPHERE,			'S');
			ab.Append(sub, "Insphere",		SET_MODE | MODE_INSPHERE,		'I');
			ab.Append(sub, "Ellipsoid",		SET_MODE | MODE_ELLIPSOID,		'E');
			ab.Append(sub, "Box",			SET_MODE | MODE_BOX,			'B');
			ab.Append(sub, "Hull Dual",		SET_MODE | MODE_HULLDUAL);
			ab.Append(sub, "Closest Point",	SET_MODE | MODE_CLOSEST);
			ab.Append(sub, "Support Point",	SET_MODE | MODE_SUPPORT);
			ab.Append(sub, "Ray Closest",	SET_MODE | MODE_RAY_CLOSEST);
			ab.Append(sub, "Show Collision",SET_MODE | MODE_COLLISION);
			ab.Append(sub, "Show Occlusion",SET_MODE | MODE_OCCLUSION);

			sub		= Menu::Create();
			menu.Append("Numbers", sub);
			sub.Append("Off",			SET_NUMBERS | NUM_OFF);
			sub.Append("Base",			SET_NUMBERS | NUM_BASE);
			sub.Append("Hull",			SET_NUMBERS | NUM_HULL);

			sub		= Menu::Create();
			menu.Append("Options", sub);
			ab.Append(sub, "Randomise Interior",	RANDOMISE_INTERIOR,		'R');
			ab.Append(sub, "Randomise Perimeter",	RANDOMISE_SURFACE,		{'R', Accelerator::Key::SHIFT});
			ab.Append(sub, "Regular Interior",		REGULAR_INTERIOR,		{'R', Accelerator::Key::CTRL});
			ab.Append(sub, "Regular Perimeter",		REGULAR_SURFACE,		{'R', Accelerator::Key::SHIFT | Accelerator::Key::CTRL});
			ab.Append(sub, "Add shape",				ADD,					VK_RETURN);
			ab.Append(sub, "Remove selected",		REMOVE,					VK_BACK);
			ab.Append(sub, "Perspective",			PROJECTION,				VK_DIVIDE);

			accel	= Accelerator(ab);

			toolbar.Create(*this, 0, CHILD | VISIBLE);
			toolbar.Init(menu);

			move_scale	= 0;
			if (p && p->Count()) {
				cuboid	box(empty);
				float3p	*p2	= *p;
				for (int n = p->Count(); n--; p2++)
					box |= position3(*p2);
				move_scale	= len(box.extent());
			}
			if (move_scale == 0)
				move_scale = 1;
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
			SetSize((RenderWindow*)hWnd, rect.Size());
			Invalidate();
			break;
		}

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			Rect	client	= GetClientRect();

			GraphicsContext ctx;
			graphics.BeginScene(ctx);
			ctx.SetRenderTarget(GetDispSurface());
			ctx.SetZBuffer(SurfaceT<norm24_uint8>(Size(), MEM_DEPTH));
			Paint(ctx);
			graphics.EndScene(ctx);
			Present();//&client);
			break;
		}

		case WM_CONTEXTMENU: {
			auto		m = ToClient(Point(lParam));
			ray3		r	= MouseRay();
			for (auto &i : shapes) {
				float	t;
				vector3	normal;
				if (i->ray_check(r, t, &normal)) {
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
							over = -1;
						} else {
							i->mode = (Shape3::MODE)(x - 1);
						}
					}
					break;
				}
			}
			Invalidate();
			break;
		}

		case WM_LBUTTONDOWN: {
			mouse		= Point(lParam);
			if (int axis = GetAxesClick(parallel_projection(0.7f, 0.7f, -10.f, 10.f) * view_loc.rot, (to<float>(mouse) - float2{50, height - 50}) / float2{50, -50})) {
				quaternion	q = GetCompassRot(axis);
				if (all(q == view_loc.rot))
					q = rotate_in_x(pi) * q;
				view_loc.rot = q;

			} else if (mode == MODE_RAY_CLOSEST) {
				ray3	r	= MouseRay();
				ray_start	= r & plane(z_axis);

			} else {
				selected_mode	= SEL_NONE;
				selected_index		= FindPoint(mouse);
				if (selected_index >= 0) {
					selected_mode	= SEL_POINT;
					highlighted		= selected_index;

				} else {
					ray3		r	= MouseRay();
					for (auto &i : shapes) {
						float	t;
						vector3	normal;
						if (i->ray_check(r, t, &normal)) {
							selected_index		= shapes.index_of(i);
							selected_mode	= SEL_SHAPE;
							selected_pos	= r.from_parametric(t);
							break;
						}
					}
				}
			}
			Invalidate();
			SetFocus();
			break;
		}

		case WM_RBUTTONDOWN: {
			mouse		= Point(lParam);
			Invalidate();
			break;
		}

		case WM_LBUTTONUP:
			currentop	= NULL;
			break;

		case WM_MOUSEMOVE: {
			point	pt	= Point(lParam);
			ray3	r	= MouseRay(pt);
			int		over2	= -1;
			for (auto &i : shapes) {
				float	t;
				vector3	normal;
				if (i->ray_check(r, t, &normal)) {
					over2 = shapes.index_of(i);
					break;
				}
			}
			if (over2 != over) {
				over = over2;
				Invalidate();
			}

			if (wParam & MK_LBUTTON) {
				switch (selected_mode) {
					case SEL_POINT: {
						float3p		&x = (*p)[selected_index];
						float4x4	vp = (float4x4)ViewportMatrix() * GetMatrices();
						position3	pos = vp * position3(x);
						pos.v.xy += float2{float(pt.x - mouse.x), float(pt.y - mouse.y)};
						pos = project(float4(pos) / vp);
						x = pos;
						break;
					}
					case SEL_SHAPE: {
						float4x4	m	= ViewportMatrix() * GetMatrices();
						position3	p0	= selected_pos;
						position3	p1	= position3(to<float>(pt), simple_cast<position3>(m * selected_pos).v.z) / m;
						if (wParam & MK_CONTROL) {
							position3	c	= shapes[selected_index]->centre();
							quaternion	q	= quaternion::between(p0 - c, p1 - c);
							float3x4	m	= translate(c) * q * translate(-c);
							shapes[selected_index]->Move(m);
						} else {
							shapes[selected_index]->Move(translate(p1 - p0));
						}
						selected_pos = p1;
						break;
					}
					default: {
						quaternion	q(normalise(float4{float(pt.y - mouse.y), float(pt.x - mouse.x), 0, 512}));
						view_loc *= q;
						break;
					}
				}
				Invalidate();
			} else if (wParam & MK_RBUTTON) {
				view_loc *= translate(float3{float(pt.x - mouse.x), float(mouse.y - pt.y), 0} * (move_scale / 256));
				Invalidate();
			}
			if (mode == MODE_CLOSEST || mode == MODE_SUPPORT || mode == MODE_RAY_CLOSEST)
				Invalidate();
			mouse = pt;
			break;
		}

		case WM_MOUSEWHEEL:
			view_loc *= translate(float3{0, 0, (short)HIWORD(wParam) * (move_scale / 2048)});
			Invalidate();
			break;

		case WM_COMMAND:
			switch (wParam & 0xff00) {
				case SET_MODE:
					mode	= MODE(wParam & 0xff);
					step	= 0;
					Invalidate();
					return 0;

				case SET_NUMBERS:
					numbers = NUMBERS(wParam & 0xff);
					Invalidate();
					return 0;

				default: switch (LOWORD(wParam)) {
					case ID_EDIT: {	// from main
						ISO::Browser b	= *(ISO::Browser*)lParam;

						if (b.GetType() == ISO::REFERENCE) {
							if (auto s = AddShape(*(ISO_ptr<void>*)b)) {
								Invalidate();
								return true;
							}
						}

						if (p && b.GetTypeDef()->SameAs<float3p>()) {
							int	i = (float3p*)b - *p;
							if (i >= 0 && i < p->Count()) {
								selected_index	= highlighted = i;
								Invalidate();
								return true;
							}
						}
						break;
					}
					case RANDOMISE_INTERIOR:
						GenerateInterior([](int k, int n) { return random.get<float3>(); });
						Invalidate();
						return 0;

					case RANDOMISE_SURFACE:
						GenerateSurface([](int k, int n) { return random.get<float2>(); });
						Invalidate();
						return 0;

					case REGULAR_INTERIOR:
						GenerateInterior(halton3<2,3,5>);
						Invalidate();
						return 0;

					case REGULAR_SURFACE:
						GenerateSurface(halton2<2,3>);
						Invalidate();
						return 0;

					case ADD:
						if (p) {
							temp_points3	pts(*p);
							switch (mode) {
								case MODE_HULL: {
									int		max_indices	= ref_polyhedron::max_indices(pts.size());
									uint16	*i			= alloc_auto(uint16, max_indices);
									int		n			= generate_hull_3d_partial(pts.begin(), pts.size(), i, step == 0 ? pts.size() : min(step - 1, pts.size()));
									shapes.push_back(make_shape(ref_polyhedron(make_indexed_container(ref_array_size<position3>(pts.cloud().points()), ref_array_size<uint16>(make_range_n(i, n))))));
									break;
								}
								case MODE_SPHERE:
									shapes.push_back(make_shape(pts.cloud().circumscribe()));
									break;

								case MODE_INSPHERE:
									shapes.push_back(make_shape(halfspace_intersection3(pts.cloud(), alloc_auto(plane, halfspace_intersection3::max_planes(pts.size()))).inscribe()));
									break;

								case MODE_ELLIPSOID:
									shapes.push_back(make_shape(pts.cloud().get_ellipse()));
									break;

								case MODE_BOX:
									shapes.push_back(make_shape(pts.cloud().get_box()));
									break;

								case MODE_HULLDUAL: {
									halfspace_intersection3	h(pts.cloud(), alloc_auto(plane, halfspace_intersection3::max_planes(pts.size())));
									auto		points		= h.points(alloc_auto(position3, h.max_verts()));
									int			max_indices	= ref_polyhedron::max_indices(points.points().size32());
									uint16		*i			= alloc_auto(uint16, max_indices);
									shapes.push_back(make_shape(ref_polyhedron(make_indexed_container(ref_array_size<position3>(points.points()), ref_array_size<uint16>(make_range_n(i, max_indices))))));
									break;
								}
								default:
									break;
							}
						}
						Invalidate();
						return 0;

					case REMOVE:
						switch (selected_mode) {
							case SEL_SHAPE:
								shapes.erase(&shapes[selected_index]);
								over = -1;
								break;
							case SEL_POINT:
								p->Remove(*p + selected_index);
								break;
						}
						selected_mode	= SEL_NONE;
						Invalidate();
						return 0;

					case PROJECTION:
						perspective = !perspective;
						Invalidate();
						return 0;

					case STEP_UP:
						step++;
						Invalidate();
						break;

					case STEP_DOWN:
						if (step) {
							--step;
							Invalidate();
						}
						break;
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


int ViewTest3D::FindPoint(const point &mouse) {
	if (p) {
		float4x4	vp = (float4x4)ViewportMatrix() * GetMatrices();

		for (int i = 0, n = p->Count(); i < n; i++) {
			position3	v = vp * position3((*p)[i]);
			if (abs(int(v.v.x) - mouse.x) <= 8 && abs(int(v.v.y) - mouse.y) <= 8)
				return i;
		}
	}
	return -1;
}

void ViewTest3D::Paint(GraphicsContext &ctx) {
	ctx.Clear(colour(one));
	ctx.SetDepthTestEnable(true);
	ctx.SetDepthTest(DT_USUAL);

	static pass *space_grid		= *ISO::root("data")["default"]["space_grid"][0];
	static pass *specular		= *ISO::root("data")["default"]["specular"][0];
	static pass *specular_grid	= *ISO::root("data")["default"]["specular_grid"][0];
	static pass *blend			= *ISO::root("data")["default"]["blend"][0];
	static pass *thickpoint		= *ISO::root("data")["default"]["thickpoint"][0];
	static pass *thickline		= *ISO::root("data")["default"]["thickline"][0];
	static pass *thicklineA		= *ISO::root("data")["default"]["thicklineA"][0];
//	static pass *circletess		= *ISO::root("data")["default"]["circletess"][0];
	static pass *circletess		= *ISO::root("data")["default"]["thickcircletessA"][0];

	ShaderConsts	consts;
	RenderEvent	re(ctx, consts);

	consts.SetProj(GetProjMatrix());
	consts.SetView(view_loc);
	consts.SetWorld(identity);
	consts.tint	= colour(1,0,0,1);

	float3		fog_dir1(zero);
	colour		fog_col1(zero);

	colour		diffuse_colour;
	float3		point_size	= float3{3.f / width, 3.f / height, width};

	float3		shadowlight_dir	= -consts.view.row(2).xyz;
	colour		shadowlight_col(0.75f);
	float		glossiness	= 10;


	AddShaderParameter("diffuse_colour",	diffuse_colour);
	AddShaderParameter("point_size",		point_size);
	AddShaderParameter("shadowlight_dir",	shadowlight_dir);
	AddShaderParameter("shadowlight_col",	shadowlight_col);
	AddShaderParameter("fog_dir1",			fog_dir1);
	AddShaderParameter("fog_col1",			fog_col1);
	AddShaderParameter("glossiness",		glossiness);

	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetBlendEnable(true);
	ctx.SetDepthTestEnable(false);

	float3		pos		= sign1(as_vec(position3(zero) / view_loc));
	float4x4	wvp		= consts.viewProj0;

#if 1
	diffuse_colour	= colour(1,0,0, .25f);
	Set(ctx, specular_grid, ISO::MakeBrowser(re.consts));
	DrawPlane(ctx, plane(x_axis), wvp);

	diffuse_colour	= colour(0,1,0, .25f);
	Set(ctx, specular_grid, ISO::MakeBrowser(re.consts));
	DrawPlane(ctx, plane(y_axis), wvp);
	
	diffuse_colour	= colour(0,0,1, .25f);
	Set(ctx, specular_grid, ISO::MakeBrowser(re.consts));
	DrawPlane(ctx, plane(z_axis), wvp);
#else
	ctx.SetZBuffer(Surface(TEXF_D24S8, Size(), MEM_DEPTH));
	ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
	ctx.SetDepthTestEnable(true);
	ctx.SetDepthTest(DT_USUAL);
//	ctx.SetDepthTestEnable(false);
	Set(ctx, space_grid, ISO::MakeBrowser(re.consts));
	{
		ImmediatePrims<QuadList<float2p> >	imm(ctx, 1);
		imm.begin() = unit_rect;
	}
#endif

	// solid shapes

	ctx.SetBlend(BLENDOP_ADD, BLEND_SRC_COLOR, BLEND_INV_SRC_ALPHA);
	ctx.SetBackFaceCull(BFC_BACK);
	ctx.SetDepthTestEnable(true);

	range<Shape3**>			occluders, includers;
	dynamic_array<Shape3*>	shapes2;

	if (mode == MODE_OCCLUSION) {
		shapes2						= shapes;
		auto	*occluders_begin	= partition(shapes2, [](Shape3 *p) { return p->mode != Shape3::normal; });
		auto	*includers_begin	= partition(occluders_begin, shapes2.end(), [](Shape3 *p) { return p->mode == Shape3::includer; });

		occluders			= make_range(occluders_begin, includers_begin);
		includers			= make_range(includers_begin, shapes2.end());

	} else if (mode == MODE_COLLISION) {
		for (auto& i : shapes) {
			position3	closestA, closestB;
			float3		normal;
			float		distance;
			bool		hit	= false;

			for (auto& j : slice(shapes, shapes.index_of(i) + 1)) {
				simplex_difference<3>	simp;
				if (hit = (simp.gjk(i->support(), j->support(), zero, 0, 0, 0, closestA, closestB, distance) && distance < 1e-6f)) {
					break;
				}
			}
			i->hit	= hit;
		}
	}

	static const colour	fills[] = {
		colour(0,0,1,.75f),
		colour(1,0,0,.75f),
		colour(0,1,0,.75f),
		colour(.5f,.5f,.5f,.75f),
	};

	for (auto &i : shapes) {
		int		smode = i->mode;
		switch (mode) {
			case MODE_OCCLUSION:
				if (smode == Shape3::normal)
					smode = test_occlusion(i, includers, occluders) ? 0 : 3;
				break;
			case MODE_COLLISION:
				smode = i->hit ? 1 : 0;
				break;
			default:
				smode = i->is_within(identity)	? 1
					:	i->is_visible(identity)	? 2
					:	0;
				break;
		}

		consts.tint	=	fills[smode];

		consts.SetProj(GetProjMatrix());
		consts.SetView(view_loc);
		consts.SetWorld(identity);

		point_size		= float3{8.f / width, 8.f / height, width};
		i->Draw(&re, specular);

		consts.tint	=	colour(0,0,0,.25f);
		switch (mode) {
			case MODE_BOX:
				Draw(&re, i->get_box(), blend);
				break;

			case MODE_SPHERE:
				Draw(&re, i->circumscribe(), blend);
				break;

			case MODE_INSPHERE:
				Draw(&re, i->inscribe(), blend);
				break;

			case MODE_CLOSEST: {
				position3	p0 = MouseRay() & plane(z_axis);
				position3	p1 = i->closest(p0);
				point_size		= float3{8.f / width, 8.f / height, width};
				consts.SetWorld(identity);
				Set(ctx, thicklineA, ISO::MakeBrowser(re.consts));
				ImmediateStream<float3p>	im(ctx, AdjacencyPrim(PRIM_LINESTRIP), 4);
				float3p	*out = im.begin();
				out[0] = p0;
				out[1] = p0;
				out[2] = p1;
				out[3] = p1;
			}
			case MODE_SUPPORT: {
				position3	p0 = MouseRay() & plane(z_axis);
				vector3		v{float(mouse.x - width / 2), float(mouse.y - height / 2), 0};
				v				= normalise(view_loc * v);
				position3	p1	= i->support(v);
				point_size		= float3{8.f / width, 8.f / height, width};
				consts.SetWorld(identity);
#if 1
				Set(ctx, thicklineA, ISO::MakeBrowser(re.consts));
				ImmediateStream<float3p>	im(ctx, AdjacencyPrim(PRIM_LINESTRIP), 4);
				float3p	*out = im.begin();
				out[0] = p0;
				out[1] = p0;
				out[2] = p1;
				out[3] = p1;
#else
				Set(ctx, thickpoint, ISO::MakeBrowser(re.consts));
				ImmediateStream<float3p>	im(ctx, PRIM_POINTLIST, 1);
				*im.begin() = p1;
#endif
				break;
			}
			case MODE_RAY_CLOSEST: {
				position3	p0	= MouseRay() & plane(z_axis);
				ray3		r(ray_start, p0);
				float		t	= i->ray_closest(r);
				position3	p1	= r.from_parametric(t);

				point_size		= float3{8.f / width, 8.f / height, width};
				consts.SetWorld(identity);

				{
					Set(ctx, thicklineA, ISO::MakeBrowser(re.consts));
					ImmediateStream<float3p>	im(ctx, AdjacencyPrim(PRIM_LINESTRIP), 4);
					float3p	*out = im.begin();
					out[0] = ray_start;
					out[1] = ray_start;
					out[2] = p0;
					out[3] = p0;
				}

				{
					Set(ctx, thickpoint, ISO::MakeBrowser(re.consts));
					ImmediateStream<float3p>	im(ctx, PRIM_POINTLIST, 1);
					*im.begin() = p1;
				}
				break;
			}
		}
	}

	if (over >= 0) {
		ctx.SetFillMode(FILL_WIREFRAME);
		consts.SetProj(GetProjMatrix());
		consts.SetView(view_loc);
		consts.SetWorld(identity);
		consts.tint	= colour(0,0,0);
		shapes[over]->Draw(&re, blend);
		ctx.SetFillMode(FILL_SOLID);
	}

	// points & generated shapes

	if (p) {
		ctx.SetDepthTestEnable(false);
		temp_points3	pts(*p);
		consts.tint	= colour(0,0,0, .5f);

		switch (mode) {
			case MODE_POINTS: {
				//point_size		= float3{8.f / width, 8.f / height, width};
				//Set(ctx, thickpoint, ISO::MakeBrowser(re.consts));
				//ImmediateStream<float3p>	im(ctx, PRIM_LINESTRIP, pts.size());
				//float3p	*out = im.begin();
				//copy_n(pts.begin(), out, pts.size());
				break;
			}
			case MODE_LINES: {
				point_size		= float3{8.f / width, 8.f / height, width};
				Set(ctx, thicklineA, ISO::MakeBrowser(re.consts));
				ImmediateStream<float3p>	im(ctx, AdjacencyPrim(PRIM_LINESTRIP), pts.size() + 2);
				float3p	*out = im.begin();
				out[0] = pts[0];
				copy_n(pts.begin(), out + 1, pts.size());
				out[pts.size() + 1] = pts[pts.size() - 1];
				break;
			}
			case MODE_HULL: {
				int		max_indices	= ref_polyhedron::max_indices(pts.size());
				uint16	*i			= alloc_auto(uint16, max_indices);
				int		n			= generate_hull_3d_partial(pts.begin(), pts.size(), i, step == 0 ? pts.size() : min(step - 1, pts.size()));

				ref_polyhedron	hull(make_indexed_container(ref_array_size<position3>(pts), ref_array_size<uint16>(make_range_n(i, n))));
				ISO_ASSERT(hull.points().size() <= max_indices);
				Set(ctx, blend, ISO::MakeBrowser(re.consts));
				Fill(ctx, hull);

				Set(ctx, thickline, ISO::MakeBrowser(re.consts));
				Line(ctx, hull);
				break;
			}
			case MODE_SPHERE: {
				sphere		s	= pts.size() == 4
					? tetrahedron(pts.begin()).circumscribe()
					: pts.cloud().circumscribe();

				consts.SetWorld(s.matrix());
				ctx.SetBackFaceCull(BFC_BACK);
				Set(ctx, specular, ISO::MakeBrowser(re.consts));
				SphereVB::get<64>().Render(ctx);

				//conic		c	= (vp * quadric(s)).project_z();
				conic		c	= ((consts.viewProj0 * s.matrix()) * quadric::standard_sphere()).project_z();
				if (c.analyse().type == conic::ELLIPSE) {
					ellipse		e	= c;
					wvp	= float4x4(float3x4(e.matrix()));
				#if 0
					Set(ctx, thicklineA, ISO::MakeBrowser(re.consts));
					CircleVB::get<8>().RenderOutlineA(ctx);
				#else
					//point_size		= float3(8.f / width, 8.f / height, width);
					consts.tint	= colour(0,0,0, 1);
					Set(ctx, circletess, ISO::MakeBrowser(re.consts));
					ImmediateStream<float4p>	im(ctx, PatchPrim(1), 1);
					im.begin()[0] = w_axis;
				#endif
				}
				break;
			}
			case MODE_INSPHERE: {
				sphere		s = pts.size() == 4
					? tetrahedron(pts.begin()).inscribe()
					: halfspace_intersection3(pts.cloud(), alloc_auto(plane, halfspace_intersection3::max_planes(pts.size()))).inscribe();

				consts.SetWorld(s.matrix());
				ctx.SetBackFaceCull(BFC_BACK);
				Set(ctx, blend, ISO::MakeBrowser(re.consts));
				SphereVB::get<64>().Render(ctx);
				
				auto	hull = get_hull3d(pts.cloud());
				//int	maxi = ref_polyhedron::max_indices(pts.size());
				//ref_polyhedron	hull(make_indexed_container(ref_array_size<position3>(pts), ref_array_size<uint16>(maxi)));
				consts.tint	= colour(0,0,0, 0.25f);
				consts.SetWorld(identity);
				//wvp				= vp;
				Set(ctx, blend, ISO::MakeBrowser(re.consts));
				Fill(ctx, hull);

				Set(ctx, thickline, ISO::MakeBrowser(re.consts));
				Line(ctx, hull);
				break;
			}

			case MODE_ELLIPSOID: {
				ellipsoid	e	= pts.cloud().get_ellipse();

				consts.SetWorld(e.matrix());
				ctx.SetBackFaceCull(BFC_BACK);
				Set(ctx, specular, ISO::MakeBrowser(re.consts));
				SphereVB::get<64>().Render(ctx);

				conic		c	= (consts.viewProj0 * quadric(e)).project_z();
				if (c.analyse().type == conic::ELLIPSE) {
					ellipse		e2	= c;
					wvp	= float4x4(float3x4(e2.matrix()));
#if 0
					Set(ctx, thicklineA, ISO::MakeBrowser(re.consts));
					CircleVB::get<8>().RenderOutlineA(ctx);
#else
					//point_size		= float3(8.f / width, 8.f / height, width);
					consts.tint	= colour(0,0,0, 1);
					Set(ctx, circletess, ISO::MakeBrowser(re.consts));
					ImmediateStream<float4>	im(ctx, PatchPrim(1), 1);
					im.begin()[0] = w_axis;
#endif
				}
				break;
			}

			case MODE_BOX: {
				cuboid		c	= pts.cloud().get_box();
				consts.SetWorld(c.matrix());
				Set(ctx, blend, ISO::MakeBrowser(re.consts));
				BoxVB::get().Render(ctx);
				break;
			}
			case MODE_HULLDUAL: {
				halfspace_intersection3	h(pts.cloud(), alloc_auto(plane, halfspace_intersection3::max_planes(pts.size())));
				auto		points		= h.points(alloc_auto(position3, h.max_verts()));
				int			max_indices	= ref_polyhedron::max_indices(points.points().size32());
				uint16		*i			= alloc_auto(uint16, max_indices);
				ref_polyhedron	hull(make_indexed_container(ref_array_size<position3>(points.points()), ref_array_size<uint16>(make_range_n(i, max_indices))));
				ISO_ASSERT(hull.points().size() <= max_indices);
				Set(ctx, blend, ISO::MakeBrowser(re.consts));
				Fill(ctx, hull);

				Set(ctx, thickline, ISO::MakeBrowser(re.consts));
				Line(ctx, hull);
				break;
			}
		}

		point_size	= float3{8.f / width, 8.f / height, width};
		consts.tint	= colour(0,0,0);
		consts.SetWorld(identity);
		//wvp				= vp;
		Set(ctx, thickpoint, ISO::MakeBrowser(re.consts));
		copy(pts, ImmediateStream<float3p>(ctx, PRIM_POINTLIST, pts.size()));
		if (highlighted >= 0) {
			consts.tint	= colour(1,0,0);
			Set(ctx, thickpoint, ISO::MakeBrowser(re.consts));
			copy_n(*p + highlighted, ImmediateStream<float3p>(ctx, PRIM_POINTLIST, 1).begin(), 1);
		}

		if (numbers) {
			DXTarget d2d(ctx.GetRenderTarget());
			if (d2d) {
				d2d::SolidBrush	black(d2d,	colour(0,0,0));
				d2d::SolidBrush	red(d2d,	colour(1,0,0));
				d2d::SolidBrush	green(d2d,	colour(0,1,0));

				d2d::Write	write;
				d2d::Font	font(write, L"arial", 16);
				d2d.SetTransform(identity);

				float4x4	transform = ViewportMatrix() * consts.viewProj0;

				d2d.BeginDraw();

				for (int i = 0; i < pts.size(); i++) {
					position3	x		= transform * position3((*p)[i]);
					float2		offset	= sincos(i) * 20 - float2(10);
					position2	x2		= position2(x.v.xy + offset);
					d2d.DrawText(rectangle(x2, x2 + 100), str16(to_string(i)), font, i == highlighted ? red : numbers == NUM_HULL ? green : black);
				}
				d2d.EndDraw();
			}
		}
	}


#if 0
	dynamic_array<float3p>	hits;
	for (int y = 0; y < height; y += 16) {
		for (int x = 0; x < width; x += 16) {
			ray3	r = MouseRay(Point(x, y));
			for (auto &i : shapes) {
				float	t;
				vector3	normal;
				if (i->ray_check(r, t, &normal)) {
					hits.push_back((const float3p&)r.from_parametric(t));
					break;
				}
			}
		}
	}
	point_size	= float3(8.f / width, 8.f / height, width);
	consts.tint	= colour(0,0,0);
	consts.SetWorld(identity);
	Set(ctx, thickpoint, ISO::MakeBrowser(re.consts));
	copy(hits, ImmediateStream<float3p>(ctx, PRIM_POINTLIST, hits.size()));
#endif

	ctx.SetWindow(Rect(0, height - 100, 100, 100));
	consts.SetViewProj(parallel_projection(0.7f, 0.7f, -10.f, 10.f));
	shadowlight_dir		= float3{0, 0, -1};
	DrawAxes(ctx, .05f, .2f, .2f, 0.25f, specular, (float3x4)float3x3(~view_loc.rot), ISO::MakeBrowser(consts));

}

Shape3 *ViewTest3D::AddShape(const ISO_ptr<void> &p) {
	for (auto &i : shape_types) {
		if (p.IsType(i.type, ISO::MATCH_NOUSERRECURSE))
			return shapes.push_back(i.make(p));
	}
	return nullptr;
}


ViewTest3D::ViewTest3D(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &_p)
	: main(main)
	, view_loc(identity, position3(0,0,2))
	, mode(MODE_POINTS), numbers(NUM_OFF), selected_index(-1), highlighted(-1), over(-1), step(0), perspective(true)
	, currentop(NULL)
{
	Create(wpos, NULL, CHILD | CLIPCHILDREN | VISIBLE, CLIENTEDGE);

	if (Shape3 *s = AddShape(_p)) {
		FitExtent(s->get_box());
		return;
	}

	if (p = ISO_conversion::convert<ISO_openarray<float3p> >(_p)) {
		cuboid	r(get_extent(*p));
		r |= r.a + select(r.extent() == zero, float3(one), float3(zero));
		FitExtent(r);
		return;
	}

	cuboid	r(none);
	for (auto i : ISO::Browser2(_p)) {
		if (Shape3 *s = AddShape(i))
			r |= s->get_box();
	}
	FitExtent(r);
}

class EditorTest3D : public Editor {
	virtual bool Matches(const ISO::Browser &b) {
		return ViewTest3D::supported_type(b);
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void, 64> &p) {
		return *new ViewTest3D(main, wpos, p);
	}
} editortest3d;
