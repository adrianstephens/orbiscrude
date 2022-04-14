#ifndef IP_H
#define IP_H

#include "base/defs.h"
#include "base/constants.h"
#include "base/strings.h"
#include "base/bits.h"
#include "base/array.h"
#include "stream.h"
#include "sockets.h"

#if defined PLAT_PC || defined PLAT_XONE
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#ifndef PLAT_PS4
#include <netdb.h>
#endif
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET SOCKET(~SOCKET(0))
#endif

namespace iso {

//-----------------------------------------------------------------------------
// socket_addr
//-----------------------------------------------------------------------------

struct socket_addr_head {
	socklen_t	len;
	uint16		family;
	uint16be	port;

	socket_addr_head(size_t _len, int family = 0, PORT port = 0) : len(socklen_t(_len - sizeof(len))), family(family), port(port) { memset(this + 1, 0, _len - sizeof(socket_addr_head)); }
	socket_addr_head(size_t _len, SOCKET sock)	: len(socklen_t(_len - sizeof(len)))	{ getpeername(sock, sockaddr(), &len); }
#ifndef PLAT_PS4
	socket_addr_head(const addrinfo *a)			: len(socklen_t(a->ai_addrlen))			{ memcpy(&family, a->ai_addr, len); }
#endif

	::sockaddr			*sockaddr()			{ return (::sockaddr*)&family; }
	const ::sockaddr	*sockaddr()	const	{ return (const ::sockaddr*)&family; }

	bool	check_error(int r)		const	{ if (r == 0) return true; socket_error("?"); return false; }
	SOCKET	check_socket(int r)		const	{ if (r < 0) socket_error("?"); return (SOCKET)r; }

	bool	bind(SOCKET sock)		const	{ return check_error(::bind(sock, sockaddr(), len)); }
	bool	connect(SOCKET sock)	const	{ return check_error(::connect(sock, sockaddr(), len)); }
	SOCKET	accept(SOCKET sock)				{ return check_socket(::accept(sock, sockaddr(), &len)); }

	int		send(SOCKET sock, const void *buffer, size_t size) const {
		return (int)sendto(sock, (const char*)buffer, socklen_t(size), 0, sockaddr(), len);
	}
	int		recv(SOCKET sock, void *buffer, size_t size) {
		return (int)recvfrom(sock, (char*)buffer, socklen_t(size), 0, sockaddr(), &len);
	}
	int		send(SOCKET sock, const const_memory_block &buffer) const {
		return send(sock, buffer, buffer.length());
	}
//	template<typename T> bool send(SOCKET sock, const T &t) const {
//		return send(sock, (const char*)&t, sizeof(T)) == sizeof(T);
//	}
	SOCKET	listener(int type = SOCK_STREAM, int proto = IPPROTO_TCP, int max_pending = 16) const {
		SOCKET sock = socket_create(family, type, proto);
		if (bind(sock) && check_error(listen(sock, max_pending)))
			return sock;
		socket_close(sock);
		return INVALID_SOCKET;
	}
	SOCKET	socket(int type = SOCK_STREAM, int proto = IPPROTO_TCP) const {
		SOCKET sock = socket_create(family, type, proto);
		if (connect(sock))
			return sock;
		socket_close(sock);
		return INVALID_SOCKET;
	}
};

//-----------------------------------------------------------------------------
// MAC & port
//-----------------------------------------------------------------------------

struct MAC : uintn<6, true> {
	MAC()	{}
	MAC(const char *s)	{ from_string(s, *this); }
	MAC(uint64 x)		: uintn<6, true>(x) {}
	friend size_t from_string(const char *s, MAC &a);
	friend size_t to_string(char *s, const MAC &a);
	uint64	EUI64() const	{ uint64 t = *this; return ((t & ~bits64(24)) << 16) | (t & bits64(24)) | (uint64(0xfffe) << 24); }
};

//-----------------------------------------------------------------------------
// IP4
//-----------------------------------------------------------------------------

namespace IP4 {
	struct addr : array<uint8, 4> {
		addr()	{}
		addr(uint8 a, uint8 b, uint8 c, uint8 d) : array<uint8, 4>{a, b, c, d} {}
		addr(const char *s)					{ from_string(s, *this); }
		addr(uint32 x)						{ *(uint32be*)this = x;	}
		operator uint32()			const	{ return *(uint32be*)this;	}
		bool	operator==(uint32 u) const	{ return operator uint32() == u; }
		addr	flip()				const	{ return swap_endian(*this); }
		friend size_t from_string(const char *s, addr &a);
		friend size_t to_string(char *s, const addr &a);
	};
	static const addr localhost(127,0,0,1);
	typedef uint16		chksum;

