#include "patch.h"
#include "collision/collision.h"
#include "render_object.h"
#include "tweak.h"
#include "utilities.h"
#include "light.h"
#include "model.h"

#ifdef ISO_EDITOR
#include "3d/model_utils.h"
#endif

#undef small

#ifndef CODE1
#define CODE1	1
#define CODE2	1
#endif

using namespace iso;

#if defined(PLAT_X360) || defined(PLAT_PS4) || defined(USE_DX11) || defined(USE_DX12)
#define USE_TESSELATOR
#endif

#if defined(PLAT_PC) && !defined(USE_DX11) && !defined(USE_DX12)

//-----------------------------------------------------------------------------
//	PC
//-----------------------------------------------------------------------------

#define MAX_TESSELATION	32
TWEAK(float, RENDER_TESS_FACTOR, 16.f, 0.f, 32.f);

static bool mainpatch = true, edgeu = true, edgev = true;

class PatchDrawer {

	class Tess {
		VertexBuffer<float2p>	vb;
		IndexBuffer<uint16>		ibh;
		IndexBuffer<uint16>		ibv;
	public:
		void	Init(int tess) {
			uint16	*pih = ibh.Begin(tess * (tess + 2) * 2);
			uint16	*piv = ibv.Begin(tess * (tess + 2) * 2);

			for (int i = 0; i < tess; i += 2) {
				*pih++ = (i + 0) * (tess + 1) + 0;
				*pih++ = (i + 1) * (tess + 1) + 0;
				*piv++ = (i + 0) + (tess + 1) * tess;
				*piv++ = (i + 1) + (tess + 1) * tess;
				for (int j = 0; j <= tess; j++) {
					int	k = tess - j;
					*pih++ = (i + 1) * (tess + 1) + j;
					*pih++ = (i + 0) * (tess + 1) + j;
					*piv++ = (i + 1) + (tess + 1) * k;
					*piv++ = (i + 0) + (tess + 1) * k;
				}
				if (i == tess - 1)
					break;

				*pih++ = (i + 1) * (tess + 1) + tess;
				*pih++ = (i + 1) * (tess + 1) + tess;
				*piv++ = (i + 1) + (tess + 1) * 0;
				*piv++ = (i + 1) + (tess + 1) * 0;
				for (int j = 0; j <= tess; j++) {
					int	k = tess - j;
					*pih++ = (i + 1) * (tess + 1) + k;
					*pih++ = (i + 2) * (tess + 1) + k;
					*piv++ = (i + 1) + (tess + 1) * j;
					*piv++ = (i + 2) + (tess + 1) * j;
				}
			}
			ibh.End();
			ibv.End();

			float2p	*v	= vb.Begin((tess + 1) * (tess + 1));
			for (int i = 0; i <= tess; i++) {
				for (int j = 0; j <= tess; j++)
					v++->set(j, i);
			}
			vb.End();
		}

		void	Draw(GraphicsContext &ctx, int tess1, int tess2, bool vert) {
			if (!vb)
				Init(tess1);
			ctx.SetIndices(vert ? ibv : ibh);
			ctx.SetVertices(vb);
			ctx.DrawIndexedPrimitive(PRIM_TRISTRIP, 0, (tess1 + 1) * (tess1 + 1), 0, (tess1 + 2) * tess2 * 2 - 2);
		}
	} tess[MAX_TESSELATION + 1];

public:
	float4p	scale, scale2;

	void	Draw(GraphicsContext &ctx, float tess1, float tess2, float tess3, float tess4) {
		float	tessu	= min(tess1, tess3);
		float	tessv	= min(tess2, tess4);
		int		itessu	= int(tessu);
		int		itessv	= int(tessv);

		itessu += int(itessu != tessu);
		itessv += int(itessv != tessv);

		scale.set(1.f / tessu, 1.f / tessv, 0, 0);
		scale2.x = 1 - itessu * scale.x;
		scale2.y = 1 - itessv * scale.y;
#if 1
		ctx.SetShaderConstants(&scale, 94, 2);
		if (itessu >= itessv)
			tess[itessu].Draw(ctx, itessu, itessv, false);
		else
			tess[itessv].Draw(ctx, itessv, itessu, true);
#else
		if (tess1 != tess3) {
			if (tess3 < tess1)
				scale.w = scale.y;
			itessv--;
		}
		if (tess2 != tess4) {
			if (tess4 < tess2)
				scale.z = scale.x;
			itessu--;
		}

		if (mainpatch) {
			ctx.SetVertexShaderConstants(94, 2, (float*)&scale);
			if (itessu >= itessv)
				tess[itessu].Draw(itessu, itessv, false);
			else
				tess[itessv].Draw(itessv, itessu, true);
		}

		if (edgeu && tess1 != tess3) {
			float	tessu2	= max(tess1, tess3);
			int		itessu2	= int(tessu2 + 0.999f);
			scale.z = 0;
			scale.w = tess1 > tess3 ? 0 : scale.y * (tessv - 1);
			scale.x	= 1 / tessu2;
			scale2.x = 1 - itessu2 * scale.x;
			float4	save2	= scale2;
			ctx.SetVertexShaderConstants(94, 2, (float*)&scale);
			tess[itessu2].Draw(itessu2, 1, false);

			scale.x	= 1 / tessu;
			scale.w = 0;
			scale2 = save2;
		}

		if (edgev && tess2 != tess4) {
			float	tessv2	= max(tess2, tess4);
			int		itessv2	= int(tessv2 + 0.999f);
			scale.z = tess2 > tess4 ? 0 : scale.x * (tessu - 1);
			scale.y	= 1 / tessv2;
			ctx.SetVertexShaderConstants(94, 2, (float*)&scale);
			tess[itessv2].Draw(itessv2, 1, true);
		}
#endif
	}

	PatchDrawer() {
		AddShaderParameter(ISO_CRC("scale", 0xec462584), scale);
	}
} patchdrawer;

