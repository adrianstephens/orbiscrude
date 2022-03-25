#include "movie.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "extra/directshow.h"
#include "windows/dib.h"
#include <dvdmedia.h>
#include <wmsdkidl.h>

using namespace iso;

//-----------------------------------------------------------------------------
GUID CLSID_Dump = {0x36a5f770, 0xfe4c, 0x11ce, {0xa8, 0xed, 0x00, 0xaa, 0x00, 0x2f, 0xea, 0xb5}};

class DumpInputPin : public InputPin {
public:
	REFERENCE_TIME	last_time;
	malloc_block	buffer;
	bool			ready;

	DumpInputPin(Filter *_filter) : InputPin(_filter), ready(false) {}

	STDMETHODIMP	EnumMediaTypes(IEnumMediaTypes **ppEnum) {
		static AM_MEDIA_TYPE*	mts[1];
		mts[0] = &mt;
		*ppEnum = new MediaTypeEnumerator(make_range(mts));
		return S_OK;
	}

    bool			CheckMediaType(const AM_MEDIA_TYPE *pmt) {
		if (pmt->majortype == MEDIATYPE_Video && pmt->subtype == MEDIASUBTYPE_RGB24) {
			if (pmt->formattype == FORMAT_VideoInfo) {
				VIDEOINFOHEADER *v = (VIDEOINFOHEADER*)pmt->pbFormat;
				return v->bmiHeader.biBitCount == 24;
			} else if (pmt->formattype == FORMAT_VideoInfo2 || pmt->formattype == WMFORMAT_MPEG2Video) {
				VIDEOINFOHEADER2 *v = (VIDEOINFOHEADER2*)pmt->pbFormat;
				return v->bmiHeader.biBitCount == 24;
			}
		}
		return false;
	}
	STDMETHODIMP	Receive(IMediaSample *pSample);
};

class DumpFilter : public Filter {
	com_ptr<SeekPosPassThru>	seekpos;
public:
	DumpFilter() {
		pins.push_back(new DumpInputPin(this));
	}
// IPersist
	STDMETHODIMP GetClassID(CLSID *pClsID) {
		*pClsID = CLSID_Dump;
		return S_OK;
	}
//IMediaFilter
	DumpInputPin	*GetPin() {
		return (DumpInputPin*)pins[0];
	}

	STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
		if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
			if (!seekpos)
				*&seekpos = new SeekPosPassThru(pins[0]);
			*ppv = com_dup(seekpos);
			return S_OK;
		}
		return Filter::QueryInterface(riid, ppv);
	}
};

// Setup data

const REGPINTYPES			sudPinTypes = {&MEDIATYPE_NULL, &MEDIASUBTYPE_NULL};
const REGFILTERPINS			sudPins		= {L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, L"Output", 1, &sudPinTypes};

STDMETHODIMP DumpInputPin::Receive(IMediaSample *sample) {
	HRESULT			hr;
	REFERENCE_TIME	start, stop;
	hr			= sample->GetTime(&start, &stop);

	MediaTypeHolder	newmt;
	if (sample->GetMediaType(&newmt) == S_OK)
		mt = *newmt;

	BYTE			*data;
	hr = sample->GetPointer(&data);
	if (FAILED(hr))
		return hr;

	LONG			size	= sample->GetActualDataLength();
	buffer.resize(size);
	memcpy(buffer, data, size);

	last_time	= start;
	ready		= true;
	return S_FALSE;
};
//-----------------------------------------------------------------------------

class DirectShowFileHandler : public FileHandler {
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
public:
	DirectShowFileHandler()		{
		ISO::getdef<movie>();
	}
} ds;

#if 0
class MP4FileHandler : public DirectShowFileHandler {
	const char*		GetExt() override { return "mp4";	}
} mp4;
#endif

class DS_frames : public ISO::VirtualDefaults {
	com_ptr<IGraphBuilder>	graph;
	com_ptr<IMediaSeeking>	seeking;

	win::DIBHEADER			bmi;
	DumpInputPin*			dumppin;
	REFERENCE_TIME			frame;
	DWORD					num_streams;
	int						audio_stream, video_stream;
	WORD					*streams;
	int						prev_frame;
	int						num_frames;
	ISO_ptr<bitmap>			prev_bitmap;

