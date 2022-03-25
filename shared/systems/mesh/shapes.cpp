#include "shapes.h"
#include "shape_gen.h"
#include "render.h"
#include "base/algorithm.h"
#include "extra/gpu_helpers.h"

namespace iso {

template<> const VertexIndexBuffer &GetVIB<cuboid>()		{ return BoxVB::get(); }
template<> const VertexIndexBuffer &GetVIB<sphere>()		{ return SphereVB::get<16>(); }
template<> const VertexIndexBuffer &GetVIB<cylinder>()		{ return CylinderVB::get<16>(); }
template<> const VertexIndexBuffer &GetVIB<cone>()			{ return ConeVB::get<16>(); }
template<> const VertexIndexBuffer &GetVIB<circle>()		{ return CircleVB::get<16>(); }

template<> const VertexIndexBuffer &GetVIB<obb3>()			{ return BoxVB::get(); }
template<> const VertexIndexBuffer &GetVIB<circle3>()		{ return CircleVB::get<16>(); }
template<> const VertexIndexBuffer &GetVIB<ellipse>()		{ return CircleVB::get<16>(); }
template<> const VertexIndexBuffer &GetVIB<ellipsoid>()		{ return SphereVB::get<16>(); }
template<> const VertexIndexBuffer &GetVIB<tetrahedron>()	{ return TetrahedronVB::get(); }

template<> const VertexIndexBuffer &GetVIB<rectangle>()		{ return RectangleVB::get(); }

//-----------------------------------------------------------------------------
// Shapes
//-----------------------------------------------------------------------------

void DrawProj(RenderEvent *re, pass *t, const VertexIndexBuffer &vib, param(float4x4) matrix) {
	re->consts.SetWorldViewProj(matrix);
	Set(re->ctx, t, ISO::MakeBrowser(re->consts));
	vib.Render(re->ctx);
}

void Draw(RenderEvent *re, pass *t, const VertexIndexBuffer &vib, param(float3x4) matrix) {
	re->consts.SetWorld(matrix);
	Set(re->ctx, t, ISO::MakeBrowser(re->consts));
	vib.Render(re->ctx);
}

void Draw(RenderEvent *re, param(capsule) s, pass *t, param(float3x4) matrix) {
	static SphereVB		vib_sphere(16, 1, true);
	static CylinderVB	vib_cylinder(16, 1, 1, false);
	float3x3	z		= look_along_z(s.dir());
	Draw(re, t, vib_cylinder, matrix * translate(s.centre()) * z * scale(concat(float2(s.radius()), len(s.dir()))));
	Draw(re, t, vib_sphere, matrix * translate(s.centre() - s.dir()) * z * scale(s.radius()));
	z.z	= -z.z;
	z.x	= -z.x;
	Draw(re, t, vib_sphere, matrix * translate(s.centre() + s.dir()) * z * scale(s.radius()));
}

//-----------------------------------------------------------------------------
// Grid
//-----------------------------------------------------------------------------

void MakeGridVerts(stride_iterator<float2p> p, int nx, int ny, float sx, float sy) {
	for (int y = 0; y <= ny; y++) {
		float	fy = y * sy;
		for (int x = 0; x <= nx; x++)
			*p++ = float2{x * sx, fy};
	}
}

void MakeGridIndices(uint16 *i, int nx, int ny) {
	for (int y = 0; y < ny; y++) {
		int	r0 = (y + 0) * (nx + 1);
		int	r1 = (y + 1) * (nx + 1);
		int	r2 = (y + 2) * (nx + 1);

		for (int x = 0; x <= nx; x++) {
			*i++ = r0 + x;
			*i++ = r1 + x;
		}
		*i++ = r1 + nx;

		if (++y < ny) {
			for (int x = nx; x >= 0; x--) {
				*i++ = r1 + x;
				*i++ = r2 + x;
			}
			*i++ = r2;
		}
	}
}

void MakeGrid(stride_iterator<float2p> p, uint16 *i, int nx, int ny, float sx, float sy) {
	MakeGridVerts(p, nx, ny, sx, sy);
	MakeGridIndices(i, nx, ny);
}

void DrawGrid(RenderEvent *re, int nx, int ny, pass *t, param(float3x4) matrix) {
	if (_Grid *g = _Grid::Get(nx, ny)) {
		re->consts.SetWorld(matrix);
		Set(re->ctx, t, ISO::MakeBrowser(re->consts));
		g->Render(re->ctx);
	}
}

void DrawGrid(GraphicsContext &ctx, int nx, int ny) {
	if (_Grid *g = _Grid::Get(nx, ny))
		g->Render(ctx);
}

void DrawWireFrameGrid(GraphicsContext &ctx, int nx, int ny) {
	ImmediateStream<float2p>	imm(ctx, PRIM_LINELIST, (nx + ny + 2) * 2);
	float2p	*p = imm.begin();

	float	sx = 2.f / nx;
	for (int x = 0; x <= nx; x++) {
		*p++ = float2{x * sx - 1, -1};
		*p++ = float2{x * sx - 1, +1};
	}

	float	sy = 2.f / ny;
	for (int y = 0; y <= ny; y++) {
		*p++ = float2{-1, y * sy - 1};
		*p++ = float2{+1, y * sy - 1};
	}
}

void DrawWireFrameBox(GraphicsContext& ctx) {
	ImmediateStream<float3p>	ims(ctx, PRIM_LINELIST, 24);
	float3p	*p	= ims.begin();
	for (int i = 0; i < 12; i++, p += 2) {
		int	ix = i / 4, iy = (ix + 1) % 3, iz = (ix + 2) % 3;
		p[0][ix] = p[1][ix] = (i & 1) * 2 - 1;
		p[0][iy] = p[1][iy] = (i & 2) - 1;
		p[0][iz] = -1;
		p[1][iz] = +1;
	}
}

void DrawWireFrameRect(GraphicsContext &ctx) {
	ImmediateStream<float3p>	ims(ctx, PRIM_LINESTRIP, 5);
	float3p	*p	= ims.begin();
	p[0] = p[4] = float3{-1, -1, 0};
	p[1] = float3{+1, -1, 0};
	p[2] = float3{+1, +1, 0};
	p[3] = float3{-1, +1, 0};
}



void DrawArrow(GraphicsContext &ctx, float axis_radius, float cone_radius, float cone_length, float axis_offset, pass *t, param(float3) dir, const ISO::Browser &params) {
	ctx.SetBackFaceCull(BFC_BACK);
//	ctx.SetFillMode(FILL_SOLID);

	float4x4	*world	= GetShaderParameter<crc32_const("world")>(params);

	*world	= float4x4(cylinder(position3(dir * axis_offset), dir, axis_radius).matrix());
	Set(ctx, t, params);
	CylinderVB::get<16>().Render(ctx);
	*world	= float4x4(cone(position3(dir * (axis_offset + 1)), dir, cone_radius).matrix());
	Set(ctx, t, params);
	ConeVB::get<16>().Render(ctx);
}

void DrawAxes(GraphicsContext &ctx, float axis_radius, float cone_radius, float cone_length, float axis_offset, pass *t, param(float3x4) mat, const ISO::Browser &params) {
	ctx.SetBackFaceCull(BFC_BACK);
//	ctx.SetFillMode(FILL_SOLID);

	colour		*col	= GetShaderParameter<"tint"_crc32>(params);
	float4x4	*world	= GetShaderParameter<"world"_crc32>(params);

	*col	= colour(1,0,0);
	*world	= float4x4(mat * cylinder(position3(axis_offset,0,0), x_axis, axis_radius).matrix());
	Set(ctx, t, params);
	CylinderVB::get<16>().Render(ctx);
	*world	= float4x4(mat * cone(position3(axis_offset + 1,0,0), float3{cone_length,0,0}, cone_radius).matrix());
	Set(ctx, t, params);
	ConeVB::get<16>().Render(ctx);

	*col	= colour(0,1,0);
	*world	= float4x4(mat * cylinder(position3(0,axis_offset,0), y_axis, axis_radius).matrix());
	Set(ctx, t, params);
	CylinderVB::get<16>().Render(ctx);
	*world	= float4x4(mat * cone(position3(0,axis_offset + 1,0), float3{0,cone_length,0}, cone_radius).matrix());
	Set(ctx, t, params);
	ConeVB::get<16>().Render(ctx);

	*col	= colour(0,0,1);
	*world	= float4x4(mat * cylinder(position3(0,0,axis_offset), z_axis, axis_radius).matrix());
	Set(ctx, t, params);
	CylinderVB::get<16>().Render(ctx);
	*world	= float4x4(mat * cone(position3(0,0,axis_offset + 1), float3{0,0,cone_length}, cone_radius).matrix());
	Set(ctx, t, params);
	ConeVB::get<16>().Render(ctx);
}

int GetAxesClick(const float4x4 &viewproj, param(float2) p) {
	return	len2(p - viewproj.x.xy) < .1f ? 1
		:	len2(p - viewproj.y.xy) < .1f ? 2
		:	len2(p - viewproj.z.xy) < .1f ? 3
		:	0;
}

quaternion GetAxesRot(int c) {
	switch (c) {
		case 1:		return rotate_in_y(-pi * half);
		case 2:		return rotate_in_x(pi * half);
		default:	return identity;
		case -1:	return rotate_in_y(pi * half);
		case -2:	return rotate_in_x(-pi * half);
		case -3:	return rotate_in_y(pi);
	}
}

float4 CalcHorizonH(const float4x4 &viewproj, float u) {
	float4	x = viewproj.x, y = viewproj.y;
	return (x * (u * y.w - y.x) + y * (x.x - u * x.w)) / (y.w * x.x - x.w * y.x);
}

float4 CalcHorizonV(const float4x4 &viewproj, float u) {
	float4	x = viewproj.x, y = viewproj.y;
	return (x * (u * y.w - y.y) + y * (x.y - u * x.w)) / (y.w * x.y - x.w * y.y);
}

quadrilateral XYPlaneQuad(param(float4x4) viewproj, float du) {

	float4	x	= viewproj.x, y = viewproj.y;
	float	rx	= (y.w * x.x - x.w * y.x);
	float	ry	= (y.w * x.y - x.w * y.y);

	if (rx * rx + ry * ry < 0.5f) {
		return quadrilateral(unit_rect);

	} else if (abs(rx) > abs(ry)) {
		float	x0	= abs(y.x * x.w) < abs(x.x * y.w) ? y.x / y.w : x.x / x.w;
		float	xl	= -1;
		float	xr	= +1;
		if (du) {
			float	offset	= mod(x0, du);
			xl	+= offset - du;
			xr	+= offset;
		}
		float	yl	= project(CalcHorizonH(viewproj, xl)).v.y;
		float	yr	= project(CalcHorizonH(viewproj, xr)).v.y;

		if (rx > 0) {
			return quadrilateral(
				position2(xl, yl),
				position2(xr, yr),
				position2(xl, one),
				position2(xr, one)
			);

		} else {
			return quadrilateral(
				position2(xr, yr),
				position2(xl, yl),
				position2(xr, -one),
				position2(xl, -one)
			);
		}

	} else {
		float	y0	= abs(y.y * x.w) < abs(x.y * y.w) ? y.y / y.w : x.y / x.w;
		float	yl	= -1;
		float	yr	= +1;
		if (du) {
			float	offset	= mod(y0, du);
			yl	+= offset - du;
			yr	+= offset;
		}
		float	xl	= project(CalcHorizonV(viewproj, yl)).v.x;
		float	xr	= project(CalcHorizonV(viewproj, yr)).v.x;

		if (ry > 0) {
			return quadrilateral(
				position2(xl, yl),
				position2(xr, yr),
				position2(-one, yl),
				position2(-one, yr)
			);
		} else {
			return quadrilateral(
				position2(xr, yr),
				position2(xl, yl),
				position2(one, yr),
				position2(one, yl)
			);
		}
	}
}

quadrilateral PlaneQuad(param(float4x4) viewproj, param(plane) p, float du) {
	return XYPlaneQuad(viewproj / from_xy_plane(p), du);
}

/*
void DrawGroundPlane(RenderEvent *re, float du) {
	quadrilateral quad = XYPlaneQuad(re->consts.viewProj0, du);
	ImmediatePrims<SingleQuad<float3p> >	im(re->ctx, 1);
	SingleQuad<float3p>	v = im.begin();

	float4x4	isolver = transpose(re->consts.viewProj0);
	isolver.z	= float4{0,0,-1,0};
	float4x4	solver = re->consts.viewProj0 * inverse(transpose(isolver));

	for (int i = 0; i < 4; i++) {
		float3	w = project(solver * concat(quad[i], zero, one));
		v[i] = concat(quad[i], w.z);
	}
}
*/

void DrawPlane(GraphicsContext &ctx, const plane &p, const float4x4 &wvp) {
	position3	verts[6];
	plane		p2	= wvp * p;
	uint32		nv	= clip_plane(p2, verts) - verts;

	if (nv > 2) {
		float3	n	= p.normal() * sign(p2.v.w);
		ImmediateStream<VertexIndexBuffer::vertex>	imm(ctx, PRIM_TRISTRIP, nv);
		convex_to_tristrip(imm.begin(), transformc(make_range_n(verts, nv), [&n](param(position3) i)->VertexIndexBuffer::vertex { return {i.v, n}; }));
	}
}

void DrawPlane(RenderEvent *re, param(plane) p) {
	DrawPlane(re->ctx, p, re->consts.viewProj0);
}


}//namespace iso
