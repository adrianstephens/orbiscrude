#include "particle.h"
#include "base/algorithm.h"
#include "tweak.h"
#include "crc_dictionary.h"
#include "profiler.h"
#include "systems/mesh/model.h"
#include "maths/bezier.h"
#include "extra/random.h"
#include "utilities.h"
//#include "systems/mesh/light.h"

using namespace iso;

#if	defined(PLAT_X360)
#include "particle.fx.x360.h"
#elif defined(PLAT_PS3)
#include "particle.fx.ps3.h"
#elif defined(PLAT_PS4)
#include "particle.fx.ps4.h"
#elif defined(PLAT_XONE)
#include "particle.fx.xone.h"
#elif defined(PLAT_WII)
#include "particle.fx.wii.h"
#elif defined(PLAT_PC)
#include "particle.fx.pc.h"
#elif defined(PLAT_ANDROID)
#include "particle.fx.android.h"
#else
#include "particle.fx.h"
#endif

//#define SOFTWARE_PARTICLES

#if defined PLAT_WII || (defined PLAT_PC && !defined USE_DX11 && !defined USE_DX12) || defined PLAT_IOS || defined PLAT_MAC
#define SOFTWARE_PARTICLES
#endif

TWEAK(int, PARTICLES_CALC_EXTENT_INTERLEAVE, 10);

enum ParticleModifierFlags {
	PM_PROHIBIT_SPAWNING = 1<<0,
};

colour ScreenFlash::total;

#ifdef PLAT_PS3
Particle *AllocParticles(int n) {
	return (Particle*)graphics.AllocPtr(n * sizeof(Particle), 128, MEM_HOST_HIGH);
}
void FreeParticles(Particle *p, int n) {
	if (p)
		graphics.FreePtr(p, n * sizeof(Particle), MEM_HOST_HIGH);
}
#else
Particle *AllocParticles(int n) {
	return (Particle*)iso::aligned_alloc(n * sizeof(Particle), 16);
}
void FreeParticles(Particle *p, int n) {
	if (p)
		aligned_free(p);
}
#endif

//-----------------------------------------------------------------------------
//	ParticleSystem
//-----------------------------------------------------------------------------

class ParticleSystem : public Handles2<ParticleSystem, AppEvent>, public HandlesGlobal<ParticleSystem, RenderEvent> {
public:
	ISO_ptr<iso::fx>	iso_fx;
	layout_particle		*shaders;

	rng<simple_random>	random;
	float	layerscaler;
	float4	params;
	float3	fx;
	float3	fy;

	~ParticleSystem() {}

	void	operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN) {
			if (iso_fx = ISO::root("data")["particle"]) {
				shaders		= (layout_particle*)(ISO_ptr<technique>*)*iso_fx;
				AddShaderParameter(ISO_CRC("layer_scaler", 0xd7659436),		layerscaler);
				AddShaderParameter(ISO_CRC("particle_params", 0xe53d39ff),	params);
				AddShaderParameter(ISO_CRC("particle_x", 0x05f1b036), 		fx);
				AddShaderParameter(ISO_CRC("particle_y", 0x72f680a0), 		fy);
			} else {
				shaders = 0;
			}
		}
	}
	void operator()(RenderEvent *re, uint32 stage) {
		re->Next("PARTICLE");
	#ifdef PLAT_X360
		if (!re->ctx.Tiling())
			re->ctx.Resolve(RT_COLOUR0_RETAIN);
	#endif
		re->ctx.SetBackFaceCull(re->consts.view.det() < 0 ? BFC_FRONT : BFC_BACK);
		re->ctx.SetMask(re->mask & ~CM_ALPHA);
		re->ctx.SetDepthTest(DT_CLOSER_SAME);
		re->ctx.SetDepthWriteEnable(false);
		re->ctx.SetBlendEnable(true);
	}
	void	operator()(RenderEvent &re) {
		if (!re.Excluded(RMASK_NOSHADOW))
			re.AddRenderItem(this, MakeKey(particles::STAGE, 0), 0);
	}
} particle_system;

void InitParticleSystem() {
	AppEvent ev(AppEvent::BEGIN);
	particle_system(&ev);
}

//-----------------------------------------------------------------------------
//	type helpers
//-----------------------------------------------------------------------------

class pm_calls {
public:
	void (*Apply)(void *p, ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags);
};

class pm_type : public pm_calls, public ISO::TypeUserSave	{
public:
	pm_type(tag _id, const ISO::Type *_subtype) : ISO::TypeUserSave(_id, _subtype)	{}
};

template<class T> class pm_template : public pm_type	{
	static void _Apply(void *p, ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) { ((T*)p)->Apply(ps, begin, end, dt, flags); }
public:
	pm_template(tag _id, const ISO::Type *_subtype) : pm_type(_id, _subtype)	{ Apply = _Apply; }
};

template<class T, int N> class pm_user_comp : public pm_template<T> {
public:
	ISO::TypeCompositeN<N>	comp;
	typedef T _S, _T;
	pm_user_comp(tag _id) : pm_template<T>(_id, &comp)	{}
};

inline void ApplyParticleModifier(ISO_ptr<void> &p, ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) {
	static_cast<const pm_type*>(p.GetType())->Apply(p, ps, begin, end, dt, flags);
}

void ApplyParticleModifiers(anything::view_t array, ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) {
	for (auto i : array)
		ApplyParticleModifier(i, ps, begin, end, dt, flags);
}

//-----------------------------------------------------------------------------
//	types
//-----------------------------------------------------------------------------

struct offset_texture {
	Texture				t;
	float				x;
	float				y;
};

struct normal_mapped {
	Texture				diffuse;
	Texture				normal;
	float				glossiness;
	float				x;
	float				y;
};

struct custom_particle {
	ISO_ptr<iso::technique>	technique;
	anything			params;
	float				x;
	float				y;
};

//-----------------------------------------------------------------------------
// emitter/effect commands
//-----------------------------------------------------------------------------

struct em_die {
	void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		ps.Die();
	}
};

struct em_emit {
	float		num_per_second;
	pm_list		initialisers;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (flags & PM_PROHIBIT_SPAWNING)
			return;

		if (ps.emit_leftover < 0)
			return;

		ps.DontDie();
		int		inum;
		float	old_emit_leftover = ps.emit_leftover;
		if (num_per_second < 0) {
			inum = int(-num_per_second * ps.emit_scale + 0.5f);
		} else {
			float num = num_per_second * ps.emit_scale * dt + ps.emit_leftover;
			inum = int(num);
			ps.emit_leftover = num - inum;
		}

		if (inum > ps.max_parts - ps.num_parts) {
			if (ps.max_parts == 0) {
				ps.max_parts = num_per_second < 0 ? inum : int(num_per_second + 1);
				ps.particles = AllocParticles(ps.max_parts);//new Particle[ps.max_parts];
			}
			inum = min(inum, ps.max_parts - ps.num_parts);
		}

		old_emit_leftover = ps.emit_leftover;

		begin	= &ps.particles[ps.num_parts];
		float dstart_life	= num_per_second > 0 ? 1 / (ps.emit_scale * num_per_second) : 0;
		float start_life	= dstart_life * old_emit_leftover;
		for (int i = 0; i < inum; i++, start_life += dstart_life)
			ps.particles[ps.num_parts++].Init(start_life);

		ApplyParticleModifiers(initialisers, ps, begin, begin + inum, dt, flags);

		if ((ps.mode & PS_SPACE) == PS_WORLD_SPACE) {
			float3x4	velmat	= ps.mat - ps.prev_mat;
			float3x4	mat		= scale(reciprocal(ps.scale)) * ps.mat;
			bool	usevel	= !(ps.mode & PS_ALONG_Z);
			for (Particle *i = begin; i != begin + inum; ++i) {
				if (usevel)
					i->speed.xyz = velmat * position3(i->pos.xyz) / dt + mat * float3(i->speed.xyz);
				else
					i->speed.xyz = mat.y;
				i->pos.xyz = mat * position3(i->pos.xyz);
			}
		}
	}
};

