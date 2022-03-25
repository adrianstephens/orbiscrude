#ifndef PLAT_WINRT
#include <dsound.h>
#include "dx/dx_helpers.h"

#undef small
#undef min
#undef max
#endif

#include "filetypes/sound/sample.h"

namespace iso {

#ifdef PLAT_WINRT

class SoundBuffer {
public:
	SoundBuffer(iso::sample *sm);
	void	SetMusic()		{}
};

class _SoundVoice {
	friend class _Sound;
public:
	void		Init(const SoundBuffer &buffer, bool music, bool stream)	{}
	bool		Start()							{ return false; }
	bool		Stop()							{ return false;	}
	bool		Pause(bool pause)				{ return false; }
	int			Status()	const				{ return 0; }
	bool		Cue(const SoundBuffer *buffer)	{ return false; }
	void		Reset()							{}

	void		SetPitch(float p)				{}
	void		SetVolume(float v)				{}
	void		MakePositional(float d)			{}
	void		SetCutOffDistance(float d)		{}
	void		SetPosition(param(position3) p, param(float3) v);

	uint32		GetPlayPosition()	const		{ return 0; }

public:
	bool		IsPlaying()			const		{ return false; }
};

#else

class SoundBuffer : public indirect32<com_ptr<IDirectSoundBuffer> > {
public:
	SoundBuffer(iso::sample *sm);
	void	SetMusic()		{}
};

class _SoundVoice {
	friend class _Sound;

	com_ptr<IDirectSoundBuffer>	buffer;
	DS3DBUFFER			buffparams;
	float				originalfreq;
public:
	_SoundVoice()	{ buffparams.dwSize = sizeof(DS3DBUFFER); }
	~_SoundVoice()	{ if (buffer) buffer->Stop(); }

	void		Init(const SoundBuffer &buffer, bool music, bool stream);
	bool		Start() {
		HRESULT	hr = buffer->Play(0,0,0);
		SetVolume(1.0f);
		SetPitch(1.0f);
		return SUCCEEDED(hr);
	}
	bool		Stop()							{ return SUCCEEDED(buffer->Stop());	}
	bool		Pause(bool pause)				{ return SUCCEEDED(pause ? buffer->Stop() : buffer->Play(0,0,0)); }
	int			Status()	const				{ DWORD status; return buffer && SUCCEEDED(buffer->GetStatus(&status)) ? status : 0; }
	bool		Cue(const SoundBuffer *buffer)	{ return false; }
	void		Reset()							{ buffer.clear(); }

	void		SetPitch(float p)				{ buffer->SetFrequency(DWORD(p * originalfreq)); }
	void		SetVolume(float v)				{ buffer->SetVolume(v == 0 ? DSBVOLUME_MIN : (LONG)(log10(v) * 1000)); }
	void		MakePositional(float d)			{}
	void		SetCutOffDistance(float d)		{}
	void		SetPosition(param(position3) p, param(float3) v);

	uint32		GetPlayPosition()	const		{ DWORD play; return SUCCEEDED(buffer->GetCurrentPosition(&play, NULL)) ? play : 0; }

public:
	bool		IsPlaying()			const		{ return !!(Status() & DSBSTATUS_PLAYING); }
};
#endif

class _Sound {
	friend class _SoundVoice;
	friend class SoundBuffer;
protected:
	void	SetListenerMatrix(param(float3x4) viewmat, param(float3) viewvel);
	void	SetListenerMatrices(const float3x4 *viewmats, const float3 *viewvels, int numviewmats);
	void	SetVoicePosition(class SoundVoice *voice, param(position3) p, param(float3) v);
public:
	bool	Init();
};
SoundBuffer* GetSoundBuffer(ISO_ptr<void> &p);

}//namespace iso
