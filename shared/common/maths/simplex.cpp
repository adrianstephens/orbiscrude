#include "simplex.h"
#include "base/array.h"
#include "base/algorithm.h"
#include "allocators/pool.h"
#include "geometry.h"

using namespace iso;

//calculate the barycentric coorinates for a point in a segment
float barycentricCoordinates(param(float3) p, param(float3) a, param(float3) b) {
	const float3 d	= b - a;
	const float denominator = dot(d, d);
	return select(denominator > zero, -dot(a - p, d) / denominator, zero);
}

//calculate the barycentric coorinates for a point in a triangle
float2 barycentricCoordinates(param(float3) p, param(float3) a, param(float3) b, param(float3) c) {
	const float3 ab	= b - a;
	const float3 ac	= c - a;
	const float3 n	= cross(ab, ac);

	const float va = dot(n, cross(b, c));//edge region of BC, signed area rbc, u = S(rbc)/S(abc) for a
	const float vb = dot(n, cross(c, a));//edge region of AC, signed area rac, v = S(rca)/S(abc) for b
	const float vc = dot(n, cross(a, b));//edge region of AB, signed area rab, w = S(rab)/S(abc) for c
	const float totalArea = va + vb + vc;
	return select(totalArea == zero, float2(zero), float2{vb, vc} / totalArea);
}

/*
v0 = b - a;
v1 = c - a;
v2 = p - a;
float2 barycentricCoordinates(param(float3) v0, param(float3) v1, param(float3) v2) {
const float d00 = dot(v0, v0);
const float d01 = dot(v0, v1);
const float d11 = dot(v1, v1);
const float d20 = dot(v2, v0);
const float d21 = dot(v2, v1);
return (float2(d11, d00) * float2(d20, d21) - d01 * float2(d21, d20)) / (d00 * d11 - d01 * d01);
}
*/

bool isValidTriangleBarycentricCoord(const float2 v) {
	auto	one_eps = one + epsilon;
	return all(v >= epsilon && one_eps >= v) && one_eps > (v.x + v.y);
}

int PointOutsideOfPlane4(param(float3) _a, param(float3) _b, param(float3) _c, param(float3) _d) {
	// this is not 0 because of the following scenario:
	// All the points lie on the same plane and the plane goes through the origin (0,0,0).
	// On the Wii U, the math below has the problem that when point A gets projected on the plane cumputed by A, B, C, the distance to the plane might not be 0 for the mentioned scenario but a small positive or negative value.
	// This can lead to the wrong boolean results. Using a small negative value as threshold is more conservative but safer.
	const float3 ab = _b - _a;
	const float3 ac = _c - _a;
	const float3 ad = _d - _a;
	const float3 bd = _d - _b;
	const float3 bc = _c - _b;

	const float3 v0 = cross(ab, ac);
	const float3 v1 = cross(ac, ad);
	const float3 v2 = cross(ad, ab);
	const float3 v3 = cross(bd, bc);

	const float4 signa = {dot(v0, _a), dot(v1, _a), dot(v2, _a), dot(v3, _a)};
	const float4 signd = {dot(v0, _d), dot(v1, _b), dot(v2, _c), dot(v3, _b)};
	return bit_mask(signa * signd >= -1e-6);//same side, outside of the plane
}

float3 closestPtPointSegment(float3* Q, uint32& size) {
	const float3 a		= Q[0];
	const float3 b		= Q[1];
	const float3 ab		= b - a;
	const float denom	= len2(ab);
	if (epsilon >= denom) {
		size = 1;
		return Q[0];
	}
	return madd(ab, clamp(-dot(a, ab) / denom, zero, one), a);
}

float3 iso::getClosestPoint(const float3* Q, const float3* A, param(float3) closest, const uint32 size) {
	switch (size) {
		default:
		case 1:
			return A[0];
		case 2: {
			float	v = barycentricCoordinates(closest, Q[0], Q[1]);
			return madd(A[1] - A[0], v, A[0]);
		}
		case 3: {
			//calculate the Barycentric of closest point p in the mincowsky sum
			float2	v = barycentricCoordinates(closest, Q[0], Q[1], Q[2]);
			return A[0] + (A[1] - A[0]) * v.x + (A[2] - A[0]) * v.y;
		}
	}
}