struct em_ifbefore {
	float		time;
	pm_list		yes;
	pm_list		isnot;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (isnot && ps.life < time)
			ps.DontDie();
		ApplyParticleModifiers(ps.life < time ? yes : isnot, ps, begin, end, dt, flags);
	}
};

struct em_ifless {
	int			num;
	pm_list		yes;
	pm_list		isnot;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		ApplyParticleModifiers(end - begin < num ? yes : isnot, ps, begin, end, dt, flags);
	}
};

struct em_attime {
	float		time;
	pm_list		list;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (ps.prev_life <= time) {
			if (ps.life <= time)
				ps.DontDie();
			else
				ApplyParticleModifiers(list, ps, begin, end, ps.life - time, flags);
		}
	}
};

struct em_spawn {
	ISO_ptr<particle_effect> effect;
	float3p		offset;
	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (flags & PM_PROHIBIT_SPAWNING)
			return;

		if (ps.emit_leftover < 0)
			return;
		ParticleSet	*ps2 = new ParticleSet(ps.start_time + ps.life);
		ps2->rel_mat	= translate(position3(offset));
		ps2->emit_scale	= ps.emit_scale;
		ps2->Init(effect.ID(), effect, ps.prev_mat * ps2->rel_mat);
		ps.Append(ps2, NULL);
		ps.DontDie();
	}
};

struct em_flash {
	float3p			col;
	float			time;
	float			radius;
	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		new ScreenFlash(ps.start_time + ps.life, get_trans(ps.mat), colour(col, one), time, radius);
	}
};


//-----------------------------------------------------------------------------
// per-particle commands
//-----------------------------------------------------------------------------

struct pm_kill {
	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (flags & PM_PROHIBIT_SPAWNING)
			return;

		for (Particle *i = begin; i != end; ++i)
			ps.Kill(i);
	}
};

struct pm_set1 {
	int			field;
	float		value;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int		f	= field;
		float	v	= value;
		for (Particle *i = begin; i != end; ++i)
			((float*)i)[f] = v;
	}
};

struct pm_set3 {
	int			field;
	float3p		value;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int		f	= field;
		float3p	v	= value;
		for (Particle *i = begin; i != end; ++i)
			(float3p&)(((float*)i)[f]) = v;
	}
};

struct pm_random1 {
	int			field;
	float		low, high;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int		f	= field;
		float	a	= low, b = high - a;
		for (Particle *i = begin; i != end; ++i)
			((float*)i)[f] = ps.rand.to(b) + a;
	}
};

struct pm_random3 {
	int			field;
	float3p	low, high;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int		f	= field;
		float	x0	= low.x, x1 = high.x - x0;
		float	y0	= low.y, y1 = high.y - y0;
		float	z0	= low.z, z1 = high.z - z0;
		for (Particle *i = begin; i != end; ++i) {
			((float*)i)[f + 0] = ps.rand.to(x1) + x0;
			((float*)i)[f + 1] = ps.rand.to(y1) + y0;
			((float*)i)[f + 2] = ps.rand.to(z1) + z0;
		}
	}
};

struct pm_pos2vel {
	float		scale_low;
	float		scale_high;
	float		angle_low;
	float		angle_high;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float	s0 = scale_low, s1 = scale_high - s0;
		float	a0 = angle_low, a1 = angle_high - a0;
		if (a0 == 0) {
			for (Particle *i = begin; i != end; ++i)
				i->speed.xyz = normalise(i->pos.xyz) * (ps.rand.to(s1) + s0);
		} else if (a1 == 0) {
			float3x3	tilt = rotate_in_x(degrees(angle_low));
			for (Particle *i = begin; i != end; ++i)
				i->speed.xyz = -(look_along_y(-i->pos.xyz) * tilt).y * (ps.rand.to(s1) + s0);
		} else {
			a0 = degrees(a0);
			a1 = degrees(a1);
			for (Particle *i = begin; i != end; ++i)
				i->speed.xyz = -(look_along_y(-i->pos.xyz) * rotate_in_x(ps.rand.to(a1) + a0)).y * (ps.rand.to(s1) + s0);
		}
	}
};

struct pm_sphere {
	int			field;
	float3p		centre;
	float3p		dir;
	float		spread;
	float		thickness;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float3		c(centre);
		float3		d(dir);
		if (spread == 0) {
			for (Particle *i = begin; i != end; ++i)
				(float3p&)(((float*)i)[field]) = c + d * (1.f - ps.rand.to(thickness));
		} else {
			float		w	= 0.5f / spread;
			float		r	= len(d);
			float3		v{zero, zero, r};
			float4 n = float4{dir.y / r, -dir.x / r, 0, dir.z / r + 1};
			quaternion	q1(n * rsqrt(max(len2(n), 1e-6f)));	// normalise while preventing division by zero
			for (Particle *i = begin; i != end; ++i) {
//				float2		a	= normalise(float2(ps.rand.to(2.0f) - 1, ps.rand.to(2.0f) - 1));
				float2		a	= sincos(ps.rand.to(pi * 2));
				float2		b	= normalise(float2{float(ps.rand), w});
				quaternion	q2(a * b.x, zero, b.y);
				(float3p&)(((float*)i)[field]) = c + (q1 * q2 * v) * (1 - ps.rand.to(thickness));
			}
		}
	}
};

struct pm_disc {
	int			field;
	float3p		centre;
	float3p		dir;
	float		thickness;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float3x3	m	= look_along_z(float3(dir));
		float		r	= len(float3(dir));
		float3		x	= m.x * r;
		float3		y	= m.y * r;
		float3		c(centre);
		if (thickness < 0) {
			for (Particle *i = begin; i != end; ++i) {
				float		a = (two * pi) * float(i - ps.particles) / ps.max_parts;
				float2		sc	= sincos(a);
				(float3p&)(((float*)i)[field]) = float3(c + x * sc.x + y * sc.y);
			}
		} else {
			for (Particle *i = begin; i != end; ++i) {
				float2		sc	= sincos(ps.rand.to(pi * 2));
				(float3p&)(((float*)i)[field]) = float3(c + (x * sc.x + y * sc.y) * (one - ps.rand.to(thickness)));
			}
		}
	}
};

struct pm_spawn {
	ISO_ptr<particle_effect> effect;
	float3p		offset;
	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (flags & PM_PROHIBIT_SPAWNING)
			return;
		if (ps.emit_leftover < 0)
			return;
		float3x4	mat = translate(position3(offset));
		if ((ps.mode & PS_SPACE) == PS_RELATIVE_SPACE)
			mat = ps.mat * mat;
		for (Particle *i = begin; i != end; ++i) {
			ParticleSet	*ps2 = new ParticleSet(ps.start_time + ps.life);
			ps2->Init(effect.ID(), effect, mat * translate(position3(i->pos.xyz)));
			ps2->prev_mat = mat * translate(position3(i->speed.xyz * -dt));
			ps2->emit_scale = ps.emit_scale;
			ps.Append(ps2, i);
		}
	}
};

struct pm_transition1 {
	int		field;
	float	target;
	float	rate;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int		f			= field;
		float	rate_dt		= rate * dt;
		float	target2		= target;
		for (Particle *i = begin; i != end; ++i) {
			((float*)i)[f] = rate_dt < 0
				? max(((float*)i)[f] + rate_dt, target2)
				: min(((float*)i)[f] + rate_dt, target2);
		}
	}
};

struct pm_transition3 {
	int			field;
	float3p		target;
	float		rate;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float	rate_dt		= rate * dt;
		for (Particle *i = begin; i != end; ++i) {
			for (int j = 0; j < 3; j++) {
				float	&t = ((float*)i)[field + j];
				if (t < target[j])
					t = min(t + rate_dt, target[j]);
				else
					t = max(t - rate_dt, target[j]);
			}
		}
	}
};