	struct header {
		uint8		ISO_BITFIELDS2(version:4, h_len:4);
		uint8		tos;		// Type of service
		uint16be	total;		// Length of the packet in dwords
		uint16		ident;		// unique identifier
		uint16be	flags;		// Flags
		uint8		ttl;		// Time to live
		uint8		proto;		// Protocol number (TCP, UDP etc)
		uint16		checksum;	// IP checksum
		addr		source, dest;
		//options (if any)

		uint16		header_len()	{ return h_len * 4; }
		void*		payload()		{ return (char*)this + h_len * 4; }
	};

	struct IGMP {
		uint8		ISO_BITFIELDS2(version:4, type:4);
		uint8		reserve;
		uint16		checksum;
		addr		dst_ip;
	};

	struct ARP {
		uint16be	mac_type;
		uint16be	protocol_type;
		uint8		mac_length;
		uint8		protocol_length;
		uint16be	opocode;
		MAC			src_mac;
		addr		src_ip;
		MAC			dst_mac;
		addr		dst_ip;
	};

	struct multicast_group {
		addr	multicast, local;
		multicast_group(const addr &multicast, const addr &local) : multicast(multicast), local(local) {}
	};

	struct multicast_source {
		addr	multicast, source, local;
		multicast_source(const addr &multicast, const addr &source, const addr &local) : multicast(multicast), source(source), local(local) {}
	};
	chksum	calc_checksum(void *buffer, uint32 size);

	struct socket_addr : socket_addr_head {
		addr	ip;
		uint8	unused[8];

		socket_addr()								: socket_addr_head(sizeof(*this))		{}
	#ifndef PLAT_PS4
		socket_addr(const addrinfo *a)				: socket_addr_head(a)					{}
	#endif
		socket_addr(SOCKET sock)					: socket_addr_head(sizeof(*this), sock)	{}
		socket_addr(const addr &ip, PORT port)		: socket_addr_head(sizeof(*this), AF_INET, port), ip(ip) {}
		socket_addr(PORT port)						: socket_addr_head(sizeof(*this), AF_INET, port) {}

		SOCKET	listener(int type = SOCK_STREAM, int proto = IPPROTO_TCP, int max_pending = 16) {
			return socket_addr_head::listener(type, proto, max_pending);
		}
		bool	is_local() const {
			return (ip >> 16) == 0xa9fe	//169.254
				||	ip == localhost;	//127.0.0.1
		}
	};

	struct Options {
		SOCKET	s;
		Options(SOCKET s) : s(s)	{}
		template<typename T> bool set(int opt, const T &val) const { return socket_setopt(s, IPPROTO_IP, opt, val); }

