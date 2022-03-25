#ifndef DIRECTSHOW_H
#define DIRECTSHOW_H

#include "base/array.h"
#include "com.h"
#include <dshow.h>

namespace iso {

//-----------------------------------------------------------------------------
//	MediaType
//-----------------------------------------------------------------------------

struct MediaType : public AM_MEDIA_TYPE {
	void init() {
		clear(*this);
		lSampleSize			= 1;
		bFixedSizeSamples	= true;
	}

	MediaType()	{
		init();
	}
	MediaType(const GUID &_majortype) {
		init();
		majortype = _majortype;
	}
	MediaType(const AM_MEDIA_TYPE &mt) {
		*(AM_MEDIA_TYPE*)this = mt;
		if (cbFormat)
			memcpy(pbFormat = (PBYTE)CoTaskMemAlloc(cbFormat), mt.pbFormat, cbFormat);
		if (pUnk)
			pUnk->AddRef();
	}
	~MediaType() {
		if (cbFormat)
			CoTaskMemFree(pbFormat);
		if (pUnk)
			pUnk->Release();
	}

	MediaType&	operator=(const MediaType &mt) {
		if (&mt != this) {
			if (cbFormat)
				CoTaskMemFree(pbFormat);
			*(AM_MEDIA_TYPE*)this = mt;
			if (cbFormat)
				memcpy(pbFormat = (PBYTE)CoTaskMemAlloc(cbFormat), mt.pbFormat, cbFormat);
			if (pUnk)
				pUnk->AddRef();
		}
		return *this;
	}
	MediaType&	operator=(const AM_MEDIA_TYPE &mt) {
		return operator=(static_cast<const MediaType&>(mt));
	}
	bool		operator==(const AM_MEDIA_TYPE &mt) const {
		return	majortype	== mt.majortype
			&&	subtype		== mt.subtype
			&&	formattype	== mt.formattype
			&&	cbFormat	== mt.cbFormat
			&&	(cbFormat == 0 || pbFormat != NULL && mt.pbFormat != NULL && memcmp(pbFormat, mt.pbFormat, cbFormat) == 0);
	}
	bool		operator!=(const AM_MEDIA_TYPE &mt) const {
		return !(*this == mt);
	}
	bool		IsValid() const {
		return majortype != GUID_NULL;
	}
	bool		IsPartiallySpecified(void) const {
		return majortype == GUID_NULL || formattype == GUID_NULL;
	}
	bool		MatchesPartial(const AM_MEDIA_TYPE &mt) const {
		return
			(mt.majortype	== GUID_NULL || majortype	== mt.majortype)
		&&	(mt.subtype		== GUID_NULL || subtype		== mt.subtype)
		&&	(mt.formattype	== GUID_NULL || (formattype	== mt.formattype
			&& cbFormat == mt.cbFormat && (!cbFormat || memcmp(pbFormat, mt.pbFormat, cbFormat) == 0)
			)
		);
	}

	bool		IsFixedSize()			const	{ return bFixedSizeSamples; };
	bool		IsTemporalCompressed()	const	{ return bTemporalCompression; };
	ULONG		GetSampleSize()			const	{ return bFixedSizeSamples ? lSampleSize : 0; }
	memory_block GetFormat()			const	{ return memory_block(pbFormat, cbFormat); };

	void		SetSampleSize(ULONG sz)	{
		bFixedSizeSamples = sz != 0;
		if (sz)
			lSampleSize = sz;
	}
	void		SetTemporalCompression(bool compressed) {
		bTemporalCompression = compressed;
	}