struct pm_curve1 {
	int			source;
	int			dest;
	float		start, finish;
	float4p		control;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float	scale	= 1.f / (finish - start);
		if (source < 16) {
			float4	b	= bezier_helpers<3>::blend * float4(control);
			float	bx	= b.x, by = b.y, bz = b.z, bw = b.w;
			for (Particle *i = begin; i != end; ++i) {
				prefetch(i + 2);
				float		t	= (((float*)i)[source] - start) * scale;
				((float*)i)[dest] = fsel(t, fsel(1 - t, t * (t * (t * bx + by) + bz) + bw, ((float*)i)[dest]), ((float*)i)[dest]);
			}
		} else {
			float4	b	= transpose((float4x4)bezier_helpers<3>::tangentblend) * float4(control);
			float	by	= b.y, bz = b.z, bw = b.w;
			float	d	= dt * scale;
			for (Particle *i = begin; i != end; ++i) {
				prefetch(i + 2);
				float		t	= (i->life - start) * scale;
				((float*)i)[dest] += fsel(t, fsel(1 - t, (t * (t * by + bz) + bw) * d, 0), 0);
			}
		}
	}
};

struct pm_curve3 {
	int			source;
	int			dest;
	float		start, finish;
	float3p		control[4];

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float	scale		= 1.f / (finish - start);
		float3x4	b		= float3x4(float3x4(control[0],control[1],control[2],control[3]) * bezier_helpers<3>::blend);
		for (Particle *i = begin; i != end; ++i) {
			prefetch(i + 2);
			float		t	= (((float*)i)[source] - start) * scale;
			if (t >= 0 && t <= 1)
				(float3p&)((float*)i)[dest]= b.w + (b.z + (b.y + b.x * t) * t) * t;
		}
	}
};

struct pm_transform1 {
	int			source;
	int			dest;
	float		mult, trans;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (source < 16) {
			for (Particle *i = begin; i != end; ++i)
				((float*)i)[dest]	= ((float*)i)[source] * mult + trans;
		} else {
			float	d			= dt * mult;
			for (Particle *i = begin; i != end; ++i)
				((float*)i)[dest]	+= d;
		}
	}
};

struct pm_sine1 {
	int			source;
	int			dest;
	float		s_scale, d_scale, phase;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (source < 16) {
			for (Particle *i = begin; i != end; ++i) {
				float		t	= ((float*)i)[source] * s_scale + phase;
				((float*)i)[dest]	= iso::sin(t) * d_scale;
			}
		} else {
			float	d_scale = (this->d_scale * s_scale) * dt;
			float	phase	= this->phase - pi / 2;
			for (Particle *i = begin; i != end; ++i) {
				float		t	= i->life * s_scale + phase;
				((float*)i)[dest]	+= iso::sin(t) * d_scale;
			}
		}
	}
};
struct pm_mul1 {
	int			source1, source2;
	int			dest;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int	d = dest, s1 = source1, s2 = source2;
		for (Particle *i = begin; i != end; ++i)
			((float*)i)[d]	= ((float*)i)[s1] * ((float*)i)[s2];
	}
};

struct pm_physics {
	float	gravity;
	float	drag;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		float3	gravity_dt{zero, zero, gravity * dt};
		float	drag_dt		= iso::pow(drag, dt);
		for (Particle *i = begin; i != end; ++i) {
			prefetch(i + 2);
			i->pos		+= i->speed * dt;
			i->speed.xyz = (i->speed.xyz + gravity_dt) * drag_dt;
		}
	}
};

struct pm_plane {
	float4p		plane;
	float		bounce;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		iso::plane	p = iso::plane(float4(this->plane));
		for (Particle *i = begin; i != end; ++i) {
			if (p.dist(position3(i->pos.xyz)) < zero && dot(i->speed.xyz, p.normal()) < zero) {
				if (bounce < 0) {
					ps.Kill(i);
				} else {
					i->speed.xyz = (i->speed.xyz - p.normal() * dot(i->speed.xyz, p.normal()) * 2) * bounce;
				}
			}
		}
	}
};

struct pm_life {
	float	duration;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		if (flags & PM_PROHIBIT_SPAWNING)
			return;

		if (duration) {
			for (Particle *i = begin; i != end; ++i) {
				i->life		+= dt;
				if (i->life > duration)
					ps.Kill(i);
			}
		} else {
			for (Particle *i = begin; i != end; ++i)
				i->life		+= dt;
		}
	}
};

struct pm_ifless {
	int			field;
	float		size;
	pm_list		yes;
	pm_list		isnot;

	struct test {
		int			field;
		float		size;
		test(int _field, float _size) : field(_field), size(_size) {}
		bool operator()(const Particle &p) { return ((float*)&p)[field] < size; }
	};
	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		Particle *middle = partition(begin, end, test(field, size));
		ApplyParticleModifiers(yes, ps, middle, end, dt, flags);
		ApplyParticleModifiers(isnot, ps, begin, middle, dt, flags);
	}
};

struct pm_iflarger {
	int			field;
	float		size;
	pm_list 	yes;
	pm_list 	isnot;

	struct test {
		int			field;
		float		size2;
		test(int _field, float _size) : field(_field), size2(square(_size)) {}
		bool operator()(const Particle &p) { return len2(load<float3>((const float*)&p + field)) > size2; }
	};
	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		Particle *middle = partition(begin, end, test(field, size));
		ApplyParticleModifiers(yes, ps, middle, end, dt, flags);
		ApplyParticleModifiers(isnot, ps, begin, middle, dt, flags);
	}
};

struct pm_partition {
	ISO_openarray<pm_list>		list;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		size_t	nbranch	= list.Count();
		size_t	npart	= end - begin;
		float	f		= float(npart) / float(nbranch);
		int		n		= int(f);
		float	d		= f - n;
		float	e		= ps.rand;
		for (int i = 0; i < nbranch; i++) {
			e = e + d;
			int	n1 = n + int(e);
			e -= int(e);
			ApplyParticleModifiers(list[i], ps, begin, begin + n1, dt, flags);
			begin += n1;
		}
	}
};

struct pm_timesplit {
	ISO_openarray<pair<float, pm_list> >		list;

	struct test {
		float	scale, value;
		test(float total, float time) : scale(reciprocal(total)), value(time * scale) {}
		bool operator()(const Particle &p) { return frac(p.life * scale) < value; }
	};

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		int		nbranch	= list.Count();
		float	total	= 0;
		for (int i = 0; i < nbranch; i++)
			total += list[i].a;

		float	t		= 0;
		for (int i = 0; i < nbranch - 1; i++) {
			t += list[i].a;
			Particle	*middle = partition(begin, end, test(total, t));
			ApplyParticleModifiers(list[i].b, ps, middle, end, dt, flags);
			end		= middle;
		}
		ApplyParticleModifiers(list[nbranch - 1].b, ps, begin, end, dt, flags);
	}
};

struct pm_attime {
	float		time;
	pm_list		list;

	force_inline void Apply(ParticleSet &ps, Particle *begin, Particle *end, float dt, ParticleModifierFlags flags) const {
		for (Particle *i = begin; i != end; ++i) {
			if (i->life < time && i->life + dt >= time)
				ApplyParticleModifiers(list, ps, i, i + 1, i->life - time, flags);
		}
	}
};

//-----------------------------------------------------------------------------
//	ISO_defs
//-----------------------------------------------------------------------------

ISO_DEFUSERCOMPV(offset_texture, t, x, y);
ISO_DEFUSERCOMPV(normal_mapped, diffuse, normal, glossiness, x, y);
ISO_DEFUSERCOMPV(custom_particle, technique, params, x, y);
ISO_DEFUSERCOMPV(particle_effect, particle, maxparticles, mode, modifiers);

