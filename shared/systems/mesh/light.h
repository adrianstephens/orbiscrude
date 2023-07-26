#ifndef LIGHT_H
#define LIGHT_H

#include "base/vector.h"
#include "extra/colour.h"

namespace iso {

struct Light {
	int				type;
	float			range;
	float			spread;
	colour			col;
	float3x4		matrix;

	Light()	{}
	Light(int _type, param(colour) col, float range, float spread = 1, param(float3x4) matrix = identity) :
		type(_type), range(range), spread(spread), col(col), matrix(matrix)
	{}
};

class SH {
	float4	 x,  y,  z;
	float4	xx, yy, zz;
	float4	xy, yz, zx;
public:
	SH()	{ clear(*this); }

	void	Clear()							{ clear(*this); }
	void	AddAmbient(param(colour) col)	{ xx += col.rgba; yy += col.rgba; zz += col.rgba;	}
	void	SetAmbient(param(colour) col)	{ AddAmbient(colour(col.rgba - GetAmbient().rgba));	}

	void	AddDir(param(float3) n, param(colour) col) {
		float3	n2 = n * n;
		float3	n3 = n * rotate(n);
		x	+= col.rgba * n.x	* (32.f / 64);
		y	+= col.rgba * n.y	* (32.f / 64);
		z	+= col.rgba * n.z	* (32.f / 64);
		xx	+= col.rgba * (n2.x * 30.f + 6.f)	/ 64.f;
		yy	+= col.rgba * (n2.y * 30.f + 6.f)	/ 64.f;
		zz	+= col.rgba * (n2.z * 30.f + 6.f)	/ 64.f;
		xy	+= col.rgba * n3.x	* (60 / 64.f);
		yz	+= col.rgba * n3.y	* (60 / 64.f);
		zx	+= col.rgba * n3.z	* (60 / 64.f);
	}

	void	AddFromSH(float4 s[9]) {
		x	-= s[3] * sqrt(3 / (pi * 4));
		z	-= s[1] * sqrt(3 / (pi * 4));
		y	-= s[2] * sqrt(3 / (pi * 4));
		zx	+= s[4] * sqrt(15 / (pi * 4));
		yz	+= s[5] * sqrt(15 / (pi * 4));
		xy	+= s[6] * sqrt(15 / (pi * 4));
		xx	+= s[0] * sqrt(1 / (pi * 4)) - s[6] * iso::sqrt(5 / (pi * 16)) + s[8] * iso::sqrt(15 / (pi * 16));
		zz	+= s[0] * sqrt(1 / (pi * 4)) - s[6] * iso::sqrt(5 / (pi * 16)) - s[8] * iso::sqrt(15 / (pi * 16));
		yy	+= s[0] * sqrt(1 / (pi * 4)) + s[6] * iso::sqrt(5 / (pi * 4));
	}

	void	AddFromSH(const SH &other) {
		x	+= other.x;		y	+= other.y;		z	+= other.z;
		xx	+= other.xx;	yy	+= other.yy;	zz	+= other.zz;
		xy	+= other.xy;	yz	+= other.yz;	zx	+= other.zx;
	}

	colour	GetAmbient()			const	{ return colour((xx + yy + zz) / 3.f);	}
	colour	GetAverage()			const	{ return colour((x + y + z + xx + yy + zz) / 6.f); }

	float4&	operator[](int i)				{ return *(&x + i); }
	const float4& operator[](int i) const	{ return *(&x + i); }

	void	AdjustCols(param(float3x4) m) {
		for (float4 *p = (float4*)this; p < (float4*)(this + 1); ++p)
			p->xyz = m * p->xyz;
	}
};

}//namespace iso

#endif	// LIGHT_H
