#ifndef SOUND_H
#define SOUND_H

#include "base/vector.h"
#include "scenegraph.h"
#include "_sound.h"

#ifndef _USRDLL
#include "object.h"
#endif

namespace iso {

struct SampleBuffer	: iso_ptr32<SoundBuffer>		{};
struct SoundList	: ISO_openarray<SampleBuffer>	{};

enum SoundType {
	ST_ALL	= -1,
	ST_SFX,
	ST_MUSIC,
	ST_GUI,
	ST_HUD,
	ST_VO,
	ST_INTRO,	// Career mode intro volume.
};

class SoundVoice : public _SoundVoice {
	friend class	Sound;

	enum Flags {
		PENDING				= 1 << 0,
		POSITIONAL			= 1 << 1,
		POSOBJECT			= 1 << 2,	// So we don't persist when the object is killed.
		PAUSED				= 1 << 3,
		SKIP_MASTER_PITCH	= 1 << 4,
		LOOPSEQUENCE		= 1 << 5,
	};

	iso::flags<Flags, uint8>	flags;
	uint8			type;
	uint16			refs;
	float3p			position;
#ifdef OBJECT_H
	ObjectReference	posobj;
#endif
	SoundList::view_t	sequencearray;
	int				sequenceindex;
	float			volume, pitch;

public:
	SoundVoice() : flags(0), refs(0), sequenceindex(-1), volume(1.0f) {}

	void			addref()					{ refs++;	}
	void			release()					{ refs--;	}

	void			Reset()						{ _SoundVoice::Reset(); flags = 0; refs = 0; sequenceindex = -1; volume = pitch = 1; }

	void			Init(const SoundBuffer &buffer, SoundType sound_type);
	void			Init(const SoundList &p, SoundType type, bool loop);
	SoundVoice&		SetVolume(float v);
	SoundVoice&		SetPitch(float p);
	SoundVoice&		SetPositional(param(position3) _p);
#ifdef OBJECT_H
	SoundVoice&		SetPositional(Object *o);
#endif
	void			SetCutOffDistance(float _dist)	{ _SoundVoice::SetCutOffDistance(_dist); } // not really needed but for clarity
	void			SkipMasterPitch()			{ flags.set(SKIP_MASTER_PITCH);		}
	bool			Stop()						{ flags.clear_all(PENDING | PAUSED); return _SoundVoice::Stop(); }
	bool			Pause(bool pause)			{ return IsPlaying() && flags.test_set(PAUSED, pause) != pause && _SoundVoice::Pause(pause); }

	bool			IsPlaying()			const	{ return flags.test_any(PENDING | PAUSED) || _SoundVoice::IsPlaying(); }
	bool			IsPositional()		const	{ return flags.test(POSITIONAL);	}
	bool			IsPaused()			const	{ return flags.test(PAUSED);		}
	SoundType		GetType()			const	{ return SoundType(type);			}
	float			GetVolume()			const	{ return volume;					}
	float			GetPitch()			const	{ return pitch;						}
};

class Sound : public _Sound {
	friend class SoundVoice;
	friend class _Sound;

	struct AtennuationCone {
		float	min_cos, scale, offset;

		AtennuationCone() : min_cos(1), scale(1), offset(0)	{}

		void	set(float min_angle, float max_angle, float min_vol) {
			float2	cosines = cos(float2{min_angle, max_angle} / 2);
			min_cos = cosines.y;
			scale	= (cosines.x - cosines.y) * (1 - min_vol);
			offset	= min_vol - min_cos * scale;
		}
		float	get_atten(param(float3x4) listener, param(position3) source) {
			float	d = min_cos < 1 ? max(float(dot(listener.z, normalise(source - get_trans(listener)))), min_cos) : min_cos;
			return min(d * scale + offset, 1.f);
		}
	};

	typedef array<SoundVoice, 256> Voices;
	static Voices		voices;
	static int			next_voice;
	static float		volume[16], prev_volume[16];
	static float		last_master_pitch;	// So we don't reset it everytime for non-positionals
	static float		master_pitch;
	static float		default_cutoffdist;
	static AtennuationCone	cone;

	int					listener_num;
	position3			listener_pos[8];

public:
	void				Update(float deltatime, param(float3x4) view = identity);
	void				Update(float deltatime, const float3x4 *views, int numviews);

	static SoundVoice*	GetFreeVoice();
	static void			StopSounds(SoundType sound_type = ST_ALL);
	static bool			PauseSounds(bool pause = true, SoundType sound_type = ST_ALL);

	static void			SetDefaultCutOffDistance(float _dist)								{ default_cutoffdist = _dist;	}
	static void			SetAttenuationCone(float min_angle, float max_angle, float min_vol)	{ cone.set(min_angle, max_angle, min_vol);	}
	static void			SetMasterPitch(float pitch)											{ master_pitch = pitch;			}
	static float		SetVolume(SoundType st, float _vol)									{ return volume[st]	= _vol;		}

	static float		GetVolume(SoundType st)					{ return volume[st];			}
#ifdef PLAT_WII
	static void			ShutDown()								{ StopSounds(); _ShutDown();	}
#endif

	Sound() {
		master_pitch = 1;
	}
};

SoundVoice	*Play(const SoundBuffer *p, SoundType type = ST_SFX);
SoundVoice	*Play(const SoundBuffer *p, SoundType type, float volume, float pitch = 1.0f);
SoundVoice	*PlayAt(const SoundBuffer *p, param(position3) _p);
SoundVoice	*PlayAt(const SoundBuffer *p, param(position3) _p, float volume, float pitch = 1.0f);
SoundVoice	*PlaySoundList(const SoundList &p, SoundType type = ST_MUSIC, bool loop = true);

#ifdef OBJECT_H
SoundVoice	*PlayPositional(const SoundBuffer *p, Object *o);
SoundVoice	*PlayPositional(const SoundBuffer *p, Object *o, float volume, float pitch = 1.0f);
#endif

struct SoundVoiceHolder : ref_ptr<SoundVoice> {
	SoundVoice*			operator=(SoundVoice *b)		{ Stop(); return ref_ptr<SoundVoice>::operator=(b);	}
	void				Take(SoundVoiceHolder &b)		{ Stop(); swap((ref_ptr<SoundVoice>&)*this, b);	}
	void				Release()						{ clear();									}
	void				Stop()							{ if (p) {p->Stop(); clear(); }				}
	SoundVoiceHolder&	SetVolume(float x)				{ if (p) p->SetVolume(x);	return *this;	}
	SoundVoiceHolder&	SetPitch(float x)				{ if (p) p->SetPitch(x);	return *this;	}
	bool				IsPlaying() const				{ return p && p->IsPlaying();				}
};

} //namespace iso

ISO_DEFUSER(iso::SoundList, ISO_openarray<iso::SampleBuffer>);

#endif // SOUND_H
