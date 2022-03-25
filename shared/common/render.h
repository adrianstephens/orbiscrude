#ifndef RENDER_H
#define RENDER_H

#include "base/vector.h"
#include "graphics.h"
#include "maths/geometry.h"
#include "profiler.h"
#include "vector_iso.h"

namespace iso {
class World;

enum {
	RS_PRE				= 0x00,
	RS_ZPASS			= 0x10,
	RS_OPAQUE			= 0x20,
	RS_ZONLY			= 0x30,
	RS_TRANSP			= 0x40,
	RS_LAST				= 0xff
};

enum {
	RS_EDGES			= RS_TRANSP + 1,
};

#ifdef PLAT_WII
enum {
	RS_OPAQUE_PATCHES		= RS_OPAQUE,
	RS_OPAQUE_UPPERSHADOW,
	RS_OPAQUE_MIDDLESHADOW,
	RS_OPAQUE_LOWERSHADOW,
};
#endif

enum {
	RF_MOTIONBLUR		= 1 << 0,
	RF_STEREOSCOPIC		= 1 << 1,
};

enum RenderMasks {
	RMASK_SORT			= 1 << 0,
	RMASK_NOSHADOW		= 1 << 1,
	RMASK_USETEXTURE	= 1 << 2,
	RMASK_UPPERSHADOW	= 1 << 3,
	RMASK_MIDDLESHADOW	= 1 << 4, RMASK_DRAWFIRST = RMASK_MIDDLESHADOW,
	RMASK_DRAWLAST		= 1 << 5,
	RMASK_DOUBLESIDED	= 1 << 6, RMASK_EDGES = RMASK_DOUBLESIDED,
	RMASK_COLLISION		= 1 << 7,
	RMASK_FADING		= 1 << 8,
	RMASK_ANAGLYPH		= 1 << 9,
	RMASK_DYNAMIC		= 1 << 15,

	RMASK_SUBVIEW		= 1 << 16,
	RMASK_VIEWINDEX		= 1 << 17,
};

struct FlagPair {
	uint32	exclude, require;
	FlagPair(uint32 exclude, uint32 require) : exclude(exclude), require(require) {}
	bool	Excluded(uint32 f)		const	{ return !!(exclude & f);	}
	bool	Required(uint32 f)		const	{ return !!(require & f);	}
};


//	flags0	flags1	|	test	mask	keep	|Test:0	1	|action
//-------------------------------------------------------------------------
//	0		0		|	0		0		1		|	1	1	|always pass
//	1		0		|	0		1		1		|	1	0	|pass if not set
//	0		1		|	1		1		1		|	0	1	|pass if set
//	1		1		|	0		0		0		|	1	1	|do not check (always pass)

struct MaskTester {
	uint32	test, mask, keep;

	MaskTester(uint32 flags0, uint32 flags1) : test(flags1 & ~flags0), mask(flags0 ^ flags1), keep(~(flags0 & flags1)) {}
	bool	Test(uint32 f)		const	{ return (f & mask) == test;	}
	bool	Test1(uint32 f)		const	{ return !!(f & test);			}
	uint32	Adjust(uint32 f)	const	{ return f & keep;				}
};

struct SceneShaderConsts {
	float4x4	proj0,		viewProj0;
	float4x4	proj,		iproj;
	float4x4	viewProj,	iviewProj;
	float3x4	view,		iview;
	float2		zbconv;

	SceneShaderConsts() {}
	SceneShaderConsts(param(float3x4) view, param(float4x4) proj) {
		SetViewProj(view, proj);
	}
	void SetViewProj(param(float4x4) m) {
		viewProj0		= m;
		viewProj		= hardware_fix(m);
		iviewProj		= inverse(viewProj);
	}
	void SetProj(param(float4x4) m) {
		proj0			= m;
		proj			= hardware_fix(m);
		iproj			= inverse(proj);
		SetViewProj(m * view);
		float4x4 zb		= map_fix(m);
		zbconv			= float2{zb.z.z, zb.w.z};
	}
	void SetViewProj(param(float3x4) v, param(float4x4) p) {
		view			= v;
		iview			= (float3x4)inverse(v);
		SetProj(p);
	}
	void SetIViewProj(param(float3x4) v, param(float4x4) p) {
		iview			= v;
		view			= inverse(v);
		SetProj(p);
	}
	void SetView(param(float3x4) v) {
		view			= v;
		iview			= inverse(v);
		SetViewProj(proj0 * v);
	}
	void SetIView(param(float3x4) v) {
		iview			= v;
		view			= inverse(v);
		SetViewProj(proj0 * view);
	}
};

struct ObjectShaderConsts {
	float4x4	worldViewProj;
	float3x4	world, worldView;

