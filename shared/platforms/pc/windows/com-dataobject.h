#ifndef COM_DATAOBJECT_H
#define COM_DATAOBJECT_H

#include "com.h"
#include "window.h"

#include <shlobj.h>

namespace iso {

class DataObject : public com_list<IDataObject, IDropSource> {
protected:
	CLIPFORMAT	cf;
	DWORD		tymed;
	void		*data;
public:
//IDataObject
	STDMETHODIMP	GetData(FORMATETC *fmt, STGMEDIUM *med) {
		if (fmt->cfFormat != cf || !(fmt->tymed & tymed))
			return DV_E_FORMATETC;
		med->tymed			= tymed;
		med->pUnkForRelease	= 0;
		med->hGlobal		= tymed == TYMED_HGLOBAL ? win::Global(data).Dup() : data;
		return S_OK;
	}
	STDMETHODIMP	QueryGetData(FORMATETC *fmt) {
		return fmt->cfFormat == cf && (fmt->tymed & tymed) ? S_OK : DV_E_FORMATETC;
	}
	STDMETHODIMP	GetDataHere(FORMATETC *fmt, STGMEDIUM *med)							{ return DATA_E_FORMATETC;}
	STDMETHODIMP	GetCanonicalFormatEtc(FORMATETC *in, FORMATETC *out)				{ out->ptd = NULL; return E_NOTIMPL; }
	STDMETHODIMP	SetData(FORMATETC *fmt, STGMEDIUM *med, BOOL release)				{ return E_NOTIMPL; }
	STDMETHODIMP	EnumFormatEtc(DWORD dir, IEnumFORMATETC **enum_fmt) {
		if (dir == DATADIR_GET) {
			FORMATETC	format = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
			return SHCreateStdEnumFmtEtc(1, &format, enum_fmt);
		}
		return E_NOTIMPL;
	}

	STDMETHODIMP	DAdvise(FORMATETC *fmt, DWORD adv, IAdviseSink *sink,  DWORD *conn)	{ return OLE_E_ADVISENOTSUPPORTED; }
	STDMETHODIMP	DUnadvise(DWORD conn)												{ return OLE_E_ADVISENOTSUPPORTED; }
	STDMETHODIMP	EnumDAdvise(IEnumSTATDATA **enum_advise)							{ return OLE_E_ADVISENOTSUPPORTED; }
//IDropSource
	STDMETHODIMP	QueryContinueDrag(BOOL escape, DWORD key_state) {
		return	escape && Cancel()									? DRAGDROP_S_CANCEL
			:	!(key_state & (MK_LBUTTON | MK_RBUTTON)) && Drop()	? DRAGDROP_S_DROP
	//		:	!(key_state & (MK_LBUTTON | MK_RBUTTON))			? DRAGDROP_S_DROP
			:	S_OK;
	}
	STDMETHODIMP	GiveFeedback(DWORD effect)											{ return DRAGDROP_S_USEDEFAULTCURSORS; }

//MyDataObject
	virtual	bool	Cancel()	{ return true; }
	virtual	bool	Drop()		{ return true; }
	virtual ~DataObject()		{}
	DataObject(CLIPFORMAT _cf, DWORD _tymed, void *_data) : cf(_cf), tymed(_tymed), data(_data) {}
};

}

#endif // COM_DATAOBJECT_H

