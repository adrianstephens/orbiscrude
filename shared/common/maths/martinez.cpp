#include "martinez.h"

namespace iso {

int Martinez::findIntersection(const Point &a0, const Point &a1, const Point &b0, const Point &b1, Point& i0, Point& i1) {
	static const double epsilon2 = 0.0000001; // it was 0.001 before
	static const double dist_epsilon2 = square(0.00000001);

	Point	da			= a1 - a0;
	Point	db			= b1 - b0;
	Point	E			= b0 - a0;
	auto	kross		= cross(da, db);
	auto	len2a		= len2(da);
	auto	len2b		= len2(db);

	if (square(kross) > epsilon2 * len2a * len2b) {
		// lines of the segments are not parallel
		auto s = cross(E, db) / kross;
		if (s < 0 || s > 1)
			return 0;

		auto t = cross(E, da) / kross;
		if (t < 0 || t > 1)
			return 0;

		// intersection of lines is a point an each segment
		i0 = a0 + s * da;
		if (dist2(i0, a0) < dist_epsilon2)
			i0 = a0;
		else if (dist2(i0, a1) < dist_epsilon2)
			i0 = a1;
		else if (dist2(i0, b0) < dist_epsilon2)
			i0 = b0;
		else if (dist2(i0, b1) < dist_epsilon2)
			i0 = b1;
		return 1;
	}

	// lines of the segments are parallel
	if (square(cross(E, da)) > epsilon2 * len2a * len2(E))
		return 0;	// lines of the segment are different

	auto	s0	= dot(da, E) / len2a;
	auto	s1	= s0 + dot(da, db) / len2a;
	auto	v0	= min(s0, s1);
	auto	v1	= max(s0, s1);

#if 1
	if (v0 > 1 || v1 < 0)
		return 0;

	if (v0 == 1) {
		i0 = a1;
		return 1;
	}
	if (v1 == 0) {
		i0 = a0;
		return 1;
	}
	return 2;
#else
	if (v0 < 1) {
		if (v1 > 0) {
			if (v0 <= 0) {
				i0 = a0;
			} else {
				i0 = a0 + v0 * da;
				if (dist2(i0, a0) < dist_epsilon2)
					i0 = a0;
				else if (dist2(i0, a1) < dist_epsilon2)
					i0 = a1;
				else if (dist2(i0, b0) < dist_epsilon2)
					i0 = b0;
				else if (dist2(i0, b1) < dist_epsilon2)
					i0 = b1;
			}
			i1 = v1 < 1 ? a0 + v1 * da : a1;
			return 2;

		} else if (v1 < 0) {
			return 0;

		} else {	// v1 == 0
			i0 = a0;
			return 1;
		}

	} else if (v0 > 1) {
		return 0;

	} else {	// v0 == 1
		i0 = a1;
		return 1;
	}
#endif
}

struct Martinez::Connector {
	struct PointChain : list<Point> {
		PointChain(const Point &a, const Point &b) {
			push_back(a);
			push_back(b);
		}
		int LinkSegment(const Point &a, const Point &b) {
			if (all(a == front())) {
				if (all(b == back()))
					return 2;
				push_front(b);
				return 1;
			}
			if (all(b == back())) {
				push_back(a);
				return 1;
			}
			if (all(b == front())) {
				if (all(a == back()))
					return 2;
				push_front(a);
				return 1;
			}
			if (all(a == back())) {
				push_back(b);
				return 1;
			}
			return 0;
		}
		bool LinkPointChain(PointChain& chain) {
			if (all(chain.front() == back())) {
				end().insert_before(++chain.begin(), chain.end());
				return true;
			}
			if (all(chain.back() == front())) {
				begin().insert_before(chain.begin(), --chain.end());
				return true;
			}
			if (all(chain.front() == front())) {
				reverse(chain.begin(), chain.end());
				begin().insert_before(chain.begin(), --chain.end());
				return true;
			}
			if (all(chain.back() == back())) {
				reverse(chain.begin(), chain.end());
				end().insert_before(++chain.begin(), chain.end());
				return true;
			}
			return false;
		}
	};
	list<PointChain>	open;
	list<PointChain>	closed;

	void add(const Point &a, const Point &b) {
		for (auto j = open.begin(); j != open.end(); ++j) {
			switch (j->LinkSegment(a, b)) {
				case 0:	// not linked
					break;
				case 1:	// linked
					for (auto k = j; ++k != open.end();) {
						if (j->LinkPointChain(*k)) {
							open.remove(k);
							break;
						}
					}
					return;
				case 2:	// linked + closed
					closed.end().insert_before(j);
					return;
			}
		}
		// The segment cannot be connected with any open polygon
		open.emplace_back(a, b);
	}