#elif defined PLAT_WII

//-----------------------------------------------------------------------------
//	WII
//-----------------------------------------------------------------------------

#define MAX_TESSELATION	32
#define USE_DTT

inline float3 avg_normal(param(float3) n0, param(float3) n1, param(float3) d) {
	float3	a = n0 + n1;
	return a - d * (dot(d, a) / len2(d));
}

class PatchDrawer {
public:
	struct Vertex : array_vec<uint16,4>	{};
	struct Index	{ uint8 mtx, tmtx, pos, nrm, s, t; };

	struct SeamVertex {
		float4p	pos;	// w == 1 for norms
		float2p	uv;
	};
	struct SeamIndex {
		uint8 mtx, tmtx, pos, nrm, t;
	};

	VertexBuffer			vb;
	_IndexBuffer<Index>		ibs[MAX_TESSELATION + 1];
	_IndexBuffer<SeamIndex>	seam_ib;
	VertexStream			vs[2];

	void					Init();
	static void				MakeIB(_IndexBuffer<Index> &ib, int tess);

	VertexBuffer			&GetVB() {
		if (!vb)
			Init();
		return vb;
	}
	_IndexBuffer<Index>		&GetIB(int tess) {
		_IndexBuffer<Index> &ib = ibs[tess];
		if (!ib)
			MakeIB(ib, tess);
		return ib;
	}

	void	Begin() {}

	void	DrawSeam(int tess1, int tess2, int s);
	void	DrawSeam(int tess1, int tess2, int tessv, int q, param(float4x4) s, param(float4x4) s1);

	void	Draw(const bezier_patch &patch
		, float tess1, float tess2, float tess3, float tess4
		, const ShaderConstants &sc, pass *pass
		, float3x3 *uv_mats, int nuv
	);

	PatchDrawer();
};

#if CODE1
TWEAK(float, RENDER_TESS_FACTOR, 8.f, 0.f, 32.f);
#endif

#if CODE2

#ifdef USE_DTT
float4x3	patch_uv[4];
#else
float4x2	patch_uv[4];
#endif
int			g_utess = 4, g_vtess = 8;

static float3x3	blendu(
	float3( 1, -2, 1),
	float3(-2,  2, 0),
	float3( 1,  0, 0)
);

PatchDrawer	patchdrawer;

PatchDrawer::PatchDrawer() {
	AddShaderParameter(ISO_CRC("patch_uv", 0xd8cea439), patch_uv);
}

void PatchDrawer::Init() {
	Vertex	*v	= vb.Begin<Vertex>(MAX_TESSELATION + 1);
	vs[0].Init(USAGE_POSITION, v);
	vs[1].Init(USAGE_NORMAL, (Vertex*)&v->y);
	for (int t = 0; t <= MAX_TESSELATION; t++)
		v++->set(t * t * t, t * t, t, 1<<14);
	vb.End();
	seam_ib.Create((MAX_TESSELATION + MAX_TESSELATION - 1) * 3);
}

void PatchDrawer::MakeIB(_IndexBuffer<Index> &ib, int tess) {
	Index	*i	= ib.Begin(((tess + 1) * 2 + 1) * tess);
	for (int s = 0; s < tess; s++) {
		int	m = (s & 7) * 3;
		for (int t = 0; t <= tess; t++) {
			i->mtx = i->tmtx = m;
			i->nrm = i->pos = t;
			i->s = s; i->t = t;
			i++;
			i->mtx = i->tmtx = m + 3;
			i->nrm = i->pos = t;
			i->s = s + 1; i->t = t;
			i++;
		}
#if 0
		i->mtx = i->tmtx = 0;
		i->nrm = i->pos = -1;
		i->s = i->t = -1;
		i++;
#else
		i->mtx = i->tmtx = m + 3;
		i->nrm = i->pos = tess;
		i->s = s + 1; i->t = tess;
		i++;
		if (++s < tess) {
			m = (s & 7) * 3;
			for (int t = tess; t >= 0; t--) {
				i->mtx = i->tmtx = m;
				i->nrm = i->pos = t;
				i->s = s; i->t = t;
				i++;
				i->mtx = i->tmtx = m + 3;
				i->nrm = i->pos = t;
				i->s = s + 1; i->t = t;
				i++;
			}
			i->mtx = i->tmtx = m + 3;
			i->nrm = i->pos = 0;
			i->s = s + 1; i->t = 0;
			i++;
		}
#endif
	}
	ib.End();
}

void PatchDrawer::DrawSeam(int tess1, int tess2, int s) {
	VertexBuffer	vb;
	SeamVertex		*v = vb.Begin<SeamVertex>(tess1 + tess2 + 2, MEM_FRAME);
	VertexStream	seam_vs[3];

	seam_vs[0].Init(USAGE_POSITION, v);
	seam_vs[1].Init(USAGE_NORMAL, (SeamVertex*)&v->pos.y);
	seam_vs[2].Init(ComponentUsage(USAGE_TEXCOORD + 7), (SeamVertex*)&v->uv);

	for (int i = 0; i <= tess1; i++) {
		float	t = float(i);
		v[i].pos.set(t * t * t, t * t, t, 1);
		v[i].uv.set(s, t);
	}
	for (int i = 0; i <= tess2; i++) {
		float	t = float(i) * tess1 / tess2;
		v[i + tess1 + 1].pos.set(t * t * t, t * t, t, 1);
		v[i + tess1 + 1].uv.set(s, t);
	}
	vb.End();

	SeamIndex	*i	= seam_ib.Begin();
	int		m	= (s & 7) * 3;
	for (int t1 = 1, t2 = 0; t2 < tess2; t2++) {
		i[0].mtx = i[0].tmtx = m;
		i[0].nrm = i[0].pos = i[0].t = t1;

		i[1].mtx = i[1].tmtx = m;
		i[1].nrm = i[1].pos = i[1].t = tess1 + t2 + 2;

		i[2].mtx = i[2].tmtx = m;
		i[2].nrm = i[2].pos = i[2].t = tess1 + t2 + 1;

		i += 3;

		if (t1 * tess2 < t2 * tess1) {
			i[0].mtx = i[0].tmtx = m;
			i[0].nrm = i[0].pos = i[0].t = t1;

			t1++;
			i[1].mtx = i[1].tmtx = m;
			i[1].nrm = i[1].pos = i[1].t = t1;

			i[2].mtx = i[2].tmtx = m;
			i[2].nrm = i[2].pos = i[2].t = tess1 + t2 + 2;

			i += 3;
		}
	}

	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetVertexType<SeamVertex>();
	ctx.SetStreamSource(seam_vs, 3);
	ctx.DrawIndexedVertices(PRIM_TRILIST, seam_ib, 0, (tess1 + tess2 - 2) * 3);
	ctx.SetBackFaceCull(BFC_BACK);
}