		struct _multicast {
			auto	me()						const	{ return T_get_enclosing(this, &Options::multicast); }
			bool	interfce(addr v)			const	{ return me()->set(IP_MULTICAST_IF, v); }				//outgoing interface for sending IPv4 multicast traffic
			bool	loop(bool v)				const	{ return me()->set(IP_MULTICAST_LOOP, v); }				//Controls whether data sent by an application on the local computer (not necessarily by the same socket) in a multicast session will be received by a socket joined to the multicast destination group on the loopback interface
			bool	ttl(uint32 v)				const	{ return me()->set(IP_MULTICAST_TTL, v); }				//TTL value associated with IP multicast traffic on the socket.
			bool	join(multicast_group v)		const	{ return me()->set(IP_ADD_MEMBERSHIP, v); }				//Join the socket to the multicast group on the specified interface
			bool	leave(multicast_group v)	const	{ return me()->set(IP_DROP_MEMBERSHIP, v); }			//Leaves the specified multicast group from the specified interface
		#ifndef PLAT_PS4
			bool	join(multicast_source v)	const	{ return me()->set(IP_ADD_SOURCE_MEMBERSHIP, v); }		//Join the supplied multicast group on the given interface and accept data sourced from the supplied source address
			bool	leave(multicast_source v)	const	{ return me()->set(IP_DROP_SOURCE_MEMBERSHIP, v); }		//Drops membership to the given multicast group, interface, and source address
		#endif
		} multicast;
		struct _unicast {
			auto	me()						const	{ return T_get_enclosing(this, &Options::unicast); }
			bool	interfce(addr v)			const	{ return me()->set(IP_UNICAST_IF, v); }				//Gets or sets the outgoing interface for sending IPv4 traffic. This option does not change the default interface for receiving IPv4 traffic. This option is important for multihomed computers.
		} unicast;

		bool	ttl(bool v)								const	{ return set(IP_TTL, v); }						//Changes the default value set by the TCP/IP service provider in the TTL field of the IP header in outgoing datagrams
		bool	include_header(bool v)					const	{ return set(IP_HDRINCL, v); }					//indicates the application provides the IP header (SOCK_RAW ony)

	#ifndef PLAT_PS4
		bool	block_source(multicast_source v)		const	{ return set(IP_BLOCK_SOURCE, v); }				//Removes the given source as a sender
		bool	unblock_source(multicast_source v)		const	{ return set(IP_UNBLOCK_SOURCE, v); }			//Adds the given source as a sender to the supplied multicast group and interface.
		bool	options(const char *v)					const	{ return set(IP_OPTIONS, v); }					//IP options to be inserted into outgoing packets
		bool	packet_info(uint32 v)					const	{ return set(IP_PKTINFO, v); }					//Indicates that packet information should be returned by the WSARecvMsg function.
		bool	receive_interface(bool v)				const	{ return set(IP_RECVIF, v); }					//whether the IP stack should populate the control buffer with details about which interface received a packet with a datagram socket
	#endif
	#ifdef PLAT_PC
		bool	dont_fragment(bool v)					const	{ return set(IP_DONTFRAGMENT, v); }				//data should not be fragmented regardless of the local MTU
		bool	original_arrival_interface(bool v)		const	{ return set(IP_ORIGINAL_ARRIVAL_IF, v); }		//Indicates if the WSARecvMsg function should return optional control data containing the arrival interface where the packet was received for datagram sockets.
		bool	receive_broadcast(bool v)				const	{ return set(IP_RECEIVE_BROADCAST, v); }		//Allows or blocks broadcast reception.
	#endif
	};
};

//-----------------------------------------------------------------------------
// IP6
//-----------------------------------------------------------------------------

#ifndef PLAT_PS4
namespace IP6 {
	enum multicast_scope {
		interface_local		= 0x1,
		link_local			= 0x2,
		realm_local			= 0x3,
		admin_local			= 0x4,
		site_local			= 0x5,
		organization_local	= 0x8,
		global				= 0xe,
	};

	struct addr : array<uint16be, 8> {
		static addr	any() { addr a; clear(a); return a; }
		addr()				{}
		addr(const char *s) { from_string(s, *this); }
		addr(uint64 routing, uint64 interfce)	{ uint64 *p = (uint64*)this; p[0] = routing; p[1] = interfce; }
		addr(uint64 routing, const MAC &mac)	{ uint64 *p = (uint64*)this; p[0] = routing; p[1] = mac.EUI64() ^ bit64(57); }
		addr(const IP4::addr &a)				{ uint64 *p = (uint64*)this; p[0] = 0; p[1] = uint64(0xffff) << 32; *(IP4::addr*)(begin() + 6) = a; }
		addr(multicast_scope sc, uint64 b)		{ uint64 *p = (uint64*)this; p[0] = uint64(sc | 0xff00) << 48; p[1] = b; }
		template<typename...TT> addr(const TT&... tt) : array<uint16be, 8>(tt...) {}
		friend size_t from_string(const char *s, addr &a);
		friend size_t to_string(char *s, const addr &a);
	};

