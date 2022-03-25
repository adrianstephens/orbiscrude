#include "csg.h"

#define AVOID_RECURSION

namespace iso {

int	csg_counts[4];

interval<float>	csgnode::polygon::quick_projection(param(float3) n) const {
	float	dx = dot(n, matrix.x);
	float	dy = dot(n, matrix.y);
	float	dw = dot(n, matrix.w);
	return {min(dx, dy) + dw, max(dx, dy) + dw};
}

interval<float>	csgnode::polygon::projection(param(float3) n) const {
	float	dx = dot(n, matrix.x);
	float	dy = dot(n, matrix.y);
	float	dw = dot(n, matrix.w);

	interval<float>	r(none);
	for (auto &i : verts)
		r |= dw + i.x * dx + i.y * dy;
	return r;
}

interval<float>	csgnode::polygon::intersection(const line3 &line) const {

	float3x4	basis(line.dir(), cross(matrix.z, line.dir()), matrix.z, line.p);
	float3x4	mat2	= matrix / basis;

	interval<float>	r(none);

	position3	d0		= mat2 * position3(verts.back(), zero);
	for (auto &v1 : verts) {
		position3	d1	= mat2 * position3(v1, zero);
		if (d0.v.y * d1.v.y <= zero) {
			position3	v = lerp(d0, d1, d0.v.y / (d0.v.y - d1.v.y));
			r |= v.v.x;
		}
		d0	= d1;
	}
	return r;
}

bool intersect(const csgnode::polygon &p0, const csgnode::polygon &p1) {
	auto	plane0 = p0.plane();
	if (!p1.projection(plane0.normal()).contains(plane0.dist()))
		return false;

	auto	plane1 = p1.plane();
	if (colinear(plane0.normal(), plane1.normal())) {
		return true;
	}

	if (!p0.projection(plane1.normal()).contains(plane1.dist()))
		return false;

	line3	line = plane0 & plane1;
	interval<float>	int0 = p0.intersection(line);
	interval<float>	int1 = p1.intersection(line);
	return overlap(int0, int1);
}


// Split poly by splitplane if needed, then put the polygon or polygon fragments in the appropriate lists
// Coplanar polygons go into either coplanarFront or coplanarBack depending on their orientation with respect to this plane
// Polygons in front or in back of this plane go into either front or back
void csgnode::polygon::split(const plane2 &splitplane, float epsilon, dynamic_array<polygon> &coplanarFront, dynamic_array<polygon> &coplanarBack, dynamic_array<polygon> &front, dynamic_array<polygon> &back) const {
	enum TYPE {
		COPLANAR	= 0,
		FRONT		= 1,
		BACK		= 2,
		SPANNING	= 3
	};

	line2	x	= (splitplane / matrix) & z_axis;

	// Classify each point as well as the entire csg_polygon into one of the above four classes
	int		type = 0;
	for (auto &i : verts) {
		//float	d	= splitplane.dist(matrix * position3(i, zero));
		float	d	= x.dist(position2(i));
		//ISO_ASSERT(approx_equal(d0, d));
		if (d < -epsilon)
			type |= BACK;
		else if (d > epsilon)
			type |= FRONT;
		if (type == SPANNING)
			break;
	}

	++csg_counts[type];

	// Put the poly in the correct list, splitting it when necessary
	switch (type) {
		case COPLANAR:
			(dot(splitplane.normal(), matrix.z.xyz) > 0 ? coplanarFront : coplanarBack).push_back(*this);
			break;

		case FRONT:
			front.push_back(*this);
			break;

		case BACK:
			back.push_back(*this);
			break;

		case SPANNING: {
			dynamic_array<float2> f, b;
			float2	v0	= verts.back();
			float	d0	= x.dist(position2(v0));
//			float	d0	= splitplane.dist(matrix * position3(v0, zero));

			for (auto &v1 : verts) {
				float	d1	= x.dist(position2(v1));
//				float	d1	= splitplane.dist(matrix * position3(v1, zero));

				if (d0 >= -epsilon)
					f.push_back(v0);

				if (d0 <= epsilon)
					b.push_back(v0);

				if (min(d0, d1) < -epsilon && max(d0, d1) > epsilon) {
					float2	v = lerp(v0, v1, d0 / (d0 - d1));
					f.push_back(v);
					b.push_back(v);
				}
				v0	= v1;
				d0	= d1;
			}

			if (f.size() >= 3)
				front.push_back(polygon(*this, move(f)));

			if (b.size() >= 3)
				back.push_back(polygon(*this, move(b)));
			break;
		}
	}
}

// Recursively remove all polygons in list that are inside n
dynamic_array<csgnode::polygon> clip(const dynamic_array<csgnode::polygon> &list, const csgnode &n, float epsilon) {
#ifdef AVOID_RECURSION
	struct stack_element {
		csgnode	*n;
		dynamic_array<csgnode::polygon> list;
	} stack[32], *sp = stack;

	dynamic_array<csgnode::polygon> clipped;
	dynamic_array<csgnode::polygon> list2;// = list;

	bool	first = true;
	for (const csgnode *p = &n; p; first = false) {

		dynamic_array<csgnode::polygon> list_front, list_back;
		for (auto &i : first ? list : list2)
			i.split(p->p, epsilon, list_front, list_back, list_front, list_back);

		csgnode	*f	= p->front, *b = p->back;

		if (!f)
			clipped.append(list_front);

		if (f && list_front.empty())
			f = 0;

		if (b && list_back.empty())
			b = 0;

		if (f && b) {
			ISO_ASSERT(sp < end(stack));
			if (f->total > b->total) {
				sp->n		= exchange(f, nullptr);
				sp->list	= move(list_front);
			} else {
				sp->n		= b;
				sp->list	= move(list_back);
			}
			++sp;
		}


		if (f) {
			p		= f;
			list2	= move(list_front);

		} else if (b) {
			p		= b;
			list2	= move(list_back);

		} else if (sp > stack) {
			--sp;
			p		= sp->n;
			list2	= move(sp->list);

		} else {
			p		= 0;
		}
	}
	return clipped;
#else
	if (len2(n.p.normal()) == zero)
		return list;

	dynamic_array<csgnode::polygon> list_front, list_back;
	for (auto &i : list)
		i.split(n.p, list_front, list_back, list_front, list_back);

	if (n.front && !list_front.empty())
		list_front = clip(list_front, *n.front, epsilon);

	if (n.back && !list_back.empty())
		list_front.append(clip(list_back, *n.back));

	return list_front;
#endif
}

csgnode *clip1(const csgnode *n, const csgnode &y, float epsilon) {
	csgnode	*r	= new csgnode;
	r->p		= n->p;
	r->polygons	= clip(n->polygons, y, epsilon);
	r->total	= n->total;
	r->front	= n->front;
	r->back		= n->back;
	return r;
}

// Recursively remove all polygons in tree A that are inside tree B
csg clip(const csg &x, const csg &y) {
	csg	ret;
	ret.p			= x.p;
	ret.polygons	= clip(x.polygons, y, epsilon);
	ret.total		= x.total;

#ifdef AVOID_RECURSION
	ret.front		= x.front;
	ret.back		= x.back;

	csgnode	*stack[32], **sp = stack;

	for (csgnode *n = &ret; n;) {
		csgnode	*f	= n->front, *b = n->back;
		if (f && b) {
			ISO_ASSERT(sp < end(stack));
			if (f->total > b->total)
				*sp++ = n->front = clip1(exchange(f, nullptr), y, epsilon);
			else
				*sp++ = n->back = clip1(b, y, epsilon);
		}

		n 	= f ? (n->front = clip1(f, y, epsilon))
			: b ? (n->back  = clip1(b, y, epsilon))
			: sp > stack ? *--sp : 0;
	}
#else
	if (x.front)
		ret.front = new csgnode(clip(*x.front, y, epsilon));
	if (x.back)
		ret.back = new csgnode(clip(*x.back, y, epsilon));
#endif
	return ret;
}

#ifdef DEFER_INVERT

csgnode clip(const _not<csgnode> &a, const csgnode &b) {
	return clip(a.t, b);
}
csgnode clip(const csgnode &a, const _not<csgnode> &b) {
	return clip(a, b.t);
}
csgnode clip(const _not<csgnode> &a, const _not<csgnode> &b) {
	return clip(a.t, b.t);
}
csgnode::csgnode(const _not<csgnode> &b) : p(-b.t.p), polygons(b.t.polygons) {
	for (auto &i : polygons)
		i.flip();
	if (b.t.front)
		back = new csgnode(~*b.t.front);
	if (b.t.back)
		front = new csgnode(~*b.t.back);
}

#else

csg& csg::invert() {
#ifdef AVOID_RECURSION
	csgnode	*stack[32], **sp = stack;

	for (csgnode *n = this; n;) {
		for (auto &i : n->polygons)
			i.flip();
		n->p = -n->p;

		csgnode	*f	= n->front, *b = n->back;
		n->front	= b;
		n->back		= f;

		if (f && b) {
			ISO_ASSERT(sp < end(stack));
			*sp++ = f->total > b->total ? exchange(f, nullptr) : b;
		}

		n = f ? f : b ? b : sp > stack ? *--sp : 0;
	}
#else
	for (auto &i : polygons)
		i.flip();
	p = -p;
	if (front)
		front->invert();
	if (back)
		back->invert();
	swap(front, back);
#endif
	return *this;
}

#endif

csgnode::plane2 get_splitplane(const dynamic_array<csgnode::polygon> &list) {
#if 1
	if (list.size() > 1024) {
		dynamic_array<position3>	centres = transformc(list, [](const csgnode::polygon &i) { return get_trans(i.matrix); });
		auto			ext		= get_extent(centres);
		int				axis 	= max_component_index(ext.extent());
		const position3	*m 		= median(centres, [axis](const position3 &a, const position3 &b) { return a.v[axis] < b.v[axis]; });

		return csgnode::plane2(rotate(float3(x_axis), -axis), *m);
	}
#endif
	return csgnode::plane2(list[0].matrix.z, get_trans(list[0].matrix));
}


// Build a BSP tree out of polygons
// When called on an existing tree, the new polygons are filtered down to the bottom of the tree and become new nodes there
// Each set of polygons is partitioned using the first csg_polygon (no heuristic is used to pick a good split)

csg& csg::operator+=(dynamic_array<polygon> &&list) {
#ifdef AVOID_RECURSION
	struct stack_element {
		csgnode	*n;
		dynamic_array<polygon> list;
	} stack[32], *sp = stack;

	for (csgnode *n = this; n;) {
		n->total += list.size32();

		if (len2(n->p.normal()) == zero)
			n->p = get_splitplane(list);

		dynamic_array<polygon> list_front, list_back;
		list_front.reserve(n->total / 2);
		list_back.reserve(n->total / 2);

		int prev_split = csg_counts[3];
		for (auto &i : list)
			i.split(n->p, epsilon, n->polygons, n->polygons, list_front, list_back);

		int	num_splits = csg_counts[3] - prev_split;
		if (num_splits > 8 && num_splits > list.size() / 16)
			ISO_TRACE("lots of split\n");

		if (list_front.size() && list_back.size()) {
			ISO_ASSERT(sp < end(stack));
			if (list_front.size() > list_back.size()) {
				if (!n->front)
					n->front = new csgnode;
				sp->n	= n->front;
				swap(sp->list, list_front);
				list_front.clear();
			} else {
				if (!n->back)
					n->back = new csgnode;
				sp->n	= n->back;
				swap(sp->list, list_back);
			}
			++sp;
		}

		if (list_front.size()) {
			if (!n->front)
				n->front = new csgnode;
			n		= n->front;
			list	= move(list_front);

		} else if (list_back.size()) {
			if (!n->back)
				n->back = new csgnode;
			n		= n->back;
			list	= move(list_back);

		} else if (sp > stack) {
			--sp;
			n		= sp->n;
			list	= move(sp->list);

		} else {
			n		= 0;
		}
	}
#else
	if (list.size()) {
		if (len2(p.normal()) == zero)
			p = list[0].tri.plane();

		dynamic_array<polygon> list_front, list_back;

		for (auto &i : list)
			i.split(p, polygons, polygons, list_front, list_back);

		if (list_front.size()) {
			if (!front)
				front = new csgnode;
			*front += move(list_front);
		}
		if (list_back.size()) {
			if (!back)
				back = new csgnode;
			*back += move(list_back);
		}
	}
#endif
	return *this;
}

csg::csg(dynamic_array<polygon> &&list, float epsilon) : epsilon(epsilon) {
	operator+=(move(list));
}

csg::csg(const parallelepiped &box, float epsilon) : epsilon(epsilon) {
	dynamic_array<polygon> list;
	for (auto i : planes<3>())
		list.emplace_back(0, get_face(box, i));
	operator+=(move(list));
}

csg::csg(const csg &x) : csgnode(x), epsilon(x.epsilon) {
#ifdef AVOID_RECURSION
	csgnode		*stack[32], **sp = stack;
	for (csgnode *n = this; n; ) {
		auto	f = n->front;
		auto	b = n->back;

		if (f && b) {
			ISO_ASSERT(sp < end(stack));
			*sp++ = f->total > b->total
				? (n->front = new csgnode(*exchange(f, nullptr)))
				: (n->back  = new csgnode(*b));
		}
		n 	= f ? (n->front = new csgnode(*f))
			: b ? (n->back  = new csgnode(*b))
			: sp > stack ? *--sp : 0;
	}
#else
	front	= b.front	? new csgnode(*b.front)	: 0;
	back	= b.back	? new csgnode(*b.back)	: 0;
#endif
}

csg::~csg() {
#ifdef AVOID_RECURSION
	if (front || back) {
		csgnode		*stack[32], **sp = stack;

		if (front && back)
			*sp++ = exchange(front->total > back->total ? front : back, nullptr);

		for (csgnode *n = front ? front : back; n; ) {
			auto	f = exchange(n->front, nullptr);
			auto	b = exchange(n->back, nullptr);
			if (f && b) {
				ISO_ASSERT(sp < end(stack));
				*sp ++ = f->total > b->total ? exchange(f, nullptr) : b;
			}
			auto	n0 = n;
			n = f ? f : b ? b : sp > stack ? *--sp : 0;
			delete n0;
		}
	}
#else
	delete front;
	delete back;
#endif
}

void get_polys(const csgnode &n, dynamic_array<csgnode::polygon> &list) {
	list.append(n.polygons);
	if (n.front)
		get_polys(*n.front, list);
	if (n.back)
		get_polys(*n.back, list);
}

dynamic_array<csgnode::polygon> all_polys(const csgnode &x) {
	dynamic_array<csgnode::polygon> list;
#ifdef AVOID_RECURSION
	csgnode		*stack[32], **sp = stack;
	for (const csgnode *n = &x; n; ) {
		list.append(n->polygons);

		auto	f = n->front;
		auto	b = n->back;

		if (f && b) {
			ISO_ASSERT(sp < end(stack));
			*sp++ = f->total > b->total ? exchange(f, nullptr) : b;
		}
		n 	= f ? f : b ? b : sp > stack ? *--sp : 0;
	}
#else
	get_polys(x, list);
#endif
	return list;
}

#ifdef DEFER_INVERT
void get_inv_polys(const csgnode &n, dynamic_array<csgnode::polygon> &list) {
	list.append(n.polygons);//flip here
	if (n.front)
		get_inv_polys(*n.front, list);
	if (n.back)
		get_inv_polys(*n.back, list);
}

dynamic_array<csgnode::polygon> all_polys(const _not<csgnode> &n) {
	dynamic_array<csgnode::polygon> list;
	get_inv_polys(n.t, list);
	return list;
}
#endif

dynamic_array<csgnode::polygon> csg::polys() const {
	dynamic_array<csgnode::polygon> list;
	get_polys(*this, list);
	return list;
}

//All CSG operations are implemented in terms of two functions, clip() and invert(), which remove parts of a BSP tree inside another BSP tree and swap solid and empty space, respectively
//To find the union of a and b, we want to remove everything in a inside b and everything in b inside a, then combine polygons from a and b into one solid:
//
//clip(a, b);
//clip(b, a);
//a.add(b.polys());
//
//The only tricky part is handling overlapping coplanar polygons in both trees.
//The code above keeps both copies, but we need to keep them in one tree and remove them in the other tree.
//To remove them from b we can clip the inverse of b against a, thus:
//
//a = clip(a, b);
//b = clip(b, a);
//b.invert();
//b = clip(b, a);
//b.invert();
//a.add(b.polys());

csg operator|(const csg &a1, const csg &b1) {
	csg	a = clip(a1, b1);
	csg	b = clip(b1, a);
	b = clip(b.invert(), a);

	return a += b.invert().polys();
}

// A - B = ~(~A | B)
csg operator-(const csg &a1, const csg &b1) {
#if 0
	return (~a1 | b).invert();
#else
	csg		a	= clip(csg(a1).invert(), b1);
	csg		b	= clip(b1, a);
	b = clip(b.invert(), a);

	return (a += b.invert().polys()).invert();
#endif
}

// A & B = ~(~A | ~B)
csg operator&(const csg &a1, const csg &b1) {
#if 0
	return (~a1 | ~b1).invert();
#else
	csg		a	= csg(a1).invert();
	csg		b	= clip(b1, a).invert();
	a = clip(a, b);
	b = clip(b, a);

	return (a += b.polys()).invert();
#endif
}
}
