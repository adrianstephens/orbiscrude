#include "main.h"
#include "device.h"

#if 0
#include "Mfapi.h"
#include "Mfidl.h"
#include "Mfobjects.h"
#include "Mfreadwrite.h"
#include "Mferror.h"
#else
#include <evr.h>
#include <d3d9.h>
#include <vmr9.h>
#include <dvdmedia.h>
#endif

#include "extra/directshow.h"
#include "windows/registry.h"
#include "iso/iso_files.h"

using namespace app;

#if 0
//------------------------------------------------------------------------------
//	Windows Media Foundation
//------------------------------------------------------------------------------

class VideoCapture : public ISO::VirtualDefaults {
	uint32			count;
	IMFActivate		**devices;
	ISO_ptr<void>	*ptrs;
public:
	VideoCapture();
	~VideoCapture();
	size_t			Count()					{ return count;			}
	tag				GetName(int i)			{ return ptrs[i].ID();	}
	ISO::Browser2	Index(int i);
};

class VideoCaptureDevice : public ISO::VirtualDefaults {
	IMFActivate					*device;
	com_ptr<IMFMediaSource>		source;
	com_ptr<IMFSourceReader>	reader;
public:
	VideoCaptureDevice(IMFActivate *_device) : device(_device) {
		com_ptr<IMFAttributes>	config;

		if (FAILED(device->ActivateObject(source.uuid(), (void**)&source))
		||	FAILED(MFCreateAttributes(&config, 1))
		||	FAILED(config->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE))
		||	FAILED(MFCreateSourceReaderFromMediaSource(source, config, &reader))
		) return;

		GUID		major, sub;
		int			stream_index = 0;

		// Find the native format of the stream.
		for (int type_index = 0; ; type_index++) {
			com_ptr<IMFMediaType>	native, type;
			if (SUCCEEDED(reader->GetNativeMediaType(stream_index, type_index, &native))
			&&	SUCCEEDED(native->GetGUID(MF_MT_MAJOR_TYPE, &major))
			&&	SUCCEEDED(native->GetGUID(MF_MT_SUBTYPE, &sub))
			) {
//				reader->SetCurrentMediaType(stream_index, NULL, native);
				// Select a subtype.
				if (major == MFMediaType_Video)
					sub = MFVideoFormat_RGB32;
				else if (major == MFMediaType_Audio)
					sub = MFAudioFormat_PCM;

				if (SUCCEEDED(MFCreateMediaType(&type))
				&&	SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, major))
				&&	SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, sub))
				&&	SUCCEEDED(reader->SetCurrentMediaType(stream_index, NULL, type))
				) {
					DWORD		stream_index, flags;
					LONGLONG	time_stamp;
					com_ptr<IMFSample>	sm;
					reader->ReadSample(
						MF_SOURCE_READER_ANY_STREAM,	// Stream index.
						0,								// Flags.
						&stream_index,					// Receives the actual stream index.
						&flags,							// Receives status flags.
						&time_stamp,					// Receives the time stamp.
						&sm							// Receives the sm or NULL.
					);
				}
			} else {
				break;
			}
		}
	}
};
ISO_DEFVIRT(VideoCapture);
ISO_DEFVIRT(VideoCaptureDevice);

VideoCapture::VideoCapture() : count(0) {
	com_ptr<IMFAttributes>	config;
	if (SUCCEEDED(MFCreateAttributes(&config, 1))					// Create an attribute store to hold the search criteria.
		&&	SUCCEEDED(config->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))// Request video capture devices.
		&&	SUCCEEDED(MFEnumDeviceSources(config, &devices, &count))	// Enumerate the devices
		&&	count) {

			ptrs = new ISO_ptr<void>[count];
			com_string	name;
			uint32		len;
			for (int i = 0; i < count; i++) {
				if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len)))
					ptrs[i] = ISO_ptr<VideoCaptureDevice>(fixed_string<256>(name), devices[i]);
			}
	}
}

VideoCapture::~VideoCapture() {
	for (int i = 0; i < count; i++)
		devices[i]->Release();
	CoTaskMemFree(devices);
}

ISO::Browser2 VideoCapture::Index(int i) {
	return *ISO::Browser(ptrs[i]);
}