	void		SetFormat(const memory_block &mb) {
		uint32 length = mb.size32();
		if (cbFormat != length) {
			if (cbFormat)
				CoTaskMemFree((PVOID)pbFormat);
			cbFormat = length;
			pbFormat = (BYTE*)CoTaskMemAlloc(length);
		}
		memcpy(pbFormat, mb, length);
	}
	void		SetFormatBuffer(uint32 length) {
		if (cbFormat != length) {
			if (cbFormat)
				CoTaskMemFree((PVOID)pbFormat);
			cbFormat = length;
			pbFormat = (BYTE*)CoTaskMemAlloc(length);
		}
		memset(pbFormat, 0, length);
	}
	void		ResetFormatBuffer() {
		if (cbFormat)
			CoTaskMemFree((PVOID)pbFormat);
		cbFormat = 0;
		pbFormat = 0;
	}
};

struct MediaTypeHolder {
	MediaType		*mt;
	MediaTypeHolder()	: mt(0)				{}
	MediaTypeHolder(AM_MEDIA_TYPE *_mt)		: mt(com_allocator::make<MediaType>(*_mt))		{}
	MediaTypeHolder(const GUID &_majortype)	: mt(com_allocator::make<MediaType>(_majortype))	{}
	~MediaTypeHolder()						{ if (mt) { mt->~MediaType(); CoTaskMemFree(mt); } }
	AM_MEDIA_TYPE**	operator&()				{ return (AM_MEDIA_TYPE**)&mt; }
	MediaType*		operator->()	const	{ return mt; }
	operator MediaType*()			const	{ return mt; }
};

//-----------------------------------------------------------------------------
//	ReferenceTime
//-----------------------------------------------------------------------------

class ReferenceTime {
	enum {SECOND = 10000000};
public:
	REFERENCE_TIME	v;

	ReferenceTime()						: v(0)			{}
	ReferenceTime(REFERENCE_TIME rt)	: v(rt)			{}
	ReferenceTime(double t)				: v(t * SECOND)	{}
	operator REFERENCE_TIME()	const	{ return v; };
	operator double()			const	{ return double(v) / SECOND; }

	ReferenceTime &operator+=(const ReferenceTime &rt) {
		v += rt.v; return *this;
	}
	ReferenceTime& operator-=(const ReferenceTime& rt) {
		v -= rt.v; return *this;
	}
	int32 Millisecs() {
		return int32(v / (SECOND / 1000000));
	}
};

//-----------------------------------------------------------------------------
//	com_enumerator
//-----------------------------------------------------------------------------

inline AM_MEDIA_TYPE *com_dup(AM_MEDIA_TYPE *s) {
	return com_allocator::make<MediaType>(*s);
}
template<typename S> inline S *com_dup(S *s) {
	if (s)
		s->AddRef();
	return s;
}
template<typename S> inline S *com_dup(_com_ptr<S> &s) {
	if (s)
		s->AddRef();
	return s;
}

template<typename T, typename B, typename I = T**> class com_enumerator : public com<B> {
	range<I>	r;
	I			i;
public:
	com_enumerator(const range<I> &_r)			: r(_r), i(r.begin()) {}
	com_enumerator(const range<I> &_r, I _i)	: r(_r), i(_i) {}

	STDMETHODIMP Next(ULONG n, T **pp, ULONG *fetched) {
		T	**pp0	= pp;
		for (I e = min(i + n, r.end()); i != e; ++i)
			*pp++ = com_dup(*i);

		if (fetched)
			*fetched = pp - pp0;

		return n == pp - pp0 ? S_OK : S_FALSE;
	}
	STDMETHODIMP Skip(ULONG n)	{ if (i + n > r.end()) return S_FALSE; i += n; return S_OK; }
	STDMETHODIMP Reset()		{ i = r.begin(); return S_OK; }
	STDMETHODIMP Clone(B **pp)	{ *pp = new com_enumerator(r, i); return S_OK; }
};

//-----------------------------------------------------------------------------
//	Filter
//-----------------------------------------------------------------------------

class Filter : public com_inherit<type_list<IPersist, IMediaFilter>, com_list<IBaseFilter, IAMovieSetup> > {
friend class Pin;
protected:
	string						name;
	FILTER_STATE				state;
	com_ptr2<IReferenceClock>	clock;
	IFilterGraph*				graph;
	IMediaEventSink*			sink;
	dynamic_array<IPin*>		pins;

