#include "common.fxh"
#include "lighting.fxh"

//-----------------------------------------------------------------------------
//	triangular matrices
//-----------------------------------------------------------------------------

float4x4 translation(float3 t) {
	return float4x4(
		float4(1,0,0,0),
		float4(0,1,0,0),
		float4(0,0,1,0),
		float4(t, 1)
	);
}

struct lower3 {
	float3	d;
	float3	o;
};
lower3 make_lower3(float3 d, float3 o) {
	lower3	m;
	m.d	= d;
	m.o	= o;
	return m;
}
lower3	adj(lower3 m)	{
	return make_lower3(m.d.yzx * m.d.zxy, float3(0, 0, m.o.x * m.o.y) - m.o.xyz * m.d.zxy);
}
float3x3 lower3_to_float3x3(lower3 m) {
	return float3x3(
		m.d.x, m.o.xz, 
		0, m.d.y, m.o.y,
		0, 0, m.d.z
	);
}
lower3 to_lower3(float3x3 m) {
	return make_lower3(m._m00_m11_m22, m._m10_m21_m20);
}

struct symmetrical3 {
	float3	d3;
	float3	o;
};
symmetrical3 make_symmetrical3(float3 d3, float3 o) {
	symmetrical3	s;
	s.d3	= d3;
	s.o		= o;
	return s;
}
float3x3 symmetrical3_to_float3x3(symmetrical3 s) {
	return float3x3(
		s.d3.x,	s.o.xz,
		s.o.x,	s.d3.y,	s.o.y,
		s.o.zy,	s.d3.z
	);
}
symmetrical3 to_symmetrical3(float3x3 m) {
	return make_symmetrical3(m._m00_m11_m22, m._m10_m21_m20);
}
symmetrical3 apply(float3x3 m, symmetrical3 q) {
	return to_symmetrical3(mul(transpose(m), mul(symmetrical3_to_float3x3(q), m)));
}
float	det(symmetrical3 s)		{ return s.d3.x * s.d3.y * s.d3.z + s.o.x * s.o.y * s.o.z * 2 - dot(s.d3.zxy, s.o * s.o); }
float	det2x2(symmetrical3 s)	{ return s.d3.x * s.d3.y - s.o.x * s.o.x; }
float3	diagonal(float3x3 m)	{ return m._m00_m11_m22; }

struct symmetrical4 {
	float4	d4;
	float3	d3;
	float3	o;
};
symmetrical4 make_symmetrical4(float4 d4, float3 d3, float3 o) {
	symmetrical4	s;
	s.d4	= d4;
	s.d3	= d3;
	s.o		= o;
	return s;
}
float4x4 to_float4x4(symmetrical4 s) {
	return float4x4(
		s.d4.x,	s.d3.x,	s.o.xz,
		s.d3.x,	s.d4.y,	s.d3.y,	s.o.y,
		s.o.x,	s.d3.y,	s.d4.z,	s.d3.z,
		s.o.zy,	s.d3.z,	s.d4.w
	);
}
float4x4 to_float4x4(float3x3 s) {
	return float4x4(
		s[0], 0,
		s[1], 0,
		s[2], 0,
		0, 0, 0, 1
	);
}

symmetrical4 to_symmetrical4(float4x4 m) {
	return make_symmetrical4(m._m00_m11_m22_m33, m._m10_m21_m32, m._m20_m31_m30);
}
symmetrical4 apply(float4x4 m, symmetrical4 q) {
	return to_symmetrical4(mul(transpose(m), mul(to_float4x4(q), m)));
}
float det(symmetrical4 s) {
	return det(to_float4x4(s));
}
symmetrical3 strip_x(symmetrical4 s) {
	return make_symmetrical3(s.d4.yzw, float3(s.d3.yz, s.o.y));
}
symmetrical3 strip_y(symmetrical4 s) {
	return make_symmetrical3(s.d4.xzw, float3(s.o.x, s.d3.z, s.o.z));
}
symmetrical3 strip_z(symmetrical4 s) {
	return make_symmetrical3(s.d4.xyw, float3(s.d3.x, s.o.yz));
}
symmetrical3 strip_w(symmetrical4 s) {
	return make_symmetrical3(s.d4.xyz, float3(s.d3.xy, s.o.x));
}

