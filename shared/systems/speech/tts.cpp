#include "tts.h"
#include "base/strings.h"
#include "base/maths.h"
#include "base/array.h"
#include "thread.h"

using namespace iso;

#if defined PLAT_PC

TextToSpeech::TextToSpeech(const char* language, bool female, float rate, float frequency) {
//	init_com();
	if (!voice.create(CLSID_SpVoice))
		return;

	if (frequency) {
		com_ptr<ISpStreamFormat>	stream;
		GUID						guid_format;
		unique_com<WAVEFORMATEX>	waveformat;

		if (SUCCEEDED(voice->GetOutputStream(&stream))
		&& SUCCEEDED(stream->GetFormat(&guid_format, &waveformat))
		&& waveformat && waveformat->nSamplesPerSec != frequency
		) {
			if (auto audio = stream.query<ISpAudio>()) {
				waveformat->nSamplesPerSec = frequency;
				audio->SetFormat(guid_format, waveformat);
			}
		}
	}

	com_ptr<ISpObjectTokenCategory> Category(CLSID_SpObjectTokenCategory);
	if (FAILED(Category->SetId(SPCAT_VOICES, false)))
		return;

	com_ptr<ISpObjectToken> token;

#if 1
	auto	opts	= format_string("gender=%s", female ? "female" : "male");

	if (language)
		opts.format(";language=%x", LocaleNameToLCID(string16(language), LOCALE_ALLOW_NEUTRAL_NAMES));

	com_ptr<IEnumSpObjectTokens> i;
	if (SUCCEEDED(Category->EnumTokens(NULL, string16(opts), &i)))
		i->Next(1, &token, NULL);

#else
	if (language) {
		// Enumerate the installed voices
		com_ptr<IEnumSpObjectTokens>	i;
		if (SUCCEEDED(Category->EnumTokens(NULL, NULL, &i))) {
			while (i->Next(1, &token, NULL) == S_OK) {
				com_ptr<ISpDataKey> attributes;
				char16				*language_char16;
				if (token->OpenKey(L"Attributes", &attributes) != S_OK || attributes->GetStringValue(L"Language", &language_char16) != S_OK)
					return;

				uint32	language_id = from_string<xint32>(language_char16);
				char16	language_str[LOCALE_NAME_MAX_LENGTH * 3];
				LCIDToLocaleName(language_id, language_str, LOCALE_NAME_MAX_LENGTH, LOCALE_ALLOW_NEUTRAL_NAMES);

				if (istr(language) == language_str)
					break;

				token.clear();
			}
		}
	}
#endif

	// default voice if no matching found
	if (!token) {
		i.clear();
		if (SUCCEEDED(Category->EnumTokens(NULL, L"VendorPreferred", &i)))
			i->Next(1, &token, NULL);
	}

	voice->SetVoice(token);
	voice->SetRate(min(max(int(ln(rate) * 10), -10), 10));
	voice->SetNotifyCallbackFunction(&event_handler, 0, (LPARAM)this);
	voice->SetInterest(SPFEI_ALL_TTS_EVENTS, SPFEI_ALL_TTS_EVENTS);
}

float TextToSpeech::GetFrequency()	const {
	com_ptr<ISpStreamFormat>	stream;
	GUID						guid_format;
	unique_com<WAVEFORMATEX>	waveformat;
	return SUCCEEDED(voice->GetOutputStream(&stream)) && SUCCEEDED(stream->GetFormat(&guid_format, &waveformat)) && waveformat ? waveformat->nSamplesPerSec : 0;
}

bool TextToSpeech::Say(const char *speech) {
	return voice && SUCCEEDED(voice->Speak(string16(speech), SPF_ASYNC, NULL));
}

const char *visemes[22] = {
	"\n",	// SP_VISEME_0,		// Silence
	"AE",	// SP_VISEME_1,		// AE, AX, AH
	"AA",	// SP_VISEME_2,		// AA
	"AO",	// SP_VISEME_3,		// AO
	"EY",	// SP_VISEME_4,		// EY, EH, UH
	"ER",	// SP_VISEME_5,		// ER
	"y",	// SP_VISEME_6,		// y, IY, IH, IX
	"w",	// SP_VISEME_7,		// w, UW
	"OW",	// SP_VISEME_8,		// OW
	"AW",	// SP_VISEME_9,		// AW
	"OY",	// SP_VISEME_10,	// OY
	"AY",	// SP_VISEME_11,	// AY
	"h",	// SP_VISEME_12,	// h
	"r",	// SP_VISEME_13,	// r
	"l",	// SP_VISEME_14,	// l
	"s",	// SP_VISEME_15,	// s, z
	"SH",	// SP_VISEME_16,	// SH, CH, JH, ZH
	"TH",	// SP_VISEME_17,	// TH, DH
	"f",	// SP_VISEME_18,	// f, v
	"d",	// SP_VISEME_19,	// d, t, n
	"k",	// SP_VISEME_20,	// k, g, NG
	"p",	// SP_VISEME_21,	// p, b, m
};

void TextToSpeech::event_handler() {
	SPEVENT		event;
	while (voice->GetEvents(1, &event, NULL) == S_OK) {
		switch (event.eEventId) {
			case SPEI_VISEME: {
				auto	viseme = (SPVISEMES)LOWORD(event.lParam);
				//ISO_TRACEF("Viseme = ") << viseme << '\n';
				ISO_TRACEF(visemes[viseme]) << ',';
				break;
			}
			case SPEI_SENTENCE_BOUNDARY:
				ISO_TRACE("\n");
				break;

			case SPEI_START_INPUT_STREAM:
			case SPEI_END_INPUT_STREAM:
			case SPEI_VOICE_CHANGE:
			case SPEI_TTS_BOOKMARK:
			case SPEI_WORD_BOUNDARY:
			case SPEI_PHONEME:
			case SPEI_TTS_AUDIO_LEVEL:
			case SPEI_TTS_PRIVATE:
			default:
				break;
		}
	}
}