float3 closestPtPointTriangleBaryCentric(param(float3) a, param(float3) b, param(float3) c, uint32* indices, uint32& size) {
	size = 3;

	const float3 ab	= b - a;
	const float3 ac	= c - a;
	const float3 n	= cross(ab, ac);

	//if the shape is oblong, the degeneracy test sometime can't catch the degeneracy in the tetraheron
	//so we need to make sure we still can ternimate with the previous triangle by returning the maxinum distance
	const float nn = dot(n, n);
	if (nn == zero)
		return maximum;

	const float va = dot(n, cross(b, c));//edge region of BC, signed area rbc, u = S(rbc)/S(abc) for a
	const float vb = dot(n, cross(c, a));//edge region of AC, signed area rac, v = S(rca)/S(abc) for b
	const float vc = dot(n, cross(a, b));//edge region of AB, signed area rab, w = S(rab)/S(abc) for c

										 //face region
	if (va >= zero && vb >= zero && vc >= zero)
		return n * dot(n, a) / nn;

	const float d1 = -dot(ab, a); //  snom
	const float d2 = -dot(ac, a); //  tnom
	const float d3 = -dot(ab, b); // -sdenom
	const float d4 = -dot(ac, b); //  unom = d4 - d3
	const float d5 = -dot(ab, c); //  udenom = d5 - d6
	const float d6 = -dot(ac, c); // -tdenom

	size = 2;
	//check if p in edge region of AB
	if (zero >= vc && d1 >= zero && zero >= d3) {
		const float denom = d1 - d3;
		return select(abs(denom) > epsilon, madd(ab, d1 / denom, a), a);
	}

	//check if p in edge region of BC
	if (zero >= va && d4 >= d3 && d5 >= d6) {
		const float unom	= d4 - d3;
		const float denom	= unom + d5 - d6;
		indices[0] = indices[1];
		indices[1] = indices[2];
		return select(abs(denom) > epsilon, madd(c - b, unom / denom, b), b);
	}

	//check if p in edge region of AC
	if (zero >= vb && d2 >= zero && zero >= d6) {
		const float denom = d2 - d6;
		indices[1] = indices[2];
		return select(abs(denom) > epsilon, madd(ac, d2 / denom, a), a);
	}

	size = 1;
	//check if p in vertex region outside a
	if (zero >= d1 && zero >= d2) // vertex region a
		return a;

	//check if p in vertex region outside b
	if (d3 >= zero && d3 >= d4) { // vertex region b
		indices[0] = indices[1];
		return b;
	}

	//p is in vertex region outside c
	indices[0] = indices[2];
	return c;
}

float3 closestPtPointTriangle(float3* Q, float3* A, uint32& size) {
	size = 3;

	const float3 a = Q[0];
	const float3 b = Q[1];
	const float3 c = Q[2];
	const float3 ab = b - a;
	const float3 ac = c - a;

	if (epsilon >= len2(cross(ab, ac))) {
		//degenerate
		size = 2;
		return closestPtPointSegment(Q, size);
	}

	uint32 _size;
	uint32 indices[3]={0, 1, 2};
	float3 closestPt = closestPtPointTriangleBaryCentric(a, b, c, indices, _size);

	if(_size != 3) {
		const float3 q0 = Q[indices[0]], q1 = Q[indices[1]];
		const float3 a0 = A[indices[0]], a1 = A[indices[1]];

		Q[0] = q0; Q[1] = q1;
		A[0] = a0; A[1] = a1;

		size = _size;
	}

	return closestPt;
}

float3 closestPtPointTriangle(float3* Q, float3* A, int* aInd, uint32& size) {
	size = 3;

	const float3 a = Q[0];
	const float3 b = Q[1];
	const float3 c = Q[2];
	const float3 ab = b - a;
	const float3 ac = c - a;

	if (epsilon >= len2(cross(ab, ac))) {
		//degenerate
		size = 2;
		return closestPtPointSegment(Q, size);
	}

	uint32 _size;
	uint32 indices[3]={0, 1, 2};
	float3 closestPt = closestPtPointTriangleBaryCentric(a, b, c, indices, _size);

	if (_size != 3) {
		const float3 q0 = Q[indices[0]], q1 = Q[indices[1]];
		const float3 a0 = A[indices[0]], a1 = A[indices[1]];
		const int aInd0 = aInd[indices[0]], aInd1 = aInd[indices[1]];

		Q[0] = q0; Q[1] = q1;
		A[0] = a0; A[1] = a1;
		aInd[0] = aInd0; aInd[1] = aInd1;

		size = _size;
	}

	return closestPt;
}

static float3 getClosestPtPointTriangle(float3* Q, const int bIsOutside4, uint32* indices, uint32& size) {
	float bestSqDist = maximum;

	uint32 _indices[3] = {0, 1, 2};
	float3 closestPt = zero;

	if (bIsOutside4 & 1) {
		//use the original indices, size, v and w
		closestPt = closestPtPointTriangleBaryCentric(Q[0], Q[1], Q[2], indices, size);
		bestSqDist = len2(closestPt);
	}

	if (bIsOutside4 & 2) {
		uint32 _size = 3;
		_indices[0] = 0; _indices[1] = 2; _indices[2] = 3; 
		float3 tClosestPt = closestPtPointTriangleBaryCentric(Q[0], Q[2], Q[3], _indices, _size);
		const float sqDist = len2(tClosestPt);

		if (bestSqDist > sqDist) {
			closestPt = tClosestPt;
			bestSqDist = sqDist;

			indices[0] = _indices[0];
			indices[1] = _indices[1];
			indices[2] = _indices[2];

			size = _size;
		}
	}

	if (bIsOutside4 & 4) {
		uint32 _size = 3;
		_indices[0] = 0; _indices[1] = 3; _indices[2] = 1; 
		float3 tClosestPt= closestPtPointTriangleBaryCentric(Q[0], Q[3], Q[1], _indices, _size);
		const float sqDist = len2(tClosestPt);

		if (bestSqDist > sqDist) {
			closestPt = tClosestPt;
			bestSqDist = sqDist;

			indices[0] = _indices[0];
			indices[1] = _indices[1];
			indices[2] = _indices[2];

			size = _size;
		}

	}

	if (bIsOutside4 & 8) {
		uint32 _size = 3;
		_indices[0] = 1; _indices[1] = 3; _indices[2] = 2; 
		float3 tClosestPt = closestPtPointTriangleBaryCentric(Q[1], Q[3], Q[2], _indices, _size);
		const float sqDist = len2(tClosestPt);
		if (bestSqDist > sqDist) {
			closestPt = tClosestPt;
			bestSqDist = sqDist;

			indices[0] = _indices[0];
			indices[1] = _indices[1];
			indices[2] = _indices[2];

			size = _size;
		}
	}

	return closestPt;
}