float		conic_evaluate(symmetrical3 s, float2 p)	{ return dot(p * p, s.d3.xy)	+ 2 * (dot(p, s.o.zy)	+ s.o.x * p.x * p.y)	+ s.d3.z; }
float2		conic_centre(symmetrical3 s)				{ return project(cofactors(symmetrical3_to_float3x3(s))[2]); }
float3x2	conic_tangents(symmetrical3 s) {
	float2	v	= sqrt(det(s) / -s.d3.yx) / det2x2(s);
	return float3x2(float2(s.d3.y, -s.o.x) * v.x, float2(-s.o.x, s.d3.x) * v.y, conic_centre(s));
}

float		quadric_evaluate(symmetrical4 q, float3 p)	{ return dot(p * p, q.d4.xyz)	+ 2 * (dot(p, float3(q.o.zy, q.d3.z))			+ dot(p.xyz * p.yzx, float3(q.d3.xy, q.o.x)))	+ q.d4.w; }
float		quadric_evaluate(symmetrical4 q, float4 p)	{ return dot(p * p, q.d4)		+ 2 * (dot(p.xyz * p.w, float3(q.o.zy, q.d3.z)) + dot(p.xyz * p.yzx, float3(q.d3.xy, q.o.x))); }
float3		quadric_centre(symmetrical4 q)				{ return project(cofactors(to_float4x4(q))[3]); }	
float4x3	quadric_tangents(symmetrical4 q) {
	float3x3	s3	= symmetrical3_to_float3x3(strip_w(q));
	float3x3	co	= cofactors(s3);
	float3		v	= sqrt(det(q) / -diagonal(co)) / det(s3);
	return float4x3(
		co[0] * v.x,
		co[1] * v.y,
		co[2] * v.z,
		quadric_centre(q)
	);
}

symmetrical3 quadric_project_z(symmetrical4 q) {
	float3	z	= float3(q.o.x, q.d3.yz);
	return make_symmetrical3(q.d4.xyw * q.d4.z - square(z), float3(q.d3.x, q.o.yz) * q.d4.z - float3(q.d3.yz, q.o.x) * z);
}

/*

//-----------------------------------------------------------------------------
//	ellipse
//-----------------------------------------------------------------------------

Ellipse conic_to_ellipse(symmetrical3 s) {
	float2	c		= conic_centre(s);
	float	k		= -conic_evaluate(s, c);
	float3	m		= float3(s.d3.xy, s.o.x) / k;
	float	d		= sqrt(square(m.x - m.y) + square(m.z) * 4);
	float2	e		= (float2(1, -1) * d + m.x + m.y) / 2;
	float2	major	= normalise(m.y > m.x ? float2(e.y - m.y, m.z) : float2(m.z, e.y - m.x));
	
	Ellipse	ellipse;
	ellipse.v		= float4(c, major * rsqrt(e.y));
	ellipse.ratio	= sqrt(e.y / e.x);
	return ellipse;
}

float3x2 conic_to_shear_ellipse(symmetrical3 s) {
	float	d1	= det(s);
	float	d2	= det2x2(s);
	return float3x2(
		sqrt(-d1 / (s.d3.x * d2)), 0,
		float2(-s.o.x, s.d3.x) * sqrt(-d1 / s.d3.x) / d2,
		conic_centre(s)
	);
}

Ellipse VS_quadric(symmetrical4 q : POSITION_IN) {
	return conic_to_ellipse(quadric_project_z(apply(cofactors(worldViewProj), q)));
}

technique quadric_ellipse {
	pass p0 {
		SET_VS(VS_quadric);
		SET_GS(GS_ellipse);
		SET_PS(PS_thickpoint);
	}
}
*/

//-----------------------------------------------------------------------------
//	quadric
//-----------------------------------------------------------------------------