	enum EXTENSION {
		HOP_BY_HOP		= 0,	//Options that need to be examined by all devices on the path.
		DESTINATION		= 60,	//Options that need to be examined only by the destination of the packet.
		ROUTING			= 43,	//Methods to specify the route for a datagram (used with Mobile IPv6).
		FRAGMENT		= 44,	//Contains parameters for fragmentation of datagrams.
		AUTHENTICATION	= 51,	//Contains information used to verify the authenticity of most parts of the packet.
		ESP				= 50,	//Carries encrypted data for secure communication.
		DESTINATION2	= 60,	//Options that need to be examined only by the destination of the packet.
		MOBILITY		= 135,	//Parameters used with Mobile IPv6.
	};
	struct header {
		union {
			uint32	u;
			bitfield<uint32be, 28, 4>	version;
			bitfield<uint32be, 20, 8>	traffic_class;
			bitfield<uint32be,  0, 20>	flow_label;
		};
		uint16be	payload_length;
		uint8		next_header;
		uint8		hop_limit;
		addr		source, dest;
	};

	struct extension_header {
		uint8	next_header, len;
	};

	struct hop_by_hop : extension_header {
		uint16be	options[];
	};
	struct destination : extension_header {
		uint16be	options[];
	};
	struct fragment : extension_header {	//len=0
		union {
			uint16	u;
			bitfield<uint16be, 3, 13>	fragment_offset;
			bitfield<uint16be, 1, 2>	res;
			bitfield<uint16be, 0, 1>	m;
		};
		uint32be	id;
	};
	struct routing : extension_header {
		uint16be	type, segments_left;
		uint32be	options[];
	};
	struct authentication : extension_header {
		uint16be	reserved;
		uint32be	spi, seq;
		uint32be	icv[];
	};
	struct encapsulating_security_payload {
		uint32be	spi, seq;
		uint32be	payload[];
		//uint8	padding, next_header;
		//uint32be	icv[];
	};

	struct multicast_group {
		addr		multicast;
		uint32be	interfce;
		multicast_group(const addr &multicast, uint32 interfce) : multicast(multicast), interfce(interfce) {}
	};

	struct socket_addr : socket_addr_head {
		uint32  flowinfo;
		addr	ip;
		uint32	zone : 28, level : 4;

		socket_addr()							: socket_addr_head(sizeof(*this))		{}
		socket_addr(const addrinfo *a)			: socket_addr_head(a)					{}
		socket_addr(SOCKET sock)				: socket_addr_head(sizeof(*this), sock)	{}
		socket_addr(const addr &ip, PORT port)	: socket_addr_head(sizeof(*this), AF_INET6, port), ip(ip) {}
		socket_addr(PORT port)					: socket_addr_head(sizeof(*this), AF_INET6, port), ip(0, 0, 0, 0, 0, 0, 0, 0) {}

		SOCKET	listener(int type = SOCK_STREAM, int proto = IPPROTO_TCP, int max_pending = 16) {
			return socket_addr_head::listener(type, proto, max_pending);
		}
		bool	is_local() const {
			return ip == addr(0, 0, 0, 0, 0, 0, 0, 1);
		}
	};

