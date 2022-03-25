#include "gjk.h"

using namespace iso;
/*
position2 GJK_convex_polygon::operator()(param(float2) v) {
#if 0
	int		i0	= 0;
	float2	a	= p.p[0] - centre;
	float1	sa	= cross(v, a);

	for (int n = p.n; n; n >>= 1) {
		int		i	= i0 + (n >> 1);
		float2	b	= p.p[i] - centre;
		if (cross(v, b) < zero && cross(a, b) * sa < zero) {
			i0 = i + 1;
			--n;
		}
	}

	int	i1 = i0 == 0 ? p.n - 1 : i0 - 1;
	if (dot(v, p.p[i1] - p.p[i0]) > zero)
		i0 = i1;

	prev_i	= i0;
	return p.p[i0] - centre;
#else
	float	bestd = dot(float2(p.p[0]), v);
	int		besti = 0;
	for (int i = 1; i < p.n; i++) {
		float	d = dot(float2(p.p[i]), v);
		if (d > bestd) {
			bestd = d;
			besti = i;
		}
	}
	prev_i = besti;
	return p.p[besti] + centre;
#endif
}
*/
//-----------------------------------------------------------------------------
//	2D
//-----------------------------------------------------------------------------

// solve t line segment using barycentric coordinates.
//
// p = t1 * w1 + t2 * w2
// t1 + t2 = 1
//
// The vector from the origin to the closest point on the line is perpendicular to the line.
// e12 = w2 - w1
// dot(p, e) = 0
// t1 * dot(w1, e) + t2 * dot(w2, e) = 0
//
// 2-by-2 linear system
// [1      1     ][t1] = [1]
// [w1.e12 w2.e12][t2] = [0]
//
// Define
// d12_1 =  dot(w2, e12)
// d12_2 = -dot(w1, e12)
// d12 = d12_1 + d12_2
//
// Solution
// t1 = d12_1 / d12
// t2 = d12_2 / d12
// NB - not storing redundant weights

int GJK2::solve2() {
	float2 e12	= v2.xy - v1.xy;

	// w1 region
	float d12_2 = -dot(v1.xy, e12);
	if (d12_2 <= zero)
		return 1;

	// w2 region
	float d12_1 = dot(v2.xy, e12);
	if (d12_1 <= zero) {
		v1		= v2;
		return 1;
	}

	// Must be in e12 region
	v1.z	= d12_1 / (d12_1 + d12_2);
	return 2;
}

// Possible regions:
// - points[2]
// - edge points[0]-points[2]
// - edge points[1]-points[2]
// - inside the triangle
// NB - not storing redundant weights

int GJK2::solve3() {
	// Edge12
	// [1      1     ][t1] = [1]
	// [w1.e12 w2.e12][t2] = [0]
	// a3 = 0
	float2	e12		= v2.xy - v1.xy;
	float	d12_1	=  dot(v2.xy, e12);
	float	d12_2	= -dot(v1.xy, e12);

	// Edge13
	// [1      1     ][t1] = [1]
	// [w1.e13 w3.e13][a3] = [0]
	// t2 = 0
	float2	e13		= v3.xy - v1.xy;
	float	d13_1	=  dot(v3.xy, e13);
	float	d13_2	= -dot(v1.xy, e13);

	// Edge23
	// [1      1     ][t2] = [1]
	// [v2.xy.e23 w3.e23][a3] = [0]
	// t1 = 0
	float2	e23		= v3.xy - v2.xy;
	float	d23_1	=  dot(v3.xy, e23);
	float	d23_2	= -dot(v2.xy, e23);

	// Triangle123
	float n123		= cross(e12, e13);
	float d123_1	= n123 * cross(v2.xy, v3.xy);
	float d123_2	= n123 * cross(v3.xy, v1.xy);
	float d123_3	= n123 * cross(v1.xy, v2.xy);

	// w1 region
	if (d12_2 <= zero && d13_2 <= zero)
		return 1;

	// e12
	if (d12_1 > zero && d12_2 > zero && d123_3 <= zero) {
		v1.z	= d12_1 / (d12_1 + d12_2);
		return 2;
	}

	// e13
	if (d13_1 > zero && d13_2 > zero && d123_2 <= zero) {
		v1.z	= d13_1 / (d13_1 + d13_2);
		v2		= v3;
		return 2;
	}

	// w2 region
	if (d12_1 <= zero && d23_2 <= zero) {
		v1	= v2;
		return 1;
	}

	// w3 region
	if (d13_1 <= zero && d23_1 <= zero) {
		v1	= v3;
		return 1;
	}

	// e23
	if (d23_1 > zero && d23_2 > zero && d123_1 <= zero) {
		v1	= concat(v3.xy, d23_2 / (d23_1 + d23_2));
		return 2;
	}

	// Must be in triangle123
	float inv_d123 = 1 / (d123_1 + d123_2 + d123_3);
	v1.z	= d123_1 * inv_d123;
	v2.z	= d123_2 * inv_d123;
	return 3;
}

