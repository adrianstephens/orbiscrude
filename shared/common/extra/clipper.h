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

#ifndef CLIPPER_H
#define CLIPPER_H

#include "base/vector.h"
#include "base/array.h"
#include "base/list.h"
#include "base/algorithm.h"

// use_int32: When enabled 32bit ints are used instead of 64bit ints. This
// improve performance but coordinate values are limited to the range +/- 46340
//#define use_int32

// use_lines: Enables line clipping. Adds a very minor cost to performance.
#define use_lines

#define HORIZONTAL		(-1.0E+40)
#define TOLERANCE		(1.0e-20)

namespace ClipperLib {
using namespace iso;

enum ClipType		{ ctIntersection, ctUnion, ctDifference, ctXor };
enum PolyType		{ ptSubject, ptClip };
enum PolyFillType	{ pftEvenOdd, pftNonZero, pftPositive, pftNegative };
enum InitOptions	{ ioReverseSolution = 1, ioStrictlySimple = 2, ioPreserveCollinear = 4 };
enum JoinType		{ jtSquare, jtRound, jtMiter };
enum EndType		{ etClosedPolygon, etClosedLine, etOpenButt, etOpenSquare, etOpenRound };
enum Direction		{ dRightToLeft, dLeftToRight };
enum EdgeSide		{ esLeft, esRight };

#ifdef use_int32
typedef int			cInt;
static const cInt	loRange = 0x7FFF;
static const cInt	hiRange = 0x7FFF;
#else
typedef int64		cInt;
static const cInt	loRange = 0x3FFFFFFF;
static const cInt	hiRange = 0x3FFFFFFFFFFFFFFFLL;
#endif

inline cInt Round(double val) {
	return val < 0
		? static_cast<cInt>(val - 0.5)
		: static_cast<cInt>(val + 0.5);
}

typedef array_vec<cInt, 2> IntPoint;
typedef dynamic_array<IntPoint>	Path;
typedef dynamic_array<Path>		Paths;

class PolyNode {
public:
	friend class Clipper;	// to access index
	friend class ClipperOffset;
	unsigned  index;		// node index in parent.children
	bool	  is_open;
	JoinType  join_type;
	EndType	  end_type;

	PolyNode* GetNextSiblingUp() const {
		return !parent ? 0
			: index == parent->children.size() - 1 ?  parent->GetNextSiblingUp()
			: parent->children[index + 1];
	}
	void	  AddChild(PolyNode& child) {
		unsigned cnt = (unsigned)children.size();
		children.push_back(&child);
		child.parent = this;
		child.index	 = cnt;
	}
public:
	PolyNode() : index(0), is_open(false), parent(0) {}
	Path						contour;
	dynamic_array<PolyNode*>	children;
	PolyNode*					parent;

