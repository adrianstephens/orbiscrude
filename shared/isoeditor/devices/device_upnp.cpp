#include "comms/upnp.h"
#include "iso/iso_files.h"
#include "jobs.h"

#include "filetypes/sound/sample.h"
#include "filetypes/video/movie.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	UPnP
//-----------------------------------------------------------------------------
struct UPnP_container : public ISO::VirtualDefaults {
	dynamic_array<ISO_ptr<void> >	entries;
	UPnP_Device		*device;
	string			url, id;

	void			Init(UPnP_Device *_device, const char *_url, const char *_id) {
		device	= _device;
		url		= _url;
		id		= _id;
	}

	int				Count();
	tag				GetName(int i)		{ return entries[i].ID(); }
	ISO::Browser2	Index(int i);
};

struct UPnP_item : public ISO::VirtualDefaults {
	UPnP_container	*container;
	string			id;
	ISO_ptr<void>	ptr;

	void			Init(UPnP_container *_container, const char *_id) {
		container	= _container;
		id			= _id;
	}

	int				Count()				{ return -1; }
	tag				GetName(int i)		{ return ptr.ID(); }
	ISO::Browser2	Deref();
};

template<> struct ISO::def<UPnP_container>	: public ISO::TypeUserSave	{
	ISO::VirtualT<UPnP_container>	v;
	def() : ISO::TypeUserSave("UPnP_container", &v) {}
};
template<> struct ISO::def<UPnP_item>	: public ISO::TypeUserSave	{
	ISO::VirtualT<UPnP_item>	v;
	def() : ISO::TypeUserSave("UPnP_item", &v) {}
};

ISO::Browser2 UPnP_item::Deref() {
	if (!ptr) {
		SOAP	soap("Browse", "urn:schemas-upnp-org:service:ContentDirectory:1");
		soap.AddParameter("ObjectID",		id);
		soap.AddParameter("BrowseFlag",		"BrowseMetadata");
		soap.AddParameter("Filter",			"*");//"dc:title");
		soap.AddParameter("StartingIndex",	0U);
		soap.AddParameter("RequestedCount",	1U);
		soap.AddParameter("SortCriteria");

		XMLDOM		domr	= XMLDOM(HTTPinput(soap.Send(container->device->Connection(), container->url)));
		const char *result	= domr/"*:Envelope"/"*:Body"/0/"Result";
		if (!result)
			return ISO::Browser();

		XMLDOM		domr2	= XMLDOM(memory_reader(result));
		XMLDOM0		res		= domr2/0/0/"res";
		const char *prot	= res->GetAttr("protocolInfo");
#if 1
		const ISO::Type *type
			= str(prot).find("image") ? ISO::getdef<bitmap>()
			: str(prot).find("audio") ? ISO::getdef<sample>()
			: str(prot).find("video") ? ISO::getdef<movie>()
			: 0;
		ptr = MakePtrExternal(type, (const char*)res, (const char*)(domr2/0/0/"dc:title"));
#else
		if (FileHandler *fh = FileHandler::Get(strrchr(res, '.'))) {
			ptr	= fh->ReadWithFilename(domr2/0/0/"dc:title", (const char*)res);
//			HTTP	http(res);
//			SOCKET	sock = http.Send("GET", http.path);
//			ptr	= fh->Read(domr2/0/0/"dc:title", *new HTTPinput(sock));
//			ptr	= fh->Read(domr2/0/0/"dc:title", HTTPOpenURL(res));
		}
#endif
	}
	return ISO::MakeBrowser(ptr);
}