bool GJK2::run(GJK2_Primitive &a, GJK2_Primitive &b, int count, float2 *separation) {
	if (separation)
		*separation	= zero;

	float2	sep		= closest(count);
	float	dist	= len2(sep);

	for (int i = 0; i < 20; ++i) {
		// get search direction
		float2	dir = get_dir(count, v1.xy, v2.xy);

		// ensure the search direction is numerically fit
		// if not, the origin is probably contained by a line segment or triangle, thus the shapes are overlapped, but it is difficult to determine if it is just very close to it
		if (len2(dir) < square((float)epsilon))
			return true;

		// compute tentative new simplex vertex using support points
		float3	&v	= (&v1)[count++];
		v.xy		= b.support(dir) - a.support(-dir);

		if (dot(dir, v.xy) < zero)
			break;

		if (count == 2) {
			count = solve2();
		} else if (count == 3) {
			count = solve3();
			if (count == 3)	// if we have 3 points, then the origin is in the corresponding triangle
				return true;
		}

		// compute closest point
		sep		= count == 1 ? float2(v1.xy) : v1.xy * v1.z + v2.xy * (one - v1.z);

		float prev_dist = dist;
		dist	= len2(sep);

		if (dist < square((float)epsilon))
			return true;

		if (dist >= prev_dist)
			break;
	}

	if (separation)
		*separation = sep;
	return false;
}

bool GJK2::run(support &a, support &b, int count, float2 *separation) {
	if (separation)
		*separation	= zero;

	float2	sep		= closest(count);
	float	dist	= len2(sep);

	for (int i = 0; i < 20; ++i) {
		// get search direction
		float2	dir = get_dir(count, v1.xy, v2.xy);

		// ensure the search direction is numerically fit
		// if not, the origin is probably contained by a line segment or triangle, thus the shapes are overlapped, but it is difficult to determine if it is just very close to it
		if (len2(dir) < square((float)epsilon))
			return true;

		// compute tentative new simplex vertex using support points
		float3	&v	= (&v1)[count++];
		v.xy		= b(dir) - a(-dir);

		if (dot(dir, v.xy) < zero)
			break;

		if (count == 2) {
			count = solve2();
		} else if (count == 3) {
			count = solve3();
			if (count == 3)	// if we have 3 points, then the origin is in the corresponding triangle
				return true;
		}

		// compute closest point
		sep		= count == 1 ? float2(v1.xy) : v1.xy * v1.z + v2.xy * (one - v1.z);

		float prev_dist = dist;
		dist	= len2(sep);

		if (dist < square((float)epsilon))
			return true;

		if (dist >= prev_dist)
			break;
	}

	if (separation)
		*separation = sep;
	return false;
}
/*
template<> void iso::Distance(DistanceOutput* output, GJK2_TransShapePrim<circle> &&a, GJK2_TransShapePrim<circle> &&b) {
	output->Set(
		a.mat.translation(),
		b.mat.translation()
	);
	output->ApplyRadii(a.mat.xx, b.mat.xx);
}
*/
//-----------------------------------------------------------------------------
//	3D
//-----------------------------------------------------------------------------

void GJK3::compute_det() {
	for (int i = 0, t = bits; t; ++i, t >>= 1) {
		if (t & 1)
			dp[i][last] = dp[last][i] = dot(y[i], y[last]);
	}
	dp[last][last] = dot(y[last], y[last]);

	det[last_bit][last] = 1;
	for (int j = 0, sj = 1; j < 4; ++j, sj <<= 1) {
		if (bits & sj) {
			int		s2		= sj | last_bit;
			det[s2][j]		= dp[last][last]- dp[last][j];
			det[s2][last]	= dp[j][j]		- dp[j][last];
			for (int k = 0, sk = 1; k < j; ++k, sk <<= 1) {
				if (bits & sk) {
					int		s3		= sk | s2;
					det[s3][k]		= det[s2][j]			* (dp[j][j]		- dp[j][k])
									+ det[s2][last]			* (dp[last][j]	- dp[last][k]);
					det[s3][j]		= det[sk|last_bit][k]	* (dp[k][k]		- dp[k][j])
									+ det[sk|last_bit][last]* (dp[last][k]	- dp[last][j]);
					det[s3][last]	= det[sk|sj][k]			* (dp[k][k]		- dp[k][last])
									+ det[sk|sj][j]			* (dp[j][k]		- dp[j][last]);
				}
			}
		}
	}
	if (all_bits == 15) {
		det[15][0]	= det[14][1] * (dp[1][1] - dp[1][0])
					+ det[14][2] * (dp[2][1] - dp[2][0])
					+ det[14][3] * (dp[3][1] - dp[3][0]);

		det[15][1]	= det[13][0] * (dp[0][0] - dp[0][1])
					+ det[13][2] * (dp[2][0] - dp[2][1])
					+ det[13][3] * (dp[3][0] - dp[3][1]);

		det[15][2]	= det[11][0] * (dp[0][0] - dp[0][2])
					+ det[11][1] * (dp[1][0] - dp[1][2])
					+ det[11][3] * (dp[3][0] - dp[3][2]);

		det[15][3]	= det[ 7][0] * (dp[0][0] - dp[0][3])
					+ det[ 7][1] * (dp[1][0] - dp[1][3])
					+ det[ 7][2] * (dp[2][0] - dp[2][3]);
	}
}

