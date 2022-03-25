#include "base/vector.h"
#include "extra/filters.h"
#include "filetypes/sound/sample.h"
#include "filetypes/sound/xma.h"
#include "iso/iso_convert.h"
#include "speech/tts.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Sample functions
//-----------------------------------------------------------------------------

ISO_ptr<void> Looping(ISO_ptr<void> p, int loop) {
	if (p.GetType()->SameAs<XMA>()) {
		XMA* xma = p;
		if (loop)
			xma->flags |= XMA::LOOP;
		else
			xma->flags &= ~XMA::LOOP;
	} else if (ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p)) {
		if (loop)
			sm->flags |= sample::LOOP;
		else
			sm->flags &= ~sample::LOOP;
		return sm;
	}
	return p;
}

ISO_ptr<void> Music(holder<ISO_ptr<void> > wp) {
	if (ISO_ptr<void>& p = wp)
		if (p.GetType()->SameAs<sample>()) {
			((sample*)p)->flags |= sample::MUSIC;
			return p;
		}
	return ISO_NULL;
}

ISO_ptr<void> Resample(ISO_ptr<void> p, float newfreq) {
	if (TypeType(p.GetType()) == ISO::OPENARRAY) {
		anything*	a = p;
		int			n = a->Count();
		ISO_ptr<ISO_openarray<ISO_ptr<sample>>> p2(p.ID());
		p2->Create(n);
		for (int i = 0; i < n; i++)
			(*p2)[i] = Resample((*a)[i], newfreq);
		return p2;
	}

	if (ISO_ptr<sample> sm = ISO_conversion::convert<sample>(p))
		return resample(sm, newfreq);

	return ISO_NULL;
}

sample MakeSineWave(float samplerate, float frequency, float amp, float length) {
	sample sm;
	sm.Create(int(length * samplerate), 1, 16);
	sm.SetFrequency(samplerate);
	int16* s = sm.Samples();
	for (int i = 0, n = sm.Length(); i < n; i++)
		*s++ = int16(amp * 32767 * sin(i * frequency / samplerate));
	return sm;
}

sample GetChannel(ISO_ptr<sample> sm, int c) {
	sample sm2;
	int	length = sm->Length(), chans = sm->Channels();
	int16* s = sm->Samples() + c;
	int16* d = sm2.Create(length, 1, 16);
	for (int i = 0; i < length; i++, s += chans)
		*d++ = *s;
	sm2.SetFrequency(sm->Frequency());
	return sm2;
}
#ifdef PLAT_PC
sample TTS(const char *text, const char *language, bool female, float rate, float frequency) {
	TextToSpeech	tts(language, female, rate ? rate : 1, frequency);
	auto	memory	= tts.SayToSample(text);
	sample sm(tts.GetFrequency(), 1, 16);
	sm.samples	= make_range<uint16>(memory);
	return sm;
}
#endif

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(Looping),
	ISO_get_operation(Music),
	ISO_get_operation(Resample),
	ISO_get_operation(MakeSineWave),
	ISO_get_operation(GetChannel)
#ifdef PLAT_PC
	, ISO_get_operation(TTS)
#endif
);