	struct Options {
		SOCKET	s;
		struct _multicast {
			auto	me()						const	{ return T_get_enclosing(this, &Options::multicast); }
			bool	hops(uint32 v)				const	{ return me()->set(IPV6_MULTICAST_HOPS, v); }		//Gets or sets the TTL value associated with IPv6 multicast traffic on the socket. It is illegal to set the TTL to a value greater than 255.
			bool	interfce(uint32 v)			const	{ return me()->set(IPV6_MULTICAST_IF, v); }			//Gets or sets the outgoing interface for sending IPv6 multicast traffic. This option does not change the default interface for receiving IPv6 multicast traffic. This option is important for multihomed computers.
			bool	loop(bool v)				const	{ return me()->set(IPV6_MULTICAST_LOOP, v); }		//Indicates multicast data sent on the socket will be echoed to the sockets receive buffer if it is also joined on the destination multicast group. If optval is set to 1 on the call to setsockopt, the option is enabled. If set to 0, the option is disabled.
			bool	join(multicast_group v)		const	{ return me()->set(IPV6_JOIN_GROUP, v); }			//Join the socket to the supplied multicast group on the specified interface.
			bool	leave(multicast_group v)	const	{ return me()->set(IPV6_LEAVE_GROUP, v); }			//Leave the supplied multicast group from the given interface.
		} multicast;
		struct _unicast {
			auto	me()						const	{ return T_get_enclosing(this, &Options::unicast); }
			bool	hops(uint32 v)				const	{ return me()->set(IPV6_UNICAST_HOPS, v); }			//Gets or sets the current TTL value associated with IPv6 socket for unicast traffic. It is illegal to set the TTL to a value greater than 255.
			bool	interfce(uint32 v)			const	{ return me()->set(IPV6_UNICAST_IF, v); }			//Gets or sets the outgoing interface for sending IPv6 traffic. This option does not change the default interface for receiving IPv6 traffic. This option is important for multihomed computers.
		} unicast;

		Options(SOCKET s) : s(s)	{}
		template<typename T> bool set(int opt, const T &val) const { return socket_setopt(s, IPPROTO_IPV6, opt, val); }

		bool	v6_only(bool v)							const	{ return set(IPV6_V6ONLY, v); }				//Indicates if a socket created for the AF_INET6 address family is restricted to IPv6 communications only
		bool	include_header(bool v)					const	{ return set(IPV6_HDRINCL, v); }			//Indicates the application provides the IPv6 header on all outgoing data
		bool	packet_info(bool v)						const	{ return set(IPV6_PKTINFO, v); }			//Indicates that packet information should be returned by the WSARecvMsg function.
		bool	receive_interface(bool v)				const	{ return set(IPV6_RECVIF, v); }				//populate the control buffer with details about which interface received a packet with a datagram socket
	#ifdef PLAT_PC
		bool	original_arrival_interface(bool v)		const	{ return set(IP_ORIGINAL_ARRIVAL_IF, v); }	//return optional control data containing the original arrival interface where the packet was received for datagram sockets.
		bool	hop_limit(bool v)						const	{ return set(IPV6_HOPLIMIT, v); }			//Indicates that hop (TTL) information should be returned in the WSARecvMsg function
		bool	protection_level(int v)					const	{ return set(IPV6_PROTECTION_LEVEL, v); }	//Enables restriction of a socket to a specified scope, such as addresses with the same link local or site local prefix
	#endif
	};
};
#endif

//-----------------------------------------------------------------------------
// Socket
//-----------------------------------------------------------------------------

struct SocketOptions {
	SOCKET	s;
	SocketOptions(SOCKET s) : s(s)	{}
	template<typename T> bool set(int opt, const T &val) const { return socket_setopt(s, SOL_SOCKET, opt, val); }

	bool	broadcast(bool v)						const	{ return set(SO_BROADCAST, v); }
	bool	keepalive(bool v)						const	{ return set(SO_KEEPALIVE, v); }
	bool	linger(bool on, int secs)				const	{ ::linger v = {on, (uint16)secs}; return set(SO_LINGER, v); }
	bool	receive_buffer(int v)					const	{ return set(SO_RCVBUF, v); }
	bool	reuse_addr(bool v)						const	{ return set(SO_REUSEADDR, v); }
	bool	receive_timeout(float s)				const	{ return set(SO_RCVTIMEO, uint32(s * 1000)); }
	bool	send_buffer(int v)						const	{ return set(SO_SNDBUF, v); }
	bool	send_timeout(float s)					const	{ return set(SO_SNDTIMEO, uint32(s * 1000)); }

#ifndef PLAT_PS4
	bool	oob_inline(bool v)						const	{ return set(SO_OOBINLINE, v); }
#endif
#ifdef PLAT_PC
	bool	conditional_accept(bool v)				const	{ return set(SO_CONDITIONAL_ACCEPT, v); }
	bool	dont_linger(bool v)						const	{ return set(SO_DONTLINGER, v); }
	bool	exclusive_addr(bool v)					const	{ return set(SO_EXCLUSIVEADDRUSE, v); }
#endif
};

struct Socket : readwriter_mixin<const Socket> {
	SOCKET	s;