ISO_ptr<void> GetVideoCaptureMF(tag id) {
	ISO_ptr<VideoCapture>	p(id);
	if (p->Count())
		return p;
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
#if 0
class MF_frames : public ISO::VirtualDefaults {
	com_ptr<IMFSourceReader>	reader;
	int		width, height;
	float	frame_rate;

	ISO_ptr<bitmap>	bm;
	ISO_ptr<bitmap>	GetFrame(int i) {
		return bm;
	}
public:
	MF_frames(const filename &fn) : frame_rate(0) {
		com_ptr<IMFAttributes>		config;
		HRESULT hr = MFCreateSourceReaderFromURL(com_string(format_string("file:\\%s", (const char*)fn)), config, &reader);
		frame_rate = 30.f;
	}
	~MF_frames();
	int					Count()							{ return 100;			}
	ISO::Browser2		Index(int i)					{ return GetFrame(i);	}

	int					Width()							{ return width;			}
	int					Height()						{ return height;		}
	float				FrameRate()						{ return frame_rate;	}
};

template<> struct ISO_def<MF_frames> : public ISO::VirtualT<MF_frames> {};

class MFFileHandler : public FileHandler {
	virtual	const char*		GetExt()					{ return "mp4";	}

	virtual	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) {
		ISO_ptr<MF_frames>	frames(NULL, fn);
		if (frames->FrameRate())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} mf;
#endif

#endif
//------------------------------------------------------------------------------
//	DirectShow
//------------------------------------------------------------------------------

class VideoCaptureDeviceDS : public ISO::VirtualDefaults {
public:
	com_ptr<IBaseFilter>	source;
	string					name;
	VideoCaptureDeviceDS(IBaseFilter *_source, const com_string &_name) : source(_source), name(_name) {}
};

class VideoCaptureDS : public ISO::VirtualDefaults {
	dynamic_array<VideoCaptureDeviceDS>	ptrs;
public:
	VideoCaptureDS();
	size_t			Count()					{ return ptrs.size();	}
	tag				GetName(int i)			{ return ptrs[i].name;	}
	ISO::Browser2	Index(int i)			{ return ISO::MakeBrowser(ptrs[i]);		}
};

VideoCaptureDS::VideoCaptureDS() {
	com_ptr<ICreateDevEnum>	devices;
	com_ptr<IEnumMoniker>	classes;

	HRESULT	hr;
	devices.create(CLSID_SystemDeviceEnum);
	hr = devices->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &classes, 0);

	if (classes) {
		for (com_ptr<IMoniker> moniker; classes->Next(1, &moniker, NULL) == S_OK; moniker.clear()) {
	        com_ptr<IPropertyBag>	props;
			if (SUCCEEDED(moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&props))) {
				com_variant	name, path;
				props->Read(L"FriendlyName", &name, NULL);
				props->Read(L"DevicePath", &path, NULL);

				IBaseFilter	*source;
				if (SUCCEEDED(moniker->BindToObject(0,0, IID_IBaseFilter, (void**)&source)))
					new(ptrs) VideoCaptureDeviceDS(source, com_string(name));
			}
		}
	}
}

ISO_DEFVIRT(VideoCaptureDS);
ISO_DEFVIRT(VideoCaptureDeviceDS);

ISO_ptr<void> GetVideoCaptureDS(tag id) {
	ISO_ptr<VideoCaptureDS>	p(id);
	if (p->Count())
		return p;
	return ISO_NULL;
}

//------------------------------------------------------------------------------
//	DirectShow Graph
//------------------------------------------------------------------------------

class DirectShowPin {
public:
	com_ptr<IPin>	pin;
	DirectShowPin(IPin *_pin) : pin(_pin) { pin->AddRef(); }
	ISO::Browser2	Connection() const {
		com_ptr<IPin> pin2;
		if (SUCCEEDED(pin->ConnectedTo(&pin2))) {
			PIN_INFO	info;
			pin2->QueryPinInfo(&info);
			return ISO_ptr<DirectShowPin>(str(info.achName), pin2);
		}
		return ISO_NULL;
	}

	string			Filter() const {
		PIN_INFO	pin_info;
		FILTER_INFO	filter_info;
		pin->QueryPinInfo(&pin_info);
		pin_info.pFilter->QueryFilterInfo(&filter_info);
		pin_info.pFilter->Release();
		filter_info.pGraph->Release();
		return filter_info.achName;
	}

	const char		*Direction() const {
		PIN_DIRECTION	dir;
		return SUCCEEDED(pin->QueryDirection(&dir)) ? (
			dir == PINDIR_INPUT ? "input" : "output"
		) : "unknown";
	}

	ISO_ptr<void>	MediaType() const {
		iso::MediaType	mt;
		if (SUCCEEDED(pin->ConnectionMediaType(&mt)))
			return ISO::MakePtr(0, mt);
		return ISO_NULL;
	}
};

ISO_DEFUSERCOMPV(MediaType, majortype, subtype, bFixedSizeSamples, bTemporalCompression, lSampleSize, formattype, GetFormat);
ISO_DEFUSERCOMPV(DirectShowPin, Connection, Filter, Direction, MediaType);

