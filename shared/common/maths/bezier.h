#ifndef BEZIER_H
#define BEZIER_H

#include "maths/geometry.h"
#include "maths/polynomial.h"

namespace iso {

template<typename T, int N> struct spline {
	T	c[N];

	template<typename...P> spline(P&&...p) {}
	T	evaluate(float t) const	{ return poly_eval(c, t); }
//	spline<T, N - 1>	tangent() const { return }
};

//-----------------------------------------------------------------------------
// bezier_spline
//-----------------------------------------------------------------------------

struct cubic_param : position3	{
	cubic_param(const cubic_param &p)	: position3((const position3&)p)			{}
	template<typename T> cubic_param(const T &t) : position3(t * t * t, t * t, t)	{}
};

struct bezier_spline : public aligner<16> {
	static	float4x4	blend, tangentblend, normalblend;
	float4	c0, c1, c2, c3;

	struct T {
		float4 v;
		explicit T(float t)		: v(blend * cubic_param(t))		{}
		T(const cubic_param &t)	: v(blend * t)					{}
	};

	force_inline bezier_spline() {}
	force_inline bezier_spline(param(float4) c0, param(float4) c1, param(float4) c2, param(float4) c3)	: c0(c0), c1(c1), c2(c2), c3(c3) {}
	force_inline bezier_spline(param(float3) c0, param(float3) c1, param(float3) c2, param(float3) c3)	: c0(concat(c0,zero)), c1(concat(c1,zero)), c2(concat(c2,zero)), c3(concat(c3,zero)) {}
	force_inline bezier_spline(param(float2) c0, param(float2) c1, param(float2) c2, param(float2) c3)	: c0(concat(c0,zero,zero)), c1(concat(c1,zero,zero)), c2(concat(c2,zero,zero)), c3(concat(c3,zero,zero)) {}
	force_inline bezier_spline(param(position3) c0, param(position3) c1, param(position3) c2, param(position3) c3)	: c0(concat(c0.v,one)), c1(concat(c1.v,one)), c2(concat(c2.v,one)), c3(concat(c3.v,one)) {}
	force_inline bezier_spline(param(position2) c0, param(position2) c1, param(position2) c2, param(position2) c3)	: c0(concat(c0.v,one,zero)), c1(concat(c1.v,one,zero)), c2(concat(c2.v,one,zero)), c3(concat(c3.v,one,zero)) {}

	force_inline const float4&		operator[](int i)						const	{ return (&c0)[i];	}
	force_inline float4&			operator[](int i)								{ return (&c0)[i];	}
	force_inline const float4x4&	as_float4x4()							const	{ return *(const float4x4*)this; }

	force_inline float4				evaluate0(const T &t)					const {
#ifdef __MWERKS__
		float4 v;
		wii::mul(&v, (float4x4*)&c0, &t);
		return v;
#else
		return c0 * t.v.x + c1 * t.v.y + c2 * t.v.z + c3 * t.v.w;
#endif
	}

	force_inline float4	evaluate(const cubic_param &p)	const	{ float4 v(blend * p); return c0 * v.x + c1 * v.y + c2 * v.z + c3 * v.w;	}
	force_inline float4	tangent(const cubic_param &p)	const	{ float4 v(tangentblend * p); return c0 * v.x + c1 * v.y + c2 * v.z + c3 * v.w;	}
	force_inline float4	normal(const cubic_param &p)	const	{ float4 v(normalblend * p); return c0 * v.x + c1 * v.y + c2 * v.z + c3 * v.w;	}
	force_inline float4	curvature(float t)				const	{ return (c3 - c0 + (c1 - c2) * 3.f) * (t * 6) + (c0 + c2) * 6.f - c1 * 12.f;	}

	void				split(float t, bezier_spline &bl, bezier_spline &br) const;
	bezier_spline		split_left(float t)									const;
	bezier_spline		split_right(float t)								const;
	void				split(bezier_spline &bl, bezier_spline &br)			const;
	bezier_spline		split_left()										const;
	bezier_spline		split_right()										const;
	bezier_spline		flip()												const;
	bezier_spline		middle(float t0, float t1)							const;

	float4				closest_point(param(position2) pos, float *_t, float *_d) const;
	float4				closest_point(param(position3) pos, float *_t, float *_d) const;
	float4				closest_point(param(ray3) ray, float *_t, float *_d) const;
	bool				ray_check(param(ray2) r, float &t, vector2 *normal) const;
	position2			support(param(float2) v)							const;
	position3			support(param(float3) v)							const;
	bool				side_2d(param(ray2) pos)							const;