	typedef com_enumerator<IPin, IEnumPins> PinEnumerator;

public:
	Filter() : state(State_Stopped), graph(0), sink(0) {}
//IPersist
//	STDMETHODIMP GetClassID(CLSID *pClassID);

//IMediaFilter
	STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE *State) {
		*State = state;
		return S_OK;
	}
	STDMETHODIMP SetSyncSource(IReferenceClock *pClock) {
		clock = pClock;
		return S_OK;
	}
	STDMETHODIMP GetSyncSource(IReferenceClock **pClock) {
		*pClock = com_dup(clock);
		return S_OK;
	}
	STDMETHODIMP Stop() {
		state = State_Stopped;
		return S_OK;
	}
	STDMETHODIMP Pause() {
		state = State_Paused;
		return S_OK;
	}
	STDMETHODIMP Run(REFERENCE_TIME tStart) {
		state = State_Running;
		return S_OK;
	}

//IBaseFilter
	STDMETHODIMP EnumPins(IEnumPins **ppEnum) {
		*ppEnum = new PinEnumerator(make_range(pins.begin(), pins.end()));
		return S_OK;
	}
	STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin);

	STDMETHODIMP QueryFilterInfo(FILTER_INFO *pInfo) {
		fstr(pInfo->achName) = name;
		pInfo->pGraph = com_dup(graph);
		return S_OK;
	}

	STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph,LPCWSTR pName) {
		graph	= pGraph;
		sink	= 0;
		if (graph && SUCCEEDED(graph->QueryInterface(IID_IMediaEventSink, (void**)&sink)))
			sink->Release();
		name = pName;
		return S_OK;
	}

	STDMETHODIMP QueryVendorInfo(LPWSTR *pVendorInfo) {
		return E_NOTIMPL;
	}

// IAMovieSetup
	STDMETHODIMP Register()		{ return S_FALSE; }
	STDMETHODIMP Unregister()	{ return S_FALSE; }
};

//-----------------------------------------------------------------------------
//	Pin
//-----------------------------------------------------------------------------

class Pin : public com2<IQualityControl, com<IPin> > {
	friend class Filter;
protected:
	string			name;
	Filter			*filter;
	MediaType		mt;
	PIN_DIRECTION	dir;
	com_ptr2<IPin>	connected;
	REFERENCE_TIME	seg_start, seg_stop;
	double			seg_rate;

	typedef com_enumerator<AM_MEDIA_TYPE, IEnumMediaTypes> MediaTypeEnumerator;

	HRESULT		AgreeMediaType(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt);
	HRESULT		AttemptConnection(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt);

	bool		IsStopped() const {
		return filter->state == State_Stopped;
	}
	virtual bool CheckMediaType(const AM_MEDIA_TYPE *pmt)=0;

public:
// IPin
	STDMETHODIMP Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt) {
		if (connected)
			return VFW_E_ALREADY_CONNECTED;
		PIN_DIRECTION pd;
		pReceivePin->QueryDirection(&pd);
		if (pd == dir)
			return VFW_E_INVALID_DIRECTION;
		return AgreeMediaType(pReceivePin, pmt);
	}
	STDMETHODIMP ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt) {
		if (connected)
			return VFW_E_ALREADY_CONNECTED;

		PIN_DIRECTION pd;
		pConnector->QueryDirection(&pd);
		if (pd == dir)
			return VFW_E_INVALID_DIRECTION;

		if (CheckMediaType(pmt)) {
			connected	= pConnector;
			mt			= *pmt;
			return S_OK;
		}
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	STDMETHODIMP Disconnect() {
		if (!IsStopped())
			return VFW_E_NOT_STOPPED;
		if (connected) {
			connected.clear();
			return S_OK;
		}
		return S_FALSE;
	}
	STDMETHODIMP ConnectedTo(IPin **ppPin) {
		if (*ppPin = com_dup(connected))
			return S_OK;
		return VFW_E_NOT_CONNECTED;
	}
	STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt) {
		if (connected) {
			new(pmt) MediaType(mt);
			return S_OK;
		} else {
			new(pmt) MediaType;
			return VFW_E_NOT_CONNECTED;
		}
	}
	STDMETHODIMP QueryPinInfo(PIN_INFO *pInfo) {
		pInfo->pFilter = com_dup(filter);
		fstr(pInfo->achName) = name;
		pInfo->dir = dir;
		return S_OK;
	}
	STDMETHODIMP QueryDirection(PIN_DIRECTION *pPinDir) {
		*pPinDir = dir;
		return S_OK;
	}
	STDMETHODIMP QueryId(LPWSTR *Id) {
		*Id = ole_string(name);
		return S_OK;
	}
	STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt) {
		return CheckMediaType(pmt) ? S_OK : S_FALSE;
	}
	STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum) {
		*ppEnum = new MediaTypeEnumerator(range<AM_MEDIA_TYPE**>());
		return S_OK;
	}
	STDMETHODIMP QueryInternalConnections(IPin **apPin, ULONG *nPin) {
		return E_NOTIMPL;
	}
	STDMETHODIMP EndOfStream() {
		return S_OK;
	}
	STDMETHODIMP BeginFlush() {
		return S_OK;
	}
	STDMETHODIMP EndFlush() {
		return S_OK;
	}
	STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) {
		seg_start	= tStart;
		seg_stop	= tStop;
		seg_rate	= dRate;
		return S_OK;
	}
