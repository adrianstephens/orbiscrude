/*******************************************************************************
 *                                                                              *
 * Author    :  Angus Johnson                                                   *
 * Version   :  6.4.2                                                           *
 * Date      :  27 February 2017                                                *
 * Website   :  http://www.angusj.com                                           *
 * Copyright :  Angus Johnson 2010-2017                                         *
 *                                                                              *
 * License:                                                                     *
 * Use, modification & distribution is subject to Boost Software License Ver 1. *
 * http://www.boost.org/LICENSE_1_0.txt                                         *
 *                                                                              *
 * Attributions:                                                                *
 * The code in this library is an extension of Bala Vatti's clipping algorithm: *
 * "A generic solution to polygon clipping"                                     *
 * Communications of the ACM, Vol 35, Issue 7 (July 1992) pp 56-63.             *
 * http://portal.acm.org/citation.cfm?id=129906                                 *
 *                                                                              *
 * Computer graphics and geometric modeling: implementation and algorithms      *
 * By Max K. Agoston                                                            *
 * Springer; 1 edition (January 4, 2005)                                        *
 * http://books.google.com/books?q=vatti+clipping+agoston                       *
 *                                                                              *
 * See also:                                                                    *
 * "Polygon Offsetting by Computing Winding Numbers"                            *
 * Paper no. DETC2005-85513 pp. 565-575                                         *
 * ASME 2005 International Design Engineering Technical Conferences             *
 * and Computers and Information in Engineering Conference (IDETC/CIE2005)      *
 * September 24-28, 2005 , Long Beach, California, USA                          *
 * http://www.me.berkeley.edu/~mcmains/pubs/DAC05OffsetPolygon.pdf              *
 *                                                                              *
 *******************************************************************************/

#include "clipper.h"
#include "base/maths.h"

