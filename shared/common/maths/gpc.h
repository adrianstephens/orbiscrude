//-----------------------------------------------------------------------------
//
//	based on Generic Polygon Clipper:
//		A new algorithm for calculating the difference, intersection, exclusive-or or union of arbitrary polygon sets.
//		Alan Murta (email: gpc@cs.man.ac.uk)
//-----------------------------------------------------------------------------

#ifndef GPC_H
#define GPC_H

#include "base/vector.h"
#include "base/array.h"

#undef IGNORE

namespace iso {

#define GPC_EPSILON 2.2204460492503131e-016

// Set operation type
enum gpc_op {
	GPC_DIFF, // Difference
	GPC_INT,  // Intersection
	GPC_XOR,  // Exclusive or
	GPC_UNION // Union
};

typedef double2p					gpc_vertex;
typedef dynamic_array<gpc_vertex>	gpc_vertex_list;

struct gpc_vertex_list_flag : gpc_vertex_list {
	enum {
		HOLE	= 1 << 0,
		IGNORE	= 1 << 1,
	};
	uint32	flag;
	gpc_vertex_list_flag() {}
	gpc_vertex_list_flag(gpc_vertex_list &&a, uint32 flag) : gpc_vertex_list(move(a)), flag(flag) {}
};

typedef dynamic_array<gpc_vertex_list_flag>	gpc_polygon;
typedef dynamic_array<gpc_vertex_list>		gpc_tristrip;

gpc_polygon gpc_polygon_clip(gpc_op set_operation, const gpc_polygon &subject_polygon, const gpc_polygon &clip_polygon);
gpc_tristrip gpc_tristrip_clip(gpc_op set_operation, const gpc_polygon &subject_polygon, const gpc_polygon &clip_polygon);
gpc_tristrip gpc_polygon_to_tristrip(const gpc_polygon &polygon);

}
#endif