//IQualityControl
	STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q) {
		return E_NOTIMPL;
	}
	STDMETHODIMP SetSink(IQualityControl *piqc) {
		return S_OK;
	}

	void SetFormat(AM_MEDIA_TYPE *pmt) {
		mt = *pmt;
	}
};

//-----------------------------------------------------------------------------
//	InputPin
//-----------------------------------------------------------------------------

class InputPin : public com2<IMemInputPin, Pin> {
	com_ptr2<IMemAllocator>	allocator;
	bool	read_only;
public:
	InputPin(Filter *_filter) { filter = _filter; dir = PINDIR_INPUT; }
//IMemInputPin
	STDMETHODIMP GetAllocator(IMemAllocator **ppAllocator) {
		if (!allocator)
			allocator.create(CLSID_MemoryAllocator);
		*ppAllocator = com_dup(allocator);
		return S_OK;
	}
	STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly) {
		allocator = pAllocator;
		read_only = !!bReadOnly;
		return S_OK;
	}
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps) {
		return E_NOTIMPL;
	}
	//STDMETHODIMP Receive(IMediaSample *pSample);
	STDMETHODIMP ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed) {
		HRESULT hr = S_OK;
		*nSamplesProcessed = 0;
		while (nSamples-- > 0 && (hr = Receive(pSamples[*nSamplesProcessed])) == S_OK)
			(*nSamplesProcessed)++;
		return hr;
	}
	STDMETHODIMP ReceiveCanBlock() {
		int		num_outpins = 0;
		com_iterator<IEnumPins, IPin>	e;
		filter->EnumPins(&e);
		while (IPin *pin = ++e) {
			PIN_DIRECTION	pd;
			HRESULT			hr = pin->QueryDirection(&pd);
			if (FAILED(hr))
				return hr;

			if (pd == PINDIR_OUTPUT) {
				com_ptr<IPin> connected;
				if (SUCCEEDED(pin->ConnectedTo(&connected))) {
					num_outpins++;
					if (com_ptr<IMemInputPin> input = connected.query()) {
						if (input->ReceiveCanBlock() != S_FALSE)
							return S_OK;
					} else {
						return S_OK;
					}
				}
			}
		}
		return num_outpins == 0 ? S_OK : S_FALSE;
	}
};

//-----------------------------------------------------------------------------
//	SeekPosPassThru
//-----------------------------------------------------------------------------

class SeekPosPassThru : public com2<IMediaSeeking, TDispatch<IMediaPosition> > {
	com_ptr<IPin>	pin;