template<> struct ISO::def<em_die> : public pm_user_comp<em_die,0> {
	def() : pm_user_comp<em_die,0>("em_die")
	{}
};

template<> struct ISO::def<em_emit> : public pm_user_comp<em_emit,2> {
	def() : pm_user_comp<em_emit,2>("em_emit") {
		comp.ISO_SETFIELDS(0,num_per_second, initialisers);
	}
};

template<> struct ISO::def<em_ifbefore> : public pm_user_comp<em_ifbefore,3> {
	def() : pm_user_comp<em_ifbefore,3>("em_ifbefore") {
		comp.ISO_SETFIELDS(0,time, yes, isnot);
	}
};

template<> struct ISO::def<em_ifless> : public pm_user_comp<em_ifless,3> {
	def() : pm_user_comp<em_ifless,3>("em_ifless") {
		comp.ISO_SETFIELDS(0,num, yes, isnot);
	}
};

template<> struct ISO::def<em_attime> : public pm_user_comp<em_attime,2> {
	def() : pm_user_comp<em_attime,2>("em_attime") {
		comp.ISO_SETFIELDS(0,time, list);
	}
};

template<> struct ISO::def<em_spawn> : public pm_user_comp<em_spawn,2> {
	def() : pm_user_comp<em_spawn,2>("em_spawn") {
		comp.ISO_SETFIELDS(0,effect, offset);
	}
};

template<> struct ISO::def<em_flash> : public pm_user_comp<em_flash,3> {
	def() : pm_user_comp<em_flash,3>("em_flash") {
		comp.ISO_SETFIELDS(0,col, time, radius);
	}
};

template<> struct ISO::def<pm_kill> : public pm_user_comp<pm_kill,0> {
	def() : pm_user_comp<pm_kill,0>("pm_kill") {}
};

template<> struct ISO::def<pm_set1> : public pm_user_comp<pm_set1,2> {
	def() : pm_user_comp<pm_set1,2>("pm_set1") {
		comp.ISO_SETFIELDS(0,field, value);
	}
};

template<> struct ISO::def<pm_set3> : public pm_user_comp<pm_set3,2> {
	def() : pm_user_comp<pm_set3,2>("pm_set3") {
		comp.ISO_SETFIELDS(0,field, value);
	}
};

template<> struct ISO::def<pm_random1> : public pm_user_comp<pm_random1,3> {
	def() : pm_user_comp<pm_random1,3>("pm_random1") {
		comp.ISO_SETFIELDS(0,field, low, high);
	}
};

template<> struct ISO::def<pm_random3> : public pm_user_comp<pm_random3,3> {
	def() : pm_user_comp<pm_random3,3>("pm_random3") {
		comp.ISO_SETFIELDS(0,field, low, high);
	}
};

template<> struct ISO::def<pm_pos2vel> : public pm_user_comp<pm_pos2vel,4> {
	def() : pm_user_comp<pm_pos2vel,4>("pm_pos2vel") {
		comp.ISO_SETFIELDS(0,scale_low, scale_high, angle_low, angle_high);
	}
};

template<> struct ISO::def<pm_sphere> : public pm_user_comp<pm_sphere,5> {
	def() : pm_user_comp<pm_sphere,5>("pm_sphere") {
		comp.ISO_SETFIELDS(0,field, centre, dir, spread, thickness);
	}
};

template<> struct ISO::def<pm_disc> : public pm_user_comp<pm_disc,4> {
	def() : pm_user_comp<pm_disc,4>("pm_disc") {
		comp.ISO_SETFIELDS(0,field, centre, dir, thickness);
	}
};

template<> struct ISO::def<pm_spawn> : public pm_user_comp<pm_spawn,2> {
	def() : pm_user_comp<pm_spawn,2>("pm_spawn") {
		comp.ISO_SETFIELDS(0,effect, offset);
	}
};

template<> struct ISO::def<pm_transition1> : public pm_user_comp<pm_transition1,3> {
	def() : pm_user_comp<pm_transition1,3>("pm_transition1") {
		comp.ISO_SETFIELDS(0,field, target, rate);
	}
};

template<> struct ISO::def<pm_transition3> : public pm_user_comp<pm_transition3,3> {
	def() : pm_user_comp<pm_transition3,3>("pm_transition3") {
		comp.ISO_SETFIELDS(0,field, target, rate);
	}
};

template<> struct ISO::def<pm_curve1> : public pm_user_comp<pm_curve1,5> {
	def() : pm_user_comp<pm_curve1,5>("pm_curve1") {
		comp.ISO_SETFIELDS(0,source, dest, start, finish, control);
	}
};

template<> struct ISO::def<pm_curve3> : public pm_user_comp<pm_curve3,5> {
	def() : pm_user_comp<pm_curve3,5>("pm_curve3") {
		comp.ISO_SETFIELDS(0,source, dest, start, finish, control);
	}
};

template<> struct ISO::def<pm_transform1> : public pm_user_comp<pm_transform1,4> {
	def() : pm_user_comp<pm_transform1,4>("pm_transform1") {
		comp.ISO_SETFIELDS(0,source, dest, mult, trans);
	}
};

template<> struct ISO::def<pm_sine1> : public pm_user_comp<pm_sine1,5> {
	def() : pm_user_comp<pm_sine1,5>("pm_sine1") {
		comp.ISO_SETFIELDS(0,source, dest, s_scale, d_scale, phase);
	}
};

template<> struct ISO::def<pm_mul1> : public pm_user_comp<pm_mul1,3> {
	def() : pm_user_comp<pm_mul1,3>("pm_mul1") {
		comp.ISO_SETFIELDS(0,source1, source2, dest);
	}
};

template<> struct ISO::def<pm_physics> : public pm_user_comp<pm_physics,2> {
	def() : pm_user_comp<pm_physics,2>("pm_physics") {
		comp.ISO_SETFIELDS(0,gravity, drag);
	}
};

template<> struct ISO::def<pm_plane> : public pm_user_comp<pm_plane,2> {
	def() : pm_user_comp<pm_plane,2>("pm_plane") {
		comp.ISO_SETFIELDS(0,plane, bounce);
	}
};

template<> struct ISO::def<pm_life> : public pm_user_comp<pm_life,1> {
	def() : pm_user_comp<pm_life,1>("pm_life") {
		comp.ISO_SETFIELD(0,duration);
	}
};

template<> struct ISO::def<pm_ifless> : public pm_user_comp<pm_ifless,4> {
	def() : pm_user_comp<pm_ifless,4>("pm_ifless") {
		comp.ISO_SETFIELDS(0,field, size, yes, isnot);
	}
};

template<> struct ISO::def<pm_iflarger> : public pm_user_comp<pm_iflarger,4> {
	def() : pm_user_comp<pm_iflarger,4>("pm_iflarger") {
		comp.ISO_SETFIELDS(0,field, size, yes, isnot);
	}
};

template<> struct ISO::def<pm_partition> : public pm_user_comp<pm_partition,1> {
	def() : pm_user_comp<pm_partition,1>("pm_partition") {
		comp.ISO_SETFIELD(0,list);
	}
};

template<> struct ISO::def<pm_timesplit> : public pm_user_comp<pm_timesplit,1> {
	def() : pm_user_comp<pm_timesplit,1>("pm_timesplit") {
		comp.ISO_SETFIELD(0,list);
	}
};

template<> struct ISO::def<pm_attime> : public pm_user_comp<pm_attime,2> {
	def() : pm_user_comp<pm_attime,2>("pm_attime") {
		comp.ISO_SETFIELDS(0,time, list);
	}
};