	static Socket TCP()		{ return Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); }
	static Socket UDP()		{ return Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); }

	Socket() : s(INVALID_SOCKET)		{}
	Socket(int family, int socktype, int protocol) : s(socket_create(family, socktype, protocol))	{}
	Socket(SOCKET s)	: s(s)			{}
	Socket(Socket &&s)	: s(s.detach())	{}
//	Socket& operator=(SOCKET _s)		{ s = _s; return *this; }
	Socket& operator=(Socket &&_s)		{ swap(s, _s.s); return *this; }
	~Socket()							{ socket_close(s);	}

	SOCKET	detach()										{ return exchange(s, INVALID_SOCKET); }
	explicit operator bool()						const	{ return s != INVALID_SOCKET;}
	operator SOCKET()								const	{ return s; }
	operator SOCKET&()										{ return s; }

	Socket	accept()								const	{ return socket_accept(s, 0, 0); }
	bool	connect(void *addr, size_t addr_len)	const	{ return socket_connect(s, addr, addr_len); }
//	size_t	receive_all(void *buffer, size_t size)	const	{ return socket_receive_all(s, buffer, size); }
	bool	select(float time)						const	{ return socket_select(time, s); }
	bool	select()								const	{ return socket_select(s); }
//	bool	valid()									const	{ return s != INVALID_SOCKET; }
	void	close()											{ socket_close(detach());	}

	template<typename T> bool setopt(int lev, int opt, const T &val) const { return socket_setopt(s, lev, opt, val); }
	SocketOptions	options()						const	{ return s; }
	IP4::Options	ip4_options()					const	{ return s; }
#ifndef PLAT_PS4
	IP6::Options	ip6_options()					const	{ return s; }
#endif

	bool		exists()								const	{ return s != INVALID_SOCKET;	}
	bool		eof()									const	{ return false; }
	size_t	readbuff(void *buffer, size_t size)			const	{ return socket_receive(s, buffer, size); }
	size_t	writebuff(const void *buffer, size_t size)	const	{ return socket_send(s, buffer, size); }
	SOCKET	_clone()	const { return INVALID_SOCKET; }
};

//-----------------------------------------------------------------------------
// low level frames
//-----------------------------------------------------------------------------

// Ethernet frame
struct ETHERNET_PRE {
	uint8		pre[7];
	uint8		sfd;
};

struct ETHERNET2 {
	enum ETHERTYPE {
		IPv4				= 0x0800,
		ARP					= 0x0806,
		WakeOnLAN			= 0x0842,
		AVTransport			= 0x22F0,
		IETF_TRILL			= 0x22F3,
		DECnet4				= 0x6003,
		RevAddrResolution	= 0x8035,
		AppleTalk			= 0x809B,
		AARP				= 0x80F3,
		VLAN				= 0x8100,
		IPX					= 0x8137,
		IPX2				= 0x8138,
		QNX					= 0x8204,
		IPv6				= 0x86DD,
		FlowControl			= 0x8808,
		Slow				= 0x8809,
		CobraNet			= 0x8819,
		MPLSunicast			= 0x8847,
		MPLSmulticast		= 0x8848,
		PPPoE_discovery		= 0x8863,
		PPPoE_session		= 0x8864,
		Jumbo				= 0x8870,
		HomePlug			= 0x887B,
		EAP					= 0x888E,
		PROFINET			= 0x8892,
		HyperSCSI			= 0x889A,
		ATA					= 0x88A2,
		EtherCAT			= 0x88A4,
		ProviderBridging	= 0x88A8,
		Powerlink			= 0x88AB,
		LLDP				= 0x88CC,
		SERCOS_III			= 0x88CD,
		HomePlugAVMME		= 0x88E1,
		MediaRedundancy		= 0x88E3,
		MACSsecurity		= 0x88E5,
		PBB					= 0x88E7,
		PTP					= 0x88F7,
		CFM					= 0x8902,
		FCoE				= 0x8906,
		FCoEinit			= 0x8914,
		RoCE				= 0x8915,
		HSR					= 0x892F,
		ConfigTest			= 0x9000,
		LLT					= 0xCAFE,
	};
	MAC			dst_mac;
	MAC			src_mac;
	uint16be	type;	//or length if < 1500;
//	uint8		data[];	//46-1500
//	uint32		chksum;
};