float3 closestPtPointTetrahedron(float3* Q, float3* A, uint32& size) {
	const float3 a = Q[0];
	const float3 b = Q[1];
	const float3 c = Q[2];  
	const float3 d = Q[3];

	//degenerated
	const float3 ab = b - a;
	const float3 ac = c - a;
	if (abs(dot(normalise(cross(ab, ac)), d - a)) < 1e-4f) {
		size = 3;
		return closestPtPointTriangle(Q, A, size);
	}

	const int bIsOutside4 = PointOutsideOfPlane4(a, b, c, d);
	if (bIsOutside4 == 0)
		return zero;		//All inside

	uint32 indices[3] = {0, 1, 2};

	const float3 closest = getClosestPtPointTriangle(Q, bIsOutside4, indices, size);

	const float3 q0 = Q[indices[0]], q1 = Q[indices[1]], q2 = Q[indices[2]];
	const float3 a0 = A[indices[0]], a1 = A[indices[1]], a2 = A[indices[2]];
	Q[0] = q0; Q[1] = q1; Q[2] = q2;
	A[0] = a0; A[1] = a1; A[2] = a2;

	return closest;
}

float3 iso::GJKCPairDoSimplex(float3* Q, float3* A, uint32& size) {
	switch(size) {
		default:
			ISO_ASSERT(0);
		case 1:
			return Q[0];
		case 2:
			return closestPtPointSegment(Q, size);
		case 3:
			return closestPtPointTriangle(Q, A, size);
		case 4:
			return closestPtPointTetrahedron(Q, A, size);
	}
}


//-----------------------------------------------------------------------------
//	EPA2probe
//-----------------------------------------------------------------------------

class EPA2probe {
	enum { MAX_FACETS = 64 };

	struct Facet	{
		line2		plane;
		position2	v0, vmid;
		Facet*		adjFacets[2];	//adjacent facets

		Facet(position2 v0, position2 v1) : plane(v0, v1), v0(v0) {
			adjFacets[0] = adjFacets[1]	= NULL;
		}
		void	set_v1(position2 v1) {
			plane = {v0, v1};
		}

		void link_before(Facet *facet) {
			adjFacets[1]		= facet;
			facet->adjFacets[0]	= this;
		} 
	};

	struct Comparator {
		bool operator()(const Facet* a, const Facet* b) const { return a->plane.dist(a->vmid) < b->plane.dist(b->vmid); }
	};

	priority_queue<static_array<Facet*, MAX_FACETS>, Comparator> heap;
	static_array<Facet, MAX_FACETS>	facets;

	Facet*	addFacet(position2 v0, position2 v1, const convex2& a) {
		Facet	*facet = new(facets) Facet(v0, v1);
		facet->vmid	= a(-facet->plane.normal());
		heap.push(facet);
		return facet;
	}

public:
	enum STATUS {
		EPA_CONTACT,		// two shapes intersect
		EPA_DEGENERATE,		// can't converage
		EPA_FAIL			// fail to construct an initial polygon to work with 
	};


	EPA2probe(const convex2& a, const float2* A, const int size);
	void	Probe(const convex2& a, float eps, int max_out);
	int		GetVerts(float2* A, int max_size) {
		int	i = 0;
		for (auto first = heap.top(), facet = first; i < max_size;) {
			A[i] = facet->v0;
			++i;
			if ((facet = facet->adjFacets[1]) == first)
				break;
		}
		return i;
	}
};

EPA2probe::EPA2probe(const convex2& a, const float2* A, const int size) {
	position2	t[3];
	for (int i = 0; i < size; i++)
		t[i] = position2(A[i]);

	//if the simplex isn't a triangle, we need to construct one before we can expand it
	switch (size) {
		case 1: {
			t[1]	= a(x_axis);
			if (len2(t[0] - t[1]) == zero)
				break;
			//fall through
		}
		case 2: {
			float2	n	= perp(t[1] - t[0]);
			t[2]		= a(-n);
			//fall through
		}
		case 3: {
			if (cross(t[1] - t[0], t[2] - t[0]) > zero)
				swap(t[1], t[2]);

			float	upper_bound	= maximum;
			Facet	*f0 = addFacet(t[0], t[1], a);
			Facet	*f1 = addFacet(t[1], t[2], a);
			Facet	*f2 = addFacet(t[2], t[0], a);

			f0->link_before(f1);
			f1->link_before(f2);
			f2->link_before(f0);
			break;
		}
	}
}

void EPA2probe::Probe(const convex2& a, float eps, int max_out) {
	while (!heap.empty() && facets.size() * 2 < max_out) {
		Facet*	facet	= heap.pop_value(); //get the shortest distance triangle of origin from the list
		Facet*	fnext	= facet->adjFacets[1];

		auto	v		= facet->vmid;

		facet->set_v1(v);
		facet->vmid		= a(-facet->plane.normal());
		
		Facet*	f1		= addFacet(v, fnext->v0, a);
		facet->link_before(f1);
		f1->link_before(fnext);
	}
}