initialise particle_defs(
	ISO::getdef<offset_texture>(),
	ISO::getdef<normal_mapped>(),
	ISO::getdef<custom_particle>(),
	ISO::getdef<particle_effect>(),

	ISO::getdef<em_die>(),
	ISO::getdef<em_emit>(),
	ISO::getdef<em_ifbefore>(),
	ISO::getdef<em_ifless>(),
	ISO::getdef<em_attime>(),
	ISO::getdef<em_spawn>(),
	ISO::getdef<em_flash>(),

	ISO::getdef<pm_kill>(),
	ISO::getdef<pm_set1>(),
	ISO::getdef<pm_set3>(),
	ISO::getdef<pm_random1>(),
	ISO::getdef<pm_random3>(),
	ISO::getdef<pm_pos2vel>(),
	ISO::getdef<pm_spawn>(),
	ISO::getdef<pm_sphere>(),
	ISO::getdef<pm_disc>(),
	ISO::getdef<pm_transition1>(),
	ISO::getdef<pm_transition3>(),
	ISO::getdef<pm_curve1>(),
	ISO::getdef<pm_curve3>(),
	ISO::getdef<pm_transform1>(),
	ISO::getdef<pm_sine1>(),
	ISO::getdef<pm_mul1>(),
	ISO::getdef<pm_physics>(),
	ISO::getdef<pm_plane>(),
	ISO::getdef<pm_life>(),
	ISO::getdef<pm_ifless>(),
	ISO::getdef<pm_iflarger>(),
	ISO::getdef<pm_partition>(),
	ISO::getdef<pm_timesplit>(),
	ISO::getdef<pm_attime>()
);

//-----------------------------------------------------------------------------
//	ParticleRenderer
//-----------------------------------------------------------------------------

class ParticleRenderer {
protected:
	void (*vRender)(void *t, RenderEvent *re, ParticleSet &ps, param(iso::float3x4) mat, param(iso::float3x4) view, param(iso::float4x4) proj, float alpha);
	void (*vDestruct)(void *t);
public:
	void Render(RenderEvent *re, ParticleSet &ps, param(iso::float3x4) mat, param(iso::float3x4) view, param(iso::float4x4) proj, float alpha) {
		vRender(this, re, ps, mat, view, proj, alpha);
	}
	~ParticleRenderer() { if (void (*f)(void*) = vDestruct) { vDestruct = 0; f(this); } }
};

template<typename T> class ParticleRendererT : public ParticleRenderer {
	static void tRender(void *t, RenderEvent *re, ParticleSet &ps, param(iso::float3x4) mat, param(iso::float3x4) view, param(iso::float4x4) proj, float alpha) {
		((T*)t)->Render(re, ps, mat, view, proj, alpha);
	}
	static void tDestruct(void *t) {
		((T*)t)->~T();
	}
public:
	ParticleRendererT() { vRender = &tRender; vDestruct = &tDestruct; }
};

//-----------------------------------------------------------------------------
//	ParticleRenderer2D
//-----------------------------------------------------------------------------

class ParticleRenderer2D : public ParticleRendererT<ParticleRenderer2D> {
	Texture					*texture;
	Texture					*normal;
	float					glossiness;
	float					layerscaler;
	float					offx, offy;
	ShaderConstants			sc;
public:
	void	Render(RenderEvent *re, ParticleSet &ps, param(float3x4) mat, param(float3x4) view, param(float4x4) proj, float alpha);

	ParticleRenderer2D(Texture *_texture) {
		layerscaler	= 1.f / _texture->Depth();
		offx		= 0;
		offy		= 0;
		texture		= _texture;
		normal		= NULL;
	}
	ParticleRenderer2D(offset_texture *p) {
		layerscaler	= 1.f / p->t.Depth();
		offx		= p->x;
		offy		= p->y;
		texture		= &p->t;
		normal		= NULL;
	}
	ParticleRenderer2D(normal_mapped *p) {
		layerscaler	= 1.f / p->diffuse.Depth();
		offx		= p->x;
		offy		= p->y;
		glossiness	= p->glossiness;
		texture		= &p->diffuse;
		normal		= &p->normal;
	}
};

namespace iso {

#ifdef SOFTWARE_PARTICLES

#ifdef PLAT_WII
struct ParticleVertex {
	float3p	pos;
	rgba8	col;
	float2p	uv;
};
#else
struct ParticleVertex {
	float3p	pos;
	float3p	uv;
	float4p	col;
};
#endif

ParticleVertex *PutQuad(QuadList<ParticleVertex> p, param(position3) pos, param(float3) dx, param(float3) dy, param(colour) col, float im, float layerscaler) {
	p[0].pos = pos - dx - dy;
	p[1].pos = pos + dx - dy;
	p[2].pos = pos - dx + dy;
	p[3].pos = pos + dx + dy;

#ifdef PLAT_WII
	float	v	= floor(im) * layerscaler;
	p[0].uv.set(0, v + layerscaler);
	p[1].uv.set(1, v + layerscaler);
	p[2].uv.set(0, v);
	p[3].uv.set(1, v);
#else
	p[0].uv = {0, 1, im};
	p[1].uv = {1, 1, im};
	p[2].uv = {0, 0, im};
	p[3].uv = {1, 0, im};
#endif
	p[0].col = p[1].col = p[2].col = p[3].col = col.rgba;
	return p.next();
}

template<> static const VertexElements ve<ParticleVertex> = (const VertexElement[]) {
	{&ParticleVertex::pos,	"position"_usage},
	{&ParticleVertex::uv,	"texcoord0"_usage},
	{&ParticleVertex::col,	"color"_usage},
	{0, GetComponentType<float[3]>(),	"normal"_usage},
	{0, GetComponentType<float[4]>(),	"tangent"_usage},
};

#elif defined(PLAT_X360) || defined(USE_DX11) || defined(USE_DX12) || defined(PLAT_PS4)
template<> static const VertexElements ve<Particle> = (const VertexElement[]) {
	{&Particle::pos,		"position"_usage},
	{&Particle::col,		"texcoord0"_usage},
	{&Particle::params,		"texcoord1"_usage},
	{&Particle::speed,		"texcoord2"_usage},
};
#elif defined(PLAT_PS3)
template<> static const VertexElements *ve<Particle> = (const VertexElement[]) {
	{&Particle::pos,		USAGE_POSITION},
	{&Particle::col,		USAGE_TEXCOORD, 0},
	{&Particle::params,		USAGE_TEXCOORD, 1},
	{&Particle::speed,		USAGE_TEXCOORD, 2},
	MakeVE<float[2]>(0,		USAGE_TEXCOORD,	7, 1),
};

VertexBuffer<float[2]> &GetCornerVB() {
	static float corners[][2] = {
		{-1, -1},
		{ 1, -1},
		{ 1,  1},
		{-1,  1}
	};
	static VertexBuffer<float[2]> vb_corners(corners, MEM_HOST);
	return vb_corners;
}
#endif
}