class DirectShowFilter : public ISO::VirtualDefaults {
public:
	com_ptr<IBaseFilter>	filter;
	DirectShowFilter(IBaseFilter *_filter) : filter(_filter) { filter->AddRef(); }

	uint32			Count()	const {
		uint32	n = 0;
		com_ptr<IEnumPins>	pins;
		if (SUCCEEDED(filter->EnumPins(&pins))) {
			for (com_ptr<IPin> pin; pins->Next(1, &pin, NULL) == S_OK; pin.clear())
				n++;
		}
		return n;
	}
	tag2			GetName(int i) const {
		com_ptr<IEnumPins>	pins;
		if (SUCCEEDED(filter->EnumPins(&pins))) {
			for (com_ptr<IPin> pin; pins->Next(1, &pin, NULL) == S_OK; pin.clear()) {
				if (i-- == 0) {
					PIN_INFO	info;
					pin->QueryPinInfo(&info);
					return str(info.achName);
				}
			}
		}
		return tag2();
	}
	ISO::Browser2	Index(int i) const {
		com_ptr<IEnumPins>	pins;
		if (SUCCEEDED(filter->EnumPins(&pins))) {
			for (com_ptr<IPin> pin; pins->Next(1, &pin, NULL) == S_OK; pin.clear()) {
				if (i-- == 0) {
					PIN_INFO	info;
					pin->QueryPinInfo(&info);
					return ISO_ptr<DirectShowPin>(str(info.achName), pin);
				}
			}
		}
		return ISO::Browser2();
	}
};
ISO_DEFUSERVIRT(DirectShowFilter);

class DirectShowGraph : public ISO::VirtualDefaults {
public:
	com_ptr<IGraphBuilder>	graph;

	uint32			Count()	const {
		uint32	n = 0;
		com_ptr<IEnumFilters>	filters;
		if (SUCCEEDED(graph->EnumFilters(&filters))) {
			for (com_ptr<IBaseFilter> filter; filters->Next(1, &filter, NULL) == S_OK; filter.clear())
				n++;
		}
		return n;
	}
	tag2			GetName(int i) const {
		com_ptr<IEnumFilters>	filters;
		if (SUCCEEDED(graph->EnumFilters(&filters))) {
			for (com_ptr<IBaseFilter> filter; filters->Next(1, &filter, NULL) == S_OK; filter.clear()) {
				if (i-- == 0) {
					FILTER_INFO	info;
					filter->QueryFilterInfo(&info);
					return str(info.achName);
				}
			}
		}
		return tag2();
	}
	ISO::Browser2	Index(int i) const {
		com_ptr<IEnumFilters>	filters;
		if (SUCCEEDED(graph->EnumFilters(&filters))) {
			for (com_ptr<IBaseFilter> filter; filters->Next(1, &filter, NULL) == S_OK; filter.clear()) {
				if (i-- == 0) {
					FILTER_INFO	info;
					filter->QueryFilterInfo(&info);
					return ISO_ptr<DirectShowFilter>(str(info.achName), filter);
				}
			}
		}
		return ISO::Browser2();
	}
};
ISO_DEFUSERVIRT(DirectShowGraph);

class GRFFileHandler : public FileHandler {
	virtual	const char*		GetExt()				{ return "grf";	}
	virtual	const char*		GetDescription()		{ return "DirectShow Graph"; }
	virtual	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) {
		ISO_ptr<DirectShowGraph>	p(id);
		p->graph.create(CLSID_FilterGraph);
		if (SUCCEEDED(LoadGraphFile(p->graph, fn)))
			return p;
		return ISO_NULL;
	}
	virtual bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
		return p.IsType<DirectShowGraph>()
			&& SUCCEEDED(SaveGraphFile(((DirectShowGraph*)p)->graph, fn));
	}
} grf;

//------------------------------------------------------------------------------
//	DirectShow Helpers
//------------------------------------------------------------------------------

