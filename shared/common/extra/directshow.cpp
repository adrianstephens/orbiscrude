#include "directshow.h"
#include "filename.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Pin
//-----------------------------------------------------------------------------

STDMETHODIMP Filter::FindPin(LPCWSTR Id, IPin **ppPin) {
	for (auto i = pins.begin(), e = pins.end(); i != e; ++i) {
		if (Pin *pin = static_cast<Pin*>(*i)) {
			if (pin->name == Id) {
				*ppPin = pin;
				pin->AddRef();
				return S_OK;
			}
		}
	}
	*ppPin = NULL;
	return VFW_E_NOT_FOUND;
}

HRESULT Pin::AttemptConnection(IPin* pReceivePin, const AM_MEDIA_TYPE *pmt) {
	if (CheckMediaType(pmt)) {
		connected = pReceivePin;
		mt = *pmt;
		HRESULT	hr = pReceivePin->ReceiveConnection(this, pmt);
		if (FAILED(hr))
			connected.clear();
		return hr;
	}
	return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT Pin::AgreeMediaType(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt) {
	if (pmt && !((MediaType*)pmt)->IsPartiallySpecified())
		return AttemptConnection(pReceivePin, pmt);

	HRESULT hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;
	for (int i = 0; i < 2; i++) {
		com_ptr<IEnumMediaTypes>	e;
		HRESULT hr = i == 0 ? EnumMediaTypes(&e) : pReceivePin->EnumMediaTypes(&e);
		if (SUCCEEDED(hr)) {
			for (;;) {
				MediaTypeHolder	mt;
				ULONG			count;
				hr = e->Next(1, &mt, &count);
				if (hr != S_OK)
					break;

				if (mt && (pmt == NULL || mt->MatchesPartial(*pmt))) {
					hr = AttemptConnection(pReceivePin, mt);
					if (hr == S_OK)
						return hr;
					if (hrFailure == VFW_E_NO_ACCEPTABLE_TYPES && FAILED(hr) && hr != E_FAIL && hr != E_INVALIDARG && hr != VFW_E_TYPE_NOT_ACCEPTED)
						hrFailure = hr;
				}
			}
		}
		if (FAILED(hr) && hr != E_FAIL && hr != E_INVALIDARG && hr != VFW_E_TYPE_NOT_ACCEPTED)
			hrFailure = hr;
	}

	return hrFailure;
}

//-----------------------------------------------------------------------------
//	Helpers
//-----------------------------------------------------------------------------

bool iso::PinHasMajorType(IPin *pin, const GUID &majorType) {
	if (com_ptr<IAMStreamConfig> config = query(pin)) {
		int count, size;
		if (SUCCEEDED(config->GetNumberOfCapabilities(&count, &size))) {
			malloc_block	caps(size);
			for (int i = 0; i < count; i++) {
				MediaTypeHolder		mt;
				if (SUCCEEDED(config->GetStreamCaps(i, &mt, caps)) && mt->majortype == majorType)
					return true;
			}
		}
	}

	com_ptr<IEnumMediaTypes> types;
	MediaTypeHolder			mt;
	ULONG					n;
	return SUCCEEDED(pin->EnumMediaTypes(&types)) && types->Next(1, &mt, &n) == S_OK && mt->majortype == majorType;
}

HRESULT	iso::ConfigurePin(IPin *pin, AM_MEDIA_TYPE *_mt, uint32 width, uint32 height, uint64 frame) {
	com_ptr<IAMStreamConfig>	config = query(pin);
	MediaTypeHolder		mt(_mt);
	VIDEOINFOHEADER		*vih	= (VIDEOINFOHEADER*)mt->pbFormat;
	BITMAPINFOHEADER	&bmi	= vih->bmiHeader;
	vih->AvgTimePerFrame		= frame;
	bmi.biWidth					= width;
	bmi.biHeight				= height;
	bmi.biSizeImage				= width * height * (bmi.biBitCount >> 3);
	return config->SetFormat(mt);
}

IPin* iso::GetPin(IBaseFilter *filter, PIN_DIRECTION dir, const GUID *majorType, const GUID *categories, int ncat) {
	com_ptr<IEnumPins>	pins;
	if (SUCCEEDED(filter->EnumPins(&pins))) {
		ULONG	num;
		for (com_ptr<IPin> pin; pins->Next(1, &pin, &num) == S_OK; pin.clear()) {
			if (majorType && !PinHasMajorType(pin, *majorType))
				continue;

			PIN_DIRECTION thisdir;
			if (SUCCEEDED(pin->QueryDirection(&thisdir)) && thisdir == dir) {
				if (ncat == 0)
					return pin;

				com_ptr<IKsPropertySet>	propertySet;
				if (SUCCEEDED(pin->QueryInterface(IID_IKsPropertySet, (void**)&propertySet))) {
					GUID		category;
					DWORD		size;
					PIN_INFO	chi;
					pin->QueryPinInfo(&chi);

					if (chi.pFilter)
						chi.pFilter->Release();

					if (SUCCEEDED(propertySet->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &category, sizeof(GUID), &size))) {
						for (int i = 0; i < ncat; i++) {
							if (category == categories[i])
								return pin;
						}
					}
				}
			}
		}
	}
	return 0;
}


HRESULT iso::LoadGraphFile(IGraphBuilder *graph, const filename &fn) {
	HRESULT					hr;
	com_ptr<IStorage>		storage;
	if (FAILED(hr = StgOpenStorage(str16(fn), 0, STGM_TRANSACTED | STGM_READ | STGM_SHARE_DENY_WRITE, 0, 0, &storage)))
		return hr;

	com_ptr<IStream>		stream;
	if (FAILED(hr = storage->OpenStream(L"ActiveMovieGraph", 0, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream)))
		return hr;

	com_ptr<IPersistStream>	persist = query(graph);
	return persist->Load(stream);
}

HRESULT iso::SaveGraphFile(IGraphBuilder *graph, const filename &fn) {
	HRESULT					hr;
	com_ptr<IStorage>		storage;
	if (FAILED(hr = StgCreateDocfile(str16(fn), STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &storage)))
		return hr;

	com_ptr<IStream>		stream;
	if (FAILED(hr = storage->CreateStream(L"ActiveMovieGraph", STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE, 0, 0, &stream)))
		return hr;

	com_ptr<IPersistStream>	persist = query(graph);
	if (FAILED(hr = persist->Save(stream, TRUE)))
		return hr;

	return storage->Commit(STGC_DEFAULT);
}
