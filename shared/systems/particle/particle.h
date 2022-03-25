#ifndef PARTICLE_H
#define PARTICLE_H

#include "render_object.h"
#include "extra/random.h"
#include "iso/iso.h"
#include "object.h"

namespace particles {
	enum {STAGE = iso::RS_TRANSP + 8};
}

typedef	iso::anything	pm_list;

struct particle_effect {
	iso::ISO_ptr<void>	particle;
	int					maxparticles;
	int					mode;
	pm_list				modifiers;
};

//-----------------------------------------------------------------------------
//	Particle
//-----------------------------------------------------------------------------
struct Particle {
	iso::float4		pos;
	iso::colour		col;
	union {
		float		params[4];
		struct {
			float	width, height, image, life;
		};
	};
	iso::float4		speed;

	force_inline void	Init(float start_life = 0) { pos = iso::zero; speed = iso::zero; col = iso::colour(iso::one); *(iso::float4*)&width = iso::zero; life = start_life; }
};

class ParticleRenderer;

enum PS_MODE {
	PS_ORIENT					= 3,
	PS_ORIENT_FLAT				= 0,
	PS_ORIENT_VELOCITY			= 1,
	PS_ORIENT_POSITION			= 2,
	PS_ORIENT_AS_IS				= 3,

	PS_SPACE					= 12,
	PS_RELATIVE_SPACE			= 0,
	PS_WORLD_SPACE				= 4,
	PS_SCREEN_SPACE				= 8,
	PS_SCREENPOS_SPACE			= 12,

	PS_INSTANT_OFF				= 16,
	PS_ALONG_Z					= 32,	//?? (also affects world space particles speed)

	PS_TINT						= 128,	// tint based on scene cols
	PS_SOFT						= 256,

	PS_DONT_INTERLEAVE_AABB_CALC= 512,
};

struct ParticleSet : public iso::e_link<ParticleSet>, public iso::aligner<16> {
	iso::float3x4			rel_mat, mat, prev_mat;
	iso::float3				scale;
	iso::cuboid				extent;
	iso::rng<iso::simple_random>	rand;

	iso::crc32				id;
	iso::anything::view_t	modifiers;
	ParticleRenderer		*pr;
	Particle				*particles;
	Particle				*dependancy;
	Particle				*kill;
	iso::e_list<ParticleSet> dependants;

	iso::int16				mode, num_parts, max_parts, num_killed;
	iso::bool8				die;

	float					start_time, prev_life, life;
	float					emit_leftover, emit_scale;
	float					alpha;

	void			Init(iso::crc32 _id, const particle_effect *pe, param(iso::float3x4) _mat);
	void			Kill(Particle *p);

	void			Emit(float n);
	void			StopEmitting(bool recurse);
	void			ScaleEmitRate(float scale);

	bool			Update(float time);
	void			Render(iso::RenderEvent *re, param(iso::float3x4) view, param(iso::float4x4) proj, float alpha = 1.0f);
	void			Append(ParticleSet *ps, Particle *i);
	iso::cuboid		GetExtent();
	bool			CheckVisible(param(iso::float4x4) viewProj);

	void			Die()				{ die = true;		}
	void			DontDie()			{ die = false;		}

	Particle		*Begin()	const	{ return particles; }
	Particle		*End()		const	{ return particles + num_parts; }
	int				NumParts()	const	{ return num_parts; }
	float			Age()		const	{ return life;		}

	iso::float3x4&	GetMatrix()								{ return mat;	}
	void			SetMatrix(param(iso::float3x4) _mat)	{ mat = _mat;	}

	float			GetAlpha()	const		{ return alpha; }
	void			SetAlpha(float _alpha)	{ alpha = _alpha; }

	ParticleSet(float time);
	~ParticleSet();
};

//-----------------------------------------------------------------------------
//	ParticleHolder
//-----------------------------------------------------------------------------
class ParticleHolder
	: public iso::HandlesWorld<ParticleHolder, iso::FrameEvent2>
	, public iso::HandlesWorld<ParticleHolder, iso::RenderEvent>
	, public iso::DeleteOnDestroy<ParticleHolder>
	, public iso::referee
	, public iso::aligner<16>
{
	ParticleSet				*ps;
	iso::ObjectReference	obj;
	iso::uint8				bone;
	iso::float3x4			mat;
	iso::cuboid				obj_extent;	// extent in object space
	iso::cuboid				extent;		// extent in world space
	iso::float4x4			inv_extent_mat;
	int						calc_extent_interleave;

	iso::float3x4	GetMatrix()	const	{ return bone != iso::Pose::INVALID ? obj->GetBoneWorldMat(bone) * mat : obj ? obj->GetWorldMat() * mat : mat; }

public:
	static void		StopEmitting(iso::Object *obj);
	void			StopEmitting()							{ ps->StopEmitting(true); }
	ParticleSet*	GetParticleSet()						{ return ps; }
	void			SetMatrix(const iso::float3x4 &_mat)	{ mat = _mat; }

	void	operator()(iso::RenderEvent*, iso::uint32);
	void	operator()(iso::RenderEvent&);
	void	operator()(iso::FrameEvent2&);
	void	operator()(iso::QueryTypeMessage&);

	ParticleHolder(iso::World *world, iso::crc32 id, const particle_effect *pe, param(iso::float3x4) relmat, iso::Object *_obj = NULL, iso::crc32 _bone = iso::crc32());
	~ParticleHolder();
};
typedef iso::linked_ref<ParticleHolder> ParticleHolderReference;

//-----------------------------------------------------------------------------
//	ScreenFlash
//-----------------------------------------------------------------------------
class ScreenFlash
	: public iso::HandlesWorld<ScreenFlash, iso::RenderEvent>
	, public iso::CleanerUpper<ScreenFlash>
	, public iso::aligner<16>
{
	static iso::colour total;

	iso::position3	pos;
	iso::colour		col;
	float			duration, radius;
	float			start_time;

public:
	void				operator()(iso::RenderEvent&);
	static iso::colour	FlushColour()	{ iso::colour _total = total; total = iso::colour(iso::zero); return _total; }
	ScreenFlash(float time, param(iso::position3) _pos, param(iso::colour) _col, float _duration, float _radius);
};

#endif	// PARTICLE_H