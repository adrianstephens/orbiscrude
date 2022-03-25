#define INITGUID
#include "sound.h"

using namespace iso;

void iso::Init(SampleBuffer *x, void *physram) {
#ifndef ISO_EDITOR
	iso_ptr32<void> *p = (iso_ptr32<void>*)x->get();
	if (p && !p[-2]) {
		p[-2] = p;
		new(p) SoundBuffer((sample*)p);
	}
#endif
}
void iso::DeInit(SampleBuffer *x) {}

#ifdef PLAT_WINRT

bool _Sound::Init() {
	return false;
}

void _Sound::SetListenerMatrix(param(float3x4) viewmat, param(float3) viewvel) {
}

void _Sound::SetListenerMatrices(const float3x4 *viewmats, const float3 *viewvels, int numviewmats) {
}

void _Sound::SetVoicePosition(class SoundVoice *voice, param(position3) p, param(float3) v) {
}

SoundBuffer::SoundBuffer(sample *sm) {
}


#else

#include <mmreg.h>

#pragma comment(lib, "dsound")

com_ptr<IDirectSound8>			dsound;
com_ptr<IDirectSound3DListener>	d3dlistener;
DS3DLISTENER					d3dlistenerparams;


static D3DVECTOR IsoToDSound3D(param(position3) p)	{ return force_cast<D3DVECTOR>(p.v.xzy); }
static D3DVECTOR IsoToDSound3D(param(float3) v)		{ return force_cast<D3DVECTOR>(v.xzy); }

bool _Sound::Init() {
	if (dsound)
		return true;

	ISO::getdef<sample>();

	HRESULT		hr;
	if (SUCCEEDED(hr = DirectSoundCreate8(NULL, &dsound, NULL)) &&
		SUCCEEDED(hr = dsound->SetCooperativeLevel(GetForegroundWindow(), DSSCL_NORMAL))
	) {
		DSBUFFERDESC				desc;
		com_ptr<IDirectSoundBuffer>	primary;
		clear(desc);
		desc.dwSize		= sizeof(DSBUFFERDESC);
		desc.dwFlags	= DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
		if (SUCCEEDED(hr = dsound->CreateSoundBuffer(&desc, &primary, NULL))) {
			if (SUCCEEDED(hr = primary.query(&d3dlistener, IID_IDirectSound3DListener))) {
				d3dlistenerparams.dwSize = sizeof(DS3DLISTENER);
				d3dlistener->GetAllParameters(&d3dlistenerparams);
				d3dlistenerparams.flRolloffFactor = 0.125f;
				d3dlistener->CommitDeferredSettings();
			}
		}
	}

	return SUCCEEDED(hr);
}

void _Sound::SetListenerMatrix(param(float3x4) viewmat, param(float3) viewvel) {
	d3dlistenerparams.vPosition		= IsoToDSound3D(viewmat.w);
	d3dlistenerparams.vOrientFront	= IsoToDSound3D(viewmat.y);
	d3dlistenerparams.vOrientTop	= IsoToDSound3D(viewmat.z);
	if (d3dlistener)
		d3dlistener->SetAllParameters(&d3dlistenerparams, DS3D_IMMEDIATE);
}

void _Sound::SetListenerMatrices(const float3x4 *viewmats, const float3 *viewvels, int numviewmats) {
	SetListenerMatrix(viewmats[0], viewvels[0]);
}

void _Sound::SetVoicePosition(class SoundVoice *voice, param(position3) p, param(float3) v) {
	com_ptr<IDirectSound3DBuffer>	buffer3d;
	if (SUCCEEDED(voice->buffer.query(&buffer3d, IID_IDirectSound3DBuffer))) {
		voice->buffparams.vPosition = IsoToDSound3D(p);
		voice->buffparams.vVelocity = IsoToDSound3D(v);
		voice->buffparams.flMinDistance = 5.0f;
		voice->buffparams.dwMode = DS3DMODE_NORMAL;
		buffer3d->SetAllParameters(&voice->buffparams, DS3D_IMMEDIATE);
	}
}

SoundBuffer::SoundBuffer(sample *sm) {
	if (!sm)
		return;

	DSBUFFERDESC		dsbdesc;
	int	bps	= sm->BytesPerSample();
#if 0
	PCMWAVEFORMAT		pcmwf;
	iso::clear(pcmwf);
	pcmwf.wf.wFormatTag			= WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels			= sm->Channels();
	pcmwf.wf.nSamplesPerSec		= DWORD(sm->Frequency());
	pcmwf.wf.nBlockAlign		= WORD(bps);
	pcmwf.wf.nAvgBytesPerSec	= DWORD(sm->Frequency() * bps);
	pcmwf.wBitsPerSample		= sm->Bits();
#else
	WAVEFORMATEXTENSIBLE	wfx;
	iso::clear(wfx);
	wfx.Format.wFormatTag			= WAVE_FORMAT_EXTENSIBLE;//WAVE_FORMAT_PCM;
	wfx.Format.nChannels			= sm->Channels();
	wfx.Format.nSamplesPerSec		= DWORD(sm->Frequency());
	wfx.Format.nBlockAlign			= WORD(bps);
	wfx.Format.nAvgBytesPerSec		= DWORD(sm->Frequency() * bps);
	wfx.Format.wBitsPerSample		= sm->Bits();
	wfx.Format.cbSize				= sizeof(wfx) - sizeof(WAVEFORMATEX);
	wfx.Samples.wValidBitsPerSample	= sm->Bits();
	wfx.dwChannelMask				= (1 << sm->Channels()) - 1;
	wfx.SubFormat					= KSDATAFORMAT_SUBTYPE_PCM;
#endif

	iso::clear(dsbdesc);
	dsbdesc.dwSize				= sizeof(DSBUFFERDESC);		// Need default controls (pan, volume, frequency).
//	dsbdesc.dwFlags				= DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;
	dsbdesc.dwFlags				= (sm->Channels() == 1 ? DSBCAPS_CTRL3D : 0) | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS;
	dsbdesc.dwBufferBytes		= sm->Length() * bps;
	dsbdesc.lpwfxFormat			= (WAVEFORMATEX*)&wfx;//&pcmwf;

	if (!(dsound && SUCCEEDED(dsound->CreateSoundBuffer(&dsbdesc, &*this, NULL))))
		return;

	void	*ptr1, *ptr2;
	DWORD	n1, n2;
	get()->Lock(0, 0, &ptr1, &n1, &ptr2, &n2, DSBLOCK_ENTIREBUFFER);
	memcpy(ptr1, (char*)sm->Samples(), n1);
	get()->Unlock(ptr1, n1, ptr2, n2);
}


void _SoundVoice::Init(const SoundBuffer &_buffer, bool music, bool stream) {
	dsound->DuplicateSoundBuffer(_buffer, &buffer);
	if (_buffer) {
		DWORD	t;
		_buffer->GetFrequency(&t);
		originalfreq = float(t);

		com_ptr<IDirectSound3DBuffer>	ds3d;
		if (SUCCEEDED(buffer->QueryInterface(IID_IDirectSound3DBuffer, (void**)&ds3d))) {
			ds3d->GetAllParameters(&buffparams);
			buffparams.dwMode = DS3DMODE_DISABLE;	// Initially not positional.
			ds3d->SetAllParameters(&buffparams, DS3D_IMMEDIATE);
		}
		SetVolume(1.0f);
		SetPitch(1.0f);
	}
}
#endif