const GUID PIN_CATEGORY_ROXIOCAPTURE	= {0x6994AD05, 0x93EF, 0x11D0, {0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};
const GUID MEDIASUBTYPE_I420			= {0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

enum VideoOutputType {
    VideoOutputType_None,
    VideoOutputType_RGB24,
    VideoOutputType_RGB32,
    VideoOutputType_ARGB32,

    VideoOutputType_I420,
    VideoOutputType_YV12,

    VideoOutputType_Y41P,
    VideoOutputType_YVU9,

    VideoOutputType_YVYU,
    VideoOutputType_YUY2,
    VideoOutputType_UYVY,
    VideoOutputType_HDYC,

    VideoOutputType_MPEG2_VIDEO,
    VideoOutputType_H264,

    VideoOutputType_dvsl,
    VideoOutputType_dvsd,
    VideoOutputType_dvhd,

    VideoOutputType_MJPG
};

struct MediaOutputInfo {
	VideoOutputType type;
	AM_MEDIA_TYPE	*mt;
	uint64	min_frame, max_frame;
	uint32	min_width, min_height;
	uint32	max_width, max_height;
	uint32	xGranularity, yGranularity;
	bool	fourCC;

	uint32	ClosestWidth(uint32 width) const {
		return min_width == max_width ? min_width
			: min_width + ((clamp(width, min_width, max_width) - min_width) / xGranularity) * xGranularity;
	}

	uint32	ClosestHeight(uint32 width) const {
		return min_height == max_height ? min_height
			: min_height + ((clamp(width, min_height, max_height) - min_height) / yGranularity) * yGranularity;
	}

};

void AddOutput(AM_MEDIA_TYPE *mt, VIDEO_STREAM_CONFIG_CAPS *vscc, bool allowV2, dynamic_array<MediaOutputInfo> &outputs) {
    VideoOutputType type = VideoOutputType_None;

	if (mt->majortype == MEDIATYPE_Video) {
		// Packed RGB formats
		if(mt->subtype == MEDIASUBTYPE_RGB24)
			type = VideoOutputType_RGB24;
		else if(mt->subtype == MEDIASUBTYPE_RGB32)
			type = VideoOutputType_RGB32;
		else if(mt->subtype == MEDIASUBTYPE_ARGB32)
			type = VideoOutputType_ARGB32;

		// Planar YUV formats
		else if(mt->subtype == MEDIASUBTYPE_I420)
			type = VideoOutputType_I420;
		else if(mt->subtype == MEDIASUBTYPE_IYUV)
			type = VideoOutputType_I420;
		else if(mt->subtype == MEDIASUBTYPE_YV12)
			type = VideoOutputType_YV12;

		else if(mt->subtype == MEDIASUBTYPE_Y41P)
			type = VideoOutputType_Y41P;
		else if(mt->subtype == MEDIASUBTYPE_YVU9)
			type = VideoOutputType_YVU9;

		// Packed YUV formats
		else if(mt->subtype == MEDIASUBTYPE_YVYU)
			type = VideoOutputType_YVYU;
		else if(mt->subtype == MEDIASUBTYPE_YUY2)
			type = VideoOutputType_YUY2;
		else if(mt->subtype == MEDIASUBTYPE_UYVY)
			type = VideoOutputType_UYVY;

		else if(mt->subtype == MEDIASUBTYPE_MPEG2_VIDEO)
			type = VideoOutputType_MPEG2_VIDEO;

		else if(mt->subtype == MEDIASUBTYPE_H264)
			type = VideoOutputType_H264;

		else if(mt->subtype == MEDIASUBTYPE_dvsl)
			type = VideoOutputType_dvsl;
		else if(mt->subtype == MEDIASUBTYPE_dvsd)
			type = VideoOutputType_dvsd;
		else if(mt->subtype == MEDIASUBTYPE_dvhd)
			type = VideoOutputType_dvhd;

		else if(mt->subtype == MEDIASUBTYPE_MJPG)
			type = VideoOutputType_MJPG;
	}

	if (mt->formattype == FORMAT_VideoInfo || (allowV2 && mt->formattype == FORMAT_VideoInfo2)) {
		VIDEOINFOHEADER		*vih		= (VIDEOINFOHEADER*)(mt->pbFormat);
		BITMAPINFOHEADER	*bmiHeader	= mt->formattype == FORMAT_VideoInfo ? &((VIDEOINFOHEADER*)mt->pbFormat)->bmiHeader : &((VIDEOINFOHEADER2*)mt->pbFormat)->bmiHeader;
		bool				fourCC		= false;

		if (type == VideoOutputType_None) {
			switch (bmiHeader->biCompression) {
				// Packed RGB formats
				case '2BGR':	type = VideoOutputType_RGB32;	break;
				case '4BGR':	type = VideoOutputType_RGB24;	break;
				case 'ABGR':	type = VideoOutputType_ARGB32;	break;
				// Planar YUV formats
				case '024I':
				case 'VUYI':	type = VideoOutputType_I420;	break;
				case '21VY':	type = VideoOutputType_YV12;	break;
				// Packed YUV formats
				case 'UYVY':	type = VideoOutputType_YVYU;	break;
				case '2YUY':	type = VideoOutputType_YUY2;	break;
				case 'YVYU':	type = VideoOutputType_UYVY;	break;
				case 'CYDH':	type = VideoOutputType_HDYC;	break;
				case 'V4PM':
				case '2S4M':	type = VideoOutputType_MPEG2_VIDEO;	break;
				case '462H':	type = VideoOutputType_H264;	break;
				case 'GPJM':	type = VideoOutputType_MJPG;	break;
			}
			fourCC = true;
		}

		if (type != VideoOutputType_None) {
			MediaOutputInfo &output = outputs.push_back();

			if (vscc) {
				output.min_frame	= vscc->MinFrameInterval;
				output.max_frame	= vscc->MaxFrameInterval;
				output.min_width	= vscc->MinOutputSize.cx;
				output.max_width	= vscc->MaxOutputSize.cx;
				output.min_height	= vscc->MinOutputSize.cy;
				output.max_height	= vscc->MaxOutputSize.cy;

				if (!output.min_width || !output.min_height || !output.max_width || !output.max_height) {
					output.min_width	= output.max_width	= bmiHeader->biWidth;
					output.min_height	= output.max_height = bmiHeader->biHeight;
				}

				output.xGranularity = max(vscc->OutputGranularityX, 1);
				output.yGranularity = max(vscc->OutputGranularityY, 1);

			} else {
				output.min_width	= output.max_width	= bmiHeader->biWidth;
				output.min_height	= output.max_height = bmiHeader->biHeight;
				output.min_frame	= output.max_frame	= vih->AvgTimePerFrame ? vih->AvgTimePerFrame : 10000000/30; //elgato hack
				output.xGranularity	= output.yGranularity = 1;
			}

			output.mt		= mt;
			output.type		= type;
			output.fourCC	= fourCC;
			return;
		}
	}
}


void GetOutputList(IPin *pin, dynamic_array<MediaOutputInfo> &outputs) {
	com_ptr<IAMStreamConfig> config;
	if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, (void**)&config))) {
		int count, size;
		HRESULT hr;
		if (SUCCEEDED(hr = config->GetNumberOfCapabilities(&count, &size))) {
			malloc_block	caps(size);
			for (int i = 0; i < count; i++) {
				MediaTypeHolder mt;
				if (SUCCEEDED(config->GetStreamCaps(i, &mt, caps)))
					AddOutput(mt, caps, true, outputs);
			}
		} else if (hr == E_NOTIMPL) { //...usually elgato.
			com_ptr<IEnumMediaTypes>	types;
			if (SUCCEEDED(pin->EnumMediaTypes(&types))) {
				ULONG			i;
				MediaTypeHolder	mt;
				if (types->Next(1, &mt, &i) == S_OK)
					AddOutput(mt, 0, true, outputs);
			}
		}
	}
}