namespace ClipperLib {

static double const pi				  = 3.141592653589793238;
static double const two_pi			  = pi * 2;
static double const def_arc_tolerance = 0.25;

//------------------------------------------------------------------------------
// Miscellaneous global functions
//------------------------------------------------------------------------------

double Area(const Path& poly) {
	int size = (int)poly.size();
	if (size < 3)
		return 0;

	double a = 0;
	for (int i = 0, j = size - 1; i < size; ++i) {
		a += ((double)poly[j].x + poly[i].x) * ((double)poly[j].y - poly[i].y);
		j = i;
	}
	return -a * 0.5;
}

double Area(const OutPt* op) {
	if (!op)
		return 0;

	const OutPt* startOp = op;
	double a = 0;
	do {
		a += (double)(op->Prev->x + op->x) * (double)(op->Prev->y - op->y);
		op = op->Next;
	} while (op != startOp);
	return a * 0.5;
}

double Area(const OutRec& outRec) {
	return Area(outRec.Pts);
}

//------------------------------------------------------------------------------

// See "The point in Polygon Problem for Arbitrary Polygons" by Hormann & Agathos
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.88.5498&rep=rep1&type=pdf
int PointInPolygon(const IntPoint& pt, const Path& path) {
	// returns 0 if false, +1 if true, -1 if pt ON polygon boundary
	int	   result = 0;
	size_t cnt	  = path.size();
	if (cnt < 3)
		return 0;

	IntPoint ip = path[0];
	for (size_t i = 1; i <= cnt; ++i) {
		IntPoint ipNext = (i == cnt ? path[0] : path[i]);
		if (ipNext.y == pt.y) {
			if ((ipNext.x == pt.x) || (ip.y == pt.y && ((ipNext.x > pt.x) == (ip.x < pt.x))))
				return -1;
		}
		if ((ip.y < pt.y) != (ipNext.y < pt.y)) {
			if (ip.x >= pt.x) {
				if (ipNext.x > pt.x)
					result = 1 - result;
				else {
					double d = (double)(ip.x - pt.x) * (ipNext.y - pt.y) - (double)(ipNext.x - pt.x) * (ip.y - pt.y);
					if (!d)
						return -1;
					if ((d > 0) == (ipNext.y > ip.y))
						result = 1 - result;
				}
			} else {
				if (ipNext.x > pt.x) {
					double d = (double)(ip.x - pt.x) * (ipNext.y - pt.y) - (double)(ipNext.x - pt.x) * (ip.y - pt.y);
					if (!d)
						return -1;
					if ((d > 0) == (ipNext.y > ip.y))
						result = 1 - result;
				}
			}
		}
		ip = ipNext;
	}
	return result;
}

int PointInPolygon(const IntPoint& pt, OutPt* op) {
	// returns 0 if false, +1 if true, -1 if pt ON polygon boundary
	int	   result  = 0;
	OutPt* startOp = op;
	for (;;) {
		if (op->Next->y == pt.y) {
			if (op->Next->x == pt.x || (op->y == pt.y && ((op->Next->x > pt.x) == (op->x < pt.x))))
				return -1;
		}
		if ((op->y < pt.y) != (op->Next->y < pt.y)) {
			if (op->x >= pt.x) {
				if (op->Next->x > pt.x)
					result = 1 - result;
				else {
					double d = (double)(op->x - pt.x) * (op->Next->y - pt.y) - (double)(op->Next->x - pt.x) * (op->y - pt.y);
					if (!d)
						return -1;
					if ((d > 0) == (op->Next->y > op->y))
						result = 1 - result;
				}
			} else {
				if (op->Next->x > pt.x) {
					double d = (double)(op->x - pt.x) * (op->Next->y - pt.y) - (double)(op->Next->x - pt.x) * (op->y - pt.y);
					if (!d)
						return -1;
					if ((d > 0) == (op->Next->y > op->y))
						result = 1 - result;
				}
			}
		}
		op = op->Next;
		if (startOp == op)
			break;
	}
	return result;
}

bool Poly2ContainsPoly1(OutPt* OutPt1, OutPt* OutPt2) {
	OutPt* op = OutPt1;
	do {
		// nb: PointInPolygon returns 0 if false, +1 if true, -1 if pt on polygon
		int res = PointInPolygon(*op, OutPt2);
		if (res >= 0)
			return res > 0;
		op = op->Next;
	} while (op != OutPt1);
	return true;
}
//----------------------------------------------------------------------

bool SlopesEqual(const TEdge& e1, const TEdge& e2, bool UseFullInt64Range) {
#ifndef use_int32
	if (UseFullInt64Range)
		return fullmul(e1.top.y - e1.bot.y, e2.top.x - e2.bot.x) == fullmul(e1.top.x - e1.bot.x, e2.top.y - e2.bot.y);
	else
#endif
		return (e1.top.y - e1.bot.y) * (e2.top.x - e2.bot.x) == (e1.top.x - e1.bot.x) * (e2.top.y - e2.bot.y);
}

bool SlopesEqual(const IntPoint pt1, const IntPoint pt2, const IntPoint pt3, bool UseFullInt64Range) {
#ifndef use_int32
	if (UseFullInt64Range)
		return fullmul(pt1.y - pt2.y, pt2.x - pt3.x) == fullmul(pt1.x - pt2.x, pt2.y - pt3.y);
	else
#endif
		return (pt1.y - pt2.y) * (pt2.x - pt3.x) == (pt1.x - pt2.x) * (pt2.y - pt3.y);
}

bool SlopesEqual(const IntPoint pt1, const IntPoint pt2, const IntPoint pt3, const IntPoint pt4, bool UseFullInt64Range) {
#ifndef use_int32
	if (UseFullInt64Range)
		return fullmul(pt1.y - pt2.y, pt3.x - pt4.x) == fullmul(pt1.x - pt2.x, pt3.y - pt4.y);
	else
#endif
		return (pt1.y - pt2.y) * (pt3.x - pt4.x) == (pt1.x - pt2.x) * (pt3.y - pt4.y);
}

//------------------------------------------------------------------------------

void IntersectPoint(TEdge& Edge1, TEdge& Edge2, IntPoint& ip) {
	if (Edge1.dx == Edge2.dx) {
		ip.y = Edge1.curr.y;
		ip.x = Edge1.TopX(ip.y);
		return;
	}

	if (Edge1.dx == 0) {
		ip.x = Edge1.bot.x;
		ip.y = Edge2.IsHorizontal() ? get(Edge2.bot.y) : Round(ip.x / Edge2.dx + Edge2.bot.y - (Edge2.bot.x / Edge2.dx));
	} else if (Edge2.dx == 0) {
		ip.x = Edge2.bot.x;
		ip.y = Edge1.IsHorizontal() ? get(Edge1.bot.y) : Round(ip.x / Edge1.dx + Edge1.bot.y - (Edge1.bot.x / Edge1.dx));
	} else {
		double	b1	= Edge1.bot.x - Edge1.bot.y * Edge1.dx;
		double	b2	= Edge2.bot.x - Edge2.bot.y * Edge2.dx;
		double	q	= (b2 - b1) / (Edge1.dx - Edge2.dx);
		ip.y = Round(q);
		ip.x = abs(Edge1.dx) < abs(Edge2.dx) ? Round(Edge1.dx * q + b1) : Round(Edge2.dx * q + b2);
	}

	if (ip.y < Edge1.top.y || ip.y < Edge2.top.y) {
		ip.y = Edge1.top.y > Edge2.top.y		? Edge1.top.y : Edge2.top.y;
		ip.x = abs(Edge1.dx) < abs(Edge2.dx)	? Edge1.TopX(ip.y) : Edge2.TopX(ip.y);
	}
	// finally, don't allow 'ip' to be BELOW curr.y (ie bottom of scanbeam) ...
	if (ip.y > Edge1.curr.y) {
		ip.y = Edge1.curr.y;
		// use the more vertical edge to derive x ...
		ip.x = abs(Edge1.dx) > abs(Edge2.dx) ? Edge2.TopX(ip.y) : Edge1.TopX(ip.y);
	}
}
//------------------------------------------------------------------------------

void ReversePolyPtLinks(OutPt* pp) {
	if (OutPt *pp1 = pp) {
		do {
			OutPt *pp2	= pp1->Next;
			pp1->Next	= pp1->Prev;
			pp1->Prev	= pp2;
			pp1			= pp2;
		} while (pp1 != pp);
	}
}

//------------------------------------------------------------------------------

void DisposeOutPts(OutPt*& pp) {
	if (pp) {
		pp->Prev->Next = 0;
		while (pp) {
			OutPt* t = pp;
			pp = pp->Next;
			delete t;
		}
	}
}

//------------------------------------------------------------------------------

TEdge* RemoveEdge(TEdge* e) {
	// removes e from double_linked_list (but without removing from memory)
	e->Prev->Next = e->Next;
	e->Next->Prev = e->Prev;
	TEdge* result = e->Next;
	e->Prev		  = 0;	// flag as removed (see ClipperBase.Clear)
	return result;
}

//------------------------------------------------------------------------------

bool GetOverlapSegment(IntPoint pt1a, IntPoint pt1b, IntPoint pt2a, IntPoint pt2b, IntPoint& pt1, IntPoint& pt2) {
	// precondition: segments are Collinear.
	if (abs(pt1a.x - pt1b.x) > abs(pt1a.y - pt1b.y)) {
		if (pt1a.x > pt1b.x)
			swap(pt1a, pt1b);
		if (pt2a.x > pt2b.x)
			swap(pt2a, pt2b);
		if (pt1a.x > pt2a.x)
			pt1 = pt1a;
		else
			pt1 = pt2a;
		if (pt1b.x < pt2b.x)
			pt2 = pt1b;
		else
			pt2 = pt2b;
		return pt1.x < pt2.x;
	} else {
		if (pt1a.y < pt1b.y)
			swap(pt1a, pt1b);
		if (pt2a.y < pt2b.y)
			swap(pt2a, pt2b);
		if (pt1a.y < pt2a.y)
			pt1 = pt1a;
		else
			pt1 = pt2a;
		if (pt1b.y > pt2b.y)
			pt2 = pt1b;
		else
			pt2 = pt2b;
		return pt1.y > pt2.y;
	}
}
//------------------------------------------------------------------------------

inline double GetDx(const IntPoint pt1, const IntPoint pt2) { return pt1.y == pt2.y ? HORIZONTAL : (double)(pt2.x - pt1.x) / (pt2.y - pt1.y); }

bool FirstIsBottomPt(const OutPt* btmPt1, const OutPt* btmPt2) {
	OutPt* p = btmPt1->Prev;
	while (all(*p == *btmPt1) && p != btmPt1)
		p = p->Prev;
	double dx1p = abs(GetDx(*btmPt1, *p));

	p			= btmPt1->Next;
	while (all(*p == *btmPt1) && p != btmPt1)
		p = p->Next;
	double dx1n = abs(GetDx(*btmPt1, *p));

	p = btmPt2->Prev;
	while (all(*p == *btmPt2) && p != btmPt2)
		p = p->Prev;
	double dx2p = abs(GetDx(*btmPt2, *p));

	p			= btmPt2->Next;
	while (all(*p == *btmPt2) && p != btmPt2)
		p = p->Next;
	double dx2n = abs(GetDx(*btmPt2, *p));

	return max(dx1p, dx1n) == max(dx2p, dx2n) && min(dx1p, dx1n) == min(dx2p, dx2n)
		? Area(btmPt1) > 0  // if otherwise identical use orientation
		: (dx1p >= dx2p && dx1p >= dx2n) || (dx1n >= dx2p && dx1n >= dx2n);
}

//------------------------------------------------------------------------------

OutPt* GetBottomPt(OutPt* pp) {
	OutPt* dups = 0;
	OutPt* p	= pp->Next;
	while (p != pp) {
		if (p->y > pp->y) {
			pp	 = p;
			dups = 0;
		} else if (p->y == pp->y && p->x <= pp->x) {
			if (p->x < pp->x) {
				dups = 0;
				pp	 = p;
			} else {
				if (p->Next != pp && p->Prev != pp)
					dups = p;
			}
		}
		p = p->Next;
	}
	if (dups) {
		// there appear to be at least 2 vertices at BottomPt so ...
		while (dups != p) {
			if (!FirstIsBottomPt(p, dups))
				pp = dups;
			dups = dups->Next;
			while (any(*dups != *pp))
				dups = dups->Next;
		}
	}
	return pp;
}
//------------------------------------------------------------------------------

bool Pt2IsBetweenPt1AndPt3(const IntPoint pt1, const IntPoint pt2, const IntPoint pt3) {
	if (all(pt1 == pt3) || all(pt1 == pt2) || all(pt3 == pt2))
		return false;
	else if (pt1.x != pt3.x)
		return (pt2.x > pt1.x) == (pt2.x < pt3.x);
	else
		return (pt2.y > pt1.y) == (pt2.y < pt3.y);
}
//------------------------------------------------------------------------------

bool HorzSegmentsOverlap(cInt seg1a, cInt seg1b, cInt seg2a, cInt seg2b) {
	if (seg1a > seg1b)
		swap(seg1a, seg1b);
	if (seg2a > seg2b)
		swap(seg2a, seg2b);
	return (seg1a < seg2b) && (seg2a < seg1b);
}

//------------------------------------------------------------------------------
// ClipperBase class methods ...
//------------------------------------------------------------------------------

bool RangeTest(const IntPoint& Pt, bool& useFullRange) {
	if (useFullRange)
		return Pt.x <= hiRange && Pt.y <= hiRange && -Pt.x <= hiRange && -Pt.y <= hiRange;

	if (Pt.x > loRange || Pt.y > loRange || -Pt.x > loRange || -Pt.y > loRange) {
		useFullRange = true;
		return RangeTest(Pt, useFullRange);
	}
	return true;
}
//------------------------------------------------------------------------------

TEdge* FindNextLocMin(TEdge* E) {
	for (;;) {
		while (any(E->bot != E->Prev->bot) || all(E->curr == E->top))
			E = E->Next;

		if (!E->IsHorizontal() && !E->Prev->IsHorizontal())
			return E;

		while (E->Prev->IsHorizontal())
			E = E->Prev;

		TEdge* E2 = E;
		while (E->IsHorizontal())
			E = E->Next;

		if (E->top.y != E->Prev->bot.y) // ie just an intermediate horz.
			return E2->Prev->bot.x < E->bot.x ? E2 : E;
	}
}
//------------------------------------------------------------------------------

TEdge* ClipperBase::ProcessBound(TEdge* E, bool NextIsForward) {
	TEdge* Result = E;
	TEdge* Horz	  = 0;

	if (E->OutIdx == TEdge::Skip) {
		// if edges still remain in the current bound beyond the skip edge then create another LocMin and call ProcessBound once more
		if (NextIsForward) {
			while (E->top.y == E->Next->bot.y)
				E = E->Next;
			// don't include top horizontals when parsing a bound a second time, they will be contained in the opposite bound ...
			while (E != Result && E->IsHorizontal())
				E = E->Prev;
		} else {
			while (E->top.y == E->Prev->bot.y)
				E = E->Prev;
			while (E != Result && E->IsHorizontal())
				E = E->Next;
		}

		if (E == Result) {
			Result = NextIsForward ? E->Next : E->Prev;

		} else {
			// there are more edges in the bound beyond result starting with E
			E = NextIsForward ? Result->Next : Result->Prev;
			LocalMinimum locMin(E->bot.y, 0, E);
			E->WindDelta	  = 0;
			Result			  = ProcessBound(E, NextIsForward);
			minima.push_back(locMin);
		}
		return Result;
	}

	TEdge* EStart;

	if (E->IsHorizontal()) {
		// We need to be careful with open paths because this may not be a true local minima (ie E may be following a skip edge).
		// Also, consecutive horz. edges may start heading left before going right.
		if (NextIsForward)
			EStart = E->Prev;
		else
			EStart = E->Next;
		if (EStart->IsHorizontal()) {	// ie an adjoining horizontal skip edge
			if (EStart->bot.x != E->bot.x && EStart->top.x != E->bot.x)
				swap(E->top.x, E->bot.x);
		} else if (EStart->bot.x != E->bot.x)
			swap(E->top.x, E->bot.x);
	}

	EStart = E;
	if (NextIsForward) {
		while (Result->top.y == Result->Next->bot.y && Result->Next->OutIdx != TEdge::Skip)
			Result = Result->Next;
		if (Result->IsHorizontal() && Result->Next->OutIdx != TEdge::Skip) {
			// nb: at the top of a bound, horizontals are added to the bound only when the preceding edge attaches to the horizontal's left vertex unless a TEdge::Skip edge is encountered when that becomes the top divide
			Horz = Result;
			while (Horz->Prev->IsHorizontal())
				Horz = Horz->Prev;
			if (Horz->Prev->top.x > Result->Next->top.x)
				Result = Horz->Prev;
		}
		while (E != Result) {
			E->NextInLML = E->Next;
			if (E->IsHorizontal() && E != EStart && E->bot.x != E->Prev->top.x)
				swap(E->top.x, E->bot.x);
			E = E->Next;
		}
		if (E->IsHorizontal() && E != EStart && E->bot.x != E->Prev->top.x)
			swap(E->top.x, E->bot.x);
		Result = Result->Next;	// move to the edge just beyond current bound
	} else {
		while (Result->top.y == Result->Prev->bot.y && Result->Prev->OutIdx != TEdge::Skip)
			Result = Result->Prev;
		if (Result->IsHorizontal() && Result->Prev->OutIdx != TEdge::Skip) {
			Horz = Result;
			while (Horz->Next->IsHorizontal())
				Horz = Horz->Next;
			if (Horz->Next->top.x == Result->Prev->top.x || Horz->Next->top.x > Result->Prev->top.x)
				Result = Horz->Next;
		}

		while (E != Result) {
			E->NextInLML = E->Prev;
			if (E->IsHorizontal() && E != EStart && E->bot.x != E->Next->top.x)
				swap(E->top.x, E->bot.x);
			E = E->Prev;
		}
		if (E->IsHorizontal() && E != EStart && E->bot.x != E->Next->top.x)
			swap(E->top.x, E->bot.x);
		Result = Result->Prev;	// move to the edge just beyond current bound
	}

	return Result;
}
//------------------------------------------------------------------------------

bool ClipperBase::AddPath(const Path& pg, PolyType type, bool Closed) {
#ifdef use_lines
	if (!Closed && type == ptClip)
		return false;//throw clipperException("AddPath: Open paths must be subject.");
#else
	if (!Closed)
		throw clipperException("AddPath: Open paths have been disabled.");
#endif

	int highI = (int)pg.size() - 1;
	if (Closed)
		while (highI > 0 && all(pg[highI] == pg[0]))
			--highI;
	while (highI > 0 && all(pg[highI] == pg[highI - 1]))
		--highI;
	if ((Closed && highI < 2) || (!Closed && highI < 1))
		return false;

	// create a new edge array ...
	TEdge* edges = new TEdge[highI + 1];

	bool IsFlat = true;
	// 1. Basic (first) edge initialization ...
	edges[1].curr = pg[1];
	RangeTest(pg[0], use_full_range);
	RangeTest(pg[highI], use_full_range);
	edges[0].InitEdge(&edges[1], &edges[highI], pg[0]);
	edges[highI].InitEdge(&edges[0], &edges[highI - 1], pg[highI]);
	for (int i = highI - 1; i >= 1; --i) {
		if (!RangeTest(pg[i], use_full_range)) {
			delete[] edges;
			return false;
		}
		edges[i].InitEdge(&edges[i + 1], &edges[i - 1], pg[i]);
	}
	TEdge* eStart = &edges[0];

	// 2. Remove duplicate vertices, and (when closed) collinear edges ...
	TEdge *E = eStart, *eLoopStop = eStart;
	for (;;) {
		// nb: allows matching start and end points when not Closed ...
		if (all(E->curr == E->Next->curr) && (Closed || E->Next != eStart)) {
			if (E == E->Next)
				break;
			if (E == eStart)
				eStart = E->Next;
			E		  = RemoveEdge(E);
			eLoopStop = E;
			continue;
		}
		if (E->Prev == E->Next)
			break;	// only two vertices
		else if (Closed && SlopesEqual(E->Prev->curr, E->curr, E->Next->curr, use_full_range) && (!preserve_colinear || !Pt2IsBetweenPt1AndPt3(E->Prev->curr, E->curr, E->Next->curr))) {
			// Collinear edges are allowed for open paths but in closed paths the default is to merge adjacent collinear edges into a single edge
			// However, if the PreserveCollinear property is enabled, only overlapping collinear edges (ie spikes) will be removed from closed paths.
			if (E == eStart)
				eStart = E->Next;
			E		  = RemoveEdge(E);
			E		  = E->Prev;
			eLoopStop = E;
			continue;
		}
		E = E->Next;
		if ((E == eLoopStop) || (!Closed && E->Next == eStart))
			break;
	}

	if ((!Closed && (E == E->Next)) || (Closed && (E->Prev == E->Next))) {
		delete[] edges;
		return false;
	}

	if (!Closed) {
		has_open_paths		 = true;
		eStart->Prev->OutIdx = TEdge::Skip;
	}

	// 3. Do second stage of edge initialization ...
	E = eStart;
	do {
		E->InitEdge2(type);
		E = E->Next;
		if (IsFlat && E->curr.y != eStart->curr.y)
			IsFlat = false;
	} while (E != eStart);

	// 4. Finally, add edge bounds to LocalMinima list ...

	// Totally flat paths must be handled differently when adding them to LocalMinima list to avoid endless loops etc ...
	if (IsFlat) {
		if (Closed) {
			delete[] edges;
			return false;
		}
		E->Prev->OutIdx = TEdge::Skip;
		LocalMinimum locMin(E->bot.y, 0, E);
		locMin.RightBound->side		 = esRight;
		locMin.RightBound->WindDelta = 0;
		for (;;) {
			if (E->bot.x != E->Prev->top.x)
				swap(E->top.x, E->bot.x);
			if (E->Next->OutIdx == TEdge::Skip)
				break;
			E->NextInLML = E->Next;
			E			 = E->Next;
		}
		minima.push_back(locMin);
		m_edges.push_back(edges);
		return true;
	}

	m_edges.push_back(edges);
	TEdge* EMin = 0;

	// workaround to avoid an endless loop in the while loop below when open paths have matching start and end points ...
	if (all(E->Prev->bot == E->Prev->top))
		E = E->Next;

	for (;;) {
		E = FindNextLocMin(E);
		if (E == EMin)
			break;
		else if (!EMin)
			EMin = E;

		// E and E.Prev now share a local minima (left aligned if horizontal)
		// Compare their slopes to find which starts which bound ...

		bool	leftBoundIsForward = E->dx >= E->Prev->dx;
		LocalMinimum locMin = leftBoundIsForward ? LocalMinimum(E->bot.y, E, E->Prev) : LocalMinimum(E->bot.y, E->Prev, E);

		if (!Closed)
			locMin.LeftBound->WindDelta = 0;
		else if (locMin.LeftBound->Next == locMin.RightBound)
			locMin.LeftBound->WindDelta = -1;
		else
			locMin.LeftBound->WindDelta = 1;
		locMin.RightBound->WindDelta = -locMin.LeftBound->WindDelta;

		E = ProcessBound(locMin.LeftBound, leftBoundIsForward);
		if (E->OutIdx == TEdge::Skip)
			E = ProcessBound(E, leftBoundIsForward);

		TEdge* E2 = ProcessBound(locMin.RightBound, !leftBoundIsForward);
		if (E2->OutIdx == TEdge::Skip)
			E2 = ProcessBound(E2, !leftBoundIsForward);

		if (locMin.LeftBound->OutIdx == TEdge::Skip)
			locMin.LeftBound = 0;
		else if (locMin.RightBound->OutIdx == TEdge::Skip)
			locMin.RightBound = 0;
		minima.push_back(locMin);
		if (!leftBoundIsForward)
			E = E2;
	}
	return true;
}
//------------------------------------------------------------------------------

bool ClipperBase::AddPaths(const Paths& ppg, PolyType type, bool Closed) {
	bool result = false;
	for (auto &i : ppg)
		if (AddPath(i, type, Closed))
			result = true;
	return result;
}
//------------------------------------------------------------------------------

void ClipperBase::Clear() {
	DisposeLocalMinimaList();
	for (auto i : m_edges) {
		delete[] i;
	}
	m_edges.clear();
	use_full_range = false;
	has_open_paths = false;
}
//------------------------------------------------------------------------------

void ClipperBase::Reset() {
	local_min = minima.begin();
	if (local_min == minima.end())
		return;	 // ie nothing to process

	sort(minima.begin(), minima.end(), [](const LocalMinimum& locMin1, const LocalMinimum& locMin2) { return locMin2.y < locMin1.y; });

	scanbeam.clear();
	// reset all edges ...
	for (auto lm = minima.begin(); lm != minima.end(); ++lm) {
		InsertScanbeam(lm->y);
		TEdge* e = lm->LeftBound;
		if (e) {
			e->curr	  = e->bot;
			e->side	  = esLeft;
			e->OutIdx = TEdge::Unassigned;
		}

		e = lm->RightBound;
		if (e) {
			e->curr	  = e->bot;
			e->side	  = esRight;
			e->OutIdx = TEdge::Unassigned;
		}
	}
	aet = 0;
	local_min	  = minima.begin();
}
//------------------------------------------------------------------------------

void ClipperBase::DisposeLocalMinimaList() {
	minima.clear();
	local_min = minima.begin();
}
//------------------------------------------------------------------------------

bool ClipperBase::PopLocalMinima(cInt y, const LocalMinimum*& locMin) {
	if (local_min == minima.end() || (*local_min).y != y)
		return false;
	locMin = &(*local_min);
	++local_min;
	return true;
}
//------------------------------------------------------------------------------

IntRect ClipperBase::GetBounds() {
	IntRect		result;
	auto		lm = minima.begin();
	if (lm == minima.end()) {
		result.left = result.top = result.right = result.bottom = 0;
		return result;
	}
	result.left	  = lm->LeftBound->bot.x;
	result.top	  = lm->LeftBound->bot.y;
	result.right  = lm->LeftBound->bot.x;
	result.bottom = lm->LeftBound->bot.y;
	while (lm != minima.end()) {
		// todo - needs fixing for open paths
		result.bottom = max(result.bottom, lm->LeftBound->bot.y);
		TEdge* e	  = lm->LeftBound;
		for (;;) {
			TEdge* bottomE = e;
			while (e->NextInLML) {
				if (e->bot.x < result.left)
					result.left = e->bot.x;
				if (e->bot.x > result.right)
					result.right = e->bot.x;
				e = e->NextInLML;
			}
			result.left	 = min(result.left, e->bot.x);
			result.right = max(result.right, e->bot.x);
			result.left	 = min(result.left, e->top.x);
			result.right = max(result.right, e->top.x);
			result.top	 = min(result.top, e->top.y);
			if (bottomE == lm->LeftBound)
				e = lm->RightBound;
			else
				break;
		}
		++lm;
	}
	return result;
}
//------------------------------------------------------------------------------

void ClipperBase::InsertScanbeam(const cInt y) { scanbeam.push(y); }
//------------------------------------------------------------------------------

bool ClipperBase::PopScanbeam(cInt& y) {
	if (scanbeam.empty())
		return false;
	y = scanbeam.top();
	scanbeam.pop();
	while (!scanbeam.empty() && y == scanbeam.top())
		scanbeam.pop(); // Pop duplicates.
	return true;
}
//------------------------------------------------------------------------------

void ClipperBase::DisposeAllOutRecs() {
	for (auto &i : poly_outs) {
		if (i->Pts)
			DisposeOutPts(i->Pts);
		delete i;
		i = 0;
	}
	poly_outs.clear();
}

//------------------------------------------------------------------------------

void ClipperBase::DeleteFromAEL(TEdge* e) {
	TEdge* AelPrev = e->PrevInAEL;
	TEdge* AelNext = e->NextInAEL;
	if (!AelPrev && !AelNext && (e != aet))
		return;	 // already deleted
	if (AelPrev)
		AelPrev->NextInAEL = AelNext;
	else
		aet = AelNext;
	if (AelNext)
		AelNext->PrevInAEL = AelPrev;
	e->NextInAEL = 0;
	e->PrevInAEL = 0;
}
//------------------------------------------------------------------------------

OutRec* ClipperBase::CreateOutRec() {
	OutRec* result	  = new OutRec;
	result->IsHole	  = false;
	result->is_open	  = false;
	result->FirstLeft = 0;
	result->Pts		  = 0;
	result->BottomPt  = 0;
	result->PolyNd	  = 0;
	poly_outs.push_back(result);
	result->index = (int)poly_outs.size() - 1;
	return result;
}
//------------------------------------------------------------------------------

void ClipperBase::SwapPositionsInAEL(TEdge* Edge1, TEdge* Edge2) {
	// check that one or other edge hasn't already been removed from AEL ...
	if (Edge1->NextInAEL == Edge1->PrevInAEL || Edge2->NextInAEL == Edge2->PrevInAEL)
		return;

	if (Edge1->NextInAEL == Edge2) {
		TEdge* Next = Edge2->NextInAEL;
		if (Next)
			Next->PrevInAEL = Edge1;
		TEdge* Prev = Edge1->PrevInAEL;
		if (Prev)
			Prev->NextInAEL = Edge2;
		Edge2->PrevInAEL = Prev;
		Edge2->NextInAEL = Edge1;
		Edge1->PrevInAEL = Edge2;
		Edge1->NextInAEL = Next;
	} else if (Edge2->NextInAEL == Edge1) {
		TEdge* Next = Edge1->NextInAEL;
		if (Next)
			Next->PrevInAEL = Edge2;
		TEdge* Prev = Edge2->PrevInAEL;
		if (Prev)
			Prev->NextInAEL = Edge1;
		Edge1->PrevInAEL = Prev;
		Edge1->NextInAEL = Edge2;
		Edge2->PrevInAEL = Edge1;
		Edge2->NextInAEL = Next;
	} else {
		TEdge* Next		 = Edge1->NextInAEL;
		TEdge* Prev		 = Edge1->PrevInAEL;
		Edge1->NextInAEL = Edge2->NextInAEL;
		if (Edge1->NextInAEL)
			Edge1->NextInAEL->PrevInAEL = Edge1;
		Edge1->PrevInAEL = Edge2->PrevInAEL;
		if (Edge1->PrevInAEL)
			Edge1->PrevInAEL->NextInAEL = Edge1;
		Edge2->NextInAEL = Next;
		if (Edge2->NextInAEL)
			Edge2->NextInAEL->PrevInAEL = Edge2;
		Edge2->PrevInAEL = Prev;
		if (Edge2->PrevInAEL)
			Edge2->PrevInAEL->NextInAEL = Edge2;
	}

	if (!Edge1->PrevInAEL)
		aet = Edge1;
	else if (!Edge2->PrevInAEL)
		aet = Edge2;
}
//------------------------------------------------------------------------------

bool ClipperBase::UpdateEdgeIntoAEL(TEdge*& e) {
	if (!e->NextInLML)
		return false;//throw clipperException("UpdateEdgeIntoAEL: invalid call");

	e->NextInLML->OutIdx = e->OutIdx;
	TEdge* AelPrev		 = e->PrevInAEL;
	TEdge* AelNext		 = e->NextInAEL;
	if (AelPrev)
		AelPrev->NextInAEL = e->NextInLML;
	else
		aet = e->NextInLML;
	if (AelNext)
		AelNext->PrevInAEL = e->NextInLML;
	e->NextInLML->side		= e->side;
	e->NextInLML->WindDelta = e->WindDelta;
	e->NextInLML->wind	= e->wind;
	e->NextInLML->wind2	= e->wind2;
	e						= e->NextInLML;
	e->curr					= e->bot;
	e->PrevInAEL			= AelPrev;
	e->NextInAEL			= AelNext;
	if (!e->IsHorizontal())
		InsertScanbeam(e->top.y);
	return true;
}

//------------------------------------------------------------------------------
// Clipper methods ...
//------------------------------------------------------------------------------

Paths Clipper::Execute(ClipType clipType, PolyFillType subjFillType, PolyFillType clipFillType) {
	Paths	solution;
	if (!has_open_paths) {
		subj_fill_type	= subjFillType;
		clip_fill_type	= clipFillType;
		op				= clipType;
		using_polytree = false;
		if (ExecuteInternal())
			BuildResult(solution);
		DisposeAllOutRecs();
	}
	return solution;
}

PolyTree Clipper::ExecuteTree(ClipType clipType, PolyFillType subjFillType, PolyFillType clipFillType) {
	PolyTree	polytree;
	subj_fill_type	= subjFillType;
	clip_fill_type	= clipFillType;
	op		= clipType;
	using_polytree = true;
	bool succeeded	= ExecuteInternal();
	if (succeeded)
		BuildResult2(polytree);
	DisposeAllOutRecs();
	return polytree;
}

void Clipper::FixHoleLinkage(OutRec& outrec) {
	// skip OutRecs that (a) contain outermost polygons or (b) already have the correct owner/child linkage ...
	if (outrec.FirstLeft && (outrec.IsHole == outrec.FirstLeft->IsHole || !outrec.FirstLeft->Pts)) {
		OutRec* orfl = outrec.FirstLeft;
		while (orfl && (orfl->IsHole == outrec.IsHole || !orfl->Pts))
			orfl = orfl->FirstLeft;
		outrec.FirstLeft = orfl;
	}
}

bool Clipper::ExecuteInternal() {
	bool succeeded = true;

	Reset();
	maxima.clear();
	sorted_edges = 0;

	succeeded = true;
	cInt botY, topY;
	if (!PopScanbeam(botY))
		return false;

	InsertLocalMinimaIntoAEL(botY);
	while (PopScanbeam(topY) || LocalMinimaPending()) {
		if (!ProcessHorizontals()) {
			succeeded = false;
			break;
		}
		ghost_joins.clear();
		if (!ProcessIntersections(topY)) {
			succeeded = false;
			break;
		}
		if (!ProcessEdgesAtTopOfScanbeam(topY))
			return false;

		botY = topY;
		InsertLocalMinimaIntoAEL(botY);
	}

	if (succeeded) {
		// fix orientations ...
		for (auto i : poly_outs) {
			if (!i->Pts || i->is_open)
				continue;
			if ((i->IsHole ^ reverse_output) == (Area(*i) > 0))
				ReversePolyPtLinks(i->Pts);
		}

		if (!joins.empty())
			JoinCommonEdges();

		// unfortunately FixupOutPolygon() must be done after JoinCommonEdges()
		for (auto i : poly_outs) {
			if (!i->Pts)
				continue;
			if (i->is_open)
				FixupOutPolyline(*i);
			else
				FixupOutPolygon(*i);
		}

		if (strict_simple)
			DoSimplePolygons();
	}

	joins.clear();
	ghost_joins.clear();
	return succeeded;
}
//------------------------------------------------------------------------------

void Clipper::SetWindingCount(TEdge& edge) {
	TEdge* e = edge.PrevInAEL;
	// find the edge of the same polytype that immediately preceeds 'edge' in AEL
	while (e && ((e->type != edge.type) || (e->WindDelta == 0)))
		e = e->PrevInAEL;
	if (!e) {
		if (edge.WindDelta == 0) {
			PolyFillType pft = (edge.type == ptSubject ? subj_fill_type : clip_fill_type);
			edge.wind	 = (pft == pftNegative ? -1 : 1);
		} else
			edge.wind = edge.WindDelta;
		edge.wind2 = 0;
		e			  = aet;	// ie get ready to calc wind2
	} else if (edge.WindDelta == 0 && op != ctUnion) {
		edge.wind  = 1;
		edge.wind2 = e->wind2;
		e			  = e->NextInAEL;  // ie get ready to calc wind2
	} else if (IsEvenOddFillType(edge)) {
		// EvenOdd filling ...
		if (edge.WindDelta == 0) {
			// are we inside a subj polygon ...
			bool   Inside = true;
			TEdge* e2	  = e->PrevInAEL;
			while (e2) {
				if (e2->type == e->type && e2->WindDelta != 0)
					Inside = !Inside;
				e2 = e2->PrevInAEL;
			}
			edge.wind = (Inside ? 0 : 1);
		} else {
			edge.wind = edge.WindDelta;
		}
		edge.wind2 = e->wind2;
		e			  = e->NextInAEL;  // ie get ready to calc wind2
	} else {
		// nonZero, Positive or Negative filling ...
		if (e->wind * e->WindDelta < 0) {
			// prev edge is 'decreasing' WindCount (WC) toward zero
			// so we're outside the previous polygon ...
			if (abs(e->wind) > 1) {
				// outside prev poly but still inside another.
				// when reversing direction of prev poly use the same WC
				if (e->WindDelta * edge.WindDelta < 0)
					edge.wind = e->wind;
				// otherwise continue to 'decrease' WC ...
				else
					edge.wind = e->wind + edge.WindDelta;
			} else
				// now outside all polys of same polytype so set own WC ...
				edge.wind = (edge.WindDelta == 0 ? 1 : edge.WindDelta);
		} else {
			// prev edge is 'increasing' WindCount (WC) away from zero
			// so we're inside the previous polygon ...
			if (edge.WindDelta == 0)
				edge.wind = (e->wind < 0 ? e->wind - 1 : e->wind + 1);
			// if wind direction is reversing prev then use same WC
			else if (e->WindDelta * edge.WindDelta < 0)
				edge.wind = e->wind;
			// otherwise add to WC ...
			else
				edge.wind = e->wind + edge.WindDelta;
		}
		edge.wind2 = e->wind2;
		e			  = e->NextInAEL;  // ie get ready to calc wind2
	}

	// update wind2 ...
	if (IsEvenOddAltFillType(edge)) {
		// EvenOdd filling ...
		while (e != &edge) {
			if (e->WindDelta != 0)
				edge.wind2 = (edge.wind2 == 0 ? 1 : 0);
			e = e->NextInAEL;
		}
	} else {
		// nonZero, Positive or Negative filling ...
		while (e != &edge) {
			edge.wind2 += e->WindDelta;
			e = e->NextInAEL;
		}
	}
}

//------------------------------------------------------------------------------

bool Clipper::IsContributing(const TEdge& edge) const {
	PolyFillType pft, pft2;
	if (edge.type == ptSubject) {
		pft	 = subj_fill_type;
		pft2 = clip_fill_type;
	} else {
		pft	 = clip_fill_type;
		pft2 = subj_fill_type;
	}

	switch (pft) {
		case pftEvenOdd:
			// return false if a subj line has been flagged as inside a subj polygon
			if (edge.WindDelta == 0 && edge.wind != 1)
				return false;
			break;
		case pftNonZero:
			if (abs(edge.wind) != 1)
				return false;
			break;
		case pftPositive:
			if (edge.wind != 1)
				return false;
			break;
		default:  // pftNegative
			if (edge.wind != -1)
				return false;
	}

	switch (op) {
		case ctIntersection:
			switch (pft2) {
				case pftEvenOdd:
				case pftNonZero: return (edge.wind2 != 0);
				case pftPositive: return (edge.wind2 > 0);
				default: return (edge.wind2 < 0);
			}
			break;
		case ctUnion:
			switch (pft2) {
				case pftEvenOdd:
				case pftNonZero: return (edge.wind2 == 0);
				case pftPositive: return (edge.wind2 <= 0);
				default: return (edge.wind2 >= 0);
			}
			break;
		case ctDifference:
			if (edge.type == ptSubject)
				switch (pft2) {
					case pftEvenOdd:
					case pftNonZero: return (edge.wind2 == 0);
					case pftPositive: return (edge.wind2 <= 0);
					default: return (edge.wind2 >= 0);
				}
			else
				switch (pft2) {
					case pftEvenOdd:
					case pftNonZero: return (edge.wind2 != 0);
					case pftPositive: return (edge.wind2 > 0);
					default: return (edge.wind2 < 0);
				}
			break;
		case ctXor:
			if (edge.WindDelta == 0)  // XOr always contributing unless open
				switch (pft2) {
					case pftEvenOdd:
					case pftNonZero: return (edge.wind2 == 0);
					case pftPositive: return (edge.wind2 <= 0);
					default: return (edge.wind2 >= 0);
				}
			else
				return true;
			break;
		default: return true;
	}
}
//------------------------------------------------------------------------------

OutPt* Clipper::AddLocalMinPoly(TEdge* e1, TEdge* e2, const IntPoint& Pt) {
	OutPt* result;
	TEdge *e, *prevE;
	if (e2->IsHorizontal() || (e1->dx > e2->dx)) {
		result	   = AddOutPt(e1, Pt);
		e2->OutIdx = e1->OutIdx;
		e1->side   = esLeft;
		e2->side   = esRight;
		e		   = e1;
		if (e->PrevInAEL == e2)
			prevE = e2->PrevInAEL;
		else
			prevE = e->PrevInAEL;
	} else {
		result	   = AddOutPt(e2, Pt);
		e1->OutIdx = e2->OutIdx;
		e1->side   = esRight;
		e2->side   = esLeft;
		e		   = e2;
		if (e->PrevInAEL == e1)
			prevE = e1->PrevInAEL;
		else
			prevE = e->PrevInAEL;
	}

	if (prevE && prevE->OutIdx >= 0 && prevE->top.y < Pt.y && e->top.y < Pt.y) {
		cInt xPrev = prevE->TopX(Pt.y);
		cInt xE	   = e->TopX(Pt.y);
		if (xPrev == xE && (e->WindDelta != 0) && (prevE->WindDelta != 0) && SlopesEqual(IntPoint(xPrev, Pt.y), prevE->top, IntPoint(xE, Pt.y), e->top, use_full_range)) {
			OutPt* outPt = AddOutPt(prevE, Pt);
			AddJoin(result, outPt, e->top);
		}
	}
	return result;
}
//------------------------------------------------------------------------------

void Clipper::AddLocalMaxPoly(TEdge* e1, TEdge* e2, const IntPoint& Pt) {
	AddOutPt(e1, Pt);
	if (e2->WindDelta == 0)
		AddOutPt(e2, Pt);
	if (e1->OutIdx == e2->OutIdx) {
		e1->OutIdx = TEdge::Unassigned;
		e2->OutIdx = TEdge::Unassigned;
	} else if (e1->OutIdx < e2->OutIdx) {
		AppendPolygon(e1, e2);
	} else {
		AppendPolygon(e2, e1);
	}
}
//------------------------------------------------------------------------------

void Clipper::AddEdgeToSEL(TEdge* edge) {
	// SEL pointers in PEdge are reused to build a list of horizontal edges.
	// However, we don't need to worry about order with horizontal edge processing.
	if (!sorted_edges) {
		sorted_edges			= edge;
		edge->PrevInSEL			= 0;
		edge->NextInSEL			= 0;
	} else {
		edge->NextInSEL			= sorted_edges;
		edge->PrevInSEL			= 0;
		sorted_edges->PrevInSEL = edge;
		sorted_edges			= edge;
	}
}
//------------------------------------------------------------------------------

bool Clipper::PopEdgeFromSEL(TEdge*& edge) {
	if (!sorted_edges)
		return false;
	edge = sorted_edges;
	DeleteFromSEL(sorted_edges);
	return true;
}
//------------------------------------------------------------------------------

void Clipper::CopyAELToSEL() {
	TEdge* e	  = aet;
	sorted_edges = e;
	while (e) {
		e->PrevInSEL = e->PrevInAEL;
		e->NextInSEL = e->NextInAEL;
		e			 = e->NextInAEL;
	}
}

//------------------------------------------------------------------------------

void Clipper::InsertLocalMinimaIntoAEL(const cInt botY) {
	const LocalMinimum* lm;
	while (PopLocalMinima(botY, lm)) {
		TEdge* lb = lm->LeftBound;
		TEdge* rb = lm->RightBound;

		OutPt* Op1 = 0;
		if (!lb) {
			// nb: don't insert LB into either AEL or SEL
			InsertEdgeIntoAEL(rb, 0);
			SetWindingCount(*rb);
			if (IsContributing(*rb))
				Op1 = AddOutPt(rb, rb->bot);
		} else if (!rb) {
			InsertEdgeIntoAEL(lb, 0);
			SetWindingCount(*lb);
			if (IsContributing(*lb))
				Op1 = AddOutPt(lb, lb->bot);
			InsertScanbeam(lb->top.y);
		} else {
			InsertEdgeIntoAEL(lb, 0);
			InsertEdgeIntoAEL(rb, lb);
			SetWindingCount(*lb);
			rb->wind	= lb->wind;
			rb->wind2	= lb->wind2;
			if (IsContributing(*lb))
				Op1 = AddLocalMinPoly(lb, rb, lb->bot);
			InsertScanbeam(lb->top.y);
		}

		if (rb) {
			if (rb->IsHorizontal()) {
				AddEdgeToSEL(rb);
				if (rb->NextInLML)
					InsertScanbeam(rb->NextInLML->top.y);
			} else
				InsertScanbeam(rb->top.y);
		}

		if (!lb || !rb)
			continue;

		// if any output polygons share an edge, they'll need joining later ...
		if (Op1 && rb->IsHorizontal() && ghost_joins.size() > 0 && (rb->WindDelta != 0)) {
			for (auto &i : ghost_joins) {
				// if the horizontal Rb and a 'ghost' horizontal overlap, then convert the 'ghost' join to a real join ready for later ...
				if (HorzSegmentsOverlap(i.OutPt1->x, i.x, rb->bot.x, rb->top.x))
					AddJoin(i.OutPt1, Op1, i);
			}
		}

		if (lb->OutIdx >= 0 && lb->PrevInAEL && lb->PrevInAEL->curr.x == lb->bot.x && lb->PrevInAEL->OutIdx >= 0 && SlopesEqual(lb->PrevInAEL->bot, lb->PrevInAEL->top, lb->curr, lb->top, use_full_range) && (lb->WindDelta != 0)
			&& (lb->PrevInAEL->WindDelta != 0)) {
			OutPt* Op2 = AddOutPt(lb->PrevInAEL, lb->bot);
			AddJoin(Op1, Op2, lb->top);
		}

		if (lb->NextInAEL != rb) {
			if (rb->OutIdx >= 0 && rb->PrevInAEL->OutIdx >= 0 && SlopesEqual(rb->PrevInAEL->curr, rb->PrevInAEL->top, rb->curr, rb->top, use_full_range) && (rb->WindDelta != 0) && (rb->PrevInAEL->WindDelta != 0)) {
				OutPt* Op2 = AddOutPt(rb->PrevInAEL, rb->bot);
				AddJoin(Op1, Op2, rb->top);
			}

			TEdge* e = lb->NextInAEL;
			if (e) {
				while (e != rb) {
					// nb: For calculating winding counts etc, IntersectEdges() assumes
					// that param1 will be to the Right of param2 ABOVE the intersection ...
					IntersectEdges(rb, e, lb->curr);  // order important here
					e = e->NextInAEL;
				}
			}
		}
	}
}
//------------------------------------------------------------------------------

void Clipper::DeleteFromSEL(TEdge* e) {
	TEdge* SelPrev = e->PrevInSEL;
	TEdge* SelNext = e->NextInSEL;
	if (!SelPrev && !SelNext && (e != sorted_edges))
		return;	 // already deleted

	if (SelPrev)
		SelPrev->NextInSEL = SelNext;
	else
		sorted_edges = SelNext;

	if (SelNext)
		SelNext->PrevInSEL = SelPrev;
	e->NextInSEL = 0;
	e->PrevInSEL = 0;
}
//------------------------------------------------------------------------------

void Clipper::IntersectEdges(TEdge* e1, TEdge* e2, const IntPoint& Pt) {
	bool e1Contributing = (e1->OutIdx >= 0);
	bool e2Contributing = (e2->OutIdx >= 0);

#ifdef use_lines
	// if either edge is on an OPEN path ...
	if (e1->WindDelta == 0 || e2->WindDelta == 0) {
		// ignore subject-subject open path intersections UNLESS they
		// are both open paths, AND they are both 'contributing maximas' ...
		if (e1->WindDelta == 0 && e2->WindDelta == 0)
			return;

		// if intersecting a subj line with a subj poly ...
		else if (e1->type == e2->type && e1->WindDelta != e2->WindDelta && op == ctUnion) {
			if (e1->WindDelta == 0) {
				if (e2Contributing) {
					AddOutPt(e1, Pt);
					if (e1Contributing)
						e1->OutIdx = TEdge::Unassigned;
				}
			} else {
				if (e1Contributing) {
					AddOutPt(e2, Pt);
					if (e2Contributing)
						e2->OutIdx = TEdge::Unassigned;
				}
			}
		} else if (e1->type != e2->type) {
			// toggle subj open path OutIdx on/off when abs(clip.WndCnt) == 1 ...
			if ((e1->WindDelta == 0) && abs(e2->wind) == 1 && (op != ctUnion || e2->wind2 == 0)) {
				AddOutPt(e1, Pt);
				if (e1Contributing)
					e1->OutIdx = TEdge::Unassigned;
			} else if ((e2->WindDelta == 0) && (abs(e1->wind) == 1) && (op != ctUnion || e1->wind2 == 0)) {
				AddOutPt(e2, Pt);
				if (e2Contributing)
					e2->OutIdx = TEdge::Unassigned;
			}
		}
		return;
	}
#endif

	// update winding counts...
	// assumes that e1 will be to the Right of e2 ABOVE the intersection
	if (e1->type == e2->type) {
		if (IsEvenOddFillType(*e1)) {
			int oldE1WindCnt = e1->wind;
			e1->wind		 = e2->wind;
			e2->wind		 = oldE1WindCnt;
		} else {
			if (e1->wind + e2->WindDelta == 0)
				e1->wind = -e1->wind;
			else
				e1->wind += e2->WindDelta;
			if (e2->wind - e1->WindDelta == 0)
				e2->wind = -e2->wind;
			else
				e2->wind -= e1->WindDelta;
		}
	} else {
		if (!IsEvenOddFillType(*e2))
			e1->wind2 += e2->WindDelta;
		else
			e1->wind2 = (e1->wind2 == 0) ? 1 : 0;
		if (!IsEvenOddFillType(*e1))
			e2->wind2 -= e1->WindDelta;
		else
			e2->wind2 = (e2->wind2 == 0) ? 1 : 0;
	}

	PolyFillType e1FillType, e2FillType, e1FillType2, e2FillType2;
	if (e1->type == ptSubject) {
		e1FillType	= subj_fill_type;
		e1FillType2 = clip_fill_type;
	} else {
		e1FillType	= clip_fill_type;
		e1FillType2 = subj_fill_type;
	}
	if (e2->type == ptSubject) {
		e2FillType	= subj_fill_type;
		e2FillType2 = clip_fill_type;
	} else {
		e2FillType	= clip_fill_type;
		e2FillType2 = subj_fill_type;
	}

	cInt e1Wc, e2Wc;
	switch (e1FillType) {
		case pftPositive: e1Wc = e1->wind; break;
		case pftNegative: e1Wc = -e1->wind; break;
		default: e1Wc = abs(e1->wind);
	}
	switch (e2FillType) {
		case pftPositive: e2Wc = e2->wind; break;
		case pftNegative: e2Wc = -e2->wind; break;
		default: e2Wc = abs(e2->wind);
	}

	if (e1Contributing && e2Contributing) {
		if ((e1Wc != 0 && e1Wc != 1) || (e2Wc != 0 && e2Wc != 1) || (e1->type != e2->type && op != ctXor)) {
			AddLocalMaxPoly(e1, e2, Pt);
		} else {
			AddOutPt(e1, Pt);
			AddOutPt(e2, Pt);
			swap(e1->side, e2->side);
			swap(e1->OutIdx, e2->OutIdx);
		}
	} else if (e1Contributing) {
		if (e2Wc == 0 || e2Wc == 1) {
			AddOutPt(e1, Pt);
			swap(e1->side, e2->side);
			swap(e1->OutIdx, e2->OutIdx);
		}
	} else if (e2Contributing) {
		if (e1Wc == 0 || e1Wc == 1) {
			AddOutPt(e2, Pt);
			swap(e1->side, e2->side);
			swap(e1->OutIdx, e2->OutIdx);
		}
	} else if ((e1Wc == 0 || e1Wc == 1) && (e2Wc == 0 || e2Wc == 1)) {
		// neither edge is currently contributing ...

		cInt e1Wc2, e2Wc2;
		switch (e1FillType2) {
			case pftPositive: e1Wc2 = e1->wind2; break;
			case pftNegative: e1Wc2 = -e1->wind2; break;
			default: e1Wc2 = abs(e1->wind2);
		}
		switch (e2FillType2) {
			case pftPositive: e2Wc2 = e2->wind2; break;
			case pftNegative: e2Wc2 = -e2->wind2; break;
			default: e2Wc2 = abs(e2->wind2);
		}

		if (e1->type != e2->type) {
			AddLocalMinPoly(e1, e2, Pt);
		} else if (e1Wc == 1 && e2Wc == 1) {
			switch (op) {
				case ctIntersection:
					if (e1Wc2 > 0 && e2Wc2 > 0)
						AddLocalMinPoly(e1, e2, Pt);
					break;
				case ctUnion:
					if (e1Wc2 <= 0 && e2Wc2 <= 0)
						AddLocalMinPoly(e1, e2, Pt);
					break;
				case ctDifference:
					if (((e1->type == ptClip) && (e1Wc2 > 0) && (e2Wc2 > 0)) || ((e1->type == ptSubject) && (e1Wc2 <= 0) && (e2Wc2 <= 0)))
						AddLocalMinPoly(e1, e2, Pt);
					break;
				case ctXor: AddLocalMinPoly(e1, e2, Pt);
			}
		} else {
			swap(e1->side, e2->side);
		}
	}
}
//------------------------------------------------------------------------------

void Clipper::SetHoleState(TEdge* e, OutRec* outrec) {
	TEdge* eTmp = 0;
	for (TEdge* e2	= e->PrevInAEL; e2; e2 = e2->PrevInAEL) {
		if (e2->OutIdx >= 0 && e2->WindDelta != 0) {
			if (!eTmp)
				eTmp = e2;
			else if (eTmp->OutIdx == e2->OutIdx)
				eTmp = 0;
		}
	}
	if (!eTmp) {
		outrec->FirstLeft = 0;
		outrec->IsHole	  = false;
	} else {
		outrec->FirstLeft = poly_outs[eTmp->OutIdx];
		outrec->IsHole	  = !outrec->FirstLeft->IsHole;
	}
}
//------------------------------------------------------------------------------

OutRec* GetLowermostRec(OutRec* outRec1, OutRec* outRec2) {
	// work out which polygon fragment has the correct hole state ...
	if (!outRec1->BottomPt)
		outRec1->BottomPt = GetBottomPt(outRec1->Pts);
	if (!outRec2->BottomPt)
		outRec2->BottomPt = GetBottomPt(outRec2->Pts);

	OutPt* OutPt1 = outRec1->BottomPt;
	OutPt* OutPt2 = outRec2->BottomPt;

	return	OutPt1->y > OutPt2->y		? outRec1
		:	OutPt1->y < OutPt2->y		? outRec2
		:	OutPt1->x < OutPt2->x		? outRec1
		:	OutPt1->x > OutPt2->x		? outRec2
		:	OutPt1->Next == OutPt1			? outRec2
		:	OutPt2->Next == OutPt2			? outRec1
		:	FirstIsBottomPt(OutPt1, OutPt2)	? outRec1
		:	outRec2;
}

//------------------------------------------------------------------------------

bool OutRec1RightOfOutRec2(OutRec* outRec1, OutRec* outRec2) {
	do {
		outRec1 = outRec1->FirstLeft;
		if (outRec1 == outRec2)
			return true;
	} while (outRec1);
	return false;
}
//------------------------------------------------------------------------------

OutRec* Clipper::GetOutRec(int index) {
	OutRec* outrec = poly_outs[index];
	while (outrec != poly_outs[outrec->index])
		outrec = poly_outs[outrec->index];
	return outrec;
}
//------------------------------------------------------------------------------

void Clipper::AppendPolygon(TEdge* e1, TEdge* e2) {
	// get the start and ends of both output polygons ...
	OutRec* outRec1 = poly_outs[e1->OutIdx];
	OutRec* outRec2 = poly_outs[e2->OutIdx];

	OutRec* holeStateRec = OutRec1RightOfOutRec2(outRec1, outRec2)	? outRec2
		:	OutRec1RightOfOutRec2(outRec2, outRec1)					? outRec1
		:	GetLowermostRec(outRec1, outRec2);

	// get the start and ends of both output polygons and
	// join e2 poly onto e1 poly and delete pointers to e2 ...

	OutPt* p1_lft = outRec1->Pts;
	OutPt* p1_rt  = p1_lft->Prev;
	OutPt* p2_lft = outRec2->Pts;
	OutPt* p2_rt  = p2_lft->Prev;

	// join e2 poly onto e1 poly and delete pointers to e2 ...
	if (e1->side == esLeft) {
		if (e2->side == esLeft) {
			// z y x a b c
			ReversePolyPtLinks(p2_lft);
			p2_lft->Next = p1_lft;
			p1_lft->Prev = p2_lft;
			p1_rt->Next	 = p2_rt;
			p2_rt->Prev	 = p1_rt;
			outRec1->Pts = p2_rt;
		} else {
			// x y z a b c
			p2_rt->Next	 = p1_lft;
			p1_lft->Prev = p2_rt;
			p2_lft->Prev = p1_rt;
			p1_rt->Next	 = p2_lft;
			outRec1->Pts = p2_lft;
		}
	} else {
		if (e2->side == esRight) {
			// a b c z y x
			ReversePolyPtLinks(p2_lft);
			p1_rt->Next	 = p2_rt;
			p2_rt->Prev	 = p1_rt;
			p2_lft->Next = p1_lft;
			p1_lft->Prev = p2_lft;
		} else {
			// a b c x y z
			p1_rt->Next	 = p2_lft;
			p2_lft->Prev = p1_rt;
			p1_lft->Prev = p2_rt;
			p2_rt->Next	 = p1_lft;
		}
	}

	outRec1->BottomPt = 0;
	if (holeStateRec == outRec2) {
		if (outRec2->FirstLeft != outRec1)
			outRec1->FirstLeft = outRec2->FirstLeft;
		outRec1->IsHole = outRec2->IsHole;
	}
	outRec2->Pts	   = 0;
	outRec2->BottomPt  = 0;
	outRec2->FirstLeft = outRec1;

	int OKIdx		= e1->OutIdx;
	int ObsoleteIdx = e2->OutIdx;

	e1->OutIdx = TEdge::Unassigned;  // nb: safe because we only get here via AddLocalMaxPoly
	e2->OutIdx = TEdge::Unassigned;

	for (TEdge* e = aet; e; e = e->NextInAEL) {
		if (e->OutIdx == ObsoleteIdx) {
			e->OutIdx = OKIdx;
			e->side	  = e1->side;
			break;
		}
	}

	outRec2->index = outRec1->index;
}
//------------------------------------------------------------------------------

OutPt* Clipper::AddOutPt(TEdge* e, const IntPoint& pt) {
	if (e->OutIdx < 0) {
		OutRec* outRec = CreateOutRec();
		outRec->is_open = e->WindDelta == 0;
		OutPt* newOp   = new OutPt(outRec->index, pt);
		outRec->Pts	   = newOp;
		if (!outRec->is_open)
			SetHoleState(e, outRec);
		e->OutIdx = outRec->index;
		return newOp;

	} else {
		OutRec* outRec	= poly_outs[e->OutIdx];		// OutRec.Pts is the 'Left-most' point & OutRec.Pts.Prev is the 'Right-most'
		OutPt*	op		= outRec->Pts;
		bool	ToFront	= e->side == esLeft;
		if (ToFront && all(pt == *op))
			return op;
		else if (!ToFront && all(pt == *op->Prev))
			return op->Prev;

		OutPt* newOp	  = new OutPt(outRec->index, pt, op, op->Prev);
		newOp->Prev->Next = newOp;
		op->Prev		  = newOp;
		if (ToFront)
			outRec->Pts = newOp;
		return newOp;
	}
}
//------------------------------------------------------------------------------

OutPt* Clipper::GetLastOutPt(TEdge* e) {
	OutRec* outRec = poly_outs[e->OutIdx];
	return e->side == esLeft ? outRec->Pts: outRec->Pts->Prev;
}
//------------------------------------------------------------------------------

bool Clipper::ProcessHorizontals() {
	TEdge* horzEdge;
	while (PopEdgeFromSEL(horzEdge)) {
		if (!ProcessHorizontal(horzEdge))
			return false;
	}
	return true;
}
//------------------------------------------------------------------------------

inline bool IsMinima(TEdge* e)						{ return e && (e->Prev->NextInLML != e) && (e->Next->NextInLML != e); }
inline bool IsMaxima(TEdge* e, const cInt y)		{ return e && e->top.y == y && !e->NextInLML; }
inline bool IsIntermediate(TEdge* e, const cInt y)	{ return e->top.y == y && e->NextInLML; }

TEdge* GetMaximaPair(TEdge* e) {
	return	all(e->Next->top == e->top) && !e->Next->NextInLML	? e->Next
		:	all(e->Prev->top == e->top) && !e->Prev->NextInLML	? e->Prev
		:	0;
}

TEdge* GetMaximaPairEx(TEdge* e) {
	// as GetMaximaPair() but returns 0 if MaxPair isn't in AEL (unless it's horizontal)
	TEdge* result = GetMaximaPair(e);
	if (result && (result->OutIdx == TEdge::Skip || (result->NextInAEL == result->PrevInAEL && !result->IsHorizontal())))
		return 0;
	return result;
}
//------------------------------------------------------------------------------

void Clipper::SwapPositionsInSEL(TEdge* Edge1, TEdge* Edge2) {
	if (!Edge1->NextInSEL && !Edge1->PrevInSEL)
		return;

	if (!Edge2->NextInSEL && !Edge2->PrevInSEL)
		return;

	if (Edge1->NextInSEL == Edge2) {
		TEdge* Next = Edge2->NextInSEL;
		if (Next)
			Next->PrevInSEL = Edge1;
		TEdge* Prev = Edge1->PrevInSEL;
		if (Prev)
			Prev->NextInSEL = Edge2;
		Edge2->PrevInSEL = Prev;
		Edge2->NextInSEL = Edge1;
		Edge1->PrevInSEL = Edge2;
		Edge1->NextInSEL = Next;

	} else if (Edge2->NextInSEL == Edge1) {
		TEdge* Next = Edge1->NextInSEL;
		if (Next)
			Next->PrevInSEL = Edge2;
		TEdge* Prev = Edge2->PrevInSEL;
		if (Prev)
			Prev->NextInSEL = Edge1;
		Edge1->PrevInSEL = Prev;
		Edge1->NextInSEL = Edge2;
		Edge2->PrevInSEL = Edge1;
		Edge2->NextInSEL = Next;

	} else {
		TEdge* Next		 = Edge1->NextInSEL;
		TEdge* Prev		 = Edge1->PrevInSEL;
		Edge1->NextInSEL = Edge2->NextInSEL;
		if (Edge1->NextInSEL)
			Edge1->NextInSEL->PrevInSEL = Edge1;
		Edge1->PrevInSEL = Edge2->PrevInSEL;
		if (Edge1->PrevInSEL)
			Edge1->PrevInSEL->NextInSEL = Edge1;
		Edge2->NextInSEL = Next;
		if (Edge2->NextInSEL)
			Edge2->NextInSEL->PrevInSEL = Edge2;
		Edge2->PrevInSEL = Prev;
		if (Edge2->PrevInSEL)
			Edge2->PrevInSEL->NextInSEL = Edge2;
	}

	if (!Edge1->PrevInSEL)
		sorted_edges = Edge1;
	else if (!Edge2->PrevInSEL)
		sorted_edges = Edge2;
}

//------------------------------------------------------------------------

/*******************************************************************************
 * Notes: Horizontal edges (HEs) at scanline intersections (ie at the top or    *
 * Bottom of a scanbeam) are processed as if layered. The order in which HEs    *
 * are processed doesn't matter. HEs intersect with other HE bot.Xs only [#]    *
 * (or they could intersect with top.Xs only, ie EITHER bot.Xs OR top.Xs),      *
 * and with other non-horizontal edges [*]. Once these intersections are        *
 * processed, intermediate HEs then 'promote' the Edge above (NextInLML) into   *
 * the AEL. These 'promoted' edges may in turn intersect [%] with other HEs.    *
 *******************************************************************************/

bool Clipper::ProcessHorizontal(TEdge* horzEdge) {
	Direction dir;
	cInt	  horzLeft, horzRight;
	bool	  is_open = horzEdge->WindDelta == 0;

	horzEdge->GetHorzDirection(dir, horzLeft, horzRight);

	TEdge *eLastHorz = horzEdge, *eMaxPair = 0;
	while (eLastHorz->NextInLML && eLastHorz->NextInLML->IsHorizontal())
		eLastHorz = eLastHorz->NextInLML;
	if (!eLastHorz->NextInLML)
		eMaxPair = GetMaximaPair(eLastHorz);

	auto r_Maxima	= reversed(maxima);
	decltype(maxima.begin())				maxIt;
	noref_cv_t<decltype(r_Maxima.begin())>	maxRit;

	if (!maxima.empty()) {
		// get the first maxima in range (x) ...
		if (dir == dLeftToRight) {
			maxIt = maxima.begin();
			while (maxIt != maxima.end() && *maxIt <= horzEdge->bot.x)
				maxIt++;
			if (maxIt != maxima.end() && *maxIt >= eLastHorz->top.x)
				maxIt = maxima.end();
		} else {
			maxRit = r_Maxima.begin();
			while (maxRit != r_Maxima.end() && *maxRit > horzEdge->bot.x)
				maxRit++;
			if (maxRit != r_Maxima.end() && *maxRit <= eLastHorz->top.x)
				maxRit = r_Maxima.end();
		}
	}

	OutPt* op1 = 0;

	for (;;) { // loop through consec. horizontal edges
		bool   IsLastHorz = (horzEdge == eLastHorz);
		TEdge* e		  = horzEdge->GetNextInAEL(dir);
		while (e) {
			// this code block inserts extra coords into horizontal edges (in output polygons) whereever maxima touch these horizontal edges
			// This helps 'simplifying' polygons (ie if the Simplify property is set)
			if (maxima.size() > 0) {
				if (dir == dLeftToRight) {
					while (maxIt != maxima.end() && *maxIt < e->curr.x) {
						if (horzEdge->OutIdx >= 0 && !is_open)
							AddOutPt(horzEdge, IntPoint(*maxIt, horzEdge->bot.y));
						maxIt++;
					}
				} else {
					while (maxRit != r_Maxima.end() && *maxRit > e->curr.x) {
						if (horzEdge->OutIdx >= 0 && !is_open)
							AddOutPt(horzEdge, IntPoint(*maxRit, horzEdge->bot.y));
						maxRit++;
					}
				}
			};

			if ((dir == dLeftToRight && e->curr.x > horzRight) || (dir == dRightToLeft && e->curr.x < horzLeft))
				break;

			// Also break if we've got to the end of an intermediate horizontal edge ...
			// nb: Smaller dx's are to the right of larger dx's ABOVE the horizontal.
			if (e->curr.x == horzEdge->top.x && horzEdge->NextInLML && e->dx < horzEdge->NextInLML->dx)
				break;

			if (horzEdge->OutIdx >= 0 && !is_open) { // note: may be done multiple times
				op1				 = AddOutPt(horzEdge, e->curr);
				for (TEdge* eNextHorz = sorted_edges; eNextHorz; eNextHorz = eNextHorz->NextInSEL) {
					if (eNextHorz->OutIdx >= 0 && HorzSegmentsOverlap(horzEdge->bot.x, horzEdge->top.x, eNextHorz->bot.x, eNextHorz->top.x))
						AddJoin(GetLastOutPt(eNextHorz), op1, eNextHorz->top);
				}
				AddGhostJoin(op1, horzEdge->bot);
			}

			// OK, so far we're still in range of the horizontal Edge  but make sure we're at the last of consec. horizontals when matching with eMaxPair
			if (e == eMaxPair && IsLastHorz) {
				if (horzEdge->OutIdx >= 0)
					AddLocalMaxPoly(horzEdge, eMaxPair, horzEdge->top);
				DeleteFromAEL(horzEdge);
				DeleteFromAEL(eMaxPair);
				return true;
			}

			if (dir == dLeftToRight)
				IntersectEdges(horzEdge, e, IntPoint(e->curr.x, horzEdge->curr.y));
			else
				IntersectEdges(e, horzEdge, IntPoint(e->curr.x, horzEdge->curr.y));

			TEdge* eNext = e->GetNextInAEL(dir);
			SwapPositionsInAEL(horzEdge, e);
			e = eNext;
		}  // end while(e)

		// Break out of loop if HorzEdge.NextInLML is not also horizontal ...
		if (!horzEdge->NextInLML || !horzEdge->NextInLML->IsHorizontal())
			break;

		if (!UpdateEdgeIntoAEL(horzEdge))
			return false;

		if (horzEdge->OutIdx >= 0)
			AddOutPt(horzEdge, horzEdge->bot);
		horzEdge->GetHorzDirection(dir, horzLeft, horzRight);

	}  // end for (;;)

	if (horzEdge->OutIdx >= 0 && !op1) {
		op1				 = GetLastOutPt(horzEdge);
		for (TEdge* eNextHorz = sorted_edges; eNextHorz; eNextHorz = eNextHorz->NextInSEL) {
			if (eNextHorz->OutIdx >= 0 && HorzSegmentsOverlap(horzEdge->bot.x, horzEdge->top.x, eNextHorz->bot.x, eNextHorz->top.x))
				AddJoin(GetLastOutPt(eNextHorz), op1, eNextHorz->top);
		}
		AddGhostJoin(op1, horzEdge->top);
	}

	if (horzEdge->NextInLML) {
		if (horzEdge->OutIdx >= 0) {
			op1 = AddOutPt(horzEdge, horzEdge->top);
			if (!UpdateEdgeIntoAEL(horzEdge))
				return false;
			if (horzEdge->WindDelta != 0) {
				// nb: HorzEdge is no longer horizontal here
				TEdge* ePrev = horzEdge->PrevInAEL;
				TEdge* eNext = horzEdge->NextInAEL;
				if (ePrev && ePrev->curr.x == horzEdge->bot.x && ePrev->curr.y == horzEdge->bot.y && ePrev->WindDelta != 0 && (ePrev->OutIdx >= 0 && ePrev->curr.y > ePrev->top.y && SlopesEqual(*horzEdge, *ePrev, use_full_range)))
					AddJoin(op1, AddOutPt(ePrev, horzEdge->bot), horzEdge->top);
				else if (eNext && eNext->curr.x == horzEdge->bot.x && eNext->curr.y == horzEdge->bot.y && eNext->WindDelta != 0 && eNext->OutIdx >= 0 && eNext->curr.y > eNext->top.y && SlopesEqual(*horzEdge, *eNext, use_full_range))
					AddJoin(op1, AddOutPt(eNext, horzEdge->bot), horzEdge->top);
			}
		} else if (!UpdateEdgeIntoAEL(horzEdge)) {
			return false;
		}
	} else {
		if (horzEdge->OutIdx >= 0)
			AddOutPt(horzEdge, horzEdge->top);
		DeleteFromAEL(horzEdge);
	}
	return true;
}
//------------------------------------------------------------------------------

bool Clipper::ProcessIntersections(const cInt topY) {
	if (!aet)
		return true;

	BuildIntersectList(topY);
	size_t IlSize = intersections.size();
	if (IlSize == 0)
		return true;
	if (IlSize == 1 || FixupIntersectionOrder())
		ProcessIntersectList();
	else
		return false;

	//DisposeIntersectNodes();

	sorted_edges = 0;
	return true;
}
//------------------------------------------------------------------------------

void Clipper::DisposeIntersectNodes() {
	for (size_t i = 0; i < intersections.size(); ++i)
		delete intersections[i];
	intersections.clear();
}
//------------------------------------------------------------------------------

void Clipper::BuildIntersectList(const cInt topY) {
	if (!aet)
		return;

	// prepare for sorting ...
	TEdge* e	  = aet;
	sorted_edges = e;
	while (e) {
		e->PrevInSEL = e->PrevInAEL;
		e->NextInSEL = e->NextInAEL;
		e->curr.x	 = e->TopX(topY);
		e			 = e->NextInAEL;
	}

	// bubblesort ...
	bool isModified;
	do {
		isModified = false;
		e		   = sorted_edges;
		while (e->NextInSEL) {
			TEdge*	 eNext = e->NextInSEL;
			IntPoint Pt;
			if (e->curr.x > eNext->curr.x) {
				IntersectPoint(*e, *eNext, Pt);
				if (Pt.y < topY)
					Pt = IntPoint(e->TopX(topY), topY);
				intersections.push_back(new IntersectNode(e, eNext, Pt));

				SwapPositionsInSEL(e, eNext);
				isModified = true;
			} else
				e = eNext;
		}
		if (e->PrevInSEL)
			e->PrevInSEL->NextInSEL = 0;
		else
			break;

	} while (isModified);
	sorted_edges = 0;	// important
}
//------------------------------------------------------------------------------

void Clipper::ProcessIntersectList() {
	for (size_t i = 0; i < intersections.size(); ++i) {
		IntersectNode* iNode = intersections[i];
		IntersectEdges(iNode->Edge1, iNode->Edge2, *iNode);
		SwapPositionsInAEL(iNode->Edge1, iNode->Edge2);
		delete iNode;
	}
	intersections.clear();
}
//------------------------------------------------------------------------------

bool Clipper::FixupIntersectionOrder() {
	// pre-condition: intersections are sorted Bottom-most first.
	// Now it's crucial that intersections are made only between adjacent edges,
	// so to ensure this the order of intersections may need adjusting ...
	CopyAELToSEL();
	sort(intersections.begin(), intersections.end(), [](IntersectNode* node1, IntersectNode* node2) { return node2->y < node1->y; });
	size_t cnt = intersections.size();
	for (size_t i = 0; i < cnt; ++i) {
		if (!intersections[i]->EdgesAdjacent()) {
			size_t j = i + 1;
			while (j < cnt && !intersections[j]->EdgesAdjacent())
				j++;
			if (j == cnt)
				return false;
			swap(intersections[i], intersections[j]);
		}
		SwapPositionsInSEL(intersections[i]->Edge1, intersections[i]->Edge2);
	}
	return true;
}
//------------------------------------------------------------------------------

bool Clipper::DoMaxima(TEdge* e) {
	TEdge* eMaxPair = GetMaximaPairEx(e);
	if (!eMaxPair) {
		if (e->OutIdx >= 0)
			AddOutPt(e, e->top);
		DeleteFromAEL(e);
		return true;
	}

	TEdge* eNext = e->NextInAEL;
	while (eNext && eNext != eMaxPair) {
		IntersectEdges(e, eNext, e->top);
		SwapPositionsInAEL(e, eNext);
		eNext = e->NextInAEL;
	}

	if (e->OutIdx == TEdge::Unassigned && eMaxPair->OutIdx == TEdge::Unassigned) {
		DeleteFromAEL(e);
		DeleteFromAEL(eMaxPair);
	} else if (e->OutIdx >= 0 && eMaxPair->OutIdx >= 0) {
		if (e->OutIdx >= 0)
			AddLocalMaxPoly(e, eMaxPair, e->top);
		DeleteFromAEL(e);
		DeleteFromAEL(eMaxPair);
	}
#ifdef use_lines
	else if (e->WindDelta == 0) {
		if (e->OutIdx >= 0) {
			AddOutPt(e, e->top);
			e->OutIdx = TEdge::Unassigned;
		}
		DeleteFromAEL(e);

		if (eMaxPair->OutIdx >= 0) {
			AddOutPt(eMaxPair, e->top);
			eMaxPair->OutIdx = TEdge::Unassigned;
		}
		DeleteFromAEL(eMaxPair);
	}
#endif
	else
		return false;//throw clipperException("DoMaxima error");
	return true;
}
//------------------------------------------------------------------------------

bool Clipper::ProcessEdgesAtTopOfScanbeam(const cInt topY) {
	TEdge* e = aet;
	while (e) {
		// 1. process maxima, treating them as if they're 'bent' horizontal edges, but exclude maxima with horizontal edges. nb: e can't be a horizontal
		bool IsMaximaEdge = IsMaxima(e, topY);

		if (IsMaximaEdge) {
			TEdge* eMaxPair = GetMaximaPairEx(e);
			IsMaximaEdge	= !eMaxPair || !eMaxPair->IsHorizontal();
		}

		if (IsMaximaEdge) {
			if (strict_simple)
				maxima.push_back(e->top.x);
			TEdge* ePrev = e->PrevInAEL;
			if (!DoMaxima(e))
				return false;
			if (!ePrev)
				e = aet;
			else
				e = ePrev->NextInAEL;
		} else {
			// 2. promote horizontal edges, otherwise update curr.x and curr.y ...
			if (IsIntermediate(e, topY) && e->NextInLML->IsHorizontal()) {
				UpdateEdgeIntoAEL(e);
				if (e->OutIdx >= 0)
					AddOutPt(e, e->bot);
				AddEdgeToSEL(e);
			} else {
				e->curr.x = e->TopX(topY);
				e->curr.y = topY;
			}

			// When StrictlySimple and 'e' is being touched by another edge, then make sure both edges have a vertex here ...
			if (strict_simple) {
				TEdge* ePrev = e->PrevInAEL;
				if (e->OutIdx >= 0 && e->WindDelta != 0 && ePrev && ePrev->OutIdx >= 0 && ePrev->curr.x == e->curr.x && ePrev->WindDelta != 0) {
					IntPoint pt = e->curr;
					AddJoin(AddOutPt(ePrev, pt), AddOutPt(e, pt), pt);  // StrictlySimple (type-3) join
				}
			}

			e = e->NextInAEL;
		}
	}

	// 3. Process horizontals at the top of the scanbeam ...
	sort(maxima);
	if (!ProcessHorizontals())
		return false;
	maxima.clear();

	// 4. Promote intermediate vertices ...
	e = aet;
	while (e) {
		if (IsIntermediate(e, topY)) {
			OutPt* op = 0;
			if (e->OutIdx >= 0)
				op = AddOutPt(e, e->top);
			UpdateEdgeIntoAEL(e);

			// if output polygons share an edge, they'll need joining later ...
			TEdge* ePrev = e->PrevInAEL;
			TEdge* eNext = e->NextInAEL;
			if (ePrev && ePrev->curr.x == e->bot.x && ePrev->curr.y == e->bot.y && op && ePrev->OutIdx >= 0 && ePrev->curr.y > ePrev->top.y
				&& SlopesEqual(e->curr, e->top, ePrev->curr, ePrev->top, use_full_range)
				&& e->WindDelta != 0 && ePrev->WindDelta != 0
			) {
				AddJoin(op, AddOutPt(ePrev, e->bot), e->top);
			} else if (eNext && eNext->curr.x == e->bot.x && eNext->curr.y == e->bot.y && op && eNext->OutIdx >= 0 && eNext->curr.y > eNext->top.y
				&& SlopesEqual(e->curr, e->top, eNext->curr, eNext->top, use_full_range)
				&& e->WindDelta != 0 && eNext->WindDelta != 0
			) {
				OutPt* op2 = AddOutPt(eNext, e->bot);
				AddJoin(op, op2, e->top);
			}
		}
		e = e->NextInAEL;
	}
	return true;
}
//------------------------------------------------------------------------------

void Clipper::FixupOutPolyline(OutRec& outrec) {
	OutPt* pp	  = outrec.Pts;
	OutPt* lastPP = pp->Prev;
	while (pp != lastPP) {
		pp = pp->Next;
		if (all(*pp == *pp->Prev)) {
			if (pp == lastPP)
				lastPP = pp->Prev;
			OutPt* tmpPP   = pp->Prev;
			tmpPP->Next	   = pp->Next;
			pp->Next->Prev = tmpPP;
			delete pp;
			pp = tmpPP;
		}
	}

	if (pp == pp->Prev) {
		DisposeOutPts(pp);
		outrec.Pts = 0;
		return;
	}
}
//------------------------------------------------------------------------------

void Clipper::FixupOutPolygon(OutRec& outrec) {
	// FixupOutPolygon() - removes duplicate points and simplifies consecutive parallel edges by removing the middle vertex.
	OutPt* lastOK	   = 0;
	outrec.BottomPt	   = 0;
	OutPt* pp		   = outrec.Pts;
	bool   preserveCol = preserve_colinear || strict_simple;

	for (;;) {
		if (pp->Prev == pp || pp->Prev == pp->Next) {
			DisposeOutPts(pp);
			outrec.Pts = 0;
			return;
		}

		// test for duplicate points and collinear edges ...
		if (all(*pp == *pp->Next) || all(*pp == *pp->Prev) || (SlopesEqual(*pp->Prev, *pp, *pp->Next, use_full_range) && (!preserveCol || !Pt2IsBetweenPt1AndPt3(*pp->Prev, *pp, *pp->Next)))) {
			lastOK		   = 0;
			OutPt* tmp	   = pp;
			pp->Prev->Next = pp->Next;
			pp->Next->Prev = pp->Prev;
			pp			   = pp->Prev;
			delete tmp;
		} else if (pp == lastOK) {
			break;
		} else {
			if (!lastOK)
				lastOK = pp;
			pp = pp->Next;
		}
	}
	outrec.Pts = pp;
}
//------------------------------------------------------------------------------

int PointCount(OutPt* Pts) {
	if (!Pts)
		return 0;
	int	   result = 0;
	OutPt* p	  = Pts;
	do {
		result++;
		p = p->Next;
	} while (p != Pts);
	return result;
}
//------------------------------------------------------------------------------

void Clipper::BuildResult(Paths& polys) {
	polys.reserve(poly_outs.size());
	for (auto i : poly_outs) {
		if (!i->Pts)
			continue;
		Path   pg;
		OutPt* p   = i->Pts->Prev;
		int	   cnt = PointCount(p);
		if (cnt < 2)
			continue;
		pg.reserve(cnt);
		for (int j = 0; j < cnt; ++j) {
			pg.push_back(*p);
			p = p->Prev;
		}
		polys.push_back(pg);
	}
}
//------------------------------------------------------------------------------

void Clipper::BuildResult2(PolyTree& polytree) {
	polytree.Clear();
	polytree.AllNodes.reserve(poly_outs.size());
	// add each output polygon/contour to polytree ...
	for (auto i : poly_outs) {
		int		cnt	   = PointCount(i->Pts);
		if (cnt >= (i->is_open ? 3 : 2)) {
			FixHoleLinkage(*i);
			PolyNode* pn = new PolyNode();
			// nb: polytree takes ownership of all the PolyNodes
			polytree.AllNodes.push_back(pn);
			i->PolyNd	= pn;
			pn->parent	= 0;
			pn->index	= 0;
			pn->contour.reserve(cnt);
			OutPt* op = i->Pts->Prev;
			for (int j = 0; j < cnt; j++) {
				pn->contour.push_back(*op);
				op = op->Prev;
			}
		}
	}

	// fixup PolyNode links etc ...
	polytree.children.reserve(poly_outs.size());
	for (auto i : poly_outs) {
		if (i->PolyNd) {
			if (i->is_open) {
				i->PolyNd->is_open = true;
				polytree.AddChild(*i->PolyNd);
			} else if (i->FirstLeft && i->FirstLeft->PolyNd) {
				i->FirstLeft->PolyNd->AddChild(*i->PolyNd);
			} else {
				polytree.AddChild(*i->PolyNd);
			}
		}
	}
}

//------------------------------------------------------------------------------

inline bool E2InsertsBeforeE1(TEdge& e1, TEdge& e2) {
	return	e2.curr.x != e1.curr.x		? e2.curr.x < e1.curr.x
		:	e2.top.y > e1.top.y			? e2.top.x < e1.TopX(e2.top.y)
		:	e1.top.x > e2.TopX(e1.top.y);
}

bool GetOverlap(const cInt a1, const cInt a2, const cInt b1, const cInt b2, cInt& Left, cInt& Right) {
	if (a1 < a2) {
		if (b1 < b2) {
			Left  = max(a1, b1);
			Right = min(a2, b2);
		} else {
			Left  = max(a1, b2);
			Right = min(a2, b1);
		}
	} else {
		if (b1 < b2) {
			Left  = max(a2, b1);
			Right = min(a1, b2);
		} else {
			Left  = max(a2, b2);
			Right = min(a1, b1);
		}
	}
	return Left < Right;
}

inline void UpdateOutPtIdxs(OutRec& outrec) {
	OutPt* op = outrec.Pts;
	do {
		op->index = outrec.index;
		op		= op->Prev;
	} while (op != outrec.Pts);
}

void Clipper::InsertEdgeIntoAEL(TEdge* edge, TEdge* startEdge) {
	if (!aet) {
		edge->PrevInAEL = 0;
		edge->NextInAEL = 0;
		aet	= edge;
	} else if (!startEdge && E2InsertsBeforeE1(*aet, *edge)) {
		edge->PrevInAEL			 = 0;
		edge->NextInAEL			 = aet;
		aet->PrevInAEL = edge;
		aet			 = edge;
	} else {
		if (!startEdge)
			startEdge = aet;
		while (startEdge->NextInAEL && !E2InsertsBeforeE1(*startEdge->NextInAEL, *edge))
			startEdge = startEdge->NextInAEL;
		edge->NextInAEL = startEdge->NextInAEL;
		if (startEdge->NextInAEL)
			startEdge->NextInAEL->PrevInAEL = edge;
		edge->PrevInAEL		 = startEdge;
		startEdge->NextInAEL = edge;
	}
}
//----------------------------------------------------------------------

OutPt* DupOutPt(OutPt* outPt, bool InsertAfter) {
	OutPt* result = new OutPt(*outPt);
	if (InsertAfter) {
		result->Prev	  = outPt;
		outPt->Next->Prev = result;
		outPt->Next		  = result;
	} else {
		result->Next	  = outPt;
		outPt->Prev->Next = result;
		outPt->Prev		  = result;
	}
	return result;
}
//------------------------------------------------------------------------------

bool JoinHorz(OutPt* op1, OutPt* op1b, OutPt* op2, OutPt* op2b, const IntPoint Pt, bool DiscardLeft) {
	Direction Dir1 = op1->x > op1b->x ? dRightToLeft : dLeftToRight;
	Direction Dir2 = op2->x > op2b->x ? dRightToLeft : dLeftToRight;
	if (Dir1 == Dir2)
		return false;

	// When DiscardLeft, we want Op1b to be on the Left of Op1, otherwise we want Op1b to be on the Right. (And likewise with Op2 and Op2b.)
	// So, to facilitate this while inserting Op1b and Op2b ...
	// when DiscardLeft, make sure we're AT or RIGHT of Pt before adding Op1b, otherwise make sure we're AT or LEFT of Pt. (Likewise with Op2b.)
	if (Dir1 == dLeftToRight) {
		while (op1->Next->x <= Pt.x && op1->Next->x >= op1->x && op1->Next->y == Pt.y)
			op1 = op1->Next;
		if (DiscardLeft && (op1->x != Pt.x))
			op1 = op1->Next;
		op1b = DupOutPt(op1, !DiscardLeft);
		if (any(*op1b != Pt)) {
			op1		= op1b;
			*(IntPoint*)op1	= Pt;
			op1b	= DupOutPt(op1, !DiscardLeft);
		}
	} else {
		while (op1->Next->x >= Pt.x && op1->Next->x <= op1->x && op1->Next->y == Pt.y)
			op1 = op1->Next;
		if (!DiscardLeft && (op1->x != Pt.x))
			op1 = op1->Next;
		op1b = DupOutPt(op1, DiscardLeft);
		if (any(*op1b != Pt)) {
			op1		= op1b;
			*(IntPoint*)op1	= Pt;
			op1b	= DupOutPt(op1, DiscardLeft);
		}
	}

	if (Dir2 == dLeftToRight) {
		while (op2->Next->x <= Pt.x && op2->Next->x >= op2->x && op2->Next->y == Pt.y)
			op2 = op2->Next;
		if (DiscardLeft && (op2->x != Pt.x))
			op2 = op2->Next;
		op2b = DupOutPt(op2, !DiscardLeft);
		if (any(*op2b != Pt)) {
			op2		= op2b;
			*(IntPoint*)op2	= Pt;
			op2b	= DupOutPt(op2, !DiscardLeft);
		};
	} else {
		while (op2->Next->x >= Pt.x && op2->Next->x <= op2->x && op2->Next->y == Pt.y)
			op2 = op2->Next;
		if (!DiscardLeft && (op2->x != Pt.x))
			op2 = op2->Next;
		op2b = DupOutPt(op2, DiscardLeft);
		if (any(*op2b != Pt)) {
			op2		= op2b;
			*(IntPoint*)op2	= Pt;
			op2b	= DupOutPt(op2, DiscardLeft);
		};
	};

	if ((Dir1 == dLeftToRight) == DiscardLeft) {
		op1->Prev  = op2;
		op2->Next  = op1;
		op1b->Next = op2b;
		op2b->Prev = op1b;
	} else {
		op1->Next  = op2;
		op2->Prev  = op1;
		op1b->Prev = op2b;
		op2b->Next = op1b;
	}
	return true;
}
//------------------------------------------------------------------------------

bool Clipper::JoinPoints(Join& j, OutRec* outRec1, OutRec* outRec2) {
	OutPt *op1 = j.OutPt1, *op1b;
	OutPt *op2 = j.OutPt2, *op2b;

	// There are 3 kinds of joins for output polygons ...
	// 1. Horizontal joins where Join.OutPt1 & Join.OutPt2 are vertices anywhere along (horizontal) collinear edges (& Join.OffPt is on the same horizontal).
	// 2. Non-horizontal joins where Join.OutPt1 & Join.OutPt2 are at the same location at the Bottom of the overlapping segment (& Join.OffPt is above).
	// 3. StrictSimple joins where edges touch but are not collinear and where Join.OutPt1, Join.OutPt2 & Join.OffPt all share the same point.
	bool isHorizontal = j.OutPt1->y == j.y;

	if (isHorizontal && all(j == *j.OutPt1) && all(j == *j.OutPt2)) {
		// Strictly Simple join ...
		if (outRec1 != outRec2)
			return false;

		op1b = j.OutPt1->Next;
		while (any(op1b != op1) && all(*op1b == j))
			op1b = op1b->Next;
		bool reverse1 = op1b->y > j.y;

		op2b  = j.OutPt2->Next;
		while (any(op2b != op2) && all(*op2b == j))
			op2b = op2b->Next;
		bool reverse2 = op2b->y > j.y;

		if (reverse1 == reverse2)
			return false;

		if (reverse1) {
			op1b	   = DupOutPt(op1, false);
			op2b	   = DupOutPt(op2, true);
			op1->Prev  = op2;
			op2->Next  = op1;
			op1b->Next = op2b;
			op2b->Prev = op1b;
			j.OutPt1  = op1;
			j.OutPt2  = op1b;
			return true;
		} else {
			op1b	   = DupOutPt(op1, true);
			op2b	   = DupOutPt(op2, false);
			op1->Next  = op2;
			op2->Prev  = op1;
			op1b->Prev = op2b;
			op2b->Next = op1b;
			j.OutPt1  = op1;
			j.OutPt2  = op1b;
			return true;
		}
	} else if (isHorizontal) {
		// treat horizontal joins differently to non-horizontal joins since with them we're not yet sure where the overlapping is. OutPt1.Pt & OutPt2.Pt may be anywhere along the horizontal edge.
		op1b = op1;
		while (op1->Prev->y == op1->y && op1->Prev != op1b && op1->Prev != op2)
			op1 = op1->Prev;

		while (op1b->Next->y == op1b->y && op1b->Next != op1 && op1b->Next != op2)
			op1b = op1b->Next;

		if (op1b->Next == op1 || op1b->Next == op2)
			return false;  // a flat 'polygon'

		op2b = op2;
		while (op2->Prev->y == op2->y && op2->Prev != op2b && op2->Prev != op1b)
			op2 = op2->Prev;

		while (op2b->Next->y == op2b->y && op2b->Next != op2 && op2b->Next != op1)
			op2b = op2b->Next;

		if (op2b->Next == op2 || op2b->Next == op1)
			return false;  // a flat 'polygon'

		cInt Left, Right;
		// Op1 --> Op1b & Op2 --> Op2b are the extremites of the horizontal edges
		if (!GetOverlap(op1->x, op1b->x, op2->x, op2b->x, Left, Right))
			return false;

		// DiscardLeftSide: when overlapping edges are joined, a spike will created which needs to be cleaned up. However, we don't want Op1 or Op2 caught up on the discard side as either may still be needed for other joins ...
		IntPoint Pt;
		bool	 DiscardLeftSide;
		if (op1->x >= Left && op1->x <= Right) {
			Pt				= *op1;
			DiscardLeftSide = op1->x > op1b->x;
		} else if (op2->x >= Left && op2->x <= Right) {
			Pt				= *op2;
			DiscardLeftSide = op2->x > op2b->x;
		} else if (op1b->x >= Left && op1b->x <= Right) {
			Pt				= *op1b;
			DiscardLeftSide = op1b->x > op1->x;
		} else {
			Pt				= *op2b;
			DiscardLeftSide = op2b->x > op2->x;
		}
		j.OutPt1 = op1;
		j.OutPt2 = op2;
		return JoinHorz(op1, op1b, op2, op2b, Pt, DiscardLeftSide);

	} else {
		// nb: For non-horizontal joins ...
		//    1. Jr.OutPt1.Pt.y == Jr.OutPt2.Pt.y
		//    2. Jr.OutPt1.Pt > Jr.OffPt.y

		// make sure the polygons are correctly oriented ...
		op1b = op1->Next;
		while (all(*op1b == *op1) && any(op1b != op1))
			op1b = op1b->Next;
		bool reverse1 = ((op1b->y > op1->y) || !SlopesEqual(*op1, *op1b, j, use_full_range));

		if (reverse1) {
			op1b = op1->Prev;
			while (all(*op1b == *op1) && any(op1b != op1))
				op1b = op1b->Prev;
			if ((op1b->y > op1->y) || !SlopesEqual(*op1, *op1b, j, use_full_range))
				return false;
		};
		op2b = op2->Next;
		while (all(*op2b == *op2) && any(op2b != op2))
			op2b = op2b->Next;
		bool reverse2 = ((op2b->y > op2->y) || !SlopesEqual(*op2, *op2b, j, use_full_range));

		if (reverse2) {
			op2b = op2->Prev;
			while (all(*op2b == *op2) && any(op2b != op2))
				op2b = op2b->Prev;
			if ((op2b->y > op2->y) || !SlopesEqual(*op2, *op2b, j, use_full_range))
				return false;
		}

		if (op1b == op1 || op2b == op2 || op1b == op2b || (outRec1 == outRec2 && reverse1 == reverse2))
			return false;

		if (reverse1) {
			op1b	   = DupOutPt(op1, false);
			op2b	   = DupOutPt(op2, true);
			op1->Prev  = op2;
			op2->Next  = op1;
			op1b->Next = op2b;
			op2b->Prev = op1b;
			j.OutPt1  = op1;
			j.OutPt2  = op1b;
			return true;
		} else {
			op1b	   = DupOutPt(op1, true);
			op2b	   = DupOutPt(op2, false);
			op1->Next  = op2;
			op2->Prev  = op1;
			op1b->Prev = op2b;
			op2b->Next = op1b;
			j.OutPt1  = op1;
			j.OutPt2  = op1b;
			return true;
		}
	}
}
//----------------------------------------------------------------------

static OutRec* ParseFirstLeft(OutRec* FirstLeft) {
	while (FirstLeft && !FirstLeft->Pts)
		FirstLeft = FirstLeft->FirstLeft;
	return FirstLeft;
}

void Clipper::FixupFirstLefts1(OutRec* OldOutRec, OutRec* NewOutRec) {
	// tests if NewOutRec contains the polygon before reassigning FirstLeft
	for (auto i : poly_outs) {
		OutRec* firstLeft = ParseFirstLeft(i->FirstLeft);
		if (i->Pts && firstLeft == OldOutRec) {
			if (Poly2ContainsPoly1(i->Pts, NewOutRec->Pts))
				i->FirstLeft = NewOutRec;
		}
	}
}

void Clipper::FixupFirstLefts2(OutRec* InnerOutRec, OutRec* OuterOutRec) {
	// A polygon has split into two such that one is now the inner of the other.
	// It's possible that these polygons now wrap around other polygons, so check every polygon that's also contained by OuterOutRec's FirstLeft container (including 0) to see if they've become inner to the new inner polygon ...
	OutRec* orfl = OuterOutRec->FirstLeft;
	for (auto i : poly_outs) {
		if (i->Pts && i != OuterOutRec && i != InnerOutRec) {
			OutRec* firstLeft = ParseFirstLeft(i->FirstLeft);
			if (firstLeft == orfl || firstLeft == InnerOutRec || firstLeft == OuterOutRec) {
				if (Poly2ContainsPoly1(i->Pts, InnerOutRec->Pts))
					i->FirstLeft = InnerOutRec;
				else if (Poly2ContainsPoly1(i->Pts, OuterOutRec->Pts))
					i->FirstLeft = OuterOutRec;
				else if (i->FirstLeft == InnerOutRec || i->FirstLeft == OuterOutRec)
					i->FirstLeft = orfl;
			}
		}
	}
}

void Clipper::FixupFirstLefts3(OutRec* OldOutRec, OutRec* NewOutRec) {
	// reassigns FirstLeft WITHOUT testing if NewOutRec contains the polygon
	for (auto i : poly_outs) {
		OutRec* firstLeft = ParseFirstLeft(i->FirstLeft);
		if (i->Pts && firstLeft == OldOutRec)
			i->FirstLeft = NewOutRec;
	}
}
//----------------------------------------------------------------------

void Clipper::JoinCommonEdges() {
	for (auto &join : joins) {
		OutRec* outRec1 = GetOutRec(join.OutPt1->index);
		OutRec* outRec2 = GetOutRec(join.OutPt2->index);

		if (!outRec1->Pts || !outRec2->Pts)
			continue;
		if (outRec1->is_open || outRec2->is_open)
			continue;

		// get the polygon fragment with the correct hole state (FirstLeft)/ before calling JoinPoints() ...
		OutRec* holeStateRec	= outRec1 == outRec2	? outRec1
			:	OutRec1RightOfOutRec2(outRec1, outRec2)	? outRec2
			:	OutRec1RightOfOutRec2(outRec2, outRec1)	? outRec1
			:	GetLowermostRec(outRec1, outRec2);

		if (!JoinPoints(join, outRec1, outRec2))
			continue;

		if (outRec1 == outRec2) {
			// instead of joining two polygons, we've just created a new one by
			// splitting one polygon into two.
			outRec1->Pts	  = join.OutPt1;
			outRec1->BottomPt = 0;
			outRec2			  = CreateOutRec();
			outRec2->Pts	  = join.OutPt2;

			// update all OutRec2.Pts index's ...
			UpdateOutPtIdxs(*outRec2);

			if (Poly2ContainsPoly1(outRec2->Pts, outRec1->Pts)) {
				// outRec1 contains outRec2 ...
				outRec2->IsHole	   = !outRec1->IsHole;
				outRec2->FirstLeft = outRec1;

				if (using_polytree)
					FixupFirstLefts2(outRec2, outRec1);

				if ((outRec2->IsHole ^ reverse_output) == (Area(*outRec2) > 0))
					ReversePolyPtLinks(outRec2->Pts);

			} else if (Poly2ContainsPoly1(outRec1->Pts, outRec2->Pts)) {
				// outRec2 contains outRec1 ...
				outRec2->IsHole	   = outRec1->IsHole;
				outRec1->IsHole	   = !outRec2->IsHole;
				outRec2->FirstLeft = outRec1->FirstLeft;
				outRec1->FirstLeft = outRec2;

				if (using_polytree)
					FixupFirstLefts2(outRec1, outRec2);

				if ((outRec1->IsHole ^ reverse_output) == (Area(*outRec1) > 0))
					ReversePolyPtLinks(outRec1->Pts);

			} else {
				// the 2 polygons are completely separate ...
				outRec2->IsHole	   = outRec1->IsHole;
				outRec2->FirstLeft = outRec1->FirstLeft;

				// fixup FirstLeft pointers that may need reassigning to OutRec2
				if (using_polytree)
					FixupFirstLefts1(outRec1, outRec2);
			}

		} else {
			// joined 2 polygons together ...
			outRec2->Pts	  = 0;
			outRec2->BottomPt = 0;
			outRec2->index	  = outRec1->index;

			outRec1->IsHole = holeStateRec->IsHole;
			if (holeStateRec == outRec2)
				outRec1->FirstLeft = outRec2->FirstLeft;
			outRec2->FirstLeft = outRec1;

			if (using_polytree)
				FixupFirstLefts3(outRec2, outRec1);
		}
	}
}

//------------------------------------------------------------------------------
// ClipperOffset support functions ...
//------------------------------------------------------------------------------

double2p GetUnitNormal(const IntPoint& pt1, const IntPoint& pt2) {
	if (pt2.x == pt1.x && pt2.y == pt1.y)
		return zero;

	double dx = (double)(pt2.x - pt1.x);
	double dy = (double)(pt2.y - pt1.y);
	double f  = rsqrt(dx * dx + dy * dy);
	return double2p{dy * f, -dx * f};
}

//------------------------------------------------------------------------------
// ClipperOffset class
//------------------------------------------------------------------------------

void ClipperOffset::AddPath(const Path& path, JoinType joinType, EndType endType) {
	int highI = (int)path.size() - 1;
	if (highI < 0)
		return;
	PolyNode* newNode	= new PolyNode();
	newNode->join_type = joinType;
	newNode->end_type	= endType;

	// strip duplicate points from path and also get index to the lowest point ...
	if (endType == etClosedLine || endType == etClosedPolygon)
		while (highI > 0 && all(path[0] == path[highI]))
			highI--;
	newNode->contour.reserve(highI + 1);
	newNode->contour.push_back(path[0]);
	int j = 0, k = 0;
	for (int i = 1; i <= highI; i++)
		if (any(newNode->contour[j] != path[i])) {
			j++;
			newNode->contour.push_back(path[i]);
			if (path[i].y > newNode->contour[k].y || (path[i].y == newNode->contour[k].y && path[i].x < newNode->contour[k].x))
				k = j;
		}
	if (endType == etClosedPolygon && j < 2) {
		delete newNode;
		return;
	}
	m_polyNodes.AddChild(*newNode);

	// if this path's lowest pt is lower than all the others then update m_lowest
	if (endType != etClosedPolygon)
		return;
	if (m_lowest.x < 0) {
		m_lowest = IntPoint(m_polyNodes.ChildCount() - 1, k);
	} else {
		IntPoint ip = m_polyNodes.children[(int)m_lowest.x]->contour[(int)m_lowest.y];
		if (newNode->contour[k].y > ip.y || (newNode->contour[k].y == ip.y && newNode->contour[k].x < ip.x))
			m_lowest = IntPoint(m_polyNodes.ChildCount() - 1, k);
	}
}

void ClipperOffset::AddPaths(const Paths& paths, JoinType joinType, EndType endType) {
	for (auto i : paths)
		AddPath(i, joinType, endType);
}

void ClipperOffset::FixOrientations() {
	// fixup orientations of all closed paths if the orientation of the closed path with the lowermost vertex is wrong ...
	if (m_lowest.x >= 0 && Area(m_polyNodes.children[(int)m_lowest.x]->contour) < 0) {
		for (auto node : m_polyNodes.children) {
			if (node->end_type == etClosedPolygon || (node->end_type == etClosedLine && Area(node->contour) >= 0))
				ReversePath(node->contour);
		}
	} else {
		for (auto node : m_polyNodes.children) {
			if (node->end_type == etClosedLine && Area(node->contour) < 0)
				ReversePath(node->contour);
		}
	}
}

Paths ClipperOffset::Execute(double delta) {
	FixOrientations();
	DoOffset(delta);

	// now clean up 'corners' ...
	Clipper clpr;
	clpr.AddPaths(m_destPolys, ptSubject, true);
	if (delta > 0)
		return clpr.Execute(ctUnion, pftPositive, pftPositive);

	IntRect r = clpr.GetBounds();
	Path	outer(4);
	outer[0] = IntPoint(r.left - 10, r.bottom + 10);
	outer[1] = IntPoint(r.right + 10, r.bottom + 10);
	outer[2] = IntPoint(r.right + 10, r.top - 10);
	outer[3] = IntPoint(r.left - 10, r.top - 10);

	clpr.AddPath(outer, ptSubject, true);
	clpr.reverse_output = true;
	clpr.Execute(ctUnion, pftNegative, pftNegative);
	return none;
}

PolyTree ClipperOffset::ExecuteTree(double delta) {
	FixOrientations();
	DoOffset(delta);

	// now clean up 'corners' ...
	Clipper clpr;
	clpr.AddPaths(m_destPolys, ptSubject, true);
	if (delta > 0)
		return clpr.ExecuteTree(ctUnion, pftPositive, pftPositive);

	IntRect r = clpr.GetBounds();
	Path	outer(4);
	outer[0] = IntPoint(r.left - 10, r.bottom + 10);
	outer[1] = IntPoint(r.right + 10, r.bottom + 10);
	outer[2] = IntPoint(r.right + 10, r.top - 10);
	outer[3] = IntPoint(r.left - 10, r.top - 10);

	clpr.AddPath(outer, ptSubject, true);
	clpr.reverse_output = true;
	auto	solution = clpr.ExecuteTree(ctUnion, pftNegative, pftNegative);
	// remove the outer PolyNode rectangle ...
	if (solution.ChildCount() == 1 && solution.children[0]->ChildCount() > 0) {
		PolyNode* outerNode = solution.children[0];
		solution.children.reserve(outerNode->ChildCount());
		solution.children[0]			= outerNode->children[0];
		solution.children[0]->parent	= outerNode->parent;
		for (int i = 1; i < outerNode->ChildCount(); ++i)
			solution.AddChild(*outerNode->children[i]);
	} else {
		solution.Clear();
	}
	return solution;
}

void ClipperOffset::DoOffset(double delta) {
	m_destPolys.clear();
	m_delta = delta;

	// if Zero offset, just copy any CLOSED polygons to m_p and return ...
	if (abs(delta) < TOLERANCE) {
		m_destPolys.reserve(m_polyNodes.ChildCount());
		for (int i = 0; i < m_polyNodes.ChildCount(); i++) {
			PolyNode& node = *m_polyNodes.children[i];
			if (node.end_type == etClosedPolygon)
				m_destPolys.push_back(node.contour);
		}
		return;
	}

	// see offset_triginometry3.svg in the documentation folder ...
	m_miterLim = MiterLimit > 2 ? 2 / (MiterLimit * MiterLimit) : 0.5;

	double y =	ArcTolerance <= 0.0								? def_arc_tolerance
			:	ArcTolerance > abs(delta) * def_arc_tolerance	? abs(delta) * def_arc_tolerance
			:	ArcTolerance;

	// see offset_triginometry2.svg in the documentation folder ...
	double steps = pi / iso::acos(1 - y / abs(delta));
	if (steps > abs(delta) * pi)
		steps = abs(delta) * pi;	// ie excessive precision check

	m_sin		  = iso::sin(two_pi / steps);
	m_cos		  = iso::cos(two_pi / steps);
	m_StepsPerRad = steps / two_pi;
	if (delta < 0.0)
		m_sin = -m_sin;

	m_destPolys.reserve(m_polyNodes.ChildCount() * 2);
	for (int i = 0; i < m_polyNodes.ChildCount(); i++) {
		PolyNode& node = *m_polyNodes.children[i];
		m_srcPoly	   = node.contour;

		int len = (int)m_srcPoly.size();
		if (len == 0 || (delta <= 0 && (len < 3 || node.end_type != etClosedPolygon)))
			continue;

		m_destPoly.clear();
		if (len == 1) {
			if (node.join_type == jtRound) {
				double x = 1.0, y = 0.0;
				for (cInt j = 1; j <= steps; j++) {
					m_destPoly.push_back(IntPoint(Round(m_srcPoly[0].x + x * delta), Round(m_srcPoly[0].y + y * delta)));
					double X2 = x;
					x		  = x * m_cos - m_sin * y;
					y		  = X2 * m_sin + y * m_cos;
				}
			} else {
				double x = -1.0, y = -1.0;
				for (int j = 0; j < 4; ++j) {
					m_destPoly.push_back(IntPoint(Round(m_srcPoly[0].x + x * delta), Round(m_srcPoly[0].y + y * delta)));
					if (x < 0)
						x = 1;
					else if (y < 0)
						y = 1;
					else
						x = -1;
				}
			}
			m_destPolys.push_back(m_destPoly);
			continue;
		}
		// build m_normals ...
		m_normals.clear();
		m_normals.reserve(len);
		for (int j = 0; j < len - 1; ++j)
			m_normals.push_back(GetUnitNormal(m_srcPoly[j], m_srcPoly[j + 1]));

		m_normals.push_back(node.end_type == etClosedLine || node.end_type == etClosedPolygon ? GetUnitNormal(m_srcPoly[len - 1], m_srcPoly[0]) : double2p(m_normals[len - 2]));

		if (node.end_type == etClosedPolygon) {
			int k = len - 1;
			for (int j = 0; j < len; ++j)
				OffsetPoint(j, k, node.join_type);
			m_destPolys.push_back(m_destPoly);

		} else if (node.end_type == etClosedLine) {
			int k = len - 1;
			for (int j = 0; j < len; ++j)
				OffsetPoint(j, k, node.join_type);
			m_destPolys.push_back(m_destPoly);
			m_destPoly.clear();
			// re-build m_normals ...
			double2p n = m_normals[len - 1];
			for (int j = len - 1; j > 0; j--)
				m_normals[j] = double2p{-m_normals[j - 1].x, -m_normals[j - 1].y};
			m_normals[0] = double2p{-n.x, -n.y};
			k	= 0;
			for (int j = len - 1; j >= 0; j--)
				OffsetPoint(j, k, node.join_type);
			m_destPolys.push_back(m_destPoly);

		} else {
			int k = 0;
			for (int j = 1; j < len - 1; ++j)
				OffsetPoint(j, k, node.join_type);

			IntPoint pt1;
			if (node.end_type == etOpenButt) {
				int j = len - 1;
				pt1	  = IntPoint((cInt)Round(m_srcPoly[j].x + m_normals[j].x * delta), (cInt)Round(m_srcPoly[j].y + m_normals[j].y * delta));
				m_destPoly.push_back(pt1);
				pt1 = IntPoint((cInt)Round(m_srcPoly[j].x - m_normals[j].x * delta), (cInt)Round(m_srcPoly[j].y - m_normals[j].y * delta));
				m_destPoly.push_back(pt1);
			} else {
				int j		 = len - 1;
				k			 = len - 2;
				m_sinA		 = 0;
				m_normals[j] = double2p{-m_normals[j].x, -m_normals[j].y};
				if (node.end_type == etOpenSquare)
					DoSquare(j, k);
				else
					DoRound(j, k);
			}

			// re-build m_normals ...
			for (int j = len - 1; j > 0; j--)
				m_normals[j] = double2p{-m_normals[j - 1].x, -m_normals[j - 1].y};
			m_normals[0] = double2p{-m_normals[1].x, -m_normals[1].y};

			k = len - 1;
			for (int j = k - 1; j > 0; --j)
				OffsetPoint(j, k, node.join_type);

			if (node.end_type == etOpenButt) {
				pt1 = IntPoint((cInt)Round(m_srcPoly[0].x - m_normals[0].x * delta), (cInt)Round(m_srcPoly[0].y - m_normals[0].y * delta));
				m_destPoly.push_back(pt1);
				pt1 = IntPoint((cInt)Round(m_srcPoly[0].x + m_normals[0].x * delta), (cInt)Round(m_srcPoly[0].y + m_normals[0].y * delta));
				m_destPoly.push_back(pt1);
			} else {
				k	   = 1;
				m_sinA = 0;
				if (node.end_type == etOpenSquare)
					DoSquare(0, 1);
				else
					DoRound(0, 1);
			}
			m_destPolys.push_back(m_destPoly);
		}
	}
}
//------------------------------------------------------------------------------

void ClipperOffset::OffsetPoint(int j, int& k, JoinType jointype) {
	// cross product ...
	m_sinA = (m_normals[k].x * m_normals[j].y - m_normals[j].x * m_normals[k].y);
	if (abs(m_sinA * m_delta) < 1.0) {
		// dot product ...
		double cosA = (m_normals[k].x * m_normals[j].x + m_normals[j].y * m_normals[k].y);
		if (cosA > 0) { // angle => 0 degrees
			m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + m_normals[k].x * m_delta), Round(m_srcPoly[j].y + m_normals[k].y * m_delta)));
			return;
		}
		// else angle => 180 degrees
	} else if (m_sinA > 1.0) {
		m_sinA = 1.0;
	} else if (m_sinA < -1.0) {
		m_sinA = -1.0;
	}

	if (m_sinA * m_delta < 0) {
		m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + m_normals[k].x * m_delta), Round(m_srcPoly[j].y + m_normals[k].y * m_delta)));
		m_destPoly.push_back(m_srcPoly[j]);
		m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + m_normals[j].x * m_delta), Round(m_srcPoly[j].y + m_normals[j].y * m_delta)));
	} else
		switch (jointype) {
			case jtMiter: {
				double r = 1 + (m_normals[j].x * m_normals[k].x + m_normals[j].y * m_normals[k].y);
				if (r >= m_miterLim)
					DoMiter(j, k, r);
				else
					DoSquare(j, k);
				break;
			}
			case jtSquare: DoSquare(j, k); break;
			case jtRound: DoRound(j, k); break;
		}
	k = j;
}
//------------------------------------------------------------------------------