int UPnP_container::Count() {
	if (entries.size() == 0) {
		SOAP	soap("Browse", "urn:schemas-upnp-org:service:ContentDirectory:1");
		soap.AddParameter("ObjectID",		id);
		soap.AddParameter("BrowseFlag",		"BrowseDirectChildren");
		soap.AddParameter("Filter",			"*");//"dc:title");
		soap.AddParameter("StartingIndex",	0U);
		soap.AddParameter("RequestedCount",	50U);
		soap.AddParameter("SortCriteria");

		int			total	= 0;

		SOCKET		sock	= soap.Send(device->Connection(), url);
		XMLDOM		domr	= XMLDOM(HTTPinput(sock));

		const char *result	= domr/"*:Envelope"/"*:Body"/0/"Result";
		if (!result || !from_string((const char*)(domr/"*:Envelope"/"*:Body"/0/"TotalMatches"), total))
			return 0;

		entries.resize(total);

		XMLDOM		domr2	= XMLDOM(memory_reader(result));
		int			num		= int((domr2/0).NumNodes());

		for (int i = 0; i < num; i++) {
			const char	*name		= (domr2/0/i)->Name();
			const char	*title		= domr2/0/i/"dc:title";

			if (str(name) == "container") {
				ISO_ptr<UPnP_container>	p(title);
				p->Init(device, url, (domr2/0/i)->GetAttr("id"));
				entries[i] = p;
			} else {
				ISO_ptr<UPnP_item>	p(filename(title).name());
				p->Init(this, (domr2/0/i)->GetAttr("id"));
				entries[i] = p;
			}
		}

		if (total > num) {
			ConcurrentJobs::Get().add([this, num]() {
				SOAP	soap("Browse", "urn:schemas-upnp-org:service:ContentDirectory:1");
				soap.AddParameter("ObjectID",		id);
				soap.AddParameter("BrowseFlag",		"BrowseDirectChildren");
				soap.AddParameter("Filter",			"*");//"dc:title");
				soap.AddParameter("StartingIndex",	num);
				soap.AddParameter("RequestedCount",	50U);
				soap.AddParameter("SortCriteria");
				int			index	= num;
				while (index < entries.size()) {
					(soap/"StartingIndex").SetValue(index);
					SOCKET		sock	= soap.Send(device->Connection(), url);
					XMLDOM		domr	= XMLDOM(HTTPinput(sock));
					const char *result	= domr/"*:Envelope"/"*:Body"/0/"Result";
					XMLDOM		domr2	= XMLDOM(memory_reader(result));
					int			num		= int((domr2/0).NumNodes());

					for (int i = 0; i < num; i++, index++) {
						const char	*name		= (domr2/0/i)->Name();
						const char	*title		= domr2/0/i/"dc:title";

						if (str(name) == "container") {
							ISO_ptr<UPnP_container>	p(title);
							p->Init(device, url, (domr2/0/i)->GetAttr("id"));
							entries[index] = p;
						} else {
							ISO_ptr<UPnP_item>	p(filename(title).name());
							p->Init(this, (domr2/0/i)->GetAttr("id"));
							entries[index] = p;
						}
					}
				}
			});
		}
	}

	return int(entries.size());
}

ISO::Browser2 UPnP_container::Index(int i) {
	return entries[i];
}

ISO_ptr<void> GetDevices(UPnP_Device *device) {
	ISO_ptr<anything>	dev((const char*)((*device)/"friendlyName"));

	XMLDOM0		services = (*device)/"serviceList";
	for (XMLDOM0::iterator i = services.begin(); i != services.end(); ++i) {
		const char	*type = (*i)/"serviceType";
		if (strstr(type, "ContentDirectory")) {
			ISO_ptr<UPnP_container>	p(type);
			p->Init(device, (*i)/"controlURL", "0");
			dev->Append(p);
		} else {
			ISO_ptr<int>	p(type);
			dev->Append(p);
		}
	}

	XMLDOM0		devices	= (*device)/"deviceList";
	for (XMLDOM0::iterator i = devices.begin(); i != devices.end(); ++i)
		dev->Append(GetDevices(new UPnP_Device(device->Root(), *i)));

	XMLDOM0		icons = (*device)/"iconList";
	ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > >	icons2("icons");
	for (XMLDOM0::iterator i = icons.begin(); i != icons.end(); ++i) {
		ISO_ptr<bitmap>	p;
		const char *url = (*i)/"url";
		icons2->Append(p.CreateExternal(device->Connection().GetURL(url, 80)));
	}
	if (*icons2)
		dev->Append(icons2);

	return dev;
}

//-----------------------------------------------------------------------------
//	UPnPDevice
//-----------------------------------------------------------------------------
#include "main.h"
#include "device.h"
#include "thread.h"
using namespace app;