MediaOutputInfo *FindBestOutput(dynamic_array<MediaOutputInfo> &outputs, uint32 width, uint32 height, uint64 frame, bool prioritize_FPS) {
	uint32			bestSizeDist	= ~uint32(0);
	uint64			bestFrameDist	= ~uint64(0);
	MediaOutputInfo	*best			= 0;

	for (auto i = outputs.begin(), e = outputs.end(); i != e; ++i) {
		uint32	totalDist	= width - i->ClosestWidth(width) + height - i->ClosestHeight(height);
		uint64	frameDist	= frame < i->min_frame ? i->min_frame - frame : frame > i->max_frame ? frame - i->max_frame : 0;

		if (prioritize_FPS
			? (frameDist != bestFrameDist	? (frameDist < bestFrameDist) : (totalDist < bestSizeDist))
			: (totalDist != bestSizeDist	? (totalDist < bestSizeDist) : (frameDist < bestFrameDist))
		) {
			best			= i;
			bestSizeDist	= totalDist;
			bestFrameDist	= frameDist;
		}
	}
	return best;
}

//-------------	IVideoWindow
Point GetNativeVideoSize(IVideoWindow *v) {
	long	width, height;
	v->GetMinIdealImageSize(&width, &height);
	return Point(width, height);
}
HRESULT SetVideoPosition(IVideoWindow *v, const Rect &dest) {
	return v->SetWindowPosition(dest.Left(), dest.Top(), dest.Width(), dest.Height());
}
void PassMessage(IVideoWindow *v, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	v->NotifyOwnerMessage((LONG_PTR)hWnd, message, wParam, lParam);
}
void Repaint(IVideoWindow *v, HWND hWnd, HDC hDC) {
	v->put_Visible(OATRUE);
}
HRESULT Initialise(IVideoWindow **v, HWND hWnd, IGraphBuilder *graph, IBaseFilter *source, IPin *pin) {
	HRESULT	hr;
#if 0
	com_ptr<IBaseFilter>		vmr;
	vmr.create(CLSID_VideoRendererDefault);
	hr = graph->AddFilter(vmr, L"Video Renderer");

	com_ptr<IPin>	render_pin		= GetPin(vmr, PINDIR_INPUT, 0, 0, 0);
	hr = graph->Connect(pin, render_pin);
#elif 0
	hr = graph->Render(pin);
#else
	com_ptr<ICaptureGraphBuilder2>	capture;
	capture.create(CLSID_CaptureGraphBuilder2)->SetFiltergraph(graph);
	hr = capture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, source, NULL, NULL);