struct ShearQuadric {
	float3	centre	: SHEAR0;
	lower3	m		: SHEAR1;
};

struct QuadricVert {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	ShearQuadric s;
};

ShearQuadric VS_quadric(symmetrical4 q : POSITION_IN) {
#if 0
	float4x4	fix = float4x4(
		1,0,0,0,
		0,-1,0,0,
		0,0,-0.5,0.5,
		0,0,0,1
	);
	float4x4	co	= cofactors(mul(fix, worldViewProj));
#else
	float4x4	co	= cofactors(worldViewProj);
	float		det	= dot(worldViewProj[0], co[0]);
	co	=	co * rsqrt(det);
#endif
	q				= apply(co, q);
	float4x3	qt	= quadric_tangents(q);

	symmetrical3 slice	= strip_x(apply(transpose(translation(qt[3])), q));

	float3x2	qc		= conic_tangents(slice);
	float		zscale	= sqrt(square(slice.o.y) - slice.d3.y * slice.d3.z) / slice.d3.y;

	ShearQuadric	eq;

	eq.centre	= qt[3];
	eq.m		= make_lower3(float3(qt[0].x, qc[0].x, zscale), float3(qt[0].y, qc[0].y, qt[0].z));

//	eq.centre	= float3(0,0,0);
//	eq.m		= make_lower3(float3(qt[0].x, 0, 0), float3(0,0,0));

	return eq;
}


QuadricVert quadric_vert(ShearQuadric s, float2 centre, float2 axis1, float2 axis2, float u, float v) {
	QuadricVert	qv;
	qv.uv		= float2(u, v);
	qv.pos		= float4(centre + axis1 * u + axis2 * v, 0, 1);
	qv.s		= s;
	return qv;
}

GEOM_SHADER(4)
void GS_quadric(point ShearQuadric eq[1], inout TriangleStream<QuadricVert> tris) {
	float2	centre	= eq[0].centre.xy;
	float2	axis1	= float2(eq[0].m.d.x, eq[0].m.o.x);
	float2	axis2	= float2(0, eq[0].m.d.y);

	tris.Append(quadric_vert(eq[0], centre, axis1, axis2, -1, -1));
	tris.Append(quadric_vert(eq[0], centre, axis1, axis2, +1, -1));
	tris.Append(quadric_vert(eq[0], centre, axis1, axis2, -1, +1));
	tris.Append(quadric_vert(eq[0], centre, axis1, axis2, +1, +1));
}

float4 PS_quadric(QuadricVert p) : OUTPUT0 {
	float	z2	= dot(p.uv, p.uv);
	if (z2 > 1)
		return 0;

	float3	pos		= float3(p.uv, sqrt(1 - z2));
	
//	float3	vpos	= p.s.centre + mul(pos, lower3_to_float3x3(p.s.m));
//	float3	wpos	= project(mul(cofactors(worldViewProj), float4(vpos, 1.0)));
//	return float4((wpos + 1) / 2, 1);

	float3	adjd	= p.s.m.d.yzx * p.s.m.d.zxy;
	float3	adjo	= float3(0, 0, p.s.m.o.x * p.s.m.o.y) - p.s.m.o.xyz * p.s.m.d.zxy;

	float4x4	nmat0	= float4x4(
		adjd.x, adjo.xz, 0,
		0, adjd.y, adjo.y, 0,
		0, 0, adjd.z, 0,
		-p.s.centre, 1
	);

//	float4x4	nmat	= mul(worldViewProj, nmat0);
//	float4x4	nmat	= mul(mul(worldViewProj, translation(-p.s.centre)), to_float4x4(lower3_to_float3x3(adj(p.s.m))));
	float4x4	nmat	= mul(worldViewProj, mul(translation(-p.s.centre), to_float4x4(lower3_to_float3x3(adj(p.s.m)))));
	float3		norm	= normalise(mul(float4(pos, 1), nmat).xyz);
	return float4((norm + 1) / 2, 1);
}