#if 0
struct UPnPDevice : DeviceT<UPnPDevice>, DeviceCreateT<UPnPDevice> {
	void			operator()(const DeviceAdd &add) {
		add("UPnP", this, LoadPNG("IDB_DEVICE_UPNP"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		ISO_ptr<anything>	a("UPnP");
		RunThread([&main, a]() {
			dynamic_array<string>	usns;
			HTTP::Context			context("UPnP");
			for (Socket sock = UPnP_scan(); sock.select(4.0f); ) {
				IP4::socket_addr	addr;
				char		buff[1024];
				int			len		= addr.recv(sock, buff, sizeof(buff));
				buff[len] = 0;

				if (char *st = (char*)istr(buff, len).find("\nst:")) {
					st = skip_whitespace(st + 4);
					if (str(st, str(st).find('\r') - st) != "upnp:rootdevice")
						continue;
				}

				if (char *usn = (char*)istr(buff, len).find("usn:")) {
					usn = skip_whitespace(usn + 4);
					count_string	usn2(usn, str(usn).find('\r') - usn);
					if (iso::find(usns, usn2) != usns.end())
						continue;
					usns.push_back(usn2);
				}

				if (char *loc = (char*)istr(buff, len).find("location:")) {
					loc = skip_whitespace(loc += 9);
					*str(loc).find('\r') = 0;
					a->Append(GetDevices(new UPnP_RootDevice(context, loc)));
#ifdef ISO_EDITOR
					((IsoEditor*)IsoEditor::Cast(main))->Update(ISO_ptr<void>(a));
#endif
				}
			}
			return 0;
		});
		return a;
	}
} upnp_device;
#else

struct UPnPDevices : DeviceT<UPnPDevices> {

	struct UPnPDevice : DeviceCreateT<UPnPDevice> {
		ISO_ptr<anything>	a;

		ISO_ptr<void>	operator()(const Control &main) {
			if (a->Count() == 1)
				return (*a)[0];
			return a;
		}
		void	operator()(AppEvent *ev) {
			if (ev->state == AppEvent::END)
				delete this;
		}
		const char *name() {
			return a.ID().get_tag();
		}
		UPnPDevice(HTTP::Context context, const char *url) : a(GetDevices(new UPnP_RootDevice(context, url))) {}
	};

	void			operator()(const DeviceAdd &add) {
		DeviceAdd	sub = add("UPnP", LoadPNG("IDB_DEVICE_UPNP"));
		//return;
		RunThread([this, sub]() {
			HTTP::Context			context("UPnP");
			dynamic_array<string>	usns;
			for (Socket sock = UPnP_scan(); sock.select(4.0f); ) {
				IP4::socket_addr	addr;
				char		buff[1024];
				int			len		= addr.recv(sock, buff, sizeof(buff));
				buff[len] = 0;

				if (char *st = (char*)istr(buff, len).find("\nst:")) {
					st = skip_whitespace(st + 4);
					if (str(st, str(st).find('\r') - st) != "upnp:rootdevice")
						continue;
				}

				if (char *usn = (char*)istr(buff, len).find("usn:")) {
					usn = skip_whitespace(usn + 4);
					count_string	usn2(usn, str(usn).find('\r') - usn);
					if (find_check(usns, usn2))
						continue;
					usns.push_back(usn2);
				}

				if (char *loc = (char*)istr(buff, len).find("location:")) {
					loc = skip_whitespace(loc += 9);
					*str(loc).find('\r') = 0;

					UPnPDevice	*dev = new UPnPDevice(context, loc);
					sub(dev->name(), dev);
				}
			}
			return 0;
		});
	}
} upnp_devices;
#endif

#ifdef PLAT_WIN32

struct UPnPDevice2 : DeviceT<UPnPDevice2>, DeviceCreateT<UPnPDevice2> {
	fixed_string<1024>	url;

	void			operator()(const DeviceAdd &add) {
		add("UPnP by URL", this, LoadPNG("IDB_DEVICE_UPNP"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		HTTP::Context	context("UPnP");
		if (GetValueDialog(main, url))
			return GetDevices(new UPnP_RootDevice(context, url));
		return ISO_NULL;
	}
} upnp_device2;

#endif



