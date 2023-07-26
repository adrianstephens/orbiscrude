#include "windows/splitter.h"
#include "windows/d2d.h"
#include "main.h"
#include <ExDisp.h>
#include <ExDispId.h>
#include <MsHTML.h>
#include <MsHTMDid.h>

using namespace iso;
using namespace win;

inline HRESULT Advise(IUnknown *connection_point, IUnknown *p, const IID& iid, DWORD &cookie) {
	if (!connection_point)
		return E_INVALIDARG;

	com_ptr<IConnectionPointContainer>	cont;
	com_ptr<IConnectionPoint>			cp;

	HRESULT h = connection_point->QueryInterface(__uuidof(IConnectionPointContainer), (void**)&cont);
	if (SUCCEEDED(h))
		h = cont->FindConnectionPoint(iid, &cp);
	if (SUCCEEDED(h))
		h = cp->Advise(p, &cookie);
	return h;
}

inline HRESULT Unadvise(IUnknown *connection_point, const IID& iid, DWORD cookie) {
	if (!connection_point)
		return E_INVALIDARG;

	com_ptr<IConnectionPointContainer>	cont;
	com_ptr<IConnectionPoint>			cp;

	HRESULT h = connection_point->QueryInterface(__uuidof(IConnectionPointContainer), (void**)&cont);
	if (SUCCEEDED(h))
		h = cont->FindConnectionPoint(iid, &cp);
	if (SUCCEEDED(h))
		h = cp->Unadvise(cookie);
	return h;
}

class ViewBrowser : public Window<ViewBrowser>, public com_list<
	IOleClientSite,
	IOleInPlaceSite,
	IOleControlSite,
	IDispatch
> {
	com_ptr<IWebBrowser2>	wb;
	DWORD					cookie;
	dynamic_array<DWORD>	anchors;

	//IOleClientSite
	HRESULT STDMETHODCALLTYPE SaveObject()											{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMoniker(DWORD assign, DWORD which, IMoniker **pp)	{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer **cont)					{ *cont = NULL; return E_NOINTERFACE; }
	HRESULT STDMETHODCALLTYPE ShowObject()											{ return S_OK; }
	HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL show)								{ return S_OK; }
	HRESULT STDMETHODCALLTYPE RequestNewObjectLayout()								{ return E_NOTIMPL; }

	//IOleInPlaceSite : public IOleWindow
	HRESULT STDMETHODCALLTYPE GetWindow(HWND *p)									{ *p = hWnd; return S_OK; }
	HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL mode)						{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE CanInPlaceActivate()									{ return S_OK; }
	HRESULT STDMETHODCALLTYPE OnInPlaceActivate()									{ return S_OK; }
	HRESULT STDMETHODCALLTYPE OnUIActivate()										{ return S_OK; }
	HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame **frame, IOleInPlaceUIWindow **ui, RECT *pos, RECT *clip, OLEINPLACEFRAMEINFO *info) {
		*frame	= 0;
		*ui		= 0;
		*pos	= *clip = GetClientRect();
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtant)								{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL undoable)							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate()									{ return S_OK; }
	HRESULT STDMETHODCALLTYPE DiscardUndoState()									{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE DeactivateAndUndo()									{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE OnPosRectChange(const RECT *rect)						{ return S_OK; }

	//IOleControlSite
	HRESULT STDMETHODCALLTYPE OnControlInfoChanged()								{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE LockInPlaceActive(BOOL lock)							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetExtendedControl(IDispatch **disp)					{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE TransformCoords(POINTL *hi, POINTF *cont, DWORD flags){ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG *msg, DWORD modifiers)		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE OnFocus(BOOL focus)									{ return E_NOTIMPL; }//site ? site->OnFocusChangeIS(static_cast<IInputObject*>(this), focus) : S_OK; }
	HRESULT STDMETHODCALLTYPE ShowPropertyFrame()									{ return E_NOTIMPL; }

	//IDispatch
	HRESULT	STDMETHODCALLTYPE GetTypeInfoCount(UINT *n)								{ return E_NOTIMPL; }
	HRESULT	STDMETHODCALLTYPE GetTypeInfo(UINT i, LCID loc, ITypeInfo **info)		{ return E_NOTIMPL; }
	HRESULT	STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, BSTR *names, UINT num, LCID loc, DISPID *dispid) {return E_NOTIMPL; }
	HRESULT	STDMETHODCALLTYPE Invoke(DISPID dispid, REFIID riid, LCID loc, WORD flags, DISPPARAMS* params, VARIANT* result, EXCEPINFO* excepinfo, UINT* err) {
		static bool anchor = false;
		if (IID_NULL != riid)
			return DISP_E_UNKNOWNINTERFACE;
		if (!params)
			return DISP_E_PARAMNOTOPTIONAL;

		com_variant	*args = (com_variant*)params->rgvarg;

		switch (dispid) {
			case DISPID_BEFORENAVIGATE2:
				// The parameters for this DISPID are as follows:
				// [0]: Cancel flag  - VT_BYREF|VT_BOOL
				// [1]: HTTP headers - VT_BYREF|VT_VARIANT
				// [2]: Address of HTTP POST data  - VT_BYREF|VT_VARIANT
				// [3]: Target frame name - VT_BYREF|VT_VARIANT
				// [4]: Option flags - VT_BYREF|VT_VARIANT
				// [5]: URL to navigate to - VT_BYREF|VT_VARIANT
				// [6]: An object that evaluates to the top-level or frame WebBrowser object corresponding to the event.
				if (!anchor) {
					ManageAnchorsEventSink(false);
				} else if (params->cArgs >= 5 && !com_string(*args[5]).find("#_mysearch")) {
					wb->Navigate2(args[5], args[4], args[3], args[2], args[1]);
					*args[0]	= true;
					anchor		= false;
				}
				break;

			case DISPID_NAVIGATECOMPLETE2:
				ManageAnchorsEventSink(true);
				break;

			case DISPID_HTMLELEMENTEVENTS_ONCLICK:
				anchor = true;
				break;

			default:
				return DISP_E_MEMBERNOTFOUND;
		}
		return S_OK;
	}
	void	ManageAnchorsEventSink(bool advise);

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	ViewBrowser(const WindowPos &pos, const char *url);
};

LRESULT ViewBrowser::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE:
			if (wb) {
				if (com_ptr<IOleInPlaceObject> obj = wb.query()) {
					Rect	client(Point(0, 0), Point(lParam));
					return obj->SetObjectRects(&client, &client);
				}
			}
			break;
	}
	return Super(message, wParam, lParam);
}