	operator Polygon() const {
		Martinez::Polygon	p;
		for (auto &i : closed) {
			Contour& contour = p.contours().push_back();
			for (auto &j : i)
				contour.points().push_back(j);
		}
		return p;
	}
};

void Martinez::add(const Segment& s, PolygonType ptype) {
	if (auto d = sign(s)) {
		auto	&e1		= eventHolder.emplace_back(s.a, d > 0, ptype);
		auto	&e2		= eventHolder.emplace_back(s.b, d < 0, ptype, &e1);
		e1.other = &e2;

		eq.push(&e1);
		eq.push(&e2);
	}
}

void Martinez::divide(SweepEvent* e, const Point& p) {
	SweepEvent	*e2		= e->other;

	// Right event of the left line segment resulting from dividing e (the line segment associated to e)
	SweepEvent &er = eventHolder.emplace_back(p, false, e->ptype, e, e->etype);

	// Left event of the right line segment resulting from dividing e (the line segment associated to e)
	SweepEvent &el = eventHolder.emplace_back(p, true, e->ptype, e2, e2->etype);

	// avoid a rounding error; the left event would be processed after the right event
	if (compare(e2, &el)) {
		e2->left	= true;
		el.left		= false;
		ISO_TRACE("oops1");
	}

	// avoid a rounding error; the left event would be processed after the right event
	if (!compare(e, &er)) {
		ISO_TRACE("oops2");
	}
	e2->other	= &el;
	e->other	= &er;
	eq.push(&el);
	eq.push(&er);
}

void Martinez::possibleIntersection(SweepEvent* e0, SweepEvent* e1) {
	// e0, e1 are both left

	//  you can uncomment these two lines if self-intersecting polygons are not allowed
	//if (e0->ptype == e1->ptype)
	//	return false;

	SweepEvent	*e2		= e0->other;
	SweepEvent	*e3		= e1->other;
	Point	ip1, ip2;

	switch (findIntersection(e0->p, e2->p, e1->p, e3->p, ip1, ip2)) {
		case 0:
			return;

		case 1:
			//ISO_ASSERT(any(e0->p != e1->p) && any(e2->p != e3->p));
			//if (any(e0->p != e1->p) && any(e2->p != e3->p)) {
				// the line segments do not intersect at an endpoint of both line segments
				if (any(e0->p != ip1) && any(e2->p != ip1))		// if ip1 is not an endpoint of the line segment associated to e0 then divide e0
					divide(e0, ip1);
				if (any(e1->p != ip1) && any(e3->p != ip1))		// if ip1 is not an endpoint of the line segment associated to e1 then divide e1
					divide(e1, ip1);
			//}
			return;

		case 2:
			// The line segments overlap
			if (e0->ptype == e1->ptype) {
				ISO_TRACE("bad overlap");
				auto	e0i = eventHolder.index_of(e0);
				auto	e1i = eventHolder.index_of(e1);
			}
			auto		etype	= e0->in_out == e1->in_out ? SAME_TRANSITION : DIFFERENT_TRANSITION;

			if (all(e0->p == e1->p)) {
				if (all(e2->p == e3->p)) {
					// both line segments equal
					e0->etype = e2->etype = NON_CONTRIBUTING;
					e1->etype = e3->etype = etype;
					return;
				}
				if (!compare(e2, e3)) {
					swap(e0, e1);
					swap(e2, e3);
				}
				e0->etype	= e2->etype = NON_CONTRIBUTING;
				e1->etype	= etype;
				divide(e1, e2->p);
				return;
			}

			if (all(e2->p == e3->p)) {
				if (!compare(e0, e1)) {
					swap(e0, e1);
					swap(e2, e3);
				}
				e1->etype	= e3->etype = NON_CONTRIBUTING;
				e2->etype	= etype;
				divide(e0, e1->p);
				return;
			}

			if (!compare(e0, e1))
				swap(e0, e1);

			if (!compare(e2, e3))
				swap(e2, e3);

			if (e0 != e3->other) {
				// no line segment includes totally the other one
				e1->etype	= NON_CONTRIBUTING;
				e2->etype	= etype;
				divide(e0, e1->p);
				divide(e1, e2->p);
			} else {
				// one line segment includes the other one
				e1->etype	= e1->other->etype = NON_CONTRIBUTING;
				divide(e0, e1->p);
				e3->other->etype = etype;
				divide(e3->other, e2->p);
			}
			return;
	}
}

Martinez::Polygon Martinez::compute(const Polygon& subject, const Polygon& clipping, Op op) {

	// Test 1 for trivial result case
	if (subject.empty() || clipping.empty()) {
		return	op == DIFF ? subject
			:	op == UNION || op == XOR ? (subject.empty() ? clipping : subject)
			:	Polygon();
	}

	// Test 2 for trivial result case
	auto	subj_ext = subject.get_box();
	auto	clip_ext = clipping.get_box();
	if (!overlap(subj_ext, clip_ext)) {
		if (op == DIFF)
			return subject;

		if (op == UNION || op == XOR) {
			Polygon	result = subject;
			for (auto &i : clipping)
				result.contours().push_back(i);
			return result;
		}
		return Polygon();
	}

	// Boolean operation is non-trivial

	// Insert all the endpoints associated to the line segments into the event queue
	for (auto &i : subject)
		for (auto &&j : i.edges())
			add(j, SUBJECT);
	for (auto &i : clipping)
		for (auto &&j : i.edges())
			add(j, CLIPPING);

	Connector connector; // to connect the edge solutions
	e_list<SweepEvent>	S;
	// for optimization 1
	const auto minmaxx = op == INTERSECTION || op == UNION ? min(subj_ext.b.v.x, clip_ext.b.v.x)
		: op == DIFF ? subj_ext.b.v.x
		: infinity;

	while (!eq.empty()) {
		SweepEvent* e = eq.pop_value();

		// optimization 1
		if (e->p.x > minmaxx) {
			if (op == UNION) {
				// add all the non-processed line segments to the result
				if (!e->left)
					connector.add(e->p, e->other->p);
				while (!eq.empty()) {
					e = eq.pop_value();
					if (!e->left)
						connector.add(e->p, e->other->p);
				}
			}
			break;
		}

		if (e->left) {
			// the line segment must be inserted into S
			auto i = lower_boundc(S, *e, [](const SweepEvent &e1, const SweepEvent &e2) { return compare_active(&e1, &e2); });
			i.insert_before(e);
			i	= e->iterator();

			// Compute the inside and in_out flags
			if (i == S.begin()) {
				// there is not a previous line segment in S
				e->inside = e->in_out = false;

			} else {
				auto prev = i;
				--prev;
				if (prev->etype != NORMAL) {
					// e overlaps with prev
					if (prev == S.begin()) {
						e->inside	= true;		// not relevant to set true or false
						e->in_out	= false;
					} else {
						// the previous two line segments in S are overlapping line segments
						auto prev2 = prev;
						--prev2;
						if (prev->ptype == e->ptype) {
							e->in_out	= !prev->in_out;
							e->inside	= !prev2->in_out;
						} else {
							e->in_out	= !prev2->in_out;
							e->inside	= !prev->in_out;
						}
					}
				} else if (e->ptype == prev->ptype) {
					// previous line segment in S belongs to the same polygon that "e" belongs to
					e->inside	= prev->inside;
					e->in_out	= !prev->in_out;
				} else {
					// previous line segment in S belongs to a different polygon that "e" belongs to
					e->inside	= !prev->in_out;
					e->in_out	= prev->inside;
				}

				// Process a possible intersection between "e" and its previous neighbor in S
				possibleIntersection(prev.get(), e);
			}

			// Process a possible intersection between "e" and its next neighbor in S
			if (++i != S.end())
				possibleIntersection(e, i.get());


		} else {
			// the line segment must be removed from S
			// Check if the line segment belongs to the Boolean operation
			auto	e2 = e->other;
			switch (e->etype) {
				case NORMAL:
					switch (op) {
						case INTERSECTION:
							if (e2->inside)
								connector.add(e->p, e2->p);
							break;
						case UNION:
							if (!e2->inside)
								connector.add(e->p, e2->p);
							break;
						case DIFF:
							if ((e->ptype == SUBJECT) ^ e2->inside)
								connector.add(e->p, e2->p);
							break;
						case XOR:
							connector.add(e->p, e2->p);
							break;
					}
					break;
				case SAME_TRANSITION:
					if (op == INTERSECTION || op == UNION)
						connector.add(e->p, e2->p);
					break;
				case DIFFERENT_TRANSITION:
					if (op == DIFF)
						connector.add(e->p, e2->p);
					break;
			}

			auto i		= e2->iterator();
			auto next	= i;
			auto prev	= i;

			// Get the next and previous line segments
			++next;
			if (prev == S.begin())
				prev = S.end();
			else
				--prev;

			// delete line segment associated to e from S and check for intersection between the neighbors of "e" in S
			S.remove(i);

			if (next != S.end() && prev != S.end())
				possibleIntersection(prev.get(), next.get());
		}
	}
	return connector;
}

#if 1//def ISO_TEST
static struct test {
	test() {
		Martinez::Polygon	p0, p1;
		p0.contours().push_back(Martinez::Contour{
			double2{ 0, 0},
			double2{10, 0},
			double2{ 0,10},
		});
		p1.contours().push_back(Martinez::Contour{
			double2{-3, 6},
			double2{ 7, 6},
			double2{-3,16},
		});

		Martinez	mart;
		auto		r = mart.compute(p0, p1, Martinez::UNION);
	}

} _test;
#endif

}