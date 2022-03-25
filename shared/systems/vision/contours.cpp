#include "contours.h"
#include "base/algorithm.h"
#include "maths/triangulate.h"

namespace iso {

//template<typename T> array_vec<T, 2> corner(const interval<array_vec<T, 2> > &ext, CORNER i) {
//	return array_vec<T, 2>{(&ext.a)[i & 1].x, (&ext.a)[i >> 1].y};
//}

void DirectedContour(const block<int, 2> &image, dynamic_array<point> &points, point p0, Direction dir, int nbd) {
	Direction trace = dir.clockwise();

	// find p1 (3.1)
	while (trace != dir && !trace.active(image, p0))
		trace = trace.clockwise();

	if (trace == dir) {
		// signals the starting pixel is alone! (3.1)
		points.push_back(p0);
		image[p0.y][p0.x] = -nbd;
		return;
	}

	point	p1	= trace.offset(p0);
	point	p2	= p1;
	point	p3	= p0; // (3.2);
	do {
		uint8	checked = 0;//	 N , NE ,E ,SE ,S ,SW ,W ,NW

		trace	= trace.counter_clockwise();
		while (!trace.active(image, p3)) { // 3.3
			checked |= 1 << trace;
			trace	= trace.counter_clockwise();
		}

		points.push_back(p3);

		if (int v = image[p3.y][p3.x]) {
			if (p3.x == image.size<1>() - 1 || (checked & (1 << Direction::EAST)))	// this is 3.4(a) with an edge case check
				image[p3.y][p3.x] = -nbd;
			else if (v == 1)	// only set if the pixel has not been visited before 3.4(b)!
				image[p3.y][p3.x] = nbd;
		}

		p2		= p3;					// 3.5
		p3		= trace.offset(p3);		// 3.5
		trace	= trace.flip();
	} while (any(p3 != p0 | p2 != p1));	// 3.5
}

contour<point> SuzukiContour(const block<int, 2> &image) {
	struct contour_hole {
		contour<point>	*c;
		bool			hole;
		contour_hole(contour<point> *_c, bool _hole) : c(_c), hole(_hole) {}
	};
	dynamic_array<contour_hole>	borders;

	// Prepare the special outer frame
	contour<point>		root;
//	root.points.append(corners_cw(interval<point>(point(0, 0), point(image.size<1>(), image.size<2>()))));
	root.points.push_back(zero);
	root.points.push_back(point{int(image.size<1>()), 0});
	root.points.push_back(point{int(image.size<1>()), int(image.size<2>())});
	root.points.push_back(point{0, int(image.size<2>())});
	borders.push_back(contour_hole(&root, true));

	for (int y = 0; y < image.size<2>(); y++) {
		int	lnbd = 1; // Begining of appendix 1, this is the begining of a scan

		for (int x = 0; x < image.size<1>(); x++) {
			int		f		= image[y][x];
			bool	outer	= f == 1 && (x == 0 || image[y][x - 1] == 0);

			if (outer || (f >= 1 && (x == image.size<1>() - 1 || image[y][x + 1] == 0))) { // check 1(c)
				contour<point> *border	= new contour<point>;
				bool			hole	= !outer;
				borders.push_back(contour_hole(border, hole));

				// in 1(b) we set lnbd to the pixel value if it is greater than 1
				if (hole && f > 1)
					lnbd = f;

				contour_hole	&borderPrime	= borders[lnbd - 1];
				(borderPrime.hole ^ hole ? borderPrime.c : borderPrime.c->parent)->attach(border);

				DirectedContour(image, border->points, point{x, y}, outer ? Direction::WEST : Direction::EAST, num_elements32(borders));
			}
			// This is step (4)
			if (f != 0 && f != 1)
				lnbd = abs(f);
		}
	}
	return root;
}

contour<position2> *_ExpandContour(const contour<position2> &c, float x) {
	contour<position2> *c2 = new contour<position2>;
	int		n1 = c.points.size32();
	c2->points.resize(n1 == 1 ? 4 : n1 * 2);
	c2->points.resize(expand_polygon(c.points, c.points.size32(), x, c2->points));

	hierarchy_appender<contour<position2> > m(c2);
	for (auto &i : c.children)
		m.add_child(_ExpandContour(i, x));

	return c2;
}

contour<position2> ExpandContour(const contour<position2> &c, float x) {
	contour<position2> c2;
	int		n1 = c.points.size32();
	c2.points.resize(n1 * 2);
	c2.points.resize(expand_polygon(c.points, c.points.size32(), x, c2.points));

	hierarchy_appender<contour<position2> > m(&c2);
	for (auto &i : c.children)
		m.add_child(_ExpandContour(i, x));

	return c2;
}

contour<position2> operator*(param(float2x3) mat, const contour<position2> &c) {
	contour<position2> c2;
	int		n1 = c.points.size32();
	c2.points.resize(n1);

	transform(c.points, c2.points, [&mat](param(position2) p) {
		return mat * p;
	});

	hierarchy_appender<contour<position2> > m(&c2);
	for (auto &i : c.children)
		m.add_child(mat * i);

	return c2;
}

void MergeCloseContours(contour<position2> &root, float dist)  {
	Triangulator		t;
	dynamic_array<int>	indices;
	dynamic_array<int>	starts;

	int				nc = 0;
	for (auto &i : root.children) {
		t.AddVerts(i.points);
		starts.push_back(indices.size32());
		indices.append(repeat(nc, i.points.size32()));
		++nc;
	}

	t.Triangulate();

	struct closest_record {
		float	dist;
		int		i0, i1;
		closest_record() : dist(maximum), i0(0), i1(0)  {}
	};
	dynamic_array<closest_record>	closest(triangle_number_excl(nc));

	for (auto &i : t.tris) {
		float4	v1	= t.verts[i[2].vert];
		int		c1	= indices[int(v1.w)];
		for (int j = 0; j < 3; j++) {
			float4	v0	= v1;
			int		c0	= c1;
			v1	= t.verts[i[j].vert];
			c1	= indices[int(v1.w)];
			if (c0 != c1) {
				float	dist = len2(v0.xy - v1.xy);
				closest_record	&c = closest[triangle_index_excl(c0, c1)];
				if (dist < c.dist) {
					c.dist	= dist;
					c.i0	= int(v0.w) - starts[c0];
					c.i1	= int(v1.w) - starts[c1];
					if (c0 < c1)
						swap(c.i0, c.i1);
				}
			}
		}
	}

	for (int i = 0; i < nc; ++i) {
		auto	&s0		= root.children[i];
		for (int j = 0; j < i; ++j) {
			closest_record	&c = closest[triangle_index_excl(i, j)];
			if (c.dist < square(dist)) {
				//ISO_TRACEF("Merge %i (at %i) and %i (at %i)\n", i, c.i0, j, c.i1);

				auto			&s1 = root.children[j];
				int				n0	= s0.points.size32();
				int				n1	= s1.points.size32();
				CombineContours(s0.points, s1.points, c.i0, c.i1);

				for (int k = j + 1; k < i; k++) {
					closest_record	&c1 = closest[triangle_index_excl(i, k)];
					//adjust point index that was on i
					c1.i0	= wrap(c1.i0 - c.i0, n0);
				}
				for (int k = i + 1; k < nc; k++) {
					closest_record	&c1 = closest[triangle_index_excl(k, i)];
					closest_record	&c2 = closest[triangle_index_excl(k, j)];
					if (c2.dist < c1.dist) {
						c1.dist = c2.dist;
						c1.i0	= c2.i0;
						//adjust point index that was on j
						c1.i1	= wrap(c2.i1 - c.i1, n1) + n0 + 1;
					} else {
						//adjust point index that was on i
						c1.i1	= wrap(c1.i1 - c.i0, n0);
					}
					c2.dist = maximum;
				}
				s1.points.reset();
			}
		}
	}
}

} // namespace iso