int	iso::EPAProbe(const convex2& a, float2* A, int size, float eps, int max_out) {
	EPA2probe	epa(a, A, size);
	epa.Probe(a, eps, max_out);
	return epa.GetVerts(A, max_out);
}

//-----------------------------------------------------------------------------
//	EPA2
//-----------------------------------------------------------------------------

class EPA2 {
	enum { MAX_FACETS = 64 };

	static position2	to_pos(param(float4) ab)	{ return position2(ab.xy - ab.zw); }

	struct Facet	{
		line2	plane;
		float4	ab;				//support vertices from a and b
		Facet*	adjFacets[2];	//adjacent facets

		Facet(float4 ab0, float4 ab1) : plane(to_pos(ab0), to_pos(ab1)), ab(ab0) {
			adjFacets[0] = adjFacets[1]	= NULL;
		}
		void link_before(Facet *facet) {
			adjFacets[1]		= facet;
			facet->adjFacets[0]	= this;
		} 
	};

	struct PenetrationComparator {
		bool operator()(const Facet* a, const Facet* b) const { return a->plane.dist() < b->plane.dist(); }
	};

	priority_queue<static_array<Facet*, MAX_FACETS>, PenetrationComparator> heap;
	fixed_pool<Facet, MAX_FACETS>	facet_pool;

	Facet*	addFacet(float4 ab0, float4 ab1, float upper) {
		if (void *p = facet_pool.alloc()) {
			Facet * facet	= new(p) Facet(ab0, ab1);
			if (facet->plane.dist() <= upper)
				heap.push(facet);
			return facet;
		}
		return nullptr;
	}

public:
	enum STATUS {
		EPA_CONTACT,		// two shapes intersect
		EPA_DEGENERATE,		// can't converage
		EPA_FAIL			// fail to construct an initial polygon to work with 
	};


	EPA2(const convex2& a, const convex2& b, const float2* A, const float2* B, const int size);
	STATUS		PenetrationDepth(const convex2& a, const convex2& b, float eps, position2& pa, position2& pb, float2& normal, float& penetration);

	// for debug
	bool		PenetrationIteration(const convex2& a, const convex2& b);
	int			GetVerts(float2* A, float2* B, int max_size) {
		int	i = 0;
		for (auto first = heap.top(), facet = first; i < max_size;) {
			A[i] = facet->ab.xy;
			B[i] = facet->ab.zw;
			++i;
			if ((facet = facet->adjFacets[1]) == first)
				break;
		}
		return i;
	}
};

EPA2::EPA2(const convex2& a, const convex2& b, const float2* A, const float2* B, const int size) {
	float4	t[3];
	for (int i = 0; i < size; i++)
		t[i] = concat(A[i], B[i]);

	//if the simplex isn't a triangle, we need to construct one before we can expand it
	switch (size) {
		case 1: {
			const float2 x = x_axis;
			t[1]	= concat(a(-x), b(x));
			if (len2(to_pos(t[0]) - to_pos(t[1])) == zero)
				break;
			//fall through
		}
		case 2: {
			float2	n	= perp(to_pos(t[1]) - to_pos(t[0]));
			t[2]		= concat(a(-n), b(n));
			//fall through
		}
		case 3: {
			auto	pt0	= to_pos(t[0]);
			auto	pt1	= to_pos(t[1]);
			auto	pt2	= to_pos(t[2]);

			if (cross(pt1 - pt0, pt2 - pt0) > zero)
				swap(t[1], t[2]);

			float	upper_bound	= maximum;
			Facet	*f0 = addFacet(t[0], t[1], upper_bound);
			Facet	*f1 = addFacet(t[1], t[2], upper_bound);
			Facet	*f2 = addFacet(t[2], t[0], upper_bound);
			if (!heap.empty()) {
				f0->link_before(f1);
				f1->link_before(f2);
				f2->link_before(f0);
			}
			break;
		}
	}
}

bool EPA2::PenetrationIteration(const convex2& a, const convex2& b) {

	if (heap.empty())
		return false;

	Facet*	facet	= heap.pop_value(); //get the shortest distance triangle of origin from the list
	float2		n	= facet->plane.normal();
	float4		t	= concat(a(n), b(-n));
	position2	q	= to_pos(t);

	float	upper_bound		= dot(facet->plane.normal(), q);

	Facet*	fprev	= facet->adjFacets[0];
	Facet*	fnext	= facet->adjFacets[1];
	Facet*	f0		= addFacet(facet->ab, t, upper_bound);
	Facet*	f1		= addFacet(t, fnext->ab, upper_bound);
	if (!f0 || !f1)
		return false;

	fprev->link_before(f0);
	f0->link_before(f1);
	f1->link_before(fnext);

	facet_pool.release(facet);

	return true;
}

