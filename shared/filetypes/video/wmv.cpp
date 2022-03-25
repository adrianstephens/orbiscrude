#include "movie.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "wmsdk.h"
#include "wmsysprf.h"
//#include "wmfsdk11/include/wmsdk.h"
//#include "wmfsdk11/include/wmsysprf.h"
#include "com.h"

using namespace iso;

class WMVFileHandler : public FileHandler {
	const char*		GetExt() override { return "wmv";	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
public:
	WMVFileHandler()		{ ISO::getdef<movie>(); }
} wmv;

HRESULT ShowAllAttributes(IWMHeaderInfo3 *info) {
	WORD	num_attr;
	HRESULT hr	= info->GetAttributeCountEx(0xFFFF, &num_attr);

	for (int i = 0; i < num_attr; i++) {
		// Get the required buffer lengths for the name and value.
		WORD	name_len;
		DWORD	value_len;
		hr = info->GetAttributeByIndexEx(0xFFFF, i, NULL, &name_len, NULL, NULL, NULL, &value_len);

		// Allocate the buffers.
		WCHAR	*name	= new WCHAR[name_len];
		BYTE*	value	= new BYTE[value_len];

		// Get the attribute.
		WORD				lang;
		WMT_ATTR_DATATYPE	type;
		hr = info->GetAttributeByIndexEx(0xFFFF, i, name, &name_len, &type, &lang, value, &value_len);

		// Display the attribute global index and name.
		ISO_TRACEF("%3d - %S (Language %d):\t", i, name, lang);

		// Display the attribute depending upon type.
		switch(type) {
			case WMT_TYPE_WORD:		ISO_TRACEF() << *(uint16*)value << '\n'; break;
			case WMT_TYPE_DWORD:	ISO_TRACEF() << *(uint32*)value << '\n'; break;
			case WMT_TYPE_QWORD:	ISO_TRACEF() << *(uint64*)value << '\n'; break;
			case WMT_TYPE_STRING:	ISO_TRACEF() << (WCHAR*)value << '\n';	break;
			case WMT_TYPE_BINARY:	ISO_TRACEF("<binary value>\n");			break;
			case WMT_TYPE_BOOL:		ISO_TRACEF() << *(bool*)value << '\n';	break;
			case WMT_TYPE_GUID:		ISO_TRACEF() << *(GUID*)value << '\n';	break;
		}

		// Release allocated memory for the next pass.
		delete[] name;
		delete[] value;
	}

	return hr;
}

malloc_block GetAttribute(IWMHeaderInfo3 *info, const char *find, uint16 stream = 0xffff) {
	WORD	num_attr;
	HRESULT hr	= info->GetAttributeCountEx(stream, &num_attr);

	for (int i = 0; i < num_attr; i++) {
		WORD				name_len;
		DWORD				value_len;
		WMT_ATTR_DATATYPE	type;
		hr = info->GetAttributeByIndexEx(stream, i, NULL, &name_len, &type, NULL, NULL, &value_len);

		string16	name(name_len);
		hr = info->GetAttributeByIndexEx(stream, i, name.begin(), &name_len, NULL, NULL, NULL, NULL);
		if (str(find) == name) {
			malloc_block	value(value_len);
			info->GetAttributeByIndexEx(stream, i, NULL, NULL, NULL, NULL, value, &value_len);
			return value;
		}
	}
	return malloc_block();
}

//-----------------------------------------------------------------------------
// IStream wrapper
//-----------------------------------------------------------------------------

class ISO_INSSBuffer : public com<INSSBuffer> {
	malloc_block		buffer;
	uint32				len;
public:
	ISO_INSSBuffer(uint32 maxlen) : buffer(maxlen), len(0) {}