	ISO_ptr<bitmap>	GetFrame(int i);

	bool	SetFormat(const MediaType *mt) {
		if (mt->formattype == FORMAT_VideoInfo) {
			if (VIDEOINFOHEADER *v = mt->GetFormat()) {
				bmi			= v->bmiHeader;
				frame		= v->AvgTimePerFrame;
				return true;
			}
		} else if (mt->formattype == FORMAT_VideoInfo2 || mt->formattype == WMFORMAT_MPEG2Video) {
			if (VIDEOINFOHEADER2 *v = mt->GetFormat()) {
				bmi			= v->bmiHeader;
				frame		= v->AvgTimePerFrame;
				return true;
			}
		}
		return false;
	}

public:
	int						Count()			const		{ return num_frames;	}
	ISO::Browser2			Index(int i)				{ return GetFrame(i);	}
	int						Width()			const		{ return bmi.Width();	}
	int						Height()		const		{ return bmi.Height();	}
	float					FrameRate()		const		{ return 1e7f / frame;	}

	DS_frames(const filename &fn);
};
ISO_DEFVIRT(DS_frames);

GUID STATIC_KSDATAFORMAT_SPECIFIER_VIDEOINFO2= {0xf72a76A0L, 0xeb0a, 0x11d0, {0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba}};

DS_frames::DS_frames(const filename &fn) {
	init_com();

	HRESULT	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&graph);
	if (FAILED(hr))
		return;

#if 0
	hr = graph->RenderFile(str16(fn), NULL);
	hr = SaveGraphFile(graph, "d:\\render.grf");
#else
	com_ptr<IBaseFilter> source;
	hr = graph->AddSourceFilter(str16(fn), L"Source", &source);
	if (FAILED(hr))
		return;

	com_iterator<IEnumPins, IPin>	pins;
	source->EnumPins(&pins);
	while (com_ptr<IPin> &pin = ++pins) {
		PIN_DIRECTION	dir;
		if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT && PinHasMajorType(pin, MEDIATYPE_Video)) {
			com_ptr<IAMStreamConfig>	config;
			MediaTypeHolder				mt;

			if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, (void**)&config)) && SUCCEEDED(config->GetFormat(&mt))) {
				SetFormat(mt);

			} else {
				com_ptr<IEnumMediaTypes> types;
				ULONG					n;
				if (SUCCEEDED(pin->EnumMediaTypes(&types)) && types->Next(1, &mt, &n) == S_OK)
					SetFormat(mt);
			}
			DumpFilter	*dump = new DumpFilter;

//			MediaTypeHolder	mt2(MEDIATYPE_Video);
//			mt2->subtype	= MEDIASUBTYPE_RGB24;
//			dump->SetFormat(mt2);

			dumppin	= dump->GetPin();

			hr	= graph->AddFilter(dump, L"Dump");
			hr	= graph->Connect(pin, dumppin);
		}
	}
#endif

	hr = graph.query<IMediaControl>()->Pause();
	hr = graph.query(&seeking);

	LONGLONG	duration;
	hr	= seeking->GetDuration(&duration);
	num_frames	= duration / frame;

	LONGLONG	current = 0;
	hr = seeking->SetPositions(&current, AM_SEEKING_AbsolutePositioning, &current, AM_SEEKING_AbsolutePositioning);

	if (FAILED(hr))
		return;

	// Wait for completion.
//	long evCode;
//	graph.query<IMediaEvent>()->WaitForCompletion(INFINITE, &evCode);
}

ISO_ptr<bitmap> DS_frames::GetFrame(int i) {
	LONGLONG	current = i *frame;
	dumppin->ready = false;

//	graph.query<IVideoFrameStep>()->Step(

	seeking->SetPositions(&current, AM_SEEKING_AbsolutePositioning, &current, AM_SEEKING_AbsolutePositioning);
//	for (int n = 200; n && !dumppin->ready != current; n--)
//		Thread::sleep(.01f);
	return bmi.CreateBitmap(tag(), dumppin->buffer);
}

ISO_ptr<void> DirectShowFileHandler::ReadWithFilename(tag id, const filename &fn) {
	ISO_ptr<DS_frames>	frames(NULL, fn);
	if (frames->Count())
		return ISO_ptr<movie>(id, frames);
	return ISO_NULL;
}