void ClipperOffset::DoSquare(int j, int k) {
	double dx = iso::tan(iso::atan2(m_sinA, m_normals[k].x * m_normals[j].x + m_normals[k].y * m_normals[j].y) / 4);
	m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + m_delta * (m_normals[k].x - m_normals[k].y * dx)), Round(m_srcPoly[j].y + m_delta * (m_normals[k].y + m_normals[k].x * dx))));
	m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + m_delta * (m_normals[j].x + m_normals[j].y * dx)), Round(m_srcPoly[j].y + m_delta * (m_normals[j].y - m_normals[j].x * dx))));
}
//------------------------------------------------------------------------------

void ClipperOffset::DoMiter(int j, int k, double r) {
	double q = m_delta / r;
	m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + (m_normals[k].x + m_normals[j].x) * q), Round(m_srcPoly[j].y + (m_normals[k].y + m_normals[j].y) * q)));
}
//------------------------------------------------------------------------------

void ClipperOffset::DoRound(int j, int k) {
	double a		= iso::atan2(m_sinA, m_normals[k].x * m_normals[j].x + m_normals[k].y * m_normals[j].y);
	int	   steps	= max((int)Round(m_StepsPerRad * abs(a)), 1);
	double x		= m_normals[k].x, y = m_normals[k].y, X2;

	for (int i = 0; i < steps; ++i) {
		m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + x * m_delta), Round(m_srcPoly[j].y + y * m_delta)));
		X2 = x;
		x  = x * m_cos - m_sin * y;
		y  = X2 * m_sin + y * m_cos;
	}
	m_destPoly.push_back(IntPoint(Round(m_srcPoly[j].x + m_normals[j].x * m_delta), Round(m_srcPoly[j].y + m_normals[j].y * m_delta)));
}