void GJK3::clear_old_dets() {
	for (int j = 0; j < 16; j++) {
		for (int i = 0; i < 4; ++i) {
			if (!(bits & (1<<i)) || !(bits & j))
				det[j][i] = FLT_NAN;
		}
	}
}

bool GJK3::valid(int s) const {
	for (int i = 0, bit = 1; i < 4; ++i, bit <<= 1) {
		if (all_bits & bit) {
			if (s & bit) {
				if (det[s][i] <= 0)
					return false;
			} else if (det[s|bit][i] > 0) {
				return false;
			}
		}
	}
	return true;
}
float3 GJK3::compute_vector(const float3 *p, int bits1) const {
	float	sum		= 0;
	float3	v		= float3(zero);
	const float	*d	= det[bits1];
	for (int t = bits1; t; t >>= 1, ++d, ++p) {
		if (t & 1) {
			sum += *d;
			v	+= *p * *d;
		}
	}
	return v / sum;
}

bool GJK3::closest(float3 &v) {
	for (int s = bits; s; --s) {
		if ((s & bits) == s) {
			if (valid(s | last_bit)) {
				bits = s | last_bit;
				clear_old_dets();
				v = compute_vector(y, bits);
				return true;
			}
		}
	}
	if (valid(last_bit)) {
		bits	= last_bit;
		v		= y[last];
		return true;
	}
	return false;
}

// used for detecting degenerate cases that cause termination problems due to rounding errors.
bool GJK3::degenerate(param(float3) w, int bits) {
	for (int i = 0, t = bits; t; ++i, t >>= 1) {
		if ((t & 1) && len2(y[i] - w) < square(0.001f))
			return true;
	}
	return false;
}

bool GJK3::intersect(GJK3_Primitive &a, GJK3_Primitive &b, float3 &v, float sep, float sep_thresh) {
	float	v2		= dot(v, v);
	int		iter	= 0;
	do {
		for (last = 0, last_bit = 1; bits & last_bit; ++last, last_bit <<= 1);

		position3	va	= a.support(-v);
		position3	vb	= b.support( v);
		float3		w	= va - vb;

		p[last]		= va;
		y[last]		= w;

		float	dotvw = dot(v, w);
		if (dotvw > sep) {
			max_sep = max(max_sep, dotvw * rsqrt(v2));
//			return false;
			// check for separation and convergence
			if (max_sep > sep_thresh || max_sep > 0.999f * sqrt(v2)) {
				position	= position3((va.v + vb.v) * half);
				normal		= v;
				return false;
			}
		}

		if (degenerate(w, all_bits))
			return false;

		all_bits	= bits | last_bit;

		compute_det();
		if (!closest(v) || ++iter == 100)
			return false;

		v2	= dot(v, v);

	} while (bits < 15 && v2 > .0001f);
	return true;
}

bool GJK3::collide(GJK3_TransPrim &a, GJK3_TransPrim &b, bool separate) {
	float3	v	= get_trans(a.mat) - get_trans(b.mat);
	if (all(v == zero))
		return false;

	if (!intersect(a, b, v, 0, 0))
		return false;

	if (!separate) {
		penetration	= len(v);
		position	= position3(compute_vector((float3*)p, bits));
		normal		= -normalise(compute_vector(y, bits));
		return true;
	}

	v	= get_trans(a.mat) - get_trans(b.mat);
	float	v2	= dot(v, v);

	const float SEP_CONV_THRESH	= 0.001f;
	const float	EXTRA_SEP		= 0.5f;
	const int	MAX_ITERS		= 10;

	float3	sep;
	for (int iters = 0; iters < MAX_ITERS; iters++) {
		position3	va	= a.support(-v);
		position3	vb	= b.support( v);
		float3		w	= va - vb;
		sep			= v * (rsqrt(v2) * EXTRA_SEP - dot(v, w) / v2);
		reset();
		intersect(a, b, v, EXTRA_SEP, 1e20f);
		v2	= dot(v, v);
		if (max_sep * (1.0f - SEP_CONV_THRESH) < EXTRA_SEP)
			break;
	}
	penetration	= len(sep) - sqrt(v2);// - max_sep;
	position	= position3(compute_vector((float3*)p, bits) - sep);
	normal		= -normalise(sep + compute_vector(y, bits));
	return true;
}