void PatchDrawer::DrawSeam(int tess1, int tess2, int tessv, int q, param(float4x4) s, param(float4x4) s1) {

	float3		n0		= cross(float4(s.y - s.x).xyz, float4(s1.x - s.x).xyz);
	float3		n2		= cross(float4(s.w - s.z).xyz, float4(s1.w - s.w).xyz);
	if (q & 1) {
		n0 = -n0;
		n2 = -n2;
	}
	float3		n1		= avg_normal(n0, n2, float4(s.w - s.x).xyz);

	float3x3	nmat	= float3x3(n0, n1, n2) * blendu;
	float4x4	mat		= s * bezier_spline::blend;

	ctx.SetWorldMatrix(0, float3x4(mat), nmat);

	VertexBuffer	vb;
	SeamVertex		*v = vb.Begin<SeamVertex>(tess1 + tess2 + 2, MEM_FRAME);
	VertexStream	seam_vs[3];

	seam_vs[0].Init(USAGE_POSITION, v);
	seam_vs[1].Init(USAGE_NORMAL, (SeamVertex*)&v->pos.y);
	seam_vs[2].Init(ComponentUsage(USAGE_TEXCOORD + 7), (SeamVertex*)&v->uv);

	for (int i = 0; i <= tess1; i++) {
		float	t = float(i) / tess1;
		v[i].pos.set(t * t * t, t * t, t, 1);
	}
	for (int i = 0; i <= tess2; i++) {
		float	t = float(i) / tess2;
		v[i + tess1 + 1].pos.set(t * t * t, t * t, t, 1);
	}
	int x = q & 1 ? tessv : 0;
	if (q & 2) {
		for (int i = 0; i <= tess1; i++)
			v[i].uv.set(i, x);
		for (int i = 0; i <= tess2; i++)
			v[i + tess1 + 1].uv.set(float(i) * tess1 / tess2, x);
	} else {
		for (int i = 0; i <= tess1; i++)
			v[i].uv.set(x, i);
		for (int i = 0; i <= tess2; i++)
			v[i + tess1 + 1].uv.set(x, float(i) * tess1 / tess2);
	}
	vb.End();

	SeamIndex	*i	= seam_ib.Begin();
	int			m	= 0;
	int			o0	= tess1 + 2;
	int			o1	= tess1 + 1 + (q & 1);
	int			o2	= tess1 + 2 - (q & 1);
	for (int t1 = 1, t2 = 0; t2 < tess2; t2++) {
		i[0].mtx = i[0].tmtx = m;
		i[0].nrm = i[0].pos = i[0].t = t1;

		i[1].mtx = i[1].tmtx = m;
		i[1].nrm = i[1].pos = i[1].t = o1 + t2;

		i[2].mtx = i[2].tmtx = m;
		i[2].nrm = i[2].pos = i[2].t = o2 + t2;

		i += 3;

		if (t1 * tess2 < t2 * tess1) {
			i->mtx = i->tmtx = m;
			i->nrm = i->pos = i->t = t1++;
			i++;

			if (q & 1) {
				i->mtx = i->tmtx = m;
				i->nrm = i->pos = i->t = o0 + t2;
				i++;
			}

			i->mtx = i->tmtx = m;
			i->nrm = i->pos = i->t = t1;
			i++;

			if (!(q & 1)) {
				i->mtx = i->tmtx = m;
				i->nrm = i->pos = i->t = o0 + t2;
				i++;
			}
		}
	}

	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetVertexType<SeamVertex>();
	ctx.SetStreamSource(seam_vs, 3);
	ctx.DrawIndexedVertices(PRIM_TRILIST, seam_ib, 0, (tess1 + tess2 - 2) * 3);
	ctx.SetBackFaceCull(BFC_BACK);
}

