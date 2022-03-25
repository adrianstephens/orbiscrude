#include "sound.h"

namespace iso {

Sound::Voices			Sound::voices;
Sound::AtennuationCone	Sound::cone;
float	Sound::volume[16]			= {1};
float	Sound::prev_volume[16]		= {0};
int		Sound::next_voice			= 0;
float 	Sound::last_master_pitch	= 1;
float 	Sound::master_pitch			= 1;
float	Sound::default_cutoffdist	= 1000; //0000.0f;

void Sound::Update(float deltatime, param(float3x4) view) {
	Update(deltatime, &view, 1);
}

void Sound::Update(float deltatime, const float3x4 *views, int numviews) {
	float3	listener_vel[8];

	listener_num = numviews;
	for (int i = 0; i < numviews; i++) {
		position3	pos	= get_trans(views[i]);
		if (deltatime == 0 || len2(listener_vel[i] = (pos - listener_pos[i]) / deltatime) > square(500))
			listener_vel[i] = float3(zero);
		listener_pos[i] = pos;
	}

	SetListenerMatrices(views, listener_vel, listener_num);

	bool pitch_update = last_master_pitch != master_pitch;
	last_master_pitch = master_pitch;

	for (int i = 0; i < voices.size(); i++) {
		SoundVoice &v = voices[i];
		if (v.IsPlaying()) {
			if (v.IsPositional()) {
				position3	pos(v.position);
#ifdef OBJECT_H
				if (v.flags.test(SoundVoice::POSOBJECT)) {
					if (v.posobj) {
						position3	newpos = v.posobj->GetWorldPos();
						SetVoicePosition(&v, newpos, (newpos - pos) / deltatime);
						v.position = (pos = newpos).v;
					} else {
						// Object is deleted so kill self.
						v.Stop();
						continue;
					}
				}
#endif
				v._SoundVoice::SetVolume(volume[v.type] * v.volume * cone.get_atten(views[0], pos));

			} else {
				if (volume[v.type] != prev_volume[v.type] || v.flags.test(SoundVoice::PENDING))
					v._SoundVoice::SetVolume(volume[v.type] * v.volume);

				if (v.flags.test(SoundVoice::PENDING) || (pitch_update && !v.flags.test(SoundVoice::SKIP_MASTER_PITCH)))
					v._SoundVoice::SetPitch(Sound::master_pitch * v.pitch);

			}

			if (!v.IsPaused()) {
				if (v.flags.test_clear(SoundVoice::PENDING))
					v.Start();
			}

			if (v.sequenceindex >= 0) {
				if (v.sequenceindex < v.sequencearray.Count()) {
					if (v.Cue(v.sequencearray[v.sequenceindex])) {
						if (++v.sequenceindex >= v.sequencearray.Count())
							v.sequenceindex = v.flags.test(SoundVoice::LOOPSEQUENCE) ? 0 : -1;	// loop check.
					}
				} else {
					v.sequenceindex = -1;
				}
			}
		} else {
			v._SoundVoice::Reset();
		}
	}
	raw_copy(volume, prev_volume);
}

void Sound::StopSounds(SoundType sound_type) {
	for (int i = 0; i < voices.size(); i++) {
		SoundVoice &v = voices[i];
		if (sound_type == ST_ALL || v.type == sound_type)
			v.Stop();
	}
}

bool Sound::PauseSounds(bool pause, SoundType sound_type) {
	for (int i = 0; i < voices.size(); i++) {
		SoundVoice &v = voices[i];
		if ((sound_type == ST_ALL || v.type == sound_type) && v.IsPlaying())
			v.Pause(pause);
	}
	return pause;
}

SoundVoice*	Sound::GetFreeVoice() {
	for (int i = 0; i < voices.size(); i++) {
		SoundVoice	&v = voices[(i + next_voice) % voices.size()];
		if (v.refs == 0 && !v.IsPlaying()) {
			next_voice++;
			v.Reset();
			return &v;
		}
	}
	return NULL;
}

void SoundVoice::Init(const SoundBuffer &p, SoundType sound_type) {
	_SoundVoice::Init(p, sound_type == ST_MUSIC, sequenceindex != -1);
	type	= sound_type;
	flags.set(PENDING);
}

void SoundVoice::Init(const SoundList &p, SoundType type, bool loop) {
	sequencearray = p.View();
	sequenceindex = p.Count() > 1 ? 1 : (loop ? 0 : -1);
	if (loop)
		flags.set(LOOPSEQUENCE);
	Init(*p[0], type);
}

SoundVoice &SoundVoice::SetVolume(float v) {
	volume = v;
	_SoundVoice::SetVolume(Sound::GetVolume(GetType()) * v);
	return *this;
}

SoundVoice &SoundVoice::SetPitch(float p) {
	pitch = p;
	_SoundVoice::SetPitch(flags.test(SKIP_MASTER_PITCH) ? p : Sound::master_pitch * p);
	return *this;
}

SoundVoice&	SoundVoice::SetPositional(param(position3) _p) {
	position = _p.v;
	flags.set(POSITIONAL);
	MakePositional(Sound::default_cutoffdist);
//	SetPosition(_p, float3(zero));
	return *this;
}

SoundVoice* Play(const SoundBuffer *p, SoundType type) {
	SoundVoice *v;
	if (!(p && (v = Sound::GetFreeVoice())))
		return NULL;
	v->Init(*p, type);
	return v;
}

SoundVoice* Play(const SoundBuffer *p, SoundType type, float volume, float pitch) {
	SoundVoice *v;
	if (!(p && (v = Play(p, type))))
		return NULL;
	return &(v->SetVolume(volume).SetPitch(pitch));
}

SoundVoice*	PlayAt(const SoundBuffer *p, param(position3) _pos) {
	if (SoundVoice *snd = Play(p)) {
		snd->SetPositional(_pos);
		return snd;
	}
	return NULL;
}

SoundVoice*	PlayAt(const SoundBuffer *p, param(position3) _pos, float volume, float pitch) {
	if (SoundVoice *snd = Play(p, ST_SFX, volume, pitch)) {
		snd->SetPositional(_pos);
		return snd;
	}
	return NULL;
}

SoundVoice *PlaySoundList(const SoundList &p, SoundType type, bool loop) {
	SoundVoice *v = Sound::GetFreeVoice();
	if (v) {
		v->Init(p, type, loop);
	}
	return v;
}

#ifdef OBJECT_H
SoundVoice&	SoundVoice::SetPositional(Object *o) {
	posobj = o;
	flags.set(POSOBJECT);
	MakePositional(Sound::default_cutoffdist);
	return SetPositional(o->GetWorldPos());
}

SoundVoice*	PlayPositional(const SoundBuffer *p, Object *o) {
	if (SoundVoice *snd = Play(p)) {
		snd->SetPositional(o);
		return snd;
	}
	return NULL;
}

SoundVoice*	PlayPositional(const SoundBuffer *p, Object *o, float volume, float pitch) {
	if (SoundVoice *snd = Play(p, ST_SFX, volume, pitch)) {
		snd->SetPositional(o);
		return snd;
	}
	return NULL;
}
#endif

}	// namespace iso

