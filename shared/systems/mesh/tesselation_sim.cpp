#include "base/defs.h"
#include "base/array.h"
#include "base/maths.h"
#include "base/soft_float.h"

using namespace iso;

struct Tesselation {
	enum {MAX = 64};
	enum Spacing {EQUAL, FRACT_EVEN, FRACT_ODD};

	dynamic_array<uint16>	indices;
	dynamic_array<float2p>	uvs;

	static uint32	effective(Spacing spacing, float x) {
		switch (spacing) {
			default:
			case EQUAL:			return ceil(clamp(x, 1, MAX));
			case FRACT_EVEN:	return ceil(clamp(x, 2, MAX) / 2) * 2;
			case FRACT_ODD:		return ceil(clamp(x + 1, 2, MAX) / 2) * 2 - 1;
		}
	}
	static uint32	effective_min(Spacing spacing) {
		return spacing == FRACT_ODD ? 3 : 2;
	}
	
	static uint16	*put_tri(uint16 *p, uint32 a0) {
		p[0] = a0;
		p[1] = a0 + 1;
		p[2] = a0 + 2;
		return p + 3;
	}
	static uint16	*put_quad(uint16 *p, uint32 a0) {
		p[0] = a0;
		p[1] = p[4] = a0 + 1;
		p[2] = p[3] = a0 + 3;
		p[5] = a0 + 2;
		return p + 6;
	}
	static uint16	*put_fan(uint16 *p, uint32 a0, uint32 b0, uint32 bn) {
		while (bn--) {
			p[0] = a0;
			p[1] = b0;
			p[2] = ++b0;
			p += 3;
		}
		return p;
	}
	static uint16	*put_strip(uint16 *p, uint32 a0, uint32 an, uint32 b0, uint32 bn) {
		uint32 n = min(an, bn);
		if (n == 0)
			return p;

		for (; --n; p += 6) {
			p[0] = a0;
			p[1] = p[4] = b0;
			p[2] = p[3] = ++a0;
			p[5] = ++b0;
		}
		return	an < bn	? put_fan(p, a0, b0, bn - an + 1)
			:	bn < an	? put_fan(p, b0, a0, an - bn + 1)
			:	p;
	}

	Tesselation(const float4p &edges, const float2p &inside, Spacing spacing = EQUAL);	//quad
	Tesselation(const float3p &edges, float inside, Spacing spacing = EQUAL);			//tri
	Tesselation(const float2p &edges, Spacing spacing = EQUAL);							//isoline
};


//quad
Tesselation::Tesselation(const float4p &edges, const float2p &inside, Spacing spacing) {
	if (edges.x <= 0 || edges.y <= 0 || edges.z <= 0|| edges.w <= 0)
		return;

	uint32	ex	= effective(spacing, inside.x);
	uint32	ey	= effective(spacing, inside.y);
	uint32	e0	= effective(spacing, edges.x);
	uint32	e1	= effective(spacing, edges.y);
	uint32	e2	= effective(spacing, edges.z);
	uint32	e3	= effective(spacing, edges.w);
	if (e0 == 1 && e1 == 1 && e2 == 1 && e3 == 1) {
		if (ex == 1 || ex == 1) {
			auto	*p	= uvs.resize(4).begin();
			p[0].set(0, 0);
			p[1].set(1, 0);
			p[2].set(0, 1);
			p[2].set(1, 1);

			put_quad(indices.resize(2 * 3).begin(), 0);
			return;
		}
	}
	if (ex == 1)
		ex = effective_min(spacing);
	if (ey == 1)
		ey = effective_min(spacing);


	uint32	nuv_outer	= e0 + e1 + e2 + e3;
	uint32	nuv			= nuv_outer + (ex - 1) * (ey - 1);
	uint32	ntri		= e0 + e1 + e2 + e3 + (ex + ey - 4) * 2 + (ex - 2) * (ey - 2) * 2;

	auto	*p		= uvs.resize(nuv).begin();
	auto	*ix		= indices.resize(ntri * 3).begin();
	uint32	a		= 0, b = nuv_outer;
	float	fi;

	--ex;
	--ey;

	//outer ring
	p++->set(0, 0);
	fi = 1 - (e0 - edges.x) / 2;
	for (int i = 0; i < e0 - 1; i++)
		p++->set((i + fi) / edges.x, 0);

	ix	= put_strip(ix, a, e0, b, ex);
	a	+= e0;
	b	+= ex - 1;

	p++->set(1, 0);
	fi = 1 - (e1 - edges.y) / 2;
	for (int i = 0; i < e1 - 1; i++)
		p++->set(1, (i + fi) / edges.y);

	ix = put_strip(ix, a, e1, b, ey);
	a	+= e1;
	b	+= ey - 1;

	p++->set(1, 1);
	fi = 1 - (e2 - edges.z) / 2;
	for (int i = 0; i < e2 - 1; i++)
		p++->set(1 - (i + fi) / edges.z, 1);

	ix	= put_strip(ix, a, e2, b, ex);
	a	+= e2;
	b	+= ex - 1;

	p++->set(0, 1);
	fi = 1 - (e3 - edges.w) / 2;
	for (int i = 0; i < e3 - 1; i++)
		p++->set(0, 1 - (i + fi) / edges.w);

	ix	= put_strip(ix, a, e3, b, ey);
	a	+= e3;
	b	+= ey - 1;

	float	s = (1 - (ex - inside.x) / 2) / inside.x;
	float	t = (1 - (ey - inside.y) / 2) / inside.y;

	//inner rings
	while (ex > 1 && ey > 1) {
		for (int i = 0; i < ex - 1; i++)
			p++->set(s + i / inside.x, t);

		for (int i = 0; i < ey - 1; i++)
			p++->set(1 - s, t + i / inside.y);


		for (int i = 0; i < ex - 1; i++)
			p++->set(1 - s - i / inside.x, 1 - t);

		for (int i = 0; i < ey - 1; i++)
			p++->set(s, 1 - t - i / inside.y);

		if (ex == 2) {
			ix	= put_quad(ix, a);
		} else {
			ix	= put_strip(ix, a, ex, b, ex - 2);
			a	+= ex;
			b	+= ex - 2;

			ix	= put_strip(ix, a, ey, b, ey - 2);
			a	+= ey;
			b	+= ey - 2;

			ix	= put_strip(ix, a, ex, b, ex - 2);
			a	+= ex;
			b	+= ex - 2;

			ix	= put_strip(ix, a, ey, b, ey - 2);
			a	+= ey;
			b	+= ey - 2;
		}

		s += 1 / inside.x;
		t += 1 / inside.y;

		ex -= 2;
		ey -= 2;
	}

	if (ex == 1) {
		for (int i = 0; i < ey; i++)
			p++->set(s, t + i / inside.y);

	} else if (ey == 1) {
		for (int i = 0; i < ex; i++)
			p++->set(s + i / inside.x, t);
	}
}