void PatchDrawer::Draw(const bezier_patch &patch
	, float tess1, float tess2, float tess3, float tess4
	, const ShaderConstants &sc, pass *pass
	, float3x3 *uv_mats, int nuv
) {
	VertexBuffer	&vb = GetVB();

//	tess1 = tess3 = g_utess;
//	tess2 = tess4 = g_vtess;
	float	tessu	= min(tess1, tess3),	tessv	= min(tess2, tess4),	seamv;
	float	ctessu	= ceil(tessu),			ctessv	= ceil(tessv),			cseamv;
	int		itessu	= int(ctessu),			itessv	= int(ctessv),			iseamv;
	int		seam	= 0;

	float4x4		r0, r1, r2, r3;

	if (itessu > itessv) {	//flip the patch 90 to make the matrices go along v
		r0 = patch.col(3).as_float4x4();
		r1 = patch.col(2).as_float4x4();
		r2 = patch.col(1).as_float4x4();
		r3 = patch.col(0).as_float4x4();
		float3x3	suv(float3(zero, 1/ctessv, zero), float3(-1/ctessu, zero, zero), float3(one, zero, one));
		for (int i = 0; i < nuv; i++) {
			float3x3	t = transpose(uv_mats[i] * suv);
			patch_uv[i] = t;
		}
		swap(ctessu, ctessv);
		swap(itessu, itessv);

		sc.Set(pass);

		int	iseamu	= int(ceil(max(tess2, tess4)));
		if (iseamu != itessu) {
			if (tess2 < tess4)
				DrawSeam(itessu, iseamu, itessv, 1, r3, r2);
			else
				DrawSeam(itessu, iseamu, itessv, 0, r0, r1);
		}

		seamv	= max(tess1, tess3);
		cseamv	= ceil(seamv);
		iseamv	= int(cseamv);
		if (iseamv != itessv)
			seam = tess1 < tess3 ? 1 : 2;

	} else {
		r0 = patch.row(0).as_float4x4();
		r1 = patch.row(1).as_float4x4();
		r2 = patch.row(2).as_float4x4();
		r3 = patch.row(3).as_float4x4();
		float3	suv(reciprocal(float2(ctessu, ctessv)), one);
		for (int i = 0; i < nuv; i++) {
			float3x3	t = transpose(uv_mats[i] * scale(suv));
			patch_uv[i] = t;
		}

		sc.Set(pass);

		int	iseamu	= int(ceil(max(tess1, tess3)));
		if (iseamu != itessu) {
			if (tess1 < tess3)
				DrawSeam(itessu, iseamu, itessv, 3, r3, r2);
			else
				DrawSeam(itessu, iseamu, itessv, 2, r0, r1);
		}

		seamv	= max(tess2, tess4);
		cseamv	= ceil(seamv);
		iseamv	= int(cseamv);
		if (iseamv != itessv)
			seam = tess2 < tess4 ? 1 : 2;
	}


	float3		rtessu	= reciprocal(float3(ctessu * ctessu * ctessu, ctessu * ctessu, ctessu));
	float3		rtessv	= reciprocal(float3(ctessv * ctessv * ctessv, ctessv * ctessv, ctessv));
	float4x4	blendu	= bezier_spline::blend * scale(rtessu);
	float4		vfactors = float4(1, 3, 3, 3) * perm<0,0,1,2>(rtessv);

	//generate quadratic patch of normals
	float3		n0	= cross(float4(r0.y - r0.x).xyz, float4(r1.x - r0.x).xyz);
	float3		n2	= cross(float4(r0.w - r0.z).xyz, float4(r1.w - r0.w).xyz);
	float3		n1	= avg_normal(n0, n2, float4(r0.w - r0.x).xyz);

	float3		n6	= cross(float4(r3.y - r3.x).xyz, float4(r3.x - r2.x).xyz);
	float3		n8	= cross(float4(r3.w - r3.z).xyz, float4(r3.w - r2.w).xyz);
	float3		n7	= avg_normal(n6, n8, float4(r3.w - r3.x).xyz);

	float3		n3	= avg_normal(n0, n6, float4(r3.x - r0.x).xyz);
	float3		n5	= avg_normal(n2, n8, float4(r3.w - r0.w).xyz);
	float3		n4	= avg_normal(n1, n7, float4(r3.y - r0.y).xyz);	// approx d

	float3x3	blendnu(
		float3( 1, -2, 1) * rtessu.y,
		float3(-2,  2, 0) * rtessu.z,
		float3( 1,  0, 0)
	);
	float3x3	blendnv(
		float3( 1, -2, 1) * float(1<<14) * rtessv.y,
		float3(-2,  2, 0) * float(1<<14) * rtessv.z,
		float3( 1,  0, 0)
	);

	float3x3	nc0(n0, n1, n2);
	float3x3	nc1(n3, n4, n5);
	float3x3	nc2(n6, n7, n8);

	nc0 = nc0 * blendnu;
	nc1 = nc1 * blendnu;
	nc2 = nc2 * blendnu;

	r0 = r0 * blendu;
	r1 = r1 * blendu;
	r2 = r2 * blendu;
	r3 = r3 * blendu;

#if 1
	r0.z = r0.x + r0.y + r0.z; r0.x = r0.x * 6; r0.y = r0.x + r0.y * 2;
	r1.z = r1.x + r1.y + r1.z; r1.x = r1.x * 6; r1.y = r1.x + r1.y * 2;
	r2.z = r2.x + r2.y + r2.z; r2.x = r2.x * 6; r2.y = r2.x + r2.y * 2;
	r3.z = r3.x + r3.y + r3.z; r3.x = r3.x * 6; r3.y = r3.x + r3.y * 2;

	_IndexBuffer<Index>	&ib	= GetIB(itessv);

	for (int u = 0, s = 0; ; u++) {
//		float4x4	mat	= float4x4(r0.w, r1.w, r2.w, r3.w) * blendv;
		float4x4	mat	= float4x4(
			(r3.w - r0.w) * vfactors.x + (r1.w - r2.w) * vfactors.y,
			(r0.w + r2.w - r1.w - r1.w) * vfactors.z,
			(r1.w - r0.w) * vfactors.w,
			r0.w
		);

		float3		tu(u * u, u, 1);
		float3x3	nmat= float3x3(
			nc0 * tu,
			nc1 * tu,
			nc2 * tu
		) * blendnv;

		int	i = u - s;
		ctx.SetWorldMatrix(i * 3, float3x4(mat), nmat);

		if (u == 0) {
			if (seam == 1)
				DrawSeam(itessv, iseamv, u);
			ctx.SetVertexType<Vertex>();
			ctx.SetStreamSource(vs, 2);
		}

		if (i == 8 || u == itessu) {
			ctx.DrawIndexedVertices(PRIM_TRISTRIP, ib, ((itessv + 1) * 2 + 1) * s, ((itessv + 1) * 2 + 1) * i);
			if (u == itessu) {
				if (seam == 2)
					DrawSeam(itessv, iseamv, u);
				break;
			}
			s = u;
			ctx.SetWorldMatrix(0, float3x4(mat), nmat);
		}

		r0.w += r0.z; r0.z += r0.y; r0.y += r0.x;
		r1.w += r1.z; r1.z += r1.y; r1.y += r1.x;
		r2.w += r2.z; r2.z += r2.y; r2.y += r2.x;
		r3.w += r3.z; r3.z += r3.y; r3.y += r3.x;
	}
#else
	float4x4	blendv	= bezier_spline::blend * scale(rtessv);
	for (int u = 0; u <= itessu; u++) {
		position3	tu(u * u * u, u * u, u);
		float4x4	mat	= float4x4(
			r0 * tu,
			r1 * tu,
			r2 * tu,
			r3 * tu
		) * blendv;
		ctx.SetWorldMatrix(float3x4(mat), u * 3);
	}
#endif
}

