#include "ip.h"

using namespace iso;

#ifdef PLAT_PC
#include "IPHlpApi.h"

struct MAC_addresses : dynamic_array<MAC> {
	MAC_addresses() {
		IP_ADAPTER_INFO	adapter_info[16];
		DWORD	size	= sizeof(adapter_info);
		DWORD	status	= GetAdaptersInfo(adapter_info, &size);
		for (IP_ADAPTER_INFO *p = adapter_info; p; p = p->Next)
			push_back(*(MAC*)p->Address);
	}
};

#elif defined PLAT_MAC

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/ioctl.h>

struct MAC_addresses : dynamic_array<MAC> {
	MAC_addresses(){
		enum { BUFFERSIZE = 1024 };
		char	buffer[BUFFERSIZE];
		char	*end	= buffer;

		ifconf ifc;
		ifc.ifc_len = BUFFERSIZE;
		ifc.ifc_buf = buffer;

		int	sock	= socket(AF_INET, SOCK_DGRAM, 0);
		if (ioctl(sock, SIOCGIFCONF, (char*)&ifc) >= 0)
			end	= buffer + ifc.ifc_len;
		close(sock);

		for (const char *p = buffer; p < end; ) {
			ifreq	*ifr = (ifreq*)p;
			if (ifr->ifr_addr.sa_family == AF_LINK) {
				sockaddr_dl *sdl = (sockaddr_dl*)&ifr->ifr_addr;
				if (sdl->sdl_alen == 6)
					memcpy(&push_back(), sdl->sdl_data + sdl->sdl_nlen, 6);
			}
			p += sizeof(ifr->ifr_name) + max(sizeof(ifr->ifr_addr), ifr->ifr_addr.sa_len);
		}
	}
};
#endif

MAC GetMAC() {
	return MAC_addresses()[0];
}

struct DHCP {
	static const PORT SERVER_PORT		= 67;
	static const PORT CLIENT_PORT		= 68;
	static const PORT SERVER_ALTPORT	= 1067;
	static const PORT CLIENT_ALTPORT	= 1068;
	static const PORT PXE_PORT			= 4011;

	enum {
		COOKIE				= 0x63825363,
		MIN_PACKETSZ		= 300, // The Linux in-kernel DHCP client silently ignores any packet smaller than this.
		BRDBAND_FORUM_IANA	= 3561	// Broadband forum IANA enterprise
	};
	enum opcode {
		BOOTREQUEST		= 1,
		BOOTREPLY		= 2,
	};
	enum message_type {
		DISCOVER		= 1,
		OFFER			= 2,
		REQUEST			= 3,
		DECLINE			= 4,
		ACK				= 5,
		NAK				= 6,
		RELEASE			= 7,
		INFORM			= 8,
	};
	enum hardware_type {
		HTYPE_ETHER		= 1,	// Ethernet 10Mbps
		HTYPE_IEEE802	= 6,	// IEEE 802.2 Token Ring
		HTYPE_FDDI		= 8,	// FDDI...
	};
	enum options {
		OPTION_PAD					= 0,

		REQ_NETMASK					= 1,
		REQ_ROUTER					= 3,
		REQ_DNSSERVER				= 6,
		REQ_HOSTNAME				= 12,
		REQ_DOMAINNAME				= 15,
		REQ_BROADCAST				= 28,

		OPTION_VENDOR_CLASS_OPT		= 43,
		OPTION_REQUESTED_IP			= 50,
		OPTION_LEASE_TIME			= 51,
		OPTION_OVERLOAD				= 52,
		OPTION_MESSAGE_TYPE			= 53,
		OPTION_SERVER_IDENTIFIER	= 54,
		OPTION_REQUESTED_OPTIONS	= 55,
		OPTION_MESSAGE				= 56,
		OPTION_MAXMESSAGE			= 57,
		OPTION_T1					= 58,
		OPTION_T2					= 59,
		OPTION_VENDOR_ID			= 60,
		OPTION_CLIENT_ID			= 61,
		OPTION_SNAME				= 66,
		OPTION_FILENAME				= 67,
		OPTION_USER_CLASS			= 77,
		OPTION_CLIENT_FQDN			= 81,
		OPTION_AGENT_ID				= 82,
		OPTION_ARCH					= 93,
		OPTION_PXE_UUID				= 97,
		OPTION_SUBNET_SELECT		= 118,
		OPTION_DOMAIN_SEARCH		= 119,
		OPTION_SIP_SERVER			= 120,
		OPTION_VENDOR_IDENT			= 124,
		OPTION_VENDOR_IDENT_OPT		= 125,
		OPTION_END					= 255,

		SUBOPT_CIRCUIT_ID			= 1,
		SUBOPT_REMOTE_ID			= 2,
		SUBOPT_SUBNET_SELECT		= 5,	//	RFC	3527
		SUBOPT_SUBSCR_ID			= 6,	//	RFC	3393
		SUBOPT_SERVER_OR			= 11,	//	RFC	5107

