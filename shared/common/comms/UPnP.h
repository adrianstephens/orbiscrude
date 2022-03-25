#ifndef UPNP_H
#define UPNP_H

#include "extra/xml.h"
#include "HTTP.h"

namespace iso {

//-----------------------------------------------------------------------------
//	UPnP_Device
//-----------------------------------------------------------------------------

class UPnP_Device : public XMLDOM0 {
	class UPnP_RootDevice *root;
protected:
	UPnP_Device(UPnP_RootDevice *root) : root(root)	{}
public:
	UPnP_Device(UPnP_RootDevice *root, const XMLDOM0 &dom0) : XMLDOM0(dom0), root(root)	{}
	~UPnP_Device()	{}

	UPnP_RootDevice		*Root()			const { return root; }
	inline const HTTP	&Connection()	const;

	XMLDOM0			FindService(const char *name) const {
		XMLDOM0		dom1	= (*this)/"serviceList";
		for (int i = 0, n = int(dom1.NumNodes()); i < n; i++) {
			const char	*service = dom1/i/"serviceType";
			if (strstr(service, name))
				return dom1/i;
		}
		return XMLDOM0();
	}
};

class UPnP_RootDevice : public UPnP_Device {
public:
	HTTP		http;
	XMLDOM		dom;
public:
	UPnP_RootDevice(HTTP::Context context, const char *url)
		: UPnP_Device(this)
		, http(context, url)
		, dom(HTTPinput(http.Request("GET", http.PathParams())))
	{
		*(XMLDOM0*)this = dom/"root"/"device";
	}
	UPnP_RootDevice(const HTTP &http)
		: UPnP_Device(this)
		, http(http)
		, dom(HTTPinput(http.Request("GET", http.PathParams())))
	{
		*(XMLDOM0*)this = dom/"root"/"device";
	}
};

class UPnP_Service : public XMLDOM0 {
public:
	UPnP_Device	*device;
	string		url;

	UPnP_Service(UPnP_Device *device, const XMLDOM0 &dom0) : XMLDOM0(dom0), device(device), url((dom0/"controlURL").Value()) {}
	const HTTP	&Connection() const		{ return device->Connection(); }
};

const HTTP &UPnP_Device::Connection() const	{ return root->http; }

//-----------------------------------------------------------------------------
//	SOAP
//-----------------------------------------------------------------------------
class SOAP : public XMLDOM0	{
	XMLDOM		dom;
	const char	*schema, *command;
public:
	SOAP(const char *command, const char *schema);
	XMLDOM0		AddParameter(const char *name, const char *type, const char *value);
	XMLDOM0		AddParameter(const char *name, const char *value = NULL)	{ return AddParameter(name, "string", value); }
	XMLDOM0		AddParameter(const char *name, uint32 value)				{ return AddParameter(name, "ui4", to_string(int(value))); }
	XMLDOM0		AddParameter(const char *name, int value)					{ return AddParameter(name, "i4", to_string(int(value))); }
	XMLDOM0		AddParameter(const char *name, const XMLDOM0 &value) {
		dynamic_memory_writer	m;
		value.write(m);
		m.putc(0);
		return AddParameter(name, NULL, (const char*)m.data());
	}
	SOCKET		Send(const HTTP &http, const char *url);
	SOCKET		Send(UPnP_Service *service)			{ return Send(service->Connection(), service->url);	}
	bool		write(ostream_ref file)	const		{ return dom.write(file); }
};

//-----------------------------------------------------------------------------
//	UPnP
//-----------------------------------------------------------------------------

SOCKET	UPnP_scan(const char *target, int delay = 3);
SOCKET	UPnP_scan();

} // namespace iso

#endif