template<> static const VertexElements ve<PatchDrawer::Vertex> = (const VertexElement[]) {
	VertexElement(0,	0,				USAGE_TEXMATRIX,7),
	VertexElement(0,	0,				USAGE_BLENDINDICES),
	MakeVE<array_vec<uint16,3>>	(0,	USAGE_POSITION,	0, 1),
	MakeVE<array_vec<uint16,3>>	(0,	USAGE_NORMAL,	0, 1),
	MakeVE<uint8[2]>				(0,	USAGE_TEXCOORD,	7)
};

template<> static const VertexElements ve<PatchDrawer::SeamVertex> = (const VertexElement[]) {
	VertexElement(0,	0,		USAGE_TEXMATRIX,	7),
	VertexElement(0,	0,		USAGE_BLENDINDICES),
	MakeVE<float3p>(0,			USAGE_POSITION,		0, 1),
	MakeVE<float3p>(0,			USAGE_NORMAL,		0, 1),
	MakeVE<float2p>(0,			USAGE_TEXCOORD,		7, 1)
};

#else

extern PatchDrawer	patchdrawer;

#endif

#else

#define MAX_TESSELATION	32
TWEAK(float, RENDER_TESS_FACTOR, 16.f, 0.f, 32.f);

#endif

//-----------------------------------------------------------------------------
//	PatchModel3
//-----------------------------------------------------------------------------

#if CODE1

bezier_spline GetEdge(const bezier_patch &patch, int i) {
	switch (i) {
		default:return patch.bottom();
		case 1: return patch.right();
		case 2: return patch.top();
		case 3: return patch.left();
	}
}
float Curviness(const bezier_spline &s) {
	float3	line	= s[3].xyz - s[0].xyz;
//	float3	edge0	= s.c1.xyz - s.c0.xyz;
//	float3	edge1	= s.c3.xyz - s.c2.xyz;
//	float x0 = abs(len(cross(line, edge0)) / len(line) / len(edge0));
//	float x1 = abs(len(cross(line, edge1)) / len(line) / len(edge1));
//	return max(max(x0, x1), .1f) * len(line);
	return len(line);
}

void TesselationInfo::Init(PatchModel3 *model) {
	total = 0;
	int	nsubs = model->subpatches.Count();
	for (int i = 0; i < nsubs; i++)
		total += ISO::Browser(model->subpatches[i].verts).Count();

	bezier_spline	*edges = new bezier_spline[total * 4], *end_edges = edges;

	shared		= new pair<int,int>[total * 4];
	curvy		= new float[total * 2];
	spheres		= new sphere[total];

	for (int i = 0, pi = 0; i < nsubs; i++) {
		ISO::Browser	b(model->subpatches[i].verts);
		for (int i = 0, n = b.Count(); i < n; i++, pi++) {
			bezier_patch	patch;
			float3p			*p = b[i];

			for (int j = 0; j < 16; j++)
				patch.cp(j) = concat(p[j], one);

			spheres[pi]			= sphere::bound_quick((position3(&)[16])patch);
			curvy[pi * 2 + 0]	= max(Curviness(patch.row(0)), Curviness(patch.row(3)));
			curvy[pi * 2 + 1]	= max(Curviness(patch.col(0)), Curviness(patch.col(3)));

			for (int j = 0; j < 4; j++) {
				bezier_spline	e(GetEdge(patch, j));
				bezier_spline	*f = find(edges, end_edges, e);
				if (f == end_edges) {
					*end_edges++ = e.flip();
					shared[f - edges].a	= pi * 4 + j;
					shared[f - edges].b	= -1;
				} else {
					shared[f - edges].b	= pi * 4 + j;
				}
			}
		}
	}

	num_shared = int(end_edges - edges);
	for (int i = 0; i < num_shared;) {
		if (shared[i].b == -1)
			shared[i] = shared[--num_shared];
		else
			i++;
	}
	delete[] edges;
}

void TesselationInfo::Calculate(param(float4x4) matrix, float factor, float *tess) {
	float3	cull1	=  rsqrt(square(matrix.x.xyz - matrix.x.w) + square(matrix.y.xyz - matrix.y.w) + square(matrix.z.xyz - matrix.z.w));
	float3	cull2	= -rsqrt(square(matrix.x.xyz + matrix.x.w) + square(matrix.y.xyz + matrix.y.w) + square(matrix.z.xyz + matrix.z.w));

	for (int k = 0; k < total; k++) {
		float4		middle1	= matrix * spheres[k].centre();
		if (any((middle1.xy  - middle1.w) * cull1.xy  > spheres[k].radius())
		||	any((middle1.xyz + middle1.w) * cull2.xyz > spheres[k].radius())
		) {
			tess[k * 4 + 0] = tess[k * 4 + 2] =
			tess[k * 4 + 1] = tess[k * 4 + 3] = 0;
		} else {
			float	w = max(middle1.w, one);
			min(curvy[k * 2 + 0] * factor / w, float(MAX_TESSELATION));
			tess[k * 4 + 0] = tess[k * 4 + 2] = min(curvy[k * 2 + 0] * factor / w, float(MAX_TESSELATION));
			tess[k * 4 + 1] = tess[k * 4 + 3] = min(curvy[k * 2 + 1] * factor / w, float(MAX_TESSELATION));
		}
	}
	for (int i = 0; i < num_shared; i++) {
		int	a = shared[i].a, b = shared[i].b;
		if (tess[a] && tess[b])
			tess[a] = tess[b] = max(tess[a], tess[b]);
	}
}