void ParticleRenderer2D::Render(RenderEvent *re, ParticleSet &ps, param(float3x4) mat, param(float3x4) view, param(float4x4) proj, float alpha) {
	GraphicsContext	&ctx = re->ctx;

	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetDepthWriteEnable(false);
#ifdef PLAT_X360
	ctx.SetUVMode(0, ALL_CLAMP);
#endif
	re->consts.worldView		= mat;
	re->consts.worldViewProj	= proj * mat;
	re->consts.tint			= colour(ps.mode & PS_TINT ? float3(re->consts.average.rgb) : float3(one), ps.alpha * alpha);

	AddShaderParameter(ISO_CRC("diffuse_samp", 0xe31becbe),	*texture);
	particle_system.layerscaler = layerscaler;
	if (normal)
		AddShaderParameter(ISO_CRC("normal_samp", 0x840f17cf), *normal);
	int		orient = ps.mode & PS_ORIENT;

#ifdef SOFTWARE_PARTICLES
	Set(ctx, (*particle_system.shaders->textured_col)[0]);
	{
		ImmediateStream<ParticleVertex>	ims(ctx, prim<QuadList>(), verts<QuadList>(ps.NumParts()));
		ParticleVertex	*p	= ims.begin();
		float3x3		tm	= transpose(float3x3(mat));

		if (orient == PS_ORIENT_FLAT || orient == PS_ORIENT_AS_IS) {
			float3			fx	= orient == PS_ORIENT_FLAT ? tm.x : y_axis;
			float3			fy	= orient == PS_ORIENT_FLAT ? -tm.y : x_axis;
			for (Particle *i = ps.Begin(); i != ps.End(); ++i) {
				float2		sc	= sincos(degrees(i->pos.w));

				float3		dx	= (fx *  sc.x + fy * sc.y) * i->width;
				float3		dy	= (fx * -sc.y + fy * sc.x) * i->height;
				p = PutQuad(p, position3(i->pos.xyz) + dx * offx + dy * offy, dx, dy, i->col, i->image, layerscaler);
			}
		} else {
			uint32	field	= orient == PS_ORIENT_VELOCITY ? 3 : 0;
			float3	z		= ps.mode & PS_ALONG_Z ? z_axis : view.z;
			for (Particle *i = ps.Begin(); i != ps.End(); ++i) {
				float3	o	= ((float3*)i)[field];
				if (i->pos.w != zero) {
					float2	sc		= sincos(degrees(i->pos.w));
					float	lenx	= len(o.xy);
					o = concat(o.xy * (sc.x * lenx + sc.y * o.z), (-sc.y * lenx + sc.x * o.z) * lenx);
				}
#if 0
				float3		x	= normalise(mat * o);
				float3		y	= normalise(float3(-x.y, x.x, zero));
				float3		dx	= tm * x * i->width;
				float3		dy	= tm * y * i->height;
#else
				float3		x	= normalise(o);
				float3		y	= normalise(cross(z, x));
				float3		dx	= x * i->width;
				float3		dy	= y * i->height;
#endif
				p = PutQuad(p, position3(i->pos.xyz) + dx * offx + dy * offy, dx, dy, i->col, i->image, layerscaler);
			}
		}
	}

#else

	particle_system.params = float4{offx, offy, layerscaler, (float)orient};

	technique *t;
	switch (orient) {
		case PS_ORIENT_FLAT: {
			float3x3		tm	= transpose(float3x3(mat));
			particle_system.fx =  tm.x;
			particle_system.fy = -tm.y;
			t = normal ? particle_system.shaders->particles_norm : ps.mode & PS_SOFT ? particle_system.shaders->particles_soft : particle_system.shaders->particles;
			break;
		}
		case PS_ORIENT_AS_IS:
			particle_system.fx = float3{0, 1, 0};
			particle_system.fy = float3{1, 0, 0};
			t = normal ? particle_system.shaders->particles_norm : ps.mode & PS_SOFT ? particle_system.shaders->particles_soft : particle_system.shaders->particles;
			break;
		default:
			particle_system.fx = ps.mode & PS_ALONG_Z ? float3{zero, zero, one} : view.z;
			t = normal ? particle_system.shaders->particles_norm2 : ps.mode & PS_SOFT ? particle_system.shaders->particles_soft2 : particle_system.shaders->particles2;
			break;
	}
#ifdef PLAT_X360
	uint32 stride = sizeof(Particle) / 4;
	sc.InitSet(ctx, (*t)[0], ISO::Browser(), &stride, GetVE<Particle>());
	graphics.Device()->SetVertexDeclaration(NULL);
	graphics.Device()->DrawVerticesUP(D3DPT_QUADLIST, ps.NumParts() * 4, ps.Begin(), sizeof(Particle) / 4);
#elif defined(PLAT_PS3)
	sc.InitSet(ctx, (*t)[0], ISO::Browser());
	ctx.SetVertices(0, ps.Begin(), sizeof(Particle), 0, 4);
	ctx.SetVertices(1, GetCornerVB(), 0, 4);
	cellGcmSetFrequencyDividerOperation(ctx.GetContext(), 0x8000);
	ctx.DrawPrimitive(PRIM_QUADLIST, 0, ps.NumParts());
#elif defined(PLAT_PC)
	sc.InitSet(ctx, (*t)[0], ISO::Browser());
	ctx.SetVertexType<Particle>();
	VertexBuffer<Particle>	vb(ps.Begin(), ps.NumParts());
	ctx.SetVertices(0, vb);
	ctx.DrawPrimitive(PRIM_POINTLIST, 0, ps.NumParts());
#elif defined(PLAT_PS4)
	sc.InitSet(ctx, (*t)[0], ISO::Browser());
	VertexBuffer<Particle>	vb(ps.Begin(), ps.NumParts());
	ctx.SetVertices(0, vb);
	ctx.DrawPrimitive(PRIM_POINTLIST, 0, ps.NumParts());
#endif

#endif

#ifdef PLAT_X360
	ctx.SetUVMode(0, ALL_WRAP);
#endif
	ctx.SetBackFaceCull(BFC_BACK);
	re->consts.tint = colour(one);
}
//-----------------------------------------------------------------------------
//	ParticleRendererCustom
//-----------------------------------------------------------------------------

class ParticleRendererCustom : public ParticleRendererT<ParticleRendererCustom> {
	custom_particle		*c;
	ShaderConstants		sc;
public:
	void Render(RenderEvent *re, ParticleSet &ps, param(float3x4) mat, param(float3x4) view, param(float4x4) proj, float alpha);
	ParticleRendererCustom(custom_particle *_c) : c(_c) {}
};

void ParticleRendererCustom::Render(RenderEvent *re, ParticleSet &ps, param(float3x4) mat, param(float3x4) view, param(float4x4) proj, float alpha) {
	GraphicsContext	&ctx = re->ctx;
	ctx.SetBackFaceCull(BFC_NONE);
#ifdef PLAT_X360
	ctx.SetUVMode(0, ALL_CLAMP);
#endif
	re->consts.worldView		= mat;
	re->consts.worldViewProj	= proj * mat;
	particle_system.layerscaler = 1;

	int			orient = ps.mode & PS_ORIENT;

#ifdef SOFTWARE_PARTICLES
	sc.InitSet(ctx, (*c->technique)[0], ISO::Browser(c->params));
	{
		ImmediateStream<ParticleVertex>	ims(ctx, prim<QuadList>(), verts<QuadList>(ps.NumParts()));
		ParticleVertex	*p	= ims.begin();
		if (orient == PS_ORIENT_FLAT || orient == PS_ORIENT_AS_IS) {
			float3x3		tm	= transpose(float3x3(mat));
			float3			fx	= orient == PS_ORIENT_FLAT ? tm.x : x_axis;
			float3			fy	= orient == PS_ORIENT_FLAT ? tm.y : y_axis;
			for (Particle *i = ps.Begin(); i != ps.End(); ++i) {
				float2		sc	= sincos(degrees(i->pos.w));
				float3		dx	= (fx *  sc.x - fy * sc.y) * i->width;
				float3		dy	= (fx * -sc.y - fy * sc.x) * i->height;
				p = PutQuad(p, position3(i->pos.xyz), dx, dy, i->col, i->image, 1);
			}
		} else {
			uint32	field	= orient == PS_ORIENT_VELOCITY ? 3 : 0;
			for (Particle *i = ps.Begin(); i != ps.End(); ++i) {
				float3	o	= ((float3*)i)[field];
				if (i->pos.w != zero) {
					float2	sc		= sincos(degrees(i->pos.w));
					float	lenx	= len(o.xy);
					o = concat(o.xy * (sc.x * lenx + sc.y * o.z), (-sc.y * lenx + sc.x * o.z) * lenx);
				}
				float3		x	= normalise(mat * o);
				float3		y	= normalise(cross(z_axis, x));
				float3x3	tm	= transpose(float3x3(mat));
				float3		dx	= tm * x * i->width;
				float3		dy	= tm * y * i->height;
				p = PutQuad(p, position3(i->pos.xyz), dx, dy, i->col, i->image, 1);
			}
		}
	}
#else
	particle_system.params = float4{c->x, c->y, 1, (float)orient};
	switch (orient) {
		case PS_ORIENT_FLAT: {
			float3x3		tm	= transpose(float3x3(mat));
			particle_system.fx =  tm.x;
			particle_system.fy = -tm.y;
			break;
		}
		case PS_ORIENT_AS_IS:
			particle_system.fx = float3{0, 1, 0};
			particle_system.fy = float3{1, 0, 0};
			break;
		default:
			particle_system.fx = ps.mode & PS_ALONG_Z ? float3{zero, zero, one} : view.z;
			break;
	}
#ifdef PLAT_X360
	uint32 stride = sizeof(Particle) / 4;
	sc.InitSet(ctx, (*c->technique)[0], ISO::Browser(c->params), &stride, GetVE<Particle>());
	graphics.Device()->SetVertexDeclaration(NULL);
	graphics.Device()->DrawVerticesUP(D3DPT_QUADLIST, ps.NumParts() * 4, ps.Begin(), sizeof(Particle) / 4);
#elif defined(PLAT_PS3)
	sc.InitSet(ctx, (*c->technique)[0], ISO::Browser(c->params));
	ctx.SetVertices(0, ps.Begin(), 0, 4);
	ctx.SetVertices(1, GetCornerVB(), 0, 4);
	cellGcmSetFrequencyDividerOperation(ctx.GetContext(), 0x8000);
	ctx.DrawPrimitive(PRIM_QUADLIST, 0, ps.NumParts());
#endif

#endif

#ifdef PLAT_X360
	ctx.SetUVMode(0, ALL_WRAP);
#endif
	ctx.SetBackFaceCull(BFC_BACK);
}