//------------------------------------------------------------------------------
// Miscellaneous public functions
//------------------------------------------------------------------------------

void Clipper::DoSimplePolygons() {
	for (auto outrec : poly_outs) {
		OutPt*	op	   = outrec->Pts;
		if (!op || outrec->is_open)
			continue;
		do	// for each Pt in Polygon until duplicate found do ...
		{
			OutPt* op2 = op->Next;
			while (op2 != outrec->Pts) {
				if (all(*op == *op2) && op2->Next != op && op2->Prev != op) {
					// split the polygon into two ...
					OutPt* op3 = op->Prev;
					OutPt* op4 = op2->Prev;
					op->Prev   = op4;
					op4->Next  = op;
					op2->Prev  = op3;
					op3->Next  = op2;

					outrec->Pts		= op;
					OutRec* outrec2 = CreateOutRec();
					outrec2->Pts	= op2;
					UpdateOutPtIdxs(*outrec2);
					if (Poly2ContainsPoly1(outrec2->Pts, outrec->Pts)) {
						// OutRec2 is contained by OutRec1 ...
						outrec2->IsHole	   = !outrec->IsHole;
						outrec2->FirstLeft = outrec;
						if (using_polytree)
							FixupFirstLefts2(outrec2, outrec);
					} else if (Poly2ContainsPoly1(outrec->Pts, outrec2->Pts)) {
						// OutRec1 is contained by OutRec2 ...
						outrec2->IsHole	   = outrec->IsHole;
						outrec->IsHole	   = !outrec2->IsHole;
						outrec2->FirstLeft = outrec->FirstLeft;
						outrec->FirstLeft  = outrec2;
						if (using_polytree)
							FixupFirstLefts2(outrec, outrec2);
					} else {
						// the 2 polygons are separate ...
						outrec2->IsHole	   = outrec->IsHole;
						outrec2->FirstLeft = outrec->FirstLeft;
						if (using_polytree)
							FixupFirstLefts1(outrec, outrec2);
					}
					op2 = op;  // ie get ready for the Next iteration
				}
				op2 = op2->Next;
			}
			op = op->Next;
		} while (op != outrec->Pts);
	}
}

