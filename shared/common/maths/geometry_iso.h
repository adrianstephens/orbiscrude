#ifndef GEOMETRY_ISO_H
#define GEOMETRY_ISO_H

#include "vector_iso.h"
#include "maths/geometry.h"

namespace ISO {

struct circle {
	union { struct { float2p centre; float radius2; };	float4p _x; };
	~circle() {}
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

} // namespace ISO

ISO_DEFCOMPV(ISO::circle, centre, radius2);
ISO_DEFUSERX(iso::circle, ISO::circle, "circle");
ISO_DEFCOMPV(ISO::ellipse, centre, major, ratio);
ISO_DEFUSERX(iso::ellipse, ISO::ellipse, "ellipse");
ISO_DEFCOMPV(ISO::rectangle, min, max);
ISO_DEFUSERX(iso::rectangle, ISO::rectangle, "rectangle");
ISO_DEFCOMPV(ISO::shape2x3, x, y, z);
ISO_DEFUSERX(iso::triangle, ISO::shape2x3, "triangle");
ISO_DEFUSERX(iso::parallelogram, ISO::shape2x3, "parallelogram");
ISO_DEFCOMPV(ISO::quadrilateral, a, b, c, d);
ISO_DEFUSERX(iso::quadrilateral, ISO::quadrilateral, "quadrilateral");
ISO_DEFCOMPV(ISO::conic, d3, o);
ISO_DEFUSERX(iso::conic, ISO::conic, "conic");

ISO_DEFCOMPV(ISO::sphere, centre, radius);
ISO_DEFUSERX(iso::sphere, ISO::sphere, "sphere");
ISO_DEFCOMPV(ISO::ellipsoid, centre, axis1, axis2, axis3_len);
ISO_DEFUSERX(iso::ellipsoid, ISO::ellipsoid, "ellipsoid");
ISO_DEFCOMPV(ISO::cuboid, min, max);
ISO_DEFUSERX(iso::cuboid, ISO::cuboid, "cuboid");
ISO_DEFCOMPV(ISO::directed_shape, centre, radius, dir);
ISO_DEFUSERX(iso::cylinder, ISO::directed_shape, "cylinder");
ISO_DEFUSERX(iso::capsule, ISO::directed_shape, "capsule");
ISO_DEFUSERX(iso::cone, ISO::directed_shape, "cone");
ISO_DEFCOMPV(ISO::tetrahedron, x, y, z, centre);
ISO_DEFUSERX(iso::tetrahedron, ISO::tetrahedron, "tetrahedron");
ISO_DEFCOMPV(ISO::quadric, d4, d3, o);
ISO_DEFUSERX(iso::quadric, ISO::quadric, "quadric");
ISO_DEFCOMPV(ISO::circle3, centre, radius2, pl);
ISO_DEFUSERX(iso::circle3, ISO::circle3, "circle3");

ISO_DEFUSERCOMPXV(iso::curve_vertex, "curve_vertex", x, y, flags);

#endif // GEOMETRY_ISO_H