struct ETHERNET1 : ETHERNET_PRE, ETHERNET2 {
};

// UDP header
struct UDP {
	uint16be	src_port;
	uint16be	dst_port;
	uint16be	total;
	uint16		checksum;
};

// TCP header
struct TCP {
	uint16be	src_port;
	uint16be	dst_port;
	uint32be	sequence;
	uint32be	acknowledge;
	uint16		ISO_BITFIELDS8(h_len:4, res:6, urg:1, ack:1, psh:1, rst:1, syn:1, fin:1);
	uint16be	window_size;
	uint16		checksum;
	uint16be	urgent;
	//options (if any)
	//data (if any)
};

// ICMP packet
struct ICMP {
	enum type_enum {
		ECHO_REPLY					= 0,
		DESTINATION_UNREACHABLE		= 3,
		SOURCE_QUENCH				= 4,	// Deprecated
		REDIRECT					= 5,
		ALTERNATE_HOST_ADDRESS		= 6,
		ECHO_REQUEST				= 8,
		ROUTER_ADVERTISEMENT		= 9,
		ROUTER_SOLICITATION			= 10,
		TIME_EXCEEDED				= 11,
		PARAMETER_PROBLEM			= 12,
		TIMESTAMP					= 13,
		TIMESTAMP_REPLY				= 14,
		INFORMATION_REQUEST			= 15,
		INFORMATION_REPLY			= 16,
		ADDRESS_MASK_REQUEST		= 17,
		ADDRESS_MASK_REPLY			= 18,
		TRACEROUTE					= 30,
		DATAGRAM_CONVERSION_ERROR	= 31,
		MOBILE_HOST_REDIRECT		= 32,
		IPV6_WHERE_ARE_YOU			= 33,
		IPV6_I_AM_HERE				= 34,
		MOBILE_REGISTRATION_REQUEST	= 35,
		MOBILE_REGISTRATION_REPLY	= 36,
		DNS_REQUEST					= 37,
		DNS_REPLY					= 38,
		SKIP						= 39,
		PHOTURIS					= 40,
	};
	uint8		type;
	uint8		code;
	uint16		checksum;
	uint16be	id;
	uint16be	seq;

	ICMP()	{}
	ICMP(type_enum type, uint16 id, uint16 seq) : type(type), code(0), checksum(0), id(id), seq(seq)	{}
};

//-----------------------------------------------------------------------------
// socket streams
//-----------------------------------------------------------------------------

class socket_input : public reader_mixin<socket_input> {
	SOCKET	sock;
public:
	socket_input(SOCKET sock = INVALID_SOCKET) : sock(sock) {}
	size_t		readbuff(void *buffer, size_t size)			const { return socket_receive(sock, buffer, size); }
	void		seek_cur(streamptr offset)					const { stream_skip(*this, offset); }
	streamptr	length()									const { return maximum; }
	SOCKET		socket()									const { return sock; }
	SOCKET		_clone()									const { return sock; }
};

class socket_output : public writer_mixin<socket_output> {
	SOCKET	sock;
public:
	socket_output(SOCKET sock = INVALID_SOCKET) : sock(sock) {}
	size_t		writebuff(const void *buffer, size_t size)	const { return socket_send(sock, buffer, size); }
	streamptr	length()									const { return maximum; }
	SOCKET		socket()									const { return sock; }
	SOCKET		_clone()									const { return sock; }
};

//-----------------------------------------------------------------------------
// socket_address
// - like socket_addr, but resolves host names and holds a list of addresses
//-----------------------------------------------------------------------------

#ifndef PLAT_PS4
struct socket_addr_any : socket_addr_head {
	uint64	dummy[(_SS_MAXSIZE - sizeof(socket_addr_head)) / sizeof(uint64)];
	socket_addr_any(const addrinfo *a)	: socket_addr_head(a) {}