TesselationInfo::~TesselationInfo() {
	delete[] spheres;
	delete[] curvy;
	delete[] shared;
}

#endif

class RenderPatchModel3 {
	friend class PatchModel3Handler;
	PatchModel3			*model;

	struct RenderSubPatch {
		uint32				patch_size;
		int					npatches;
#ifdef USE_TESSELATOR
		_VertexBuffer		patches;
		VertexDescription	vd;
#else
		char						*patches;
		const ISO::TypeComposite	*comp;
#endif
		ShaderConstants		sc[2];
	};

	RenderSubPatch		*rsps;
	TesselationInfo		info;

	static inline void	Begin(GraphicsContext &ctx, const Tesselation::iterator &i);
	static inline void	Draw2(GraphicsContext &ctx, RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass);

public:
	RenderPatchModel3(PatchModel3 *_model);
	~RenderPatchModel3();

	void				UpdateTesselation(const RenderCollector &rc, param(float4x4) matrix, float *tess);
	void				Draw(GraphicsContext &ctx, int pass, Tesselation::iterator it);

	int					Total()		const { return info.total; }
	cuboid				GetBox()	const { return cuboid(position3(model->minext), position3(model->maxext)); }
	const PatchModel3*	GetModel()	const { return model; }
};

#if CODE1

RenderPatchModel3::RenderPatchModel3(PatchModel3 *_model) : model(_model) {
	info.Init(model);
	int	nsubs	= model->subpatches.Count();
	rsps		= new RenderSubPatch[nsubs];
	for (int i = 0; i < nsubs; i++) {
		SubPatch		&subpatch	= model->subpatches[i];
		RenderSubPatch	&rsp		= rsps[i];
		ISO::Browser		b(subpatch.verts);
		rsp.patch_size	= b[0].GetSize();
		rsp.npatches	= b.Count();

#ifdef PLAT_X360
		rsp.patches.Init(b[0], rsp.npatches * rsp.patch_size);

		const ISO::TypeComposite *comp = (const ISO::TypeComposite*)b[0].GetTypeDef()->SkipUser();
		int					texcoord	= 0;
		VertexElements		ve[256], *pve = ve;
		for (int i = 0, n = comp->Count(); i < n; i++) {
			const ISO::Element	&e			= (*comp)[i];
			const ISO::TypeArray *a			= (const ISO::TypeArray*)e.type.get();
			uint32				usage_index = 0;
			ComponentUsage		usage		= GetComponentUsage(comp->GetID(i), usage_index);

			if (usage == USAGE_POSITION) {
				for (int i = 0; i < 16; i++)
					new (pve++) VertexElements(e.offset + i * 12, GetComponentType<float[3]>(), usage, i);
			} else if (usage == USAGE_COLOR) {
				for (int i = 0; i < 4; i++) {
					new (pve++) VertexElements(e.offset + i * 12, GetComponentType<float[3]>(), usage, i);
				}
			} else if (usage == USAGE_TEXCOORD) {
				new (pve++) VertexElements(e.offset,		GetComponentType<float[4]>(), "texcoord", texcoord++);
				new (pve++) VertexElements(e.offset + 16,	GetComponentType<float[4]>(), "texcoord", texcoord++);
			}

		}
		pve->Terminate();
		for (int i = 0, nt = subpatch.technique->Count(); i < nt; i++) {
			pass	*pass = (*subpatch.technique)[i];
			rsp.sc[i].Init(pass, ISO::Browser(subpatch.parameters));
			pass->Bind(ve, &rsp.patch_size);
		}
#elif defined(USE_TESSELATOR)
		const ISO::TypeComposite *comp = (const ISO::TypeComposite*)b[0].GetTypeDef()->SkipUser();
		uint32				offset		= 0;
		VertexElement		ve[256], *pve = ve;
		for (auto e : comp->Components()) {
			const ISO::TypeArray *a			= (const ISO::TypeArray*)e.type.get();
			USAGE2	usage(comp->GetID(i).get_crc32());

			if (usage == USAGE_POSITION) {
				*pve++ = VertexElement(offset, GetComponentType<float[3]>(), usage, i);
				offset += 12;
			} else if (usage == USAGE_COLOR) {
				*pve++ = VertexElement(offset, GetComponentType<float[3]>(), usage, i);
				offset += 12;
			} else if (usage == USAGE_TEXCOORD) {
				*pve++ = VertexElement(offset, GetComponentType<float[2]>(), usage);
				offset += 8;
			}
		}
		rsp.patches.Init(rsp.npatches * offset * 16);
		char	*source	= b[0];
		//char	*buffer	= (char*)rsp.patches.Begin();
		auto	data	= rsp.patches.WriteData();
		char	*buffer	= data;
		for (auto e : comp->Components()) {
			const ISO::TypeArray *a			= (const ISO::TypeArray*)e.type.get();
			USAGE2	usage(comp->GetID(i).get_crc32());

			if (usage == USAGE_POSITION) {
				for (int i = 0; i < rsp.npatches; i++)
					copy_n((float3p*)(source + e.offset + rsp.patch_size * i), make_stride_iterator((float3p*)(buffer + offset * 16 * i), offset), 16);
				buffer += 12;
			} else if (usage == USAGE_COLOR) {
				buffer += 12;
			} else if (usage == USAGE_TEXCOORD) {
				buffer += 8;
			}
		}
		rsp.patch_size	= offset;
		//rsp.patches.End();

		for (int i = 0, nt = subpatch.technique->Count(); i < nt; i++) {
			pass	*pass = (*subpatch.technique)[i];
			rsp.sc[i].Init(pass, ISO::Browser(subpatch.parameters));
			if (i == 0)
				rsp.vd.Init(ve, pve - ve, pass);
		}
#else
		for (int i = 0, nt = subpatch.technique->Count(); i < nt; i++)
			rsp.sc[i].Init((*subpatch.technique)[i], ISO::Browser(subpatch.parameters));
		rsp.comp	= (const ISO::TypeComposite*)b[0].GetTypeDef();
		rsp.patches	= b[0];
#endif
	}
	CTOR_RETURN
}