//-----------------------------------------------------------------------------
//	ParticleRenderer3D
//-----------------------------------------------------------------------------

class ParticleRenderer3D : public ParticleRendererT<ParticleRenderer3D> {
	ISO_ptr<Model3> model;
public:
	void Render(RenderEvent *re, ParticleSet &ps, param(float3x4) mat, param(float3x4) view, param(float4x4) proj, float alpha);
	ParticleRenderer3D(ISO_ptr<Model3> _model) : model(_model) {
	}
};

void ParticleRenderer3D::Render(RenderEvent *re, ParticleSet &ps, param(float3x4) mat, param(float3x4) view, param(float4x4) proj, float alpha) {
	GraphicsContext	&ctx = re->ctx;
	int		mode		= ps.mode & PS_ORIENT;
	bool	batched		= IsBatched(model);

	ctx.SetBlendEnable(true);
	ctx.SetBackFaceCull(BFC_BACK);

	float3x4	mat2		= inverse(view) * mat;

	for (Particle *i = ps.Begin(); i != ps.End(); ++i) {
		re->consts.world			= translate(position3(i->pos.xyz))
			* (mode ? look_along_z((mode==PS_ORIENT_VELOCITY ? i->speed.xyz : i->pos.xyz)) : float3x4(identity))
			* rotate_in_x(degrees(i->pos.w))
			* scale(float3{i->width, i->width, i->height});
		re->consts.world			= mat2 * re->consts.world;
		if (batched) {
			AddBatch(model, float3x4(re->consts.world));
		} else {
			re->consts.worldView		= view * re->consts.world;
			re->consts.worldViewProj	= proj * re->consts.worldView;
			re->consts.tint			= colour(i->col.rgb, i->col.a * ps.alpha * alpha);
			Draw(ctx, model);
		}
	}
	if (batched)
		DrawBatches(0);
	re->consts.tint = colour(one);
}

//-----------------------------------------------------------------------------
//	ParticleSet
//-----------------------------------------------------------------------------

cuboid ParticleSet::GetExtent() {
	cuboid	box;
	if (num_parts) {
		Particle	*p = particles;
		position3	pos	= position3(p->pos.xyz);
		float		s	= max(p->width, p->height);
		position3	a	= pos - s, b = pos + s;
		p++;
		for (int n = num_parts - 1; n--; p++) {
			s = max(s, max(p->width, p->height));
			a = min(a, pos - s);
			b = max(b, pos + s);
		}

		extent = cuboid(a, b);

		if ((mode & PS_SPACE) == PS_RELATIVE_SPACE)
			box = (mat * extent).get_box();
		else
			box = cuboid(position3(extent.a.v * scale), position3(extent.b.v * scale));
	} else {
		box = empty;
	}

	for (e_list<ParticleSet>::iterator i = dependants.begin(); i != dependants.end(); ++i)
		box |= i->GetExtent();

	return box;
}

void ParticleSet::Kill(Particle *p) {
	*(Particle**)&p->life	= kill;
	kill					= p;
	num_killed++;
}

bool ParticleSet::CheckVisible(param(float4x4) viewProj) {
	return is_visible(GetExtent(), viewProj);
}

void ParticleSet::Append(ParticleSet *ps, Particle *i)
{
	ps->dependancy	= i;
	dependants.push_back(ps);
}

void ParticleSet::Emit(float n) {
	emit_leftover += n;
	for (e_list<ParticleSet>::iterator i = dependants.begin(); i != dependants.end(); ++i)
		i->Emit(n);
}

void ParticleSet::StopEmitting(bool recurse) {
	emit_leftover = -1;
	if (mode & PS_INSTANT_OFF)
		num_parts = 0;

	if (recurse) {
		for (e_list<ParticleSet>::iterator i = dependants.begin(); i != dependants.end(); ++i)
			i->StopEmitting(true);
	}
}

void ParticleSet::ScaleEmitRate(float scale) {
	if (scale != emit_scale) {
		emit_scale = scale;
		for (e_list<ParticleSet>::iterator i = dependants.begin(); i != dependants.end(); ++i)
			i->ScaleEmitRate(scale);
	}
}

bool ParticleSet::Update(float time) {
//	PROFILE_CPU_EVENT("Particle");
	PROFILE_CPU_EVENT(GetLabel(id));

	prev_life	= life;
	life		= time - start_time;
	kill		= NULL;
	num_killed	= 0;
	die			= num_parts == 0;
	Particle* end = particles + num_parts;
	ApplyParticleModifiers(modifiers, *this, particles, particles + num_parts, life - prev_life, (ParticleModifierFlags)0);

	// if new particles were spawned, update them part of the dt so they get spread out and aren't bunched together...but don't update anything that will spawn new particles
	if (particles && particles + num_parts > end) {
		for (Particle *p = end ? end : particles; p < particles + num_parts; p++) {
			if (float time = p->life)
				ApplyParticleModifiers(modifiers, *this, p, p + 1, time, PM_PROHIBIT_SPAWNING);
		}
	}

	for (e_list<ParticleSet>::iterator i = dependants.begin(); i != dependants.end(); ) {
		float3x4	&dmat = i->mat;
		if (Particle *part = i->dependancy) {
			dmat = mat;
			if ((mode & PS_SPACE) == PS_RELATIVE_SPACE)
				dmat.w = mat * position3(part->pos.xyz);
			else
				dmat.w = position3(part->pos.xyz);
			int	k = part - particles - num_parts + num_killed;
			if (k >= 0) {
				Particle *p = kill;
				while (k-- && p)
					p	= *(Particle**)&p->life;
				if (p)
					i->dependancy = p;
				else
					i->StopEmitting(false);
			}
		} else {
//			dmat = mat * translate(prev_mat.inverse() * dmat.translation());
			dmat = mat * i->rel_mat;
		}
		if (i->Update(time)) {
			die = false;
			++i;
		} else {
			delete (i++).get();
		}
	}
	prev_mat	= mat;

	for (Particle *p = kill, *n; p; p = n) {
		n	= *(Particle**)&p->life;
		*p	= particles[--num_parts];
	}

	return !die;
}