		SUBOPT_PXE_BOOT_ITEM		= 71,	//	PXE	standard
		SUBOPT_PXE_DISCOVERY		= 6,
		SUBOPT_PXE_SERVERS			= 8,
		SUBOPT_PXE_MENU				= 9,
		SUBOPT_PXE_MENU_PROMPT		= 10,
	};

	uint8		opcode, htype, hlen, hops;
	uint32		id;
	uint16be	secs, flags;
	IP4::addr	ciaddr, yiaddr, siaddr, giaddr;
	uint8		chaddr[16], sname[64], file[128];
	uint8		options[312];

	struct option {
		byte_writer &w;
		uint8		*len;
		option(byte_writer &_w, int opt) : w(_w) {
			w.putc(opt);
			len	= w.p++;
		}
		~option() {
			*len = w.p - len - 1;
		}
		option& putc(uint8 t)	{ w.putc(t); return *this; }
		template<typename T> option& write(const T &t)	{ w.write(t); return *this; }
	};
};

struct DHCPTester {
	MAC	mac;
	DHCPTester() {
		MAC_addresses	macs;
		for (auto i : macs)
			ISO_TRACEF("mac=") << i << "\n";

		socket_init();
		struct IP_DHCP : IP4::header, UDP, DHCP {} packet;
		IP4::header	&ip		= packet;
		UDP			&udp	= packet;
		DHCP		&dhcp	= packet;

		clear(packet);

		ip.version	= 4;
		ip.h_len	= uint8(sizeof(IP4::header) / 4);
		ip.total	= uint16(sizeof(packet));
		ip.ident	= 0x1234;
		ip.ttl		= 0x80;
		ip.proto	= IPPROTO_UDP;
		ip.dest		= 0xffffffff;

		udp.src_port	= DHCP::CLIENT_PORT;
		udp.dst_port	= DHCP::SERVER_PORT;
		udp.total		= sizeof(UDP) + sizeof(DHCP);

		dhcp.opcode	= DHCP::BOOTREQUEST;
		dhcp.htype	= DHCP::HTYPE_ETHER;
		dhcp.hlen	= 6;
		dhcp.id		= 0x3903F326;//42;

		MAC	mac = GetMAC();
		(MAC&)dhcp.chaddr = mac;

		byte_writer	opts(dhcp.options);
		opts.write<uint32be>(DHCP::COOKIE);

		DHCP::option(opts, DHCP::OPTION_MESSAGE_TYPE)
			.putc(DHCP::DISCOVER);
		DHCP::option(opts, DHCP::OPTION_CLIENT_ID)
			.putc(1)
			.write(mac);
		DHCP::option(opts, DHCP::OPTION_MAXMESSAGE)
			.write<uint16be>(sizeof(IP4::header) + sizeof(UDP) + sizeof(DHCP));
		DHCP::option(opts, DHCP::OPTION_REQUESTED_OPTIONS)
			.putc(DHCP::REQ_NETMASK		)
			.putc(DHCP::REQ_ROUTER		)
			.putc(DHCP::REQ_DNSSERVER	)
			.putc(DHCP::REQ_HOSTNAME	)
			.putc(DHCP::REQ_DOMAINNAME	)
			.putc(DHCP::REQ_BROADCAST	);
		opts.putc(DHCP::OPTION_END);

		udp.checksum	= IP4::calc_checksum(&udp, udp.total);
		ip.checksum		= IP4::calc_checksum(&ip, ip.total);

	#if 0
		Socket				sock1(AF_INET, SOCK_RAW, IPPROTO_UDP);
		sock1.options().broadcast(true);
		sock1.ip4_options().include_header(true);

		IP4::socket_addr	addr1("255.255.255.255", DHCP::SERVER_PORT);
		addr1.send(sock1, &packet, sizeof(packet));
	#else
		socket_address	addr(AF_INET, SOCK_RAW, IPPROTO_UDP);
		SOCKET			sock	= addr.socket();
		auto			err		= WSAGetLastError();

		socket_setopt(sock, SOL_SOCKET, SO_BROADCAST, 1);
		socket_setopt(sock, IPPROTO_IP, IP_HDRINCL, 1);

		addr.resolve("255.255.255.255", DHCP::SERVER_PORT);
		addr.send(sock, &packet, sizeof(packet));

		//	addr.resolve("255.255.255.255", DHCP::CLIENT_PORT);
	#endif

		IP_DHCP	packet2;
		Socket listener = IP4::socket_addr(DHCP::CLIENT_PORT).listener();
		for (;;) {
			clear(packet2);
			auto	sock	= listener.accept();
			auto	err		= WSAGetLastError();
			int		r		= sock.readbuff(&packet2, sizeof(packet2));
			ISO_TRACEF() << packet2.source << '\n';
		}
	}
};// dhcptest;
