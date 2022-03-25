#include "UPnP.h"

namespace iso {

//-----------------------------------------------------------------------------
//	SOAP
//-----------------------------------------------------------------------------

SOAP::SOAP(const char *command, const char *schema) : XMLDOM0(format_string("m:%s", command)), schema(schema), command(command) {
	AddAttr("xmlns:m", schema);
	dom.AddNode(XMLDOM0("?xml").AddAttr("version", "1.0"))
		.AddNode(XMLDOM0("SOAP-ENV:Envelope").AddAttr("xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/").AddAttr("SOAP-ENV:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/")
			.AddNode(XMLDOM0("SOAP-ENV:Body")
				.AddNode(*this)
			)
		);
}

XMLDOM0 SOAP::AddParameter(const char *name, const char *type, const char *value) {
	XMLDOM0	p(name);
	if (type)
		p.AddAttr("xmlns:dt", "urn:schemas-microsoft-com:datatypes").AddAttr("dt:dt", type);
	p.SetValue(value);
	AddNode(p);
	return p;
}

SOCKET SOAP::Send(const HTTP &http, const char *url) {
	dynamic_memory_writer	m;
//	dom.write(m);
	m.write(dom);
	return http.Request("POST", url,
		format_string(
			"Content-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"%s#%s\"\r\n",
			schema, command
		),
		m.data()
	);
}

//-----------------------------------------------------------------------------
//	UPnP
//-----------------------------------------------------------------------------
/*
MX:	Maximum wait time in seconds. Should be between 1 and 120 inclusive
ST:	Search Target. Must be one of the following:
ssdp:all									Search for all devices and services.
upnp:rootdevice								Search for root devices only.
uuid:<uuid>									Search for a particular device. Device UUID specified by UPnP vendor.
urn:schemas-upnp-org:device:<type:ver>		Search for any device of this type. Device type and version defined by UPnP Forum working committee.
urn:schemas-upnp-org:service:<service:ver>	Search for any service of this type.	Service type and version defined by UPnP Forum working committee.
urn:<domain>:device:<device:v>				Search for any device of this type. Domain name, device type and version defined by UPnP vendor. Period characters in the domain name must be replaced with hyphens in accordance with RFC 2141.
urn:<domain>:service:<service:v>			Search for any service of this type. Domain name, service type and version defined by UPnP vendor. Period characters in the domain name must be replaced with hyphens in accordance with RFC 2141.
*/

SOCKET UPnP_scan(const char *target, int delay) {
	socket_init();

	IP4::addr	local_ip = 0u;
	IP4::addr	multi_ip = "239.255.255.250";
	PORT		port	= 1900;

	fixed_string<256>	search;
	search << "M-SEARCH * HTTP/1.1\r\n"
		"Host:" << multi_ip << ':' << port << "\r\n"
		"ST:"	<< target << "\r\n"
		"MAN:\"ssdp:discover\"\r\n"
		"MX:"	<< delay << "\r\n\r\n";

	char	hostname[256];
	if (gethostname(hostname, sizeof(hostname)) != 0)
		return INVALID_SOCKET;

	socket_address	test = socket_address::UDP();
	test.resolve(hostname, port);
	for (auto &i : test) {
		if (!i.is_local()) {
			local_ip = ((IP4::socket_addr&)i).ip;
			break;
		}
	}

	auto	sock = Socket::UDP();
	sock.options().reuse_addr(true);								// Enable SO_REUSEADDR to allow multiple instances of this application to receive copies of the multicast datagrams.
#if 0
	socket_setopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, 0);											// Disable loopback so you do not receive your own datagrams.
	socket_setopt(sock, IPPROTO_IP, IP_MULTICAST_IF, local_ip);										// Set local interface for outbound multicast datagrams
	socket_setopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, IP4::multicast_group(multi_ip, local_ip));	// Join the multicast group on the local interface
#else
	auto	opts = sock.ip4_options();
	opts.multicast.loop(false);										// Disable loopback so you do not receive your own datagrams.
	opts.multicast.interfce(local_ip);								// Set local interface for outbound multicast datagrams
	opts.multicast.join(IP4::multicast_group(multi_ip, local_ip));	// Join the multicast group on the local interface
#endif

	if (IP4::socket_addr(multi_ip, port).send(sock, search.data()))
		return sock.detach();

	return INVALID_SOCKET;
}

SOCKET UPnP_scan() { return UPnP_scan("upnp:rootdevice"); }


}	// namespace iso