	friend bool	operator==(const bezier_spline &b1, const bezier_spline &b2) {
		return all(b1.c0 == b2.c0) && all(b1.c1 == b2.c1) && all(b1.c2 == b2.c2) && all(b1.c3 == b2.c3);
	}
	friend bezier_spline operator*(param(float4x4) m, const bezier_spline &s) {
		return bezier_spline(m * s.c0, m * s.c1, m * s.c2, m * s.c3);
	}
	friend bezier_spline operator*(param(float3x4) m, const bezier_spline &s) {
		return bezier_spline(m * position3(s.c0.xyz), m * position3(s.c1.xyz), m * position3(s.c2.xyz), m * position3(s.c3.xyz));
	}
	friend bezier_spline operator*(param(float2x3) m, const bezier_spline &s) {
		return bezier_spline(m * position2(s.c0.xy), m * position2(s.c1.xy), m * position2(s.c2.xy), m * position2(s.c3.xy));
	}
};

float	len(param(bezier_spline) b, int level = 3);

//-----------------------------------------------------------------------------
// bezier_patch
//-----------------------------------------------------------------------------

struct bezier_patch {
	bezier_spline	r0, r1, r2, r3;

	force_inline bezier_patch()	{}
	force_inline bezier_patch(param(bezier_spline) _r0, param(bezier_spline) _r1, param(bezier_spline) _r2, param(bezier_spline) _r3) : r0(_r0), r1(_r1), r2(_r2), r3(_r3) {}
	bezier_patch(const rectangle &r);

	force_inline const bezier_spline&	operator[](int i)					const	{ return (&r0)[i];		}
	force_inline bezier_spline&			operator[](int i)							{ return (&r0)[i];		}
	force_inline const float4&			cp(int i)							const	{ return (&r0.c0)[i];	}
	force_inline float4&					cp(int i)									{ return (&r0.c0)[i];	}

	force_inline const bezier_spline&	bottom()							const	{ return r0;			}
	force_inline bezier_spline			top()								const	{ return bezier_spline(r3.c3, r3.c2, r3.c1, r3.c0);}
	force_inline bezier_spline			left()								const	{ return bezier_spline(r3.c0, r2.c0, r1.c0, r0.c0);}
	force_inline bezier_spline			right()								const	{ return bezier_spline(r0.c3, r1.c3, r2.c3, r3.c3);}

	force_inline bezier_spline&			row(int i)									{ return (&r0)[i];		}
	force_inline const bezier_spline&	row(int i)							const	{ return (&r0)[i];		}
	force_inline bezier_spline			col(int i)							const	{ return bezier_spline(r0[i], r1[i], r2[i], r3[i]);}
	force_inline bezier_spline			evaluate_u(const cubic_param &u)	const	{ bezier_spline::T t(u); return bezier_spline(r0.evaluate0(t), r1.evaluate0(t), r2.evaluate0(t), r3.evaluate0(t)); }
	force_inline bezier_spline			evaluate_v(const cubic_param &v)	const	{ bezier_spline::T t(v); return bezier_spline(col(0).evaluate0(t), col(1).evaluate0(t), col(2).evaluate0(t), col(3).evaluate0(t)); }

	force_inline float4					evaluate(const cubic_param &u, const cubic_param &v)	const	{ return evaluate_u(u).evaluate(v);	}
	force_inline vector3				normal(const cubic_param &u, const cubic_param &v)		const	{ return cross(evaluate_v(v).tangent(u).xyz, evaluate_u(u).tangent(v).xyz);	}
	force_inline float4					evaluate(param(float2) uv)								const	{ return evaluate(uv.x, uv.y);	}
	force_inline vector3				normal(param(float2) uv)								const	{ return normal(uv.x, uv.y);	}

	bezier_patch		middle(float u0, float u1, float v0, float v1)		const;

	bool				calc_params_nr(param(position3) p, float4 &result, vector3 *normal, param(float2) uv) const;
	bool				calc_params_nr(param(float2) p, float4 &result, vector3 *normal, param(float2) uv) const;

	bool				ray_check(param(ray3) r, float &t, vector3 *normal)	const;

	bool				test(param(sphere) s, float &t, vector3 *normal)						const;
	bool				test(param(obb3) obb, float &t, position3 *position, vector3 *normal)	const;

	cuboid				get_box() const;
};

bezier_patch operator*(param(float4x4) m, const bezier_patch &p);
bezier_patch operator*(param(float3x4) m, const bezier_patch &p);

} //namespace iso

#endif	// BEZIER_H