technique quadric {
	pass p0 {
		SET_VS(VS_quadric);
		SET_GS(GS_quadric);
		SET_PS(PS_quadric);
	}
}

//-----------------------------------------------------------------------------
//	quadric with tesselation of tripatch
//-----------------------------------------------------------------------------

struct QuadricPatchConsts {
	float	edges[3]	: TESS_FACTOR;
	float	inside		: INSIDE_TESS_FACTOR;
	float	bulge		: BULGE;
};


// patch Constant Function
QuadricPatchConsts HS_quadricC(InputPatch<ShearQuadric, 1> patch) {
	QuadricPatchConsts	consts;
	float	tess	= 16;

	consts.edges[0]	= tess;
	consts.edges[1]	= tess;
	consts.edges[2]	= tess;
	consts.inside	= tess;
	consts.bulge	= 1 / cos(pi / tess);
	return consts;
}

// hull shader
HULL_SHADER(HS_quadricC, tri, integer, triangle_ccw, 1)
ShearQuadric HS_quadric(InputPatch<ShearQuadric, 1> patch) {
	return patch[0];
}

// domain shader
DOMAIN_SHADER(tri)
QuadricVert DS_quadric(QuadricPatchConsts consts, float3 u : DOMAIN_LOCATION, OutputPatch<ShearQuadric, 1> patch) {
	QuadricVert	qv;

	float	r		= u.x + u.y;
	float	a		= r ? u.x / r : 0;

	float2	sc;
	sincos(a * 2 * pi, sc.y, sc.x);
	sc	*= r * consts.bulge;

	float2	centre	= patch[0].centre.xy;
	float2	axis1	= float2(patch[0].m.d.x, patch[0].m.o.x);
	float2	axis2	= float2(0, patch[0].m.d.y);

	qv.uv		= sc;
	qv.pos		= float4(centre + axis1 * sc.x + axis2 * sc.y, 0, 1);
	qv.s		= patch[0];
	return qv;
}

//-----------------------------------------------------------------------------
//	quadric with tesselation of quadpatch
//-----------------------------------------------------------------------------

struct QuadricPatchConsts4 {
	float	edges[4]	: TESS_FACTOR;
	float	inside[2]	: INSIDE_TESS_FACTOR;
	float	bulge		: BULGE;
};

// patch Constant Function
QuadricPatchConsts4 HS_quadric4C(InputPatch<ShearQuadric, 1> patch) {
	QuadricPatchConsts4	consts;
	float	tess	= 16;

	consts.edges[0]		= tess;
	consts.edges[1]		= tess;
	consts.edges[2]		= tess;
	consts.edges[3]		= tess;
	consts.inside[0]	= tess;
	consts.inside[1]	= tess;
	consts.bulge	= 1 / cos(pi / tess);
	return consts;
}

// hull shader
HULL_SHADER(HS_quadric4C, quad, integer, triangle_ccw, 1)
ShearQuadric HS_quadric4(InputPatch<ShearQuadric, 1> patch) {
	return patch[0];
}

// domain shader
DOMAIN_SHADER(quad)
QuadricVert DS_quadric4(QuadricPatchConsts4 consts, float2 u : DOMAIN_LOCATION, OutputPatch<ShearQuadric, 1> patch) {
	QuadricVert	qv;

	float	r	= sqrt(u.y);
	float2	sc;
	sincos(u.x * 2 * pi, sc.y, sc.x);

	sc	*= r * consts.bulge;

	float2	centre	= patch[0].centre.xy;
	float2	axis1	= float2(patch[0].m.d.x, patch[0].m.o.x);
	float2	axis2	= float2(0, patch[0].m.d.y);

	qv.uv		= sc;
	qv.pos		= float4(centre + axis1 * sc.x + axis2 * sc.y, 0, 1);
	qv.s		= patch[0];
	return qv;
}


technique quadric_tess4 {
	pass p0 {
		SET_VS(VS_quadric);
		SET_HS(HS_quadric4);
		SET_DS(DS_quadric4);
		SET_PS(PS_quadric);
	}
}
