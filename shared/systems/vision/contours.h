#ifndef CONTOURS_H
#define CONTOURS_H

#include "maths/geometry.h"
#include "base/array.h"
#include "base/tree.h"
#include "base/block.h"
#include "maths/triangulate.h"

namespace iso {

struct Direction {
	enum DIRECTION {
		NORTH,
		NORTH_EAST,
		EAST,
		SOUTH_EAST,
		SOUTH,
		SOUTH_WEST,
		WEST,
		NORTH_WEST,
		BAD = -1,
	};
	DIRECTION	d;
	static int dirx(DIRECTION i) { return i & 3 ? 1 - (i & 4) / 2 : 0;	}
	static int diry(DIRECTION i) { return dirx(DIRECTION(i - 2));	}

	// DIRECTION from and to adjacent pixels
	static DIRECTION fromTo(const point &from, const point &to) {
		if (from.y == to.y) {
			return	from.x == to.x	? BAD
				:	from.x < to.x	? EAST
				:	WEST;
		} else if (from.y < to.y) {
			return	from.x == to.x	? SOUTH
				:	from.x < to.x	? SOUTH_EAST
				:	SOUTH_WEST;
		} else {
			return from.x == to.x	? NORTH
				:	from.x < to.x	? NORTH_EAST
				:	NORTH_WEST;
		}
	}

	Direction(DIRECTION _d) : d(_d) {}
	operator DIRECTION()			const { return d; }
	DIRECTION	clockwise()			const { return DIRECTION((d + 1) & 7);	}
	DIRECTION	counter_clockwise()	const { return DIRECTION((d - 1) & 7);	}
	DIRECTION	flip()				const { return DIRECTION(d ^ 4);	}
	int			dirx()				const { return dirx(d);	}
	int			diry()				const { return diry(d);	}

	point		offset(const point &from) const {
		return point{from.x + dirx(d), from.y + diry(d)};
	}

	bool active(block<int, 2> img, const point &from) const {
		point	p = offset(from);
		return p.x >= 0 && p.x < img.size<1>() && p.y >= 0 && p.y < img.size<2>() && img[p.y][p.x];
	}
};

template<typename T> struct contour : hierarchy<contour<T> > {
	dynamic_array<T>	points;

	contour()	{}

	contour(const contour &c2) : points(c2.points) {
		for (auto &i : c2.children)
			this->attach(new contour(i));
	}
	contour(contour &&c2) : hierarchy<contour<T> >(move(c2)), points(move(c2.points)) {}
	template<typename T2> contour(const contour<T2> &c2) : points(transformc(c2.points, [](const T2 &p) { return to<element_type<T>>(p);})) {
		hierarchy_appender<contour<T> > m(this);
		for (auto &i : c2.children)
			m.add_child(new contour(i));
	}
	interval<T> get_extent() const {
		if (points.empty())
			return interval<T>();
		return iso::get_extent(points);
	}
};

template<typename T> inline interval<T> get_extent(const contour<T> &c) {
	return get_extent(c.points);
}

contour<position2> operator*(param(float2x3) mat, const contour<position2> &c);

template<typename T> void CombineContours(dynamic_array<T> &a, dynamic_array<T> &b, int va, int vb) {
	ISO_ASSERT(va >= 0 && va < a.size());
	ISO_ASSERT(vb >= 0 && vb < b.size());
	block_exchange(a.begin(), a.begin() + va, a.end());
	a.insert(a.end(), a.front());
	a.append(b.begin() + vb, b.end());
	a.append(b.begin(), b.begin() + vb);
	a.push_back(b[vb]);
}

template<typename T> void CombineContours(contour<T> &a, contour<T> &b) {
	int		va, vb;
//	float	d = closest_point(make_point_cloud2(a.points), make_point_cloud2(b.points), va, vb);
	CombineContours(a.points, b.points, va, vb);
	b.detach();
}

template<typename T> float GetMaxArea(const contour<T> &root) {
	float	max_area = 0;
	//BUG
//	for_each(root.children, [&max_area](const contour<T> &a) {
//		max_area = max(max_area, make_simple_polygon(make_point_cloud(a.points)).area());
//	});
	return max_area;
}

template<typename T> void CleanByArea(contour<T> &root, float area) {
	for (auto i = root.children.beginp(); i != root.children.end(); ++i) {
		if (as_simple_polygon(make_point_cloud(i->points)).area() < area)
			delete i.unlink();
	}
}

template<typename T> bool CleanByRelativeExtent(contour<T> &root, float factor, float min_area = 0) {
	float	a	= area(get_extent(root.points));
	if (a <= min_area)
		return false;
	min_area = a * factor;
	for (auto i = root.children.beginp(); i != root.children.end(); ++i) {
		if (!CleanByRelativeExtent(*i, factor, min_area))
			delete i.unlink();
	}
	return true;
}

void				DirectedContour(const block<int, 2> &image, dynamic_array<point> &points, point p0, Direction dir, int nbd);
contour<point>		SuzukiContour(const block<int, 2> &image);
contour<position2>	ExpandContour(const contour<position2> &c, float x);
void				MergeCloseContours(contour<position2> &root, float dist);

} // namespace iso

#endif // CONTOURS_H