EPA2::STATUS EPA2::PenetrationDepth(const convex2& a, const convex2& b, float eps, position2& pa, position2& pb, float2& normal, float& penetration) {
	if (heap.empty())
		return EPA_FAIL;

	STATUS	result			= EPA_DEGENERATE;
	float	upper_bound		= maximum;

	Facet* facet;
	do {
		facet			= heap.pop_value(); //get the shortest distance triangle of origin from the list
		float2		n	= facet->plane.normal();
		float4		t	= concat(a(n), b(-n));
		position2	q	= to_pos(t);

		// calculate the distance from support point to the plane
		const float dist = facet->plane.dist(q);
		if (abs(dist) <= eps) {
			result	= EPA_CONTACT;
			break;
		}

		//update the upper bound with the distance to the origin
		upper_bound		= min(upper_bound, dist + facet->plane.dist());

		Facet*	fprev	= facet->adjFacets[0];
		Facet*	fnext	= facet->adjFacets[1];
		Facet*	f0		= addFacet(facet->ab, t, upper_bound);
		Facet*	f1		= addFacet(t, fnext->ab, upper_bound);
		if (!f0 || !f1)
			break;

		fprev->link_before(f0);
		f0->link_before(f1);
		f1->link_before(fnext);

		facet_pool.release(facet);

	} while(!heap.empty() > 0 && upper_bound > heap.top()->plane.dist());

	auto	ab0		= facet->ab;
	auto	ab1		= facet->adjFacets[1]->ab;
	//calculate the closest points for a shape pair

	float2		lambdas		= S1D(to_pos(ab0).v, to_pos(ab1).v);
	float4		closest		= ab0 * lambdas.x + ab1 * lambdas.y;

	pa			= position2(closest.xy);
	pb			= position2(closest.zw);
	normal		= -facet->plane.normal();
	penetration	= -facet->plane.dist();

	return result;
}

int	iso::EPAPenetration(const convex2& a, const convex2& b, const float2* A, const float2* B, const int size, float eps, position2& pa, position2& pb, float2& normal, float& penDepth) {
	EPA2	epa(a, b, A, B, size);
	return epa.PenetrationDepth(a, b, eps, pa, pb, normal, penDepth);
}

int	iso::EPAPenetrationDebug(const convex2& a, const convex2& b, float2* A, float2* B, const int size, int max_size, int steps) {
	EPA2	epa(a, b, A, B, size);
	while (steps-- && epa.PenetrationIteration(a, b));
	return epa.GetVerts(A, B, max_size);
}

//-----------------------------------------------------------------------------
//	EPA3
//-----------------------------------------------------------------------------

class EPA3 {
	enum {
		MaxEdges			= 32,
		MaxFacets			= 64,
		MaxSupportPoints	= 64,
	};

	struct Facet;

	struct Edge {
		Facet* facet;
		uint32 index;
		Edge() {}
		Edge(Facet *facet, const uint32 index) : facet(facet), index(index) {}
	};

	typedef static_array<Edge, MaxEdges>			EdgeBuffer;
	typedef fixed_pool_deferred<Facet, MaxFacets>	FacetPool;

	struct Facet	{
		plane	plane;
		Facet*	adjFacets[3];	//the triangle adjacent to edge i in this triangle
		uint8	adjEdges[3];	//the edge connected with the corresponding triangle
		uint8	verts[3];		//the index of vertices of the triangle
		bool	obsolete;		//a flag to denote whether the triangle is still part of the boundary of the new polytope
		bool	in_heap;		//a flag to indicate whether the triangle is in the heap

		Facet(const uint32 i0, const uint32 i1, const uint32 i2, position3 p0, position3 p1, position3 p2) : plane(p0, p1, p2), obsolete(false), in_heap(false) {
			adjFacets[0]	= adjFacets[1]	= adjFacets[2]	= NULL;
			verts[0]		= uint8(i0);
			verts[1]		= uint8(i1);
			verts[2]		= uint8(i2);
		}

		//create ajacency information
		void link(const uint8 edge0, Facet *facet, const uint8 edge1) {
			adjFacets[edge0]		= facet;
			adjEdges[edge0]			= edge1;
			facet->adjFacets[edge1]	= this;
			facet->adjEdges[edge1]	= edge0;
		} 

		//store all the boundary facets for the new polytope in the edgeBuffer and free indices when an old facet isn't part of the boundary anymore
		void silhouette(param(position3) w, EdgeBuffer& edgeBuffer, FacetPool& pool) {
			Edge stack[MaxFacets];
			for (uint32 a = 0; a < 3; ++a)
				stack[a] = Edge(this, adjEdges[2 - a]);

			for (int size = 3; size--;) {
				Facet* const f		= stack[size].facet;
				const uint32 index	= stack[size].index;

				if (!f->obsolete) {
					if (f->plane.dist(w) < zero) {
						// facet isn't visible from w (we don't have a reflex edge), this facet will be on the boundary and part of the new polytope so push it into edgeBuffer
						edgeBuffer.emplace_back(f, index);

					} else {
						// facet is visible from w, therefore, we need to remove this facet from the heap and push its adjacent facets onto the stack
						f->obsolete		= true;
						uint32 index1	= index  == 2 ? 0 : index  + 1;
						uint32 index2	= index1 == 2 ? 0 : index1 + 1;
						stack[size++]	= Edge(f->adjFacets[index2], f->adjEdges[index2]);
						stack[size++]	= Edge(f->adjFacets[index1], f->adjEdges[index1]);

						ISO_ASSERT(size <= MaxFacets);
						if (!f->in_heap)
							//if the facet isn't in the heap, we can release that memory
							pool.deferred_release(f);
					}
				}
			}
		}
	};

	struct FacetDistanceComparator {
		bool operator()(const Facet* left, const Facet* right) const { return left->plane.dist() < right->plane.dist(); }
	};

	priority_queue<static_array<Facet*, MaxFacets>, FacetDistanceComparator> heap;
	float3			aBuf[MaxSupportPoints];
	float3			bBuf[MaxSupportPoints];
	FacetPool		facet_pool;
	uint32			numVerts;
	