	template<typename T> bool	GetPeer(T **p) {
		*p	= NULL;
		com_ptr<IPin>	connected;
		return SUCCEEDED(pin->ConnectedTo(&connected)) && SUCCEEDED(connected.query(p));
	}

public:
	SeekPosPassThru(IPin *_pin) : pin(_pin) {}
//IMediaSeeking
	STDMETHODIMP GetCapabilities(DWORD *pCapabilities) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetCapabilities(pCapabilities) : E_NOTIMPL;
	}
	STDMETHODIMP CheckCapabilities(DWORD *pCapabilities) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->CheckCapabilities(pCapabilities) : E_NOTIMPL;
	}
	STDMETHODIMP SetTimeFormat(const GUID *pFormat) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->SetTimeFormat(pFormat) : E_NOTIMPL;
	}
	STDMETHODIMP GetTimeFormat(GUID *pFormat) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetTimeFormat(pFormat) : E_NOTIMPL;
	}
	STDMETHODIMP IsUsingTimeFormat(const GUID *pFormat) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->IsUsingTimeFormat(pFormat) : E_NOTIMPL;
	}
	STDMETHODIMP IsFormatSupported(const GUID *pFormat) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->IsFormatSupported(pFormat) : E_NOTIMPL;
	}
	STDMETHODIMP QueryPreferredFormat(GUID *pFormat) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->QueryPreferredFormat(pFormat) : E_NOTIMPL;
	}
	STDMETHODIMP ConvertTimeFormat(LONGLONG *pTarget, const GUID *pTargetFormat,LONGLONG Source, const GUID *pSourceFormat) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->ConvertTimeFormat(pTarget, pTargetFormat,Source, pSourceFormat) : E_NOTIMPL;
	}
	STDMETHODIMP SetPositions(LONGLONG *pCurrent, DWORD CurrentFlags, LONGLONG *pStop, DWORD StopFlags) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->SetPositions(pCurrent, CurrentFlags, pStop, StopFlags) : E_NOTIMPL;
	}

	STDMETHODIMP GetPositions(LONGLONG *pCurrent, LONGLONG *pStop) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetPositions(pCurrent, pStop) : E_NOTIMPL;
	}
	STDMETHODIMP GetCurrentPosition(LONGLONG *pCurrent) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetCurrentPosition(pCurrent) : E_NOTIMPL;
	}
	STDMETHODIMP GetStopPosition(LONGLONG *pStop) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetStopPosition(pStop) : E_NOTIMPL;
	}
	STDMETHODIMP SetRate(double dRate) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->SetRate(dRate) : E_NOTIMPL;
	}
	STDMETHODIMP GetRate(double *pdRate) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetRate(pdRate) : E_NOTIMPL;
	}
	STDMETHODIMP GetDuration(LONGLONG *pDuration) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetDuration(pDuration) : E_NOTIMPL;
	}
	STDMETHODIMP GetAvailable(LONGLONG *pEarliest, LONGLONG *pLatest) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetAvailable(pEarliest, pLatest) : E_NOTIMPL;
	}
	STDMETHODIMP GetPreroll(LONGLONG *pllPreroll) {
		com_ptr<IMediaSeeking>	ms;
		return GetPeer(&ms) ? ms->GetPreroll(pllPreroll) : E_NOTIMPL;
	}

//IMediaPosition
	STDMETHODIMP get_Duration(REFTIME *plength) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->get_Duration(plength) : E_NOTIMPL;
	}
	STDMETHODIMP put_CurrentPosition(REFTIME llTime) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->put_CurrentPosition(llTime) : E_NOTIMPL;
	}
	STDMETHODIMP get_StopTime(REFTIME *pllTime) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->get_StopTime(pllTime) : E_NOTIMPL;
	}
	STDMETHODIMP put_StopTime(REFTIME llTime) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->put_StopTime(llTime) : E_NOTIMPL;
	}
	STDMETHODIMP get_PrerollTime(REFTIME *pllTime) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->get_PrerollTime(pllTime) : E_NOTIMPL;
	}
	STDMETHODIMP put_PrerollTime(REFTIME llTime) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->put_PrerollTime(llTime) : E_NOTIMPL;
	}
	STDMETHODIMP get_Rate(double *pdRate) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->get_Rate(pdRate) : E_NOTIMPL;
	}
	STDMETHODIMP put_Rate(double dRate) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->put_Rate(dRate) : E_NOTIMPL;
	}
	STDMETHODIMP get_CurrentPosition(REFTIME *pllTime) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->get_CurrentPosition(pllTime) : E_NOTIMPL;
	}
	STDMETHODIMP CanSeekForward(LONG *pCanSeekForward) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->CanSeekForward(pCanSeekForward) : E_NOTIMPL;
	}
	STDMETHODIMP CanSeekBackward(LONG *pCanSeekBackward) {
		com_ptr<IMediaPosition>	ms;
		return GetPeer(&ms) ? ms->CanSeekBackward(pCanSeekBackward) : E_NOTIMPL;
	}
};

//-----------------------------------------------------------------------------
//	Helpers
//-----------------------------------------------------------------------------

class filename;

bool	PinHasMajorType(IPin *pin, const GUID &majorType);
HRESULT	ConfigurePin(IPin *pin, AM_MEDIA_TYPE *_mt, uint32 width, uint32 height, uint64 frame);
IPin*	GetPin(IBaseFilter *filter, PIN_DIRECTION dir, const GUID *majorType, const GUID *categories, int ncat);

HRESULT LoadGraphFile(IGraphBuilder *graph, const filename &fn);
HRESULT SaveGraphFile(IGraphBuilder *graph, const filename &fn);

} // namespace iso

#endif // DIRECTSHOW_H