RenderPatchModel3::~RenderPatchModel3() {
	delete[] rsps;
}

void RenderPatchModel3::UpdateTesselation(const RenderCollector &rc, param(float4x4) matrix, float *tess) {
	PROFILE_CPU_EVENT("UpdateTesselation");
	info.Calculate(matrix, rc.mask & RMASK_NOSHADOW ? RENDER_TESS_FACTOR / 4 : RENDER_TESS_FACTOR, tess);
}

#endif

#if defined PLAT_X360

void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {
	ctx.SetIndices(i);
	ctx.Device()->SetRenderState(D3DRS_MAXTESSELLATIONLEVEL, iorf(15.f).i);
	ctx.Device()->SetRenderState(D3DRS_TESSELLATIONMODE, D3DTM_PEREDGE);
	ctx.Device()->SetVertexDeclaration(NULL);
}
void RenderPatchModel3::Draw2(RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
	sc.Set(pass);
	ctx.Device()->SetStreamSource(0, rsp->patches, 0, 0);
	ctx.Device()->DrawIndexedTessellatedPrimitive(D3DTPT_QUADPATCH, 0, i.si * 4, rsp->npatches);
	i.si += rsp->npatches;
}

#elif defined PLAT_PS3

void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {
	float3x4	*world = GetShaderParameter("world");
	ctx.EDGESetWorld(*world);
}
void RenderPatchModel3::Draw2(RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
	sc.Set(pass);
	ctx.EDGETesselate(i, rsp->patches, rsp->patch_size, rsp->npatches);
	i += rsp->npatches * 4;
}

#elif defined PLAT_WII

#if CODE2
void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {
	patchdrawer.Begin();
}
void RenderPatchModel3::Draw2(RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
	float3x3		uv_mats[4];
	bezier_patch	patch;
	char			*p = rsp->patches;

	for (int n = rsp->npatches; n--; p += rsp->patch_size) {
		float	tess1 = *i++, tess2 = *i++, tess3 = *i++, tess4 = *i++;
		if (tess1 == 0)
			continue;
		float3p	*p2 = (float3p*)p;
		for (int j = 0; j < 16; j++)
			patch.cp(j) = position3(p2[j]);

		int		nuv	= (rsp->patch_size - sizeof(float3p[16])) / sizeof(float2p[4]);
		float2p	*uv = (float2p*)(p2 + 16);
		for (int j = 0; j < nuv; j++, uv += 4) {
			float3x3	matC(
				float3(float2(uv[1]), one),
				float3(float2(uv[2]), one),
				float3(float2(uv[0]), one) * 2
			);
			float3x3	invC	= inverse(matC);
			float3		v		= invC * float3(float2(uv[3]), one);

#if 0
			float3x3	invP(
				float3(one,		zero,	-half),
				float3(zero,	one,	-half),
				float3(zero,	zero,	+half)
			);

			matC.x *= v.x;
			matC.y *= v.y;
			matC.z *= v.z * -2;

			uv_mats[j] = matC * invP;
#else
			float3	z		= matC.z * v.z;
			uv_mats[j].x	= matC.x * v.x + z;
			uv_mats[j].y	= matC.y * v.y + z;
			uv_mats[j].z	= -z;
#endif

		}
		patchdrawer.Draw(patch, tess1, tess2, tess3, tess4, sc, pass, uv_mats, nuv);
	}
}
#endif

#elif defined PLAT_IOS || defined PLAT_MAC || defined PLAT_ANDROID

void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {
}
void RenderPatchModel3::Draw2(GraphicsContext &ctx, RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
}

#elif defined PLAT_PS4

void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {
}
void RenderPatchModel3::Draw2(GraphicsContext &ctx, RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
	sc.Set(ctx, pass);
//	ctx.SetVertexType(rsp->vd);
	ctx.SetVertices(0, rsp->patches, rsp->patch_size, rsp->vd);
	ctx.DrawVertices(PatchPrim(16), 0, rsp->npatches * 16);
}

#elif defined USE_DX11 || defined USE_DX12

void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {
}
void RenderPatchModel3::Draw2(GraphicsContext &ctx, RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
	sc.Set(ctx, pass);
	ctx.SetVertexType(rsp->vd);
	ctx.SetVertices(0, rsp->patches, rsp->patch_size);
	ctx.DrawVertices(PatchPrim(16), 0, rsp->npatches * 16);
}

#else

void RenderPatchModel3::Begin(GraphicsContext &ctx, const Tesselation::iterator &i) {}
void RenderPatchModel3::Draw2(GraphicsContext &ctx, RenderSubPatch *rsp, Tesselation::iterator &i, const ShaderConstants &sc, pass *pass) {
	sc.Set(ctx, pass);
	char	*p		= rsp->patches;
	for (int n = rsp->npatches; n--; p += rsp->patch_size) {
		float	tess1 = *i++, tess2 = *i++, tess3 = *i++, tess4 = *i++;
		if (tess1 == 0)
			continue;
		const	ISO::TypeComposite	&comp	= *rsp->comp;
		int		texcoord = 0;
		for (int i = 0, n = comp.Count(); i < n; i++) {
			const ISO::Element		&e			= comp[i];
			const ISO::TypeArray	*a			= (const ISO::TypeArray*)e.type.get();
			uint32					usage_index = 0;
			ComponentUsage			usage		= GetComponentUsage(comp.GetID(i), usage_index);
			if (usage == USAGE_POSITION) {
				ctx.SetShaderConstants((float3p*)(p + e.offset), 64, 16);
			} else if (usage == USAGE_COLOR) {
				ctx.SetShaderConstants((float3p*)(p + e.offset) + i, 80 + i, 4);
			} else if (usage == USAGE_TEXCOORD) {
				ctx.SetShaderConstants((float4p*)(p + e.offset), 84 + texcoord * 2, 2);
				texcoord++;
			}
		}
		patchdrawer.Draw(ctx, tess1, tess2, tess3, tess4);
	}
}
#endif