	PolyNode*	GetNext() const {
		return !children.empty() ? children[0] : GetNextSiblingUp();
	}
	bool		IsHole() const {
		bool	  result = true;
		PolyNode* node	 = parent;
		while (node) {
			result = !result;
			node   = node->parent;
		}
		return result;
	}
	int			ChildCount()	const { return (int)children.size(); }
};

class PolyTree : public PolyNode {
	friend class Clipper;  // to access AllNodes
	dynamic_array<PolyNode*> AllNodes;
public:
	~PolyTree() { Clear(); };
	PolyNode*	GetFirst() const {
		return !children.empty() ? children[0] : 0;
	}
	void		Clear() {
		for (auto i : AllNodes)
			delete i;
		AllNodes.resize(0);
		children.resize(0);
	}
	int			Total() const {
		int result = (int)AllNodes.size();
		// with negative offsets, ignore the hidden outer polygon ...
		if (result > 0 && children[0] != AllNodes[0])
			result--;
		return result;
	}
};

double	Area(const Path& poly);
int		PointInPolygon(const IntPoint& pt, const Path& path);

Paths	SimplifyPolygon(const Path& in_poly, PolyFillType fillType = pftEvenOdd);
Paths	SimplifyPolygons(const Paths& in_polys, PolyFillType fillType = pftEvenOdd);

Path	CleanPolygon(const Path& in_poly, double distance = 1.415);
Paths	CleanPolygons(const Paths& in_polys, double distance = 1.415);

Paths	MinkowskiSum(const Path& pattern, const Path& path, bool pathIsClosed);
Paths	MinkowskiSum(const Path& pattern, const Paths& paths, bool pathIsClosed);
Paths	MinkowskiDiff(const Path& poly1, const Path& poly2);

Paths	PolyTreeToPaths(const PolyTree& polytree);
Paths	ClosedPathsFromPolyTree(const PolyTree& polytree);
Paths	OpenPathsFromPolyTree(PolyTree& polytree);

void	ReversePath(Path& p);
void	ReversePaths(Paths& p);

//------------------------------------------------------------------------------
//	internal structures
//------------------------------------------------------------------------------

struct IntRect {
	cInt	left;
	cInt	top;
	cInt	right;
	cInt	bottom;
};

struct OutPt : IntPoint {
	int			index;
	OutPt*		Next;
	OutPt*		Prev;
	OutPt() {}
	OutPt(int index, IntPoint Pt) : IntPoint(Pt), index(index), Next(this), Prev(this) {}
	OutPt(int index, IntPoint Pt, OutPt *Next, OutPt *Prev) : IntPoint(Pt), index(index), Next(Next), Prev(Prev) {}
};

struct Join : IntPoint {
	OutPt*		OutPt1;
	OutPt*		OutPt2;
	Join(OutPt* OutPt1, OutPt* OutPt2, IntPoint OffPt) : IntPoint(OffPt), OutPt1(OutPt1), OutPt2(OutPt2) {}
};

// OutRec: contains a path in the clipping solution. Edges in the AEL will carry a pointer to an OutRec when they are part of the clipping solution
struct OutRec {
	int			index;
	bool		IsHole;
	bool		is_open;
	OutRec*		FirstLeft;
	PolyNode*	PolyNd;
	OutPt*		Pts;
	OutPt*		BottomPt;
};

//------------------------------------------------------------------------------
// TEdge
//------------------------------------------------------------------------------

struct TEdge {
	static int const Unassigned		= -1;	// edge not currently 'owning' a solution
	static int const Skip			= -2;	// edge that would otherwise close a path

	IntPoint	bot;
	IntPoint	curr;		// current (updated for every new scanbeam)
	IntPoint	top;
	double		dx;
	PolyType	type;
	EdgeSide	side;		// side only refers to current side of solution poly
	int			WindDelta;	// 1 or -1 depending on winding direction
	int			wind;
	int			wind2;		// winding count of the opposite polytype
	int			OutIdx;
	TEdge		*Next, *Prev;
	TEdge		*NextInLML;
	TEdge		*NextInAEL, *PrevInAEL;
	TEdge		*NextInSEL, *PrevInSEL;

	bool	IsHorizontal()				const	{ return dx == HORIZONTAL; }
	TEdge*	GetNextInAEL(Direction dir) const	{ return dir == dLeftToRight ? NextInAEL : PrevInAEL; }
	cInt	TopX(const cInt currentY)	const	{ return currentY == top.y ? cInt(top.x) : cInt(bot.x + Round(dx * (currentY - bot.y))); }

	void GetHorzDirection(Direction& Dir, cInt& Left, cInt& Right) {
		if (bot.x < top.x) {
			Left  = bot.x;
			Right = top.x;
			Dir	  = dLeftToRight;
		} else {
			Left  = top.x;
			Right = bot.x;
			Dir	  = dRightToLeft;
		}
	}