void ReversePath(Path& p) { reverse(p.begin(), p.end()); }

void ReversePaths(Paths& p) {
	for (auto &i : p)
		ReversePath(i);
}

Paths SimplifyPolygon(const Path& in_poly, PolyFillType fillType) {
	Clipper c;
	c.strict_simple = true;
	c.AddPath(in_poly, ptSubject, true);
	return c.Execute(ctUnion, fillType, fillType);
}

Paths SimplifyPolygons(const Paths& in_polys, PolyFillType fillType) {
	Clipper c;
	c.strict_simple = true;
	c.AddPaths(in_polys, ptSubject, true);
	return c.Execute(ctUnion, fillType, fillType);
}

inline double DistanceSqrd(const IntPoint& pt1, const IntPoint& pt2) {
	return square((double)pt1.x - pt2.x) + square((double)pt1.y - pt2.y);
}

double DistanceFromLineSqrd(const IntPoint& pt, const IntPoint& ln1, const IntPoint& ln2) {
	// The equation of a line in general form (Ax + By + C = 0)
	// given 2 points (x¹,y¹) & (x²,y²) is ...
	//(y¹ - y²)x + (x² - x¹)y + (y² - y¹)x¹ - (x² - x¹)y¹ = 0
	// A = (y¹ - y²); B = (x² - x¹); C = (y² - y¹)x¹ - (x² - x¹)y¹
	// perpendicular distance of point (x³,y³) = (Ax³ + By³ + C)/Sqrt(A² + B²)
	// see http://en.wikipedia.org/wiki/Perpendicular_distance
	double A = double(ln1.y - ln2.y);
	double B = double(ln2.x - ln1.x);
	double C = A * ln1.x + B * ln1.y;
	return square(A * pt.x + B * pt.y - C) / (square(A) + square(B));
}