#endif
	hr = query(graph, v);
	hr = (*v)->put_Owner((OAHWND)hWnd);
	hr = (*v)->put_WindowStyle(Control::CHILD | Control::CLIPCHILDREN);
	return hr;
}

//-------------	IVMRWindowlessControl
Point GetNativeVideoSize(IVMRWindowlessControl *v) {
	long	width, height;
	v->GetNativeVideoSize(&width, &height, NULL, NULL);
	return Point(width, height);
}
HRESULT SetVideoPosition(IVMRWindowlessControl *v, const Rect &dest) {
	return v->SetVideoPosition(0, (LPRECT)&dest);
}
void PassMessage(IVMRWindowlessControl *v, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
}
void Repaint(IVMRWindowlessControl *v, HWND hWnd, HDC hDC) {
	v->RepaintVideo(hWnd, hDC);
}
HRESULT Initialise(IVMRWindowlessControl **v, HWND hWnd, IGraphBuilder *graph, IBaseFilter *source, IPin *pin) {
	HRESULT	hr;
	com_ptr<ICaptureGraphBuilder2>	capture;
	capture.create(CLSID_CaptureGraphBuilder2)->SetFiltergraph(graph);

	com_ptr<IBaseFilter>		vmr;
	com_ptr<IVMRFilterConfig>	config;
	vmr.create(CLSID_VideoMixingRenderer);
		SUCCEEDED(hr = graph->AddFilter(vmr, L"Video Mixing Renderer"))
	&&	SUCCEEDED(hr = vmr.query(&config))
	&&	SUCCEEDED(hr = config->SetRenderingMode(VMRMode_Windowless))
	&&	SUCCEEDED(hr = vmr.query(v))
	&&	SUCCEEDED(hr = (*v)->SetVideoClippingWindow(hWnd))
	&&	SUCCEEDED(hr = capture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, source, NULL, vmr));
	return hr;
}

//-------------	IVMRWindowlessControl9
Point GetNativeVideoSize(IVMRWindowlessControl9 *v) {
	long	width, height;
	v->GetNativeVideoSize(&width, &height, NULL, NULL);
	return Point(width, height);
}
HRESULT SetVideoPosition(IVMRWindowlessControl9 *v, const Rect &dest) {
	return v->SetVideoPosition(0, (LPRECT)&dest);
}
void PassMessage(IVMRWindowlessControl9 *v, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
}
void Repaint(IVMRWindowlessControl9 *v, HWND hWnd, HDC hDC) {
	v->RepaintVideo(hWnd, hDC);
}
HRESULT Initialise(IVMRWindowlessControl9 **v, HWND hWnd, IGraphBuilder *graph, IBaseFilter *source, IPin *pin) {
	HRESULT	hr;
	com_ptr<ICaptureGraphBuilder2>	capture;
	capture.create(CLSID_CaptureGraphBuilder2)->SetFiltergraph(graph);

	com_ptr<IBaseFilter>		vmr;
	com_ptr<IVMRFilterConfig9>	config;
	vmr.create(CLSID_VideoMixingRenderer9);
		SUCCEEDED(hr = graph->AddFilter(vmr, L"Video Mixing Renderer"))
	&&	SUCCEEDED(hr = vmr.query(&config))
	&&	SUCCEEDED(hr = config->SetRenderingMode(VMRMode_Windowless))
	&&	SUCCEEDED(hr = vmr.query(v))
	&&	SUCCEEDED(hr = (*v)->SetVideoClippingWindow(hWnd))
	&&	SUCCEEDED(hr = capture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, source, NULL, vmr));
	return hr;
}

//-------------	IMFVideoDisplayControl
Point GetNativeVideoSize(IMFVideoDisplayControl *v) {
	SIZE	s;
	v->GetNativeVideoSize(&s, NULL);
	return Point(s.cx, s.cy);
}
HRESULT SetVideoPosition(IMFVideoDisplayControl *v, const Rect &dest) {
	return v->SetVideoPosition(0, (LPRECT)&dest);
}
void PassMessage(IMFVideoDisplayControl *v, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
}
void Repaint(IMFVideoDisplayControl *v, HWND hWnd, HDC hDC) {
	v->RepaintVideo();
}
HRESULT Initialise(IMFVideoDisplayControl **v, HWND hWnd, IGraphBuilder *graph, IBaseFilter *source, IPin *pin) {
	HRESULT	hr;
	com_ptr<ICaptureGraphBuilder2>	capture;
	capture.create(CLSID_CaptureGraphBuilder2)->SetFiltergraph(graph);

	com_ptr<IBaseFilter>	evr;
	com_ptr<IMFGetService>	gs;
	evr.create(CLSID_EnhancedVideoRenderer);
		SUCCEEDED(hr = evr.query(&gs))
	&&	SUCCEEDED(hr = gs->GetService(MR_VIDEO_RENDER_SERVICE, IID_IMFVideoDisplayControl, (void**)v))
	&&	SUCCEEDED(hr = (*v)->SetVideoWindow(hWnd))
	&&	SUCCEEDED(hr = (*v)->SetAspectRatioMode(MFVideoARMode_PreservePicture))
	&&	SUCCEEDED(hr = graph->AddFilter(evr, L"Video Mixing Renderer"))
	&&	SUCCEEDED(hr = capture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, source, NULL, evr));
	return hr;
}