	bool	is_local() const {
		return	family == AF_INET	? ((IP4::socket_addr*)this)->is_local()
			:	family == AF_INET6	? ((IP6::socket_addr*)this)->is_local()
			:	true;
	}
};

struct socket_address : addrinfo {
	addrinfo	*res;
	sockaddr	addr;

	struct port_t : fixed_string<6> {
		port_t(const char *s)	: fixed_string<6>(s)	{}
		port_t(PORT port)		{ *this << port; }//to_string(begin(), port); }
	};

	static socket_address TCP()				{ return socket_address(AF_INET, SOCK_STREAM, IPPROTO_TCP); }
	static socket_address UDP()				{ return socket_address(AF_INET, SOCK_DGRAM, IPPROTO_UDP); }
	static socket_address RAW(int protocol) { return socket_address(AF_INET, SOCK_RAW, protocol); }

	socket_address(int family, int socktype, int protocol) : res(this) {
		clear(*(addrinfo*)this);
		ai_family	= family;
		ai_socktype	= socktype;
		ai_protocol	= protocol;
	}

	socket_address(SOCKET sock)	: res(this) {
		socklen_t	len;
		getpeername(sock, &addr, &len);//need to alloc ai_addr?
		ai_addr		= &addr;
		ai_addrlen	= len;
	}

	~socket_address() {
		if (res != this)
			freeaddrinfo(res);
	}

	operator addrinfo*()		const	{ return res;	}
	addrinfo*	operator->()	const	{ return res;	}
	addrinfo**	operator&()				{ return &res;	}

	void	resolve(uint32 ip, PORT port) {
		sockaddr_in	&a		= (sockaddr_in&)addr;
		a.sin_family		= AF_INET;
		a.sin_port			= port;
		a.sin_addr.s_addr	= ip;

		ai_addr		= &addr;
		ai_addrlen	= sizeof(a);
	}
	bool	resolve(const char *host, const port_t &port) {
		return getaddrinfo(host, port, this, &res) == 0;
	}
	SOCKET	socket() const {
		return ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	}
	bool	bind(SOCKET sock) {
		return sock != INVALID_SOCKET && ::bind(sock, res->ai_addr, socklen_t(res->ai_addrlen)) == 0;
	}
	bool	connect(SOCKET sock) {
		return sock != INVALID_SOCKET && ::connect(sock, res->ai_addr, socklen_t(res->ai_addrlen)) == 0;
	}
	int		send(SOCKET sock, const void *buffer, size_t len) const {
		return (int)sendto(sock, (const char*)buffer, socklen_t(len), 0, res->ai_addr, socklen_t(res->ai_addrlen));
	}
	int		recv(SOCKET sock, void *buffer, size_t len) const {
		res->ai_addrlen = sizeof(*res->ai_addr);
		return (int)recvfrom(sock, (char*)buffer, socklen_t(len), 0, res->ai_addr, (socklen_t*)&res->ai_addrlen);
	}
	template<typename T> bool send(SOCKET sock, const T &t) const {
		return send(sock, (const char*)&t, sizeof(T)) == sizeof(T);
	}
	SOCKET	connect(const char *host, const port_t &port) {
		if (!resolve(host, port))
			return INVALID_SOCKET;
		SOCKET	sock = socket();
		if (!connect(sock)) {
			socket_close(sock);
			sock = INVALID_SOCKET;
		}
		return sock;
	}

	struct iterator {
		const addrinfo	*a;
		iterator(const addrinfo *_a) : a(_a) {}
		const socket_addr_any	operator*()	{ return a; }
		iterator&	operator++()	{ a = a->ai_next; return *this; }
		bool		operator==(const iterator &b) const { return a == b.a; }
		bool		operator!=(const iterator &b) const { return a != b.a; }
	};

	iterator	begin()	const { return res; }
	iterator	end()	const { return 0; }
};
#endif

int ping(const char *host, int packet_size = 32, int ttl = 30);

}	// namespace iso
#endif