bool SlopesNearCollinear(const IntPoint& pt1, const IntPoint& pt2, const IntPoint& pt3, double distSqrd) {
	// this function is more accurate when the point that's geometrically between the other 2 points is the one that's tested for distance.
	// ie makes it more likely to pick up 'spikes' ...
	return ( abs(pt1.x - pt2.x) > abs(pt1.y - pt2.y)
		? (
			(pt1.x > pt2.x) == (pt1.x < pt3.x) ? DistanceFromLineSqrd(pt1, pt2, pt3)
		:	(pt2.x > pt1.x) == (pt2.x < pt3.x) ? DistanceFromLineSqrd(pt2, pt1, pt3)
		:	DistanceFromLineSqrd(pt3, pt1, pt2)
		) : (
			(pt1.y > pt2.y) == (pt1.y < pt3.y) ? DistanceFromLineSqrd(pt1, pt2, pt3)
		:	(pt2.y > pt1.y) == (pt2.y < pt3.y) ? DistanceFromLineSqrd(pt2, pt1, pt3)
		:	DistanceFromLineSqrd(pt3, pt1, pt2)
		)
	) < distSqrd;
}

//------------------------------------------------------------------------------

bool PointsAreClose(IntPoint pt1, IntPoint pt2, double distSqrd) {
	double dx = (double)pt1.x - pt2.x;
	double dy = (double)pt1.y - pt2.y;
	return ((dx * dx) + (dy * dy) <= distSqrd);
}