	void InitEdge(TEdge* eNext, TEdge* ePrev, const IntPoint& Pt) {
		clear(*this);
		Next	= eNext;
		Prev	= ePrev;
		curr	= Pt;
		OutIdx	= Unassigned;
	}
	void InitEdge2(PolyType Pt) {
		type = Pt;
		if (curr.y >= Next->curr.y) {
			bot = curr;
			top = Next->curr;
		} else {
			top = curr;
			bot = Next->curr;
		}
		cInt dy = (top.y - bot.y);
		dx = dy == 0 ? HORIZONTAL : double(top.x - bot.x) / dy;
	}
};

struct IntersectNode : IntPoint {
	TEdge*		Edge1;
	TEdge*		Edge2;
	IntersectNode(TEdge *Edge1, TEdge *Edge2, IntPoint Pt) : IntPoint(Pt), Edge1(Edge1), Edge2(Edge2) {}
	bool EdgesAdjacent() const { return Edge1->NextInSEL == Edge2 || Edge1->PrevInSEL == Edge2; }
};

struct LocalMinimum {
	cInt		y;
	TEdge*		LeftBound;
	TEdge*		RightBound;
	LocalMinimum(cInt y, TEdge *LeftBound, TEdge *RightBound) : y(y), LeftBound(LeftBound), RightBound(RightBound) {}
};

//------------------------------------------------------------------------------

// ClipperBase is the ancestor to the Clipper class. It should not be instantiated directly
// This class simply abstracts the conversion of sets of polygon coordinates into edge objects that are stored in a LocalMinima list

class ClipperBase {
protected:
	dynamic_array<LocalMinimum> minima;
	LocalMinimum				*local_min;
	bool						use_full_range;
	dynamic_array<TEdge*>		m_edges;
	bool						preserve_colinear;
	bool						has_open_paths;
	dynamic_array<OutRec*>		poly_outs;
	TEdge*						aet;
	priority_queue<dynamic_array<cInt>>	scanbeam;

	void		DisposeLocalMinimaList();
	void		Reset();
	TEdge*		ProcessBound(TEdge* E, bool IsClockwise);
	void		InsertScanbeam(const cInt y);
	bool		PopScanbeam(cInt& y);
	bool		LocalMinimaPending()	const { return local_min != minima.end(); }
	bool		PopLocalMinima(cInt y, const LocalMinimum*& locMin);
	OutRec*		CreateOutRec();
	void		DisposeAllOutRecs();
	void		SwapPositionsInAEL(TEdge* edge1, TEdge* edge2);
	void		DeleteFromAEL(TEdge* e);
	bool		UpdateEdgeIntoAEL(TEdge*& e);


public:
	ClipperBase() :	local_min(0), use_full_range(false) {}
	virtual ~ClipperBase() { Clear(); }

	virtual bool AddPath(const Path& pg, PolyType type, bool Closed);
	bool		 AddPaths(const Paths& ppg, PolyType type, bool Closed);
	virtual void Clear();
	IntRect		 GetBounds();
};

class Clipper : public ClipperBase {
//protected:
public:
	dynamic_array<Join>				joins;
	dynamic_array<Join>				ghost_joins;
	dynamic_array<IntersectNode*>	intersections;
	ClipType						op;
	list<cInt>						maxima;
	TEdge*							sorted_edges;
	PolyFillType					clip_fill_type;
	PolyFillType					subj_fill_type;
	bool							reverse_output;
	bool							using_polytree;
	bool							strict_simple;

