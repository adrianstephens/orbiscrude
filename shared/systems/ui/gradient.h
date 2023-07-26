#ifndef GRADIENT_H
#define GRADIENT_H

#include "extra/colour.h"
#include "maths/geometry.h"

namespace iso {

struct GradientTransform {
	enum FILL : uint8 {
		SOLID, LINEAR, SWEEP, RADIAL, RADIAL0, RADIAL1, RADIAL_GT1, RADIAL_LT1_SWAP, RADIAL_SAME,
	};

	FILL		fill	= SOLID;
	bool		reverse	= false;
	float2x3	transform;
	float2		params;

	void	Set(FILL _fill, float2x3 _transform = identity, float2 _params = zero) {
		fill		= _fill;
		transform	= _transform;
		params		= _params;
		reverse		= false;
	}
	void	Transform(float2x3 t) {
		transform = t * transform;
	}

	void	SetLinear(position2 p0, position2 p1, position2 p2) {
		Set(LINEAR, triangle(p0, p1, p2).matrix());
	}
	void	SetLinear(position2 p0, float2 dir) {
		Set(LINEAR, float2x3{dir, perp(dir), p0});
	}
	void	SetSweep(position2 p0, float angle0, float angle1) {
		Set(SWEEP, translate(p0) * rotate2D(angle0), float2{1 / (angle1 - angle0), 0});
	}
	void	SetRadial(circle c0, circle c1) {
		bool	swapped	= c0.radius2() > c1.radius2();
		if (swapped)
			swap(c0, c1);

		float		r0	= c0.radius();
		float		r1	= c1.radius();
		float2		x	= c1.centre() - c0.centre();

		if (r0 == r1) {
			Set(RADIAL_SAME, float2x3(x, perp(x), c0.centre()), {square(r0) / len2(x), 0});

		} else {
			float	f1	= r1 / (r1 - r0);

			if (all(x == zero)) {
				Set(RADIAL0, translate(c0.centre()) * scale(r1), {r0 / r1, f1});

			} else {
				position2	cf	= lerp(c1.centre(), c0.centre(), f1);	//focal point
				x	= c1.centre() - cf;
				r1 *= rlen(x);

				float2x3	trans(x, perp(x), cf);
				if (r1 == 1) {
					Set(RADIAL1, trans * scale(2 / f1), {1 - f1, r1});

				} else {
					float	t	= square(r1) - 1;
					float2	s	= {t / r1, r1 < 1 ? -sqrt(-t) : sqrt(t)};
					Set(r1 > 1 ? RADIAL_GT1 : swapped ? RADIAL_LT1_SWAP : RADIAL, trans * scale(s / f1), {1 - f1, r1});
					if (r1 < 1)
						swapped = false;
				}
			}
		}
		reverse = swapped;
	}


	bool operator==(const GradientTransform& b) const {
		return fill == b.fill && transform == b.transform && all(params == b.params) && reverse == b.reverse;
	}

	float	get_value(position2 p) const {
		float2	uv = p / transform;

		switch (fill) {
			case LINEAR:
				return uv.x;

			case SWEEP:
				return atan2(uv) / params.x;

			case RADIAL:	{	//RADIAL	(r1<1 and !swapped)
				float	d = square(uv.x) - square(uv.y);
				if (d < 0)
					return nan;
				float	x = sqrt(d) - uv.x / params.y;
				return x < 0 ? nan : params.x + x;
			}
			case RADIAL0:		//RADIAL0
				return (sqrt(dot(uv, uv)) - params.x) * params.y;// / (1 - params.x);

			case RADIAL1:	{	//RADIAL1	(r1==1)
				float	x	= dot(uv, uv) / uv.x;
				return x < 0 ? nan : params.x + x;
			}
			case RADIAL_GT1:	{	//RADIAL_GT1	(r1>1)
				float	x	= sqrt(dot(uv, uv)) - uv.x / params.y;
				return params.x + x;
			}
			case RADIAL_LT1_SWAP:	{	//RADIAL_LT1_SWAP	(r1<1 and swapped)
				float	d = square(uv.x) - square(uv.y);
				if (d < 0)
					return nan;
				float	x = sqrt(d) + uv.x / params.y;
				return x > 0 ? nan : 1 - params.x + x;
			}
			case RADIAL_SAME:	{	//RADIAL_SAME	(r0 == r1)
				float	d = params.x - square(uv.y);
				return d < 0 ? nan : uv.x + sqrt(d);
			}

			default:
				return 0;
		}
	}


	mat<float,2,4>	scaled_range(interval<float> t) {
		if (t.a == 0 && t.b == 1)
			return {transform, params};

		switch (fill) {
			default:
			case SOLID:
				return {transform, params};
			case LINEAR:
				return {transform * translate(t.a, 0) * scale(t.extent()), params};
			case SWEEP:
				//return {transform, params};
				return {transform * rotate2D(t.a / params.x), params / (t.b - t.a)};
			case RADIAL:
			case RADIAL1:
			case RADIAL_GT1:
			case RADIAL_LT1_SWAP:
			case RADIAL_SAME:
				return {transform * scale(t.b - t.a), float2{(params.x - t.a) / (t.b - t.a), params.y}};
			case RADIAL0: {
				float	p0 = params.x + t.a / t.b;
				return {transform * scale(t.b), float2{p0, 1 / (1 - p0)}};
			}
		}
	}

	bool	scale_range(interval<float> t) {
		if (t.a == 0 && t.b == 1)
			return false;

		switch (fill) {
			case SOLID:
				return false;
			case LINEAR:
				transform	= transform * scale(1 / t.extent()) * translate(-t.a, 0);
				return true;
			case SWEEP:
				return false;
			case RADIAL:
			case RADIAL1:
			case RADIAL_GT1:
			case RADIAL_LT1_SWAP:
			case RADIAL_SAME: {
				transform	= transform * scale(t.b - t.a);
				params.x	= (params.x - t.a) / (t.b - t.a);
				return true;
			}
			case RADIAL0:
				transform	= transform * scale(t.b);
				params.x	+= t.a / t.b;
				params.y	= 1 / (1 - params.x);
				return true;
			default:
				return true;
		}
	}
};

}//namespace iso

#endif	// GRADIENT_H