	HRESULT	STDMETHODCALLTYPE	GetBuffer(BYTE** ppdwBuffer) {
		*ppdwBuffer	= (BYTE*)buffer;
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	GetBufferAndLength(BYTE** ppdwBuffer, DWORD* pdwLength) {
		*ppdwBuffer	= (BYTE*)buffer;
		*pdwLength	= len;
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	GetLength(DWORD* pdwLength) {
		*pdwLength	= len;
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	GetMaxLength(DWORD* pdwLength) {
		*pdwLength	= buffer.size32();
		return S_OK;
	}

	HRESULT	STDMETHODCALLTYPE	SetLength(DWORD dwLength) {
		len = dwLength;
		return S_OK;
	}
};

class ISO_IWMWriterSink : public com<IWMWriterSink> {
	iso::ostream_ref	file;

public:
	ISO_IWMWriterSink(ostream_ref _file) : file(_file) {}

	HRESULT	STDMETHODCALLTYPE	AllocateDataUnit(DWORD cbDataUnit, INSSBuffer** ppDataUnit) {
		*ppDataUnit = new ISO_INSSBuffer(cbDataUnit);
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	IsRealTime(BOOL* pfRealTime) {
		*pfRealTime = FALSE;
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	OnDataUnit(INSSBuffer* pDataUnit) {
		BYTE*	buffer;
		DWORD	len;
		HRESULT hr = pDataUnit->GetBufferAndLength(&buffer, &len);
		file.writebuff(buffer, len);
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	OnEndWriting() {
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE	OnHeader(INSSBuffer* pHeader) {
		BYTE*	buffer;
		DWORD	len;
		HRESULT hr = pHeader->GetBufferAndLength(&buffer, &len);
		file.writebuff(buffer, len);
		return S_OK;
	}
};

class WMV_frames : public ISO::VirtualDefaults {
	com_ptr<IWMSyncReader>	reader;
//	sample					sm;
	float					frame_rate;
	DWORD					num_streams;
	int						audio_stream, video_stream;
	WORD					*streams;
	WMT_STREAM_SELECTION	*selections;
	int						prev_frame;
	int						num_frames;
	WMVIDEOINFOHEADER		video_info;
	ISO_ptr<bitmap>			prev_bitmap;

	ISO_ptr<bitmap>	GetFrame(int i);
public:
	int				Count()								{ return num_frames;	}
	ISO::Browser2	Index(int i)						{ return GetFrame(i);	}
	int				Width()								{ return video_info.bmiHeader.biWidth;	}
	int				Height()							{ return video_info.bmiHeader.biHeight;	}
	float			FrameRate()							{ return frame_rate;	}

	WMV_frames(const filename &fn);
	~WMV_frames() {
		delete[] streams;
		delete[] selections;

		if (reader)
			reader->Close();

		CoUninitialize();
	}
};

ISO_DEFVIRT(WMV_frames);

WMV_frames::WMV_frames(const filename &fn) : audio_stream(0), video_stream(0), prev_frame(-1), streams(NULL), selections(NULL) {
	init_com();

	HRESULT			hr;

	hr	= WMCreateSyncReader(NULL, 0, &reader);
	hr	= reader->Open(str16(fn));
	if (hr != S_OK)
		return;

	com_ptr<IWMHeaderInfo3>	info = reader.query();
//	ShowAllAttributes(info);

	com_ptr<IWMProfile>	profile	= reader.query();
	hr	= profile->GetStreamCount(&num_streams);

	streams			= new WORD[num_streams];
	selections		= new WMT_STREAM_SELECTION[num_streams];

	for (int i = 0; i < num_streams; i++) {
		com_ptr<IWMStreamConfig>	stream;
		WORD	stream_number;
		GUID    stream_type;

		hr = profile->GetStream(i, &stream);
		hr = stream->GetStreamNumber(&stream_number);
		hr = stream->GetStreamType(&stream_type);

		streams[i]		= stream_number;
		selections[i]	= WMT_ON;

		if (stream_type == WMMEDIATYPE_Audio)
			audio_stream	= stream_number;
		else if (stream_type == WMMEDIATYPE_Video)
			video_stream	= stream_number;
		else
			selections[i]	= WMT_OFF;
	}

	if (video_stream) {
		IWMOutputMediaProps	*props;
		DWORD				length;
		hr				= reader->GetOutputProps(1, &props);
		hr				= props->GetMediaType(NULL, &length);

		malloc_block	type_buffer(length);
		WM_MEDIA_TYPE	*mediatype	= type_buffer;

		hr				= props->GetMediaType(mediatype, &length);
		video_info		= *(WMVIDEOINFOHEADER*)mediatype->pbFormat;
		frame_rate		= 10000000.0f / video_info.AvgTimePerFrame;

		if (malloc_block dur = GetAttribute(info, "Duration"))
			num_frames	= *(QWORD*)dur / video_info.AvgTimePerFrame;
	}
}

ISO_ptr<bitmap> WMV_frames::GetFrame(int i) {
	ISO_ptr<bitmap>	p;

	if (video_stream == 0)
		return p;

	HRESULT		hr;
	INSSBuffer	*sm;
	QWORD		cnsSampleTime, cnsPrevSampleTime, cnsDuration;
	DWORD		dwFlags, dwOutputNum;
	WORD		wStreamNum;

	if (prev_frame == -1) {
		hr	= reader->SetReadStreamSamples(video_stream, FALSE);
		hr	= reader->SetRange(0, 0);
	} else if (prev_frame == i - 1) {
		return prev_bitmap;
	}

	if (prev_frame != i - 1) {
//		hr = reader->SetRangeByFrame(video_stream, i + 1, 0);
		hr = reader->SetRange(i * video_info.AvgTimePerFrame, 0);
		if (FAILED(hr))
			return p;
	}

	hr = reader->GetNextSample(video_stream, &sm, &cnsSampleTime, &cnsDuration, &dwFlags, &dwOutputNum, &wStreamNum);

	if (FAILED(hr))
		return p;

	BYTE	*srce;
	DWORD	length;
	hr = sm->GetBufferAndLength(&srce, &length);

	int		width	= Width(), height = Height(), bitcount = video_info.bmiHeader.biBitCount;
	p.Create()->Create(width, height);

	int		scan	= (width * bitcount + 31) / 32 * 4;
	for (int y = height - 1; y >= 0; y--, srce += scan)
		copy_n((Texel<B8G8R8>*)srce, p->ScanLine(y), width);

	sm->Release();

	prev_frame	= i;
	prev_bitmap	= p;
	return p;
}

ISO_ptr<void> WMVFileHandler::ReadWithFilename(tag id, const filename &fn) {
	ISO_ptr<movie>	movie;
	ISO_ptr<WMV_frames>	frames(NULL, fn);
	if (frames->Count()) {
		movie.Create(id);
		movie->frames	= frames;
		movie->fps		= frames->FrameRate();
		movie->width	= frames->Width();
		movie->height	= frames->Height();
	}
	return movie;
}

class WMV_writer {
	IWMWriter				*writer;
	float					frame_rate;
public:
	WMV_writer() : writer(NULL) {}
	~WMV_writer() {
		if (writer) {
			writer->EndWriting();
			writer->Release();
		}
		CoUninitialize();
	}

	bool	Init(const filename &fn);
	void	SetFrameRate(float f)	{ frame_rate = f; }
	bool	PutFrame(bitmap &bm, int i);
};

bool WMV_writer::Init(const filename &fn) {
	init_com();

	HRESULT			hr;
	QWORD			time			= 0;
//	int				bytes_per_sec	= sm.SampleRate() * sm.BytesPerSample();

	if (FAILED(WMCreateWriter(NULL, &writer))
	||  FAILED(writer->SetProfileByID(WMProfile_V80_700NTSCVideo)))
		return false;

//	IWMWriterAdvanced	*writeradvanced;
//	hr	= writer->QueryInterface(IID_IWMWriterAdvanced, (void**)&writeradvanced);
//	hr	= writeradvanced->AddSink(new ISO_IWMWriterSink(file));

	WCHAR			wfilename[_MAX_PATH];
	MultiByteToWideChar(CP_ACP, 0, fn, -1, wfilename, _MAX_PATH);
	hr	= writer->SetOutputFilename(wfilename);

	return SUCCEEDED(writer->BeginWriting());
}

bool WMV_writer::PutFrame(bitmap &bm, int i) {
	HRESULT	hr		= S_OK;
	QWORD	time	= 1.0e7 * i / frame_rate;
/*
	if (sm.Exists()) {
		frame_t	frame	= sm.TimeToFrame(i / frame_rate);
		frame_t	frames	= sm.SampleRate() / frame_rate;

		bytes	= min(frames, sm.Frames() - frame) * sm.BytesPerSample();
		hr		= writer->AllocateSample(bytes, &sm);
		hr		= sm->GetBuffer(&buffer);

		memcpy(buffer, (char*)sm.Samples() + frame * sm.BytesPerSample(), bytes);
		hr = writer->WriteSample(0, time, 0, sm);
		hr = sm->Release();
	}
*/
	if (true) {
		com_ptr<INSSBuffer>		sm;
		uint32	buff_size	= 44100 * 2 * 2 / frame_rate;
		BYTE	*buffer;
		hr = writer->AllocateSample(buff_size, &sm);
		hr = sm->GetBuffer(&buffer);
		memset(buffer, 0, buff_size);
		hr = writer->WriteSample(0, time, 0, sm);
	}

	if (SUCCEEDED(hr)) {
		com_ptr<INSSBuffer>		sm;
		int		width		= bm.Width();
		int		height		= bm.Height();
		uint32	buff_size	= width * height * sizeof(Texel<B8G8R8>);
		BYTE	*buffer;
		hr = writer->AllocateSample(buff_size, &sm);
		hr = sm->GetBuffer(&buffer);

		for (int y = 0; y < height; y++)
			copy_n(bm.ScanLine(height - 1 - y), (Texel<B8G8R8>*)buffer + y * width, width);

		hr = writer->WriteSample(1, time, 0, sm);
	}
	return SUCCEEDED(hr);
}

bool WMVFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
	if (ISO_ptr<movie> mv = ISO_conversion::convert<movie>(p)) {
		WMV_writer	wmv;
		if (wmv.Init(fn)) {
			wmv.SetFrameRate(mv->fps);
			ISO::Browser	b(mv->frames);
			for (int i = 0, n = b.Count(); i < n; i++)
				wmv.PutFrame(*(bitmap*)b[i], i);
			return true;
		}
	}
	return false;
}