#if CODE2
void RenderPatchModel3::Draw(GraphicsContext &ctx, int pass, Tesselation::iterator it) {
	Begin(ctx, it);
	for (int i = 0, n = model->subpatches.Count(); i < n; i++) {
		SubPatch		&subpatch	= model->subpatches[i];
		RenderSubPatch	*rsp		= &rsps[i];
		Draw2(ctx, rsp, it, rsp->sc[pass], (*subpatch.technique)[pass]);
	}
}
#endif

#if CODE1

namespace iso {
	ISO_INIT(PatchModel3) {
#ifndef ISO_EDITOR
		RenderPatchModel3	*rm = (RenderPatchModel3*)ISO::GetUser(p).get();
		if (!rm)
			ISO::GetUser(p) = new RenderPatchModel3(p);
#endif
	}
	ISO_DEINIT(PatchModel3) {
		delete (RenderPatchModel3**)ISO::GetUser(p).get();
	}
}

struct RenderPatchObject : public RenderObject {
	class RenderPatchModel3	*rm;
	Tesselation				tesselation;
	RenderParameters		params;

	RenderPatchObject(Object *_obj, ISO_ptr<void> t);

	void operator()(MoveMessage &m)		{ Move((obj->GetWorldMat() * rm->GetBox()).get_box()); }
	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(RenderEvent *re, uint32 extra);
	void operator()(RenderCollector &rc);
};

RenderPatchObject::RenderPatchObject(Object *_obj, ISO_ptr<void> t) : RenderObject(this, _obj) {
	rm = (RenderPatchModel3*)t.User().get();
#ifdef ISO_EDITOR
	if (!rm)
		t.User() = rm = new RenderPatchModel3(t);
#endif
	tesselation.Create(rm->Total());
}

void RenderPatchObject::operator()(RenderEvent *re, uint32 extra) {
	PROFILE_CPU_EVENT("RenderPatchObject");
	re->consts.SetWorld(obj->GetWorldMat());

#ifdef PLAT_PS3
	re->ctx.SetBlendConst(colour(half, half, half));
#elif !defined PLAT_WII
	re->ctx.SetBlendConst(colour(one));
#endif

#if 0
	if (!(extra >> 8))
		SetBestLights(*this);
#endif
	rm->Draw(re->ctx, extra >> 8, tesselation.begin());
}

void RenderPatchObject::operator()(RenderCollector &rc) {
	uint32				flags	= rm->GetModel()->subpatches[0].flags;
	if (rc.Test(flags)) {
	//	Don't bother frustum culling because a single patch will span most of the level
	//	if (rc.completely_visible || rm->GetAABB().IsVisible(re->worldViewProj)) {
		flags	= rc.Adjust(flags);
		params	= rc;
		obj->Send(RenderMessage(params, rc.Time()));

		if (float opacity = params.opacity) {
			tesselation.Next();
			rm->UpdateTesselation(rc, rc.viewProj * obj->GetWorldMat(), tesselation);

			uint32 d = 1;
			uint32 j = 0;
			if (opacity < 1 || (flags & RMASK_SORT)) {
				if (!(flags & RMASK_SORT))
					rc.re->AddRenderItem(this, MakeKey(RS_ZONLY, 1), j | 0x100);
				rc.re->AddRenderItem(this, MakeKey(RS_TRANSP, 0 - d), j);
			} else if (flags & RMASK_DRAWLAST)
				rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE, 0xffffff), j);
			else {
			#ifdef PLAT_WII
				RenderEvent::AddRenderItem(this, MakeKey(RS_OPAQUE_PATCHES, d), j);
			#else
				rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE, d), j);
			#endif
			}
		}
	}
}


void Draw(GraphicsContext &ctx, ISO_ptr<PatchModel3> &patch, Tesselation::iterator it) {
	RenderPatchModel3	*rm = (RenderPatchModel3*)patch.User().get();
	if (!rm)
		patch.User() = rm = new RenderPatchModel3(patch);
	rm->Draw(ctx, 0, it);
}

namespace iso {

template<> void TypeHandler<PatchModel3>::Create(const CreateParams &cp, crc32 id, const PatchModel3 *t) {
	Object				*obj	= cp.obj;
	RenderPatchObject	*rm		= new RenderPatchObject(obj, ISO::GetPtr(t));
	cp.world->Send(RenderAddObjectMessage(rm, rm->rm->GetBox(), obj->GetWorldMat()));
#ifndef ISO_EDITOR
	const PatchModel3 *model	= t;
	for (int i = 0, n = model->subpatches.Count(); i < n; i++) {
		const SubPatch		&subpatch	= model->subpatches[i];
		if (subpatch.flags & RMASK_COLLISION) {
			ISO::Browser	b(subpatch.verts);
			for (int i = 0, n = b.Count(); i < n; i++) {
				float3p	*s	= b[i];
				bezier_patch	d;
				for (int i = 0; i < 16; i++)
					d.cp(i) = concat(s[i], one);
				CollisionSystem::Add(obj, d, subpatch.flags >> 8);
			}
		}
	}
#endif
}

extern "C" {
	TypeHandler<PatchModel3>		thPatchModel3;
}

} // namespace iso

#endif // CODE1