void ViewBrowser::ManageAnchorsEventSink(bool advise) {
	if (!advise && anchors.empty())
		return;

	com_ptr<IDispatch>				disp;
	com_ptr<IHTMLDocument2>			doc;
	com_ptr<IHTMLElementCollection> elements;

	if (SUCCEEDED(wb->get_Document(&disp))
	&&	disp
	&&	SUCCEEDED(disp.query(&doc))
	&&	SUCCEEDED(doc->get_all(&elements))
	) {
		// Advise all the anchors on the page so we can get the onclick events
		// For the search pages, the anchors collection is empty, so we have to iterate through the entire collection and advise each anchor tag
		long num_elements = 0;
		elements->get_length(&num_elements);
		for (long i = 0; i < num_elements; i++) {
			com_ptr<IDispatch>					disp;
			com_ptr<IHTMLElement>				element;
			com_ptr<IConnectionPointContainer>	cont;
			com_ptr<IConnectionPoint>			cp;
			com_string							name;
			if (SUCCEEDED(elements->item(com_variant(i), com_variant(), &disp))
			&&	SUCCEEDED((disp.query(&element)))
			&&	SUCCEEDED(element->get_tagName(&name))	&& name == "A"
			&&	SUCCEEDED(element.query(&cont))			&& SUCCEEDED(cont->FindConnectionPoint(DIID_HTMLAnchorEvents, &cp))
			) {
				if (advise) {  						// Connect the event sink
					DWORD	cookie;
					if (SUCCEEDED(cp->Advise(static_cast<IDispatch*>(this), &cookie)))
						anchors.push_back(cookie);
				} else if (!anchors.empty()) {		// Disconnect the event sink
					cp->Unadvise(anchors.back());
					anchors.pop_back();
				}
			}
		}
	}
}

ViewBrowser::ViewBrowser(const WindowPos &wpos, const char *url) {
	Create(wpos, "Web", CHILD | CLIPCHILDREN, CLIENTEDGE);

	wb.create(CLSID_WebBrowser, CLSCTX_INPROC);

	// Set client site
	if (com_ptr<IOleObject> obj = wb.query()) {
		obj->SetClientSite(this);
		auto	r = GetClientRect();
		obj->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL, this, 0, wpos.Parent(), &r);
	}

	// Register container to intercept WebBrowser events
	Advise(wb, static_cast<IDispatch*>(this), DIID_DWebBrowserEvents2, cookie);

	// Navigate to start page
	wb->Navigate(str16(url), NULL, NULL, NULL, NULL);
}

class EditorHTML : public app::Editor {
	virtual bool Matches(const ISO::Browser &b) {
		return b.Is("URL");
	}
	virtual Control Create(app::MainWindow &main, const WindowPos &pos, const ISO_ptr<void> &p) {
//		RegKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Internet Explorer\\MAIN\\FeatureControl\\FEATURE_BROWSER_EMULATION").values()["IsoEditor.exe"] = 9999u;
//		RegKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION").values()["IsoEditor.exe"] = 9999u;
		return *new ViewBrowser(pos, ISO::Browser(p).GetString());
	}
public:
	EditorHTML() {
		static ISO::TypeUserSave	url_def("URL", ISO::getdef<const char*>());
	}
} editorhtml;