bool TextToSpeech::SayToFile(const char *fn, const char *text) {
	com_ptr<ISpStreamFormat>    old_stream;
	GUID						guid_format;
	unique_com<WAVEFORMATEX>	waveformat;
	bool	ret = false;

	if (SUCCEEDED(voice->GetOutputStream(&old_stream))
		&&	SUCCEEDED(old_stream->GetFormat(&guid_format, &waveformat))
		&&	waveformat
		&&	waveformat->nAvgBytesPerSec && waveformat->nBlockAlign && waveformat->nChannels
		) {
		if (waveformat->wFormatTag == WAVE_FORMAT_PCM)
			waveformat->cbSize = 0;

		com_ptr<ISpStream>	wav_stream(CLSID_SpStream);
		if (wav_stream
			&&	SUCCEEDED(wav_stream->BindToFile(string16(fn), SPFM_CREATE_ALWAYS, &guid_format, waveformat, SPFEI_ALL_EVENTS))
			&&	SUCCEEDED(voice->SetOutput(wav_stream, true))			// Set the voice's output to the wav file instead of the speakers
			) {
			ret = SUCCEEDED(voice->Speak(string16(text), SPF_DEFAULT, NULL));
			voice->SetOutput(old_stream, false);
		}

	}
	return ret;
}

class MySpStream : public com<ISpStreamFormat> {
	GUID			guid_format;
	WAVEFORMATEX*	waveformat;
public:
	malloc_block	buffer;
	size_t			offset;

	MySpStream(GUID guid_format, WAVEFORMATEX *waveformat) : guid_format(guid_format), waveformat(waveformat), offset(0)	{}
	malloc_block	GetData() { return buffer.resize(offset).detach(); }

	// IStream
	HRESULT STDMETHODCALLTYPE	Write(void const *pv, ULONG cb, ULONG *pcbWritten) {
		if (offset + cb > buffer.length())
			buffer.resize((offset + cb) * 2);
		memcpy(buffer + offset, pv, cb);
		offset		+= cb;
		if (pcbWritten)
			*pcbWritten	= cb;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE	Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) {
		if (dlibMove.QuadPart == 0 && dwOrigin != SEEK_END) {
			plibNewPosition->QuadPart = 0;
			return S_OK;
		}
		return E_FAIL;
	}
	// Unimplemented methods of IStream
	HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstatstg, DWORD grfStatFlag)														{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)															{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize)																{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags)																		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Revert()																							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)						{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm)																			{ return E_NOTIMPL; }

	//ISpStreamFormat
	HRESULT STDMETHODCALLTYPE GetFormat(GUID* pguidFormatId, WAVEFORMATEX** ppCoMemWaveFormatEx) {
		*pguidFormatId			= guid_format;
		*ppCoMemWaveFormatEx	= unique_com<WAVEFORMATEX>(*waveformat).detach();
		return S_OK;
	}

};

malloc_block TextToSpeech::SayToSample(const char *text) {
	com_ptr<ISpStreamFormat>    old_stream;
	GUID						guid_format;
	unique_com<WAVEFORMATEX>	waveformat;

	if (SUCCEEDED(voice->GetOutputStream(&old_stream))
	&&	SUCCEEDED(old_stream->GetFormat(&guid_format, &waveformat))
	&&	waveformat
	&&	waveformat->nAvgBytesPerSec && waveformat->nBlockAlign && waveformat->nChannels
	) {
		com_ptr<MySpStream>	stream(new MySpStream(guid_format, waveformat));
		if (SUCCEEDED(voice->SetOutput(stream, true))) {			// Set the voice's output to the wav file instead of the speakers
			bool	ret = SUCCEEDED(voice->Speak(string16(text), SPF_DEFAULT, NULL));
			voice->SetOutput(old_stream, false);
			if (ret)
				return stream->GetData();
		}

	}
	return none;
}

#elif defined PLAT_IOS

TextToSpeech::TextToSpeech(const char* language, float rate, float frequency) {
	synthesizer	= [AVSpeechSynthesizer new];
	voice		= [AVSpeechSynthesisVoice voiceWithLanguage: [NSString initFromUTF8: language]];
	this->rate	= rate;
}

bool TextToSpeech::Say(const char *speech) {
	if (synthesizer) {
		AVSpeechUtterance *utterance = [[AVSpeechUtterance alloc] initWithString:speech.GetNSString()];
		utterance.rate	= rate;
		utterance.voice = voice;
		[synthesizer speakUtterance : utterance];
		return true;
	}
	return false;
}

#elif define PLAT_ANDROID

TextToSpeech::TextToSpeech(const char* language, float rate, float frequency) {
	if (jni::ObjectHolder::env)
	jni::Call(jni::ObjectHolder::env, FJavaWrapper::GameActivityThis, "AndroidThunkJava_AndroidTTSInit");
	this->rate		= rate;
	this->language	= language;
}

bool TextToSpeech::Say(const char *speech) {
	if (jni::ObjectHolder::env) {
		jni::Call(jni::ObjectHolder::env, FJavaWrapper::GameActivityThis, "AndroidThunkJava_AndroidTTSSpeech", speech, rate, language);
		return true;
	}
	return false;
}

#endif