	position3	pt(int i)	const { return position3(aBuf[i] - bBuf[i]); }

	Facet*	addFacet(const uint32 i0, const uint32 i1, const uint32 i2, float upper) {
		ISO_ASSERT(i0 != i1 && i0 != i2 && i1 != i2);

		if (void *p = facet_pool.alloc()) {
			Facet * facet	= new(p) Facet(i0, i1, i2, pt(i0), pt(i1), pt(i2));
			if (facet->plane.dist() <= upper) {
				heap.push(facet);
				facet->in_heap = true;
			}
			return facet;
		}
		return nullptr;
	}
	Facet*	addFacet(const Edge *edge, const uint32 i2, float upper) {
		Facet	*f	= edge->facet;
		uint32	i	= edge->index;
		return addFacet(f->verts[i == 2 ? 0 : i + 1], f->verts[i], i2, upper);
	}

public:
	enum STATUS {
		EPA_CONTACT,		// two shapes intersect
		EPA_DEGENERATE,		// can't converage
		EPA_FAIL			// fail to construct an initial polygon to work with 
	};

	EPA3(const convex3& a, const convex3& b, const float3* A, const float3* B, const int size);
	STATUS		PenetrationDepth(const convex3& a, const convex3& b, float eps, position3& pa, position3& pb, float3& normal, float& penDepth);
};

EPA3::EPA3(const convex3& a, const convex3& b, const float3* A, const float3* B, const int size) : numVerts(size) {
	float upper_bound	= maximum;

	for (int i = 0; i < size; i++) {
		aBuf[i] = A[i];
		bBuf[i] = B[i];
	}

	//if the simplex isn't a tetrahedron, we need to construct one before we can expand it
	switch (size) {
		case 1: {
			const float3 x = x_axis;
			aBuf[1]		= a(-x);
			bBuf[1]		= b(x);
			numVerts	= 2;
			if (len2(pt(1) - pt(0)) == zero)
				break;
			//fall through
		}
		case 2: {
			float3	n	= perp(pt(1) - pt(0));
			aBuf[2]		= a(-n);
			bBuf[2]		= b(n);
			numVerts	= 3;
			//fall through
		}
		case 3: {
			// We have a triangle inside the Minkowski sum containing the origin. We need to construct two back to back triangles which link to each other
			Facet	*f0 = addFacet(0, 1, 2, upper_bound);
			Facet	*f1 = addFacet(1, 0, 2, upper_bound);
			if (!heap.empty()) {
				f0->link(0, f1, 0);
				f0->link(1, f1, 2);
				f0->link(2, f1, 1);
			}
			break;
		}
		case 4: {
			// All face normals should be all pointing either inwards or outwards; if all inward shuffle the input vertexes
			const auto p0 = pt(0);
			if (dot(cross(pt(1) - p0, pt(2) - p0), pt(3) - p0) > zero) {
				//shuffle the input vertexes
				swap(aBuf[1], aBuf[2]);
				swap(bBuf[1], bBuf[2]);
			}

			Facet *f0 = addFacet(0, 1, 2, upper_bound);
			Facet *f1 = addFacet(0, 3, 1, upper_bound);
			Facet *f2 = addFacet(0, 2, 3, upper_bound);
			Facet *f3 = addFacet(1, 3, 2, upper_bound);

			if (!heap.empty()) {
				f0->link(0, f1, 2);
				f0->link(1, f3, 2);
				f0->link(2, f2, 0);
				f1->link(0, f2, 2);
				f1->link(1, f3, 0);
				f2->link(1, f3, 1);
			}
			break;
		}
	}
}

EPA3::STATUS EPA3::PenetrationDepth(const convex3& a, const convex3& b, float eps, position3& pa, position3& pb, float3& normal, float& penDepth) {
	if (heap.empty())
		return EPA_FAIL;

	STATUS	result			= EPA_DEGENERATE;
	float	upper_bound		= maximum;

	Facet* facet = NULL;
	do {
		facet_pool.process_deferred();
		facet			= heap.pop_value(); //get the shortest distance triangle of origin from the list
		facet->in_heap	= false;

		if (!facet->obsolete) {
			float3		n		= facet->plane.normal();
			float3		tempa	= a( n);
			float3		tempb	= b(-n);
			position3	q		= position3(tempa - tempb);

			// calculate the distance from support point to the origin along the plane normal
			// because the support point is searched along the plane normal the distance should be positive, but if the origin isn't contained in the polytope, it might be negative
			const float dist = facet->plane.dist(q);
			if (abs(dist) < eps) {
				result	= EPA_CONTACT;
				break;
			}

			//update the upper bound to the minimum between existing upper bound and the distance to origin
			upper_bound = min(upper_bound, dist + facet->plane.dist());

			const uint32 index = numVerts++;
			aBuf[index] = tempa;
			bBuf[index] = tempb;

			// Compute the silhouette cast by the new vertex
			// Note that the new vertex is on the positive side of the current facet, so the current facet will not be in the polytope. Start local search from this facet
			EdgeBuffer		edgeBuffer;
			facet->silhouette(q, edgeBuffer, facet_pool);
			if (edgeBuffer.empty())
				break;

			Edge*	edge		= edgeBuffer.begin();
			Facet*	firstFacet	= addFacet(edge, index, upper_bound);
			if (!firstFacet)
				break;
			firstFacet->link(0, edge->facet, edge->index);

			Facet *lastFacet = firstFacet;
			while (edge != edgeBuffer.end()) {
				auto	prevFacet = lastFacet;
				lastFacet = addFacet(edge, index, upper_bound);
				if (!lastFacet)
					break;
				lastFacet->link(0, edge->facet, edge->index);
				lastFacet->link(2, prevFacet, 1);
			}

			if (!lastFacet)
				break;

			firstFacet->link(2, lastFacet, 1);
		}
		facet_pool.release(facet);

	} while(!heap.empty() > 0 && upper_bound > heap.top()->plane.dist() && numVerts != MaxSupportPoints);

	//calculate the closest points for a shape pair
	auto		indices		= facet->verts;
	float3		lambdas		= S2D(pt(indices[0]), pt(indices[1]), pt(indices[2]));
	float3		closestB	= bBuf[indices[0]] * lambdas.x + bBuf[indices[1]] * lambdas.y + bBuf[indices[2]] * lambdas.z;
	position3	closestA	= facet->plane.pt0() + closestB;

	pa			= closestA;
	pb			= position3(closestB);
	normal		= -facet->plane.normal();
	penDepth	= -facet->plane.dist();

	return result;
}

