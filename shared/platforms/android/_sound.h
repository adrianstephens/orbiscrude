#include "base/defs.h"
#include <SLES/OpenSLES_Android.h>

#ifdef ISO_RELEASE
#define alErrorCheck()
#define alVerify()	true
#else
extern void alErrorCheck();
extern bool alVerify();
#endif

namespace ios {
class SoundBuffer;
}

namespace iso {
using namespace ios;

class _SoundVoice;

class _SoundVoice {
	friend class _Sound;
	SLuint32	source;

	SLint32		GetState()	const;// { SLint32 i; alGetSourcei(source, SL_SOURCE_STATE, &i); return i; }

protected:
	void		Init(const SoundBuffer &buffer, bool music, bool stream);
	bool		Start()							{ return false; }
	bool		Stop()							{ return false; }
	bool		Pause(bool pause)				{ return false;	}
	bool		Cue(const SoundBuffer *buffer);
	void		Reset();

	void		SetPitch(float p, bool _positional = false)		{}
	void		SetVolume(float v)								{}
	void		MakePositional(float d);
	void		SetCutOffDistance(float d);
	void		SetPosition(param(position3) p, param(float3) v);

public:
	_SoundVoice() : source(0)	{}
	bool		IsPlaying()			const	{ return false; }
};

class _Sound {
//	SLCcontext*	context;
//	SLCdevice*	device;
	void*		data;
	bool		playing, interrupted;

protected:
	void		SetListenerMatrices(const float3x4 *viewmats, const float3 *viewvels, int numviewmats);
	void		SetVoicePosition(class SoundVoice *voice, param(position3) _p, param(float3) _v);
public:
	void		operator()(uint32 id, uint32 data_size, const void *data);
	void		operator()(uint32 state);

	bool		Init();
	void		DeInit();
};

}//namespace iso