OutPt* Exclude(OutPt* op) {
	OutPt* result	= op->Prev;
	result->Next	= op->Next;
	op->Next->Prev	= result;
	result->index	= 0;
	return result;
}

Path CleanPolygon(const Path& in_poly, double distance) {
	Path	out_poly;

	if (size_t size = in_poly.size()) {
		OutPt* outPts = new OutPt[size];
		for (size_t i = 0; i < size; ++i)
			outPts[i] = OutPt(0, in_poly[i], &outPts[(i + 1) % size], &outPts[(i + size - 1) % size]);

		double distSqrd = distance * distance;
		OutPt* op		= &outPts[0];
		while (op->index == 0 && op->Next != op->Prev) {
			if (PointsAreClose(*op, *op->Prev, distSqrd)) {
				op = Exclude(op);
				size--;
			} else if (PointsAreClose(*op->Prev, *op->Next, distSqrd)) {
				Exclude(op->Next);
				op = Exclude(op);
				size -= 2;
			} else if (SlopesNearCollinear(*op->Prev, *op, *op->Next, distSqrd)) {
				op = Exclude(op);
				size--;
			} else {
				op->index = 1;
				op		= op->Next;
			}
		}

		if (size >= 3) {
			out_poly.resize(size);
			for (size_t i = 0; i < size; ++i) {
				out_poly[i] = *op;
				op			= op->Next;
			}
		}
		delete[] outPts;
	}
	return out_poly;
}