int	iso::EPAPenetration(const convex3& a, const convex3& b, const float3* A, const float3* B, const int size, float eps, position3& pa, position3& pb, float3& normal, float& penDepth) {
	EPA3	epa(a, b, A, B, size);
	return epa.PenetrationDepth(a, b, eps, pa, pb, normal, penDepth);
}

//-----------------------------------------------------------------------------
//	DSS
// Decision Sphere Search
//-----------------------------------------------------------------------------

//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
//
//  TODO : reput ThreeUniquePoints (lasts) but use it to compute distance to
//  the triangle formed by the last 3 points, in order to have an upper bound

#define DSS_MAX_ITER 100

struct SphericalPolygon {
	static float3 normalise_max(float3 v)	{ return v / reduce_max(v); }

	struct Element {
		float3	v;
		float3	north;
		float3	sil;
		Element()	{}
		Element(param(float3) sil) : north(normalise(sil)), sil(sil) {}
		Element(param(float3) v, param(float3) north, param(float3) sil) : v(v), north(north), sil(sil) {}
	};

	dynamic_array<Element>	elements;

	SphericalPolygon()	{}
	SphericalPolygon(initializer_list<Element> i) : elements(i) {}

	bool	empty()	const			{ return elements.empty(); }
	void	add_vertex(float3 v)	{ elements.emplace_back(v); }


	// PRECONDITION : all norths are normalised.
	float3 averageDirection() const {
		float3 avg = zero;
		for (auto &i : elements)
			avg = avg + i.v;
		return normalise(avg);
	}

	void set_to_triangle(const float3 pts[3]) {
		elements.clear();
		elements.emplace_back(normalise_max(cross(pts[0], pts[1])), normalise_max(pts[1]), pts[1]);
		elements.emplace_back(normalise_max(cross(pts[1], pts[2])), normalise_max(pts[2]), pts[2]);
		elements.emplace_back(normalise_max(cross(pts[2], pts[0])), normalise_max(pts[0]), pts[0]);
	}

	SphericalPolygon clip(param(float3) OrigVertex, param(float3) sil, bool doClean = true) const {
		static const float eps = 1e-6f;
		const float3	north	= normalise(sil);
		const int		n		= elements.size();

		switch (n) {
			case 0:
				return {};

			case 1: {
				auto	e0		= elements[0];
				float	d		= dot(e0.north, north);

				// about one degree intersection of two almost opposite hemispheres ==> empty
				if (d < -0.99984769515f)
					return {};

				if (d < 0.99984769515f) {
					float3 v	= normalise_max(cross(north, e0.north));
					e0.v = v;
					return {e0, {-v, north, OrigVertex}};
				}

				return {e0};
			}
			case 2: {
				auto	e0	= elements[0];
				auto	e1	= elements[1];
				float	d	= dot(e0.v, north);

				if (d >= eps) {
					// we'll get a triangle
					e1.v	= normalise_max(cross(north, e1.north));
					return {e0, {normalise_max(cross(e0.north, north)), north, OrigVertex}, e1};
				}
				
				if (d <= -eps) {
					// we'll get a triangle
					e0.v	= normalise_max(cross(north, e0.north));
					return {e0, e1, {normalise_max(cross(e1.north, north)), north, OrigVertex}};
				}

				// we keep a moon crescent

				if (dot(north, cross(e0.north, e0.v)) > 0) {
					if (dot(north, cross(e1.north, e1.v)) <= 0) {
						e1.north	= north;
						e1.sil		= OrigVertex;
						e0.v		= normalise_max(cross(e1.north, e0.north));
						e1.v		= -e0.v;
						return {e0, e1};
					}
				} else {
					if (dot(north, cross(e1.north, e1.v)) > 0) {
						e0.north	= north;
						e0.sil		= OrigVertex;
						e1.v		= normalise_max(cross(e0.north, e1.north));
						e0.v		= -e1.v;
						return {e0, e1};
					} else {
						return {};
					}
				}
				return {e0, e1};	//?
			}
			default: {	// n >= 3
				SphericalPolygon	result;
				int		kept	= 0;
				auto	i0		= elements.end() - 1;
				float	d0		= dot(north, i0->v);

				for (auto &i : elements) {
					float	d1 = dot(north, i.v);

					if (d0 >= eps) {		// cur is "IN"
						++kept;
						result.elements.push_back(*i0);
						if (d1 <= -eps)		// next is "OUT"
							result.elements.emplace_back(normalise_max(cross(i0->north, north)), north, OrigVertex);

					} else if (d0 > -eps) {	// cur is "ON" the clipping plane
						++kept;
						if (d1 <= -eps)		// next is "OUT"
							result.elements.emplace_back(i0->v, north, OrigVertex);
						else
							result.elements.push_back(*i0);

					} else {				// cur is "OUT"
						if (d1 >= eps)		// next is "IN"
							result.elements.emplace_back(normalise_max(cross(north, i0->north)), i0->north, i0->sil);
					}

					d0	= d1;
					i0	= &i;
				}
				if (result.elements.size() < 3 /*too small*/ || (kept == n /*no change*/ && doClean))
					return {};

				return result;
			}
		}
	}
	SphericalPolygon clip(param(float3) sil, bool doClean = true) const {
		return clip(sil, sil, doClean);
	}
};


