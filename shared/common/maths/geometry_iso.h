#ifndef GEOMETRY_ISO_H
#define GEOMETRY_ISO_H

#include "vector_iso.h"
#include "maths/geometry.h"

namespace ISO {

struct circle {
	union { struct { float2p centre; float radius2; };	float4p _x; };
	~circle()	{}
};

struct ellipse {
	union { struct { float2p centre, major; };	float4p	_x; };
	float ratio;
	void	operator=(const iso::ellipse &e) { _x = e.v;  ratio = e.ratio; }
	~ellipse() {}
};

struct rectangle {
	union { struct { float2p min, max; };		float4p _x; };
	~rectangle() {}
};

struct shape2x3 { float2p x, y, z; };

struct quadrilateral {
	float2p a, b, c, d;
};


struct conic {
	union { float3p	d3;	float4p _y; };
	union { float3p	o;	float4p _z; };
	void	operator=(const iso::conic &q) { d3 = q.d; o = q.o; }
	~conic() {}
};


struct sphere {
	union { struct { float3p centre; float radius; }; float4p _x; };
	~sphere() {}
};

struct ellipsoid {
	union { float3p centre; float4p _x; };
	union { float3p axis1;	float4p _y; };
	union { struct { float3p axis2; float axis3_len; }; float4p _z; };
	~ellipsoid() {}
};

struct cuboid {
	union { float3p min;	float4p _x; };
	union { float3p max;	float4p _y; };
	~cuboid() {}
};

struct directed_shape {
	union { struct { float3p centre; float radius; }; float4p _x; };
	union { float3p dir;	float4p _y; };
	~directed_shape() {}
};

struct tetrahedron {
	union { float3p x;		float4p _x; };
	union { float3p y;		float4p _y; };
	union { float3p z;		float4p _z; };
	union { float3p centre;	float4p _w; };
	~tetrahedron() {}
};

struct __attribute__((packed)) quadric {
	float4p	d4;
	union { float3p	d3;	float4p _y; };
	union { float3p	o;	float4p _z; };
	void	operator=(const iso::quadric &q) { d4 = q.diagonal().d; d3 = q.diagonal<1>().d; o = q.o; }
	~quadric() {}
};

struct circle3 : circle {
	float4p	pl;
};

struct bezier2d {
	float2p	c0, c1, c2, c3;
};
struct bezier2d2 {
	float2p	c0, c1, c2;
};

struct bezier_chain2d {
	ISO_openarray<float2p>	c;
};
struct bezier_chain2d2 {
	ISO_openarray<float2p>	c;
};

struct bspline2d {
	ISO_openarray<float2p>	c;
	ISO_openarray<float>	k;
};
struct bspline2d2 {
	ISO_openarray<float2p>	c;
	ISO_openarray<float>	k;
};

struct nurbs2d {
	ISO_openarray<float3p>	c;
	ISO_openarray<float>	k;
};
struct nurbs2d2 {
	ISO_openarray<float3p>	c;
	ISO_openarray<float>	k;
};

} // namespace ISO

ISO_DEFCOMPV(ISO::circle, centre, radius2);
ISO_DEFCOMPV(ISO::ellipse, centre, major, ratio);
ISO_DEFCOMPV(ISO::rectangle, min, max);
ISO_DEFCOMPV(ISO::shape2x3, x, y, z);
ISO_DEFCOMPV(ISO::quadrilateral, a, b, c, d);
ISO_DEFCOMPV(ISO::conic, d3, o);

ISO_DEFCOMPV(ISO::sphere, centre, radius);
ISO_DEFCOMPV(ISO::ellipsoid, centre, axis1, axis2, axis3_len);
ISO_DEFCOMPV(ISO::cuboid, min, max);
ISO_DEFCOMPV(ISO::directed_shape, centre, radius, dir);
ISO_DEFCOMPV(ISO::tetrahedron, x, y, z, centre);
ISO_DEFCOMPV(ISO::quadric, d4, d3, o);
ISO_DEFCOMPV(ISO::circle3, centre, radius2, pl);
ISO_DEFUSERCOMPXV(ISO::bezier2d, "bezier2d", c0, c1, c2, c3);
ISO_DEFUSERCOMPXV(ISO::bezier2d2, "bezier2d2", c0, c1, c2);
ISO_DEFUSERCOMPXV(ISO::bezier_chain2d, "bezier_chain2d", c);
ISO_DEFUSERCOMPXV(ISO::bezier_chain2d2, "bezier_chain2d2", c);
ISO_DEFUSERCOMPXV(ISO::bspline2d, "bspline2d", c, k);
ISO_DEFUSERCOMPXV(ISO::bspline2d2, "bspline2d2", c, k);
ISO_DEFUSERCOMPXV(ISO::nurbs2d, "nurbs2d", c, k);
ISO_DEFUSERCOMPXV(ISO::nurbs2d2, "nurbs2d2", c, k);

namespace iso {
ISO_DEFUSER(circle, ISO::circle);
ISO_DEFUSER(ellipse, ISO::ellipse);
ISO_DEFUSER(rectangle, ISO::rectangle);
ISO_DEFUSER(triangle, ISO::shape2x3);
ISO_DEFUSER(parallelogram, ISO::shape2x3);
ISO_DEFUSER(quadrilateral, ISO::quadrilateral);
ISO_DEFUSER(conic, ISO::conic);

ISO_DEFUSER(sphere, ISO::sphere);
ISO_DEFUSER(ellipsoid, ISO::ellipsoid);
ISO_DEFUSER(cuboid, ISO::cuboid);
ISO_DEFUSER(cylinder, ISO::directed_shape);
ISO_DEFUSER(capsule, ISO::directed_shape);
ISO_DEFUSER(cone, ISO::directed_shape);
ISO_DEFUSER(tetrahedron, ISO::tetrahedron);
ISO_DEFUSER(quadric, ISO::quadric);
ISO_DEFUSER(circle3, ISO::circle3);
}

#endif // GEOMETRY_ISO_H