//------------------------------------------------------------------------------
//	DirectShow Viewer
//------------------------------------------------------------------------------

DWORD AddToROT(IUnknown *graph) {
	com_ptr<IMoniker>				moniker;
	com_ptr<IRunningObjectTable>	rot;
	GetRunningObjectTable(0, &rot);

	WCHAR wsz[256];
	swprintf_s(wsz, L"FilterGraph %08p pid %08x", (void*)graph, GetCurrentProcessId());
	CreateItemMoniker(L"!", wsz, &moniker);

	DWORD	reg;
	rot->Register(0, graph, moniker, &reg);
	return reg;
}
void RemoveFromROT(DWORD reg) {
	com_ptr<IRunningObjectTable>	rot;
	GetRunningObjectTable(0, &rot);
	rot->Revoke(reg);
}

class ViewDirectShow : public Window<ViewDirectShow> {
	enum { WM_GRAPHNOTIFY  = WM_APP+1 };
	typedef IVideoWindow				VideoControl;
	//typedef IVMRWindowlessControl		VideoControl;
	//typedef IVMRWindowlessControl9	VideoControl;
	//typedef IMFVideoDisplayControl	VideoControl;

	MainWindow				&main;
	com_ptr<IGraphBuilder>	graph;
	com_ptr<IMediaEventEx>	media_event;
	com_ptr<VideoControl>	video_control;
	com_ptr<IVideoWindow>	video_window;
	DWORD					reg;

	Point	GetNativeVideoSize() const {
		return	video_window	? ::GetNativeVideoSize(video_window)
			:	video_control	? ::GetNativeVideoSize(video_control)
			:	Point(0,0);
	}
	HRESULT SetVideoPosition(const Rect &dest) {
		return	video_window	? ::SetVideoPosition(video_window, dest)
			:	video_control	? ::SetVideoPosition(video_control, dest)
			:	S_FALSE;
	}

public:

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE: {
				Point	size(lParam);
				Point	vid	= GetNativeVideoSize();

				if (vid.x && vid.y) {
					if (size.x * vid.y > size.y * vid.x) {
						//vertical bars
						int	w	= size.y * vid.x / vid.y;
						SetVideoPosition(Rect((size.x - w) / 2, 0, w, size.y));
					} else {
						//horizontal bars
						int	h	= size.x * vid.y / vid.x;
						SetVideoPosition(Rect(0, (size.y - h) / 2, size.x, h));
					}
				}
				break;
			}

			case WM_PAINT: {
				DeviceContext	dc(hWnd);
				Point	size	= GetClientRect().Size();
				Point	vid		= GetNativeVideoSize();

				if (video_control)
					Repaint(video_control, hWnd, dc);

				if (size.x * vid.y > size.y * vid.x) {
					//vertical bars
					int	w = size.y * vid.x / vid.y;
					int	s = (size.x - w) / 2;
					dc.Fill(Rect(0, 0, s, size.y), Brush::Grey());
					dc.Fill(Rect(s + w, 0, s, size.y), Brush::Grey());
				} else {
					//horizontal bars
					int	h = size.x * vid.y / vid.x;
					int	s = (size.y - h) / 2;
					dc.Fill(Rect(0, 0, size.x, s), Brush::Grey());
					dc.Fill(Rect(0, s + h, size.x, s), Brush::Grey());
				}
				break;
			}
#if 0
			case WM_DISPLAYCHANGE:
				if (video_control) {
					video_control->DisplayModeChanged();
				}
				break;
#endif

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
				SetFocus();
				break;

			case WM_MOUSEMOVE:
				break;

			case WM_GRAPHNOTIFY: {
				LONG		evCode;
				LONG_PTR	evParam1, evParam2;
				while (SUCCEEDED(media_event->GetEvent(&evCode, &evParam1, &evParam2, 0)))
					media_event->FreeEventParams(evCode, evParam1, evParam2);
				break;
			}