template<typename A, typename B> SphericalPolygon sphericalDisjointWithBounds(const A &a, const B &b, simplex<3> &simp, float &lowerBound, float &upperBound) {
	float3	n	= x_axis;
	float3	P	= b(-n) - a(-n);

	simp.add_vertex(P);
	upperBound	= len(simp.get_vertex());

	SphericalPolygon	positiveBound = {P};

	for (int i = 0; i < DSS_MAX_ITER && !positiveBound.empty(); ++i) {
		float3	n	= positiveBound.averageDirection();
		float3	P	= b(-n) - a(n);
		float	d	= dot(n, P);

		if (d < 0) {
			simp.add_vertex(P);
			upperBound		= min(upperBound, len(simp.get_vertex()));
			lowerBound		= -d; // > 0
			return positiveBound.clip(P, P + d * n);
		}

		simp.add_vertex(P);
		upperBound = min(upperBound, len(simp.get_vertex()));

		positiveBound = positiveBound.clip(P);
	}
	return {};
}

template<typename A, typename B> float sphericalDistance(const A &a, const B &b) {
	const float	k	= 2;
	const float	kk	= 2 * k - 1;

	float		lo = 0;
	float		hi	= maximum;
	simplex<3>	simp;

	SphericalPolygon	cur_S = sphericalDisjointWithBounds(a, b, simp, lo, hi);

	if (cur_S.empty())
		return lo;

	if (hi - lo < 1e-4f)
		return (lo + hi) / 2;

	auto	lo_S		= cur_S;
	float3	lo_n		= cur_S.averageDirection();
	float3	lo_p		= b(-lo_n) - a(lo_n);
	float	lo_diff		= dot(lo_p, lo_n);

	simp.add_vertex(lo_p);
	hi = min(hi, len(simp.get_vertex()));

	float3	cur_n		= lo_n;
	float3	cur_p		= lo_p;
	float	cur_diff	= lo_diff;

	float	cur			= kk * lo < hi ? k * lo : (lo + hi) / 2;
	bool	cur_is_lo	= true;

	for (int i = 0; hi - lo > 1e-4f && i < DSS_MAX_ITER; ++i) {
		if (cur_diff <= cur) {
			SphericalPolygon	tempPoly = cur_S.clip(cur_p, cur_p - cur * cur_n);

			if (tempPoly.empty()) { // cur is too large, restart on lo_S
				hi	= cur;
				cur = (kk * lo < hi) ? k * lo : (lo + hi) / 2;
				if (!cur_is_lo) {
					cur_is_lo	= true;
					cur_S		= lo_S;
					cur_diff	= lo_diff;
					cur_n		= lo_n;
					cur_p		= lo_p;
				}

			} else { // continue clipping
				cur_is_lo	= false;
				cur_S		= move(tempPoly);
				cur_n		= cur_S.averageDirection();
				cur_p		= b(-cur_n) - a(cur_n);
				cur_diff	= dot(cur_p, cur_n);
				
				simp.add_vertex(lo_p);
				hi = min(hi, len(simp.get_vertex()));

				if( hi <= cur ) {
					cur_is_lo	= true;
					cur			= kk * lo < hi ? k * lo : (lo + hi) / 2;
					cur_S		= lo_S;
					cur_diff	= lo_diff;
					cur_n		= lo_n;
					cur_p		= lo_p;
				}
			}

		} else {
			lo_S	= cur_S.clip(cur_p, cur_p - cur_diff * cur_n);
			if (lo_S.empty())
				return cur_diff;

			cur_is_lo	= true;
			cur_S		= lo_S;

			lo			= cur_diff;
			cur_n		= lo_n		= cur_S.averageDirection();
			cur_p		= lo_p		= b(-lo_n) - a(lo_n);
			cur_diff	= lo_diff	= dot(lo_p, lo_n);
			
			simp.add_vertex(lo_p);
			hi	= min(hi, len(simp.get_vertex()));
			cur	= (kk * lo < hi) ? k * lo : (lo + hi) / 2;
		}
	}
	
	return (lo + hi) / 2;
}

struct testDSS {
	testDSS() {
		sphericalDistance(support3(sphere({0,0,0},1)), support3(sphere({1,0,0},2)));
	}
} test_dss;
