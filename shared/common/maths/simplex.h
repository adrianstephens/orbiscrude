#ifndef SIMPLEX_H
#define SIMPLEX_H

#include "base/vector.h"
#include "base/functions.h"

namespace iso {

typedef callback<position2(param(float2))>	convex2;
typedef callback<position3(param(float3))>	convex3;

template<typename T> auto support2(T &&t) {
	return [&t](param(float2) v) { return t.support(v); };
}
template<typename T> auto support3(T &&t) {
	return [&t](param(float3) v) { return t.support(v); };
}

//-----------------------------------------------------------------------------
// minkowski_sum / minkowski_diff
//-----------------------------------------------------------------------------

template<typename A, typename B> class minkowski_sum {
public:
	A	a;
	B	b;
	template<typename A1, typename B1> minkowski_sum(A1 &&a, B1 &&b) : a(forward<A1>(a)), b(forward<B1>(b))	{}
	position2	support(param(float2) v)	const { return a.support(v) + b.support(v).v; }
};

template<typename A, typename B> minkowski_sum<A, B> make_minkowski_sum(A &&a, B &&b) {
	return {forward<A>(a), forward<B>(b)};
}

template<typename A, typename B> class minkowski_diff {
public:
	A	a;
	B	b;
	template<typename A1, typename B1> minkowski_diff(A1 &&a, B1 &&b) : a(forward<A1>(a)), b(forward<B1>(b))	{}
	position2	support(param(float2) v)	const { return position2(a(v) - b(v)); }
};
/*
template<> class minkowski_diff<convex2, convex2> {
public:
	convex2	a;
	convex2	b;
	template<typename A1, typename B1> minkowski_diff(A1 &&a, B1 &&b) : a(forward<A1>(a)), b(forward<B1>(b))	{}
	position2	support(param(float2) v)	const { return position2(a(v) - b(v)); }
};
*/
template<typename A, typename B> minkowski_diff<A, B> make_minkowski_diff(A &&a, B &&b) {
	return {forward<A>(a), forward<B>(b)};
}

//-----------------------------------------------------------------------------
// simplex functions
//-----------------------------------------------------------------------------