			case WM_WINDOWPOSCHANGED:
//				ChangePreviewState(!IsIconic(hWnd));
				break;

			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		if (video_window)
			PassMessage(video_window, hWnd, message, wParam, lParam);
		else if (video_control)
			PassMessage(video_control, hWnd, message, wParam, lParam);
		return Super(message, wParam, lParam);
	}


	ViewDirectShow(MainWindow &_main, const WindowPos &pos, VideoCaptureDeviceDS *device) : main(_main), reg(0) {
		Create(pos, "Video", CHILD, CLIENTEDGE);
		HRESULT hr;

		uint32		width = 1920, height = 1080, frame = 333333;

		graph.create(CLSID_FilterGraph);

		hr = graph.query(&media_event);
		hr = media_event->SetNotifyWindow((OAHWND)hWnd, WM_GRAPHNOTIFY, 0);		// Set the window handle used to process graph events

		dynamic_array<MediaOutputInfo>	outputs;
		GUID			cats[]	= {PIN_CATEGORY_CAPTURE, PIN_CATEGORY_ROXIOCAPTURE};
		com_ptr<IPin>	pin		= GetPin(device->source, PINDIR_OUTPUT, &MEDIATYPE_Video, cats, uint32(num_elements(cats)));

		GetOutputList(pin, outputs);
		MediaOutputInfo *best = FindBestOutput(outputs, width, height, frame, false);

		hr = ConfigurePin(pin, best->mt, width, height, frame);

		hr = graph->AddFilter(device->source, L"Video Capture");		// Add Capture filter to our graph.

		Initialise(&video_control, hWnd, graph, device->source, pin);
		/*
		com_ptr<IEnumMoniker>	encoders;
		GUID			in_types[][2] = {
			{MEDIATYPE_Video, MEDIASUBTYPE_None}
		};
		GUID			out_types[][2] = {
			{MEDIATYPE_Video, MEDIASUBTYPE_None}
		};

		com_ptr<IFilterMapper2>	filter_mapper;
		filter_mapper.create(CLSID_FilterMapper2);
		hr = filter_mapper->EnumMatchingFilters(&encoders, 0, FALSE, 0,
			TRUE, num_elements(in_types), &in_types[0][0], NULL, NULL,	// inputs
			FALSE,	// render
			TRUE, num_elements(out_types), &out_types[0][0], NULL, NULL	// outputs
		);
		*/

		com_ptr<IMediaControl>	media_control;
		hr = graph.query(&media_control);
		hr = media_control->Run();										// Start previewing video data
	}

	ViewDirectShow(MainWindow &_main, const WindowPos &pos, DirectShowGraph *dsg) : main(_main) {
		Create(pos, "Video", CHILD, CLIENTEDGE);
		*&graph = dsg->graph.get();

		reg = AddToROT(graph);

		HRESULT hr;

		com_ptr<IEnumFilters>	filters;
		hr	= graph->EnumFilters(&filters);

		for (com_ptr<IBaseFilter> filter; filters->Next(1, &filter, NULL) == S_OK; filter.clear()) {
			if (com_ptr<IBasicVideo>	bv = filter.query())
				ISO_TRACE("video!n");

			FILTER_INFO	info;
			filter->QueryFilterInfo(&info);
			ISO_TRACEF(info.achName) << '\n';
		}


		hr = query(graph, &video_window);
		hr = video_window->put_Owner((OAHWND)hWnd);
		hr = video_window->put_WindowStyle(CHILD | CLIPCHILDREN);

		hr = graph.query(&media_event);
		hr = media_event->SetNotifyWindow((OAHWND)hWnd, WM_GRAPHNOTIFY, 0);		// Set the window handle used to process graph events

		com_ptr<IMediaControl>	media_control;
		hr = graph.query(&media_control);
		hr = media_control->Run();

	}

	~ViewDirectShow() {
		RemoveFromROT(reg);
	}
};

class EditorDirectShow : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<VideoCaptureDeviceDS>() || type->SameAs<DirectShowGraph>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_VirtualTarget &b) {//, const ISO_ptr<void> &p) {
		if (b.Is<VideoCaptureDeviceDS>()) {
			return *new ViewDirectShow(main, pos, (VideoCaptureDeviceDS*)b);
		} else {
			return *new ViewDirectShow(main, pos, (DirectShowGraph*)b);
		}

	}
} editordirectshow;

struct VideoCapDevice : DeviceT<VideoCapDevice>, DeviceCreateT<VideoCapDevice> {
	void			operator()(const DeviceAdd &add)	{ add("Video Capture, this", LoadPNG("IDB_DEVICE_VIDEOCAP")); }
	ISO_ptr<void>	operator()(const Control &main)		{ return GetVideoCaptureDS("video capture"); }
} videocap_device;