//tri
Tesselation::Tesselation(const float3p &edges, float inside, Spacing spacing) {
	if (edges.x <= 0 || edges.y <= 0 || edges.z <= 0)
		return;

	uint32	ei	= effective(spacing, inside);
	uint32	e0	= effective(spacing, edges.x);
	uint32	e1	= effective(spacing, edges.y);
	uint32	e2	= effective(spacing, edges.z);
	if (e0 == 1 && e1 == 1 && e2 == 1) {
		if (ei == 1) {
			auto	*p	= uvs.resize(3).begin();
			p[0].set(0, 0);
			p[1].set(1, 0);
			p[2].set(0, 1);
			return;
		}
		ei = effective_min(spacing);
	}

	uint32	outer_ring	= e0 + e1 + e2;
	uint32	inner		= ei & 1 ? square(ei + 1) * 3 / 4 : (square(ei / 2) + ei / 2) * 3 + 1;
	inner -= ei * 3;

	auto	*p		= uvs.resize(outer_ring + inner).begin();
	float	fi;

	//outer ring
	fi = 1 - (e0 - edges.x) / 2;

	// 1,0,0 ... 1-t,t,0 ... 0,1,0
	p++->set(1, 0);
	for (int i = 0; i < e0 - 1; i++) {
		float	t = (i + fi) / edges.x;
		p++->set(1 - t, t);
	}

	// 0,1,0 ... 0,1-t,t ... 0,0,1
	p++->set(0, 1);
	fi = 1 - (e1 - edges.y) / 2;
	for (int i = 0; i < e1 - 1; i++) {
		float	t = (i + fi) / edges.y;
		p++->set(0, 1 - t);
	}

	// 0,0,1 ... t,0,1-t ... 1,0,0
	p++->set(0, 0);
	fi = 1 - (e2 - edges.z) / 2;
	for (int i = 0; i < e2 - 1; i++) {
		float	t = (i + fi) / edges.z;
		p++->set(t, 0);
	}

	float	t = (1 - (ei - inside) / 2) / inside;

	//inner rings
	--ei;
	while (ei > 1) {
		
		for (int i = 0; i < ei - 1; i++) {
			float	u = t + i / inside;
			p++->set(1 - u, u);
		}

		for (int i = 0; i < ei - 1; i++) {
			float	u = t + i / inside;
			p++->set(t, 1 - u);
		}

		for (int i = 0; i < ei - 1; i++) {
			float	u = t + i / inside;
			p++->set(u, t);
		}

		t += 1 / inside;
		ei -= 2;
	}

	if (ei == 1)
		p++->set(t, t);

}

//isoline
Tesselation::Tesselation(const float2p &edges, Spacing spacing) {
	if (edges.x <= 0 || edges.y <= 0)
		return;

	uint32	e0 = effective(EQUAL, edges.x);
	uint32	e1 = effective(spacing, edges.y);
	auto	*p	= uvs.resize(e0 * (e1 + 1)).begin();

	for (int x = 0; x < e0; x++) {
		float	u = float(x) / e0;

		p++->set(u, 0);
		float	fy = 1 - (e1 - edges.y) / 2;
		for (int y = 0; y < e1 - 1; y++) {
			float	v = (y + fy) / edges.y;
			p++->set(u, v);
		}
		p++->set(u, 1);
	}
}


dynamic_array<float2p>	MakeTesselation(const float4p &edges, const float2p &inside) {
	return move(Tesselation(edges, inside).uvs);
}
dynamic_array<float2p>	MakeTesselation(const float3p &edges, float inside) {
	return move(Tesselation(edges, inside).uvs);
}
dynamic_array<float2p>	MakeTesselation(const float2p &edges) {
	return move(Tesselation(edges).uvs);
}

typedef constructable<float2p>	cfloat2p;
typedef constructable<float3p>	cfloat3p;
typedef constructable<float4p>	cfloat4p;


struct test_tesselation {
	test_tesselation() {

		//Tesselation	t1(cfloat2p(1, 2.1f));
		Tesselation	t2(cfloat4p(3,3,3,3), cfloat2p(3,3));

	}
} _test_tesselation;
