#ifndef TTS_H
#define TTS_H

#include "base/defs.h"

#ifdef PLAT_PC
#include "com.h"
#include <sapi.h>

#elif defined PLAT_ANDROID
#include "jni_helper.h"

#elif defined PLAT_IOS
#import "AVFoundation/AVFoundation.h"

#endif

namespace iso {

struct TextToSpeech {
#ifdef PLAT_PC
	com_ptr<ISpVoice>	voice;
	void		event_handler();
	static void	event_handler(WPARAM wParam, LPARAM lParam) { ((TextToSpeech*)lParam)->event_handler(); }

#elif defined PLAT_ANDROID
	java::lang::String		language;
	float					rate;

#elif defined PLAT_IOS
	AVSpeechSynthesizer		*synthesizer;
	AVSpeechSynthesisVoice	*voice;
	float					rate;

#endif

	TextToSpeech(const char* language = 0, bool female = false, float rate = 1, float frequency = 0);
	float			GetFrequency()	const;
	bool			Say(const char *speech);
	bool			SayToFile(const char *fn, const char *text);
	malloc_block	SayToSample(const char *text);
};

} // namespace iso

#endif //TTS_H
