#ifndef OCTTREE_H
#define OCTTREE_H

#include "base/vector.h"
#include "base/array.h"
#include "base/functions.h"
#include "maths/geometry.h"

namespace iso {

//-----------------------------------------------------------------------------
//	octree
//-----------------------------------------------------------------------------

struct octree {
	typedef	callback_ref<bool(int i, param(ray3) r, float &t)>		cb_calc_dist;
	typedef	callback_ref<bool(int i, param(position3) v, float &d)>	cb_check_dist;

	struct node {
		node		*child[8];
		range<int*>	indices;
	};

	position3			centre;
	float3				scale;
	int					max_depth;

	dynamic_array<int>	indices;
	dynamic_array<node>	nodes;

	void	init(cuboid *exts, int num);

	int		find(param(position3) v, const cb_check_dist &check_dist, float maxd = maximum);
	int		shoot_ray(param(ray3) ray, const cb_calc_dist &calc_dist) const;
	int		shoot_ray(param(ray3) ray, float slack, const cb_calc_dist &calc_dist) const;
	int		shoot_ray(param(float4x4) mat, const cb_calc_dist &calc_dist) const;
	int		shoot_ray(param(float4x4) mat, float slack, const cb_calc_dist &calc_dist) const;
};

//-----------------------------------------------------------------------------
//	dynamic_octree
//-----------------------------------------------------------------------------

struct dynamic_octree {
	typedef	callback_ref<bool(void *item, param(ray3) r, float &t)>			cb_calc_dist;
	typedef	callback_ref<bool(void *item, param(position3) v, float &d)>	cb_check_dist;

	struct node {
		node					*child[8];
		dynamic_array<void*>	items;
		node() { clear(child); }
	};

	position3			centre;
	float3				scale;
	node				root;

	dynamic_octree(const cuboid &ext);
	void	add(const cuboid &ext, void *item);
	void	add(param(position3) v, void *item);
	void*	find(param(position3) v, const cb_check_dist &check_dist, float maxd = maximum);

	void*	shoot_ray(param(ray3) ray, const cb_calc_dist &calc_dist) const;
	void*	shoot_ray(param(ray3) ray, float slack, const cb_calc_dist &calc_dist) const;
	void*	shoot_ray(param(float4x4) mat, const cb_calc_dist &calc_dist) const;
	void*	shoot_ray(param(float4x4) mat, float slack, const cb_calc_dist &calc_dist) const;
};

} // namespace iso
#endif // OCTTREE_H