Paths CleanPolygons(const Paths& in_polys, double distance) {
	return transformc(in_polys, [distance](const Path &i) { return CleanPolygon(i, distance); });
}

//------------------------------------------------------------------------------

Paths Minkowski(const Path& poly, const Path& path, bool isSum, bool isClosed) {
	Paths  pp;
	if (isSum) {
		pp = transformc(path, [poly](IntPoint i) {
			Path p = transformc(poly, [i](IntPoint j) { return i + j; });
			return p;
		});
	} else {
		pp = transformc(path, [poly](IntPoint i) {
			Path p = transformc(poly, [i](IntPoint j) { return i - j; });
			return p;
		});
	}

	int	   delta   = isClosed ? 1 : 0;
	size_t polyCnt = poly.size();
	size_t pathCnt = path.size();

	Paths	solution;
	solution.reserve((pathCnt + delta) * (polyCnt + 1));

	for (size_t i = 0; i < pathCnt - 1 + delta; ++i) {
		for (size_t j = 0; j < polyCnt; ++j) {
			Path quad(4);
			quad[0] = pp[i % pathCnt][j % polyCnt];
			quad[1] = pp[(i + 1) % pathCnt][j % polyCnt];
			quad[2] = pp[(i + 1) % pathCnt][(j + 1) % polyCnt];
			quad[3] = pp[i % pathCnt][(j + 1) % polyCnt];
			if (Area(quad) < 0)
				ReversePath(quad);
			solution.push_back(quad);
		}
	}

	return solution;
}

Paths MinkowskiSum(const Path& pattern, const Path& path, bool pathIsClosed) {
	Clipper c;
	c.AddPaths(Minkowski(pattern, path, true, pathIsClosed), ptSubject, true);
	return c.Execute(ctUnion, pftNonZero, pftNonZero);
}

Paths MinkowskiSum(const Path& pattern, const Paths& paths, bool pathIsClosed) {
	Clipper c;
	IntPoint delta	= pattern[0];
	for (auto &i : paths) {
		c.AddPaths(Minkowski(pattern, i, true, pathIsClosed), ptSubject, true);
		if (pathIsClosed)
			c.AddPath(transformc(i, [delta](IntPoint i) { return i + delta; }), ptClip, true);
	}
	return c.Execute(ctUnion, pftNonZero, pftNonZero);
}

Paths MinkowskiDiff(const Path& poly1, const Path& poly2) {
	Clipper c;
	c.AddPaths(Minkowski(poly1, poly2, false, true), ptSubject, true);
	return c.Execute(ctUnion, pftNonZero, pftNonZero);
}
//------------------------------------------------------------------------------

void AddPolyNodeToPaths(const PolyNode& polynode, bool closed_only, Paths& paths) {
	if (!polynode.contour.empty() && (!closed_only || !polynode.is_open))
		paths.push_back(polynode.contour);

	for (auto i : polynode.children)
		AddPolyNodeToPaths(*i, closed_only, paths);
}

Paths PolyTreeToPaths(const PolyTree& polytree) {
	Paths	paths;
	AddPolyNodeToPaths(polytree, false, paths);
	return paths;
}

Paths ClosedPathsFromPolyTree(const PolyTree& polytree) {
	Paths	paths;
	AddPolyNodeToPaths(polytree, true, paths);
	return paths;
}

Paths OpenPathsFromPolyTree(PolyTree& polytree) {
	Paths	paths;
	// Open paths are top level only, so ...
	for (auto i : polytree.children)
		if (i->is_open)
			paths.push_back(i->contour);
	return paths;
}

//------------------------------------------------------------------------------

}  // namespace ClipperLib