	void SetWorldViewProj(param(float4x4) m) {
		worldViewProj	= hardware_fix(m);
	}
	void SetWorld(param(float3x4) m, param(float3x4) view, param(float4x4) viewProj) {
		world			= m;
		worldView		= view * m;
		worldViewProj	= viewProj * m;
	}
	void SetWorld(param(float3x4) m, const SceneShaderConsts &rm1) {
		world			= m;
		worldView		= rm1.view * m;
		worldViewProj	= rm1.viewProj * m;
	}
};

struct ShaderConsts : SceneShaderConsts, ObjectShaderConsts {
	colour			tint, average;
	float			time, dt;
	ShaderConsts() {}
	ShaderConsts(param(float3x4) view, param(float4x4) proj, float time) : SceneShaderConsts(view, proj), tint(float4(one)), time(time) {}
	void			SetWorld(param(float3x4) m)		{ ObjectShaderConsts::SetWorld(m, view, viewProj); }
};

struct RenderView {
	float3x4	offset;
	float4		fov;
	rect		window;
	Surface		display;
	Surface		depth;	// not usually set
	int			id;
};

struct RenderEventBase {
	GraphicsContext	&ctx;
	RenderEventBase(GraphicsContext &_ctx) : ctx(_ctx) {}
};

struct RenderEvent : RenderEventBase, RenderView, FlagPair, ProfileCpuGpuEvent {
	typedef callback<void(RenderEvent*, uint32)>	cb;
	struct RenderItem : cb {
		uint32	key;
		uint32	extra;
		RenderItem(const cb &c, uint32 key, uint32 extra) : cb(c), key(key), extra(extra)	{}
		RenderItem() {}
		bool operator<(const RenderItem &b)	const { return key < b.key; }
	};

	static static_array<RenderItem, 5000>	items;
	void			PostRender();

public:
	ShaderConsts	&consts;

	// settings
	uint32			flags;
	ChannelMask		mask;
	float			quality;

	// accumulated extent
	cuboid			extent;

	RenderEvent(GraphicsContext &ctx, ShaderConsts &consts, uint32 exclude = 0, uint32 require = 0)
		: RenderEventBase(ctx), FlagPair(exclude, require), ProfileCpuGpuEvent(ctx, "RenderEvent")
		, consts(consts)
		, flags(0), mask(CM_ALL), quality(1), extent(empty)
	{
		items.clear();
		SetShaderParams();
	}

	void AddRenderItem(const cb &_cb, uint32 key, uint32 extra) {
		items.emplace_back(_cb, key, extra);
	}

	void			SetTarget(const RenderView &v)	{ *(RenderView*)this = v; ctx.SetRenderTarget(v.display); }
	void			AddExtent(const cuboid &box)	{ extent |= box; }
	const cuboid&	Extent()		const			{ return extent; }
	float			Quality()		const			{ return quality; }
	float			Time()			const			{ return consts.time; }
	void			SetQuality(float q)				{ quality = q;	}
	void			SetShaderParams();
	void			Collect(World *w);

	void			Render() {
		SetShaderParams();
		for (RenderItem *i = items.begin(); i != items.end(); ++i)
			(*i)(this, i->extra);
		PostRender();
	}
};

// MakeKey
inline uint32 _MakeKey(uint8 block, uint32 i)	{ return (block << 24) | i;}
inline uint32 MakeKey(uint8 block, uint32 i)	{ return _MakeKey(block, uint32(i & 0x00ffffff));	}
inline uint32 MakeKey(uint8 block, int i)		{ return _MakeKey(block, uint32(i & 0x00ffffff));	}
inline uint32 MakeKey(uint8 block, float f)		{ return _MakeKey(block, iorf(f).monotonic() >> 8); }

ISO_DEFCOMPV(ShaderConsts,
	view, proj, iproj, viewProj, iview, iviewProj, zbconv,
	world, worldView, worldViewProj,
	tint, average, time, dt
);

} //namespace iso

#endif //RENDER_H
