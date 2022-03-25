#include "sound.h"

using namespace iso;
using namespace ios;

namespace iso {

//-----------------------------------------------------------------------------
//	SampleBuffer
//-----------------------------------------------------------------------------

ISO_TYPEDEF(IOSSoundBuffer, void);

ISO_INIT(SampleBuffer) {
}

ISO_DEINIT(SampleBuffer) {
}

//-----------------------------------------------------------------------------
//	_SoundVoice
//-----------------------------------------------------------------------------

void _SoundVoice::Reset() {
	if (source) {
		//alDeleteSources(1, &source);
		source = 0;
	}
}

void _SoundVoice::Init(const SoundBuffer &buffer, bool music, bool stream) {
}

bool _SoundVoice::Cue(const SoundBuffer *buffer) {
	return true;
}

void _SoundVoice::MakePositional(float d) {
	SetCutOffDistance(d);
}

void _SoundVoice::SetCutOffDistance(float d) {
}

void _SoundVoice::SetPosition(param(position3) p, param(float3) v) {
}

//-----------------------------------------------------------------------------
//	_Sound
//-----------------------------------------------------------------------------

bool _Sound::Init() {
	return true;
}

void _Sound::DeInit() {
}

void _Sound::SetListenerMatrices(const float3x4 *viewmats, const float3 *viewvels, int numviewmats) {
}

void _Sound::SetVoicePosition(class SoundVoice *voice, param(position3) _p, param(float3) _v) {
	voice->SetPosition(_p, _v);
}

}