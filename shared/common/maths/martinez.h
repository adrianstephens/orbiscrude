/***************************************************************************
*   Developer: Francisco Martínez del Río (2011)                          *  
*   fmartin@ujaen.es                                                      *
*   Version: 1.4.1                                                        *
*                                                                         *
*   This is a public domain program                                       *
***************************************************************************/

#include "base/array.h"
#include "base/list.h"
#include "maths/polygon.h"
#include "allocators/pool.h"

namespace iso {

class Martinez {
public:
	enum Op { INTERSECTION, UNION, DIFF, XOR };
	typedef double2			Point;
	typedef interval<Point>	Segment;
	typedef simple_polygon<dynamic_array<Point>>	Contour;
	typedef complex_polygon<dynamic_array<Contour>> Polygon;

private:
	enum EdgeType		{ NORMAL, NON_CONTRIBUTING, SAME_TRANSITION, DIFFERENT_TRANSITION };
	enum PolygonType	{ SUBJECT, CLIPPING };

	static int	findIntersection(const Point &a0, const Point &a1, const Point &b0, const Point &b1, Point& i0, Point& i1);
	static int	sign(const Segment &seg) {
		auto	s = iso::sign(seg.b - seg.a);
		return s.x ? int(s.x) : s.y ? int(s.y) : 0;
	}

	struct Connector;

	struct SweepEvent : e_link<SweepEvent> {
		Point		p;		// point associated with the event
		PolygonType ptype;	// Polygon to which the associated segment belongs to
		EdgeType	etype;
		SweepEvent	*other; // Event associated to the other endpoint of the segment
		bool		left;	// is the point the left endpoint of the segment (p, other->p)?
		bool		in_out;	// Does the segment (p, other->p) represent an inside-outside transition in the polygon for a vertical ray from (p.x, -infinite) that crosses the segment?
		bool		inside; // Only used in left events; is the segment (p, other->p) inside the other polygon?

		SweepEvent(const Point& p, bool left, PolygonType ptype, SweepEvent *other = nullptr, EdgeType etype = NORMAL) : p(p), ptype(ptype), etype(etype), other(other), left(left) {}
		bool	below(const Point& x)	const { return left ? signed_area(p, other->p, x) > 0 : signed_area(other->p, p, x) > 0; }

		friend bool operator<(const SweepEvent& e1, const SweepEvent& e2) {
			return	e1.p.x	!= e2.p.x	? e1.p.x < e2.p.x	// Different x-coordinate
				:	e1.p.y	!= e2.p.y	? e1.p.y < e2.p.y	// Different y-coordinate
				:	e1.left	!= e2.left	? !e1.left			// The right endpoint is processed first
				:	e1.below(e2.other->p);					// bottom segment is processed first
		}

		friend bool compare(const SweepEvent* e1, const SweepEvent* e2) {
			return	e1->p.x != e2->p.x		? e1->p.x < e2->p.x // Different x-coordinate
				:	e1->p.y != e2->p.y		? e1->p.y < e2->p.y // Different y-coordinate
				:	e1->left != e2->left	? !e1->left			// The right endpoint is processed first
				:	e1->below(e2->other->p);					// bottom segment is processed first
		}

		friend bool compare_active(const SweepEvent* e1, const SweepEvent* e2) {
			if (e1 == e2)
				return false;

			return	signed_area(e1->p, e1->other->p, e2->p) == 0 && signed_area(e1->p, e1->other->p, e2->other->p) == 0
				?	(all(e1->p == e2->p)	? e1 < e2					: compare(e1, e2))												// colinear - just a consistent criterion is used
				:	(all(e1->p == e2->p)	? e1->below(e2->other->p)	: compare(e1, e2)	? e1->below(e2->p) : !e2->below(e1->p));	// If they share their left endpoint use the right endpoint to sort
		}
	};

	struct SweepEventComp {
		bool operator()(SweepEvent* e1, SweepEvent* e2) const { return compare(e1, e2); }
	};

	// Event Queue
	//growing_pool<SweepEvent>	event_pool;
	priority_queue<dynamic_array<SweepEvent*>, op_chain12<op_deref, less>> eq;
	order_array<SweepEvent>		eventHolder;

	// Compute the events associated to segment s, and insert them into pq and eq
	void add(const Segment& s, PolygonType ptype);

	// Process a posible intersection between the segment associated to the left events e1 and e2
	void possibleIntersection(SweepEvent *e0, SweepEvent *e1);
	
	// Divide the segment associated to left event e, updating pq and (implicitly) the status line
	void divide(SweepEvent *e, const Point& p);

public:
	Polygon compute(const Polygon& subject, const Polygon& clipping, Op op);
};

template<typename T, typename E, int N> auto to(const pos<E,N> &p) { return to(p.v); }

template<typename A, typename B> auto poly_bool(const simple_polygon<A>& a, const simple_polygon<B>& b, Martinez::Op op) {
	Martinez::Polygon	p0, p1;
	typedef decltype((*a.points().begin()))	TA;
	typedef decltype((*b.points().begin()))	TB;
	p0.contours().push_back(transformc(a.points(), [](TA i) { return to<double>(i); }));
	p1.contours().push_back(transformc(b.points(), [](TB i) { return to<double>(i); }));

	Martinez	mart;
	return mart.compute(p0, p1, (Martinez::Op)op);
}

template<typename C> auto operator~(const simple_polygon<C>& a) { return make_not(a); }
template<typename C> auto operator&(const simple_polygon<C>& a, const simple_polygon<C>& b) { return poly_boolop(a, b, Martinez::INTERSECTION); }
template<typename C> auto operator|(const simple_polygon<C>& a, const simple_polygon<C>& b) { return poly_boolop(a, b, Martinez::UNION); }
template<typename C> auto operator^(const simple_polygon<C>& a, const simple_polygon<C>& b) { return poly_boolop(a, b, Martinez::XOR); }
template<typename C> auto operator-(const simple_polygon<C>& a, const simple_polygon<C>& b) { return poly_boolop(a, b, Martinez::DIFF); }

}