 // Finds point of minimum norm of 1-simplex
inline float2 S1D(float2 a, float2 b) {
	auto	d	= b - a;
	return barycentric(clamp(dot(b, d) / dot(d, d), zero, one));
}

inline float2 S1D(float3 a, float3 b) {
	auto	d	= b - a;
	return barycentric(clamp(dot(b, d) / dot(d, d), zero, one));
}

// Finds point of minimum norm of 2-simplex
inline float3 S2D(float2 a, float2 b, float2 c) {
	float3	B = float3{
		cross(b, c),
		cross(c, a),
		cross(a, b)
	};

	float  det	= reduce_add(B);
	switch (bit_mask(B * det < zero)) {
		// The origin is inside the triangle
		case 0:	return B / det;

			// only one bit set
		case 4:	return concat(S1D(a, b), zero);			// segment AB (shouldn't ever happen with GJK)
		case 2:	return concat(S1D(a, c), zero).xzy;		// segment AC
		case 1:	return concat(S1D(b, c), zero).zxy;		// segment BC

		// more than one bit set
		default: {
			float2	lambdas1	= S1D(a, c);
			float2	lambdas2	= S1D(b, c);
			return len2(a * lambdas1.x + c * lambdas1.y) < len2(b * lambdas2.x + c * lambdas2.y)
				? concat(lambdas1, zero).xzy
				: concat(lambdas2, zero).zxy;
		}
	}
}

// Finds point of minimum norm of 2-simplex
inline float3 S2D(float3 a, float3 b, float3 c) {
	float3	n	= cross(b - a, c - a);
	float3	p	= n * (dot(n, a) / dot(n, n));	//projected origin

	// Find best axis for projection
	int		i	= max_component_index(abs(n)) + 1;
	float2	a2 = rotate(a - p, i).xy;
	float2	b2 = rotate(b - p, i).xy;
	float2	c2 = rotate(c - p, i).xy;

	float3	B = float3{
		cross(b2, c2),
		cross(c2, a2),
		cross(a2, b2)
	};
	float  det	= reduce_add(B);
	switch (bit_mask(B * det < zero)) {
		// The origin is inside the triangle
		case 0:	return B / det;

		// only one bit set
		case 4:	return concat(S1D(a, b), zero);			// segment AB (shouldn't ever happen with GJK)
		case 2:	return concat(S1D(a, c), zero).xzy;		// segment AC
		case 1:	return concat(S1D(b, c), zero).zxy;		// segment BC

		// more than one bit set
		default: {
			float2	lambdas1	= S1D(a, c);
			float2	lambdas2	= S1D(b, c);
			return len2(a * lambdas1.x + c * lambdas1.y) < len2(b * lambdas2.x + c * lambdas2.y)
				? concat(lambdas1, zero).xzy
				: concat(lambdas2, zero).zxy;
		}
	}
}

// Finds point of minimum norm of 3-simplex
inline float4 S3D(float3 a, float3 b, float3 c, float3 d) {
	static const float  eps		= 1e-13f;
	enum : uint8 { BCD = 1, ACD = 2, ABD = 4, ABC = 8, ALL = 14 };
	static const uint8 tests[16] = {
		ALL,	// 0	
		ALL,	// 1
		ALL,	// 2
		ABC,	// 3	C = D
		ALL,	// 4
		ABD,	// 5	B = D
		ABC,	// 6	A = D
		ABC,	// 7	A = D
		ALL,	// 8
		ACD,	// 9	B = C
		ABD,	// 10	A = C
		ABD,	// 11	A = C
		ACD,	// 12	A = B
		ACD,	// 13	A = B
		ACD,	// 14	A = B
		ACD,	// 15	A = B
	};

	float4 B	= float4{
		dot(b, cross(d, c)),
		dot(c, cross(d, a)),
		dot(d, cross(b, a)),
		dot(a, cross(b, c)),
	};

	// Test if sign of ABCD is equal to the signs of the auxiliary simplices
	float  det		= reduce_add(B);
	int	   test		= abs(det) >= eps
		?	bit_mask(det * B < zero)
		:	tests[bit_mask(abs(B) < eps)];

	switch (test) {
		// origin is inside the simplex
		case 0:	return B / det;

		// only one bit set
		case ABC:	return concat(S2D(a, b, c), zero);		// triangle ABC
		case ABD:	return concat(S2D(a, b, d), zero).xywz;	// triangle ABD
		case ACD:	return concat(S2D(a, c, d), zero).xwyz;	// triangle ACD
		case BCD:	return concat(S2D(b, c, d), zero).wxyz;	// triangle BCD

		// more than one bit set
		default: {
			float4	lambda;
			float	dist	= maximum;

			if (test & ABC) {
				// triangle ABC
				float3	lambda2 = S2D(a, b, c);
				dist	= len2(a * lambda2.x + b * lambda2.y + c * lambda2.z);
				lambda	= concat(lambda2, zero);
			}
			if (test & ABD) {
				// triangle ABD
				float3	lambda2 = S2D(a, b, d);
				float	dist2	= len2(a * lambda2.x + b * lambda2.y + d * lambda2.z);
				if (dist2 < dist) {
					dist	= dist2;
					lambda	= concat(lambda2, zero).xywz;
				}
			}
			if (test & ACD) {
				// triangle ACD
				float3	lambda2 = S2D(a, c, d);
				float	dist2	= len2(a * lambda2.x + c * lambda2.y + d * lambda2.z);
				if (dist2 < dist) {
					dist	= dist2;
					lambda	= concat(lambda2, zero).xwyz;
				}
			}
			if (test & BCD) {
				// triangle BCD
				float3	lambda2 = S2D(b, c, d);
				float	dist2	= len2(b * lambda2.x + c * lambda2.y + d * lambda2.z);
				if (dist2 < dist) {
					dist	= dist2;
					lambda	= concat(lambda2, zero).wxyz;
				}
			}
			return lambda;
		}
	}
}

//-----------------------------------------------------------------------------
// simplex
//-----------------------------------------------------------------------------

template<int N> struct simplex_base {
	typedef	vec<float, N>		V;
	typedef	vec<float, N + 1>	V1;

	int		n	= 0;	// Number of simplex's vertices
	V1		v[N + 1];	// vertices, with lambda in w

	constexpr bool full() const { return n == N + 1; }

	V	get_vertex() const {
		V r = zero;
		for (int i = 0; i < n; ++i)
			r += shrink<N>(v[i]) * v[i][N];
		return r;
	}
	V	get_vertex(const V *p) const {
		V r = zero;
		for (int i = 0; i < n; ++i)
			r += p[i] * v[i][N];
		return r;
	}
	void	offset(V off) {
		V1	off1 = concat(off, zero);
		for (int i = 0; i < n; ++i)
			v[i] += off1;
	}
};


template<int N> struct simplex;

template<> struct simplex<2> : simplex_base<2> {
	int	add_vertex(float2 x) {
		switch (n) {
			case 2: {
				auto	lambdas = S2D(v[0].xy, v[1].xy, x);
				int		set = bit_mask(lambdas != zero);
				float3*	d	= v;
				if (set & 1)
					*d++ = concat(v[0].xy, lambdas.x);
				if (set & 2)
					*d++ = concat(v[1].xy, lambdas.y);
				if (set & 4)
					*d++ = concat(x, lambdas.z);
				n	= d - v;
				return set;
			}
			case 1: {
				auto	lambdas = S1D(v[0].xy, x);
				int		set = bit_mask(lambdas != zero);
				float3*	d	= v;
				if (set & 1)
					*d++ = concat(v[0].xy, lambdas.x);
				if (set & 2)
					*d++ = concat(x, lambdas.y);
				n	= d - v;
				return set;
			}
			default:
				v[0] = concat(x, one);
				n	= 1;
				return 1;
		}
	}
};

template<> struct simplex<3> : simplex_base<3> {
	int	add_vertex(float3 x) {
		switch (n) {
			case 3: {
				auto	lambdas = S3D(v[0].xyz, v[1].xyz, v[2].xyz, x);
				int		set = bit_mask(lambdas != zero);
				float4*	d	= v;
				if (set & 1)
					*d++ = concat(v[0].xyz, lambdas.x);
				if (set & 2)
					*d++ = concat(v[1].xyz, lambdas.y);
				if (set & 4)
					*d++ = concat(v[2].xyz, lambdas.z);
				if (set & 8)
					*d++ = concat(x, lambdas.w);
				n	= d - v;
				return set;
			}
			case 2: {
				auto	lambdas = S2D(v[0].xyz, v[1].xyz, x);
				int		set = bit_mask(lambdas != zero);
				float4*	d	= v;
				if (set & 1)
					*d++ = concat(v[0].xyz, lambdas.x);
				if (set & 2)
					*d++ = concat(v[1].xyz, lambdas.y);
				if (set & 4)
					*d++ = concat(x, lambdas.z);
				n	= d - v;
				return set;
			}
			case 1: {
				auto	lambdas = S1D(v[0].xyz, x);
				int		set = bit_mask(lambdas != zero);
				float4*	d	= v;
				if (set & 1)
					*d++ = concat(v[0].xyz, lambdas.x);
				if (set & 2)
					*d++ = concat(x, lambdas.y);
				n	= d - v;
				return set;
			}
			default:
				v[0] = concat(x, one);
				n	= 1;
				return 1;
		}
	}
};

template<int N> struct simplex_difference : simplex<N> {
	typedef simplex<N>	S;
	using typename S::V;
	V	va[N + 1];

	pos<float, N>	get_vertexA()		const { return pos<float, N>(S::get_vertex(va)); }
	pos<float, N>	get_vertexA(int i)	const { return pos<float, N>(va[i]); }
	pos<float, N>	get_vertexB(int i)	const { return pos<float, N>(va[i] - shrink<N>(S::v[i])); }

	void	add_vertex(V diff, pos<float, N> a) {
		va[S::n]	= a;
		int	set		= S::add_vertex(diff);
		for (V *s = va, *d = va; set; ++s, set >>= 1) {
			if (set & 1)
				*d++ = *s;
		}
	}

	template<typename A, typename B> bool gjk(const A& a, const B& b, param_t<vec<float,N>> _initial_dir, float margins, float separated, float eps, pos<float,N>& _closestA, pos<float,N>& _closestB, float& _dist);
	template<typename A, typename B> bool gjk_sweep(const A& a, const B& b, param_t<vec<float,N>> _initial_dir, float inflation, float eps, param_t<vec<float,N>> s, param_t<vec<float,N>> r, float& _t, vec<float,N>& normal, pos<float,N>& _closestA);
};

float3 getClosestPoint(const float3* Q, const float3* A, param(float3) closest, const uint32 size);
float3 GJKCPairDoSimplex(float3* Q, float3* A, uint32& size);

#if 0

template<> struct simplex_difference<3> : simplex<3> {
	typedef simplex<3>	S;
	float3	va[4];

	float3	Q[4], V[4];
	uint32	size;

	auto	get_vertex() const { return Q[0]; }
	simplex_difference() : size(0) {}

	void	add_vertex(float3 diff, position3 a) {
		va[S::n]	= a;
		int	set		= S::add_vertex(diff);
		for (float3 *s = va, *d = va; set; ++s, set >>= 1) {
			if (set & 1)
				*d++ = *s;
		}
	}

	template<typename A, typename B> bool gjk(const A& a, const B& b, param_t<float3> _initial_dir, float margins, float separated, float eps, position3& _closestA, position3& _closestB, float& _dist);
//	template<typename A, typename B> bool gjk_sweep(const A& a, const B& b, param_t<vec<float,N>> _initial_dir, float inflation, float eps, param_t<vec<float,N>> s, param_t<vec<float,N>> r, float& _t, vec<float,N>& normal, pos<float,N>& _closestA);
};

template<typename A, typename B> bool simplex_difference<3>::gjk(const A& a, const B& b, param_t<float3> _initial_dir, float margins, float separated, float eps, position3& _closestA, position3& _closestB, float& _dist) {
	const float relDif	= one - 0.000225f;	//square value of 1.5% which applied to the distance of a closest point(v) to the origin
	float3	closest	= select(len2(_initial_dir) > zero, _initial_dir, float3{1,0,0});
	float	dist	= maximum;

	do {
		float	prevDist	= dist;
		float3	prevClos	= closest;

		//de-virtualize, we don't need to use a normalize direction to get the support point this will allow the cpu better pipeline the normalize calculation while it does the support map
		const auto supportA	= a(-closest);
		const auto supportB	= b(closest);	// supportB = supportA - support
		const float3 support	= supportA - supportB;
		const float signDist	= dot(closest, support) / dist;

		if (signDist > separated) {
			//ML:gjk found a separating axis for these two objects and the distance is larger than the seperating distance, gjk might not converage so that we won't generate contact information
			return false;
		}

		if (signDist > margins && signDist > relDif * dist) {
			//ML:: gjk converage and we get the closest point information
			_closestA	= position3(getClosestPoint(Q, V, closest, size));
			_closestB	= _closestA - closest;
			_dist	= max(dist - margins, 0);
			//normal		= -closest / dist;
			return true;
		}

		ISO_ASSERT(size < 4);
		V[size]		= supportA;
		Q[size++]	= support;  

		//calculate the closest point between two convex hull
		closest		= GJKCPairDoSimplex(Q, V, size);
		dist		= len(closest);

		if (full()) {
			add_vertex(support, supportA);
			auto	closest1	= S::get_vertex();
			auto	dist1		= len(closest1);
			ISO_ASSERT(dist1 < dist || approx_equal(closest, closest1));
		}

		if (dist >= prevDist) {
			//GJK degenerated, use the previous closest point
			_closestA	= position3(getClosestPoint(Q, V, prevClos, size));
			_closestB	= _closestA - prevClos;
			//normal		= -prevClos / prevDist;
			_dist	= max(prevDist - margins, 0);
			return true;
		}

	} while (dist > eps);

	_dist = zero;
	return true;
}

#endif

/*
A				:	shape to test
B				:	shape to test
_initial_dir	:	initial search direction in the minkowsky sum in the local space of ConvexB
inflation		:	sum of amounts we inflate the shapes (to collapse spheres to points, etc)
separated		:	stop search when separation is greater than this
eps				:	epsilon
_closestA		:	closest point in A in the local space of B if acceptance threshold is sufficent large
_closestB		:	closest point in B in the local space of B if acceptance threshold is sufficent large
_dist			:	the distance of the closest points between A and B if acceptance threshold is sufficent large
*/
template<int N> template<typename A, typename B> bool simplex_difference<N>::gjk(const A& a, const B& b, param_t<vec<float,N>> _initial_dir, float inflation, float separated, float eps, pos<float,N>& _closestA, pos<float,N>& _closestB, float& _dist) {
	auto	initial_dir	= select(len2(_initial_dir) > epsilon, _initial_dir, x_axis);
	auto	closestA	= a(-initial_dir);
	auto	closest		= closestA - b(initial_dir);
	float	dist		= len(closest);

	if (dot(initial_dir, closest) > separated * dist)
		return false;

	add_vertex(closest, closestA);

	const float relDif	= one - 0.000225f;	//square value of 1.5% which applied to the distance of a closest point(v) to the origin

	while (dist > eps) {
		auto	supportA	= a(-closest);
		auto	support		= supportA - b(closest);
		auto	signDist	= dot(closest, support) / dist;

		if (signDist > separated)
			return false;

		if (signDist > inflation && signDist > relDif * dist)
			break;

		//calculate the closest point between two convex hulls
		add_vertex(support, supportA);
		auto	closest1	= S::get_vertex();
		auto	dist1		= len(closest1);

		// if GJK degenerated, use the previous closest
		if (dist1 >= dist)
			break;

		dist		= dist1;
		closest		= closest1;
		closestA	= get_vertexA();
		if (S::full())
			break;
	}

	_closestA	= closestA;
	_closestB	= closestA - closest;
	_dist		= dist;
	return true;
}

/*
A is in the local space of B
A				:	shape to test
B				:	shape to sweep
_initial_dir	:	initial search direction in the mincowsky sum in the local space of xB
inflation		:	the amount by which we inflate the swept shape
eps				:	epsilon
s				:	the sweep ray origin
r				:	the normalized sweep ray direction scaled by the sweep distance. r should be in ConvexB's space
_t				:	the time of impact(TOI)
_normal			:	the contact normal
closestA		:	closest point in A in the local space of B
*/
template<int N> template<typename A, typename B> bool simplex_difference<N>::gjk_sweep(const A& a, const B& b, param_t<vec<float,N>> _initial_dir, float inflation, float eps, param_t<vec<float,N>> s, param_t<vec<float,N>> r, float& _t, vec<float,N>& normal, pos<float,N>& _closestA) {
	if (eps <= 0)
		eps = epsilon;

	if (len2(r) < eps)
		return false;

	inflation	+= eps;

	const float eps2		= square(eps);
	const float inflation2	= square(inflation);

	float	t	= zero;
	auto	x	= s;

	auto	initial_dir		= select(len2(_initial_dir) > epsilon, _initial_dir, x_axis);
	auto	closestA		= a(-initial_dir);
	auto	closest			= closestA - (b( initial_dir) + x);
	auto	prev_closest	= closest;
	float	dist			= len2(closest);
	
	add_vertex(closest, closestA);

	for (bool keep_going = dist > eps2; keep_going; keep_going = dist > inflation2) {
		auto	supportA		= a(-closest);
		auto	support			= supportA - (b( closest) + x);
		auto	norm			= closest * rsqrt(dist);

		float	vw		= inflation - dot(norm, support);
		if (vw < zero) {
			const float	vr = dot(norm, r);

			if (vr <= zero) {
				_t	= dot(supportA - s, r) / len2(r);
				return false;
			}

			t -= vw / vr;
			//if (t > one)
			//	return false;

			auto	x0	= x;
			x			= madd(r, t, s);
			auto	dx	= x0 - x;
			S::offset(dx);

			support			+= dx;
			dist			= maximum;
			prev_closest	= closest;
		}

		float	prev_dist	= dist;
		
		//calculate the closest point between two convex hulls
		add_vertex(support, supportA);
		closest		= S::get_vertex();
		dist		= len2(closest);

		if (dist >= prev_dist) {
			//GJK degenerated, use the previous closest
			_closestA	= pos<float, N>(closestA);
			_t			= t;
			normal		= -normalise(prev_closest);
			return true;
		}

		closestA	= get_vertexA();
		if (S::full())
			break;
	}

	_closestA	= pos<float, N>(closestA);
	_t			= t;
	normal		= -normalise(select(dist > eps2, closest, prev_closest));
	return true;
}

//EPA

int		EPAPenetration(const convex2& a, const convex2& b, const float2* A, const float2* B, const int size, float eps, position2& pa, position2& pb, float2& normal, float& penDepth);
int		EPAPenetration(const convex3& a, const convex3& b, const float3* A, const float3* B, const int size, float eps, position3& pa, position3& pb, float3& normal, float& penDepth);

int		EPAPenetrationDebug(const convex2& a, const convex2& b, float2* A, float2* B, const int size, int max_size, int steps);
int		EPAProbe(const convex2& a, float2* A, int size, float eps, int max_out);
template<typename A> int EPAProbeT(A&& a, float2* pts, int size, float eps, int max_out) {
	return EPAProbe(convex2(&a), pts, size, eps, max_out);
}

} // namespace iso

#endif //SIMPLEX_H
