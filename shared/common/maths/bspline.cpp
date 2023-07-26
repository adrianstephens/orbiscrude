#include "bspline.h"

using namespace iso;

float4x4 NURBS3GetKnotMatrix(float *k) {
	float	k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5];

	float	d0	= (k0 - k3) * (k1 - k3) * (k2 - k3);
	float	d1	= d0 * (k1 - k4) * (k2 - k4);
	float	d3	= (k2 - k3) * (k2 - k4) * (k2 - k5);
	float	d2	= d3 * (k1 - k3) * (k1 - k4);
	float4	d	= reciprocal(float4{d0, d1, d2, d3});

	//			1				t				t^2			t^3
	float4	n0{	-k3 * k3 * k3,	 3 * k3 * k3,	-3 * k3,	 1};
	float4	n3{	 k2 * k2 * k2,	-3 * k2 * k2,	 3 * k2,	-1};

	//from mathics
	//w1,1:		+01233+01234+01244-01334-01344-02334-02344+03344-12334-12344+13344+23344
	//w1,t:		-3(0123+0124-0134-0234-1234+3344)
	//w1,t2:	+3(012-034-134-234+334+344)
	//w1,t3:	-01-02+03+04-12+13+14+23+24-33-34-44
	//manually factored:
	//w1,1:		+012(33+44+34)+34(+34(0+1+2)-(3+4)(01+02+12))
	//w1,t:	-3(	+012(3+4)+34(34-(01+02+12))	)
	//w1,t2:+3(	+012-34(0+1+2-3-4)			)
	//w1,t3:	+(3+4-0)(1+2-3-4)-12+34

	//from mathics
	//w2,1:		-11223-11224-11225+11234+11235+11245-11345+12234+12235+12245-12345-22345
	//w2,t:		+3(1122-1234-1235-1245+1345+2345)
	//w2,t2:	-3(112+122-123-124-125+345)
	//w2,t3:	+11+12-13-14-15+22-23-24-25+34+35+45
	//manually factored:
	//w2,1:		-345(11+22+12)+12(-12(3+4+5)+(1+2)(+34+35+45))
	//w2,t:	+3(	+345(1+2)+12(12-34-35-45)	)
	//w2,t2:-3(	+345+12(1+2-3-4-5))			)
	//w2,t3:	+(1+2-5)(1+2-3-4)-12+34

	float	k12			= k1 * k2,				k1p2		= k1 + k2,			k012	= k0 * k12;
	float	k34			= k3 * k4,				k3p4		= k3 + k4,			k345	= k34 * k5;
	float	k01p02p12	= k0 * k1p2 + k12,		k34p35p45	= k34 + k3p4 * k5;
	float	k1p2m3m4	= k1p2 - k3p4;

	float4	n1{
		+ k012 * (k3 * k3 + k4 * k4 + k34) + k34 * (+k34 * (k0 + k1p2) - k3p4 * k01p02p12)
		-3 * (k012 * k3p4 + k34 * (k34 - k01p02p12)),
		+3 * (k012 - k34 * (k0 + k1p2m3m4)),
		+ (k3p4 - k0) * k1p2m3m4 - k12 + k34,
	};

	float4	n2{
		- k345 * (k1 * k1 + k2 * k2 + k12) + k12 * (-k12 * (k3p4 + k5) + k1p2 * k34p35p45)
		+3 * (k345 * k1p2 + k12 * (k12 - k34p35p45)),
		-3 * (k345 + k12 * (k1p2m3m4 - k5)),
		+ (k1p2 - k5) * k1p2m3m4 - k12 + k34,
	};
	return float4x4(n0 * d.x, n1 * d.y, n2 * d.z, n3 * d.w);
}

float4x4 NURBS3GetControlMatrix(float4 *c) {
	return float4x4(
		c[0] * c[0].w,
		c[1] * c[1].w,
		c[2] * c[2].w,
		c[3] * c[3].w
	);
}

/*
float4 NURBS3GetWeights(float *k, float t) {
	float	k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5];

	float	l = (k3 - t) / ((k3 - k2) * (k3 - k1));
	float	r = (t - k2) / ((k3 - k2) * (k4 - k2));
	float	a = r * (t - k2) / (k5 - k2);
	float	b = (r * (k4 - t) + l * (t - k1)) / (k4 - k1);
	float	c = (l * (k3 - t)) / (k3 - k0);

	return float4{
		c * (k3 - t),
		b * (k4 - t) + c * (t - k0),
		a * (k5 - t) + b * (t - k1),
		a * (t - k2)
	};
}
iso::float3 NURBS3Evaluate(float *k, iso::float4 *cp, float t) {
	float4 w	= NURBS3GetWeights(k, t);
	float4 bw	= w * float4(cp[0].w, cp[1].w, cp[2].w, cp[3].w);
	return (cp[0].xyz * cp[0].w * b.x + cp[1].xyz * cp[1].w * b.y + cp[2].xyz * cp[2].w * b.z + cp[3].xyz * cp[3].w * b.w) / sum(bw);
}
*/

polynomial<float4,3> NURBS3Polynomial(float *k, float4 *c) {
	float4x4	km	= NURBS3GetKnotMatrix(k);
	float4x4	cm	= NURBS3GetControlMatrix(c);
	return cm * km;
}

position3 NURBS3Evaluate(float *k, int n, float4 *cp, float t) {
	while (t > k[1]) {
		cp++;
		k++;
	}
	return project(NURBS3Polynomial(k, cp)(t));
}

position3 NURBS3Evaluate2D(float *ku, int nu, float *kv, int nv, float4 *cp, float2 uv) {
	float	u = uv.x, v = uv.y;

	while (v > kv[1]) {
		cp += nu;
		kv++;
	}

	while (u > ku[1]) {
		cp++;
		ku++;
	}

	//float4		u2	= NURBS3GetKnotMatrix(ku) * cubic_param(u);
	polynomial<float4,3>	polyu = NURBS3GetKnotMatrix(ku);
	float4		u2	= polyu(u);

	float4x4	cv	= {
		NURBS3GetControlMatrix(cp + nu * 0) * u2,
		NURBS3GetControlMatrix(cp + nu * 1) * u2,
		NURBS3GetControlMatrix(cp + nu * 2) * u2,
		NURBS3GetControlMatrix(cp + nu * 3) * u2,
	};

	//	return cv * NURBS3GetKnotMatrix(kv) * cubic_param(v);
	polynomial<float4,3>	polyv = cv * NURBS3GetKnotMatrix(kv);
	return project(polyv(v));
}