void ParticleSet::Render(RenderEvent *re, param(float3x4) view, param(float4x4) proj, float alpha) {
	if (!dependants.empty() || (pr && NumParts())) {
		PROFILE_EVENT(re->ctx, GetLabel(id));
		if (pr && NumParts()) {
			float3x4	mat2;
			switch (mode & PS_SPACE) {
				case PS_RELATIVE_SPACE:		mat2 = view * mat; break;
				case PS_WORLD_SPACE:		mat2 = view * iso::scale(scale); break;
				case PS_SCREEN_SPACE:		mat2 = translate(get_trans(view)) * mat; break;
				case PS_SCREENPOS_SPACE:	mat2 = mat; break;//identity; break;
			}
			pr->Render(re, *this, mat2, view, proj, alpha);
		}

		for (e_list<ParticleSet>::iterator i = dependants.begin(); i != dependants.end(); ++i)
			i->Render(re, view, proj, this->alpha * alpha);
	}
}

void ParticleSet::Init(crc32 _id, const particle_effect *pe, param(float3x4) _mat) {
	id			= _id;
	mat			= prev_mat = _mat;
	scale		= get_scale(mat);
	modifiers	= pe->modifiers;
	mode		= pe->mode;
	pr			= NULL;
	rand = particle_system.random;

	if (max_parts = pe->maxparticles)
		particles = AllocParticles(max_parts = pe->maxparticles);//new Particle[max_parts = pe->maxparticles];

	if (pe->particle) {
		if (pe->particle.IsType<Model3>())
			pr = new ParticleRenderer3D(pe->particle);

		else if (pe->particle.IsType<Texture>())
			pr = new ParticleRenderer2D((Texture*)pe->particle);

		else if (pe->particle.IsType<offset_texture>())
			pr = new ParticleRenderer2D((offset_texture*)pe->particle);

		else if (pe->particle.IsType<normal_mapped>())
			pr = new ParticleRenderer2D((normal_mapped*)pe->particle);

		else if (pe->particle.IsType<custom_particle>())
			pr = new ParticleRendererCustom(pe->particle);

	}
}

ParticleSet::ParticleSet(float time)
	: pr(NULL), particles(NULL)
	, dependancy(NULL)
	, num_parts(0), max_parts(0)
	, start_time(time), life(0)
	, emit_leftover(0.9999f), emit_scale(1.0f)
	, alpha(1.0f)
{
	CTOR_RETURN
}

ParticleSet::~ParticleSet() {
	FreeParticles(particles, max_parts);
	delete pr;
	dependants.deleteall();
}

//void deleter(ParticleSet *ps) {
//	if (ps)
//		delete ps;
//}

//-----------------------------------------------------------------------------
//	ParticleHolder
//-----------------------------------------------------------------------------

ParticleHolder::ParticleHolder(World *world, crc32 id, const particle_effect *pe, param(float3x4) _mat, Object *_obj, crc32 _bone)
	: DeleteOnDestroy(_obj ? _obj : world), obj(_obj)
	, bone(Pose::INVALID)
	, mat(_mat)
	, calc_extent_interleave(0)
{
	if (obj) {
		obj->SetHandler<QueryTypeMessage>(this);
		if (_bone) {
			if (Pose *pose = obj->Property<Pose>())
				bone = pose->Find(_bone);
		}
	}
	ps = new ParticleSet(world->Time());
	ps->Init(id, pe, GetMatrix());
	CTOR_RETURN
}

ParticleHolder::~ParticleHolder() {
	delete ps;
}

void ParticleHolder::operator()(FrameEvent2 &ev) {
	if (ps) {
		if (obj)
			ps->SetMatrix(GetMatrix());
		if (!ps->Update(ev.time))
			delete this;
	}
}

void ParticleHolder::operator()(RenderEvent &re) {
	if (ps && !re.Excluded(RMASK_NOSHADOW)) {
		if (calc_extent_interleave <= 0 || (ps->mode & PS_DONT_INTERLEAVE_AABB_CALC)) {
			calc_extent_interleave = PARTICLES_CALC_EXTENT_INTERLEAVE;
			obj_extent = ps->GetExtent();
			inv_extent_mat = inverse((float4x4)ps->GetMatrix());
		} else {
			calc_extent_interleave--;
		}

		if (obj_extent.empty())
			extent = obj_extent;
		else
			extent = ((float3x4)(ps->GetMatrix() * inv_extent_mat) * obj_extent).get_box();

		RenderParameters params;
//		if (render_event)
//			render_event->go(RenderMessage(params, re.Time()).me());
		if (obj)
			obj->Send(RenderMessage(params, re.Time()));

		if (params.opacity != zero && is_visible(extent, re.consts.viewProj0)) {
			position3	c = ps->extent.centre();
			if ((ps->mode & PS_SPACE) == PS_RELATIVE_SPACE)
				c = ps->mat * c;
			float d = float3(re.consts.view * c).z;
			re.AddRenderItem(this, MakeKey(particles::STAGE, -d), iorf(params.opacity).i());
		}
	}
}

void ParticleHolder::operator()(RenderEvent *re, uint32 extra) {
	PROFILE_EVENT(re->ctx, "Particle");
	ps->Render(re, re->consts.view, re->consts.proj, iorf(extra).f());
}

void ParticleHolder::operator()(QueryTypeMessage &msg) {
	msg.set(ISO_CRC("TYPE_PARTICLE_HOLDER", 0xa8a21a2f), this);
}

void _StopEmitting(crc32 type, arbitrary data) {
	if (type == ISO_CRC("TYPE_PARTICLE_HOLDER", 0xa8a21a2f)) {
		if (ParticleSet *ps = data.as<ParticleHolder*>()->GetParticleSet())
			ps->StopEmitting(true);
	}
}
void ParticleHolder::StopEmitting(iso::Object *obj) {
	QueryTypeMessage(_StopEmitting).send(obj);
}

namespace iso {
	template<> void TypeHandler<particle_effect>::Create(const CreateParams &cp, crc32 id, const particle_effect *t) {
		new ParticleHolder(cp.world, id, t, cp.matrix, cp.obj, cp.bone);
	}
	static TypeHandler<particle_effect> thParticleEffect;
}

//-----------------------------------------------------------------------------
//	ScreenFlash
//-----------------------------------------------------------------------------

void ScreenFlash::operator()(RenderEvent &re) {
	if (!re.Excluded(RMASK_NOSHADOW)) {
		float	time = re.Time();
		if (time < start_time + duration) {
			position3	p = float3x4(re.consts.view) * pos;
			float		d = len(p) * 2.f - p.v.z;
			if (!radius || d < radius) {
				float	t = (time - start_time) / duration;
				float	a = radius ? (radius - d) / radius : 1;
				total += col * ((t < .25f ? t : (1 - t) / 3) * 4 * a);
			}
		} else
			delete this;
	}
}

ScreenFlash::ScreenFlash(float time, param(position3) _pos, param(colour) _col, float _duration, float _radius) : pos(_pos), col(_col), duration(_duration), radius(_radius), start_time(time) {
	CTOR_RETURN
}

namespace ent {
	struct ScreenFlash {
		float3p		colour;
		float		time;
		float		radius;
	};
}

ISO_DEFUSERCOMPV(ent::ScreenFlash, colour, time, radius);
template<> void TypeHandler<ent::ScreenFlash>::Create(const CreateParams &cp, crc32 id, const ent::ScreenFlash *t) {
	new ScreenFlash(cp.Time(), cp.obj->GetWorldPos(), colour(t->colour, one), t->time, t->radius);
}
static TypeHandler<ent::ScreenFlash> thScreenFlash;