	void	SetWindingCount(TEdge& edge);
	bool	IsEvenOddFillType(const TEdge& edge) const {
		return (edge.type == ptSubject ? subj_fill_type : clip_fill_type) == pftEvenOdd;
	}
	bool	IsEvenOddAltFillType(const TEdge& edge) const {
		return (edge.type == ptSubject ? clip_fill_type : subj_fill_type) == pftEvenOdd;
	}
	void	InsertLocalMinimaIntoAEL(const cInt botY);
	void	InsertEdgeIntoAEL(TEdge* edge, TEdge* startEdge);
	void	AddEdgeToSEL(TEdge* edge);
	bool	PopEdgeFromSEL(TEdge*& edge);
	void	CopyAELToSEL();
	void	DeleteFromSEL(TEdge* e);
	void	SwapPositionsInSEL(TEdge* edge1, TEdge* edge2);
	bool	IsContributing(const TEdge& edge) const;
	bool	DoMaxima(TEdge* e);
	bool	ProcessHorizontals();
	bool	ProcessHorizontal(TEdge* horzEdge);
	void	AddLocalMaxPoly(TEdge* e1, TEdge* e2, const IntPoint& pt);
	OutPt*	AddLocalMinPoly(TEdge* e1, TEdge* e2, const IntPoint& pt);
	OutRec* GetOutRec(int idx);
	void	AppendPolygon(TEdge* e1, TEdge* e2);
	void	IntersectEdges(TEdge* e1, TEdge* e2, const IntPoint& pt);
	OutPt*	AddOutPt(TEdge* e, const IntPoint& pt);
	OutPt*	GetLastOutPt(TEdge* e);
	bool	ProcessIntersections(const cInt topY);
	void	BuildIntersectList(const cInt topY);
	void	ProcessIntersectList();
	bool	ProcessEdgesAtTopOfScanbeam(const cInt topY);
	void	BuildResult(Paths& polys);
	void	BuildResult2(PolyTree& polytree);
	void	SetHoleState(TEdge* e, OutRec* outrec);
	void	DisposeIntersectNodes();
	bool	FixupIntersectionOrder();
	void	FixupOutPolygon(OutRec& outrec);
	void	FixupOutPolyline(OutRec& outrec);
	void	FixHoleLinkage(OutRec& outrec);
	void	AddJoin(OutPt* op1, OutPt* op2, const IntPoint offPt) {
		joins.emplace_back(op1, op2, offPt);
	}
	void	AddGhostJoin(OutPt* op, const IntPoint offPt) {
		ghost_joins.emplace_back(op, nullptr, offPt);
	}
	bool	JoinPoints(Join &j, OutRec* outRec1, OutRec* outRec2);
	void	JoinCommonEdges();
	void	DoSimplePolygons();
	void	FixupFirstLefts1(OutRec* OldOutRec, OutRec* NewOutRec);
	void	FixupFirstLefts2(OutRec* InnerOutRec, OutRec* OuterOutRec);
	void	FixupFirstLefts3(OutRec* OldOutRec, OutRec* NewOutRec);

	virtual bool ExecuteInternal();

public:
	Clipper(int initOptions = 0) : ClipperBase() {
		reverse_output		= !!(initOptions & ioReverseSolution);
		strict_simple		= !!(initOptions & ioStrictlySimple);
		preserve_colinear	= !!(initOptions & ioPreserveCollinear);
		has_open_paths		= false;
	}
	Paths		Execute(ClipType clipType, PolyFillType subjFillType, PolyFillType clipFillType);
	PolyTree	ExecuteTree(ClipType clipType, PolyFillType subjFillType, PolyFillType clipFillType);

	Paths		Execute(ClipType clipType, PolyFillType fillType = pftEvenOdd)		{ return Execute(clipType, fillType, fillType); }
	PolyTree	ExecuteTree(ClipType clipType, PolyFillType fillType = pftEvenOdd)	{ return ExecuteTree(clipType, fillType, fillType); }
};
//------------------------------------------------------------------------------

class ClipperOffset {
	Paths						m_destPolys;
	Path						m_srcPoly;
	Path						m_destPoly;
	dynamic_array<double2p>	m_normals;
	double						m_delta, m_sinA, m_sin, m_cos;
	double						m_miterLim, m_StepsPerRad;
	IntPoint					m_lowest;
	PolyNode					m_polyNodes;

	void	FixOrientations();
	void	DoOffset(double delta);
	void	OffsetPoint(int j, int& k, JoinType jointype);
	void	DoSquare(int j, int k);
	void	DoMiter(int j, int k, double r);
	void	DoRound(int j, int k);

public:
	double	MiterLimit;
	double	ArcTolerance;

	ClipperOffset(double miterLimit = 2.0, double arcTolerance = 0.25) : MiterLimit(miterLimit), ArcTolerance(arcTolerance) { m_lowest.x = -1; }
	~ClipperOffset()	{ Clear(); }
	void	AddPath(const Path& path, JoinType joinType, EndType endType);
	void	AddPaths(const Paths& paths, JoinType joinType, EndType endType);
	Paths		Execute(double delta);
	PolyTree	ExecuteTree(double delta);
	void	Clear() {
		for (auto &i :  m_polyNodes.children)
			delete i;
		m_polyNodes.children.clear();
		m_lowest.x = -1;
	}
};

//------------------------------------------------------------------------------

}  // namespace ClipperLib

#endif	// clipper_hpp
