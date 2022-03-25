#ifndef NEW_VECTOR
#include "vector.h"

namespace iso {

const float4_const axis_s<0>::v = {1,0,0,0};
const float4_const axis_s<1>::v = {0,1,0,0};
const float4_const axis_s<2>::v = {0,0,1,0};
const float4_const axis_s<3>::v = {0,0,0,1};

axis_s<0> x_axis;
axis_s<1> y_axis;
axis_s<2> z_axis, xy_plane;
axis_s<3> w_axis;

//-----------------------------------------------------------------------------
// trigonometry
//-----------------------------------------------------------------------------

#ifdef ISO_SOFT_TRIG

namespace trig_consts {
	static const double SC[]	= {
		-1.6666666666666666666667e-01, // 1/3!
		+8.3333333333333333333333e-03, // 1/5!
		-1.9841269841269841269841e-04, // 1/7!
		+2.7557319223985890652557e-06, // 1/9!
		-2.5052108385441718775052e-08, // 1/11!
		+1.6059043836821614599392e-10, // 1/13!
		-7.6471637318198164759011e-13, // 1/15!
		+2.8114572543455207631989e-15, // 1/17!
	};
	static const double CC[]	= {
		-5.0000000000000000000000e-01, // 1/2!
		+4.1666666666666666666667e-02, // 1/4!
		-1.3888888888888888888889e-03, // 1/6!
		+2.4801587301587301587302e-05, // 1/8!
		-2.7557319223985890652557e-07, // 1/10!
		+2.0876756987868098979210e-09, // 1/12!
		-1.1470745597729724713852e-11, // 1/14!
		+4.7794773323873852974382e-14, // 1/16!
	};
	static const double PC[]	= {
		-2.7368494524164255994e+01,
		+5.7208227877891731407e+01,
		-3.9688862997404877339e+01,
		+1.0152522233806463645e+01,
		-6.9674573447350646411e-01,
	};
	static const double QC[]	= {
		-1.6421096714498560795e+02,
		+4.1714430248260412556e+02,
		-3.8186303361750149284e+02,
		+1.5095270841030604719e+02,
		-2.3823859153670238830e+01,
	};
}

double sin(double x) {
	using namespace trig_consts;
	double absx		= abs(x);
	double cycles	= round(absx / pi);
	double sign		= fsel(trunc(cycles * 0.5) - (cycles * 0.5), x, -x);
	double norm_x	= absx - pi * cycles;
	double y		= norm_x * norm_x;
#if 0
	double taylor	= (((((((SC[7]
	* y + SC[6])
	* y + SC[5])
	* y + SC[4])
	* y + SC[3])
	* y + SC[2])
	* y + SC[1])
	* y + SC[0])
	* y;
#else
	double taylor	= horner(y, 0, SC[0], SC[1], SC[2], SC[3], SC[4], SC[5], SC[6], SC[7]);
#endif
	return (norm_x + norm_x * taylor) * fsel(sign, 1.0, -1.0);
}
double cos(double x) {
	return sin(x + pi / two);
}
void sincos(double x, double *s, double *c) {
	using namespace trig_consts;
	double absx		= abs(x);
	double cycles	= round(absx / pi);
	double norm_x	= absx - pi * cycles;

	double y		= square(norm_x);
	double taylors	= horner(y, 0, SC[0], SC[1], SC[2], SC[3], SC[4], SC[5], SC[6], SC[7]);
	double taylorc	= horner(y, 0, CC[0], CC[1], CC[2], CC[3], CC[4], CC[5], CC[6], CC[7]);

	double sign = fsel(trunc(cycles * 0.5) - (cycles * 0.5), 1.0, -1.0);
	*s = (norm_x + norm_x * taylors) * fsel(x, sign, -sign);
	*c = (1.0 + taylorc) * sign;
}
double tan(double x) {
	double s, c;
	sincos(x, &s, &c);
	return s / c;
}

double asin(double x) {
	using namespace trig_consts;
	double abs_x	= abs(x);
	double g		= fsel(abs_x - 0.5, (1.0 - abs_x) * 0.5, abs_x * abs_x);
	double y		= fsel(abs_x - 0.5, -2.0 * sqrt(g), abs_x);
#if 0
	double P = (((((PC[4]
	* g + PC[3])
	* g + PC[2])
	* g + PC[1])
	* g + PC[0])
	* g);
	double Q = (((((g + QC[4])
	* g + QC[3])
	* g + QC[2])
	* g + QC[1])
	* g + QC[0]);
#else
	double P = horner(g, 0, PC[0], PC[1], PC[2], PC[3], PC[4]);
	double Q = horner(g, 0, QC[0], QC[1], QC[2], QC[3], QC[4]);
#endif
	double R = y + y * P / Q;

	double res = fsel(abs_x - 0.5, pi / two + R, R);
	return fsel(x, res, -res);
}
double acos(double x) {
	using namespace trig_consts;
	return pi / two - asin(x);
}

force_inline double _atan(double x) {
	using namespace trig_consts;
	double x2 = x * x;
	double hi = (((0.0028662257f
		* x2 - 0.0161657367f)
		* x2 + 0.0429096138f)
		* x2 - 0.0752896400f)
		* x2 + 0.1065626393f;
	double lo = ((-0.1420889944f
		* x2 + 0.1999355085f)
		* x2 - 0.3333314528f)
		* x2;
	return (hi * square(square(x2)) + lo) * x + x;
}

double atan(double x) {
	using namespace trig_consts;
	double	select	= abs(x) - 1.0;
	double	bias	= fsel(select, fsel(x, pi / two, -pi / two), 0);
	return _atan(fsel(select, -reciprocal(x), x)) + bias;
}

double atan2(double y, double x) {
	using namespace trig_consts;
	double	select	= abs(y) - abs(x);
	double	bias	= fsel(select, pi / two, fsel(x, 0, pi));
	double	res		= _atan(fsel(select, x, y) / fsel(select, -y, x)) + copysign(bias, y);
	return fsel(abs(x) + abs(y) - 1e-50, res, 0.0);
}

#endif

#define _SINCOS_CC0	-0.0013602249f
#define _SINCOS_CC1	 0.0416566950f
#define _SINCOS_CC2	-0.4999990225f
#define _SINCOS_SC0	-0.0001950727f
#define _SINCOS_SC1	 0.0083320758f
#define _SINCOS_SC2	-0.1666665247f

#define _SINCOS_KC1	1.57079625129f
#define _SINCOS_KC2	7.54978995489e-8f

vf4 sin(param(vf4) x) {
	static const float4_const	consts1 = {_SINCOS_KC1, _SINCOS_KC2, _SINCOS_CC2, _SINCOS_SC2};
	static const float4_const	consts2 = {_SINCOS_CC0, _SINCOS_CC1, _SINCOS_SC0, _SINCOS_SC1};

	// Range reduction using : xl = angle * TwoOverPi;
	vf4	xl	= mul(x, vload<vf4>(0.63661977236f));

	// Find the quadrant the angle falls in using: q = (int) (ceil(abs(xl))*sign(xl))
	xl		= add(xl, vsel((vf4)half, xl, (vf4)sign_mask));
	vi4	q	= vconvert<vi4>(xl);

	// Remainder in range [-pi/4..pi/4]
	vf4 qf	= vconvert<vf4>(q);
	vf4 p1	= nmsub(qf, swizzle<vf4,0,0,0,0>(consts1), x);
	xl		= nmsub(qf, swizzle<vf4,1,1,1,1>(consts1), p1);
	vf4	xl2	= mul(xl, xl);
	vf4	xl3	= mul(xl2,xl);

	// Compute both the sin and cos of the angles using a polynomial expression:
	//	cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//	sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	vf4 ct1 = madd(swizzle<vf4,0,0,0,0>(consts2), xl2, swizzle<vf4,1,1,1,1>(consts2));
	vf4 st1 = madd(swizzle<vf4,2,2,2,2>(consts2), xl2, swizzle<vf4,3,3,3,3>(consts2));

	vf4 ct2 = madd(ct1, xl2, swizzle<vf4,2,2,2,2>(consts1));
	vf4 st2 = madd(st1, xl2, swizzle<vf4,3,3,3,3>(consts1));

	vf4 cx = madd<vf4>(ct2, xl2, one);
	vf4 sx = madd(st2, xl3, xl);

	// Use the cosine when the offset is odd and the sin when the offset is even
	vf4	res = vsel(cx, sx, viszero(vand(q, vloadu<1>())));

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	vb4 mask2 = viszero(vand(q, vloadu<2>()));
	return vsel(force_cast<vf4>(vxor((vf4)sign_mask, res)), res, mask2);
}

vf4 cos(param(vf4) x) {
	static const float4_const	consts1 = {_SINCOS_KC1, _SINCOS_KC2, _SINCOS_CC2, _SINCOS_SC2};
	static const float4_const	consts2 = {_SINCOS_CC0, _SINCOS_CC1, _SINCOS_SC0, _SINCOS_SC1};

	// Range reduction using : xl = angle * TwoOverPi;
	vf4 xl = mul(x, vload<vf4>(0.63661977236f));

	// Find the quadrant the angle falls in using: q = (int) (ceil(abs(xl))*sign(xl))
	xl = add(xl, vsel((vf4)half, xl, (vf4)sign_mask));
	vi4	q = vconvert<vi4>(xl);

	// Remainder in range [-pi/4..pi/4]
	vf4 qf	= vconvert<vf4>(q);
	vf4 p1	= nmsub(qf, swizzle<vf4,0,0,0,0>(consts1), x);
	xl		= nmsub(qf, swizzle<vf4,1,1,1,1>(consts1), p1);
	vf4 xl2	= mul(xl, xl);
	vf4 xl3 = mul(xl2,xl);

	// Compute both the sin and cos of the angles using a polynomial expression:
	//	cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//	sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	vf4 ct1 = madd(swizzle<vf4,0,0,0,0>(consts2), xl2, swizzle<vf4,1,1,1,1>(consts2));
	vf4 st1 = madd(swizzle<vf4,2,2,2,2>(consts2), xl2, swizzle<vf4,3,3,3,3>(consts2));

	vf4 ct2 = madd(ct1, xl2, swizzle<vf4,2,2,2,2>(consts1));
	vf4 st2 = madd(st1, xl2, swizzle<vf4,3,3,3,3>(consts1));

	vf4 cx = madd<vf4>(ct2, xl2, one);
	vf4 sx = madd(st2, xl3, xl);

	// Use the sine when the offset is odd and the cosine when the offset is even
	vf4	res = vsel(sx, cx, viszero(vand(q, vloadu<1>())));

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	vb4 mask2 = viszero(vand(add(q, vloadu<1>()), vloadu<2>()));
	return vsel(force_cast<vf4>(vxor((vf4)sign_mask, res)), res, mask2);
}

void _sincos(param(vf4) x, vf4 *sin, vf4 *cos) {
	static const float4_const	consts1 = {_SINCOS_KC1, _SINCOS_KC2, _SINCOS_CC2, _SINCOS_SC2};
	static const float4_const	consts2 = {_SINCOS_CC0, _SINCOS_CC1, _SINCOS_SC0, _SINCOS_SC1};

	// Range reduction using : xl = angle * 2 / pi;
	vf4 xl = mul(x, vload<vf4>(0.63661977236f));

	// Find the quadrant the angle falls in using: q = (int) (ceil(abs(xl))*sign(xl))
	xl = add(xl, vsel((vf4)half, xl, (vf4)sign_mask));
	vi4	q	= vconvert<vi4>(xl);

	// Remainder in range [-pi/4..pi/4]
	vf4 qf	= vconvert<vf4>(q);
	vf4 p1	= nmsub(qf, swizzle<vf4,0,0,0,0>(consts1), x);
	xl		= nmsub(qf, swizzle<vf4,1,1,1,1>(consts1), p1);
	vf4 xl2	= mul(xl, xl);
	vf4 xl3	= mul(xl2,xl);

	// Compute both the sin and cos of the angles using a polynomial expression:
	//	cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//	sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	vf4 ct1 = madd(swizzle<vf4,0,0,0,0>(consts2), xl2, swizzle<vf4,1,1,1,1>(consts2));
	vf4 st1 = madd(swizzle<vf4,2,2,2,2>(consts2), xl2, swizzle<vf4,3,3,3,3>(consts2));

	vf4 ct2 = madd(ct1, xl2, swizzle<vf4,2,2,2,2>(consts1));
	vf4 st2 = madd(st1, xl2, swizzle<vf4,3,3,3,3>(consts1));

	vf4 cx = madd<vf4>(ct2, xl2, one);
	vf4 sx = madd(st2, xl3, xl);

	// Use the sine when the offset is odd and the cosine when the offset is even
	vb4	sc	= viszero(vand(q, vloadu<1>()));
	vf4	_sin = vsel(sx, cx, sc);
	vf4	_cos = vsel(cx, sx, sc);

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	vb4 mask_sin = viszero(vand(q, vloadu<2>()));
	vb4 mask_cos = viszero(vand(add(q, vloadu<1>()), vloadu<2>()));
	*sin = vsel(force_cast<vf4>(vxor(force_cast<vu4>(_sin), (vf4)sign_mask)), _sin, mask_sin);
	*cos = vsel(force_cast<vf4>(vxor(force_cast<vu4>(_cos), (vf4)sign_mask)), _cos, mask_cos);
}

vf4 tan(param(vf4) x) {
	static const float4_const	consts1 = {_SINCOS_KC1, _SINCOS_KC2, _SINCOS_CC2, _SINCOS_SC2};
	static const float4_const	consts2 = {0.63661977236f, 0.0097099364f, -0.4291161787f, -0.0957822992f};

	// Range reduction using : xl = angle * 2 / pi;
	vf4 xl	= mul(x, swizzle<vf4,0,0,0,0>(consts2));

	// Find the quadrant the angle falls in using: q = (int) (ceil(abs(x))*sign(x))
	xl		= add(xl, vsel((vf4)half, xl, (vf4)sign_mask));
	vi4	q	= vconvert<vi4>(xl);

	// Remainder in range [-pi/4..pi/4]
	vf4 qf	= vconvert<vf4>(q);
	vf4 p1	= nmsub(qf, swizzle<vf4,0,0,0,0>(consts1), x);
	xl		= nmsub(qf, swizzle<vf4,1,1,1,1>(consts1), p1);
	vf4 xl2	= mul(xl, xl);
	vf4 xl3	= mul(xl2, xl);

	// Compute both the sin and cos of the angles using a polynomial expression:
	//	cx = 1.0f + x2 * (C0 * x2 + C1), and
	//	sx = xl + x3 * S0
	vf4 ct2 = madd(swizzle<vf4,1,1,1,1>(consts2), xl2, swizzle<vf4,2,2,2,2>(consts2));
	vf4 cx = madd<vf4>(ct2 ,xl2, one);
	vf4 sx = madd(swizzle<vf4,3,3,3,3>(consts2), xl3, xl);

	// For odd numbered quadrants return -cx/sx , otherwise return sx/cx
	vb4 mask = viszero(vand(q, vloadu<1>()));
	return	div(vsel(neg(cx), sx, mask), vsel(sx, cx, mask));
}

vf4 asin(param(vf4) x) {
	static const float4_const	consts1 = {-5.54846723f, -0.504400557f, 0.933933258f, 5.603603363f};

	vf4	x1 = abs(x);

	//	if (x1 > 0.5)
	//		g = 0.5 - 0.5*x1
	//		x1 = -2 * sqrtf(g)
	//	else
	//		g = x1 * x1
	auto	gt_half	= vcmp<greater>(x1, half);
	vf4 g		= vsel(mul(x1, x1), nmsub<vf4>(x1, half, half), gt_half);
	x1			= vsel(x1, mul(vload<vf4>(-2.0f), sqrt(g)), gt_half);

	// Compute the polynomials and take their ratio
	//	denom = (1.0f*g + -0.554846723e+1f)*g + 5.603603363f
	//	num = x1 * g * (-0.504400557f * g + 0.933933258f)
	vf4 denom	= add(g, swizzle<vf4,0,0,0,0>(consts1));
	vf4 num		= madd(swizzle<vf4,1,1,1,1>(consts1), g, swizzle<vf4,2,2,2,2>(consts1));
	denom		= madd(denom, g, swizzle<vf4,3,3,3,3>(consts1));
	num			= mul(mul(x1, g), num);

	// x1 = x1 + num / denom
	x1 = add(x1, div(num, denom));

	//	if (x1 > 0.5)
	//		x1 = x1 + M_PI_2
	x1 = vsel(x1, add(x1, vload<vf4>(1.57079632679489661923f)), gt_half);

	// if (!positive) x1 = -x1
	return vsel(neg(x1), x1, vcmp<greater>(x1, zero));
}

vf4 acos(param(vf4) x) {
	static const float4_const	consts1 = {-0.0012624911f, 0.0066700901f, -0.0170881256f, 0.0308918810f};
	static const float4_const	consts2 = {-0.0501743046f, 0.0889789874f, -0.2145988016f, 1.5707963050f};
	vf4	xabs	= abs(x);
	vf4	t1		= sqrt(sub(one, xabs));

	vf4 hi, lo;
	hi = madd(swizzle<vf4,0,0,0,0>(consts1), xabs, swizzle<vf4,1,1,1,1>(consts1));
	hi = madd(hi, xabs, swizzle<vf4,2,2,2,2>(consts1));
	hi = madd(hi, xabs, swizzle<vf4,3,3,3,3>(consts1));
	lo = madd(swizzle<vf4,0,0,0,0>(consts2), xabs, swizzle<vf4,1,1,1,1>(consts2));
	lo = madd(lo, xabs, swizzle<vf4,2,2,2,2>(consts2));
	lo = madd(lo, xabs, swizzle<vf4,3,3,3,3>(consts2));

	vf4	xabs2	= mul(xabs, xabs);
	vf4	result	= madd(hi, mul(xabs2, xabs2), lo);

	return vsel(nmsub(t1, result, vload<vf4>(3.1415926535898f)), mul(t1, result), vcmp<greater>(x, zero));
}

force_inline vf4 _atan(param(vf4) x) {
	static const float4_const	consts1 = {0.0028662257f, -0.0161657367f, 0.0429096138f, -0.0752896400f};
	static const float4_const	consts2 = {0.1065626393f, -0.1420889944f, 0.1999355085f, -0.3333314528f};

	vf4 hi, lo;
	vf4 x2 = mul(x,  x);
	vf4 x3 = mul(x2, x);
	vf4 x4 = mul(x2, x2);
	vf4 x8 = mul(x4, x4);
	vf4 x9 = mul(x8, x);
	hi = madd(swizzle<vf4,0,0,0,0>(consts1), x2,swizzle<vf4,1,1,1,1>(consts1));
	hi = madd(hi, x2, swizzle<vf4,2,2,2,2>(consts1));
	hi = madd(hi, x2, swizzle<vf4,3,3,3,3>(consts1));
	hi = madd(hi, x2, swizzle<vf4,0,0,0,0>(consts2));
	lo = madd(swizzle<vf4,1,1,1,1>(consts2), x2, swizzle<vf4,2,2,2,2>(consts2));
	lo = madd(lo, x2, swizzle<vf4,3,3,3,3>(consts2));
	lo = madd(lo, x3, x);

	return madd(hi, x9, lo);
}

vf4 atan(param(vf4) x) {
	auto	select	= vcmp<greater>(abs(x), one);
	vf4	bias	= vsel(force_cast<vf4>(vand(vload<vf4>(1.57079632679489661923f), select)), x, (vf4)sign_mask);
	return add(_atan(vsel(x, neg(reciprocal(x)), select)), bias);
}

vf4 atan2(param(vf4) y, param(vf4) x) {
	auto	select	= vcmp<greater>(abs(y), abs(x));
	vf4	bias	= vsel(vsel(
		force_cast<vf4>(vand(vload<vf4>(3.14159265358979323846f), vcmp<less>(x, zero))),
		vload<vf4>(1.57079632679489661923f),
		select
	), y, (vf4)sign_mask);
	vf4 res		= add(_atan(div(vsel(y, x, select), vsel(x, neg(y), select))), bias);
	return vsel(res, (vf4)zero, vcmp<equal_to>(zero, add(abs(x), abs(y))));
}

//-----------------------------------------------------------------------------
// quaternion
//-----------------------------------------------------------------------------

#if 1
bool diagonalise_step(param(float3x3) A, quaternion &q) {
	float3x3	Q = q;
	float3x3	D = transpose(Q) * A * Q;			// A = Q^T*D*Q
	float3		od(D.z.y, D.z.x, D.y.x);			// elements not on the diagonal
	int			k = max_component_index(abs(od)); 	// find k - the index of largest element of offdiag
	float1		x = od[k];
	if (x == zero)
		return true; // diagonal already

	int		k1		= (k + 1) % 3;
	int		k2		= (k + 2) % 3;
	float1 theta	= (perm<0>(D[k2] << k2) - perm<0>(D[k1] << k1)) * half / x;
	// let t = 1 / (|T|+sqrt(T^2+1)) but avoid numerical overflow
	float1 t		= abs(theta);
	t				= reciprocal(t + ((t < 1.e6f) ? sqrt(square(t) + one) : t));
	float1 c		= rsqrt(square(t) + one); // c= 1/(t^2+1) , t=s/c

	if (c == one)
		return true; // no room for improvement - reached machine precision.

	// using 1/2 angle identities:	sin(a/2) = sqrt((1-cos(a))/2), cos(a/2) = sqrt((1-cos(a))/2)
	float2	cs = sqrt(float2(one + c, one - c) * half);
	q = q * quaternion(float3(copysign(cs.y, theta), float2(zero)) << k, cs.x);
	return false;
}
quaternion diagonalise(param(float3x3) A) {
	quaternion q	= identity;
	for (int i = 0; i < 24 && diagonalise_step(A, q); i++);
	return q;
}
quaternion diagonalise(param(symmetrical3) A) {
	quaternion q	=	identity;
	for (int i = 0; i < 24; i++) {
		float3x3	Q = q;
		float3x3	D = transpose(Q) * (A * Q);			// A = Q^T*D*Q
		float3		od(D.z.y, D.z.x, D.y.x);			// elements not on the diagonal
		int			k = max_component_index(abs(od)); 	// find k - the index of largest element of offdiag
		float1		x = od[k];
		if (x == zero)
			break; // diagonal already

		int		k1		= (k + 1) % 3;
		int		k2		= (k + 2) % 3;
		float1 theta	= (perm<0>(D[k2] << k2) - perm<0>(D[k1] << k1)) * half / x;
		// let t = 1 / (|T|+sqrt(T^2+1)) but avoid numerical overflow
		float1 t		= abs(theta);
		t				= reciprocal(t + ((t < 1.e6f) ? sqrt(square(t) + one) : t));
		float1 c		= rsqrt(square(t) + one); // c= 1/(t^2+1) , t=s/c

		if (c == one)
			break; // no room for improvement - reached machine precision.

		// using 1/2 angle identities:	sin(a/2) = sqrt((1-cos(a))/2), cos(a/2) = sqrt((1-cos(a))/2)
		float2	cs = sqrt(float2(one + c, one - c) * half);
		q = q * quaternion(float3(copysign(cs.y, theta), float2(zero)) << k, cs.x);
	}
	return q;
}
#endif

//-----------------------------------------------------------------------------
// matrix
//-----------------------------------------------------------------------------

#ifndef VEC_INLINE_INVERSE
float3x3 cofactors(const float3x3 &m) {
	return float3x3(
		cross(m.y, m.z),
		cross(m.z, m.x),
		cross(m.x, m.y)
	);
}

float3x3 get_inverse_transpose(const float3x3 &m) {
	float3x3	c = cofactors(m);
	return c * reciprocal(dot(m.x, c.x));
}

float4x4 cofactors(const float4x4 &m) {
	float3	c0 = cross(m.z.xyz, m.w.xyz);
	float3	c1 = m.z.xyz * m.w.w - m.w.xyz * m.z.w;
	float4	dx = float4(cross(m.y.xyz, c1) + c0 * m.y.w, -dot(c0, m.y.xyz));
	float4	dy = float4(cross(c1, m.x.xyz) - c0 * m.x.w,  dot(c0, m.x.xyz));

	float3	c2 = cross(m.x.xyz, m.y.xyz);
	float3	c3 = m.x.xyz * m.y.w - m.y.xyz * m.x.w;
	float4	dz = float4(cross(m.w.xyz, c3) + c2 * m.w.w, -dot(c2, m.w.xyz));
	float4	dw = float4(cross(c3, m.z.xyz) - c2 * m.z.w,  dot(c2, m.z.xyz));

	return float4x4(dx, dy, dz, dw);
}

float4x4	get_inverse_transpose(const float4x4 &m) {
	float4x4	c = cofactors(m);
	return c * reciprocal(dot(m.x, c.x));
}
#endif

template<> normalised_polynomial<2> inline operator%(const normalised_polynomial<4> &num, const normalised_polynomial<2> &div) {
	float4	rem = num.xyzw;
	rem = rem - float3(div.xy, zero).xyzz * rem.w;
	rem = rem - float3(div.xy, zero).zxyz * rem.z;
	rem = rem - float3(div.xy, zero).xyzz * rem.y;
	return rem.xy;
}

template<> normalised_polynomial<3> inline operator%(const normalised_polynomial<4> &num, const normalised_polynomial<3> &div) {
	float4	rem = num.xyzw;
	rem = rem - float4(div.xyz, zero).xyzw * rem.w;
	rem = rem - float4(div.xyz, zero).wxyz * rem.z;
	return rem.xyz;
}

float2 poly_div1(param(float4) num, param(float2) div) {
	float4	rem = num;
	rem = rem - float3(div, zero).xyzz * rem.w;
	rem = rem - float3(div, zero).zxyz * rem.z;
	rem = rem - float3(div, zero).xyzz * rem.y;
	return rem.xy;
}

float3 poly_div1(param(float4) num, param(float3) div) {
	float4	rem = num;
	rem = rem - float4(div, zero).xyzw * rem.w;
	rem = rem - float4(div, zero).wxyz * rem.z;
	return rem.xyz;
}

float2 poly_div(int power, float val, const normalised_polynomial<2> &div) {
	if (!power--)
		return float2(val, 0);
	float2	num(0, val);
	while (power--)
		num = float2(zero, num.x) - div.xy * num.y;
	return num;
}

float3 poly_div(int power, float val, const normalised_polynomial<3> &div) {
	if (!power--)
		return float3(val, 0, 0);
	if (!power--)
		return float3(0, val, 0);
	float3	num(0, 0, val);
	while (power--)
		num = float3(zero, num.xy) - div.xyz * num.z;
	return num;
}

float4 poly_div(int power, float val, const normalised_polynomial<4> &div) {
	if (!power--)
		return float4(val, 0, 0, 0);
	if (!power--)
		return float4(0, val, 0, 0);
	if (!power--)
		return float4(0, 0, val, 0);
	float4	num(0, 0, 0, val);
	while (power--)
		num = float4(zero, num.xyz) - div.xyzw * num.w;
	return num;
}

template<int N> vec<float, N> poly_div(int power, float val, const polynomial<N> &div) {
	return poly_div(power, val, normalise(div));
}

//-----------------------------------------------------------------------------
//	log_rotation
//-----------------------------------------------------------------------------

log_rotation log(const float3x3 &m) {
	float	cosang	= (m.trace() - 1) * half;
	float3	r		= float3(m.z.y - m.y.z, m.x.z - m.z.x, m.y.x - m.x.y) * half;
	float	sinang	= len(r);

	if (cosang > rsqrt2)
		return sinang > zero ? r * (asin(sinang) / sinang * half) : float3(zero);

	if (cosang > -rsqrt2)
		return r * (acos(cosang) / sinang * half);

	float3	d		= m.diagonal() - cosang;
	float3	t		= (float3(m.x.y, m.y.z, m.z.x) + float3(m.y.x, m.z.y, m.x.z)) * half;
	float3	r2;
	switch (max_component_index(d * d)) {
		case 0:		r2 = float3(d.x, t.x, t.z);
		case 1:		r2 = float3(t.x, d.y, t.y);
		default:	r2 = float3(t.z, t.y, d.z);
	}

	float1	angle	= pi - asin(sinang);
	if (dot(r2, r) < zero)
		angle = -angle;

	return normalise(r2) * (angle * half);
}
#if 0
//rodrigues formula
log_rotation::operator float3x3() const {
	float1	theta	= len(v);
	float3	w		= v / theta;
	float3	w2		= w * w;
	float2	sc		= sincos(theta);

	float3	d		= float3(one) - (w2.yzx + w2.zxy) * (one - sc.x);
	float3	a		= w * sc.y;
	float3	b		= w.xyz * w.yzx * (one - sc.x);

	return float3x3(
		float3(d.x, b.x - a.z, b.z + a.y),
		float3(b.x + a.z, d.y, b.y - a.x),
		float3(b.z - a.y, b.y + a.x, d.z)
	);
}
#endif

//-----------------------------------------------------------------------------
//	other rotation stuff
//-----------------------------------------------------------------------------

symmetrical3 rotation_pi(param(float3) v) {
	float3	w	= normalise(v) * sqrt2;
	float3	w2	= w * w;
	return symmetrical3(float3(one) - (w2.yzx + w2.zxy), w.xyz * w.yzx);
}

#if 0
struct TestRot {
	log_rotation	ra, rb, rc;
	float3x3	ma, mb, mc;
	TestRot() {
		float3	a(1,2,3);
		float3	b(3,2,1);
		ra	= log_rotation::between(a, b);
		ma	= ra;

		mb	= float3x3::between(a, b);
		rb	= mb;

		mc	= quaternion::between(a, b);
	}
} testrot;
#endif

//-----------------------------------------------------------------------------
//	Matrix creators
//-----------------------------------------------------------------------------

float2x3 fov_matrix(param(float4) fov) {
	return translate((fov.zw - fov.xy) * half) * scale((fov.xy + fov.zw) * half);
}

float4x4 perspective_projection_offset(param(float4) p, float nearz, float farz) {
	return float4x4(
		perm<1,0,0,0>(zero, p.x),
		perm<0,1,0,0>(zero, p.y),
		float4(p.zw, (farz + nearz) / (farz - nearz), one),
		perm<0,0,1,0>(zero, -2 * farz * nearz / (farz - nearz))
	);
}
float4x4 perspective_projection_offset(param(float4) p, float nearz) {
	return float4x4(
		perm<1,0,0,0>(zero, p.x),
		perm<0,1,0,0>(zero, p.y),
		float4(p.zw, float2(one)),
		perm<0,0,1,0>(zero, -2 * nearz)
	);
}

float4x4 perspective_projection(param(float2) sxy, float nearz, float farz) {
	return perspective_projection_offset(float4(sxy, float2(zero)), nearz, farz);
}
float4x4 perspective_projection(param(float2) sxy, float nearz) {
	return perspective_projection_offset(float4(sxy, float2(zero)), nearz);
}

float4x4 perspective_projection(float sx, float sy, float nearz, float farz) {
	return perspective_projection(float2(sx, sy), nearz, farz);
}
float4x4 perspective_projection(float sx, float sy, float nearz) {
	return perspective_projection(float2(sx, sy), nearz);
}
float4x4 perspective_projection_angle(float theta, float aspect, float nearz, float farz) {
	return perspective_projection(float2(1, aspect) / tan(theta * half), nearz, farz);
}
float4x4 perspective_projection_angle(float theta, float aspect, float nearz) {
	return perspective_projection(float2(1, aspect) / tan(theta * half), nearz);
}

float4x4 perspective_projection_offset(float sx, float sy, float ox, float oy, float nearz, float farz) {
	return perspective_projection_offset(float4(sx, sy, ox, oy), nearz, farz);
}
float4x4 perspective_projection_offset(float sx, float sy, float ox, float oy, float nearz) {
	return perspective_projection_offset(float4(sx, sy, ox, oy), nearz);
}

float4x4 perspective_projection_rect(float left, float right, float bottom, float top, float nearz, float farz) {
	return perspective_projection_offset(
		float4(2 * nearz,		2 * nearz,		right + left,	top + bottom)
	/	float4(right - left,	top - bottom,	left - right,	bottom - top),
		nearz, farz
	);
}
float4x4 perspective_projection_rect(float left, float right, float bottom, float top, float nearz) {
	return perspective_projection_offset(
		float4(2 * nearz,		2 * nearz,		right + left,	top + bottom)
	/	float4(right - left,	top - bottom,	left - right,	bottom - top),
		nearz
	);
}
float4x4 perspective_projection_fov(float left, float right, float bottom, float top, float nearz, float farz) {
	return perspective_projection_offset(
		float4(2,				2,				left - right,	bottom - top)
	/	float4(right + left,	top + bottom,	left + right,	bottom + top),
		nearz, farz
	);
}
float4x4 perspective_projection_fov(float left, float right, float bottom, float top, float nearz) {
	return perspective_projection_offset(
		float4(2,				2,				left - right,	bottom - top)
	/	float4(right + left,	top + bottom,	left + right,	bottom + top),
		nearz
	);
}
float4x4 perspective_projection_fov(param(float4) fov, float nearz, float farz) {
	return perspective_projection_offset(float4(2.f, 2.f, fov.xy - fov.zw) / perm<0,1,0,1>(fov.xy + fov.zw), nearz, farz);
}
float4x4 perspective_projection_fov(param(float4) fov, float nearz) {
	return perspective_projection_offset(float4(2.f, 2.f, fov.xy - fov.zw) / perm<0,1,0,1>(fov.xy + fov.zw), nearz);
}

float4x4 parallel_projection(float sx, float sy, float nearz, float farz) {
	return float4x4(
		perm<1,0,0,0>(zero, sx),
		perm<0,1,0,0>(zero, sy),
		perm<0,0,1,0>(zero, 2 / (farz - nearz)),
		perm<0,0,1,2>(zero, -(farz + nearz) / (farz - nearz), one)
	);
}
float4x4 set_perspective_z(param(float4x4) proj, float new_nearz, float new_farz) {
	float zn = project(float4(proj.z * new_nearz + proj.w)).z;
	float zf = project(float4(proj.z * new_farz  + proj.w)).z;
	return parallel_projection(1, 1, zn, zf) * proj;
}
float4x4	parallel_projection_rect(param(float3) xyz0, param(float3) xyz1) {
	float3	d = xyz1 - xyz0;
	float4	r2(2 / d, zero);
	if (d.z == zero) {
		r2.z	= -one;
		return float4x4(r2.xwww, r2.wyww, r2.wwww, float4((xyz0 + xyz1) * r2.xyz * -half, one));
	}
	return float4x4(r2.xwww, r2.wyww, r2.wwzw, float4((xyz0 + xyz1) * r2.xyz * -half, one));
}
float4x4 parallel_projection_rect(param(float2) xy0, param(float2) xy1, float z0, float z1) {
	return parallel_projection_rect(float3(xy0, z0), float3(xy1, z1));
}
float4x4 parallel_projection_rect(float x0, float x1, float y0, float y1, float z0, float z1) {
	return parallel_projection_rect(float3(x0, y0, z0), float3(x1, y1, z1));
}
float4x4 parallel_projection_fov(param(float4) fov, float scale, float z0, float z1) {
	return parallel_projection_rect(float3(fov.xy * -scale, z0), float3(fov.zw * scale, z1));
}
float4x4 projection_set_farz(param(float4x4) m, float z) {
	return	translate(float3(zero, zero, -one))
		*	scale(float3(one, one, (z + 1) / 2))
		*	m
		*	translate(float3(zero, zero, one));
}

float calc_projected_z(param(float4x4) m, float z) {
	float4x4	mt	= transpose(m);
	float4		zr	= mt.z;
	float		a	= len(zr.xyz);
	float		b	= m.w.z - a * m.w.w;
	return (z * a + b) / z;
}

float3x4 stereo_skew(float offset, float focus, float shift) {
	float3x4	skew		= identity;
	skew.z.x = offset / focus;
	skew.w.x = shift - offset;
	return skew;
}

float3x3 look_along_x(param(float3) dir) {
	float3	x	= normalise(dir);
	float3	y	= normalise(perp(x));
	return float3x3(x, y, cross(x, y));
}
float3x3 look_along_y(param(float3) dir) {
	float3	y	= normalise(dir);
	float3	z	= normalise(perp(y));
	return float3x3(cross(y, z), y, z);
}
float3x3 look_along_z(param(float3) dir) {
#if 0
	float3	z	= normalise(dir);
//	float1	t	= z.z + one;
//	return quaternion(float4(z.y, -z.x, zero, t) / t * half);
	return quaternion::between(float3(0,0,1), z);
//	float3	z	= normalise(dir);
//	float3	x	= normalise((abs(z.y) < 0.99f).select((z.z, zero, -z.x), (-z.y, z.x, zero)));
//	return float3x3(x, cross(z, x), z);
#else
	float3	z	= normalise(dir);
	float3	x	= normalise(perp(z));
	return float3x3(x, cross(z, x), z);
#endif
}

float3x4 look_at(const float3x4 &at, param(float3) pos) {
	float3		t = at.translation();
	float3		d = normalise(pos - t);
	if (abs(dot(d, at.y)) < 0.98f) {
		float3	x = normalise(cross(at.y, d));
		return float3x4(x, cross(d, x), d, t);
	}
	float3	y = normalise(cross(d, at.x));
	return float3x4(cross(y, d), y, d, t);
}

float4x4 find_projection(float4 in[5], float4 out[5]) {
	float4x4	matC(out[0], out[1], out[2], out[3]);
	float4x4	matP( in[0],  in[1],  in[2],  in[3]);

	float4x4	invC = inverse(matC);
	float4x4	invP = inverse(matP);
	float4		v	= (invC * out[4]) / (invP * in[4]);

	matC.x *= v.x;
	matC.y *= v.y;
	matC.z *= v.z;
	matC.w *= v.w;
	return matC * invP;
}

float3x3 find_projection(float3 in[4], float3 out[4]) {
	float3x3	matC(out[0], out[1], out[2]);
	float3x3	matP( in[0],  in[1],  in[2]);

	float3x3	invC = inverse(matC);
	float3x3	invP = inverse(matP);
	float3		v	= (invC * out[3]) / (invP * in[3]);

	matC.x *= v.x;
	matC.y *= v.y;
	matC.z *= v.z;
	return matC * invP;
}

//-----------------------------------------------------------------------------
//	Matrix dcomposition
//-----------------------------------------------------------------------------

void QR(const float2x2 &A, float2x2 &Q, float2x2 &R) {
	float2	u1	= A.x;
	float2	u2	= A.y - project(A.y, u1);

	Q.x = normalise(u1);
	Q.y = normalise(u2);

	R = transpose(Q) * A;
}

void QR(const float3x3 &A, float3x3 &Q, float3x3 &R) {
	float3	u1	= A.x;
	float3	u2	= A.y - project(A.y, u1);
	float3	u3	= A.z - project(A.z, u1) - project(A.z, u2);

	Q.x = normalise(u1);
	Q.y = normalise(u2);
	Q.z = normalise(u3);

	R = transpose(Q) * A;
}

void QR(const float4x4 &A, float4x4 &Q, float4x4 &R) {
	float4	u1	= A.x;
	float4	u2	= A.y - project(A.y, u1);
	float4	u3	= A.z - project(A.z, u1) - project(A.z, u2);
	float4	u4	= A.w - project(A.w, u1) - project(A.w, u2) - project(A.w, u3);

	Q.x = normalise(u1);
	Q.y = normalise(u2);
	Q.z = normalise(u3);
	Q.w = normalise(u4);

	R = transpose(Q) * A;
}

//-----------------------------------------------------------------------------
// polynomials
//-----------------------------------------------------------------------------

// order 1
// x + coeffs.x = 0
template<> int normalised_polynomial<1>::roots(float1 &r) const {
	r = -x;
	return 1;
}
// coeffs.y * x + coeffs.x = 0
template<> int polynomial<1>::roots(float1 &r) const {
	if (y == zero)
		return 0;
	r = -x / y;
	return 1;
}

// order 2
// x^2 + coeffs.y * x + coeffs.x = 0
template<> int normalised_polynomial<2>::roots(float2 &r) const {
	// discriminant
	float1 e = y * half;
	float1 d = square(e) - x;
	if (d > zero) {
		r = float2(-one, +one) * sqrt(d) - e;
		return 2;

	} else if (d == zero) {
		r = -e;
		return 1;
	}
	r = float2(-e, sqrt(-d));
	return -2;
}

// coeffs.z * x^2 + coeffs.y * x + coeffs.x = 0
template<> int polynomial<2>::roots(float2 &r) const {
	if (abs(z) < 1e-6f)
		return polynomial<1>(xy).roots((float1&)r);
	return normalise(*this).roots(r);
}

// order 3
float upper_bound_lagrange(const normalised_polynomial<3> &f) {
	float3	bounds1 = pow(max(-f.xyz, zero), float3(1 / 3.f, 1 / 2.f, 1));
	float3	sorted	= sort(bounds1);
	return	sorted.z + sorted.y;
}

// x^3 + coeffs.z * x^2 + coeffs.y * x + coeffs.x = 0
template<> int normalised_polynomial<3>::roots(float3 &r) const {
	static const float4_const	cv1		= {2,	-1,		-1,		1};
	static const float4_const	cv2		= {0, -sqrt3, +sqrt3,	0};
	static const float4_const	cv3		= {1, -half, +sqrt3_2,	0};
	static const float4_const	cv4		= {1, -half, -sqrt3_2,	0};

	float	b	= z, c = y, d = x;

	float	e	= b / 3;
	float	f	= square(e) - c / 3;
	float	g	= (e * c - d) / 2 - cube(e);
	float	h	= square(g) - cube(f);

	if (h < 0) {
		//3 real roots
		float2	t	= sincos(atan2(copysign(sqrt(-h), g), g) / 3);
		r = (float3(cv1) * t.x + float3(cv2) * t.y) * sqrt(f) - e;
		return 3;
	} else if (h > 0) {
		//1 real root, 2 imaginary (y + iz) & (y - iz)
		float2	t = pow(float4(cv1).wz * sqrt(h) + g, third);
		r = float3(cv3) * t.x + float3(cv4) * t.y - float3(float2(e), 0);
		return -1;
	} else {
		//3 real and equal
		r = float3(pow(-d, third));
		return 1;
	}
}
// coeffs.w * x ^ 3 + coeffs.z * x^2 + coeffs.y * x + coeffs.x = 0
template<> int polynomial<3>::roots(float3 &r) const {
	if (abs(w) < 1e-6f)
		return polynomial<2>(xyz).roots((float2&)r);
	return normalise(*this).roots(r);
}

// order 4

// x^4 + coeffs.w * x^3 + coeffs.z * x^2 + coeffs.y * x + coeffs.x = 0
template<> int normalised_polynomial<4>::roots(float4 &roots) const {
	float a = w;
	float b = z;
	float c = y;
	float d = x;

	//  substitute x = y - A/4 to eliminate cubic term:
	//	x^4 + px^2 + qx + r = 0

	float a2	= a * a;
	float p		= b - 3 * a2 / 8;
	float q		= a2 * a / 8 - a * b * half + c;
	float r		= -3 * a2 * a2 / 256 + a2 * b / 16 - a * c / 4 + d;

	if (abs(r) < epsilon) {
		// no absolute term: y(y^3 + py + q) = 0
		float3	s3;
		int		num3	= normalised_polynomial<3>(float3(q, p, 0)).roots(s3);
		roots			= float4(s3 - a / 4, zero).wxyz;
		return num3 < 0 ? 2 : 4;
	}

	// solve the resolvent cubic ...

	float3	s3;
	normalised_polynomial<3>(float3(r * p * half - q * q / 8, -r, -p * half)).roots(s3);

	// ... and take the one real solution ...
	float z = s3.x;

	// ... to build two quadratic equations
	float u = z * z - r;
	float v = z * two - p;

	if (-min(u, v) > epsilon)
		return 0;

	u = sqrt(max(u, zero));
	v = copysign(sqrt(max(v, zero)), q);

	float2	s2a, s2b;
	int	num_a = normalised_polynomial<2>(float2(z - u,  v)).roots(s2a);
	int	num_b = normalised_polynomial<2>(float2(z + u, -v)).roots(s2b);

	if (num_a < 0 && num_b < 0) {
		return 0;

	} else if (num_a > 0 && num_b > 0) {
		float4	s	= float4(s2a, s2b) - a / 4;
		//single halley iteration to fix cancellation
		roots = halley(*this, s);
		return 4;

	} else {
		float2	s = (num_a > 0 ? s2a : s2b) - a / 4;
		//single halley iteration to fix cancellation
		roots = float4(halley(*this, s), float2(zero));
		return 2;
	}
}

// coeffs.w * x^4 + coeffs.z * x^3 + coeffs.y * x^2 + coeffs.x * x + c0 = 0
int polynomial<4>::roots(float4 &r) const {
	if (abs(w) < 1e-6f)
		return abs(polynomial<3>(float4(c0, xyz)).roots((float3&)r));
	return normalise(*this).roots(r);
}

// order 5

float upper_bound_lagrange(const normalised_polynomial<5> &f) {
	float	bounds0 = pow(max(-f.c0, zero), 1 / 5.f);
	float4	bounds1 = pow(max(-f.xyzw, zero), float4(1 / 4.f, 1 / 3.f, 1 / 2.f, 1));
	float4	sorted	= sort(bounds1);
	return	max(sorted.w, bounds0) + max(min(sorted.w, bounds0), sorted.z);
}

// lagrange upper bound applied to f(-x) to get lower bound
float lower_bound_lagrange(const normalised_polynomial<5> &f) {
	return -upper_bound_lagrange(normalised_polynomial<5>(f.c0, f.xyzw * float4(-1, 1, -1, 1)));
}

// x^5 + coeffs.w * x^4 + coeffs.z * x^3 + coeffs.y * x^2 + coeffs.x * x + c0 = 0
int normalised_polynomial<5>::roots(float4 &roots) const {
	float4	roots1;
	auto	c1			= normalised_deriv(*this);
	int		num_roots1	= c1.roots(roots1);

	float	ub			= upper_bound_lagrange(*this);
	float	lb			= lower_bound_lagrange(*this);

	int		num_roots	= 0;
	float4	roots0;

	//compute root isolating intervals by roots of derivative and outer root bounds
	//only roots going from - to + considered, because only those result in a minimum

	if (num_roots1 == 4) {
		float3	a(zero), b(zero);
		roots1	= sort(roots1);

		if (eval(roots1.w) < zero) {
			a = float3(roots1.w, a.xy);
			b = float3(ub, b.xy);
			++num_roots;
		}

		if (sign(eval(roots1.y)) != sign(eval(roots1.z))) {
			a = float3(roots1.y, a.xy);
			b = float3(roots1.z, b.xy);
			++num_roots;
		}
		if (eval(roots1.x) > zero) {
			a = float3(lb, a.xy);
			b = float3(roots1.x, b.xy);
			++num_roots;
		}
		roots0 = float4((a + b) * half, zero);

	} else {
		float2	a, b;

		if (num_roots1 == 2) {
			roots1.xy	= sort(roots1.xy);
			if (eval(roots1.x) < zero) {
				a = float2(roots1.y, zero);
				b = float2(ub, zero);
				num_roots = 1;
			} else if (eval(roots1.y) > zero) {
				a = float2(lb, zero);
				b = float2(roots1.x, zero);
				num_roots = 1;
			} else {
				a = float2(lb, roots1.y);
				b = float2(roots1.x, ub);
				num_roots = 2;
			}

		} else {
			//num_roots1 == 0
			a = float2(lb, zero);
			b = float2(ub, zero);
			num_roots = 1;
		}

		//further subdivide intervals to guarantee convergence of halley's method by using roots of further derivatives
		float3	roots2;
		auto	c2			= normalised_deriv(c1);
		int		num_roots2	= abs(c2.roots(roots2));
		if (num_roots2 == 3)
			roots2 = sort(roots2);

		for (int i = 0; i < num_roots; i++) {
			for (int j = 0; j < num_roots2; j += 2) {
				float	r = roots2[j];
				if (between(r, a[i], b[i])) {
					if (eval(r) > zero)
						b[i] = r;
					else
						a[i] = r;
				}
			}
		}

		if (num_roots2 != 3) {
			float2	roots3;
			auto	c3			= normalised_deriv(c2);
			int		num_roots3	= c3.roots(roots3);

			for (int i = 0; i < num_roots; i++) {
				for (int j = 0; j < num_roots3; j++) {
					float	r = roots3[j];
					if (between(r, a[i], b[i])) {
						if (eval(r) > zero)
							b[i] = r;
						else
							a[i] = r;
					}
				}
			}
		}
		roots0 = float4((a + b) * half, zero, zero);
	}

	//8 halley iterations
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);
	roots0 = halley(*this, roots0);

	roots = roots0;
	return num_roots;
}

// coeffs.w * x^5 + coeffs.z * x^4 + coeffs.y * x^3 + coeffs.x * x^2 + c1 * x + c0 = 0
int polynomial<5>::roots(float4 &r) const {
	if (abs(w) < 1e-6f)
		return polynomial<4>(c0, float4(c1, xyz)).roots(r);
	return normalised_polynomial<5>(c0 / w, float4(c1, xyz) / w).roots(r);
}

}	// namespace iso
#endif